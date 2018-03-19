#ifndef __SI468X_DAB_H
#define __SI468X_DAB_H

#include <stdint.h>

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

typedef struct
{
	union
	{
		uint8_t data[11];
		struct
		{
			uint32_t : 24;
			uint16_t year;
			uint8_t month;
			uint8_t day;
			uint8_t hour;
			uint8_t minute;
			uint8_t second;
		};
	};
} DAB_Time;

void si468x_DAB_set_freq_list();
void si468x_DAB_tune(uint8_t freq_index);
void si468x_DAB_band_scan();
void si468x_DAB_get_digrad_status(DAB_DigRad_Status *status);
void si468x_DAB_get_event_status(DAB_Event_Status *status);
void si468x_DAB_get_digital_service_list(uint8_t freq_index);
void si468x_DAB_decode_digital_service_list(uint8_t *service_list_data, uint8_t freq_index);
void si468x_DAB_get_component_info(uint32_t service_id, uint32_t component_id);
DAB_Time si468x_DAB_get_time();

#endif
