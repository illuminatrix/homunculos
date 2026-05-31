#!/bin/bash
# Test: ext2 write support (create file, write, mkdir, symlink, readlink, unlink)
source "$(dirname "$0")/helpers.sh"

echo "=== ext2 Write Support Test ==="
echo "  Subtest 1: Boot welcome message (regression)"
echo "  Subtest 2: Shell prompt '>' appears"
echo "  Subtest 3: 'ext2wrtest' binary runs"
echo "  Subtest 4: ext2wrtest VGA output shows file create/write/readback"
echo "  Subtest 5: ext2wrtest VGA output shows mkdir+nested"
echo "  Subtest 6: ext2wrtest VGA output shows symlink+readlink"
echo "  Subtest 7: ext2wrtest VGA output shows unlink"
echo "  Subtest 8: 'poweroff' shuts down QEMU (regression)"

errors=0

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

# --- Subtest 3: Run ext2wrtest binary ---
for ch in e x t 2 w r t e s t; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 4

vga_full=$(vga_text)
if echo "$vga_full" | grep -q "RESULT: All tests PASSED"; then
	pass "'ext2wrtest' reported all tests passed"
else
	vga_escaped=$(echo "$vga_full" | sed 's/[[:space:]]/\\s/g' | head -c 400)
	fail "Not all tests passed. VGA: [$vga_escaped]"
	errors=$((errors + 1))
fi

# Check individual PASS lines
vga_full=$(vga_text)

if echo "$vga_full" | grep -q "open(O_CREAT|O_WRONLY)"; then
	pass "Subtest 4: ext2wrtest VGA output shows file create/write/readback"
else
	fail "Subtest 4: ext2wrtest VGA output missing file create/write/readback"
	errors=$((errors + 1))
fi

if echo "$vga_full" | grep -q "mkdir /ext2wrdir"; then
	pass "Subtest 5: ext2wrtest VGA output shows mkdir+nested"
else
	fail "Subtest 5: ext2wrtest VGA output missing mkdir+nested"
	errors=$((errors + 1))
fi

if echo "$vga_full" | grep -q "symlink"; then
	pass "Subtest 6: ext2wrtest VGA output shows symlink+readlink"
else
	fail "Subtest 6: ext2wrtest VGA output missing symlink+readlink"
	errors=$((errors + 1))
fi

if echo "$vga_full" | grep -q "unlink"; then
	pass "Subtest 7: ext2wrtest VGA output shows unlink"
else
	fail "Subtest 7: ext2wrtest VGA output missing unlink"
	errors=$((errors + 1))
fi

# --- Subtest 8: poweroff regression ---
for ch in p o w e r o f f; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
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
