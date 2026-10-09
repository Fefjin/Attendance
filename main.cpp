#include <DigiFi.h>
#include <rtc_clock.h>
#include "RFID.h"
#include <DigitalIO.h>

/*
  RFID pin setup.
  This RFID library uses software SPI pins from RFID.cpp:
  MISO = 28
  MOSI = 26
  SCK  = 24
  SS   = 22
  RST  = 32
*/
#define SS_PIN 22
#define RST_PIN 32

RFID rfid(SS_PIN, RST_PIN);

int const Buzzer = A0;

DigiFi wifi;
RTC_clock rtc_clock(XTAL);

/*
  NTP setup.
  NTP is only used once in setup before the web server starts.
*/
char timeServer[] = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;
uint8_t packetBuffer[NTP_PACKET_SIZE];

/*
  RTC time variables.
*/
int hh, mm, ss, dow, dd, mon, yyyy;

/*
  Cooldown prevents one card from being read twice immediately.
  5000 = 5 seconds.
*/
const unsigned long SCAN_COOLDOWN = 5000;
unsigned long lastScanMillis[19];

/*
  Hardcoded RFID UIDs.
*/
byte Name1[4]  = {0xDD, 0x30, 0x7D, 0x03};
byte Name2[4]  = {0x39, 0x91, 0x7D, 0x03};
byte Name3[4]  = {0x1D, 0x32, 0x7D, 0x03};
byte Name4[4]  = {0xB3, 0xA2, 0x7E, 0x03};
byte Name5[4]  = {0x14, 0xF5, 0x7D, 0x03};
byte Name6[4]  = {0x1C, 0xC2, 0x7D, 0x03};
byte Name7[4]  = {0xEE, 0x40, 0x7E, 0x03};
byte Name8[4]  = {0x3C, 0x1C, 0x7D, 0x03};
byte Name9[4]  = {0x19, 0x27, 0x7E, 0x03};
byte Name10[4] = {0xE4, 0x19, 0x7D, 0x03};
byte Name11[4] = {0x5C, 0xDB, 0x7D, 0x03};
byte Name12[4] = {0xCE, 0x05, 0x7D, 0x03};
byte Name13[4] = {0x34, 0xF8, 0x7D, 0x03};
byte Name14[4] = {0x8A, 0xF5, 0x7D, 0x03};
byte Name15[4] = {0x1D, 0xDD, 0x7C, 0x03};
byte Name16[4] = {0xE2, 0xA9, 0x7D, 0x03};
byte Name17[4] = {0x46, 0xDC, 0x7D, 0x03};
byte Name18[4] = {0x2C, 0xDB, 0x7E, 0x03};
byte Name19[4] = {0x9E, 0xB7, 0x7E, 0x03};

/*
  Student log structure.
  state:
    0 = no scan yet
    1 = logged in
    2 = logged in and out; future scans ignored
*/
struct StudentLog {
  String name;
  int number;
  byte* uid;
  int state;
  String inTime;
  String outTime;
};

/*
  Student roster.
*/
StudentLog students[] = {
  {"name", 0, Name1, 0, "NA", "NA"},
  {"name", 1, Name2, 0, "NA", "NA"},
  {"name", 2, Name3, 0, "NA", "NA"},
  {"name", 3, Name4, 0, "NA", "NA"},
  {"name", 4, Name5, 0, "NA", "NA"},
  {"name", 5, Name6, 0, "NA", "NA"},
  {"name", 6, Name7, 0, "NA", "NA"},
  {"name", 7, Name8, 0, "NA", "NA"},
  {"name", 8, Name9, 0, "NA", "NA"},
  {"name", 9, Name10, 0, "NA", "NA"},
  {"name", 10, Name11, 0, "NA", "NA"},
  {"name", 11, Name12, 0, "NA", "NA"},
  {"Gname", 12, Name13, 0, "NA", "NA"},
  {"name", 13, Name14, 0, "NA", "NA"},
  {"name", 14, Name15, 0, "NA", "NA"},
  {"name", 15, Name16, 0, "NA", "NA"},
  {"name", 16, Name17, 0, "NA", "NA"},
  {"name", 17, Name18, 0, "NA", "NA"},
  {"name", 18, Name19, 0, "NA", "NA"}
};

const int studentCount = sizeof(students) / sizeof(students[0]);

/*
  Connect DigiX WiFi module to your hotspot/router.
*/
void Connect2SSID() {
  wifi.startATMode();
  wifi.setWifiMode("");
  wifi.setWSSSID("");
  wifi.setSTAKey("WPA2PSK", "AES", "");
  wifi.reset();
  wifi.endATMode();
}

/*
  Compare two 4-byte RFID UIDs.
*/
bool compareArray(byte *a, byte *b) {
  for (byte i = 0; i < 4; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

/*
  Find a student by RFID UID.
*/
int findStudentIndex(byte *uid) {
  for (int i = 0; i < studentCount; i++) {
    if (compareArray(uid, students[i].uid)) {
      return i;
    }
  }
  return -1;
}

/*
  Buzzer helper.
*/
void buzzes(int x) {
  digitalWrite(Buzzer, HIGH);
  delay(x);
  digitalWrite(Buzzer, LOW);
}

/*
  Get current time from the RTC as a printable string.
*/
String getRTCTimeString() {
  rtc_clock.get_time(&hh, &mm, &ss);
  rtc_clock.get_date(&dow, &dd, &mon, &yyyy);

  String out = "";

  if (hh < 10) out += "0";
  out += String(hh);
  out += ":";

  if (mm < 10) out += "0";
  out += String(mm);
  out += ":";

  if (ss < 10) out += "0";
  out += String(ss);

  out += " ";

  if (mon < 10) out += "0";
  out += String(mon);
  out += "/";

  if (dd < 10) out += "0";
  out += String(dd);
  out += "/";

  out += String(yyyy);

  return out;
}

/*
  Build and send an NTP packet.
*/
void sendNTPpacket() {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;

  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  wifi.write(packetBuffer, NTP_PACKET_SIZE);
}

/*
  Read NTP response and convert it to Unix time.
*/
unsigned long getNTPpacket() {
  if (wifi.available()) {
    wifi.read(packetBuffer, NTP_PACKET_SIZE);

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    const unsigned long seventyYears = 2208988800UL;
    return secsSince1900 - seventyYears;
  }

  return 0;
}

/*
  Sync RTC using NTP.
  This runs before the server starts.
*/
bool syncTimeFromNTP() {
  Serial.println("Setting UDP mode for NTP");

  wifi.setMode(UDP);

  if (!wifi.connect(timeServer, 123)) {
    Serial.println("NTP connection failed");
    return false;
  }

  unsigned long ntpUnixTime = 0;

  for (int i = 0; i < 10 && ntpUnixTime == 0; i++) {
    sendNTPpacket();
    delay(1000);
    ntpUnixTime = getNTPpacket();
  }

  if (ntpUnixTime == 0) {
    Serial.println("NTP lookup failed");
    return false;
  }

  Serial.print("Got NTP timestamp: ");
  Serial.println(ntpUnixTime);

  rtc_clock.set_timestamp(ntpUnixTime);
  Serial.println("RTC clock set");

  return true;
}

/*
  RFID read handler.
  First scan = IN.
  Second scan = OUT.
  Third or later scan = ignored.
*/
void handleRFIDRead() {
  if (!rfid.isCard()) return;
  if (!rfid.readCardSerial()) return;

  Serial.print("Card found UID: ");
  for (int i = 0; i < 4; i++) {
    if (rfid.serNum[i] < 0x10) Serial.print("0");
    Serial.print(rfid.serNum[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  int index = findStudentIndex(rfid.serNum);

  if (index == -1) {
    Serial.println("Unknown card");
    buzzes(1000);
    rfid.halt();
    delay(500);
    return;
  }

  /*
    Ignore duplicate scans of the same card during cooldown.
    This prevents a held card from becoming IN and OUT immediately.
  */
  if (millis() - lastScanMillis[index] < SCAN_COOLDOWN) {
    Serial.print("Duplicate scan ignored for ");
    Serial.println(students[index].name);
    rfid.halt();
    delay(300);
    return;
  }

  lastScanMillis[index] = millis();

  String now = getRTCTimeString();

  if (students[index].state == 0) {
    students[index].inTime = now;
    students[index].state = 1;

    buzzes(150);

    Serial.print(students[index].name);
    Serial.print(" IN at ");
    Serial.println(now);
  }
  else if (students[index].state == 1) {
    students[index].outTime = now;
    students[index].state = 2;

    buzzes(500);

    Serial.print(students[index].name);
    Serial.print(" OUT at ");
    Serial.println(now);
  }
  else {
    Serial.print(students[index].name);
    Serial.println(" already logged in and out. Ignored.");
  }

  rfid.halt();
  delay(500);
}

/*
  Build the HTML page shown in the browser.
*/
String buildHTMLPage() {
  String page =
    "<!DOCTYPE html>"
    "<html><body>"
    "<h1>RFID Attendance Log</h1>"
    "<p>Current RTC Time: " + getRTCTimeString() + "</p>"
    "<p><a href='/download'>Download CSV for Excel</a></p>"
    "<table border='1' cellpadding='6'>"
    "<tr><th>#</th><th>Name</th><th>In Time</th><th>Out Time</th><th>Status</th></tr>";

  for (int i = 0; i < studentCount; i++) {
    String status = "Not Logged";
    if (students[i].state == 1) status = "In";
    if (students[i].state == 2) status = "Complete";

    page += "<tr>";
    page += "<td>" + String(students[i].number) + "</td>";
    page += "<td>" + students[i].name + "</td>";
    page += "<td>" + students[i].inTime + "</td>";
    page += "<td>" + students[i].outTime + "</td>";
    page += "<td>" + status + "</td>";
    page += "</tr>";
  }

  page += "</table></body></html>";
  return page;
}

/*
  Build CSV file for Excel.
*/
String buildCSV() {
  String csv = "Number,Name,In Time,Out Time,Status\n";

  for (int i = 0; i < studentCount; i++) {
    String status = "Not Logged";
    if (students[i].state == 1) status = "In";
    if (students[i].state == 2) status = "Complete";

    csv += String(students[i].number) + ",";
    csv += students[i].name + ",";
    csv += students[i].inTime + ",";
    csv += students[i].outTime + ",";
    csv += status + "\n";
  }

  return csv;
}

/*
  Send CSV as downloadable file.
*/
void sendCSVResponse(String csv) {
  wifi.print("HTTP/1.1 200 OK\r\n");
  wifi.print("Content-Type: text/csv\r\n");
  wifi.print("Content-Disposition: attachment; filename=attendance.csv\r\n");
  wifi.print("Content-Length: ");
  wifi.print(csv.length());
  wifi.print("\r\n");
  wifi.print("Connection: close\r\n\r\n");
  wifi.print(csv);
}

void setup() {
  Serial.begin(115200);

  pinMode(Buzzer, OUTPUT);

  /*
    Reset pin must stay HIGH for the RFID reader to work.
  */
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);

  rfid.init();
  Serial.println("RFID initialized");

  /*
    Initialize scan cooldown timestamps.
  */
  for (int i = 0; i < studentCount; i++) {
    lastScanMillis[i] = 0;
  }

  /*
    Initialize RTC.
    __TIME__ is only a fallback until NTP succeeds.
  */
  rtc_clock.init();
  rtc_clock.set_time(__TIME__);

  /*
    Connect WiFi and sync time before starting the server.
  */
  wifi.begin(115200);
  wifi.setDebug(true);

  Connect2SSID();

  syncTimeFromNTP();

  /*
    Return DigiFi to TCP mode and start the web server.
  */
  wifi.setMode(TCP);

  Serial.print("Starting server: ");
  Serial.println(wifi.server(8080));

  Serial.println("System is ready");
}

void loop() {
  /*
    Check RFID as often as possible.
  */
  handleRFIDRead();

  /*
    Serve browser requests.
  */
  if (wifi.serverRequest()) {
    String path = wifi.serverRequestPath();

    if (path.indexOf("/download") >= 0) {
      sendCSVResponse(buildCSV());
    }
    else {
      wifi.serverResponse(buildHTMLPage(), 200);
    }
  }
}
