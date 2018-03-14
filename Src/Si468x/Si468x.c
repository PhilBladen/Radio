#include "stm32f7xx_hal.h"
#include "gpio.h"
#include "Si468x/Si468x.h"
#include "Si468x/Si468x_minipatch.h"
#include <stdlib.h>
#include <string.h>

// General:
#define RD_REPLY					0x00
#define POWER_UP					0x01
#define HOST_LOAD					0x04
#define FLASH_LOAD					0x05
#define LOAD_INIT					0x06
#define BOOT						0x07
#define GET_SYS_STATE				0x09
#define GET_POWER_UP_ARGS			0x0A
#define SET_PROPERTY				0x13
#define	FLASH_SET_PROP_LIST			0x05
#define GET_DIGITAL_SERVICE_LIST	0x80
#define START_DIGITAL_SERVICE		0x81

// DAB:
#define DAB_TUNE_FREQ				0xB0
#define DAB_DIGRAD_STATUS			0xB2
#define DAB_GET_EVENT_STATUS		0xB3
#define DAB_SET_FREQ_LIST			0xB8
#define DAB_GET_FREQ_LIST			0xB9
#define DAB_GET_TIME				0xBC

// Properties:
#define PROP_INT_CTL_ENABLE					0x0000
#define PROP_INT_CTL_REPEAT					0x0001
#define	PROP_DIGITAL_IO_OUTPUT_SELECT		0x0200
#define PROP_DIGITAL_IO_OUTPUT_SAMPLE_RATE	0x0201
#define PROP_PIN_CONFIG_ENABLE				0x0800
#define PROP_FLASH_SPI_CLOCK_FREQ_KHZ		0x0001
#define PROP_DAB_TUNE_FE_CFG				0x1712
#define PROP_FM_RDS_CONFIG					0x3C02

static const uint32_t dab_freq_list[] = {
		174928, 176640, 178352, 180064, 181936, 183648, 185360, 187072, 188928, 190640,
		192352, 194064, 195936, 197648, 199360, 201072, 202928, 204640, 206352, 208064,
		209936, 211648, 213360, 215072, 216928, 218640, 220352, 222064, 223936, 225648,
		227360, 229072, 230748, 232496, 234208, 235776, 237448, 239200
};

static void si468x_power_up();
static void si468x_load_init();
static void si468x_load_minipatch();
static void si468x_load_patch();
static void si468x_load_ROM();
static void si468x_boot();
static void si468x_set_property(uint16_t property, uint16_t value);
static void si468x_flash_set_property(uint16_t property, uint16_t value);
static void free_command(Si468x_Command *command);

static uint8_t patched = 0;
static uint8_t update_interrupts = 0;

void si468x_reset()
{
	HAL_GPIO_WritePin(SI_RST_GPIO_Port, SI_RST_Pin, GPIO_PIN_RESET);
}

void si468x_init(enum Si468x_MODE mode)
{
	current_mode = mode;

	si468x_reset();
	HAL_Delay(10);

	HAL_GPIO_WritePin(GPIOB, SI_RST_Pin, GPIO_PIN_SET);
	HAL_Delay(10);

	si468x_power_up();
	HAL_Delay(10);

	si468x_load_init();
	si468x_load_minipatch();
	HAL_Delay(10);

	si468x_load_init();
	si468x_load_patch();
	HAL_Delay(15);

	si468x_flash_set_property(PROP_FLASH_SPI_CLOCK_FREQ_KHZ, 0x9C40); // Set flash speed to 40MHz

	si468x_load_init();
	si468x_load_ROM(mode);

	si468x_boot();

	si468x_set_property(PROP_INT_CTL_ENABLE, 0x00D1); // Enable CTS, STC and DACQ interrupts
	si468x_set_property(PROP_INT_CTL_REPEAT, 0x0001); // Enable STC interrupt repeat
	si468x_set_property(PROP_DIGITAL_IO_OUTPUT_SELECT, 0x8000); // I2S set master
	si468x_set_property(PROP_DIGITAL_IO_OUTPUT_SAMPLE_RATE, 0xAC44); // I2S set sample rate 44.1kHz
	si468x_set_property(PROP_PIN_CONFIG_ENABLE, 0x8002); // I2S enable
	si468x_set_property(PROP_DAB_TUNE_FE_CFG, 0x0001); // VHFSW
	si468x_set_property(PROP_FM_RDS_CONFIG, 0x0001); // Enable RDS processor

	if (mode == Si468x_MODE_DAB)
		si468x_DAB_set_freq_list();
}

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
	Si468x_Command *command = build_command(DAB_SET_FREQ_LIST, args, args_size);
	si468x_execute(command);
	free_command(command);
	free(args);
}

void si468x_DAB_tune(uint8_t freq_index)
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint8_t args[] = {0x00, freq_index, 0x00, 0x00, 0x00};
	Si468x_Command *command = build_command(DAB_TUNE_FREQ, args, 5);
	Interrupt_Status.STCINT = 0;
	si468x_execute(command);
	free_command(command);
	wait_for_interrupt(STCINT);
}

void si468x_DAB_get_digrad_status(DAB_DigRad_Status *status)
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint8_t args[] = {0x00};
	Si468x_Command *command = build_command(DAB_DIGRAD_STATUS, args, 1);
	si468x_execute(command);
	free_command(command);

	uint8_t read_buffer[23];
	si468x_read_response(read_buffer, 23);
	memcpy(status->data, read_buffer + 4, 19);
	HAL_Delay(1);
}

void si468x_DAB_get_event_status(DAB_Event_Status *status)
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint8_t args[] = {0x00};
	Si468x_Command *command = build_command(DAB_GET_EVENT_STATUS, args, 1);
	si468x_execute(command);
	free_command(command);

	uint8_t read_buffer[8];
	si468x_read_response(read_buffer, 8);
	memcpy(status->data, read_buffer + 4, 4);
}

void si468x_DAB_get_digital_service_list()
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint8_t args[] = {0x00};
	Si468x_Command *command = build_command(GET_DIGITAL_SERVICE_LIST, args, 1);
	si468x_execute(command);
	free_command(command);

	uint8_t read_buffer[8];
	si468x_read_response(read_buffer, 6);
	uint16_t service_list_size = read_buffer[4] + (((uint16_t) read_buffer[5]) << 8);

	uint8_t *service_list_data = (uint8_t *) malloc(service_list_size + 4);
	if (!service_list_data)
	{
		HAL_Delay(1);
		// !!! ERROR
		return;
	}
	si468x_read_response(service_list_data, service_list_size + 4);
	si468x_decode_digital_service_list(service_list_data + 4);
	free(service_list_data);

	si468x_read_response(read_buffer, 6);
	HAL_Delay(1);
	//!!!
}

void si468x_decode_digital_service_list(uint8_t *service_list_data)
{
	if (current_mode != Si468x_MODE_DAB)
		return;

	uint16_t data_pointer = 4;
	uint8_t number_of_services = service_list_data[data_pointer];
	data_pointer += 4;

	unsigned char name[16];
	// Start reading service 1
	for (int i = 0; i < number_of_services; i++)
	{
		uint32_t service_id = service_list_data[data_pointer];
		service_id += ((uint32_t) service_list_data[data_pointer + 1]) << 8;
		service_id += ((uint32_t) service_list_data[data_pointer + 2]) << 16;
		service_id += ((uint32_t) service_list_data[data_pointer + 3]) << 24;
		data_pointer += 5;
		uint8_t num_components = service_list_data[data_pointer] & 0x0F;
		data_pointer += 3;
		memcpy(name, service_list_data + data_pointer, 16);
		data_pointer += 16;

		for (int j = 0; j < num_components; j++)
		{
			uint32_t component_id = service_list_data[data_pointer];
			component_id += ((uint32_t) service_list_data[data_pointer + 1]) << 8;
			component_id += ((uint32_t) service_list_data[data_pointer + 2]) << 16;
			component_id += ((uint32_t) service_list_data[data_pointer + 3]) << 24;
			data_pointer += 4;

			si468x_start_digital_service(service_id, component_id);
		}
	}
}

void si468x_start_digital_service(uint32_t service_id, uint32_t component_id)
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
	Si468x_Command *command = build_command(START_DIGITAL_SERVICE, args, 11);
	si468x_execute(command);
	free_command(command);
}

void si468x_power_up()
{
	uint8_t args[] = {
			0x80,	// CTSIEN
			0x14,	// CLK_MODE, TR_SIZE
			0x7F,	// IBIAS
			0x80,	// XTAL_FREQ
			0x8D,	// XTAL_FREQ
			0x5B,	// XTAL_FREQ
			0x00,	// XTAL_FREQ
			0x3F,	// CTUN
			0x10,
			0x00,
			0x00,
			0x00,
			0x7F,	// IBIAS_RUN
			0x00,
			0x00
	};
	Si468x_Command *command = build_command(POWER_UP, args, 15);
	si468x_execute(command);
	free_command(command);
}

void si468x_load_init()
{
	uint8_t args[] = {0x00};
	Si468x_Command *command = build_command(LOAD_INIT, args, 1);
	si468x_execute(command);
	free_command(command);
}

void si468x_load_minipatch()
{
	uint8_t args[] = {0x00, 0x00, 0x00};
	Si468x_Command *command = build_command_ext(HOST_LOAD, args, 3, minipatch_data, minipatch_size);
	si468x_execute(command);
	free_command(command);
}

void si468x_load_patch()
{
	uint8_t args[] = {0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	Si468x_Command *command = build_command(FLASH_LOAD, args, 11);
	si468x_execute(command);
	free_command(command);
	patched = 1;
}

void si468x_load_ROM(enum Si468x_MODE mode)
{
	uint8_t args[] = {
			0x00,
			0x00,
			0x00,
			mode & 0xFF,
			(mode >> 8) & 0xFF,
			(mode >> 16) & 0xFF,
			mode >> 24,
			0x00,
			0x00,
			0x00,
			0x00
	};
	Si468x_Command *command = build_command(FLASH_LOAD, args, 11);
	si468x_execute(command);
	free_command(command);
}

void si468x_boot()
{
	uint8_t args[] = {0x00};
	Si468x_Command *command = build_command(BOOT, args, 1);
	si468x_execute(command);
	free_command(command);
}

void si468x_set_property(uint16_t property, uint16_t value)
{
	uint8_t args[] = {0x00, property & 0xFF, property >> 8, value & 0xFF, value >> 8};
	Si468x_Command *command = build_command(SET_PROPERTY, args, 5);
	si468x_execute(command);
	free_command(command);
}

void si468x_flash_set_property(uint16_t property, uint16_t value)
{
	uint8_t args[] = {0x10, 0x00, 0x00, property & 0xFF, property >> 8, value & 0xFF, value >> 8};
	Si468x_Command *command = build_command(FLASH_SET_PROP_LIST, args, 7);
	si468x_execute(command);
	free_command(command);
}

void wait_for_interrupt(enum Interrupt interrupt)
{
	uint8_t status = 0;
	do
	{
		if (update_interrupts)
			si468x_update_interrupts();
		status = (Interrupt_Status.interrupt_register >> interrupt) & 0x01;
		if (interrupt == CTS)
			status |= (Interrupt_Status.interrupt_register >> ERR_CMD) & 0x01; //!!!
	} while (!status);
}

void si468x_update_interrupts()
{
	uint8_t status_byte;
	si468x_read_response(&status_byte, 1);
	while (status_byte == 0x00)
		si468x_read_response(&status_byte, 1); // !!!
	Interrupt_Status.interrupt_register = status_byte;
	update_interrupts = 0;
}

void si468x_interrupt()
{
	update_interrupts = 1;
}

uint8_t si468x_execute(Si468x_Command *command)
{
	return si468x_execute_ext(command, patched);
}

uint8_t si468x_execute_ext(Si468x_Command *command, uint8_t use_interrupt)
{
	if (use_interrupt)
		Interrupt_Status.CTS = 0;
	I2C_write(Si4684_ADDRESS, command->data, command->size);
	if (use_interrupt)
		wait_for_interrupt(CTS);
	uint8_t read_buffer[4];
	uint8_t error = si468x_read_response(read_buffer, 4);
	return error;
}

uint8_t si468x_read_response(uint8_t *response_buffer, uint16_t response_size)
{
	uint8_t command = RD_REPLY;
	I2C_write(Si4684_ADDRESS, &command, 1);
	I2C_read(Si4684_ADDRESS, response_buffer, response_size);
	return response_buffer[0] & 0x40 ? 1 : 0;
}

Si468x_Command *build_command(uint8_t command_id, uint8_t *args, uint16_t num_args)
{
	return build_command_ext(command_id, args, num_args, 0, 0);
}

Si468x_Command *build_command_ext(uint8_t command_id, uint8_t *args, uint16_t num_args, uint8_t *data, uint16_t data_size)
{
	Si468x_Command *command = (Si468x_Command *) malloc(sizeof(Si468x_Command));
	command->size = 1 + num_args + data_size;
	command->data = (uint8_t *) malloc(command->size);
	command->data[0] = command_id;
	memcpy(command->data + 1, args, num_args);
	memcpy(command->data + 1 + num_args, data, data_size);
	return command;
}

void free_command(Si468x_Command *command)
{
	free(command->data);
	free(command);
}
