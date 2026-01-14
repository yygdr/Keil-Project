#include "gd32f10x.h"
#include "IIC.h"
#include "M24C02.h"
#include "main.h"

uint8_t M24C02_WriteByte(uint8_t addr, uint8_t wdata)
{
    uint8_t i;
    IIC_Start();
    IIC_Send_Byte(M24C02_WADDR);
    if(IIC_Wait_ACK(100)!=0) return 1;
    IIC_Send_Byte(addr);
    if(IIC_Wait_ACK(100)!=0) return 2;
    IIC_Send_Byte(wdata);
    if(IIC_Wait_ACK(100)!=0) return 3;
    IIC_Stop();
    return 0;

}

uint8_t M24C02_WritePage(uint8_t addr, uint8_t *wdata)
{
    uint8_t i;
    IIC_Start();
    IIC_Send_Byte(M24C02_WADDR);
    if(IIC_Wait_ACK(100)!=0) return 1;
    IIC_Send_Byte(addr);
    if(IIC_Wait_ACK(100)!=0) return 2;
    for(i=0;i<16;i++)
    {
        IIC_Send_Byte(wdata[i]);
        if(IIC_Wait_ACK(100)!=0) return i+3;
    }
    IIC_Stop();
    return 0;
}

uint8_t M24C02_ReadData(uint8_t addr, uint8_t *rdata, uint16_t datalen)
{
    uint8_t i;
    IIC_Start();
    IIC_Send_Byte(M24C02_WADDR);
    if(IIC_Wait_ACK(100)!=0) return 1;
    IIC_Send_Byte(addr);
    if(IIC_Wait_ACK(100)!=0) return 2;
    IIC_Start();
    IIC_Send_Byte(M24C02_RADDR);
    if(IIC_Wait_ACK(100)!=0) return 3;
    for(i=0;i<datalen-1;i++)
    {
        rdata[i] = IIC_Read_Byte(1);
    }
    rdata[datalen-1] = IIC_Read_Byte(0);
    IIC_Stop();
    return 0;
}

void M24C02_ReadOTAInfo(void)
{
    memset(&OTA_Info,0,OTA_INFOCB_SIZE);                         //清空OTA_Info结构体缓冲区
	  M24C02_ReadData(0,(uint8_t *)&OTA_Info,OTA_INFOCB_SIZE);     //从24C02读取数据，存放到OTA_Info结构体
}

void M24C02_WriteOTAInfo(void)
{
	uint8_t i;                               //用于for循环
	uint8_t *wptr;                           //uint8_t类型指针
	
	wptr = (uint8_t *)&OTA_Info;             //wptr指向OTA_Info结构体首地址
	for(i=0;i<OTA_INFOCB_SIZE/16;i++){       //每次写入一页16个字节
		M24C02_WritePage(i*16,wptr+i*16);    //写入一页数据
		Delay_Ms(5);                         //延时
	}		
}
