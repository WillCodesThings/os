#pragma once
#include <stdint.h>
#include <stddef.h>
#include "exec/elf.h"

// Process states
#define PROCESS_STATE_NEW       0
#define PROCESS_STATE_READY     1
#define PROCESS_STATE_RUNNING   2
#define PROCESS_STATE_BLOCKED   3
#define PROCESS_STATE_TERMINATED 4

// Maximum number of processes
#define MAX_PROCESSES 64

// Process stack size (64KB)
#define PROCESS_STACK_SIZE 0x10000

// CPU register state
typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
    uint64_t cs, ss;
} __attribute__((packed)) cpu_state_t;

// Process Control Block
typedef struct process {
    uint32_t pid;              // Process ID
    uint32_t state;            // Process state
    char name[64];             // Process name

    cpu_state_t cpu_state;     // Saved CPU registers

    void *stack_base;          // Stack memory base
    size_t stack_size;         // Stack size

    elf_info_t elf_info;       // ELF loading info

    int exit_code;             // Exit code when terminated

    struct process *parent;    // Parent process
    struct process *next;      // Next in process list
} process_t;

// Process management functions
void process_init(void);
process_t *process_create(const char *name);
int process_load_elf(process_t *proc, const uint8_t *elf_data, size_t size);
int process_exec(process_t *proc);
void process_exit(int exit_code);
void process_terminate(process_t *proc);
process_t *process_get_current(void);
process_t *process_get_by_pid(uint32_t pid);

// Execute a binary file from the filesystem
int process_exec_file(const char *path);

// Return to kernel after process completes
void process_return_to_kernel(void);
