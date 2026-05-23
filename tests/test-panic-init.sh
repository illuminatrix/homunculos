#!/bin/bash
# Test: Kernel panics when no init binary is found
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat || exit 1

echo "=== Kernel Panic Init Test ==="

QEMU_PID=""
MONITOR_SOCK="${TESTS_DIR}/qemu-monitor.sock"
cleanup() {
	if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
		kill "$QEMU_PID" 2>/dev/null
		wait "$QEMU_PID" 2>/dev/null
	fi
	rm -f "$MONITOR_SOCK"
}
trap cleanup EXIT INT TERM

# Start QEMU with root= but without init= and without a disk
# The kernel mounts ext2 (fails, no disk) then tries/init,/sbin/init,/bin/sh
# on the tmpfs root — none exist → panic
qemu-system-i386 -kernel "$KERNEL_BIN" \
	-display none \
	-monitor "unix:${MONITOR_SOCK},server,nowait" \
	-no-reboot \
	-append "root=/dev/hda1" \
	&
QEMU_PID=$!

waited=0
while [ ! -S "$MONITOR_SOCK" ] && [ $waited -lt 10 ]; do
	sleep 0.5
	waited=$((waited + 1))
done
if [ ! -S "$MONITOR_SOCK" ]; then
	echo "ERROR: QEMU monitor socket not created after 5s" >&2
	exit 1
fi

# Wait for panic message in VGA
waited=0
found=0
while [ $waited -lt 15 ]; do
	if vga_contains "KERNEL PANIC" 2>/dev/null; then
		found=1
		break
	fi
	sleep 1
	waited=$((waited + 1))
done

if [ "$found" -eq 1 ]; then
	pass "Kernel panicked when no init binary is found"
else
	dump_head=$(vga_text | head -c 80)
	fail "Panic message not found. VGA: [$dump_head]"
	exit 1
fi
