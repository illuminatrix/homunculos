#!/bin/bash
# Test: rename syscall (SYS_rename=38) via mv command
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== MV (Rename) Test ==="

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

# --- 1) Verify /hello.txt exists before rename ---
send_keys "stat /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "stat failed"; then
	fail "/hello.txt should exist before rename but stat failed"
	errors=$((errors + 1))
else
	pass "/hello.txt exists before rename"
fi

# --- 2) Rename hello.txt -> hello2.txt ---
send_keys "mv /hello.txt /hello2.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "mv failed"; then
	fail "mv command failed"
	errors=$((errors + 1))
else
	pass "mv /hello.txt /hello2.txt succeeded"
fi

# --- 3) Verify /hello2.txt exists after rename ---
send_keys "stat /hello2.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "stat failed"; then
	fail "/hello2.txt should exist after rename but stat failed"
	errors=$((errors + 1))
else
	pass "/hello2.txt exists after rename"
fi

# --- 4) Verify /hello.txt is gone after rename ---
send_keys "stat /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "stat failed"; then
	pass "/hello.txt is gone after rename (expected)"
else
	fail "/hello.txt still exists after rename"
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
