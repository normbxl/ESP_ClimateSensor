# ESP Climate Sensor
ESP8266 and DHT11 low-power sensor node using Adafruit IO.
This one specifically uses the ESP-12E on a self made PCB making it compatible with NodeMCU-1.0.
Alternativly it has also been tested with an ESP-01 using GPIO 0.
For changing the board edit platformio.ini change the `board` setting
```
[env:esp01]
platform = espressif8266
;board = esp01
board = nodemcu
```

In file `main.cpp` change `DHTPIN`to the GPIO nummber connected to the DHT-11 data pin.

## WiFi configuration
There is no pre-defined wifi setting. If no connection setting is found in the EEPROM (not a real EEPROM, just an emulation) the credentials can
entered of the serial prompt.

Use `ssid=...` + <Enter> to set the SSID, `pwd=...` + <Enter> for the wifi key. Once these are set enter `save` + <Enter> to commit the settings.
