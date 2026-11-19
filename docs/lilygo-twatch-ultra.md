# LilyGO T-Watch Ultra SX1262

The watch runs as a headless USB companion, BLE companion or repeater. The variant
is for the SX1262 model. AMOLED, touch, GNSS, vibration and watch UI are outside
this milestone.

The pinout and power configuration were checked against LilyGO's
[Ultra hardware guide](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/212d45ce3cc390f1ebb04885b780c77daeedd00f/docs/hardware/lilygo-t-watch-ultra.md)
and [Ultra board implementation](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/212d45ce3cc390f1ebb04885b780c77daeedd00f/src/LilyGoWatchUltra.cpp).

| Function | Configuration |
| --- | --- |
| MCU and memory | ESP32-S3, 16 MB QSPI flash, 8 MB QSPI PSRAM |
| LoRa SPI | MOSI 34, MISO 33, SCK 35, NSS 36 |
| LoRa control | RESET 47, BUSY 48, DIO1 14 |
| I2C | SDA 3, SCL 2 |
| Radio power | AXP2101 at 0x34, ALDO3 at 3.3 V |
| Internal antenna | XL9555 at 0x20, P13 high |
| Shared SPI devices | SD CS 21 and NFC CS 4 held high |
| GNSS | MIA-M10Q, left uninitialized |

The board enables and reads back the radio supply and antenna configuration
before initializing the SX1262. It retains existing critical rails and
charger settings, and preserves the temperature ADC configuration across
XPowersLib initialization. Battery voltage and USB power are read from the
PMU. Power, antenna and radio initialization failures are logged over serial.

The existing MeshCore RadioLib wrapper uses DIO2 for RF switching and powers
the TCXO at 1.6 V through DIO3, matching LilyGO's RadioLib initialization
defaults. All roles use native ESP32-S3 USB CDC for serial. The BLE companion
uses MeshCore's existing BLE interface. The repeater uses the ESP32 clock
because generic RTC probing could mistake the watch's PCF85063A for a PCF8563.
Automatic light sleep is deferred so idle USB, BLE and radio remain available.

## Local build

PlatformIO installs the pinned additional dependency `lewisxhe/XPowersLib@0.2.7`.
LilyGoLib is a hardware reference and is not needed for compilation.

```sh
pio run -e lilygo_twatch_ultra_companion_radio_usb
pio run -e lilygo_twatch_ultra_companion_radio_ble
pio run -e lilygo_twatch_ultra_repeater
```

The BLE companion has initial PIN `123456`. The repeater has the standard
initial administrator password `password`. Configure suitable regional radio
settings and change the initial credentials through the usual MeshCore interfaces.

`bash build-station.sh` clean-builds the seven station targets and stages all
outputs before exposing them in `release/`. The watch adds an application
image, manifest, merged image and two SHA-256 files per target. Existing
artifact names are retained. Publication remains an explicit action.

For first installation, use PlatformIO upload or flash the merged image at
`0x0`. The application-only image is for `0x10000` on a watch already
using the matching partition layout. The two application slots are each 4 MB.
Flashing the merged image writes the bootloader, partition table, initial OTA
metadata and application. Back up factory data if it needs to be retained.

Physical boot, PMU and PSRAM behavior, battery/USB power, charging, antenna
selection, LoRa RX/TX, USB framing, BLE and repeater operation require
validation on a physical SX1262 watch.
