#include "gap.h"

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

bool connect = false;

esp_ble_adv_data_t adv_data = {
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
    .service_uuid_len = sizeof(button_service_uuid128),
    .p_service_uuid = button_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ONLY_WLST,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

esp_err_t set_scan_params() {
    esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (scan_ret){
        ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
    }
    return scan_ret;
}

// Gap event handler, most of this is handled so dont really need to do much
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        //Called after advertising data is setup, begins advertising
        // case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        //     adv_config_finished &= (~adv_config_flag);
        //     if (adv_config_finished == 0){
        //         esp_ble_gap_start_advertising(&adv_params);
        //     }
        //     break;

        //advertising start complete event to indicate advertising start successfully or failed
        // case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            
        //     if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        //         ESP_LOGE(TAG, "Advertising start failed\n");
        //     }
        //     break;
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
            //the unit of the duration is second
            uint32_t duration = 30;
            esp_ble_gap_start_scanning(duration);
            break;
        }
    
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            //scan start complete event to indicate scan start successfully or failed
            if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
                break;
            }
            ESP_LOGI(TAG, "scan start success");
        break;


        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            uint8_t *adv_name = NULL;
            uint8_t adv_name_len = 0;
            
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            switch (scan_result->scan_rst.search_evt) {
        
                case ESP_GAP_SEARCH_INQ_RES_EVT:    
                    esp_log_buffer_hex(TAG, scan_result->scan_rst.bda, 6);
                    ESP_LOGI(TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
                    
                    adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                    ESP_LOGI(TAG, "searched Device Name Len %d", adv_name_len);
                    
                    esp_log_buffer_char(TAG, adv_name, adv_name_len);
                    ESP_LOGI(TAG, "\n");
                    
                    if (adv_name != NULL) {
                        if (strlen(remote_device_name) == adv_name_len && strncmp((char *)adv_name, remote_device_name, adv_name_len) == 0) {
                            ESP_LOGI(TAG, "searched device %s\n", remote_device_name);
                            if (connect == false) {
                                connect = true;
                                ESP_LOGI(TAG, "connect to the remote device.");
                                esp_ble_gap_stop_scanning();
                                // esp_ble_gattc_open(gl_profile_tab[PROFILE_ECHO_ID].gatts_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                                esp_ble_gatts_open(gl_profile_tab[PROFILE_ECHO_ID].gatts_if, scan_result->scan_rst.bda, true);
                            }
                        }
                    }
                    break;
                
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    break;
                
                default:
                    break;
            }
            break;
        }

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
                ESP_LOGE(TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
                break;
            }
            ESP_LOGI(TAG, "stop scan successfully");
            break;

        // case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        //     if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
        //         ESP_LOGE(TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
        //         break;
        //     }
        //     ESP_LOGI(TAG, "stop adv successfully");
        //     break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                      param->update_conn_params.status,
                      param->update_conn_params.min_int,
                      param->update_conn_params.max_int,
                      param->update_conn_params.conn_int,
                      param->update_conn_params.latency,
                      param->update_conn_params.timeout);
            break;

        default:
            break;
    }
}