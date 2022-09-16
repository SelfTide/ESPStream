#/bin/bash

arduino-cli.exe compile -b esp32:esp32:esp32 ./ESP32/
arduino-cli.exe upload -p $1 -b esp32:esp32:esp32 ./ESP32/
