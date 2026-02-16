#!/usr/bin/env bash
#
# Watch src/ for changes and auto-rebuild + relaunch QEMU.
# Requires: fswatch (brew install fswatch on macOS)
#
set -euo pipefail

RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
BOLD='\033[1m'
RESET='\033[0m'

QEMU_PID=""
ISO="dist/x86_64/kernel.iso"
DISK_FLAG=""
if [ -n "${DISK:-}" ] && [ -f "${DISK}" ]; then
    DISK_FLAG="-drive file=${DISK},format=raw,index=0,media=disk"
    printf "${GREEN}Using disk image: %s${RESET}\n" "$DISK"
fi

if ! command -v fswatch &>/dev/null; then
    printf "${RED}ERROR: fswatch is not installed.${RESET}\n"
    printf "Install with: ${BOLD}brew install fswatch${RESET}\n"
    exit 1
fi

cleanup() {
    printf "\n${YELLOW}Stopping watch...${RESET}\n"
    kill_qemu
    if [ -n "${FSWATCH_PID:-}" ]; then
        kill "$FSWATCH_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

kill_qemu() {
    if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
        printf "${YELLOW}Killing QEMU (pid %s)...${RESET}\n" "$QEMU_PID"
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
        QEMU_PID=""
    fi
}

build_and_run() {
    kill_qemu

    printf "\n${BOLD}${GREEN}[watch] Rebuilding...${RESET}\n"
    if docker run --rm -v "$(pwd)":/root/env my-os make; then
        printf "${BOLD}${GREEN}[watch] Build succeeded. Launching QEMU...${RESET}\n"
        qemu-system-x86_64 \
            -cdrom "$ISO" \
            -m 256M \
            -netdev user,id=net0 \
            -device e1000,netdev=net0 \
            -serial mon:stdio \
            $DISK_FLAG \
            &
        QEMU_PID=$!
    else
        printf "${RED}[watch] Build failed!${RESET}\n"
    fi
}

# Initial build + run
build_and_run

printf "\n${BOLD}${YELLOW}Watching src/ for changes (*.c, *.h, *.asm)...${RESET}\n"
printf "${YELLOW}Press Ctrl+C to stop.${RESET}\n\n"

fswatch -r -l 1 \
    --include='\.c$' \
    --include='\.h$' \
    --include='\.asm$' \
    --exclude='.*' \
    src/ | while read -r _; do
    # Drain queued events (batch rapid saves)
    while read -r -t 0.5 _; do :; done
    printf "\n${YELLOW}[watch] Change detected!${RESET}\n"
    build_and_run
done &
FSWATCH_PID=$!

wait "$FSWATCH_PID" 2>/dev/null || true
