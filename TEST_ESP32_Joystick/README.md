# Native ESP-IDF joystick test

Open this directory in PlatformIO. Build with `pio run`, first flash with `pio run -t upload` (manual bootloader mode as in ESP_CAM), monitor with `pio device monitor`.

GPIO36 / SVP = X, GPIO39 / SVN = Y, GPIO35 = Z, GPIO32 = active-low button to GND. Use 3.3 V signals and common ground. Verify pin availability on the physical board. Ethernet settings match ESP_CAM: LAN8720 address 1, power GPIO16, MDC23, MDIO18, external RMII clock GPIO0. DHCP, IP printed on UART.

Open http://<IP>/; three axes, raw ADC and center, button, curve and dead zone. Hold neutral and press Set current position as zero. Settings persist in NVS. ADC 12-bit, output -32767..32767. Dead zone is total width in ADC counts. HTTP polling every 50 ms.

OTA: select the application firmware.bin from ../.pio_core_root/build/TEST_ESP32_Joystick/esp32_eth01/ in the browser. Do not upload bootloader or merged flash image. Two OTA slots, 4 MB flash. No authentication: trusted isolated test network only. No automatic rollback configured.

Build and hardware not yet verified.
