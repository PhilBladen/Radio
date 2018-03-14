#ifndef __SI468X_FM_H
#define __SI468X_FM_H

void si468x_FM_tune(float MHz);
float si468x_FM_seek(uint8_t up, uint8_t wrap);
void si468x_FM_RDS_status();

#endif
