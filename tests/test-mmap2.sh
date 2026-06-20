#!/bin/bash
# Test: mmap2/munmap/mprotect syscalls via mmap_test ELF binary
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== mmap2 / munmap / mprotect Test ==="

# Pre-stage the test binary
MMAP_TEST_BIN="$(dirname "$0")/mmap_test.elf"
dd if="$DISK_IMG" bs=512 skip=2048 of=.part.img 2>/dev/null
debugfs -w .part.img -R "write $MMAP_TEST_BIN /bin/mmap_test" 2>/dev/null
dd if=.part.img of="$DISK_IMG" bs=512 seek=2048 conv=notrunc 2>/dev/null
rm -f .part.img

qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

if wait_for_boot 15; then
	pass "Boot message found"
else
	fail "Boot message not found"
	qemu_stop
	exit 1
fi

sleep 2

if vga_contains ">"; then
	pass "Shell prompt '>' found"
else
	fail "Shell prompt not found"
	errors=$((errors + 1))
fi

send_keys "/bin/mmap_test" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 4

if vga_contains "ALL TESTS PASS"; then
	pass "mmap_test: ALL TESTS PASS"
elif vga_contains "FAIL"; then
	vga_raw=$(vga_text | tail -c 300)
	fail "mmap_test reported failure. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
elif vga_contains "not found"; then
	fail "mmap_test binary not found on disk"
	errors=$((errors + 1))
else
	vga_raw=$(vga_text | tail -c 200)
	fail "mmap_test produced unexpected output: [$vga_raw]"
	errors=$((errors + 1))
fi

send_keys "poweroff" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

if qemu_wait_exit 10; then
	pass "'poweroff' shut down QEMU"
else
	if qemu_is_running; then
		fail "QEMU still running after 'poweroff'"
		errors=$((errors + 1))
	else
		pass "'poweroff' shut down QEMU"
	fi
fi

if [ "$errors" -eq 0 ]; then
	echo "  Result: All checks PASSED"
else
	echo "  Result: $errors check(s) FAILED"
fi

qemu_stop
exit $errors
