#include "gatt.h"

static uint8_t out_char_val[20] = {0x45};
static uint8_t out_char_ccc[2] = {0x00, 0x00};
#define MAX_DATA sizeof(out_char_val)

static const uint16_t primary_service_uuid         = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid   = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static uint16_t cur_conid = 0;

uint16_t echo_handle_table[BS_NB];

#define CHAR_DECLARATION_SIZE (sizeof(uint8_t))
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ;

/// Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t echo_gatt_db[BS_NB] =
{
    // Echo Service Declaration
    [BS_SVC] = {{ESP_GATT_AUTO_RSP}, {
        //This is a unique uuid that indicates this is a primary service
        ESP_UUID_LEN_16, 
        (uint8_t *)&primary_service_uuid, 
        ESP_GATT_PERM_READ,
        sizeof(button_service_uuid128), 
        sizeof(button_service_uuid128), 
        button_service_uuid128
    }},


    // Echo Out Characteristic Declaration
    [BA_CHAR] = {{ESP_GATT_AUTO_RSP}, {
        ESP_UUID_LEN_16, 
        (uint8_t *)&character_declaration_uuid, 
        ESP_GATT_PERM_READ,
        CHAR_DECLARATION_SIZE,
        CHAR_DECLARATION_SIZE, 
        (uint8_t *)&char_prop_notify}
    },

    // Echo Out Characteristic Value
    [BA_VAL] = {{ESP_GATT_AUTO_RSP}, {
        sizeof(button_char_uuid128), 
        button_char_uuid128, 
        ESP_GATT_PERM_READ,
        sizeof(out_char_val),
        sizeof(out_char_val),
        out_char_val
    }},

    // Echo Out Characteristic - Client Characteristic Configuration Descriptor
    [BA_CFG] = {{ESP_GATT_AUTO_RSP}, {
        ESP_UUID_LEN_16, 
        (uint8_t *)&character_client_config_uuid, 
        ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
        sizeof(uint16_t),
        sizeof(out_char_ccc),
        (uint8_t *)out_char_ccc
    }},
};

static void gatts_profile_echo_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/* All of the GATT profiles */
struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_ECHO_ID] = {
        .gatts_cb = gatts_profile_echo_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Is set in the ESP_GATTS_REG_EVT event */
    },
};

// esp_err_t esp_ble_gatts_set_attr_value(uint16_t attr_handle, uint16_t length, const uint8_t *value)
// esp_gatt_status_t esp_ble_gatts_get_attr_value(uint16_t attr_handle, uint16_t *length, const uint8_t **value)

static uint16_t get_button_ccc() {
    uint16_t len;
    const uint8_t val[MAX_DATA];
    esp_gatt_status_t err = esp_ble_gatts_get_attr_value(echo_handle_table[BA_CFG], &len, (const uint8_t**)&val);
    if (err) {
        ESP_LOGE(TAG, "Failed to get out descriptor, error code = %x", err);
    }
    if (len != 2) {
        ESP_LOGE(TAG, "Out descriptor is not 2 bytes, acctual size = %d", len);
    }
    return (val[1] << 8) | val[0];
}

static void get_button_val(uint16_t *len, const uint8_t **buf) {
    esp_gatt_status_t err = esp_ble_gatts_get_attr_value(echo_handle_table[BA_VAL], len, buf);
    if (err) {
        ESP_LOGE(TAG, "Failed to get in value, error code = %x", err);
    }
}

// Update the out value
static void update_button(esp_gatt_if_t gatts_if, uint16_t conn_id, uint16_t len, uint8_t *value) {
    esp_err_t err = esp_ble_gatts_set_attr_value(echo_handle_table[BA_VAL], len, value);
    if (err) {
        ESP_LOGE(TAG, "Failed to set charecteristic value, error code = %x", err);
    } else {
        ESP_LOGI(TAG, "Send update, value len = %d, value :", len);
        esp_log_buffer_hex(TAG, value, len);
        //last param idicates if confirmation is needed
        err = esp_ble_gatts_send_indicate(gatts_if, conn_id, echo_handle_table[BA_VAL], len, value, false);
        if (err) {
            ESP_LOGE(TAG, "Failed to send charecteristic, error code = %x", err);
        } 
    }
}

void set_button(int next) {
    uint8_t x = next;
    update_button(gl_profile_tab[PROFILE_ECHO_ID].gatts_if, cur_conid, 1, &next);
}

/* Event handler for a particullar profile (The "Echo profile" in this case) */
static void gatts_profile_echo_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        //First event called, initalises advertising data
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);

            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(echo_gatt_db, gatts_if, BS_NB, 0);
            if (create_attr_ret) {
                ESP_LOGE(TAG, "create attr table failed, error code = %x", create_attr_ret);
            }

            set_scan_params();

            break;

        case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != BS_NB){
                ESP_LOGE(TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to BS_NB(%d)", param->add_attr_tab.num_handle, BS_NB);
            }
            else {
                ESP_LOGI(TAG, "create attribute table successfully, the number handle = %d\n",param->add_attr_tab.num_handle);
                memcpy(echo_handle_table, param->add_attr_tab.handles, sizeof(echo_handle_table));
                esp_ble_gatts_start_service(echo_handle_table[BS_SVC]);
            }
            break;
        }
        case ESP_GATTS_CONNECT_EVT:
            cur_conid = param->connect.conn_id;
            esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            break;
        case ESP_GATTS_WRITE_EVT:
            if (!param->write.is_prep){
                // the data length of gattc write  must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
                ESP_LOGI(TAG, "GATT_WRITE_EVT, handle = %d, value len = %d, value :", param->write.handle, param->write.len);
                esp_log_buffer_hex(TAG, param->write.value, param->write.len);

                // Update the output if the input changes
                // if (echo_handle_table[ESS_IDX_IN_PT_VAL] == param->write.handle) {
                //     update_button(gatts_if, param->write.conn_id, param->write.len, param->write.value);
                // }

                /* send response when param->write.need_rsp is true*/
                if (param->write.need_rsp){
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                }
            }else{
                /* handle prepare write */
                //example_prepare_write_event_env(gatts_if, &prepare_write_env, param);
                ESP_LOGI(TAG, "Prepare write event");
            }
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
            set_scan_params();
            //esp_ble_gap_start_advertising(&adv_params);
            break;

            
        default:
            break;
    }
}

// Primary GATT event handler, delegates to profile event handlers except for in the case of a ESP_GATTS_REG_EVT, which it does
// some of the work
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
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
        // esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        // if (ret){
        //     ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        // }
        // adv_config_finished |= adv_config_flag;
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