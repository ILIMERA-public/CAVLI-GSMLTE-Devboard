# CAVLI GSM/LTE Devboard

[Türkçe](README.md) · Product pages: [CAVLI GSM/LTE Devboard](https://ilimera.com/en/urunler/gelistirme-kartlari/gsm-lte-devboard) ·
[CAVLI GSM/LTE Devboard with built-in GPS](https://ilimera.com/en/urunler/gelistirme-kartlari/gsm-lte-gps-devboard)

![CAVLI GSM/LTE Devboard](docs/images/gsm-lte-features-v2.webp)

A 38 × 41 mm development board that adds 4G LTE (Cat 1.bis) connectivity to your project over UART, built on the
**Cavli C16QS** module. The Nano SIM socket, LTE and GNSS antenna connectors and PWR/NET LEDs are on the board.
It works with any 3.3 V controller (STM32, ESP32, Arduino, PIC, nRF…) through AT commands.

This repository covers both versions:

| Version | Difference | Technical document (Turkish) |
| --- | --- | --- |
| **CAVLI GSM/LTE Devboard** | 4G LTE | [PDF](docs/CAVLI_GSMLTE_teknik_dokuman_v1.pdf) |
| **CAVLI GSM/LTE Devboard with built-in GPS** | 4G LTE + built-in GPS (GNSS) | [PDF](docs/CAVLI_GSMLTE_GPS_teknik_dokuman_v1.pdf) |

Everything except the GPS example (`05_GPS_Konum_Okuma`) applies to both versions.

## Specifications

| Feature | Value |
| --- | --- |
| Cellular module | Cavli C16QS, 4G LTE Cat 1.bis |
| Positioning | Built-in GPS (GNSS) — **GPS version only** |
| Interface | UART (TX, RX), 115200 baud 8N1 (factory default) |
| Control pins | RST (active-low), PKY (Power Key) |
| Power | 3V3 and GND; the 3V3 rail must supply at least **2 A** peak |
| Logic level | 3.3 V (**not 5 V tolerant**) |
| SIM | Nano SIM socket (on board) |
| Antennas | LTE (required) and GNSS, U.FL/IPEX |
| Indicators | PWR, NET LEDs |
| Size | 38 × 41 mm |

## Pins and wiring

| Board pin | Direction | Connect to |
| --- | --- | --- |
| GND | — | Controller GND (common ground) |
| 3V3 | Input | 3.3 V regulator capable of 2 A peaks |
| TX | Output | Controller **RX** |
| RX | Input | Controller **TX** |
| PKY | Input | A GPIO: modem power on/off pulse |
| RST | Input | (Optional) a GPIO: active-low hardware reset |

ESP32 DevKit wiring used in the examples:

```text
CAVLI TX  → ESP32 GPIO16      CAVLI PKY → ESP32 GPIO4
CAVLI RX  ← ESP32 GPIO17      CAVLI GND → ESP32 GND
CAVLI 3V3 ← external 3.3 V / 2 A regulator  (the ESP32 board's 3V3 pin is not enough)
```

Change pins in the **KULLANICI AYARLARI** (user settings) block at the top of each sketch. If the modem does not
answer `AT` at start-up, the examples send a 500 ms pulse on PKY; if it still does not start, swap the
`PKY_BASILI` / `PKY_SERBEST` values.

## Power and logic level

- **Power:** the modem draws high current peaks during LTE transfers. Feed 3V3 from a good regulator able to
  deliver at least **2 A** peak, otherwise the modem drops off the network or reboots.
- **3.3 V logic:** TX, RX, RST and PKY are **not** 5 V tolerant. With 5 V controllers (Arduino Uno/Mega) always
  add a **logic level shifter**.
- **Antennas:** attach the LTE antenna before powering the board. For positioning, connect an active GNSS antenna.

## Quick start

1. Insert the Nano SIM, attach the LTE antenna, wire the board to your controller.
2. Install the ESP32 package in the Arduino IDE and upload
   [`examples/01_AT_Komut_Terminali`](examples/01_AT_Komut_Terminali).
3. Open the Serial Monitor at **115200 baud, Both NL & CR** and try:

```text
AT            → OK
AT^SIMSWAP?   → 1           (if 0, send AT^SIMSWAP=1 to use the on-board Nano SIM socket)
AT+CPIN?      → +CPIN: READY
AT+CSQ        → +CSQ: 24,99 (10 or more is good)
AT+CEREG?     → +CEREG: 0,1 (registered)
```

4. Then move on to [`src/main.cpp`](src/main.cpp) (HTTP GET) or the other examples.

## Examples

| Example | What it does |
| --- | --- |
| [`src/main.cpp`](src/main.cpp) | **R&D reference example** (PlatformIO, ESP32-C3): powers the modem, waits for SIM and network, opens the data context and performs an HTTP GET |
| [01_AT_Komut_Terminali](examples/01_AT_Komut_Terminali) | AT command terminal from the Serial Monitor |
| [02_HTTP_POST_JSON](examples/02_HTTP_POST_JSON) | HTTP POST with a JSON body every 60 s |
| [03_SMS_Gonder_ve_Al](examples/03_SMS_Gonder_ve_Al) | Sends SMS; SMS commands from an authorised number switch an output |
| [04_MQTT_Yayinla_Abone_Ol](examples/04_MQTT_Yayinla_Abone_Ol) | Publishes telemetry and subscribes to a command topic with the modem's built-in MQTT client |
| [05_GPS_Konum_Okuma](examples/05_GPS_Konum_Okuma) | **GPS version:** reads position, speed and UTC time, prints a Google Maps link |

`src/main.cpp` is a PlatformIO project (`pio run -t upload`); adjust `MODEM_TX_PIN`, `MODEM_RX_PIN` and
`MODEM_PWRKEY_PIN` to your circuit. The sketches in `examples/` open directly in the Arduino IDE and need no
extra libraries. The code ports to any 3.3 V controller: only the serial port and pin definitions change.
Code comments are in Turkish with an English summary in each header.

AT command reference (Turkish, commands are universal): [docs/AT_KOMUTLARI.md](docs/AT_KOMUTLARI.md).

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| No reply to `AT` | Cross TX/RX, join GND, check the PKY pulse, baud must be 115200 |
| SIM not detected | Select the on-board Nano SIM socket with `AT^SIMSWAP=1` |
| `+CEREG: 0,2` for a long time | Check the LTE antenna and 4G coverage |
| Modem reboots while sending data | Weak 3V3 supply: use a regulator capable of 2 A peaks |
| Does not work with an Arduino Uno, or the board gets warm | 5 V logic! Add a level shifter on TX/RX |
| `HTTPSEND: FAIL` | Check the APN and `AT+CGACT?`; HTTPS needs a certificate |
| `AT+CGPS=1` → `ERROR` | GPS exists only on the **built-in GPS** version; check the GNSS antenna |

## Resources and support

- Manufacturer documents (C16QS AT Command Manual, Hardware Manual):
  [Cavli product and solution guides](https://www.cavliwireless.com/resources/product-and-solution-guides)
- General introduction to AT commands: [Cavli — An Introduction to Cellular AT Commands](https://www.cavliwireless.com/blog/nerdiest-of-things/an-introduction-to-cellular-at-commands)
- Documentation, updates and support: [ilimera.com](https://ilimera.com/en/urunler/gelistirme-kartlari/gsm-lte-devboard) · info@ilimera.com
- Open an **Issue** in this repository for bugs and suggestions.

Want HTTPS without writing AT commands? [İLİMERA LTE Bridge](https://ilimera.com/en/urunler/gelistirme-kartlari/lte-bridge)
drives the same module with one-line JSON requests.

CAVLI GSM/LTE Devboard is developed by İLİMERA Technology. Cavli and C16QS are trademarks of Cavli Inc.

## License

The example code is provided under the [MIT License](LICENSE); you are free to use it in your own products.
Technical documents and images are the property of İLİMERA Technology.
