#ifndef BUTTON_MAIN_H
#define BUTTON_MAIN_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "driver/gpio.h"

#include "sdkconfig.h"

#include "gap.h"
#include "gatt.h"

//Service Enum, has layout of service

enum {
    // The service 
    BS_SVC,

    // Button characteristic
    BA_CHAR,
    BA_VAL,
    BA_CFG,

    // Legnth of table
    BS_NB,
};

#define TAG "ECHO_BLE"
#define DEVICE_NAME "EC-S"

extern const char remote_device_name[];


extern uint8_t button_service_uuid128[16];
extern uint8_t button_char_uuid128[16];
#define adv_config_flag (1 << 0)
extern uint8_t adv_config_finished;
extern esp_ble_adv_params_t adv_params;

#endif