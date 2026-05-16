#!/bin/bash
# Test: ELF exec (SYS_exec) and join (SYS_join) feature via the shell "run" command
#
# Verifies:
#   1. Kernel boots with welcome message (regression)
#   2. Shell prompt '>' appears (regression)
#   3. "run" command forks, execs hello.elf which prints "Hello, World!" from ring 3
#   4. Parent process waits via join() and prints "child N done"
#   5. Shell prompt returns after the run command (parent task survives)
#   6. Ring 3 CS register is correct throughout (regression)
#   7. "greeting" produces "hello" (regression - unrelated commands still work)
#   8. "poweroff" shuts down QEMU (regression)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat || exit 1

echo "=== ELF Exec / Join Feature Test ==="
echo "  Subtest 1: Boot welcome message"
echo "  Subtest 2: Shell prompt '>' appears"
echo "  Subtest 3: 'run' -> child ELF prints 'Hello, World!'"
echo "  Subtest 4: Parent prints 'child N done' after join()"
echo "  Subtest 5: Shell prompt returns after exec/join"
echo "  Subtest 6: Ring 3 CS register (regression)"
echo "  Subtest 7: 'greeting' still works (regression)"
echo "  Subtest 8: 'poweroff' shuts down QEMU (regression)"

qemu_start 15 || { fail "QEMU failed to start"; exit 1; }

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

# --- Subtest 3: Type "run" command, verify child ELF output ---
for ch in r u n; do
	monitor_cmd "sendkey $ch" >/dev/null 2>&1
	sleep 0.08
done
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 4

if vga_contains "Hello, World!"; then
	pass "Child ELF output 'Hello, World!' found in VGA (exec + write syscall from ring 3)"
else
	vga_raw=$(vga_text | tail -c 300)
	fail "'Hello, World!' not found after 'run' command. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

assert_user_mode "Subtest 3: Ring 3 CS register after hello.elf exec+exit"
errors=$((errors + $?))

# --- Subtest 4: Verify parent prints "child N done" ---
# The PID is dynamic so we match "child" followed by space, digits, space, "done"
vga_full=$(vga_text)
if echo "$vga_full" | grep -qE 'child [0-9]+ done'; then
	child_line=$(echo "$vga_full" | grep -oE 'child [0-9]+ done')
	pass "Parent output '$child_line' found (join() returned child PID)"
else
	vga_raw=$(vga_text | tail -c 300)
	fail "'child N done' not found after 'run' command. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

assert_user_mode "Subtest 4: Ring 3 CS register after join() in parent"
errors=$((errors + $?))

# --- Subtest 5: Shell prompt returns after exec/join ---
sleep 1
if vga_contains ">"; then
	pass "Shell prompt '>' returned after 'run' command (parent task alive)"
else
	dump_tail=$(vga_text | tr '\0' ' ' | tail -c 120)
	fail "Shell prompt not found after 'run'. VGA tail: [$dump_tail]"
	errors=$((errors + 1))
fi

assert_user_mode "Subtest 5: Ring 3 CS register after shell prompt return"
errors=$((errors + $?))

# --- Subtest 6: Greeting command regression ---
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

assert_user_mode "Subtest 6: Ring 3 CS register after greeting (regression)"
errors=$((errors + $?))

# --- Subtest 7: Poweroff regression ---
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
