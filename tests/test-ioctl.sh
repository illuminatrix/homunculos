#!/bin/bash
# Test: ioctl syscall (SYS_ioctl=54) — TCGETS and TCSETS on stdin/stdout
#
# Verifies:
#   1. Kernel boots with welcome message (regression)
#   2. Shell prompt '>' appears (regression)
#   3. ioctl_test binary exists and runs
#   4. ioctl(1, TCGETS, &t) returns 0 on stdout
#   5. ioctl(0, TCGETS, &t) returns 0 on stdin
#   6. ioctl(1, TCSETS, &t) returns 0 on stdout
#   7. Ring 3 CS register is correct throughout (regression)
#   8. poweroff shuts down QEMU (regression)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== ioctl Syscall Test ==="
echo "  Subtest 1: Boot welcome message (regression)"
echo "  Subtest 2: Shell prompt '>' appears (regression)"
echo "  Subtest 3: 'ioctl_test' binary runs"
echo "  Subtest 4: ioctl(1, TCGETS) = 0 (stdout)"
echo "  Subtest 5: ioctl(0, TCGETS) = 0 (stdin)"
echo "  Subtest 6: ioctl(1, TCSETS) = 0 (stdout)"
echo "  Subtest 7: Ring 3 CS register (regression)"
echo "  Subtest 8: 'poweroff' shuts down QEMU (regression)"

# --- Setup disk with ioctl_test included ---
prepare_basic_disk || { fail "Failed to prepare disk"; exit 1; }
# Add ioctl_test to the disk (ext2 is at partition offset 2048 sectors)
IOCTL_TEST_BIN="$(dirname "$0")/ioctl_test.elf"
dd if="$DISK_IMG" bs=512 skip=2048 of=.part.img 2>/dev/null
debugfs -w .part.img -R "write $IOCTL_TEST_BIN /bin/ioctl_test" 2>/dev/null
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

# --- Subtest 3-6: ioctl_test command ---
send_keys "ioctl_test" 0.08
sleep 0.2
send_keys $'\n' 0.08
sleep 2

vga_text | tail -5 > /dev/null

if vga_contains "ok"; then
	pass "'ioctl_test' produced 'ok' (binary executed)"
	# Check for all three ok lines (each ioctl that succeeds prints "ok")
	ok_count=$(vga_text | grep -o "ok" | wc -l)
	# At minimum we expect 3 ok lines (TCGETS stdout, TCGETS stdin, TCSETS)
	# But after shell echo there may be more
	if [ "$ok_count" -ge 3 ]; then
		pass "Found $ok_count 'ok' lines (>=3 expected: TCGETS+TCSETS)"
	else
		fail "Expected >=3 'ok' lines, found $ok_count"
		errors=$((errors + 1))
	fi
else
	vga_raw=$(vga_text | tail -c 300)
	if vga_contains "not found"; then
		fail "'ioctl_test' binary not found on disk (maybe not built?)"
		errors=$((errors + 1))
	elif vga_contains "fail"; then
		fail "'ioctl_test' ran but reported failure"
		errors=$((errors + 1))
	else
		fail "'ok' not found after 'ioctl_test'. VGA tail: [$vga_raw]"
		errors=$((errors + 1))
	fi
fi

assert_user_mode "Subtest 3-6: Ring 3 CS register after ioctl_test"
errors=$((errors + $?))

# --- Subtest 7: Poweroff regression ---
send_keys "poweroff" 0.08
sleep 0.2
send_keys $'\n' 0.08
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
rm -f "$IOCTL_TEST_BIN"
exit $errors
