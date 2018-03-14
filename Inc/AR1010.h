#ifndef __AR1010_H
#define __AR1010_H

#include <stdint.h>

extern void I2C_write(uint16_t address, uint8_t *data, uint16_t size);
extern void I2C_read(uint16_t address, uint8_t *read_buffer, uint16_t size);

#endif
