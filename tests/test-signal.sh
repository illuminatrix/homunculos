#!/bin/bash
# Test: Signal delivery (SIGUSR1 handler, SIGKILL, SIG_IGN, kill/trap commands, Ctrl+C)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Signal Feature Test ==="
echo "  Subtest 1: Boot welcome message (regression)"
echo "  Subtest 2: Shell prompt '>' appears (regression)"
echo "  Subtest 3: signal_test binary (sys_signal/sys_kill/SIG_IGN)"
echo "  Subtest 4: 'trap 10 kill' (trap + kill commands together)"
echo "  Subtest 5: 'trap 2' + Ctrl+C (SIGINT trap via keyboard)"
echo "  Subtest 6: 'poweroff' shuts down QEMU (regression)"

# Add signal_test to the existing shared disk
SIGNAL_TEST_BIN="$(dirname "$0")/signal_test.elf"
dd if="$DISK_IMG" bs=512 skip=2048 of=.part.img 2>/dev/null
debugfs -w .part.img -R "write $SIGNAL_TEST_BIN /bin/signal_test" 2>/dev/null
dd if=.part.img of="$DISK_IMG" bs=512 seek=2048 conv=notrunc 2>/dev/null
rm -f .part.img

qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

# --- Subtest 1: Boot + Welcome ---
if wait_for_boot 15; then
	pass "Boot message 'Illuminatrix' found (kernel boots OK)"
else
	dump_head=$(vga_text | head -c 80)
	fail "Boot message not found. VGA: [$dump_head]"
	qemu_stop
	exit 1
fi

sleep 2

# --- Subtest 2: Shell prompt ---
if vga_contains ">"; then
	pass "Shell prompt '>' in VGA (shell task running)"
else
	dump_tail=$(vga_text | tr '\0' ' ' | tail -c 120)
	fail "Shell prompt not found. VGA tail: [$dump_tail]"
	errors=$((errors + 1))
fi

# --- Subtest 3: signal_test binary ---
send_keys "signal_test" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 5

if vga_contains "ALL TESTS PASS"; then
	pass "'signal_test' produced 'ALL TESTS PASS' (all signal checks)"
elif vga_contains "FAIL"; then
	vga_raw=$(vga_text | tail -c 300)
	fail "'signal_test' reported failure. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
elif vga_contains "not found"; then
	fail "'signal_test' binary not found on disk"
	errors=$((errors + 1))
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'signal_test' produced unexpected output. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 4: trap + kill self-test ---
# The command "trap 10 kill" sets a handler for SIGUSR1, forks a child that
# calls "kill <parent_pid> 10". If both commands work, "CAUGHT" is printed.
send_keys "trap 10 kill" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 5

if vga_contains "CAUGHT"; then
	pass "'trap 10 kill' produced 'CAUGHT' (trap+kill work together)"
elif vga_contains "waiting"; then
	vga_raw=$(vga_text | tail -c 300)
	fail "'trap 10 kill' is waiting but no CAUGHT. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
elif vga_contains "not found"; then
	fail "'trap' or 'kill' command not found"
	errors=$((errors + 1))
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'trap 10 kill' produced unexpected output. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 5: Ctrl+C interrupts trap with SIGINT handler ---
# "trap 2" sets a handler for SIGINT. When we send Ctrl+C, the handler
# catches it and prints "CAUGHT" instead of the process being terminated.
send_keys "trap 2" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

# Send Ctrl+C to deliver SIGINT to the trap process
monitor_cmd "sendkey ctrl-c" >/dev/null 2>&1
sleep 3

if vga_contains "CAUGHT"; then
	pass "Ctrl+C interrupted 'trap 2' and handler printed CAUGHT"
else
	vga_raw=$(vga_text | tail -c 300)
	time_output=$(vga_text | tail -c 300 | tr '\0' ' ')
	fail "CAUGHT not found after Ctrl+C on trap 2. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 6: Poweroff ---
send_keys "poweroff" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

if qemu_wait_exit 10; then
	pass "'poweroff' shut down QEMU (SYS_reboot regression OK)"
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
