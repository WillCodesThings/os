#include <stdint.h>
#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <interrupts/port_io.h>

#include <shell/print.h>
#include <shell/shell.h>

// Declaration for the assembly-defined default handler
extern void default_interrupt_handler(void);

// C handler for default interrupts
void c_default_handler(int interrupt_num);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);
extern void irq16(void);
extern void irq17(void);
extern void irq18(void);
extern void irq19(void);
extern void irq20(void);
extern void irq21(void);
extern void irq22(void);
extern void irq23(void);
extern void irq24(void);
extern void irq25(void);
extern void irq26(void);
extern void irq27(void);
extern void irq28(void);
extern void irq29(void);
extern void irq30(void);
extern void irq31(void);
extern void irq32(void);
extern void irq33(void);
extern void irq34(void);
extern void irq35(void);
extern void irq36(void);
extern void irq37(void);
extern void irq38(void);
extern void irq39(void);
extern void irq40(void);
extern void irq41(void);
extern void irq42(void);
extern void irq43(void);
extern void irq44(void);
extern void irq45(void);
extern void irq46(void);
extern void irq47(void);
extern void irq48(void);
extern void irq49(void);
extern void irq50(void);
extern void irq51(void);
extern void irq52(void);
extern void irq53(void);
extern void irq54(void);
extern void irq55(void);
extern void irq56(void);
extern void irq57(void);
extern void irq58(void);
extern void irq59(void);
extern void irq60(void);
extern void irq61(void);
extern void irq62(void);
extern void irq63(void);
extern void irq64(void);
extern void irq65(void);
extern void irq66(void);
extern void irq67(void);
extern void irq68(void);
extern void irq69(void);
extern void irq70(void);
extern void irq71(void);
extern void irq72(void);
extern void irq73(void);
extern void irq74(void);
extern void irq75(void);
extern void irq76(void);
extern void irq77(void);
extern void irq78(void);
extern void irq79(void);
extern void irq80(void);
extern void irq81(void);
extern void irq82(void);
extern void irq83(void);
extern void irq84(void);
extern void irq85(void);
extern void irq86(void);
extern void irq87(void);
extern void irq88(void);
extern void irq89(void);
extern void irq90(void);
extern void irq91(void);
extern void irq92(void);
extern void irq93(void);
extern void irq94(void);
extern void irq95(void);
extern void irq96(void);
extern void irq97(void);
extern void irq98(void);
extern void irq99(void);
extern void irq100(void);
extern void irq101(void);
extern void irq102(void);
extern void irq103(void);
extern void irq104(void);
extern void irq105(void);
extern void irq106(void);
extern void irq107(void);
extern void irq108(void);
extern void irq109(void);
extern void irq110(void);
extern void irq111(void);
extern void irq112(void);
extern void irq113(void);
extern void irq114(void);
extern void irq115(void);
extern void irq116(void);
extern void irq117(void);
extern void irq118(void);
extern void irq119(void);
extern void irq120(void);
extern void irq121(void);
extern void irq122(void);
extern void irq123(void);
extern void irq124(void);
extern void irq125(void);
extern void irq126(void);
extern void irq127(void);
extern void irq128(void);
extern void irq129(void);
extern void irq130(void);
extern void irq131(void);
extern void irq132(void);
extern void irq133(void);
extern void irq134(void);
extern void irq135(void);
extern void irq136(void);
extern void irq137(void);
extern void irq138(void);
extern void irq139(void);
extern void irq140(void);
extern void irq141(void);
extern void irq142(void);
extern void irq143(void);
extern void irq144(void);
extern void irq145(void);
extern void irq146(void);
extern void irq147(void);
extern void irq148(void);
extern void irq149(void);
extern void irq150(void);
extern void irq151(void);
extern void irq152(void);
extern void irq153(void);
extern void irq154(void);
extern void irq155(void);
extern void irq156(void);
extern void irq157(void);
extern void irq158(void);
extern void irq159(void);
extern void irq160(void);
extern void irq161(void);
extern void irq162(void);
extern void irq163(void);
extern void irq164(void);
extern void irq165(void);
extern void irq166(void);
extern void irq167(void);
extern void irq168(void);
extern void irq169(void);
extern void irq170(void);
extern void irq171(void);
extern void irq172(void);
extern void irq173(void);
extern void irq174(void);
extern void irq175(void);
extern void irq176(void);
extern void irq177(void);
extern void irq178(void);
extern void irq179(void);
extern void irq180(void);
extern void irq181(void);
extern void irq182(void);
extern void irq183(void);
extern void irq184(void);
extern void irq185(void);
extern void irq186(void);
extern void irq187(void);
extern void irq188(void);
extern void irq189(void);
extern void irq190(void);
extern void irq191(void);
extern void irq192(void);
extern void irq193(void);
extern void irq194(void);
extern void irq195(void);
extern void irq196(void);
extern void irq197(void);
extern void irq198(void);
extern void irq199(void);
extern void irq200(void);
extern void irq201(void);
extern void irq202(void);
extern void irq203(void);
extern void irq204(void);
extern void irq205(void);
extern void irq206(void);
extern void irq207(void);
extern void irq208(void);
extern void irq209(void);
extern void irq210(void);
extern void irq211(void);
extern void irq212(void);
extern void irq213(void);
extern void irq214(void);
extern void irq215(void);
extern void irq216(void);
extern void irq217(void);
extern void irq218(void);
extern void irq219(void);
extern void irq220(void);
extern void irq221(void);
extern void irq222(void);
extern void irq223(void);
extern void irq224(void);
extern void irq225(void);

// Initializes IDT with safe default handlers for all interrupts
void setup_safe_idt(void)
{
    idt_install();

    void *irq_stubs[256] = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15,
        irq16, irq17, irq18, irq19, irq20, irq21, irq22, irq23,
        irq24, irq25, irq26, irq27, irq28, irq29, irq30, irq31,
        irq32, irq33, irq34, irq35, irq36, irq37, irq38, irq39,
        irq40, irq41, irq42, irq43, irq44, irq45, irq46, irq47,
        irq48, irq49, irq50, irq51, irq52, irq53, irq54, irq55,
        irq56, irq57, irq58, irq59, irq60, irq61, irq62, irq63,
        irq64, irq65, irq66, irq67, irq68, irq69, irq70, irq71,
        irq72, irq73, irq74, irq75, irq76, irq77, irq78, irq79,
        irq80, irq81, irq82, irq83, irq84, irq85, irq86, irq87,
        irq88, irq89, irq90, irq91, irq92, irq93, irq94, irq95,
        irq96, irq97, irq98, irq99, irq100, irq101, irq102, irq103,
        irq104, irq105, irq106, irq107, irq108, irq109, irq110, irq111,
        irq112, irq113, irq114, irq115, irq116, irq117, irq118, irq119,
        irq120, irq121, irq122, irq123, irq124, irq125, irq126, irq127,
        irq128, irq129, irq130, irq131, irq132, irq133, irq134, irq135,
        irq136, irq137, irq138, irq139, irq140, irq141, irq142, irq143,
        irq144, irq145, irq146, irq147, irq148, irq149, irq150, irq151,
        irq152, irq153, irq154, irq155, irq156, irq157, irq158, irq159,
        irq160, irq161, irq162, irq163, irq164, irq165, irq166, irq167,
        irq168, irq169, irq170, irq171, irq172, irq173, irq174, irq175,
        irq176, irq177, irq178, irq179, irq180, irq181, irq182, irq183,
        irq184, irq185, irq186, irq187, irq188, irq189, irq190, irq191,
        irq192, irq193, irq194, irq195, irq196, irq197, irq198, irq199,
        irq200, irq201, irq202, irq203, irq204, irq205, irq206, irq207,
        irq208, irq209, irq210, irq211, irq212, irq213, irq214, irq215,
        irq216, irq217, irq218, irq219, irq220, irq221, irq222, irq223,
        irq224, irq225};

    for (int i = 0; i < 256; i++)
        idt_set_gate(i, (uint64_t)irq_stubs[i]);

    idt_load();
}

void enable_interrupts()
{
    asm volatile("sti");
}

void disable_interrupts()
{
    asm volatile("cli");
}

// Safe initialization of interrupt subsystem
void init_interrupts_safe()
{
    // Step 1: Initialize IDT with default handlers
    setup_safe_idt();

    // Step 2: Remap PIC to avoid conflicts with CPU exceptions
    // Map IRQs 0-7 to interrupts 0x20-0x27
    // Map IRQs 8-15 to interrupts 0x28-0x2F
    pic_remap(0x20, 0x28);

    // Step 3: Mask all interrupts initially
    // This prevents any hardware interrupts from firing
    outb(PIC1_DATA, 0xFF); // Disable all IRQs on master PIC
    outb(PIC2_DATA, 0xFF); // Disable all IRQs on follower PIC
}

// Function to test interrupts with careful control
void test_interrupts()
{
    // Print initial status
    serial_print("Starting interrupt testing sequence...\n");

    // 1. First, enable CPU interrupts with all hardware interrupts masked
    serial_print("Enabling CPU interrupts with all hardware masked...\n");
    asm volatile("sti");

    // Wait a bit to ensure system is stable
    serial_print("Waiting to ensure stability...\n");
    for (volatile int i = 0; i < 1000000; i++)
        ;

    // 2. Now enable keyboard interrupt only
    serial_print("Enabling keyboard interrupt only...\n");
    outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << 1)); // Unmask IRQ1 (keyboard)

    // Wait again to ensure stability
    serial_print("Waiting to verify keyboard interrupt works...\n");
    for (volatile int i = 0; i < 1000000; i++)
        ;

    // 3. Enable timer interrupt for system clock
    // Uncomment when keyboard is working reliably
    serial_print("Enabling timer interrupt...\n");
    outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << 0)); // Unmask IRQ0 (timer)

    serial_print("Interrupt testing complete - interrupts enabled!\n");
}

// Debug function to print interrupt information
void debug_print_interrupt(int interrupt_num)
{
    char buffer[32];

    // Format: "INT: XX"
    buffer[0] = 'I';
    buffer[1] = 'N';
    buffer[2] = 'T';
    buffer[3] = ':';
    buffer[4] = ' ';

    // Convert interrupt number to string
    if (interrupt_num < 10)
    {
        buffer[5] = '0' + interrupt_num;
        buffer[6] = '\0';
    }
    else
    {
        buffer[5] = '0' + (interrupt_num / 10);
        buffer[6] = '0' + (interrupt_num % 10);
        buffer[7] = '\0';
    }

    serial_print(buffer);
    serial_print("\n");
}

// C handler for default interrupts
void c_default_handler(int interrupt_num)
{
    // Print which interrupt was triggered for debugging
    // debug_print_interrupt(interrupt_num);

    // Send EOI as appropriate - crucial step!
    if (interrupt_num >= 0x20 && interrupt_num < 0x30)
    {
        // It's a PIC interrupt
        if (interrupt_num >= 0x28)
        {
            // It's from the follower PIC
            outb(PIC2_COMMAND, 0x20);
        }
        // Send EOI to master PIC for all hardware interrupts
        outb(PIC1_COMMAND, 0x20);
    }
}
