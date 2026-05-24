#!/bin/bash
# Test: execv with argv (SYS_execve with argv/envp support)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== execv with argv Test ==="
echo "  Subtest 1: Boot + shell prompt"
echo "  Subtest 2: run /bin/hello (no extra args) prints argv[0]"
echo "  Subtest 3: run /bin/hello foo bar (with extra args)"
echo "  Subtest 4: poweroff (regression)"

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

# --- Subtest 2: run /bin/hello (no extra args) ---
send_keys "run /bin/hello" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 5

if vga_contains "argc=1" && vga_contains "argv[0]=/bin/hello"; then
	pass "hello.elf reports argc=1, argv[0]=/bin/hello"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "argv[0] not found. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

if vga_contains "exec: execv failed"; then
	fail "execv reported failure"
	errors=$((errors + 1))
else
	pass "execv did not report failure"
fi

# --- Subtest 3: run /bin/hello with extra arguments ---
send_keys "run /bin/hello foo bar" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 5

if vga_contains "argc=3" && vga_contains "argv[1]=foo" && vga_contains "argv[2]=bar"; then
	pass "argv[1]=foo and argv[2]=bar found (parameter passing works)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "argv parameters not found. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 4: poweroff ---
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
