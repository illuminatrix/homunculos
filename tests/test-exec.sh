#!/bin/bash
# Test: ELF exec (SYS_exec) and waitpid (SYS_waitpid) feature via shell commands
#
# Verifies:
#   1. Kernel boots with welcome message (regression)
#   2. Shell prompt '>' appears (regression)
#   3. "greeting" produces "hello" (regression - fork+exec+waitpid still work)
#   4. Ring 3 CS register is correct throughout (regression)
#   5. "poweroff" shuts down QEMU (regression)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== ELF Exec / Waitpid Feature Test ==="
echo "  Subtest 1: Boot welcome message"
echo "  Subtest 2: Shell prompt '>' appears"
echo "  Subtest 3: 'greeting' still works (regression - fork+exec+waitpid)"
echo "  Subtest 4: Ring 3 CS register (regression)"
echo "  Subtest 5: 'poweroff' shuts down QEMU (regression)"

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

assert_user_mode "Subtest 1: Ring 3 CS register after boot"
errors=$((errors + $?))

# --- Subtest 2: Shell prompt ---
if vga_contains ">"; then
	pass "Shell prompt '>' in VGA (shell task running in ring 3)"
else
	dump_tail=$(vga_text | tr '\0' ' ' | tail -c 120)
	fail "Shell prompt not found. VGA tail: [$dump_tail]"
	errors=$((errors + 1))
fi

assert_user_mode "Subtest 2: Ring 3 CS register after shell prompt"
errors=$((errors + $?))

# --- Subtest 3: Greeting command regression ---
for ch in g r e e t i n g; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

if vga_contains "hello"; then
	pass "'greeting' produced 'hello' (SYS_read+SYS_write regression OK)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'hello' not found after 'greeting'. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

assert_user_mode "Subtest 3: Ring 3 CS register after greeting (regression)"
errors=$((errors + $?))

# --- Subtest 5: Poweroff regression ---
for ch in p o w e r o f f; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
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
