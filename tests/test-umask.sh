#!/bin/bash
# Test: umask syscall (SYS_umask=60) and shell built-in
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat || exit 1

echo "=== Umask Test ==="

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

# --- Check default umask (should print 22 = octal 022) ---
send_keys "umask" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "22"; then
	pass "Default umask is 022 (printed as '22')"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "Default umask not '22'. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Set umask to 0, check return value is 22 (old mask) ---
send_keys "umask 0" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "22"; then
	pass "umask 0 returned old mask 22"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "umask 0 did not return 22. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Verify new umask is 0 ---
send_keys "umask" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "0"; then
	pass "New umask is 0"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "New umask not 0. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Restore default umask ---
send_keys "umask 022" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

# --- Open/create with umask: run 'umask 027 touch /x' as a single command ---
send_keys "umask 027 touch /x" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

send_keys "stat /x" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "33184"; then
	pass "umask 027 → touch → mode 0100640 (decimal 33184)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "Expected mode 33184 (0100640). VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- mkdir with umask: umask 077, mkdir /d, stat ---
send_keys "umask 077 mkdir /d" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

send_keys "stat /d" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "16832"; then
	pass "umask 077 → mkdir → mode 040700 (decimal 16832)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "Expected mode 16832 (040700). VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- mknod with umask: umask 027, mknod /n c 1 3, stat ---
send_keys "umask 027 mknod /n c 1 3" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

send_keys "stat /n" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "8608"; then
	pass "umask 027 → mknod → mode 020640 (decimal 8608)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "Expected mode 8608 (020640). VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Restore default umask ---
send_keys "umask 022" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

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
