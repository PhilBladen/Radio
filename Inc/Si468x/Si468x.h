#ifndef __SI468X_H
#define __SI468X_H

#include <stdint.h>

#define Si4684_ADDRESS 0x64

typedef struct
{
	uint16_t size;
	uint8_t *data;
} Si468x_Command;

enum Si468x_MODE
{
	Si468x_MODE_FM =	0x00006000,//0x000063EC,
	Si468x_MODE_DAB =	0x00092000
};

union
{
	struct
	{
		uint8_t STCINT	: 1;
		uint8_t ACFINT	: 1;
		uint8_t RDSINT	: 1;
		uint8_t RSQINT	: 1;
		uint8_t DSRVINT	: 1;
		uint8_t DACQINT	: 1;
		uint8_t ERR_CMD : 1;
		uint8_t CTS		: 1;
	};
	uint8_t interrupt_register;
} Interrupt_Status;

enum Interrupt
{
	STCINT,
	ACFINT,
	RDSINT,
	RSQINT,
	DSRVINT,
	DACQINT,
	ERR_CMD,
	CTS
};

typedef struct
{
	union
	{
		uint8_t data[19];
		struct
		{
			uint8_t RSSILINT	: 1;
			uint8_t RSSIHINT	: 1;
			uint8_t ACQINT		: 1;
			uint8_t FICERRINT	: 1;
			uint8_t HARDMUTEINT	: 1;
			uint8_t				: 3;
			uint8_t VALID		: 1;
			uint8_t				: 1;
			uint8_t ACQ			: 1;
			uint8_t FICERR		: 1;
			uint8_t HARDMUTE	: 1;
			uint8_t				: 3;
		};
	};
} DAB_DigRad_Status;

typedef struct
{
	union
	{
		uint8_t data[4];
		struct
		{
			uint8_t SVRLISTINT	: 1;
			uint8_t FREQINFOINT : 1;
			uint8_t				: 1;
			uint8_t ANNOINT		: 1;
			uint8_t				: 2;
			uint8_t RECFGWRNINT : 1;
			uint8_t RECFGINT	: 1;
			uint8_t SVRLIST		: 1;
			uint8_t FREQ_INFO	: 1;
		};
	};
} DAB_Event_Status;

enum Si468x_MODE current_mode;

void si468x_init(enum Si468x_MODE mode);
void si468x_reset();
void si468x_interrupt();

Si468x_Command *build_command(uint8_t command_id, uint8_t *args, uint16_t num_args);
Si468x_Command *build_command_ext(uint8_t command_id, uint8_t *args, uint16_t num_args, uint8_t *data, uint16_t data_size);
uint8_t si468x_execute(Si468x_Command *command);
uint8_t si468x_execute_ext(Si468x_Command *command, uint8_t use_interrupt);
void wait_for_interrupt(enum Interrupt interrupt);
uint8_t si468x_read_response(uint8_t *response_buffer, uint16_t response_size);
void si468x_update_interrupts();
void si468x_decode_digital_service_list(uint8_t *service_list_data);
void si468x_start_digital_service(uint32_t service_id, uint32_t component_id);

void si468x_DAB_set_freq_list();
void si468x_DAB_tune(uint8_t freq_index);
void si468x_DAB_get_digrad_status(DAB_DigRad_Status *status);
void si468x_DAB_get_event_status(DAB_Event_Status *status);
void si468x_DAB_get_digital_service_list();

extern void I2C_write(uint16_t address, uint8_t *data, uint16_t size);
extern void I2C_read(uint16_t address, uint8_t *read_buffer, uint16_t size);

#endif
