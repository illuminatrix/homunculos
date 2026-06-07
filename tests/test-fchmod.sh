#!/bin/bash
# Test: fchmod syscall (SYS_fchmod=94) via shell command
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Fchmod Test ==="

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

# --- fchmod 644 via shell command ---
send_keys "fchmod /hello.txt 644" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "fchmod ok"; then
	pass "fchmod 644 succeeded"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "fchmod 644 did not report ok. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Verify via stat ---
send_keys "stat /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

MODE_644=$((0x8000 | 00400 | 00200 | 00040 | 00004))
if vga_contains "mode=${MODE_644}"; then
	pass "stat confirms mode 644 (0100644 = ${MODE_644})"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat did not show mode 644. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Poweroff ---
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
