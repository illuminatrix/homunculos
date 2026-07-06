#!/bin/bash
# Build a shared ext2 disk image for all integration tests.
# Called from tests/Makefile when ext2-disk.img is out of date.
source "$(dirname "$0")/helpers.sh"

check_deps dd mkfs.ext2 debugfs sfdisk || exit 1

TESTS_DIR="$(cd "$(dirname "$0")" && pwd)"
DISK_IMG="${TESTS_DIR}/ext2-disk.img"
SHELL_BIN="${TESTS_DIR}/../shell/shell"
INIT_BIN="${TESTS_DIR}/../init/init"

rm -f "$DISK_IMG"

# Create raw ext2 partition with all needed files
dd if=/dev/zero of=.part.img bs=1M count=31 2>/dev/null
mkfs.ext2 -F -E revision=0 -b 1024 .part.img 2>/dev/null
debugfs -w .part.img -R "rmdir /lost+found" 2>/dev/null

# Standard directories
debugfs -w .part.img -R "mkdir /dev" 2>/dev/null
debugfs -w .part.img -R "mkdir /bin" 2>/dev/null

# Shell and init
if [ -f "$SHELL_BIN" ]; then
	debugfs -w .part.img -R "write $SHELL_BIN /bin/shell" 2>/dev/null
fi
if [ -f "$INIT_BIN" ]; then
	debugfs -w .part.img -R "write $INIT_BIN /bin/init" 2>/dev/null
fi

# Test-only binary for pipe testing
PIPETEST="${TESTS_DIR}/prepend.elf"
if [ -f "$PIPETEST" ]; then
	debugfs -w .part.img -R "write $PIPETEST /bin/prepend" 2>/dev/null
fi

# Test-only binary for dup testing
DUP_TEST="${TESTS_DIR}/dup_test.elf"
if [ -f "$DUP_TEST" ]; then
	debugfs -w .part.img -R "write $DUP_TEST /bin/dup_test" 2>/dev/null
fi

# Test-only binary for fcntl testing
FCNTL_TEST="${TESTS_DIR}/fcntl_test.elf"
if [ -f "$FCNTL_TEST" ]; then
	debugfs -w .part.img -R "write $FCNTL_TEST /bin/fcntl_test" 2>/dev/null
fi

# Command binaries
for cmd in poweroff reboot greeting uname ls cat stat hello pwd cd echo touch write mkdir rm ln readlink sleep systime env kill trap sync times chmod mv fchmod mknod umask; do
	src="${TESTS_DIR}/../shell/bin/$cmd"
	if [ -f "$src" ]; then
		debugfs -w .part.img -R "write $src /bin/$cmd" 2>/dev/null
	fi
done

# ext2 test files
TEST_CONTENT=$(mktemp)
printf "Hello from ext2!" > "$TEST_CONTENT"
debugfs -w .part.img -R "mkdir /testdir" 2>/dev/null
debugfs -w .part.img -R "write $TEST_CONTENT /hello.txt" 2>/dev/null
debugfs -w .part.img -R "mkdir /nested" 2>/dev/null
debugfs -w .part.img -R "mkdir /nested/deep" 2>/dev/null
printf "deep file" > "${TEST_CONTENT}2"
debugfs -w .part.img -R "write ${TEST_CONTENT}2 /nested/deep/secret.txt" 2>/dev/null
rm -f "$TEST_CONTENT" "${TEST_CONTENT}2"

# Create MBR-partitioned disk image
dd if=/dev/zero of="$DISK_IMG" bs=1M count=32 2>/dev/null
printf '2048,,L,*\n' | sfdisk "$DISK_IMG" 2>/dev/null
dd if=.part.img of="$DISK_IMG" bs=512 seek=2048 conv=notrunc 2>/dev/null
rm -f .part.img

echo "  Disk image created: $DISK_IMG"
