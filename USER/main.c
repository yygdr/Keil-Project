#include "gd32f10x.h"
#include "main.h"
#include "ustart.h"
#include "delay.h"
#include "fmc.h"
#include "iic.h"
#include "m24c02.h"
#include "boot.h"
#include "w25q64.h"
OTA_InfoCB OTA_Info;          //保存在24C02内的OTA信息相关的结构体
UpDataA_CB UpDataA;           //A区更新用到的结构体
uint32_t BootStaFlag;         //记录全局状态标志位
uint8_t wdata[4]={0xAA,0xBB,0x11,0x22};
uint8_t rdata[4];
int main(void){
	uint8_t i;
	Delay_Init();            //延时初始化
	Usart0_Init(921600);     //串口0初始化
	//LOAD_A(GD32_A_SADDR);
	IIC_Init();              //IIC初始化
	for(i=0;i<=3;i++)
	{
		M24C02_WriteByte(i,wdata[3-i]);
		u0_printf("0x%x\r\n",wdata[3-i]);
		Delay_Ms(5);
		M24C02_ReadData(i, &rdata[i], 1);
		Delay_Ms(5);
		u0_printf("0x%x\r\n",rdata[i]);
	}
	 
	M24C02_ReadOTAInfo();    //从24C02读取数据到OTA_Info结构体
	//OTA_Info.OTA_flag = 0xAABB1122;
	BootLoader_Brance();     //分支判断
	while(1)
	{
		/*--------------------------------------------------*/
		/*        UPDATA_A_FLAG置位，表明需要更新A区        */
		/*--------------------------------------------------*/
		if(BootStaFlag&UPDATA_A_FLAG){
			u0_printf("长度%d字节\r\n",OTA_Info.Firelen[UpDataA.W25Q64_BlockNB]);          //串口0输出信息
			if(OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 4 == 0){                         //判断长度是否是4的整数倍，是的话进入if
				GD32_EraseFlash(GD32_A_START_PAGE,GD32_A_PAGE_NUM);                        //擦除A区FLASH
				for(i=0;i<OTA_Info.Firelen[UpDataA.W25Q64_BlockNB]/GD32_PAGE_SIZE;i++){    //每次读写一个扇区数据，使用for循环，写入整数个扇区
					W25Q64_Read(UpDataA.Updatabuff,i*1024 + UpDataA.W25Q64_BlockNB*64*1024 ,GD32_PAGE_SIZE);               //先从W25Q64读取一个单片机扇区的数据
					GD32_WriteFlash(GD32_FLASH_SADDR + i*GD32_PAGE_SIZE,(uint32_t *)UpDataA.Updatabuff,GD32_PAGE_SIZE);    //写入到单片机A区相应的扇区
				}
				if(OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 1024 != 0){                  //判断是否还有不足一个完整扇区的数据，有的话进入if					
					W25Q64_Read(UpDataA.Updatabuff,i*1024 + UpDataA.W25Q64_BlockNB*64*1024 ,OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 1024);             //从W25Q64读取不足一个完整扇区的数据
					GD32_WriteFlash(GD32_FLASH_SADDR + i*GD32_PAGE_SIZE,(uint32_t *)UpDataA.Updatabuff,OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 1024);  //然后写入单片机A区相应的扇区
				}
				if(UpDataA.W25Q64_BlockNB == 0){   //如果W25Q64_BlockNB是0，表示是OTA更新A区，进入if
					OTA_Info.OTA_flag = 0;         //设置OTA_flag，只要不是OTA_SET_FLAG定义的值即可
					M24C02_WriteOTAInfo();         //写入24C02内保存
				}
				NVIC_SystemReset();                //重启
			}else{                                 //判断长度是否是4的整数倍，不是的话进入else
				u0_printf("长度错误\r\n");         //串口0输出信息
				BootStaFlag &=~ UPDATA_A_FLAG;     //清除UPDATA_A_FLAG标志位
			}
		}
	}
}
