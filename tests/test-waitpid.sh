#!/bin/bash
# Test: waitpid with specific PID matching (pid > 0)
#
# Tests the new waitpid features exercised through the shell:
#   - pid > 0: wait for a specific child PID (used by shell for single commands)
#   - pid array tracking: waitpid for each segment in a pipeline
#
# The shell calls waitpid(pid, 0, 0) for each forked child, exercising
# the specific-pid matching path. Pipelines call waitpid for each segment.
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat || exit 1

echo "=== Waitpid Feature Test ==="
echo "  Subtest 1: Boot + shell prompt (regression)"
echo "  Subtest 2: hello via fork+exec+waitpid (pid > 0 matching)"
echo "  Subtest 3: hello foo bar (args via pid-specific waitpid)"
echo "  Subtest 4: greeting (pid-specific waitpid regression)"
echo "  Subtest 5: greeting | prepend (pipeline with multiple waitpid calls)"
echo "  Subtest 6: poweroff (regression)"

qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

# --- Subtest 1: Boot + Prompt ---
if wait_for_boot 15; then
	pass "Boot message 'HomunculOS' found"
else
	dump_head=$(vga_text | head -c 80)
	fail "Boot message not found. VGA: [$dump_head]"
	qemu_stop
	exit 1
fi

sleep 2

if vga_contains ">"; then
	pass "Shell prompt '>' found"
else
	dump_tail=$(vga_text | tr '\0' ' ' | tail -c 120)
	fail "Shell prompt not found. VGA tail: [$dump_tail]"
	errors=$((errors + 1))
fi

# --- Subtest 2: hello (waitpid with specific PID) ---
send_keys "hello" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1

if vga_wait_for 10 "argc=1" "argv[0]=hello"; then
	pass "hello via waitpid(pid,0,0) reports argc=1, argv[0]=hello"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "argc/argv not found. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 3: hello foo bar (waitpid with specific PID and args) ---
send_keys "hello foo bar" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1

if vga_wait_for 10 "argc=3" "argv[1]=foo" "argv[2]=bar"; then
	pass "hello foo bar via waitpid(pid,0,0): argc=3, argv[1]=foo, argv[2]=bar"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "argv params not found. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 4: greeting (waitpid regression) ---
send_keys "greeting" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "hello"; then
	pass "greeting via waitpid(pid,0,0) produced 'hello'"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'hello' not found after 'greeting'. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 5: greeting | prepend (pipeline, multiple waitpid calls) ---
# The shell forks each segment and calls waitpid(pids[i], 0, 0) for each
send_keys "greeting | prepend" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "PIPE:hello"; then
	pass "Multiple waitpid calls in pipeline: 'greeting | prepend' -> 'PIPE:hello'"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'PIPE:hello' not found. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 6: poweroff ---
send_keys "poweroff" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 1

if qemu_wait_exit 10; then
	pass "'poweroff' shut down QEMU"
else
	if qemu_is_running; then
		fail "QEMU still running after 'poweroff'"
		errors=$((errors + 1))
	else
		pass "'poweroff' shut down QEMU"
	fi
fi

if [ "$errors" -eq 0 ]; then
	echo "  Result: All checks PASSED"
else
	echo "  Result: $errors check(s) FAILED (see above)"
fi

qemu_stop
exit $errors
