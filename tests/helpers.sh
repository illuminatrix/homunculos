#!/bin/bash
# Common helpers for Illuminatrix integration tests
# All functions assume CWD is tests/

export TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export KERNEL_BIN="${TESTS_DIR}/../kernel.bin"
export MONITOR_SOCK="${TESTS_DIR}/qemu-monitor.sock"
export QEMU_PID=""

PASS="PASS"
FAIL="FAIL"

cleanup() {
	if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
		kill "$QEMU_PID" 2>/dev/null
		wait "$QEMU_PID" 2>/dev/null
	fi
	rm -f "$MONITOR_SOCK"
}
trap cleanup EXIT
trap cleanup INT TERM

check_deps() {
	local missing=0
	for cmd in "$@"; do
		if ! command -v "$cmd" &>/dev/null; then
			echo "ERROR: $cmd not found" >&2
			missing=1
		fi
	done
	return $missing
}

qemu_start() {
	qemu-system-i386 -kernel "$KERNEL_BIN" \
		-display none \
		-monitor "unix:${MONITOR_SOCK},server,nowait" \
		-no-reboot \
		&
	QEMU_PID=$!
	local waited=0
	while [ ! -S "$MONITOR_SOCK" ] && [ $waited -lt 10 ]; do
		sleep 0.5
		waited=$((waited + 1))
	done
	if [ ! -S "$MONITOR_SOCK" ]; then
		echo "ERROR: QEMU monitor socket not created after 5s" >&2
		return 1
	fi
}

qemu_stop() {
	if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
		kill "$QEMU_PID" 2>/dev/null
		wait "$QEMU_PID" 2>/dev/null
	fi
}

qemu_is_running() {
	[ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null
}

qemu_wait_exit() {
	local timeout="${1:-5}"
	local waited=0
	while qemu_is_running && [ $waited -lt $timeout ]; do
		sleep 1
		waited=$((waited + 1))
	done
	! qemu_is_running
}

wait_for_boot() {
	local timeout="${1:-10}"
	local waited=0
	while [ $waited -lt $timeout ]; do
		if vga_contains "Illuminatrix" 2>/dev/null; then
			return 0
		fi
		sleep 1
		waited=$((waited + 1))
	done
	return 1
}

monitor_cmd() {
	echo "$1" | socat - "UNIX-CONNECT:${MONITOR_SOCK}" 2>/dev/null
}

vga_dump() {
	monitor_cmd "xp /4000bx 0xB8000"
}

# Parse QEMU "xp" hex dump (from stdin) into ASCII text.
# Strips address prefixes, parses hex bytes, outputs character bytes (even indices).
vga_decode() {
	local idx=0
	while IFS= read -r line; do
		line="${line#"${line%%[![:space:]]*}"}"
		line="${line%"${line##*[![:space:]]}"}"
		[ -z "$line" ] && continue
		[[ "$line" == "(qemu)"* ]] && continue
		# Remove address prefix (e.g., "B8000:" or "00000000000B8000:")
		if [[ "$line" == *":"* ]]; then
			line="${line#*: }"
		fi
		for hex in $line; do
			hex="${hex#0x}"
			[ ${#hex} -eq 2 ] || continue
			byte=$((16#$hex))
			if [ $((idx % 2)) -eq 0 ]; then
				if [ $byte -ge 32 ] && [ $byte -le 126 ]; then
					printf "\\$(printf '%03o' $byte)"
				elif [ $byte -eq 0 ]; then
					printf " "
				fi
			fi
			idx=$((idx + 1))
		done
	done
	echo
}

vga_text() {
	vga_dump | vga_decode
}

vga_contains() {
	local pattern="$1"
	vga_text | grep -q "$pattern"
}

send_keys() {
	local keys="$1"
	local delay="${2:-0.05}"
	for ((i=0; i<${#keys}; i++)); do
		local ch="${keys:$i:1}"
		local key_name
		case "$ch" in
			$'\n') key_name="ret" ;;
			' ')   key_name="spc" ;;
			-)     key_name="minus" ;;
			_)     key_name="shift-minus" ;;
			=)     key_name="equal" ;;
			+)     key_name="shift-equal" ;;
			/)     key_name="slash" ;;
			.)     key_name="dot" ;;
			,)     key_name="comma" ;;
			!)     key_name="shift-1" ;;
			?)     key_name="shift-slash" ;;
			[a-z]) key_name="$ch" ;;
			[A-Z]) key_name="shift-$(echo "$ch" | tr '[:upper:]' '[:lower:]')" ;;
			[0-9]) key_name="$ch" ;;
			*)     key_name="$ch" ;;
		esac
		monitor_cmd "sendkey $key_name" >/dev/null 2>&1
		sleep "$delay"
	done
}

pass() {
	echo "  $PASS $1"
}

fail() {
	echo "  $FAIL $1"
}
