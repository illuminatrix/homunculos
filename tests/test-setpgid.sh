#!/bin/bash
# Test: setpgid syscall (SYS_setpgid=57)
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== Setpgid Test ==="
echo "  Subtest 1: Boot welcome message (regression)"
echo "  Subtest 2: Shell prompt '>' appears (regression)"
echo "  Subtest 3: setpgid_test binary (return value checks)"
echo "  Subtest 4: poweroff shuts down QEMU (regression)"

SETPGID_TEST_BIN="$(dirname "$0")/setpgid_test.elf"
dd if="$DISK_IMG" bs=512 skip=2048 of=.part.img 2>/dev/null
debugfs -w .part.img -R "write $SETPGID_TEST_BIN /bin/setpgid_test" 2>/dev/null
dd if=.part.img of="$DISK_IMG" bs=512 seek=2048 conv=notrunc 2>/dev/null
rm -f .part.img

qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

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

send_keys "setpgid_test" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 3

if vga_contains "ALL TESTS PASS"; then
	pass "'setpgid_test' produced 'ALL TESTS PASS'"
elif vga_contains "PASS"; then
	vga_raw=$(vga_text | tail -c 300)
	pass "'setpgid_test' produced PASS output. VGA tail: [$vga_raw]"
elif vga_contains "FAIL"; then
	vga_raw=$(vga_text | tail -c 300)
	fail "'setpgid_test' reported failure. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
elif vga_contains "not found"; then
	fail "'setpgid_test' binary not found on disk"
	errors=$((errors + 1))
else
	vga_raw=$(vga_text | tail -c 200)
	fail "'setpgid_test' produced unexpected output. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

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
	echo "  Result: $errors check(s) FAILED"
fi

qemu_stop
exit $errors
