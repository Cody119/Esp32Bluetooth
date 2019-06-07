#ifndef GATT_H
#define GATT_H

#include "button_main.h"
#include "gap.h"

#define PROFILE_NUM 1
#define PROFILE_ECHO_ID 0

extern bool connect;

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void set_button(int next);

/* Esp32 uses a profile bassed system where each service is registered to a "profile".
   For the most part you need to handle this, it will jsut give you the id of the 
   relevent profile when an event occurs (gatt events).
   Ussually this is whatever profile the service was registered with.
   From there you need to manually invoke a call back you have set up or handle
   it any other way.

   This struct holds all the info for a particular profile, namley its callback
   function and its unique id assigned by the esp32
*/
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
};

extern struct gatts_profile_inst gl_profile_tab[PROFILE_NUM];

#endif