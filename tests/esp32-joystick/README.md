# ESP32 Ethernet joystick test

Target: WT32-ETH01 / ESP32-ETH01 v1.4, assumed LAN8720 PHY address 1, MDC 23, MDIO 18, PHY power 16, GPIO0 external clock. Verify against the actual board before wiring.

| Signal | GPIO |
|---|---|
| X | 36 (SVP) |
| Y | 39 (SVN) |
| Z | 35 |
| Button | 32 (active low) |

Use 3.3 V analog signals and common ground. Button connects to ground.

Install PlatformIO. From this directory:

    pio run -e serial
    pio run -e serial -t upload
    pio device monitor

Connect Ethernet to a DHCP network. Serial prints the IP when acquired. Open http://<IP>/ in a browser. Polling is 20 Hz; ADC sampling is 100 Hz. GyverJoy processes 10-bit ADC data into -255..255. Settings and three zero offsets persist in NVS. Hold the joystick in neutral and press Set current position as zero. Dead zone is in ADC counts (total nominal width); curve is shared by all axes.

Subsequent OTA upload:

    pio run -e ota -t upload --upload-port <IP>

Alternatively use the configured joystick.local if mDNS resolves. OTA and HTTP have no authentication: use only on a trusted isolated test network.

Build and hardware operation have not yet been verified.
