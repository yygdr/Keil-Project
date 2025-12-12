#include "gd32f10x.h"
#include "ustart.h"
#include "delay.h"
#include "iic.h"
#include "m24c02.h"

uint8_t rbuff[256];

//uint8_t wbuff[16]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
uint8_t wbuff[16]={15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};

int main(void){
	
	uint16_t i;
	
	Delay_Init();
	Usart0_Init(921600);
	IIC_Init();
	
	for(i=0;i<256;i++){
		M24C02_WriteByte(i,255-i);
		Delay_Ms(5);
	}
	
	// for(i=0;i<16;i++){
	// 	M24C02_WritePage(i*16,wbuff);
	// 	Delay_Ms(5);
	// }
	
	M24C02_ReadData(0,rbuff,256);
	
	for(i=0;i<256;i++){
		u0_printf("地址%d=%x\r\n",i,rbuff[i]);
	}
	
	
	while(1){

	}
}
