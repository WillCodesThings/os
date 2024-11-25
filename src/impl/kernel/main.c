#include "print.h"
#include "interuptHandler.h"
#include "test.h"

void kernel_main()
{
    print_clear();
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    // print_str("hello world \n\n another Test");

    printf("hello world %d\n %s\n", 123, "hello");
    printf("works");

    // fib(10);
    // printf("\n");
    // triangle(10);

    // wait_for_key();
}
