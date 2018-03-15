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

void si468x_DAB_set_freq_list();
void si468x_DAB_tune(uint8_t freq_index);
void si468x_DAB_band_scan();
void si468x_DAB_get_digrad_status(DAB_DigRad_Status *status);
void si468x_DAB_get_event_status(DAB_Event_Status *status);
void si468x_DAB_get_digital_service_list(uint8_t freq_index);
void si468x_DAB_decode_digital_service_list(uint8_t *service_list_data, uint8_t freq_index);

#endif
