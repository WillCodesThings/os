#!/usr/bin/env bash
#
# Smoke test: boot the OS in headless QEMU, capture serial output,
# verify "BOOT_COMPLETE" appears within the timeout period.
#
set -euo pipefail

ISO="${1:-dist/x86_64/kernel.iso}"
TIMEOUT="${2:-15}"
DISK="${3:-}"
SERIAL_LOG=$(mktemp /tmp/os-smoke-XXXXXX.log)

DISK_FLAG=""
if [ -n "$DISK" ] && [ -f "$DISK" ]; then
    DISK_FLAG="-drive file=$DISK,format=raw,index=0,media=disk"
fi

RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
BOLD='\033[1m'
RESET='\033[0m'

cleanup() {
    if [ -n "${QEMU_PID:-}" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
    fi
    rm -f "$SERIAL_LOG"
}
trap cleanup EXIT

if [ ! -f "$ISO" ]; then
    printf "${RED}ERROR: ISO not found at %s${RESET}\n" "$ISO"
    exit 1
fi

printf "${BOLD}${YELLOW}Starting smoke test (timeout: %ss)...${RESET}\n" "$TIMEOUT"

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 256M \
    -netdev user,id=net0 \
    -device e1000,netdev=net0 \
    -serial file:"$SERIAL_LOG" \
    -display none \
    -no-reboot \
    $DISK_FLAG \
    &
QEMU_PID=$!

ELAPSED=0
while [ "$ELAPSED" -lt "$TIMEOUT" ]; do
    sleep 1
    ELAPSED=$((ELAPSED + 1))

    if ! kill -0 "$QEMU_PID" 2>/dev/null; then
        printf "${RED}FAIL: QEMU exited unexpectedly after %ss${RESET}\n" "$ELAPSED"
        printf "${YELLOW}Serial output:${RESET}\n"
        cat "$SERIAL_LOG" 2>/dev/null || true
        exit 1
    fi

    if grep -q "BOOT_COMPLETE" "$SERIAL_LOG" 2>/dev/null; then
        printf "${BOLD}${GREEN}PASS: Boot completed in %ss${RESET}\n" "$ELAPSED"

        PASS=true
        for MSG in "Kernel Logger Initialized" "Mouse initialized" "ATA controller initialized" "BOOT_COMPLETE"; do
            if grep -q "$MSG" "$SERIAL_LOG"; then
                printf "  ${GREEN}OK${RESET}  %s\n" "$MSG"
            else
                printf "  ${RED}MISSING${RESET}  %s\n" "$MSG"
                PASS=false
            fi
        done

        if $PASS; then
            printf "\n${BOLD}${GREEN}All checks passed!${RESET}\n"
            exit 0
        else
            printf "\n${RED}Some checks failed.${RESET}\n"
            exit 1
        fi
    fi
done

printf "${RED}FAIL: Boot did not complete within %ss${RESET}\n" "$TIMEOUT"
printf "${YELLOW}Serial output (last 50 lines):${RESET}\n"
tail -50 "$SERIAL_LOG" 2>/dev/null || true
exit 1
