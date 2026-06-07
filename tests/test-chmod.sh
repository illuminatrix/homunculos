#!/bin/bash
# Test: chmod syscall (SYS_chmod=15) with mode verification via stat
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Chmod Test ==="

# Mode constants (must match kernel/vfs.h S_IFxxx)
S_IFREG=$((0x8000))   # 32768

# Permission bits (must match kernel/vfs.h S_Ixxx)
S_IRUSR=$((00400))
S_IWUSR=$((00200))
S_IRGRP=$((00040))
S_IROTH=$((00004))
S_IRWXU=$((00700))
S_IRWXG=$((00070))
S_IRWXO=$((00007))

# Computed full mode values
MODE_644=$((S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))   # 0100644 = 33188
MODE_777=$((S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO))             # 0100777 = 33279
MODE_600=$((S_IFREG | S_IRUSR | S_IWUSR))                        # 0100600 = 33152

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

# --- Set mode to 644 via chmod ---
send_keys "chmod 644 /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

# Check chmod didn't print "chmod failed"
if vga_contains "chmod failed"; then
	fail "chmod 644 failed"
	errors=$((errors + 1))
else
	pass "chmod 644 succeeded"
fi

# --- Verify mode via stat (mode 0100644 = 33188 decimal) ---
send_keys "stat /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "mode=${MODE_644}"; then
	pass "stat shows mode 644 (0100644 = ${MODE_644})"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat did not show mode 644. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Change to mode 777 ---
send_keys "chmod 777 /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "chmod failed"; then
	fail "chmod 777 failed"
	errors=$((errors + 1))
else
	pass "chmod 777 succeeded"
fi

# --- Verify mode changed (mode 0100777 = 33279 decimal) ---
send_keys "stat /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "mode=${MODE_777}"; then
	pass "stat shows mode 777 (0100777 = ${MODE_777})"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat did not show mode 777. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Change to mode 600 ---
send_keys "chmod 600 /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "chmod failed"; then
	fail "chmod 600 failed"
	errors=$((errors + 1))
else
	pass "chmod 600 succeeded"
fi

# --- Verify mode again (mode 0100600 = 33152 decimal) ---
send_keys "stat /hello.txt" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "mode=${MODE_600}"; then
	pass "stat shows mode 600 (0100600 = ${MODE_600})"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat did not show mode 600. VGA tail: [$vga_raw]"
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
