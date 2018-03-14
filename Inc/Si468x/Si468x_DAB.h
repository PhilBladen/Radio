#ifndef __SI368X_DAB_H
#define __SI368X_DAB_H

void si468x_DAB_set_freq_list();
void si468x_DAB_tune(uint8_t freq_index);
void si468x_DAB_get_digrad_status(DAB_DigRad_Status *status);
void si468x_DAB_get_event_status(DAB_Event_Status *status);
void si468x_DAB_get_digital_service_list();
void si468x_DAB_decode_digital_service_list(uint8_t *service_list_data);

#endif
