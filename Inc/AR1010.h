#ifndef __AR1010_H
#define __AR1010_H

#include <stdint.h>

void AR1010_init();
void AR1010_scan(uint16_t lower, uint16_t upper);
void AR1010_tune(float freq, uint8_t convert);
void AR1010_auto_tune(float freq, uint8_t convert);
void AR1010_seek();
void AR1010_auto_seek();
void AR1010_set_volume(uint8_t volume);

extern void I2C_write(uint16_t address, uint8_t *data, uint16_t size);
extern void I2C_read(uint16_t address, uint8_t *read_buffer, uint16_t size);

#endif
