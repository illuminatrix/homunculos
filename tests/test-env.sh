#!/bin/bash
# Test: environment variable support (environ, getenv, setenv)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Environment Variable Test ==="
echo "  Subtest 1: Boot + shell prompt"
echo "  Subtest 2: 'cd /dev' updates PWD"
echo "  Subtest 3: 'env' shows PWD=/dev"
echo "  Subtest 4: 'env' output format NAME=VALUE"
echo "  Subtest 5: poweroff (regression)"

qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

# --- Subtest 1: Boot + Shell prompt ---
if wait_for_boot 15; then
	pass "Boot message found"
else
	fail "Boot message not found"
	qemu_stop
	exit 1
fi

sleep 2

if vga_contains ">"; then
	pass "Shell prompt '>' found"
else
	fail "Shell prompt not found"
	errors=$((errors + 1))
fi

# --- Subtest 2: 'cd /dev' ---
send_keys "cd /dev" 0.08
sleep 0.3
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 5

if vga_contains "/dev"; then
	pass "'cd /dev' printed /dev"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'cd /dev' did not print /dev. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 3: 'env' shows PWD=/dev ---
send_keys "env" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 4

if vga_contains "PWD=/dev"; then
	pass "'env' shows PWD=/dev after cd /dev"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'env' did not show PWD=/dev. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 4: 'env' output format NAME=VALUE ---
if vga_contains "="; then
	pass "'env' output format NAME=VALUE"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'env' output has no '='. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Subtest 5: poweroff ---
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

# --- Summary ---
if [ "$errors" -eq 0 ]; then
	echo "  Result: All checks PASSED"
else
	echo "  Result: $errors check(s) FAILED"
fi

qemu_stop
exit $errors
