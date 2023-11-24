/*
 * Copyright (c) 2023, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "driver/gpio.h"

#include "mqtt_options.h"
#include "wifi_creds.h"
#include "failure_states.h"
#include "utils.h"

#define EXIT_STATUS_NO_WIFI 1
#define EXIT_STATUS_NO_MQTT_CONNECTION_AUTH_FAILURE 2
#define EXIT_STATUS_NO_MQTT_CONNECTION_UNREACHEABLE_FAILURE 3

static void start_wifi_connection();
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data);

struct logic_state {
    bool never_wifi_connected;
    int32_t wifi_event_state_id;
    int32_t mqtt_event_state_id;
    int wifi_failure_counter;
    bool mqtt_connected;
    bool push_button_pressed;
    bool push_button_led_active;
};
static struct logic_state _internal_logic_state;
static esp_mqtt_client_handle_t _mqtt_client;

void initialize_button_gpio_state()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pin_bit_mask = (1ULL<<GPIO_NUM_34);
	io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	gpio_config(&io_conf);
}

void initialize_led_gpio_state()
{
    gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = ((1ULL<<GPIO_NUM_21) | (1ULL<<GPIO_NUM_19));
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_21, 1);
    gpio_set_level(GPIO_NUM_19, 1);
}

void initialize_current_logic_state()
{
    memset(&_internal_logic_state, 0, sizeof(_internal_logic_state));
    _internal_logic_state.wifi_failure_counter = 0;
    _internal_logic_state.wifi_event_state_id = WIFI_EVENT_STA_DISCONNECTED;
    _internal_logic_state.mqtt_event_state_id = MQTT_EVENT_DISCONNECTED;
    _internal_logic_state.never_wifi_connected = true;
    _internal_logic_state.mqtt_connected = false;
    _internal_logic_state.push_button_pressed = false;
    _mqtt_client = esp_mqtt_client_init(&app_mqtt_options_config);
    esp_mqtt_client_register_event(_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, _mqtt_client);
}

void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data)
{
    switch (event_id) {
    case WIFI_EVENT_STA_START:
        _internal_logic_state.wifi_event_state_id = WIFI_EVENT_STA_START;
        break;
    case WIFI_EVENT_STA_CONNECTED:
        _internal_logic_state.wifi_event_state_id = WIFI_EVENT_STA_CONNECTED;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        _internal_logic_state.wifi_event_state_id = WIFI_EVENT_STA_DISCONNECTED;
        break;
    case IP_EVENT_STA_GOT_IP:
        _internal_logic_state.wifi_event_state_id = IP_EVENT_STA_GOT_IP;
        break;
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        _internal_logic_state.mqtt_event_state_id = MQTT_EVENT_CONNECTED;
        break;
    case MQTT_EVENT_DISCONNECTED:
        _internal_logic_state.mqtt_event_state_id = MQTT_EVENT_DISCONNECTED;
        break;
    case MQTT_EVENT_PUBLISHED:
        // NOTE: Ignore this state silently, as we only care about connection or disconnection.
        break;
    
    // NOTE: We shall never be in these states (as we never subscribe) - so we should indicate the error!.
    case MQTT_EVENT_SUBSCRIBED:
        _internal_logic_state.mqtt_event_state_id = MQTT_EVENT_ERROR;
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        _internal_logic_state.mqtt_event_state_id = MQTT_EVENT_ERROR;
        break;
    case MQTT_EVENT_DATA:
        _internal_logic_state.mqtt_event_state_id = MQTT_EVENT_ERROR;
        break;
    case MQTT_EVENT_ERROR:
        _internal_logic_state.mqtt_event_state_id = MQTT_EVENT_ERROR;
        break;
    default:
        break;
    }
}

int async_main_logic()
{
    while (1) {
restart:
        gpio_set_level(GPIO_NUM_21, 1);
        if (_internal_logic_state.wifi_event_state_id == IP_EVENT_STA_GOT_IP && _internal_logic_state.mqtt_event_state_id == MQTT_EVENT_CONNECTED)
            goto has_mqtt_connection;
        // NOTE: In case we are actually connected and have IP, but MQTT is disconnected,
        // reconnect it!
        if (_internal_logic_state.wifi_event_state_id == IP_EVENT_STA_GOT_IP && _internal_logic_state.mqtt_event_state_id == MQTT_EVENT_DISCONNECTED)
            goto has_wifi_connection_initiate_mqtt;
        start_wifi_connection();
        {
            int attempts = 0;
            // NOTE: Wait 300 seconds (5 minutes) before giving up!
            while (attempts < 500) {
                if (_internal_logic_state.wifi_event_state_id == IP_EVENT_STA_GOT_IP) {
                    _internal_logic_state.never_wifi_connected = false;
                    goto has_wifi_connection_initiate_mqtt;
                }
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }

            // NOTE: If we never have been connected, exit now!
            if (_internal_logic_state.never_wifi_connected && _internal_logic_state.wifi_event_state_id != IP_EVENT_STA_GOT_IP)
                return EXIT_STATUS_NO_WIFI;
            // NOTE: If we have been connected, we might be able to connect again!
            // Therefore, just restart until we are connected.
            goto restart;
        }
has_wifi_connection_initiate_mqtt:
        printf("Connected to WiFi!\n");
        esp_mqtt_client_start(_mqtt_client);
        {
            int attempts = 0;
            // NOTE: Wait 30 seconds before giving up!
            while (attempts < 30) {
                if (_internal_logic_state.wifi_event_state_id != IP_EVENT_STA_GOT_IP)
                    goto restart;
                if (_internal_logic_state.mqtt_event_state_id == MQTT_EVENT_CONNECTED)
                    goto has_mqtt_connection;                    
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }

            // FIXME: Check why we are disconnected and return exit status accordingly
            if (_internal_logic_state.mqtt_event_state_id == MQTT_EVENT_DISCONNECTED)
                return EXIT_STATUS_NO_MQTT_CONNECTION_UNREACHEABLE_FAILURE;
        }
has_mqtt_connection:
        gpio_set_level(GPIO_NUM_21, 0);
        if (_internal_logic_state.mqtt_event_state_id == MQTT_EVENT_DISCONNECTED)
            goto has_wifi_connection_initiate_mqtt;
        bool temp_state = gpio_get_level(GPIO_NUM_34);
        if (_internal_logic_state.push_button_pressed != temp_state) {
            _internal_logic_state.push_button_pressed = temp_state;
            if(temp_state == false) {
                int dispatch_status = esp_mqtt_client_enqueue(_mqtt_client, mqtt_publish_topic, "HEYY", 0, 0, 0, true);
                // NOTE: Stop client on -1, which indicates possible network failure
                if (dispatch_status == -1) {
                    esp_mqtt_client_stop(_mqtt_client);
                    _internal_logic_state.mqtt_event_state_id = MQTT_EVENT_DISCONNECTED;
                    goto restart;
                }
                _internal_logic_state.push_button_led_active = true;
            }
 
        }
        vTaskDelay(40 / portTICK_PERIOD_MS);
        goto has_mqtt_connection;
    }
}

void initialize_wifi_connection_properties()
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",
        }
    };
    strcpy((char*)wifi_configuration.sta.ssid, app_wifi_ssid);
    strcpy((char*)wifi_configuration.sta.password, app_wifi_pass);    
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
}

void start_wifi_connection()
{
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_connect();
    printf("esp_wifi_connect invoked. SSID:%s - password:%s\n", app_wifi_ssid, app_wifi_pass);
}

void async_main(void*)
{
    int exit_status = async_main_logic();
    if (exit_status == EXIT_STATUS_NO_WIFI)
        initiate_blink_no_wifi_connection_forever();
    else if (exit_status == EXIT_STATUS_NO_MQTT_CONNECTION_AUTH_FAILURE || exit_status == EXIT_STATUS_NO_MQTT_CONNECTION_UNREACHEABLE_FAILURE)
        initiate_blink_no_mqtt_connection_forever();
    halt();
}

void async_led_active_push_button(void*)
{
    while (1) {
        if (_internal_logic_state.push_button_led_active) {
            gpio_set_level(GPIO_NUM_19, 0);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            gpio_set_level(GPIO_NUM_19, 1);
            _internal_logic_state.push_button_led_active = false;
        } 
    }
}

void run_main_logic()
{
    initialize_current_logic_state();
    initialize_button_gpio_state();
    initialize_led_gpio_state();
    initialize_wifi_connection_properties();

    xTaskCreatePinnedToCore (
        async_main,     // Function to implement the task
        "async_main",   // Name of the task
        4096,      // Stack size in bytes
        NULL,      // Task input parameter
        0,         // Priority of the task
        NULL,      // Task handle.
        0          // Core where the task should run
    );

    xTaskCreatePinnedToCore (
        async_led_active_push_button,     // Function to implement the task
        "async_led_active_push_button",   // Name of the task
        4096,      // Stack size in bytes
        NULL,      // Task input parameter
        0,         // Priority of the task
        NULL,      // Task handle.
        0          // Core where the task should run
    );
}

void app_main(void)
{
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is an %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("Silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
    fflush(stdout);

    nvs_flash_init();
    run_main_logic();
}
