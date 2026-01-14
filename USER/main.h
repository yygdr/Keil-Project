#ifndef MAIN_H
#define MAIN_H

#include "stdint.h"
#include "string.h"

#define  GD32_FLASH_SADDR   0x08000000                                             //FLASH起始地址
#define  GD32_PAGE_SIZE     1024                                                   //FLASH扇区大小
#define  GD32_PAGE_NUM      64                                                     //FLASH总扇区个数
#define  GD32_B_PAGE_NUM    20                                                     //B区扇区个数
#define  GD32_A_PAGE_NUM    GD32_PAGE_NUM - GD32_B_PAGE_NUM                        //A区扇区个数
#define  GD32_A_START_PAGE  GD32_B_PAGE_NUM                                        //A区起始扇区编号
#define  GD32_A_SADDR       GD32_FLASH_SADDR + GD32_A_START_PAGE * GD32_PAGE_SIZE  //A区起始地址



#define  UPDATA_A_FLAG      0x00000001        //状态标志位，置位表明需要更新A了

#define OTA_SET_FLAG        0xAABB1122        //OTA_flag对勾状态对应的数值，如果OTA_flag等于该值，说明需要OTA更新A区
typedef struct{          
	uint32_t OTA_flag;                        //标志性的变量，等于OTA_SET_FLAG定义的值时，表明需要OTA更新A区
	uint32_t Firelen[11];                     //W25Q64中不同块中程序固件的长度，0号成员固定对应W25Q64中编码0的块，用于OTA
}OTA_InfoCB;                                  //OTA相关的信息结构体，需要保存到24C02
#define OTA_INFOCB_SIZE sizeof(OTA_InfoCB)    //OTA相关的信息结构体占用的字节长度
	
typedef struct
{
	uint8_t  Updatabuff[GD32_PAGE_SIZE];      //更新A区时，用于保存从W25Q64中读取的数据
	uint32_t W25Q64_BlockNB;                  //用于记录从哪个W25Q64的块中读取数据
}UpDataA_CB;                                  //更新A区用的结构体

extern OTA_InfoCB OTA_Info;   //外部变量声明
extern UpDataA_CB UpDataA;    //外部变量声明
extern uint32_t BootStaFlag;  //外部变量声明*

#endif



