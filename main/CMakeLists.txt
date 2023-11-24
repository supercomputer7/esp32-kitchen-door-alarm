idf_component_register(SRCS "main.c" "failure_states.c"
                    INCLUDE_DIRS ""
                    PRIV_REQUIRES esp_driver_gpio nvs_flash spi_flash esp_wifi mqtt)
