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
#include "esp_task_wdt.h"
#include "driver/gpio.h"

#include "failure_states.h"

static void blink_no_wifi_connection_forever(void*)
{
    gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = (1ULL<<GPIO_NUM_21);
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_21, 1);
    for (;;) {
        bool previous_state = false;
        for (int t = 0; t < 20; t++) {
            vTaskDelay(5 / portTICK_PERIOD_MS);
            gpio_set_level(GPIO_NUM_21, !previous_state);
            previous_state = !previous_state;
        }
        gpio_set_level(GPIO_NUM_21, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void blink_no_mqtt_connection_forever(void*)
{
    gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = (1ULL<<GPIO_NUM_21);
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_21, 1);
    for (;;) {
        bool previous_state = false;
        for (int t = 0; t < 20; t++) {
            vTaskDelay(20 / portTICK_PERIOD_MS);
            gpio_set_level(GPIO_NUM_21, !previous_state);
            previous_state = !previous_state;
        }
        gpio_set_level(GPIO_NUM_21, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void blink_no_wifi_connection_initialization()
{
    bool previous_state = false;
    for (int t = 0; t < 4; t++) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_21, !previous_state);
        previous_state = !previous_state;
    }
    gpio_set_level(GPIO_NUM_21, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void initiate_blink_no_wifi_connection_forever()
{
    xTaskCreatePinnedToCore (
        blink_no_wifi_connection_forever,     // Function to implement the task
        "blink_no_wifi_connection_forever",   // Name of the task
        4096,      // Stack size in bytes
        NULL,      // Task input parameter
        0,         // Priority of the task
        NULL,      // Task handle.
        0          // Core where the task should run
    );
}

void initiate_blink_no_mqtt_connection_forever()
{
    xTaskCreatePinnedToCore (
        blink_no_mqtt_connection_forever,     // Function to implement the task
        "blink_no_mqtt_connection_forever",   // Name of the task
        4096,      // Stack size in bytes
        NULL,      // Task input parameter
        0,         // Priority of the task
        NULL,      // Task handle.
        0          // Core where the task should run
    );
}
