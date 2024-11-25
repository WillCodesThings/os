#include "print.h"
#include "interuptHandler.h"

void kernel_main()
{
    print_clear();
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print_str("hello world \n\n another Test");

    // wait_for_key();
}
