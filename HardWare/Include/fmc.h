#ifndef FMC_H
#define FMC_H

#include "stdint.h"

void GD32_EraseFlash(uint16_t start, uint16_t num);
void GD32_WriteFlash(uint32_t saddr, uint32_t *wdata, uint32_t wnum);

#endif



