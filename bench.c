// bench.c — how much CPU does it cost to write N MiB to a disk?
//
// The same program runs twice: once with the disk forced into PIO mode,
// once with DMA enabled. The interesting output is not the throughput on
// its own, it is the CPU time spent per megabyte.
//
//   usage: bench <device-or-file> [MiB] [block-KiB]
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static double now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double cpu_seconds(void)
{
	struct rusage ru;
	getrusage(RUSAGE_SELF, &ru);
	return ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6 +
	       ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/dev/sda";
	size_t mib = argc > 2 ? strtoul(argv[2], NULL, 10) : 64;
	size_t blk = (argc > 3 ? strtoul(argv[3], NULL, 10) : 1024) * 1024;
	size_t total = mib * 1024 * 1024;

	/* O_DIRECT bypasses the page cache: we want to time the disk, not RAM. */
	int fd = open(path, O_WRONLY | O_DIRECT);
	int direct = 1;
	if (fd < 0) {
		fd = open(path, O_WRONLY);
		direct = 0;
		if (fd < 0) {
			perror("open");
			return 1;
		}
		fprintf(stderr, "warning: O_DIRECT unavailable, results include the page cache\n");
	}

	void *buf;
	if (posix_memalign(&buf, 4096, blk)) {
		perror("posix_memalign");
		return 1;
	}
	memset(buf, 0xA5, blk);

	double t0 = now(), c0 = cpu_seconds();
	size_t written = 0;
	while (written < total) {
		ssize_t n = write(fd, buf, blk);
		if (n <= 0) {
			perror("write");
			break;
		}
		written += n;
	}
	fsync(fd);
	double wall = now() - t0, cpu = cpu_seconds() - c0;

	double mb = written / (1024.0 * 1024.0);
	printf("device        : %s%s\n", path, direct ? "" : "  (buffered!)");
	printf("written       : %.0f MiB in blocks of %zu KiB\n", mb, blk / 1024);
	printf("wall time     : %.3f s\n", wall);
	printf("throughput    : %.1f MiB/s\n", mb / wall);
	printf("cpu time      : %.3f s\n", cpu);
	printf("cpu per MiB   : %.1f ms\n", 1000.0 * cpu / mb);
	printf("cpu busy      : %.0f %% of the wall time\n", 100.0 * cpu / wall);
	printf("not running   : %.3f s (%.0f %% of the wall time)\n",
	       wall - cpu, 100.0 * (wall - cpu) / wall);
	close(fd);
	free(buf);
	return 0;
}
