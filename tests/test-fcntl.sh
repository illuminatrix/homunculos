#!/bin/bash
# Test: fcntl64 syscall (SYS_fcntl64=221) via fcntl_test ELF binary
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Fcntl Test ==="

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

# Run the fcntl_test ELF binary
send_keys "/bin/fcntl_test" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "fcntl_test: all OK"; then
	pass "fcntl test program produced output"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "fcntl test did not produce expected output. VGA tail: [$vga_raw]"
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
