#ifndef UI_TOUCH_H
#define UI_TOUCH_H

#include "Bridgetek_EVE2.hpp"
#include "dashboard.h"
#include "ti_fee.h"


int eve_calibrate(Bridgetek_EVE2 &eve);
int eve_read_tag(Bridgetek_EVE2 &eve, uint8_t *key);
void ui_handle_touch(Bridgetek_EVE2 &eve, dashboard_data_t *data);

// calibration & settings persistence
bool loadCalibration(Bridgetek_EVE2 &eve);
void saveCalibration(Bridgetek_EVE2 &eve);

#endif
