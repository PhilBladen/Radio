#include "Si468x/Si468x_DAB.h"
#include "Si468x/Si468x.h"
#include <stdlib.h>
#include <string.h>
#include <stm32f7xx_hal.h>
#include "SST25V_flash.h"
#include "stream_utils.h"

// DAB:
#define DAB_TUNE_FREQ				0xB0
#define DAB_DIGRAD_STATUS			0xB2
#define DAB_GET_EVENT_STATUS		0xB3
#define DAB_SET_FREQ_LIST			0xB8
#define DAB_GET_FREQ_LIST			0xB9
#define DAB_GET_TIME				0xBC
#define DAB_GET_COMPONENT_INFO		0xBB
#define DAB_GET_TIME				0xBC

#define GET_DIGITAL_SERVICE_LIST	0x80
#define START_DIGITAL_SERVICE		0x81
#define GET_DIGITAL_SERVICE_DATA	0x84

static const uint32_t dab_freq_list[] = {
		174928, 176640, 178352, 180064, 181936, 183648, 185360, 187072, 188928, 190640,
		192352, 194064, 195936, 197648, 199360, 201072, 202928, 204640, 206352, 208064,
		209936, 211648, 213360, 215072, 216928, 218640, 220352, 222064, 223936, 225648,
		227360, 229072, 230748, 232496, 234208, 235776, 237448, 239200
};

typedef struct
{
	uint32_t component_id;
	union
	{
		uint8_t component_info;
		struct
		{
			uint8_t ca_flag : 1;
			uint8_t ps_flag : 1; // Primary or secondary component
			uint8_t ty		: 6; // Audio/Data service component type
		};
	};
} DAB_Component;

typedef struct
{
	uint8_t freq_index;
	uint32_t service_id;

	union
	{
		uint8_t service_info_1;
		struct
		{
			uint8_t pd_flag	: 1; // 0 for audio, 1 for data
			uint8_t pty		: 5; // Program type
		};
	};
	union
	{
		uint8_t service_info_2;
		uint8_t num_comp : 4; // Number of components
	};

	DAB_Component **components;
	char name[16];
} DAB_Service;

typedef struct
{
	uint8_t size;
	DAB_Service **services;
} DAB_Service_List;

void si468x_DAB_save_service_to_flash(DAB_Service *service, uint16_t memory_index);
//void si468x_load_service_name_list_from_flash(uint16_t memory_index);
DAB_Service *si468x_load_service_from_flash(uint16_t memory_index);
DAB_Service_List *si468x_DAB_decode_digital_service_list(uint8_t *service_list_data, uint8_t freq_index);
DAB_Service_List *si468x_DAB_get_digital_service_list(uint8_t freq_index);

void si468x_DAB_set_freq_list()
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint8_t number_of_frequencies = sizeof(dab_freq_list) / sizeof(uint32_t);
	uint16_t args_size = 4 + number_of_frequencies * 4;
	uint8_t *args = (uint8_t *) malloc(args_size);
	args[0] = number_of_frequencies;
	args[1] = 0x00;
	args[2] = 0x00;
	for (int i = 0; i < number_of_frequencies; i++)
	{
		args[3 + 4 * i] = dab_freq_list[i] & 0xFF;
		args[4 + 4 * i] = (dab_freq_list[i] >> 8) & 0xFF;
		args[5 + 4 * i] = (dab_freq_list[i] >> 16) & 0xFF;
		args[6 + 4 * i] = dab_freq_list[i] >> 24;
	}
	Si468x_Command *command = si468x_build_command(DAB_SET_FREQ_LIST, args, args_size);
	si468x_execute(command);
	si468x_free_command(command);
	free(args);
}

void si468x_DAB_band_scan()
{
	DAB_DigRad_Status digrad_status;
	DAB_Event_Status event_status;
	volatile uint16_t service_mem_id = 0;
	uint16_t total_services = 0;
	for (int freq_index = 0; freq_index < sizeof(dab_freq_list) / sizeof(uint32_t); freq_index++)
	{
		si468x_DAB_tune(freq_index);
		si468x_DAB_get_digrad_status(&digrad_status);

		if (digrad_status.VALID)
		{
			event_status.SVRLIST = 0;
			while (!event_status.SVRLIST)
				si468x_DAB_get_event_status(&event_status);

			HAL_Delay(500);
			DAB_Service_List *service_list = si468x_DAB_get_digital_service_list(freq_index);
			total_services += service_list->size;
			for (uint8_t service_index = 0; service_index < service_list->size; service_index++)
				service_mem_id++;//si468x_DAB_save_service_to_flash(service_list->services[service_index], service_mem_id++);
		}
	}
	SST25_sector_erase_4K(4096);
	SST25_write(4096, (uint8_t *) &total_services, 2);
	//!!! Handle memory freeing
}

void si468x_DAB_tune_service(uint16_t service_mem_id)
{
	DAB_Service *service = si468x_load_service_from_flash(service_mem_id);
	si468x_DAB_tune(service->freq_index);
	si468x_DAB_start_digital_service(service->service_id, service->components[0]->component_id, SER_AUDIO);
}

void si468x_DAB_tune(uint8_t freq_index)
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint8_t args[] = {0x00, freq_index, 0x00, 0x00, 0x00};
	Si468x_Command *command = si468x_build_command(DAB_TUNE_FREQ, args, 5);
	Interrupt_Status.STCINT = 0;
	si468x_execute(command);
	si468x_free_command(command);
	si468x_wait_for_interrupt(STCINT);
}

void si468x_DAB_get_digrad_status(DAB_DigRad_Status *status)
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint8_t args[] = {0x00};
	Si468x_Command *command = si468x_build_command(DAB_DIGRAD_STATUS, args, 1);
	si468x_execute(command);
	si468x_free_command(command);

	uint8_t read_buffer[23];
	si468x_read_response(read_buffer, 23);
	memcpy(status->data, read_buffer + 4, 19);
}

void si468x_DAB_get_event_status(DAB_Event_Status *status)
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint8_t args[] = {0x00};
	Si468x_Command *command = si468x_build_command(DAB_GET_EVENT_STATUS, args, 1);
	si468x_execute(command);
	si468x_free_command(command);

	uint8_t read_buffer[8];
	si468x_read_response(read_buffer, 8);
	memcpy(status->data, read_buffer + 4, 4);
}

DAB_Service_List *si468x_DAB_get_digital_service_list(uint8_t freq_index)
{
	if (current_mode != Si468x_MODE_DAB)
		return NULL;

	uint8_t args[] = {0x00};
	Si468x_Command *command = si468x_build_command(GET_DIGITAL_SERVICE_LIST, args, 1);
	si468x_execute(command);
	si468x_free_command(command);

	uint8_t read_buffer[8];
	si468x_read_response(read_buffer, 6);
	uint16_t service_list_size = read_buffer[4] + (((uint16_t) read_buffer[5]) << 8);

	uint8_t *service_list_data = (uint8_t *) malloc(service_list_size + 4);
	if (!service_list_data)
		return NULL; // !!! ERROR
	si468x_read_response(service_list_data, service_list_size + 4);
	DAB_Service_List *service_list = si468x_DAB_decode_digital_service_list(service_list_data + 4, freq_index);
	free(service_list_data);

	return service_list;
	//!!!
}

void si468x_DAB_get_component_info(uint32_t service_id, uint32_t component_id)
{
	uint8_t args[] = {
			0x00,
			0x00,
			0x00,
			service_id & 0xFF,
			(service_id >> 8) & 0xFF,
			(service_id >> 16) & 0xFF,
			service_id >> 24,
			component_id & 0xFF,
			(component_id >> 8) & 0xFF,
			(component_id >> 16) & 0xFF,
			component_id >> 24
	};
	Si468x_Command *command = si468x_build_command(DAB_GET_COMPONENT_INFO, args, 1);
	si468x_execute(command);
	si468x_free_command(command);

	HAL_Delay(5000);

	uint8_t response_buffer[50];
	si468x_read_response(response_buffer, 50);
	uint8_t num_user_applications = response_buffer[26];
	//if (num_user_applications)
		HAL_Delay(1);
	// !!!
}

void si468x_DAB_start_digital_service(uint32_t service_id, uint32_t component_id, enum Digital_Service_Type service_type)
{
	uint8_t args[] = {
			service_type,
			0x00,
			0x00,
			service_id & 0xFF,
			(service_id >> 8) & 0xFF,
			(service_id >> 16) & 0xFF,
			service_id >> 24,
			component_id & 0xFF,
			(component_id >> 8) & 0xFF,
			(component_id >> 16) & 0xFF,
			component_id >> 24
	};
	Si468x_Command *command = si468x_build_command(START_DIGITAL_SERVICE, args, 11);
	si468x_execute(command);
	si468x_free_command(command);
}

void si468x_DAB_get_digital_service_data(uint8_t *buffer, uint16_t *size, uint8_t only_status)
{
	uint8_t args[] = {0x01 | (only_status ? 0x10 : 0x00)};
	Si468x_Command *command = si468x_build_command(GET_DIGITAL_SERVICE_DATA, args, 1);
	si468x_execute(command);
	si468x_free_command(command);

	si468x_read_response(buffer, 20);

	uint16_t byte_count = (buffer[19] << 8) + buffer[18];
	if (!byte_count)
		return;

	uint8_t data_source = buffer[7] >> 6;
	if (data_source != 0x02)
		return;

	si468x_read_response(buffer, 24 + byte_count);
	HAL_Delay(1);
}

DAB_Time si468x_DAB_get_time()
{
	uint8_t args[] = {0x00};
	Si468x_Command *command = si468x_build_command(DAB_GET_TIME, args, 1);
	si468x_execute(command);
	si468x_free_command(command);

	DAB_Time time;
	si468x_read_response(time.data, 11);

	HAL_Delay(1);

	return time;
}

DAB_Service_List *si468x_DAB_decode_digital_service_list(uint8_t *service_list_data, uint8_t freq_index)
{
	if (current_mode != Si468x_MODE_DAB)
		return NULL;

	DAB_Service_List *service_list = malloc(sizeof(DAB_Service_List));

	uint16_t data_pointer = 4;
	uint8_t number_of_services = service_list_data[data_pointer];
	data_pointer += 4;

	service_list->size = number_of_services;
	service_list->services = (DAB_Service**) malloc(service_list->size * sizeof(DAB_Service*));

	// Start reading service 1
	for (int i = 0; i < service_list->size; i++)
	{
		DAB_Service *service = (DAB_Service*) malloc(sizeof(DAB_Service));
		service_list->services[i] = service;

		uint32_t service_id = service_list_data[data_pointer];
		service_id += ((uint32_t) service_list_data[data_pointer + 1]) << 8;
		service_id += ((uint32_t) service_list_data[data_pointer + 2]) << 16;
		service_id += ((uint32_t) service_list_data[data_pointer + 3]) << 24;
		data_pointer += 4;
		service->service_info_1 = service_list_data[data_pointer];
		data_pointer += 1;
		service->service_info_2 = service_list_data[data_pointer];
		data_pointer += 3;
		memcpy(service->name, service_list_data + data_pointer, 16);
		data_pointer += 16;

		service->freq_index = freq_index;
		service->service_id = service_id;

		service->components = (DAB_Component**) malloc(service->num_comp * sizeof(DAB_Component*));
		for (int j = 0; j < service->num_comp; j++)
		{
			uint32_t component_id = service_list_data[data_pointer];
			component_id += ((uint32_t) service_list_data[data_pointer + 1]) << 8;
			component_id += ((uint32_t) service_list_data[data_pointer + 2]) << 16;
			component_id += ((uint32_t) service_list_data[data_pointer + 3]) << 24;
			data_pointer += 3;

			uint8_t user_application_valid = service_list_data[data_pointer] & 0x01;
			if (user_application_valid)
				HAL_Delay(1);
			data_pointer += 1;

			DAB_Component *component = (DAB_Component*) malloc(sizeof(DAB_Component));
			component->component_id = component_id;
			component->component_info = service_list_data[data_pointer];

			service->components[j] = component;
		}
	}

	return service_list;
}

void si468x_DAB_save_service_to_flash(DAB_Service *service, uint16_t memory_index)
{
	Stream *stream = stream_create();
	uint32_t memory_address = 4096 * (memory_index + 2); // Keep first 8192 bytes free for control data
	SST25_sector_erase_4K(memory_address);
	stream_write_uint8(stream, service->freq_index);
	stream_write_uint32(stream, service->service_id);
	stream_write_uint8(stream, service->service_info_1);
	stream_write_uint8(stream, service->service_info_2);
	stream_write_bytes(stream, (uint8_t *) service->name, 16);
	for (uint8_t component_index = 0; component_index < service->num_comp; component_index++)
	{
		DAB_Component *component = service->components[component_index];
		stream_write_uint32(stream, component->component_id);
		stream_write_uint8(stream, component->component_info);
	}
	stream_flush(stream);
	SST25_write(memory_address, stream->data, stream->data_size);
	stream_free(stream);
}

DAB_Service *si468x_load_service_from_flash(uint16_t memory_index)
{
	uint16_t stream_size;
	SST25_read(4096 * (memory_index + 2), (uint8_t *) &stream_size, 2);
	uint8_t *data = malloc(stream_size);
	SST25_read(4096 * (memory_index + 2), data, stream_size);
	Stream *stream = stream_load(data, stream_size);

	DAB_Service *service = malloc(sizeof(DAB_Service));
	service->freq_index = stream_read_uint8(stream);
	service->service_id = stream_read_uint32(stream);
	service->service_info_1 = stream_read_uint8(stream);
	service->service_info_2 = stream_read_uint8(stream);
	stream_read_bytes(stream, (uint8_t *) service->name, 16);
	service->components = (DAB_Component**) malloc(service->num_comp * sizeof(DAB_Component*));
	for (uint8_t component_index = 0; component_index < service->num_comp; component_index++)
	{
		DAB_Component *component = malloc(sizeof(DAB_Component));
		component->component_id = stream_read_uint32(stream);
		component->component_info = stream_read_uint8(stream);
		service->components[component_index] = component;
	}
	stream_free(stream);
	return service;
}
