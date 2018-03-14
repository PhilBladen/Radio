#include "Si468x/Si468x.h"
#include "Si468x/Si468x_FM.h"
#include <stdlib.h>

// FM:
#define FM_TUNE_FREQ				0x30
#define FM_SEEK_START				0x31
#define FM_RSQ_STATUS				0x32
#define FM_RDS_STATUS				0x34
#define FM_RDS_BLOCKCOUNT			0x35

void si468x_FM_tune(float MHz)
{
	if (current_mode != Si468x_MODE_FM)
		return;

	uint16_t freq = MHz * 100;
	uint8_t args[] = {0x00, freq & 0xFF, freq >> 8, 0x00, 0x00, 0x00};
	Si468x_Command *command = build_command(FM_TUNE_FREQ, args, 6);
	Interrupt_Status.STCINT = 0;
	si468x_execute(command);
	wait_for_interrupt(STCINT);
	free_command(command);
}

float si468x_FM_seek(uint8_t up, uint8_t wrap)
{
	if (current_mode != Si468x_MODE_FM)
		return 0;

	uint8_t args[] = {
			0x10,
			0x00 | ((up & 0x1) << 1) | (wrap & 0x1),
			0x00,
			0x00,
			0x00
	};
	Si468x_Command *command = build_command(FM_SEEK_START, args, 5);
	Interrupt_Status.STCINT = 0;
	si468x_execute(command);
	free(command);
	wait_for_interrupt(STCINT);

	args[0] = 0x00;
	command = build_command(FM_RSQ_STATUS, args, 1);
	si468x_execute(command);
	uint8_t read_buffer[22];
	si468x_read_response(read_buffer, 22);
	uint16_t new_freq = read_buffer[6] + (((uint16_t) read_buffer[7]) << 8);
	free_command(command);
	return (float) new_freq / 100;
}

unsigned char station_name[8];
void si468x_FM_RDS_status()
{
	uint8_t args[] = {0x01};
	Si468x_Command *command = build_command(FM_RDS_STATUS, args, 1);
	si468x_execute(command);
	free_command(command);

	uint8_t rds_data[20];
	si468x_read_response(rds_data, 20);

	uint16_t BLOCK_A = ((uint16_t) rds_data[13] << 8) + rds_data[12];
	uint16_t BLOCK_B = ((uint16_t) rds_data[15] << 8) + rds_data[14];
	uint16_t BLOCK_C = ((uint16_t) rds_data[17] << 8) + rds_data[16];
	uint16_t BLOCK_D = ((uint16_t) rds_data[19] << 8) + rds_data[18];
	uint8_t group_type = (BLOCK_B & 0xF800) >> 8;
	uint8_t C10 = BLOCK_B & 0x03;
	if (group_type == 0)
	{
		uint8_t decoder_identification_code = 3 - C10;
		station_name[6 - 2 * decoder_identification_code] = BLOCK_D >> 8;
		station_name[6 - 2 * decoder_identification_code + 1] = BLOCK_D & 0xFF;
	}
	else if (group_type == 4)
	{
		uint32_t date_and_time;
		date_and_time = (BLOCK_B & 0x03) << 16;
		date_and_time += BLOCK_C << 8;
		date_and_time += BLOCK_D;

		uint8_t minutes = (date_and_time & 0x0FC0) >> 5;
		// !!!
	}
}
