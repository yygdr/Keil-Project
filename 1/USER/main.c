#include "gd32f10x.h"
#include "usart.h"

int main(void){
	Usart0_Init(921600);
	u0_printf("%d %d %d",0x30,0x30,0x30);
}
