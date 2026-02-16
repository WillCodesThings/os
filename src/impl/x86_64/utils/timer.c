#include <utils/timer.h>
#include <interrupts/port_io.h>
#include <utils/log.h>

// PIT ports
#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND       0x43
#define PIT_CHANNEL2_GATE 0x61

// PIT oscillator frequency: 1,193,182 Hz
#define PIT_FREQUENCY 1193182

// ~10ms calibration window (11932 ticks at 1.193182 MHz)
#define CALIBRATION_TICKS 11932
#define CALIBRATION_MS    10

static uint32_t tsc_khz = 0;

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void timer_init(void) {
    LOG_INFO("TMR", "Calibrating TSC...");

    // Enable PIT channel 2 gate (speaker timer, but speaker output off)
    uint8_t gate = inb(PIT_CHANNEL2_GATE);
    gate &= 0xFD; // Clear bit 1 (speaker off)
    gate |= 0x01; // Set bit 0 (enable gate for channel 2)
    outb(PIT_CHANNEL2_GATE, gate);

    // Program PIT channel 2: one-shot mode (mode 0), lobyte/hibyte, binary
    outb(PIT_COMMAND, 0xB0);

    // Load countdown value
    outb(PIT_CHANNEL2_DATA, (uint8_t)(CALIBRATION_TICKS & 0xFF));
    io_wait();
    outb(PIT_CHANNEL2_DATA, (uint8_t)((CALIBRATION_TICKS >> 8) & 0xFF));

    // Toggle gate to start countdown
    gate = inb(PIT_CHANNEL2_GATE);
    gate &= 0xFE; // Disable gate
    outb(PIT_CHANNEL2_GATE, gate);
    gate |= 0x01; // Re-enable gate (starts countdown)
    outb(PIT_CHANNEL2_GATE, gate);

    uint64_t tsc_start = rdtsc();

    // Poll PIT channel 2 output (bit 5 of port 0x61 goes high when done)
    while ((inb(PIT_CHANNEL2_GATE) & 0x20) == 0)
        ;

    uint64_t tsc_end = rdtsc();

    // Calculate TSC frequency: tsc_delta cycles in CALIBRATION_MS milliseconds
    uint64_t tsc_delta = tsc_end - tsc_start;
    tsc_khz = (uint32_t)(tsc_delta / CALIBRATION_MS);

    LOG_INFO("TMR", "TSC frequency: %u MHz", tsc_khz / 1000);

    // Clean up: disable PIT channel 2 gate
    gate = inb(PIT_CHANNEL2_GATE);
    gate &= 0xFC;
    outb(PIT_CHANNEL2_GATE, gate);
}

uint64_t timer_ticks(void) {
    return rdtsc();
}

uint32_t timer_ms_since(uint64_t start) {
    if (tsc_khz == 0)
        return 0;
    uint64_t elapsed = rdtsc() - start;
    return (uint32_t)(elapsed / tsc_khz);
}

uint32_t timer_get_frequency_khz(void) {
    return tsc_khz;
}
