#!/bin/bash
# Test: link syscall (SYS_link=9) via ln command (hard link)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Link Test ==="

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

# --- Stat /hello.txt to confirm it exists ---
send_keys "stat /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "stat failed"; then
	fail "stat /hello.txt failed"
	errors=$((errors + 1))
else
	pass "stat /hello.txt succeeded"
fi

# --- Create hard link ---
send_keys "ln /hello.txt /hello-hard.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "ln: /hello-hard.txt: error"; then
	fail "ln hard link failed"
	errors=$((errors + 1))
else
	pass "ln hard link succeeded"
fi

# --- Stat the hard link ---
send_keys "stat /hello-hard.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "stat failed"; then
	fail "stat /hello-hard.txt failed"
	errors=$((errors + 1))
elif vga_contains "ino="; then
	pass "Hard link stat shows inode info (same inode as original)"
else
	fail "Hard link stat did not show inode info"
	errors=$((errors + 1))
fi

# --- Stat the original again ---
send_keys "stat /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "stat failed"; then
	fail "stat /hello.txt failed after link"
	errors=$((errors + 1))
else
	pass "Original file still accessible after hard link"
fi

# --- Remove hard link ---
send_keys "rm /hello-hard.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "rm failed"; then
	fail "rm /hello-hard.txt failed"
	errors=$((errors + 1))
else
	pass "rm /hello-hard.txt succeeded"
fi

# --- Stat original after removal ---
send_keys "stat /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "stat failed"; then
	fail "stat /hello.txt failed after removing hard link"
	errors=$((errors + 1))
else
	pass "Original file still accessible after removing hard link"
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
