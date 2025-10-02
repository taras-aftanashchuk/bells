# ESP32 HTTP Server Example

ESP32 web server example: control 2 LEDs from a web page hosted on the ESP32.

Use [Wokwi for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode) to simulate this project.

## Building

This is a [PlatformIO](https://platformio.org) project. To build it, [install PlatformIO](https://docs.platformio.org/en/latest/core/installation/index.html), and then run the following command:

```
pio run
```

## Simulating

To simulate this project, install [Wokwi for VS Code](https://marketplace.visualstudio.com/items?itemName=wokwi.wokwi-vscode). Open the project directory in Visual Studio Code, press **F1** and select "Wokwi: Start Simulator".

Once the simulation is running, open http://localhost:8180 in your web browser to interact with the simulated HTTP server.


## Debugging

Logging

```pio device monitor -p /dev/cu.usbserial-0001 -b 115200```


## Melody file 

For now supports up to 10 melodies

Example:

```
-1,8           // Set time duration in seconds for melody
1,0,0,0,500    // rele1, rele2, rele3, rele4, duration
0,0,0,0,500    // Turn off all relays and wait 500 ms
