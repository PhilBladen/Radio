#include "stream_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void ensure_space(Stream *stream, uint16_t required_space);

Stream *stream_create()
{
	Stream *stream = malloc(sizeof(Stream));
	stream->temp_size = 0;
	stream->data_size = 2;
	stream->data = NULL;
	stream->read_address = 2;
	return stream;
}

Stream *stream_load(uint8_t *data, uint16_t size)
{
	Stream *stream = malloc(sizeof(Stream));
	stream->temp_size = 0;
	stream->data_size = size;
	stream->data = data;
	stream->read_address = 2;
	return stream;
}

void stream_free(Stream *stream)
{
	if (!stream)
		return;

	if (stream->data)
		free(stream->data);

	free(stream);
}

void stream_write_uint8(Stream *stream, uint8_t value)
{
	ensure_space(stream, 1);
	stream->temp[stream->temp_size++] = value;
}

void stream_write_uint16(Stream *stream, uint16_t value)
{
	ensure_space(stream, 2);
	stream->temp[stream->temp_size++] = value & 0xFF;
	stream->temp[stream->temp_size++] = value >> 8;
}

void stream_write_uint32(Stream *stream, uint32_t value)
{
	ensure_space(stream, 4);
	stream->temp[stream->temp_size++] = value & 0xFF;
	stream->temp[stream->temp_size++] = (value >> 8) & 0xFF;
	stream->temp[stream->temp_size++] = (value >> 16) & 0xFF;
	stream->temp[stream->temp_size++] = value >> 24;
}

void stream_write_bytes(Stream *stream, uint8_t *data, uint16_t size)
{
	if (size > BUFFER_SIZE)
		return; // ERROR!!!

	ensure_space(stream, size);
	memcpy(stream->temp + stream->temp_size, data, size);
	stream->temp_size += size;
}

uint8_t stream_read_uint8(Stream *stream)
{
	return stream->data[stream->read_address++];
}

uint16_t stream_read_uint16(Stream *stream)
{
	uint16_t value = 0;
	value |= stream->data[stream->read_address++];
	value |= (uint16_t) stream->data[stream->read_address++] << 8;
	return value;
}

uint32_t stream_read_uint32(Stream *stream)
{
	uint32_t value = 0;
	value |= stream->data[stream->read_address++];
	value |= (uint32_t) stream->data[stream->read_address++] << 8;
	value |= (uint32_t) stream->data[stream->read_address++] << 16;
	value |= (uint32_t) stream->data[stream->read_address++] << 24;
	return value;
}

void stream_read_bytes(Stream *stream, uint8_t *data, uint16_t size)
{
	memcpy(data, stream->data + stream->read_address, size);
	stream->read_address += size;
}

void ensure_space(Stream *stream, uint16_t required_space)
{
	if (stream->temp_size + required_space <= BUFFER_SIZE)
		return;

	stream_flush(stream);
}

void stream_flush(Stream *stream)
{
	stream->data = realloc(stream->data, stream->data_size + stream->temp_size);
	memcpy(stream->data + stream->data_size, stream->temp, stream->temp_size);
	stream->data_size += stream->temp_size;
	stream->temp_size = 0;

	stream->data[0] = stream->data_size & 0xFF;
	stream->data[1] = stream->data_size >> 8;
}
