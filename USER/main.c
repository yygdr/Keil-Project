#include "gd32f10x.h"
#include "ustart.h"
int main(void)
{
    Usart0_Init(921600);
		u0_printf("%d %c %x",0x30,0x30,0x30);
}
