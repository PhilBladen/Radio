#ifndef __SST25V_FLASH_H
#define __SST25V_FLASH_H

#include "stdint.h"

void SST25_write_byte(uint32_t address, uint8_t data);
void SST25_read(uint32_t address, uint8_t *read_buffer, uint16_t size);

extern void flash_SPI_write(uint8_t *data, uint16_t size);
extern void flash_SPI_read(uint8_t *read_buffer, uint16_t size);
extern uint8_t flash_SPI_read_write_byte(uint8_t data);
extern void flash_CS_pin(uint8_t set);

#endif
