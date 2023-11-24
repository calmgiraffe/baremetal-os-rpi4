#include "io.h"

void main()
{
    uart_init();
    uart_writeText("Hello big world!\n");
    while (1);
}