#!/usr/bin/env bash
# run-io-lab.sh — same disk, same write, two transfer modes.
#
#   ./run-io-lab.sh            run both modes and print the comparison
#   ./run-io-lab.sh pio        run the PIO mode only
#   ./run-io-lab.sh dma        run the DMA mode only
#
# Needs: gcc, cpio, qemu-system-x86_64, and a kernel with the PATA driver
# built in (CONFIG_ATA=y, CONFIG_ATA_PIIX=y) — see README.md.
set -euo pipefail
cd "$(dirname "$0")"

KERNEL=${KERNEL:-bzImage}
SIZE_MB=${SIZE_MB:-64}

# Hardware virtualization: without it QEMU emulates the CPU (TCG) and there
# are no VM exits at all — the PIO cost is pure instruction emulation.
if [[ -r /dev/kvm ]]; then
    ACCEL=(-enable-kvm -cpu host)
    HAVE_KVM=1
else
    ACCEL=()
    HAVE_KVM=0
    echo "note: /dev/kvm unavailable — running under TCG, exit counts will be zero"
fi

# Count VM exits with perf, when it is there and we are root.
PERF=()
if [[ $HAVE_KVM == 1 ]] && command -v perf >/dev/null && [[ $EUID == 0 ]]; then
    PERF=(perf stat -e kvm:kvm_exit,kvm:kvm_pio -a)
fi
[[ -f $KERNEL ]] || { echo "error: kernel image '$KERNEL' not found. See README.md"; exit 1; }

echo "[1/3] compiling"
gcc -O2 -Wall -static -o bench bench.c
gcc -O2 -Wall -static -o init init-io.c

echo "[2/3] packing initramfs and a scratch disk"
rm -rf root && mkdir -p root/proc root/sys root/dev
cp init bench root/
( cd root && find . | cpio -o -H newc --quiet ) | gzip -9 > lab.img
[[ -f scratch.img ]] || dd if=/dev/zero of=scratch.img bs=1M count=$((SIZE_MB + 16)) status=none

boot() {   # boot <mode>
    local extra=""
    [[ $1 == pio ]] && extra="libata.force=pio0"
    # Measure QEMU's own CPU time on the host: in PIO mode every port access
    # traps out of the guest, so the real cost shows up here, not inside.
    /usr/bin/time -f "%e %U %S" -o "host-$1.time" \
    ${PERF:+"${PERF[@]}"} ${PERF:+-o} ${PERF:+"perf-$1.txt"} \
    qemu-system-x86_64 "${ACCEL[@]}" \
        -m 1G \
        -kernel "$KERNEL" \
        -initrd lab.img \
        -drive file=scratch.img,format=raw,if=ide \
        -append "console=ttyS0 quiet panic=1 $extra" \
        -nographic -no-reboot
}

run_one() {
    echo
    echo "=============== mode: $1 ==============="
    boot "$1" | tr -d '\r' | tee "out-$1.txt"
}

case "${1:-both}" in
    pio) run_one pio ;;
    dma) run_one dma ;;
    both)
        echo "[3/3] booting twice"
        run_one dma
        run_one pio
        echo
        echo "=============== comparison ==============="
        printf "%-6s %13s %13s %13s %12s %12s\n" \
               "mode" "throughput" "guest cpu" "HOST cpu" "VM exits" "PIO exits"
        for m in dma pio; do
            [[ -f out-$m.txt ]] || continue
            tput=$(awk -F': *' '/throughput/{print $2}' out-$m.txt | tr -d '\r')
            gcpu=$(awk -F': *' '/cpu per MiB/{print $2}' out-$m.txt | tr -d '\r')
            hcpu=$(awk '{printf "%.2f s", $2 + $3}' "host-$m.time" 2>/dev/null)
            ex=$(awk '/kvm:kvm_exit/{gsub(/[^0-9]/,"",$1); print $1}' "perf-$m.txt" 2>/dev/null)
            px=$(awk '/kvm:kvm_pio/{gsub(/[^0-9]/,"",$1); print $1}' "perf-$m.txt" 2>/dev/null)
            printf "%-6s %13s %13s %13s %12s %12s\n" \
                   "$m" "$tput" "$gcpu" "$hcpu" "${ex:--}" "${px:--}"
        done
        echo
        echo "Who moved the bytes in each case?"
        echo "Note: the guest cannot see the cost of its own VM exits."
        echo "In PIO mode that cost lands in the HOST column, and in the exit counts."
        echo
        echo "Average time per exit:  perf kvm stat record -a -- sleep 5   (see README)"
        ;;
    *) echo "usage: $0 [pio|dma|both]"; exit 1 ;;
esac
