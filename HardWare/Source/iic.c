#include "gd32f10x_gpio.h"
#include "delay.h"
#include "iic.h"
void IIC_Init()
{
    rcu_periph_clock_enable(RCU_GPIOB);
	gpio_init(GPIOB,GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
	gpio_init(GPIOB,GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_7);

    IIC_SCL_H;
    IIC_SDA_H;
    // Delay_Us(2);
    // IIC_SCL_H;
    // Delay_Us(2);
    // IIC_SDA_L;
    // Delay_Us(2);
    // IIC_SCL_L;
}

void IIC_Start()
{
    IIC_SCL_H;
    IIC_SDA_H;
    Delay_Us(2);
    IIC_SDA_L;
    Delay_Us(2);
    IIC_SCL_L;
}

void IIC_Stop()
{
    IIC_SCL_H;
    IIC_SDA_L;
    Delay_Us(2);
    IIC_SDA_H;
}

void IIC_Send_Byte(uint8_t tx)
{
    int8_t i;
    //IIC_Start();
    for(i=7;i>=0;i--)
    {
        IIC_SCL_L;
        if(tx&BIT(i))
        {
            IIC_SDA_H;  
        }
        else
        {
            IIC_SDA_L;
        }
        Delay_Us(2);
        IIC_SCL_H;
        Delay_Us(2);
    }
    IIC_SCL_L;
    IIC_SDA_H;
    //IIC_Stop();
}

uint8_t IIC_Wait_ACK(uint8_t timeout)
{
    do
    {
        timeout--;
        Delay_Us(2);
    } while ((READ_SDA)&&(timeout>=0));
    if(timeout<0) return 1;
    IIC_SCL_H;
    Delay_Us(2);
    if(READ_SDA!=0) return 2;
    IIC_SCL_L;
    Delay_Us(2);
    return 0;
    
}

uint8_t IIC_Read_Byte(uint8_t ack)
{
    int8_t i;
    uint8_t rxd;

    rxd = 0;
    for(i=7;i>=0;i--)
    {
        IIC_SCL_L;
        Delay_Us(2);
        IIC_SCL_H;
        if(READ_SDA)
            rxd |= BIT(i);
        Delay_Us(2);
    }
    IIC_SCL_L;
    Delay_Us(2);
    if(ack)
    {
        IIC_SDA_L;
        IIC_SCL_H;
        Delay_Us(2);
        IIC_SCL_L;
        IIC_SDA_H;
        Delay_Us(2);
    }
    else
    {
        IIC_SDA_H;
        IIC_SCL_H;
        Delay_Us(2);
        IIC_SCL_L;
        Delay_Us(2);
    }

return rxd;
}