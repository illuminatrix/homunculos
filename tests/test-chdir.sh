#!/bin/bash
# Test: chdir/getcwd syscalls
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== chdir/getcwd Test ==="
echo "  Subtest 1: pwd prints /"
echo "  Subtest 2: cd /dev prints /dev"
echo "  Subtest 3: cd .. prints /"
echo "  Subtest 4: cd /dev/vga fails (not a dir)"
echo "  Subtest 5: cd bin; ls shows files in /bin"

qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

# --- Subtest 1: Boot + Shell prompt ---
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

# --- Subtest 2: pwd prints / ---
send_keys "pwd" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 4

if vga_contains "/"; then
	pass "pwd printed /"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "pwd did not print /. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 3: cd /dev prints /dev ---
send_keys "cd /dev" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 5

if vga_contains "/dev"; then
	pass "cd /dev printed /dev"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "cd /dev did not print /dev. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 4: cd .. prints / from /dev ---
send_keys "cd .." 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 5

if vga_contains "/"; then
	pass "cd .. printed /"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "cd .. did not print /. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 5: cd to non-dir fails ---
send_keys "cd /dev/vga" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 5

if vga_contains "failed"; then
	pass "cd /dev/vga correctly failed"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "cd /dev/vga should have failed. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 6: cd bin as built-in, then ls shows files in /bin ---
send_keys "cd bin" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

send_keys "ls" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 5

if vga_contains "pwd" && vga_contains "cd" && vga_contains "hello"; then
	pass "cd bin + ls shows /bin contents"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "cd bin + ls did not show /bin contents. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 7: poweroff ---
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

# --- Summary ---
if [ "$errors" -eq 0 ]; then
	echo "  Result: All checks PASSED"
else
	echo "  Result: $errors check(s) FAILED"
fi

qemu_stop
exit $errors
