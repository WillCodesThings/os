# .gdbinit -- OS kernel debugging dashboard

# ============================================================
# Setup
# ============================================================
set architecture i386:x86-64
set disassembly-flavor intel
set pagination off
set confirm off
set print pretty on
set print array on

file dist/x86_64/kernel.bin

# ============================================================
# Quick-connect to QEMU
# ============================================================
define qc
    target remote :1234
end
document qc
    Quick-connect to QEMU GDB stub on localhost:1234
end

# ============================================================
# Dashboard: auto-display on each stop
# ============================================================
define hook-stop
    printf "\n\033[1;34m════════════════════════════════════════════════════════════════\033[0m\n"
    printf "\033[1;33m  REGISTERS\033[0m\n"
    printf "\033[1;34m────────────────────────────────────────────────────────────────\033[0m\n"
    info registers rax rbx rcx rdx rsi rdi rbp rsp
    printf "\033[1;34m────────────────────────────────────────────────────────────────\033[0m\n"
    info registers r8 r9 r10 r11 r12 r13 r14 r15
    printf "\033[1;34m────────────────────────────────────────────────────────────────\033[0m\n"
    printf "\033[1;33m  RIP\033[0m = "
    info registers rip
    printf "\033[1;33m  RFLAGS\033[0m = "
    info registers eflags
    printf "\n\033[1;33m  CODE\033[0m\n"
    printf "\033[1;34m────────────────────────────────────────────────────────────────\033[0m\n"
    x/8i $rip
    printf "\n\033[1;33m  STACK (top 8 qwords)\033[0m\n"
    printf "\033[1;34m────────────────────────────────────────────────────────────────\033[0m\n"
    x/8gx $rsp
    printf "\033[1;34m════════════════════════════════════════════════════════════════\033[0m\n\n"
end

# ============================================================
# Custom Commands
# ============================================================

# --- Break at kernel_main ---
define bk
    break kernel_main
    printf "Breakpoint set at kernel_main. Use 'qc' then 'c' to run.\n"
end
document bk
    Set a breakpoint at kernel_main (convenience shortcut).
end

# --- Show interrupt state ---
define irqstate
    printf "\033[1;33mInterrupt State\033[0m\n"
    printf "  RFLAGS: "
    info registers eflags
end
document irqstate
    Show interrupt state: RFLAGS IF bit.
end

# --- Decompose virtual address into page table indices ---
define pagewalk
    if $argc == 0
        printf "Usage: pagewalk <virtual_address>\n"
    else
        set $vaddr = (unsigned long long)$arg0
        set $pml4_idx = ($vaddr >> 39) & 0x1FF
        set $pdpt_idx = ($vaddr >> 30) & 0x1FF
        set $pd_idx   = ($vaddr >> 21) & 0x1FF
        set $pt_idx   = ($vaddr >> 12) & 0x1FF
        printf "Virtual address: 0x%llx\n", $vaddr
        printf "  PML4 index: %d\n", $pml4_idx
        printf "  PDPT index: %d\n", $pdpt_idx
        printf "  PD   index: %d\n", $pd_idx
        printf "  PT   index: %d\n", $pt_idx
        printf "  Offset:     0x%llx\n", $vaddr & 0xFFF
    end
end
document pagewalk
    Decompose a virtual address into page table indices.
    Usage: pagewalk <address>
end

# --- Dump memory as hex ---
define xd
    if $argc == 0
        printf "Usage: xd <address> [length]\n"
        printf "  Dump memory in hex+ASCII format (default 256 bytes)\n"
    else
        if $argc == 1
            x/16gx $arg0
        else
            set $__xd_count = $arg1 / 8
            x/$__xd_count gx $arg0
        end
    end
end
document xd
    Hex dump memory.  Usage: xd <address> [byte_count]
end

# ============================================================
# Startup Banner
# ============================================================
printf "\n\033[1;32m╔══════════════════════════════════════════════╗\033[0m\n"
printf "\033[1;32m║  OS Kernel GDB Dashboard                     ║\033[0m\n"
printf "\033[1;32m╠══════════════════════════════════════════════╣\033[0m\n"
printf "\033[1;32m║  qc          connect to QEMU :1234           ║\033[0m\n"
printf "\033[1;32m║  bk          break at kernel_main            ║\033[0m\n"
printf "\033[1;32m║  irqstate    show interrupt flags             ║\033[0m\n"
printf "\033[1;32m║  pagewalk A  decompose VA to page indices    ║\033[0m\n"
printf "\033[1;32m║  xd A [N]   hex dump memory                  ║\033[0m\n"
printf "\033[1;32m╚══════════════════════════════════════════════╝\033[0m\n\n"
