#ifndef BOOT_H
#define BOOT_H

#include "stdint.h"

typedef void (*load_a)(void);       //函数指针类型声明

void BootLoader_Brance(void);       //函数声明
__asm void MSR_SP(uint32_t addr);   //函数声明
void LOAD_A(uint32_t addr);         //函数声明
void BootLoader_Clear(void);        //函数声明

#endif
