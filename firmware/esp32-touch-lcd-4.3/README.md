# LifeTodo ESP32-S3-Touch-LCD-4.3 Firmware

This is the first hardware GUI prototype for the Waveshare `ESP32-S3-Touch-LCD-4.3`.

## Current Scope

- Initializes the 800x480 RGB LCD.
- Initializes GT911 capacitive touch over I2C.
- Renders a LifeTodo "today" screen with LVGL.
- Lets the user tap task cards to toggle done/pending.
- Shows Wi-Fi status when credentials are configured.
- Prints startup, heartbeat, touch coordinate, and task toggle diagnostics over the USB-UART port.

The first version uses local demo tasks so the screen and touch can be verified before the device API is added.

## Local Tooling

This project is set up for PlatformIO.

```bash
cd firmware/esp32-touch-lcd-4.3
pio run
pio run -t upload
pio device monitor
```

On macOS, the board should appear as a serial device such as:

```text
/dev/cu.usbserial-*
/dev/cu.usbmodem*
/dev/cu.wchusbserial*
```

The tested board appeared as:

```text
/dev/cu.usbmodem5B790235471
```

The board exposes a WCH USB-UART bridge, so firmware diagnostics use the hardware UART (`Serial0`) when USB CDC is enabled.

If no serial port appears, use the USB-UART Type-C port on the board. Waveshare notes that the board supports boot mode by holding `BOOT`, connecting USB, then releasing `BOOT`; press `RESET` after flashing.

## Verified Hardware Loop

On the connected board, the prototype has been verified to:

- Boot without reboot loops.
- Detect 8 MB PSRAM.
- Render the LVGL task screen after a short red/green/blue LCD diagnostic flash.
- Report GT911 touch coordinates.
- Toggle task cards and update the remaining count.

## Wi-Fi

Copy:

```text
include/app_config.example.h
```

to:

```text
include/app_config.h
```

Then fill:

```c
#define WIFI_SSID "..."
#define WIFI_PASSWORD "..."
```

Do not commit real Wi-Fi credentials.

## Next Step

Add the device data path:

```text
GET https://lifetodo.xyz/api/devices/today?home=demo-home&device=entry
POST https://lifetodo.xyz/api/devices/tasks/{taskId}/complete
```

The GUI is intentionally separated from networking so display and touch can be validated first.
