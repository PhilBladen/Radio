#ifndef __STREAM_UTILS_H
#define __STREAM_UTILS_H

#include <stdint.h>

#define BUFFER_SIZE 128

typedef struct
{
	uint8_t temp[BUFFER_SIZE];
	uint16_t temp_size;
	uint8_t *data;
	uint16_t data_size;
	uint16_t read_address;
} Stream;

Stream *stream_create();
void stream_free(Stream *stream);
Stream *stream_load(uint8_t *data, uint16_t size);
void stream_write_uint8(Stream *stream, uint8_t value);
void stream_write_uint16(Stream *stream, uint16_t value);
void stream_write_uint32(Stream *stream, uint32_t value);
void stream_write_bytes(Stream *stream, uint8_t *data, uint16_t size);
uint8_t stream_read_uint8(Stream *stream);
uint16_t stream_read_uint16(Stream *stream);
uint32_t stream_read_uint32(Stream *stream);
void stream_read_bytes(Stream *stream, uint8_t *data, uint16_t size);
void stream_flush(Stream *stream);

#endif
