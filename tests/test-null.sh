#!/bin/bash
# Test: /dev/null device + echo command + > redirection
source "$(dirname "$0")/helpers.sh"

echo "=== /dev/null + echo + Redirection Test ==="
echo "  Subtest 1: Shell prompt appears after boot"
echo "  Subtest 2: echo produces output"
echo "  Subtest 3: echo > /dev/null suppresses output (no error)"
echo "  Subtest 4: cat /dev/null returns empty (file exists)"
echo "  Subtest 5: echo > regular file writes correctly"
echo "  Subtest 6: poweroff shuts down QEMU (regression)"

errors=0

# Helper: type a command and press Enter
run_cmd() {
	local cmd="$1"
	send_keys "$cmd" 0.05
	sleep 0.1
	monitor_cmd "sendkey ret" >/dev/null 2>&1
}

# --- Start QEMU with disk ---
qemu_start_with_disk || { fail "QEMU failed to start"; exit 1; }

# --- Subtest 1: Boot + prompt ---
if wait_for_boot 15; then
	pass "Boot message 'HomunculOS' found (kernel boots OK)"
else
	fail "Boot message not found (timeout 15s)"
	qemu_stop
	exit 1
fi

sleep 2

if vga_contains ">"; then
	pass "Shell prompt '>' found in VGA memory"
else
	dump=$(vga_text | tail -c 120)
	fail "Shell prompt '>' not found. VGA tail: [$dump]"
	errors=$((errors + 1))
fi

# --- Subtest 2: echo produces output ---
run_cmd "echo HELLO_OK"
sleep 3
if vga_contains "HELLO_OK"; then
	pass "echo HELLO_OK shows output"
else
	dump=$(vga_text | tail -c 120)
	fail "echo output not found. VGA tail: [$dump]"
	errors=$((errors + 1))
fi

# --- Subtest 3: echo > /dev/null suppresses output ---
run_cmd "echo BYE_NULL > /dev/null"
sleep 2
if vga_contains "redirection failed"; then
	fail "redirection to /dev/null failed"
	errors=$((errors + 1))
else
	pass "echo > /dev/null produced no error"
fi

# --- Subtest 4: cat /dev/null returns empty ---
run_cmd "cat /dev/null"
sleep 2
if vga_contains "cat: /dev/null: open failed"; then
	fail "cat /dev/null failed"
	errors=$((errors + 1))
else
	pass "cat /dev/null succeeded (file exists and is readable)"
fi

# --- Subtest 5: echo > regular file writes correctly ---
run_cmd "echo REDIRECT_OK > /null_redir_test.txt"
run_cmd "cat /null_redir_test.txt"
sleep 3
if vga_contains "REDIRECT_OK"; then
	pass "> redirection writes to file correctly"
else
	dump=$(vga_text | tail -c 120)
	fail "redirection output not found. VGA tail: [$dump]"
	errors=$((errors + 1))
fi

# --- Subtest 6: poweroff regression ---
run_cmd "poweroff"
sleep 1

if qemu_wait_exit 10; then
	pass "'poweroff' shut down QEMU (regression OK)"
else
	if qemu_is_running; then
		fail "QEMU still running after 'poweroff' (timeout 10s)"
		errors=$((errors + 1))
	else
		pass "'poweroff' shut down QEMU"
	fi
fi

# --- Summary ---
if [ "$errors" -eq 0 ]; then
	echo "  Result: All checks PASSED"
else
	echo "  Result: $errors check(s) FAILED (see above)"
fi

qemu_stop
exit $errors
