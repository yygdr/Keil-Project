#include "gd32f10x.h"
#include "usart.h"
#include "gd32f10x_usart.h"
uint8_t U0_RxBuff[U0_RX_SIZE];
uint8_t U0_TxBuff[U0_TX_SIZE];
UCB_CB  U0CB;

void Usart0_Init(uint32_t bandrate){
	rcu_periph_clock_enable(RCU_USART0);
	rcu_periph_clock_enable(RCU_GPIOA);
	
	gpio_init(GPIOA,GPIO_MODE_AF_PP,GPIO_OSPEED_50MHZ,GPIO_PIN_9);
	gpio_init(GPIOA,GPIO_MODE_IN_FLOATING,GPIO_OSPEED_50MHZ,GPIO_PIN_10);
	
	usart_deinit(USART0);
	usart_baudrate_set(USART0,bandrate);
	usart_parity_config(USART0,USART_PM_NONE);
	usart_word_length_set(USART0,USART_WL_8BIT);
	usart_stop_bit_set(USART0,USART_STB_1BIT);
	usart_transmit_config(USART0,USART_TRANSMIT_ENABLE);
	usart_receive_config(USART0,USART_RECEIVE_ENABLE);
	usart_dma_receive_config(USART0,USART_DENR_ENABLE);
	
	nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
	nvic_irq_enable(USART0_IRQn,0,0);
	usart_interrupt_enable(USART0,USART_INT_IDLE);
	
	U0Rx_PtrInit();
	DMA_Init();
	usart_enable(USART0);


}
void DMA_Init(void){
	dma_parameter_struct dma_init_struct;
	
	rcu_periph_clock_enable(RCU_DMA0);
	
	dma_deinit(DMA0,DMA_CH4);
	
	dma_init_struct.periph_addr = USART0+4;
	dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
	dma_init_struct.memory_addr = (uint32_t)U0_RxBuff;
	dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
	dma_init_struct.number = U0_RX_MAX+1;
	dma_init_struct.priority = DMA_PRIORITY_HIGH;
	dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
	dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
	dma_init_struct.direction = DMA_PERIPHERAL_TO_MEMORY;
	dma_init(DMA0,DMA_CH4,&dma_init_struct);
	dma_circulation_disable(DMA0,DMA_CH4);	
	dma_channel_enable(DMA0,DMA_CH4);
}

void U0Rx_PtrInit(void){
	U0CB.URxDataIN = &U0CB.URxDataPtr[0];
	U0CB.URxDataOUT = &U0CB.URxDataPtr[0];	
	U0CB.URxDataEND = &U0CB.URxDataPtr[NUM-1];
	U0CB.URxDataIN->start = U0_RxBuff;
	U0CB.URxCounter = 0;
}

void u0_printf(char *format,...){
	uint16_t i;
	va_list listdata;
	va_start(listdata,format);
	vsprintf((char *)U0_TxBuff,format,listdata);
	va_end(listdata);

	for(i=0;i<strlen((const char*)U0_TxBuff);i++){
		while(usart_flag_get(USART0,USART_FLAG_TBE)!=1);
		usart_data_transmit(USART0,U0_TxBuff[i]);
	}
	while(usart_flag_get(USART0,USART_FLAG_TBE)!=1);
}














