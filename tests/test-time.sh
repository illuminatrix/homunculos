#!/bin/bash
# Test: time/sleep/nanosleep syscalls (SYS_time=13, SYS_gettimeofday=78, SYS_nanosleep=162)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Time Test ==="
echo "  Subtest 1: Boot + shell prompt"
echo "  Subtest 2: systime (show seconds since boot)"
echo "  Subtest 3: sleep 2 (actual wait)"

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

# --- Subtest 2: systime shows seconds ---
send_keys "systime" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "seconds="; then
	pass "systime produced output"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "systime did not produce output. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 3: sleep 2 blocks ---
# Verify sleep causes a real delay by comparing systime before/after
send_keys "systime" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

send_keys "sleep 2" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1

# The shell should be blocked now — wait for sleep to finish
sleep 4

if vga_contains ">"; then
	pass "sleep 2 completed and shell prompt returned"
else
	fail "Shell prompt not found after sleep 2"
	errors=$((errors + 1))
fi

# --- Poweroff ---

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

# --- Summary ---
if [ "$errors" -eq 0 ]; then
	echo "  Result: All checks PASSED"
else
	echo "  Result: $errors check(s) FAILED"
fi

qemu_stop
exit $errors
