// init-io.c — PID 1 for the I/O lab VM. Mounts /proc and /sys, reports the
// transfer mode the kernel actually negotiated, runs the benchmark, powers off.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>

static void cat(const char *path, const char *label)
{
	char buf[256];
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return;
	ssize_t n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return;
	buf[n] = 0;
	for (char *p = buf; *p; p++)
		if (*p == '\n') *p = 0;
	printf("%s%s\n", label, buf);
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;
	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	/* without devtmpfs there is no /dev/sda to open */
	mkdir("/dev", 0755);
	mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

	printf("\n--- I/O lab ---\n");
	/* What did libata actually negotiate for this disk? */
	cat("/sys/class/ata_link/link1/ata_device/dev1.0/xfer_mode", "xfer mode    : ");
	cat("/proc/partitions", "partitions   : ");
	fflush(stdout);

	pid_t p = fork();
	if (p == 0) {
		execl("/bench", "bench", "/dev/sda", "64", "1024", (char *)NULL);
		perror("exec /bench");
		_exit(1);
	}
	int st = 0;
	waitpid(p, &st, 0);

	printf("\n--- done. Powering off. ---\n");
	fflush(stdout);
	sleep(1);
	reboot(RB_POWER_OFF);
	return 0;
}
