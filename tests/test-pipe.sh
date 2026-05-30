#!/bin/bash
# Test: Pipe support in shell
# Tests that "greeting | cat" prints "hello"
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Pipe Support Test ==="
echo "  Subtest 1: Shell prompt appears"
echo "  Subtest 2: 'greeting | cat' produces 'hello' output"
echo "  Subtest 3: 'poweroff' shuts down QEMU"

prepare_basic_disk || { fail "Failed to prepare disk"; exit 1; }
qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

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

# --- Subtest 2: "greeting | cat" via pipe ---
# Type "greeting | cat" using QEMU sendkey
# '|' is shift-backslash on US keyboards
for ch in g r e e t i n g; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
monitor_cmd "sendkey spc" >/dev/null 2>&1
sleep 0.08
monitor_cmd "sendkey shift-backslash" >/dev/null 2>&1
sleep 0.08
monitor_cmd "sendkey spc" >/dev/null 2>&1
sleep 0.08
for ch in c a t; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "hello"; then
	pass "'greeting | cat' produced 'hello' output"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'hello' not found after 'greeting | cat'. VGA tail: [$vga_raw]"
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
