# RFID Attendance System

An RFID-based attendance tracker built for the DigiX (Arduino-compatible) board with its onboard WiFi module. Students tap an RFID card on a reader to check in and check out. Attendance is timestamped using a real-time clock synced over the internet, and anyone on the same network can view the live log in a web browser or download it as a CSV file for Excel.

## Features

- **Tap in / tap out:** the first scan of a card logs the student IN, and the second scan logs them OUT. Any later scans are ignored.
- **Accurate timestamps:** the on-board RTC is synced once at startup from an NTP server (`time.nist.gov`).
- **Live web dashboard:** a built-in web server (port `8080`) shows each student's in time, out time, and status.
- **CSV export:** download the full log as `attendance.csv` from the dashboard with one click.
- **Audio feedback:** a buzzer confirms each scan (see [Buzzer Signals](#buzzer-signals)).
- **Duplicate scan protection:** a 5-second cooldown per card stops a held card from instantly registering as both IN and OUT.
- **Unknown card detection:** cards that are not on the roster trigger a long error buzz and are not logged.

## Hardware

| Component | Notes |
|---|---|
| DigiX board with onboard WiFi (DigiFi) | Main controller |
| RFID reader (RC522-style, SPI) | Uses software SPI, see wiring below |
| RFID cards/tags (4-byte UID) | One per student |
| Buzzer | Connected to `A0` |
| 2.4 GHz WiFi network or phone hotspot | Needs internet access for the NTP sync |

### Wiring

The RFID library uses software SPI with the pins defined in `RFID.cpp`:

| RFID Pin | DigiX Pin |
|---|---|
| MISO | 28 |
| MOSI | 26 |
| SCK | 24 |
| SS (SDA) | 22 |
| RST | 32 |
| Buzzer (+) | A0 |

> The RST pin is driven HIGH in `setup()` and must stay HIGH for the reader to work.

## Software Requirements

Install or add these libraries to your Arduino IDE before compiling:

- `DigiFi` (DigiX WiFi library)
- `rtc_clock` (RTC support for the Arduino Due-based DigiX)
- `RFID` (RFID reader library, including `RFID.cpp`/`RFID.h`)
- `DigitalIO`

## Setup

1. **Wire up the hardware** as shown above.
2. **Configure WiFi.** In `Connect2SSID()`, set your network name and password:

   ```cpp
   wifi.setWSSSID("YourNetworkName");
   wifi.setSTAKey("WPA2PSK", "AES", "YourPassword");
   ```

3. **Add your students and cards.** Each card's 4-byte UID is stored as a byte array, and each student is an entry in the `students[]` roster:

   ```cpp
   byte Name1[4] = {0xDD, 0x30, 0x7D, 0x03};

   StudentLog students[] = {
     {"StudentName", 0, Name1, 0, "NA", "NA"},
     ...
   };
   ```

   If you add more than 19 students, also increase the size of the `lastScanMillis[19]` array.

4. **Find a card's UID** by scanning it and reading the Serial Monitor output (`Card found UID: ...`). Unknown cards print their UID there, so you can copy it straight into the code.
5. **Upload the sketch** to the DigiX and open the Serial Monitor at **115200 baud**.
6. Wait for `System is ready` to appear.

## Usage

1. Power on the board. It connects to WiFi, syncs the clock, and starts the web server.
2. Students tap their card on the reader to check **in**, then tap again to check **out**.
3. To view the log, open a browser on the same network and go to:

   ```
   http://<board-ip-address>:8080
   ```

4. To export the data, click **Download CSV for Excel** on the page (or go to `http://<board-ip-address>:8080/download`).

### Attendance states

| State | Meaning | Status shown |
|---|---|---|
| `0` | No scan yet | Not Logged |
| `1` | Checked in | In |
| `2` | Checked in and out (further scans ignored) | Complete |

### Buzzer signals

| Event | Buzz length |
|---|---|
| Check in | Short (150 ms) |
| Check out | Medium (500 ms) |
| Unknown card | Long (1000 ms) |

## How It Works

1. **Startup:** the RFID reader, buzzer, and RTC are initialized. The RTC is first set from the compile time (`__TIME__`) as a fallback.
2. **Time sync:** the board joins WiFi, switches to UDP mode, and requests the current time from an NTP server. If it succeeds, the RTC is updated. This happens only once, before the server starts.
3. **Web server:** the board switches back to TCP mode and starts listening on port `8080`.
4. **Main loop:** the loop continuously checks for an RFID card, then checks for incoming browser requests. Requests to `/download` return the CSV file, and every other path returns the HTML attendance page.

## Sample CSV Output

```csv
Number,Name,In Time,Out Time,Status
0,StudentName,08:02:15 10/09/2026,15:30:41 10/09/2026,Complete
1,StudentName,08:05:03 10/09/2026,NA,In
2,StudentName,NA,NA,Not Logged
```

## Known Limitations

- **Data is stored in RAM only.** Resetting or powering off the board erases the log, so download the CSV before shutting down.
- **Time is in UTC.** NTP returns UTC, and no time zone offset is applied, so timestamps will not match local time. A fixed offset could be added to the NTP timestamp before calling `rtc_clock.set_timestamp()`.
- **One check-in and one check-out per student.** After the second scan, further scans are ignored until the board is reset.
- **Roster is hardcoded.** Adding or changing students requires editing and re-uploading the sketch.
- **Time is synced only at startup.** If the NTP sync fails, the RTC falls back to the compile time, which will be wrong.
- **WiFi credentials are stored in plain text in the sketch.** Do not commit real network passwords to a public repository.
- **No authentication on the web page.** Anyone on the same network can view the log and download the CSV.

## Possible Improvements

- Save logs to an SD card or send them to a cloud service so data survives a reset
- Add a time zone offset or daylight saving support
- Load the roster from a file instead of hardcoding it
- Add a reset button or web route to start a new session without re-uploading
- Add password protection to the web dashboard

## Project Info

Built as part of an internship project.

- **Author:** Fefjin

