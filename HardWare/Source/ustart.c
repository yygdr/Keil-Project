#include "stdarg.h"
#include "ustart.h"
#include "stdio.h"
#include "string.h"
uint8_t U0_RX_BUFFER[U0_Rx_SIZE];
uint8_t U0_TX_BUFFER[U0_Rx_SIZE];
UCB_CB U0CB;
void Usart0_Init(uint32_t bandrate)
{
	rcu_periph_clock_enable(RCU_GPIOA);        //开启GPIO时钟
	rcu_periph_clock_enable(RCU_USART0);       //开启USART0时钟
	
	gpio_init(GPIOA,GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);  //初始化PA9
	gpio_init(GPIOA,GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10); //初始化PA10
	
	/* reset USART */
	usart_deinit(USART0);                                           //初始化           
	/* configure USART baud rate value */                           
	usart_baudrate_set(USART0, bandrate);                           //设置波特率
	/* configure USART parity function */
	usart_parity_config(USART0, USART_PM_NONE);                     //设置优先级
	/* configure USART word length */
	usart_word_length_set(USART0, USART_WL_8BIT);                   //配置传输字长
	/* configure USART stop bit length */
	usart_stop_bit_set(USART0, USART_STB_1BIT);                     //usart停止位位长
	
	usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);           //usart发送器使能         
	usart_receive_config(USART0,USART_RECEIVE_ENABLE);              //usart接收使能
	usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);     //dma接收使能
	
	nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);               //二分组：抢占和优先级
	nvic_irq_enable(USART0_IRQn,0,0);                               //打开中断
	usart_interrupt_enable(USART0, USART_INT_IDLE);                 //空闲中断

	U0Rx_PtrInit();                                                 //指针初始化
	DMA_Init();                                                     //DMA初始化
	usart_enable(USART0);                                           //CTR寄存器BIT13 使能usart
}

void DMA_Init(void)
{
	dma_parameter_struct dma_init_struct;
	rcu_periph_clock_enable(RCU_DMA0 );                            //打开时钟

	dma_init_struct.periph_addr = USART0 + 4;                      //usart DATA寄存器地址
	dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;      //传输字节位数
	dma_init_struct.memory_addr = (uint32_t)U0_RX_BUFFER;          //目的地址
	dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;          //接收字节位数
	dma_init_struct.number = U0_RX_MAX + 1;                        //传输字节数
	dma_init_struct.priority = DMA_PRIORITY_HIGH;                  //优先级
	dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;      //外设地址不增
	dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;       //接收地址增加
	dma_init_struct.direction = DMA_PERIPHERAL_TO_MEMORY;          //方向

	
	dma_init(DMA0, DMA_CH4, &dma_init_struct);  //???CH4   DMA初始化
	
	/* disable DMA circulation mode */
	dma_circulation_disable(DMA0, DMA_CH4);     //关闭DMA循环模式？？？
	
	dma_channel_enable(DMA0, DMA_CH4);          //使能
	
}
	
 void U0Rx_PtrInit(void)
 {
	U0CB.URxDataIN = &U0CB.URxDataPtr[0];
	U0CB.URxDataOUT = &U0CB.URxDataPtr[0];
	U0CB.URxDataEND = &U0CB.URxDataPtr[NUM-1];
	U0CB.URxDataIN ->start = U0_RX_BUFFER;
	U0CB.URxCounter = 0;

 }

void u0_printf(char *format,...)
{
	uint16_t i;
	va_list listdata;                                        //创建va_list变量
	va_start(listdata,format);                               //
	vsprintf((char *)U0_TX_BUFFER,format,listdata);
	va_end(listdata);

	for(i = 0;i<strlen((const char*)U0_TX_BUFFER);i++)
	{
		while(usart_flag_get(USART0, USART_FLAG_TBE) != 1)//1:发送缓冲区为空 0:有数据
		usart_data_transmit(USART0, U0_TX_BUFFER[i]);

	}
	while(usart_flag_get(USART0, USART_FLAG_TBE)!=1);
}














