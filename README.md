# PlantBot 🌱

An autonomous sun-tracking plant-care robot built with the XIAO ESP32-S3 Sense for the Interactive Systems course.

It moves a plant along a linear track to find the best light, identifies the plant with the onboard camera via the Plant.id API, and controls everything through a Telegram bot.

---

## What it does

- Scans a 30 cm track with an LDR to map light levels.
- Moves the plant to the brightest spot, or positions it based on the identified plant's light needs (full sun, partial shade, or shade).
- Takes photos and sends them to Telegram.
- Identifies plants and checks their health using the Plant.id API.
- Runs auto-tracking every 5 minutes.
- Exposes all functions through Telegram commands and inline buttons.

---

## Hardware

| Component | Purpose |
|-----------|---------|
| Seeed XIAO ESP32-S3 Sense | Main controller, camera, and WiFi |
| FS90R continuous-rotation servo | Moves the platform along the track |
| LDR + resistor | Light sensing |
| 30 cm track with rack and pinion | Linear travel |
| USB-C cable | Power and programming |

---

## Wiring

```
SERVO signal  → D0 (GPIO 1)
LDR signal    → D1 (GPIO 2) analog input
```

The camera uses the built-in XIAO ESP32-S3 Sense camera connector and pins defined in `xiao_complete_suntracker.ino`:

```
XCLK  → GPIO 10
SIOD  → GPIO 40
SIOC  → GPIO 39
D0-D7 → GPIO 15, 17, 18, 16, 14, 12, 11, 48
VSYNC → GPIO 38
HREF  → GPIO 47
PCLK  → GPIO 13
```

> **Note:** Power and ground for the servo and LDR must be supplied from a suitable 5 V source. The servo cannot be reliably powered from the XIAO's small regulator when moving under load.

---

## Features

- Light scan and ASCII bar chart across the track
- Brightest-spot tracking
- Auto-tracking mode (every 5 minutes)
- Plant identification with Plant.id
- Plant health check with Plant.id health assessment
- Auto-positioning based on identified plant's light preference
- Manual jogging (forward/backward by centimetres)
- Camera tuning commands (night exposure, white balance, brightness)
- Telegram inline keyboard for quick control

---

## Telegram Commands

### Movement

| Command | Description |
|---------|-------------|
| `/scan` | Scan track and report light values at each step |
| `/graph` | Build an ASCII bar chart of light across the track |
| `/track` | Find and move to the brightest spot |
| `/auto` | Enable auto-tracking every 5 minutes |
| `/stop` | Disable auto-tracking |
| `/home` | Move to the start (0 cm) |
| `/end` | Move to the end of the track |
| `/forward [cm]` | Jog forward (default 1 cm) |
| `/backward [cm]` | Jog backward (default 1 cm) |
| `/setlength <cm>` | Set the track length (1-100 cm) |
| `/status` | Show current position and light level |
| `/reset` | Reset position counter to 0 cm |

### Camera and plant care

| Command | Description |
|---------|-------------|
| `/photo` | Take a photo and send it to Telegram |
| `/identify` | Identify the plant with the camera |
| `/health` | Check plant health (diseases/pests) |
| `/care` | Identify the plant, then position it for its light need |
| `/autocare` | Run `/identify` followed by `/care` in one step |

### Camera tuning

| Command | Description |
|---------|-------------|
| `/night [0-1200]` | Manual long exposure for dim rooms |
| `/dayexp` | Return to auto exposure |
| `/setexp <-2..2>` | Set exposure level |
| `/setwb <0-4>` | Set white-balance mode (0 auto, 1 sunny, 2 cloudy, 3 office, 4 home) |

---

## Setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) or [Arduino CLI](https://arduino.github.io/arduino-cli/).
2. Add ESP32 board support (Espressif Systems).
3. Select **XIAO ESP32S3** as the board.
4. Install libraries via the Library Manager:
   - `ESP32Servo`
   - `UniversalTelegramBot`
   - `ArduinoJson`
   - `HTTPClient` (built into ESP32 core)
5. Copy `credentials_example.h` to `credentials.h`.
6. Fill in `credentials.h` with your WiFi, Telegram bot token, and Plant.id API key.
7. Open `xiao_complete_suntracker.ino` and upload.
8. Send `/start` or `/menu` to your Telegram bot.

---

## Credentials

Credentials are stored in `credentials.h`, which is excluded from this repo by `.gitignore`.

Use `credentials_example.h` as a template:

```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const char* botToken = "YOUR_TELEGRAM_BOT_TOKEN";
const char* chatID = "YOUR_TELEGRAM_CHAT_ID";
const char* plantIdApiKey = "YOUR_PLANT_ID_API_KEY";
```

Get a free Plant.id API key from [https://plant.id](https://plant.id).

---

## Calibration

**Servo**
- `SERVO_STOP` (default 95): the value where the servo stops moving.
- `SERVO_FWD` (0) and `SERVO_BWD` (180): full speed directions.
- `TOTAL_TIME_MS` (7060 ms): time to travel the full 30 cm track.

**Track**
- `TOTAL_DISTANCE_CM` (30.0 cm): default track length, adjustable at runtime with `/setlength`.
- `SCAN_STEPS` (10): number of points measured during a scan.

**LDR**
- The LDR is read on `LDR_PIN` (GPIO 2). If all readings are saturated and the spread is below 100, the LDR/resistor divider needs adjustment.

---

## How it works

1. On startup the camera is initialised and a few warm-up frames are discarded.
2. The servo is attached and the bot connects to WiFi.
3. A startup message with the command list is sent to Telegram.
4. In the main loop the bot polls Telegram every 1.5 seconds.
5. Auto-tracking, when enabled, runs `/track` every 5 minutes.
6. `/identify` sends a photo to Plant.id, stores the plant name, and classifies its light need.
7. `/care` scans the track and moves the plant to the position whose light level best matches the identified plant's preference.

---

## Files

| File | Description |
|------|-------------|
| `xiao_complete_suntracker.ino` | Main final code (use this one) |
| `credentials_example.h` | Template for WiFi/Telegram/Plant.id credentials |
| `xiao_suntracker_telegram.ino` | Simpler version: sun tracker + Telegram, no camera |
| `xiao_camera_servo_telegram.ino` | Camera + servo + Telegram, no plant care logic |
| `xiao_light_scanner.ino` | Light scanner test |
| `xiao_servo_calibrate.ino` | Servo stop/neutral calibration |
| `xiao_ldr_test.ino` | LDR reading test |

---

## Notes

- The onboard camera uses the XIAO ESP32-S3 Sense default pinout and `PIXFMT_JPEG`.
- If PSRAM is available the camera is set to `FRAMESIZE_UXGA`; otherwise it falls back to `FRAMESIZE_QVGA`.
- Image exposure and white balance can be tuned live with Telegram commands.
- The Plant.id `/identify` endpoint has a daily limit of 100 free requests.
