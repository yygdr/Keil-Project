#include "gd32f10x.h"
#include "boot.h"
#include "main.h"
#include "ustart.h"
#include "delay.h"
#include "fmc.h"
#include "iic.h"
#include "m24c02.h"

load_a load_A;        //函数指针load_A

void BootLoader_Brance(void)
{
    if(OTA_Info.OTA_flag == OTA_SET_FLAG)
    {
        u0_printf("OTA¸üÐÂ\r\n");
        
    }
    else
    {
        u0_printf("Ìø×ªA·ÖÇø\r\n");
        LOAD_A(GD32_A_SADDR);
    }
}

__asm void MSR_SP(uint32_t addr)
{
    MSR MSP, r0                       //set Main Stack value
    BX r14
}

void LOAD_A(uint32_t addr)
{
    if((*(uint32_t*)addr>=0x20000000)&&(*(uint32_t*)addr<=0x20004FFF))
    {
        MSR_SP(*(uint32_t*)addr);
        load_A = (load_a)*(uint32_t*)(addr+4);
        BootLoader_Clear();  
        load_A();
    }
}
/*-------------------------------------------------*/
/*函数名：清除B区使用的外设                        */
/*参  数：无                                       */
/*返回值：无                                       */
/*-------------------------------------------------*/
void BootLoader_Clear(void)
{
	usart_deinit(USART0);   //复位串口0
	gpio_deinit(GPIOA);     //复位GPIOA
	gpio_deinit(GPIOB);     //复位GPIOB
}
