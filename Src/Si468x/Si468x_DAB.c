#include "Si468x/Si468x_DAB.h"
#include "Si468x/Si468x.h"
#include <stdlib.h>
#include <string.h>
#include <stm32f7xx_hal.h>

// DAB:
#define DAB_TUNE_FREQ				0xB0
#define DAB_DIGRAD_STATUS			0xB2
#define DAB_GET_EVENT_STATUS		0xB3
#define DAB_SET_FREQ_LIST			0xB8
#define DAB_GET_FREQ_LIST			0xB9
#define DAB_GET_TIME				0xBC
#define GET_DIGITAL_SERVICE_LIST	0x80

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

struct
{
	uint8_t size;
	DAB_Service **services;
} service_list = {0, 0};

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
			si468x_DAB_get_digital_service_list(freq_index);
		}
	}

	uint8_t currently_tuned_ensemble = 0;
	for (int i = 0; i < service_list.size; i++)
	{
		DAB_Service *service = service_list.services[i];
		if (service->pd_flag != 0)
			continue;
		char* target_name = "BBC Radio 1     ";
		if (strncmp(service->name, target_name, 16) != 0)
			continue;
		if (service->freq_index != currently_tuned_ensemble)
		{
			si468x_DAB_tune(service->freq_index);
			currently_tuned_ensemble = service->freq_index;
		}
		si468x_start_digital_service(service->service_id, service->components[0]->component_id);
	}
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

void si468x_DAB_get_digital_service_list(uint8_t freq_index)
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint8_t args[] = {0x00};
	Si468x_Command *command = si468x_build_command(GET_DIGITAL_SERVICE_LIST, args, 1);
	si468x_execute(command);
	si468x_free_command(command);

	uint8_t read_buffer[8];
	si468x_read_response(read_buffer, 6);
	uint16_t service_list_size = read_buffer[4] + (((uint16_t) read_buffer[5]) << 8);

	uint8_t *service_list_data = (uint8_t *) malloc(service_list_size + 4);
	if (!service_list_data)
		return; // !!! ERROR
	si468x_read_response(service_list_data, service_list_size + 4);
	si468x_DAB_decode_digital_service_list(service_list_data + 4, freq_index);
	free(service_list_data);
	//!!!
}

void si468x_DAB_decode_digital_service_list(uint8_t *service_list_data, uint8_t freq_index)
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint16_t data_pointer = 4;
	uint8_t number_of_services = service_list_data[data_pointer];
	data_pointer += 4;

	service_list.size += number_of_services;
	service_list.services = (DAB_Service**) realloc(service_list.services, service_list.size * sizeof(DAB_Service*));

	// Start reading service 1
	for (int i = service_list.size - number_of_services; i < service_list.size; i++)
	{
		DAB_Service *service = (DAB_Service*) malloc(sizeof(DAB_Service));
		service_list.services[i] = service;

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
			data_pointer += 4;

			DAB_Component *component = (DAB_Component*) malloc(sizeof(DAB_Component));
			component->component_id = component_id;
			component->component_info = service_list_data[data_pointer];

			service->components[j] = component;
		}
	}
}
