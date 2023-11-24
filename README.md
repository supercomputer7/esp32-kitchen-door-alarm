# ESP32 Doorbell using WiFi+MQTT

This project was written with the goal of letting a user to use an ESP32
MCU board to send MQTT messages over WiFi when the button is pressed,
which can be chained with automation tools to do neat & cool stuff, only
limited by the imagination!

## How to build - Linux host guide

First, install the ESP32 toolchain on your host machine:

- [Standard Toolchain Setup for Linux and macOS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html#get-started-get-esp-idf)

Follow the guide from step 2 to step 5, so you basically have the full toolchain.
You should be able to invoke `get_idf` from your shell to continue with building.

### Wi-Fi & MQTT details

Run the `bootstrap.sh` script to generate the appropriate `mqtt_options.h` and `wifi_creds.h` files.
Then, change the details to match your application environment.

### Compile, flash and debug

You might need to adjust the unix permissions on the exposed device node
that is used for flashing the ESP32 device.
Then, run the following commands:
```sh
get_idf
idf.py build
idf.py -p YOUR_PORT flash # Don't forget to set YOUR_PORT to wherever Linux created the device node
```

If you want to debug the application, run:
```sh
idf.py monitor
```

## License

This project is licensed under the MIT license.
See `LICENSE` file for more details.
