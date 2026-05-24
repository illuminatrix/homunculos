#!/bin/bash
# Test: stat/fstat/lstat syscalls (SYS_stat=106, SYS_lstat=107, SYS_fstat=108)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== stat/fstat/lstat Test ==="
echo "  Subtest 1: Boot + shell prompt"
echo "  Subtest 2: stat / (root directory)"
echo "  Subtest 3: stat /dev (tmpfs dir)"
echo "  Subtest 4: stat /dev/hello (device node)"
echo "  Subtest 5: poweroff (regression)"

prepare_basic_disk || { fail "Failed to prepare disk"; exit 1; }
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

# --- Subtest 2: stat / (root ext2 directory ---
send_keys "stat /" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

if vga_contains "ino="; then
	pass "stat / produced output"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat / did not produce output. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

if vga_contains "mode="; then
	pass "stat / contains mode (040755 for directory)"
else
	fail "stat / missing mode"
	errors=$((errors + 1))
fi

# --- Subtest 3: stat /dev (tmpfs directory) ---
send_keys "stat /dev" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

if vga_contains "ino="; then
	pass "stat /dev produced output"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat /dev did not produce output. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

if vga_contains "mode="; then
	pass "stat /dev contains mode"
else
	fail "stat /dev missing mode"
	errors=$((errors + 1))
fi

# --- Subtest 4: stat /dev/hello (device node) ---
send_keys "stat /dev/hello" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

if vga_contains "ino="; then
	pass "stat /dev/hello produced output"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat /dev/hello did not produce output. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

if vga_contains "mode="; then
	pass "stat /dev/hello contains mode"
else
	fail "stat /dev/hello missing mode"
	errors=$((errors + 1))
fi

# --- Subtest 5: poweroff ---
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
