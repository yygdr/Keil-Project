#ifndef USART_H
#define USART_H

#include "stdint.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

#define U0_RX_SIZE 2048
#define U0_TX_SIZE 2048
#define U0_RX_MAX  256
#define NUM        10

typedef struct{
	uint8_t *start;
	uint8_t *end;
}UCB_URxBuffptr;

typedef struct{
	uint16_t URxCounter;
	UCB_URxBuffptr URxDataPtr[NUM];
	UCB_URxBuffptr *URxDataIN;
	UCB_URxBuffptr *URxDataOUT;
	UCB_URxBuffptr *URxDataEND;
}UCB_CB;

extern UCB_CB  U0CB;
extern uint8_t U0_RxBuff[U0_RX_SIZE];

void Usart0_Init(uint32_t bandrate);
void DMA_Init(void);
void U0Rx_PtrInit(void);
void u0_printf(char *format,...);

#endif
