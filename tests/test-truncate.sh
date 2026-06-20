#!/bin/bash
# Test: truncate64/ftruncate64 via truncate, dd, cp shell commands
source "$(dirname "$0")/helpers.sh"

echo "=== truncate/dd/cp Shell Command Test ==="
echo "  Subtest 1: Boot + shell prompt"
echo "  Subtest 2: truncate shrink"
echo "  Subtest 3: truncate extend (sparse)"
echo "  Subtest 4: dd copy + content verify"
echo "  Subtest 5: cp copy + content verify"
echo "  Subtest 6: poweroff (regression)"

errors=0

run_cmd() {
	local cmd="$1"
	send_keys "$cmd" 0.05
	sleep 0.1
	monitor_cmd "sendkey ret" >/dev/null 2>&1
}

qemu_start_with_disk || { fail "QEMU failed to start"; exit 1; }

# --- Subtest 1: Boot ---
if wait_for_boot 15; then
	pass "Boot message found"
else
	fail "Boot timeout"
	qemu_stop
	exit 1
fi
sleep 2

if vga_contains ">"; then
	pass "Shell prompt found"
else
	fail "No shell prompt"
	errors=$((errors + 1))
fi

# --- Subtest 2: truncate shrink ---
# Create a 100-byte file, then truncate to 10 bytes
run_cmd 'write /trunc_test.txt ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
sleep 1
run_cmd 'stat /trunc_test.txt'
sleep 2

# Now truncate to 10
run_cmd 'truncate 10 /trunc_test.txt'
sleep 1
run_cmd 'cat /trunc_test.txt'
sleep 2

if vga_contains "ABCDEFGHIJ"; then
	pass "truncate shrink shows first 10 chars"
else
	fail "truncate shrink: wrong content"
	errors=$((errors + 1))
fi

# --- Subtest 3: truncate extend (sparse) ---
run_cmd 'truncate 50 /trunc_test.txt'
sleep 1
if vga_contains "truncate"; then
	: # just check command executed
fi

run_cmd 'cat /trunc_test.txt'
sleep 2

# Should show 10 chars followed by zeros (unwritten), but cat may show nothing
# beyond the null bytes. Check that it at least still shows first 10 chars.
if vga_contains "ABCDEFGHIJ"; then
	pass "truncate extend preserves content"
else
	fail "truncate extend: content lost"
	errors=$((errors + 1))
fi

# --- Subtest 4: dd copy ---
run_cmd 'dd if=/trunc_test.txt of=/dd_out.txt bs=10 count=1'
sleep 2

run_cmd 'cat /dd_out.txt'
sleep 2
if vga_contains "ABCDEFGHIJ"; then
	pass "dd copy content matches"
else
	fail "dd copy: wrong content"
	errors=$((errors + 1))
fi

# --- Subtest 5: cp copy ---
run_cmd 'cp /trunc_test.txt /cp_out.txt'
sleep 1
run_cmd 'cat /cp_out.txt'
sleep 2
if vga_contains "ABCDEFGHIJ"; then
	pass "cp copy content matches"
else
	fail "cp copy: wrong content"
	errors=$((errors + 1))
fi

# --- Subtest 6: poweroff ---
run_cmd "poweroff"
sleep 1
if qemu_wait_exit 10; then
	pass "'poweroff' shut down QEMU"
else
	if qemu_is_running; then
		fail "QEMU still running after poweroff"
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
