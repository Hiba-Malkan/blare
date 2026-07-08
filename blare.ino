#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

const char* WIFI_SSID = ''; 
const char* WIFI_PASSWORD = ''; 
const char* NTP_SERVER = 'pool.ntp.org';
const long GMT_OFFSET_SEC = 4 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

#define TFT_CS 20
#define TFT_RST 10
#define TFT_DC 8
#define TFT_MOSI 7
#define TFT_SCLK 6
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCL, TFT_RST);
#define BTN_UP 0
#define BTN_DOWN 1
#define BTN_LEFT 2
#define BTN_RIGHT 3
#define BTN_OK 4
#define BTN_SNOOZE 5
#define BUZZER_PIN 21
const int SCREEN_W = 284;
const int SCREEN_H = 76;
const int COL_START = 82;
const int ROW_START = 18;
bool use24Hour = true; 
bool alarmEnabled = true;
bool alarmRinging = false;
bool snoozeActive = false;
bool menuOpen = false;
int alarmHour = 7;
int alarmMinute = 0;
unsigned long lastsecondtick = 0;
unsigned long lastDraw = 0; 
unsigned long lastBeep = 0;
unsigned long lastBeep= 0;
unsigned long snoozeEndsAt = 0;
int menuIndex = 0;

int currentYear = 2026;
int currentMonth = 1;
int currentDay = 1;
int currentHour = 0;
int currentMinute = 0;
int currentSecond = 0;
bool prevUp = false;
bool prevDown = false;
bool prevLeft = false;
bool prevRight = false;
bool prevOk = false;
bool prevSnooze = false;

String twoDigits(int v) {
    if (v < 10) {
        return '0' + String(v);
    } else {
        return String(v);
    }
}

bool pressed(int pin, bool &prevState) {
    bool now = (digitalRead(pin) == LOW);
    bool hit = now && !prevState;
    prevState = now;
    return hit;
}

void drawHeader() {
    tft.fillRect(0, 0, SCREEN_W, 16, ST77XX_BLACK);
    tft.setCursor(2,2);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.print(alarmEnabled ? 'Alarm ON' : 'Alarm OFF');
    tft.print(' ');
    tft.print(use24Hour ? '24H' : '12H');
}

void drawTime() {
    int showHour = currentHour;
    if (!use24Hour) {
        showHour = showHour % 12;
        if (showHour == 0) showHour = 12;   
    }
    String timeText = twoDigits(showHour) + ":" + twoDigits(currentMinute) + ":" + twoDigits(currentSecond);
    tft.fillRect(0, 16, SCREEN_W, 28, ST77XX_BLACK);
    tft.setCursor(2, 20);
    tft.setTextSize(3);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(timeText);
}

void drawDate() {
  String dateText = String(currentYear) + "-" + twoDigits(currentMonth) + "-" + twoDigits(currentDay);
  tft.fillRect(0, 46, SCREEN_W, 12, ST77XX_BLACK);
  tft.setCursor(2, 48);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.print(dateText);
}

void drawAlarmLine() {
  tft.fillRect(0, 58, SCREEN_W, 18, ST77XX_BLACK);
  tft.setCursor(2, 60);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.print("Alarm ");
  tft.print(twoDigits(alarmHour));
  tft.print(":");
  tft.print(twoDigits(alarmMinute));
  if (snoozeActive) tft.print("SNOOZE");
  else if (alarmRinging) tft.print("RING");
}

void drawMenu() {
  const char* items[] = {
    "Set alarm hour",
    "Set alarm minute",
    "Toggle 12/24",
    "Alarm on/off",
    "Exit menu"
  };
  tft.fillRect(120, 0, SCREEN_W - 120, SCREEN_H, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(122, 2);
  tft.setTextColor(ST77XX_MAGENTA, ST77XX_BLACK);
  tft.print("MENU");
  for (int i = 0; i < 5; i++) {
    int y = 14 + i * 11;
    if (i == menuIndex) {
      tft.fillRect(120, y - 1, 160, 10, ST77XX_WHITE);
      tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    } else {
      tft.fillRect(120, y - 1, 160, 10, ST77XX_BLACK);
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    }
    tft.setCursor(122, y);
    tft.print(items[i]);
  }
}

void redrawScreen() {
  tft.fillScreen(ST77XX_BLACK);
  drawHeader();
  drawTime();
  drawDate();
  drawAlarmLine();
  if (menuOpen) drawMenu();
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(2, 2);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("Connecting WiFi...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
}

void syncClock() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  struct tm timeInfo;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&timeInfo, 500)) {
      currentYear = timeInfo.tm_year + 1900;
      currentMonth = timeInfo.tm_mon + 1;
      currentDay = timeInfo.tm_mday;
      currentHour = timeInfo.tm_hour;
      currentMinute = timeInfo.tm_min;
      currentSecond = timeInfo.tm_sec;
      return;
    }
    delay(250);
  }
}

void tickClock() {
  currentSecond++;
  if (currentSecond >= 60) {
    currentSecond = 0;
    currentMinute++;
  }
  if (currentMinute >= 60) {
    currentMinute = 0;
    currentHour++;
  }
  if (currentHour >= 24) {
    currentHour = 0;
    currentDay++;
  }
}

bool alarmShouldRing() {
  return alarmEnabled &&
         !snoozeActive &&
         currentHour == alarmHour &&
         currentMinute == alarmMinute &&
         currentSecond == 0;
}

void startAlarm() {
  alarmRinging = true;
  lastBeep = 0;
}

void stopAlarm() {
  alarmRinging = false;
  noTone(BUZZER_PIN);
}

void snoozeAlarm() {
  snoozeActive = true;
  snoozeEndsAt = millis() + 5UL * 60UL * 1000UL;
  stopAlarm();
}

void beepAlarm() {
  if (millis() - lastBeep > 500) {
    lastBeep = millis();
    tone(BUZZER_PIN, 2200, 200);
  }
}

void handleOk() {
  if (!menuOpen) {
    menuOpen = true;
    menuIndex = 0;
    return;
  }
  switch (menuIndex) {
    case 0:
      alarmHour = (alarmHour + 1) % 24;
      break;
    case 1:
      alarmMinute = (alarmMinute + 1) % 60;
      break;
    case 2:
      use24Hour = !use24Hour;
      break;
    case 3:
      alarmEnabled = !alarmEnabled;
      if (!alarmEnabled) stopAlarm();
      break;
    case 4:
      menuOpen = false;
      break;
  }
}

void readButtons() {
  if (pressed(BTN_OK, prevOk)) handleOk();

  if (pressed(BTN_UP, prevUp)) {
    if (menuOpen) {
      menuIndex--;
      if (menuIndex < 0) menuIndex = 4;
    } else if (alarmRinging) {
      stopAlarm();
    }
  }

  if (pressed(BTN_DOWN, prevDown)) {
    if (menuOpen) {
      menuIndex++;
      if (menuIndex > 4) menuIndex = 0;
    }
  }

  if (pressed(BTN_LEFT, prevLeft)) {
    if (!menuOpen) use24Hour = !use24Hour;
  }

  if (pressed(BTN_RIGHT, prevRight)) {
    if (!menuOpen) {
      alarmEnabled = !alarmEnabled;
      if (!alarmEnabled) stopAlarm();
    }
  }

  if (pressed(BTN_SNOOZE, prevSnooze)) {
    if (alarmRinging) snoozeAlarm();
  }
}

void updateAlarmState() {
  if (snoozeActive && millis() >= snoozeEndsAt) snoozeActive = false;
  if (alarmShouldRing()) startAlarm();
  if (alarmRinging) beepAlarm();
}

void updateScreen() {
  if (millis() - lastDraw >= 1000) {
    lastDraw = millis();
    drawHeader();
    drawTime();
    drawDate();
    drawAlarmLine();
    if (menuOpen) drawMenu();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_SNOOZE, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  tft.init(SCREEN_W, SCREEN_H);
  tft.setColRowStart(COL_START, ROW_START);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  Serial.println("TFT Initialized!");
  connectWifi();
  syncClock();
  redrawScreen();
}

void loop() {
  readButtons();
  updateAlarmState();

  if (millis() - lastSecondTick >= 1000) {
    lastSecondTick = millis();
    tickClock();
  }
  updateScreen();

  if (menuOpen) {
    redrawScreen();
    delay(120);
  }
  delay(10);
}