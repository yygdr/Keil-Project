#include "gd32f10x.h"
#include "usart.h"
#include "delay.h"
#include "spi.h"
#include "w25q64.h"

uint8_t wdata[256];
uint8_t rdata[256];

int main(void){

	uint16_t i,j;
	
	Delay_Init();
	Usart0_Init(921600);
	W25Q64_Init();
	
	
	W25Q64_Erase64K(0);
	
	for(i=0;i<256;i++){
		for(j=0;j<256;j++) 
			wdata[j] = i;
		W25Q64_PageWrite(wdata,i);
	}
		
	Delay_Ms(50);

    for(i=0;i<256;i++){
		W25Q64_Read(rdata,i*256,256);
		for(j=0;j<256;j++) 
			u0_printf("µØÖ·%d=%x\r\n",i*256+j,rdata[j]);
	}
	
	
	while(1){

	}
}
