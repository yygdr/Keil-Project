#ifndef USTART_H
#define USTART_H
#include "stdint.h"
#include "gd32f10x_gpio.h"
#include "gd32f10x_usart.h"
#include "gd32f10x_dma.h"
#include "gd32f10x.h"
#define U0_Rx_SIZE 2048
#define U0_RX_MAX 256
#define NUM 10
typedef struct
{
	uint8_t *start;
	uint8_t *end;
}UCB_URxBuffptr;

typedef struct
{
	uint16_t URxCounter;
	UCB_URxBuffptr URxDataPtr[NUM];
	UCB_URxBuffptr *URxDataIN;
	UCB_URxBuffptr *URxDataOUT;
	UCB_URxBuffptr *URxDataEND;
	
}UCB_CB;
extern UCB_CB U0CB;
void Usart0_Init(uint32_t bandrate);
void DMA_Init(void);
void U0Rx_PtrInit(void);
void u0_printf(char *format,...);
extern uint8_t U0_RX_BUFFER[U0_Rx_SIZE];
extern uint8_t U0_TX_BUFFER[U0_Rx_SIZE];
#endif