#!/bin/bash
# Test: mknod syscall and command
source "$(dirname "$0")/helpers.sh"

check_deps qemu-system-i386 socat dd mkfs.ext2 debugfs sfdisk || exit 1

echo "=== mknod Test ==="

# Mode constants (must match what mknod shell command passes: S_IFCHR|0666 etc.)
S_IFCHR=$((0x2000))
S_IFBLK=$((0x6000))
# 0666 = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH
PERMS_666=$(( 00400 | 00200 | 00040 | 00020 | 00004 | 00002 ))

MODE_CHR=$(( S_IFCHR | PERMS_666 ))
MODE_BLK=$(( S_IFBLK | PERMS_666 ))

qemu_start_with_disk 15 || { fail "QEMU failed to start"; exit 1; }

errors=0

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

# --- Create a character device node ---
send_keys "mknod /chardev c 1 3" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "mknod: failed"; then
	fail "mknod char device failed"
	errors=$((errors + 1))
else
	pass "mknod char device succeeded"
fi

# --- Verify via stat: mode=MODE_CHR, rdev=(1<<8)|3=259 ---
EXPECTED_RDEV=$(( (1 << 8) | 3 ))
send_keys "stat /chardev" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "mode=${MODE_CHR}"; then
	pass "stat shows chr mode ${MODE_CHR} (S_IFCHR|0666)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat chr mode expected ${MODE_CHR}. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

if vga_contains "rdev=${EXPECTED_RDEV}"; then
	pass "stat shows chr rdev=259 (1,3)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat chr rdev expected 259. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Create a block device node ---
send_keys "mknod /blkdev b 8 0" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "mknod: failed"; then
	fail "mknod block device failed"
	errors=$((errors + 1))
else
	pass "mknod block device succeeded"
fi

# --- Verify via stat ---
EXPECTED_BLK_RDEV=$(( (8 << 8) | 0 ))
send_keys "stat /blkdev" 0.08
sleep 0.2
monitor_cmd "sendkey ret" >/dev/null 2>&1
sleep 2

if vga_contains "mode=${MODE_BLK}"; then
	pass "stat shows blk mode ${MODE_BLK} (S_IFBLK|0666)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat blk mode expected ${MODE_BLK}. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

if vga_contains "rdev=${EXPECTED_BLK_RDEV}"; then
	pass "stat shows blk rdev=2048 (8,0)"
else
	vga_raw=$(vga_text | tail -c 200)
	fail "stat blk rdev expected 2048. VGA tail: [$vga_raw]"
	errors=$((errors + 1))
fi

# --- Poweroff ---
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
