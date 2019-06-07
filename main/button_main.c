#include "button_main.h"


uint8_t adv_config_finished = 0;

//CF5A0415-76A0-4B41-A4B0-6C33C520F354
uint8_t button_service_uuid128[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0x54, 0xF3, 0x20, 0xC5, 0x33, 0x6C, 0xB0, 0xA4, 0x41, 0x4B, 0xA0, 0x76, 0x15, 0x04, 0x5A, 0xCF,
};

//CF5A0415-76A0-4B41-A4B0-6C33C520F354
uint8_t button_char_uuid128[16] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0x54, 0xF3, 0x55, 0xC5, 0x33, 0x6C, 0xB0, 0xA4, 0x41, 0x4B, 0xA0, 0x76, 0x15, 0x04, 0x5A, 0xCF,
};

const char remote_device_name[] = DEVICE_NAME;

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


static xQueueHandle evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t x = gpio_get_level(GPIO_NUM_32);
    xQueueSendFromISR(evt_queue, &x, NULL);
}

esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr        =
    //.peer_addr_type   =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


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

// Bluetooth security
esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;//set the IO capability to No Input No Output
esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND; //bonding with peer device after authentication
uint8_t key_size = 16;      //the key size should be 7~16 bytes
uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;


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

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
    

    gpio_config_t config = {
        .pin_bit_mask = GPIO_SEL_32,
        .mode = GPIO_MODE_DEF_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };

    ret = gpio_config(&config);
    if (ret){
        ESP_LOGE(TAG, "Couldnt configure IO pin = %x", ret);
        return;
    }

    evt_queue = xQueueCreate(10, sizeof(uint32_t));

    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_NUM_32, gpio_isr_handler, NULL);


    // int prev = 0;
    // int next = 0;
    // for (;;) {
    //     next = gpio_get_level(GPIO_NUM_32);
    //     if (prev != next) {
    //         set_button(!next);
    //     }
    //     prev = next;
    //     vTaskDelay(100 / portTICK_RATE_MS);
    // }

    uint32_t io_num;
    for (;;) {
        if (xQueueReceive(evt_queue, &io_num, portMAX_DELAY)) {
            // Buttons are inverted
            set_button(!io_num);
            ESP_LOGI(TAG, "Button set to %d", io_num);
        }
    }

}