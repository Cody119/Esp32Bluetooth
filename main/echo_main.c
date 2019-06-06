#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "sdkconfig.h"

#include "echo_main.h"

#define TAG "ECHO_BLE"
#define DEVICE_NAME "EC-S"

#define adv_config_flag (1 << 0)

// Not sure what this is? 
// Used in ESP_GATTS_REG_EVT event when registering the services table
#define SVC_INST_ID 0

uint8_t adv_config_finished = 0;

//CF5A0415-76A0-4B41-A4B0-6C33C520F354
static uint8_t echo_service_uuid128[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0x54, 0xF3, 0x20, 0xC5, 0x33, 0x6C, 0xB0, 0xA4, 0x41, 0x4B, 0xA0, 0x76, 0x15, 0x04, 0x5A, 0xCF,
};

//CF5A0415-76A0-4B41-A4B0-6C33C520F354
static uint8_t in_char_uuid128[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0x54, 0xF3, 0x66, 0xC5, 0x33, 0x6C, 0xB0, 0xA4, 0x41, 0x4B, 0xA0, 0x76, 0x15, 0x04, 0x5A, 0xCF,
};

//CF5A0415-76A0-4B41-A4B0-6C33C520F354
static uint8_t out_char_uuid128[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0x54, 0xF3, 0x55, 0xC5, 0x33, 0x6C, 0xB0, 0xA4, 0x41, 0x4B, 0xA0, 0x76, 0x15, 0x04, 0x5A, 0xCF,
};


static uint8_t out_char_val[20] = {0x45};
static uint8_t out_char_ccc[2] = {0x00, 0x00};


static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

uint16_t echo_handle_table[ESS_IDX_NB];


// static uint8_t in_uuid[ESP_UUID_LEN_128]

/* Advertising parameters
typedef struct {
    uint16_t adv_int_min;
    //!< Minimum advertising interval for undirected and low duty cycle directed advertising.
    //              Range: 0x0020 to 0x4000
    //              Default: N = 0x0800 (1.28 second)
    //              Time = N * 0.625 msec
    //              Time Range: 20 ms to 10.24 sec 
    uint16_t adv_int_max;
    //!< Maximum advertising interval for undirected and low duty cycle directed advertising.
    //              Range: 0x0020 to 0x4000
    //              Default: N = 0x0800 (1.28 second)
    //              Time = N * 0.625 msec
    //              Time Range: 20 ms to 10.24 sec 
    esp_ble_adv_type_t adv_type;            //!< Advertising type 
    esp_ble_addr_type_t own_addr_type;      //!< Owner bluetooth device address type 
    esp_bd_addr_t peer_addr;                //!< Peer device bluetooth device address 
    esp_ble_addr_type_t peer_addr_type;     //!< Peer device bluetooth device address type 
    esp_ble_adv_channel_t channel_map;      //!< Advertising channel map 
    esp_ble_adv_filter_t adv_filter_policy; //!< Advertising filter policy 
}
esp_ble_adv_params_t;
*/

static esp_ble_adv_params_t test_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr        =
    //.peer_addr_type   =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/*
typedef struct {
    bool set_scan_rsp;            //!< Set this advertising data as scan response or not
    bool include_name;            //!< Advertising data include device name or not 
    bool include_txpower;         //!< Advertising data include TX power 
    int min_interval;             //!< Advertising data show slave preferred connection min interval 
    int max_interval;             //!< Advertising data show slave preferred connection max interval 
    int appearance;               //!< External appearance of device 
    uint16_t manufacturer_len;    //!< Manufacturer data length 
    uint8_t *p_manufacturer_data; //!< Manufacturer data point 
    uint16_t service_data_len;    //!< Service data length 
    uint8_t *p_service_data;      //!< Service data point 
    uint16_t service_uuid_len;    //!< Service uuid length 
    uint8_t *p_service_uuid;      //!< Service uuid array point 
    uint8_t flag;                 //!< Advertising flag of discovery mode, see BLE_ADV_DATA_FLAG detail 
} esp_ble_adv_data_t;
*/

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0, //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data =  NULL, //&test_manufacturer[0],
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(echo_service_uuid128),
    .p_service_uuid = echo_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};


#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ;

/// Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t echo_gatt_db[ESS_IDX_NB] =
{
    // Echo Service Declaration
    [ESS_IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {
        //This is a unique uuid that indicates this is a primary service
        ESP_UUID_LEN_16, 
        (uint8_t *)&primary_service_uuid, 
        ESP_GATT_PERM_READ,
        sizeof(echo_service_uuid128), 
        sizeof(echo_service_uuid128), 
        echo_service_uuid128
    }},


    // Echo Out Characteristic Declaration
    [ESS_IDX_OUT_CHAR] = {{ESP_GATT_AUTO_RSP}, {
        ESP_UUID_LEN_16, 
        (uint8_t *)&character_declaration_uuid, 
        ESP_GATT_PERM_READ,
        CHAR_DECLARATION_SIZE,
        CHAR_DECLARATION_SIZE, 
        (uint8_t *)&char_prop_notify}
    },

    // Echo Out Characteristic Value
    [ESS_IDX_OUT_VAL] = {{ESP_GATT_AUTO_RSP}, {
        sizeof(out_char_uuid128), 
        out_char_uuid128, 
        ESP_GATT_PERM_READ,
        sizeof(out_char_val),
        sizeof(out_char_val),
        out_char_val
    }},

    // Echo Out Characteristic - Client Characteristic Configuration Descriptor
    [ESS_IDX_OUT_NTF_CFG] = {{ESP_GATT_AUTO_RSP}, {
        ESP_UUID_LEN_16, 
        (uint8_t *)&character_client_config_uuid, 
        ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
        sizeof(uint16_t),
        sizeof(out_char_ccc),
        (uint8_t *)out_char_ccc
    }},

    // Echo IN Characteristic Declaration
    [ESS_IDX_IN_PT_CHAR] = {{ESP_GATT_AUTO_RSP}, {
        ESP_UUID_LEN_16, 
        (uint8_t *)&character_declaration_uuid, 
        ESP_GATT_PERM_READ,
        CHAR_DECLARATION_SIZE,
        CHAR_DECLARATION_SIZE, 
        (uint8_t *)&char_prop_read_write
    }},

    // Echo IN Characteristic Value
    [ESS_IDX_IN_PT_VAL] = {{ESP_GATT_AUTO_RSP}, {
        sizeof(in_char_uuid128), 
        in_char_uuid128, 
        ESP_GATT_PERM_WRITE|ESP_GATT_PERM_READ,
        sizeof(out_char_val),
        sizeof(out_char_val),
        out_char_val
    }},
};

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

#define PROFILE_NUM 1
#define PROFILE_ECHO_ID 0
static void gatts_profile_echo_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* All of the GATT profiles */
static struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_ECHO_ID] = {
        .gatts_cb = gatts_profile_echo_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Is set in the ESP_GATTS_REG_EVT event */
    },
};

/* Event handler for a particullar profile (The "Echo profile" in this case) */
static void gatts_profile_echo_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        //First event called, initalises advertising data
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);

            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(echo_gatt_db, gatts_if, ESS_IDX_NB, SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
            break;

        case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != ESS_IDX_NB){
                ESP_LOGE(TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to ESS_IDX_NB(%d)", param->add_attr_tab.num_handle, ESS_IDX_NB);
            }
            else {
                ESP_LOGI(TAG, "create attribute table successfully, the number handle = %d\n",param->add_attr_tab.num_handle);
                memcpy(echo_handle_table, param->add_attr_tab.handles, sizeof(echo_handle_table));
                esp_ble_gatts_start_service(echo_handle_table[ESS_IDX_SVC]);
            }
            break;
        }
        case ESP_GATTS_WRITE_EVT:

            break;
            
        default:
            break;
    }
}

// Primary GATT event handler, delegates to profile event handlers except for in the case of a ESP_GATTS_REG_EVT, which it does
// some of the work
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }

        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(DEVICE_NAME);
        if (set_dev_name_ret){
            ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }

        //config adv data
        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret){
            ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        }
        adv_config_finished |= adv_config_flag;
    }

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */

    int idx;
    for (idx = 0; idx < PROFILE_NUM; idx++) {
        if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gatts_if == gl_profile_tab[idx].gatts_if) {
            if (gl_profile_tab[idx].gatts_cb) {
                gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
            }
        }
    }

}

// Gap event handler, most of this is handled so dont really need to do much
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        //Called after advertising data is setup, begins advertising
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_finished &= (~adv_config_flag);
            if (adv_config_finished == 0){
                esp_ble_gap_start_advertising(&test_adv_params);
            }
            break;

        //advertising start complete event to indicate advertising start successfully or failed
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Advertising start failed\n");
            }
            break;

        default:
            break;
    }
}

// Init bluetooth LPE 
void init_bluetooth_LPE(esp_bt_controller_config_t *bt_cfg) {
    //Init the bluetooth controller
    esp_err_t ret;
    ret = esp_bt_controller_init(bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    //Enable lowpower mode
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
}

void app_main()
{
    esp_err_t ret;
    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    init_bluetooth_LPE(&bt_cfg);


    // Register GATT and GAP callback functions
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gap register error, error code = %x", ret);
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
        return;
    }

    // Register the profile
    ret = esp_ble_gatts_app_register(PROFILE_ECHO_ID);
    if (ret){
        ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
        return;
    }


    // Set the mtu for bluetooth LPE
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
}