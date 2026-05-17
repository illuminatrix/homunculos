#!/bin/bash
# Test: tmpfs filesystem via ls and cat shell commands
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat || exit 1

echo "=== tmpfs Filesystem Test ==="
echo "  Subtest 1: Shell prompt '>' appears after boot"
echo "  Subtest 2: 'ls' shows 'dev' in root"
echo "  Subtest 3: 'ls /dev' shows 'hello'"
echo "  Subtest 4: 'cat /dev/hello' shows 'hello fs'"
echo "  Subtest 5: 'poweroff' shuts down QEMU (regression)"

qemu_start 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

# --- Subtest 1: Boot + Prompt ---
if wait_for_boot 15; then
	pass "Boot message found in VGA"
else
	fail "Boot message not found (timeout 15s)"
	qemu_stop
	exit 1
fi

sleep 2

if vga_contains ">"; then
	pass "Shell prompt '>' found in VGA memory"
else
	dump_tail=$(vga_text | tr '\0' ' ' | tail -c 120)
	fail "Shell prompt '>' not found. VGA tail: [$dump_tail]"
	errors=$((errors + 1))
fi

# --- Subtest 2: ls (root) ---
for ch in l s; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

vga_full=$(vga_text)
if echo "$vga_full" | grep -q "dev"; then
	pass "'ls' shows 'dev' directory in root"
else
	fail "'dev' not found in 'ls' output. VGA tail: [$(echo "$vga_full" | tail -c 200)]"
	errors=$((errors + 1))
fi

# --- Subtest 3: ls /dev ---
for ch in l s spc slash d e v; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

vga_full=$(vga_text)
if echo "$vga_full" | grep -q "hello" && echo "$vga_full" | grep -q "vga" && echo "$vga_full" | grep -q "kbd"; then
	pass "'ls /dev' shows 'hello', 'vga', 'kbd'"
else
	fail "'ls /dev' missing entries. VGA tail: [$(echo "$vga_full" | tail -c 200)]"
	errors=$((errors + 1))
fi

# --- Subtest 4: cat /dev/hello ---
for ch in c a t spc slash d e v slash h e l l o; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

vga_full=$(vga_text)
if echo "$vga_full" | grep -q "hello fs"; then
	pass "'cat /dev/hello' printed file contents correctly"
else
	vga_raw=$(echo "$vga_full" | tail -c 200)
	fail "'hello fs' not found. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 5: poweroff regression ---
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
