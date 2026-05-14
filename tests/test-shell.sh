#!/bin/bash
# Test: Shell prompt, greeting command, and poweroff command
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat || exit 1

echo "=== Shell Interaction Test ==="
echo "  Subtest 1: Shell prompt '>' appears after boot"
echo "  Subtest 2: 'greeting' prints 'hello'"
echo "  Subtest 3: 'poweroff' shuts down QEMU"

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

# --- Subtest 2: greeting command ---
for ch in g r e e t i n g; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

if vga_contains "hello"; then
	pass "'greeting' produced 'hello' output"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'hello' not found after 'greeting'. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 3: poweroff command ---
for ch in p o w e r o f f; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

if qemu_wait_exit 10; then
	pass "'poweroff' shut down QEMU (process exited)"
else
	if qemu_is_running; then
		fail "QEMU still running after 'poweroff' (timeout 10s)"
		errors=$((errors + 1))
	else
		pass "'poweroff' shut down QEMU"
	fi
fi

qemu_stop
exit $errors
