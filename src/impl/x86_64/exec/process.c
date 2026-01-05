#include "exec/process.h"
#include "exec/elf.h"
#include "memory/heap.h"
#include "utils/memory.h"
#include "utils/string.h"
#include "fs/vfs.h"

// Process table
static process_t *process_table[MAX_PROCESSES];
static process_t *current_process = 0;
static uint32_t next_pid = 1;

// Kernel return point (saved stack pointer)
static uint64_t kernel_rsp = 0;

void process_init(void) {
    memset(process_table, 0, sizeof(process_table));
    current_process = 0;
    next_pid = 1;
}

process_t *process_create(const char *name) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        return 0;  // No free slots
    }

    // Allocate process structure
    process_t *proc = (process_t *)kcalloc(1, sizeof(process_t));
    if (!proc) {
        return 0;
    }

    // Initialize process
    proc->pid = next_pid++;
    proc->state = PROCESS_STATE_NEW;

    // Copy name (safely)
    size_t name_len = strlen(name);
    if (name_len > 63) name_len = 63;
    memcpy(proc->name, name, name_len);
    proc->name[name_len] = 0;

    // Allocate stack
    proc->stack_base = kmalloc_aligned(PROCESS_STACK_SIZE, 16);
    if (!proc->stack_base) {
        kfree(proc);
        return 0;
    }
    proc->stack_size = PROCESS_STACK_SIZE;

    // Zero the stack
    memset(proc->stack_base, 0, PROCESS_STACK_SIZE);

    // Store in process table
    process_table[slot] = proc;

    return proc;
}

int process_load_elf(process_t *proc, const uint8_t *elf_data, size_t size) {
    if (!proc || !elf_data) {
        return -1;
    }

    // Load the ELF file
    int ret = elf_load(elf_data, size, &proc->elf_info);
    if (ret != 0) {
        return ret;
    }

    // Set up initial CPU state
    memset(&proc->cpu_state, 0, sizeof(cpu_state_t));

    // Set instruction pointer to entry point
    proc->cpu_state.rip = proc->elf_info.entry_point;

    // Set stack pointer to top of stack (stack grows down)
    proc->cpu_state.rsp = (uint64_t)proc->stack_base + proc->stack_size - 8;

    // Set up flags (interrupts enabled)
    proc->cpu_state.rflags = 0x202;  // IF flag set

    // Kernel mode segments (we run in ring 0 for now)
    proc->cpu_state.cs = 0x08;  // Kernel code segment
    proc->cpu_state.ss = 0x10;  // Kernel data segment

    proc->state = PROCESS_STATE_READY;

    return 0;
}

// Assembly function to jump to process
extern void process_jump_to_entry(uint64_t entry, uint64_t stack, uint64_t *kernel_rsp_ptr);
extern void process_restore_kernel(uint64_t kernel_rsp);

int process_exec(process_t *proc) {
    if (!proc || proc->state != PROCESS_STATE_READY) {
        return -1;
    }

    current_process = proc;
    proc->state = PROCESS_STATE_RUNNING;

    // Calculate the actual entry point in our allocated memory
    // The ELF specifies a virtual address, but we loaded at a different location
    uint64_t offset = proc->cpu_state.rip - proc->elf_info.base_addr;
    uint64_t actual_entry = (uint64_t)proc->elf_info.program_memory + offset;

    // Jump to the process
    // This function saves kernel state and jumps to process entry
    // When process calls exit syscall, we return here
    process_jump_to_entry(actual_entry, proc->cpu_state.rsp, &kernel_rsp);

    // Process has returned
    proc->state = PROCESS_STATE_TERMINATED;
    current_process = 0;

    return proc->exit_code;
}

void process_exit(int exit_code) {
    if (current_process) {
        current_process->exit_code = exit_code;
        current_process->state = PROCESS_STATE_TERMINATED;

        // Return to kernel
        if (kernel_rsp != 0) {
            process_restore_kernel(kernel_rsp);
        }
    }
}

void process_terminate(process_t *proc) {
    if (!proc) return;

    // Find in process table and remove
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] == proc) {
            process_table[i] = 0;
            break;
        }
    }

    // Free resources
    if (proc->stack_base) {
        kfree(proc->stack_base);
    }

    elf_unload(&proc->elf_info);
    kfree(proc);
}

process_t *process_get_current(void) {
    return current_process;
}

process_t *process_get_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] && process_table[i]->pid == pid) {
            return process_table[i];
        }
    }
    return 0;
}

// Execute a binary file from filesystem
int process_exec_file(const char *path) {
    // Open the file
    vfs_node_t *node = vfs_open(path, VFS_READ);
    if (!node) {
        return -1;
    }

    // Allocate buffer for file contents
    uint8_t *buffer = (uint8_t *)kmalloc(node->length);
    if (!buffer) {
        vfs_close(node);
        return -2;
    }

    // Read the file
    int bytes_read = vfs_read(node, 0, node->length, buffer);
    vfs_close(node);

    if (bytes_read <= 0) {
        kfree(buffer);
        return -3;
    }

    // Create process
    const char *name = vfs_get_filename(path);
    process_t *proc = process_create(name ? name : "unknown");
    if (!proc) {
        kfree(buffer);
        return -4;
    }

    // Load ELF
    int ret = process_load_elf(proc, buffer, bytes_read);
    kfree(buffer);

    if (ret != 0) {
        process_terminate(proc);
        return -5;
    }

    // Execute
    ret = process_exec(proc);

    // Clean up
    process_terminate(proc);

    return ret;
}
