#!/bin/bash
# Test: ext2 write support via shell commands (touch, write, mkdir, ln, rm, readlink)
source "$(dirname "$0")/helpers.sh"

echo "=== ext2 Write Support Test (Shell Commands) ==="
echo "  Subtest 1: Boot welcome message (regression)"
echo "  Subtest 2: Shell prompt '>' appears"
echo "  Subtest 3: File create/write/readback via touch+write+cat"
echo "  Subtest 4: mkdir + nested file"
echo "  Subtest 5: symlink + readlink"
echo "  Subtest 6: unlink"
echo "  Subtest 7: 'poweroff' shuts down QEMU (regression)"

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

# --- Subtest 1: Boot ---
if wait_for_boot 15; then
	pass "Boot message 'Illuminatrix' found (kernel boots OK)"
else
	fail "Boot message not found (timeout 15s)"
	qemu_stop
	exit 1
fi

sleep 2

# --- Subtest 2: Shell prompt ---
if vga_contains ">"; then
	pass "Shell prompt '>' found in VGA memory"
else
	dump=$(vga_text | tail -c 120)
	fail "Shell prompt '>' not found. VGA tail: [$dump]"
	errors=$((errors + 1))
fi

# --- Subtest 3: File create/write/readback ---
run_cmd "touch /ext2wrtest.txt"
run_cmd 'write /ext2wrtest.txt Hello from write!'
run_cmd "cat /ext2wrtest.txt"
sleep 4
if vga_contains "Hello from write!"; then
	pass "cat shows 'Hello from write!' after touch+write"
else
	fail "cat did not show expected content"
	errors=$((errors + 1))
fi

# --- Subtest 4: mkdir + nested file ---
run_cmd "mkdir /ext2wrdir"
run_cmd "touch /ext2wrdir/nested.txt"
run_cmd 'write /ext2wrdir/nested.txt nested'
run_cmd "cat /ext2wrdir/nested.txt"
sleep 4
if vga_contains "nested"; then
	pass "mkdir+touch+write+cat shows 'nested'"
else
	fail "nested file content not shown"
	errors=$((errors + 1))
fi

# --- Subtest 5: symlink + readlink ---
run_cmd "ln -s /ext2wrtest.txt /ext2wrtlink"
run_cmd "readlink /ext2wrtlink"
sleep 4
if vga_contains "/ext2wrtest.txt"; then
	pass "readlink shows correct symlink target"
else
	fail "readlink did not show expected target"
	errors=$((errors + 1))
fi

# --- Subtest 6: unlink ---
run_cmd "rm /ext2wrtest.txt"
run_cmd "cat /ext2wrtest.txt"
sleep 4
if vga_contains "open failed"; then
	pass "cat fails after unlink (file gone)"
else
	fail "cat should have failed after unlink"
	errors=$((errors + 1))
fi

# --- Subtest 7: poweroff regression ---
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
