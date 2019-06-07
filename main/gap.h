#ifndef GAP_H
#define GAP_H

#include "button_main.h"
#include "gatt.h"

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
esp_err_t set_scan_params();

extern esp_ble_adv_data_t adv_data;

#endif