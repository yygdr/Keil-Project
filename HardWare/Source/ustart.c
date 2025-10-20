#include "ustart.h"

uint8_t U0_RX_BUFFER[U0_Rx_SIZE];

UCB_CB U0CB;
void Usart0_Init(uint32_t bandrate)
{
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_USART0);
	
	gpio_init(GPIOA,GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
	gpio_init(GPIOA,GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10);
	
	/* reset USART */
	usart_deinit(USART0);
	/* configure USART baud rate value */
	usart_baudrate_set(USART0, bandrate);
	/* configure USART parity function */
	usart_parity_config(USART0, USART_PM_NONE);
	/* configure USART word length */
	usart_word_length_set(USART0, USART_WL_8BIT);
	/* configure USART stop bit length */
	usart_stop_bit_set(USART0, USART_STB_1BIT);
	
	usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
	usart_receive_config(USART0,USART_RECEIVE_ENABLE);
	usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);
	
	nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
	nvic_irq_enable(USART0_IRQn,0,0);
	usart_interrupt_enable(USART0, USART_INT_IDLE);
}

void DMA_Init(void)
{
	dma_parameter_struct dma_init_struct;
	rcu_periph_clock_enable(RCU_DMA0 );

	dma_init_struct.periph_addr = USART0 + 4;
	dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
	dma_init_struct.memory_addr = (uint32_t)U0_RX_BUFFER;
	dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
	dma_init_struct.number = U0_RX_MAX + 1;
	dma_init_struct.priority = DMA_PRIORITY_HIGH;
	dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
	dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
	dma_init_struct.direction = DMA_PERIPHERAL_TO_MEMORY;

	
	dma_init(DMA0, DMA_CH4, &dma_init_struct);  //???CH4
	
	/* disable DMA circulation mode */
	dma_circulation_disable(DMA0, DMA_CH4);
	
	dma_channel_enable(DMA0, DMA_CH4);
	
}
	
 void U0Rx_PtrInit(void)
 {
	U0CB.URxDataIN = &U0CB.URxDataPtr[0];
	U0CB.URxDataOUT = &U0CB.URxDataPtr[0];
	U0CB.URxDataEND = &U0CB.URxDataPtr[NUM-1];
	U0CB.URxDataIN ->start = U0_RX_BUFFER;
	U0CB.URxCounter = 0;

 }
















