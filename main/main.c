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
#include "driver/gpio.h"

#include "utils.h"

static bool button_pressed;

void initialize_button_gpio_state()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pin_bit_mask = (1ULL<<GPIO_NUM_5);
	io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	gpio_config(&io_conf);
}

void initialize_led_gpio_state()
{
    gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = (1ULL<<GPIO_NUM_4);
	io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_4, 0);
}

void async_button_handler(void*)
{
    while (1) {
        button_pressed = gpio_get_level(GPIO_NUM_5);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void async_led_active_push_button(void*)
{
    while (1) {
        if (!button_pressed) {
            vTaskDelay(30000 / portTICK_PERIOD_MS);
            while (!button_pressed) {pl9
                gpio_set_level(GPIO_NUM_4, 1);
                vTaskDelay(200 / portTICK_PERIOD_MS);
                gpio_set_level(GPIO_NUM_4, 0);
                vTaskDelay(200 / portTICK_PERIOD_MS);
            }
        }
    }
}

void run_main_logic()
{
    initialize_button_gpio_state();
    initialize_led_gpio_state();

    xTaskCreatePinnedToCore (
        async_led_active_push_button,     // Function to implement the task
        "async_led_active_push_button",   // Name of the task
        4096,      // Stack size in bytes
        NULL,      // Task input parameter
        0,         // Priority of the task
        NULL,      // Task handle.
        0          // Core where the task should run
    );

    xTaskCreatePinnedToCore (
        async_button_handler,     // Function to implement the task
        "async_button_handler",   // Name of the task
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

    button_pressed = false;

    nvs_flash_init();
    run_main_logic();
}
