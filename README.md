# ESP32 Kitchen Door Alarm

Have a kitchen door that you always forget to close? Are you tired of stumbling upon that door?
Well, this project might help you with not forgetting that door open :)

While I hardcoded the waiting-before-starting-alarm time to be 30 seconds (30000 miliseconds) of the door being open,
you can change this to whatever you need easily.

## How to build - Linux host guide

First, install the ESP32 toolchain on your host machine:

- [Standard Toolchain Setup for Linux and macOS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/linux-macos-setup.html#get-started-get-esp-idf)

Follow the guide from step 2 to step 5, so you basically have the full toolchain.
You should be able to invoke `get_idf` from your shell to continue with building.

### Hardware details

I used a reed switch as the pressing button, and an ESP32C6 module to implement this project.
I created a simple circuit that toggles a GPIO as an output which is connected to a 2N3904 transistor via a 220 ohm resistor.
That transistor controls the current from a 3.3V to an active buzzer.

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
