#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <DHT.h>

// ─── Pin Definitions ─────────────────────────────────────────────
#define TFT_CS    10
#define TFT_RST   9
#define TFT_DC    8

#define DHT_PIN   2
#define DHT_TYPE  DHT11

#define JOY_BTN   3   // Joystick button (SW)
#define LED_PIN   4

// ─── Display & Sensor Init ───────────────────────────────────────
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHT_PIN, DHT_TYPE);

// ─── Vibrant Color Palette ───────────────────────────────────────
#define COL_BG         0x0841   // Deep navy-black
#define COL_CARD       0x1082   // Slightly lighter card bg
#define COL_ACCENT     0x07FF   // Cyan
#define COL_WARM       0xFD20   // Warm orange
#define COL_HOT        0xF800   // Red
#define COL_COOL       0x841F   // Blue
#define COL_WHITE      0xFFFF
#define COL_GRAY       0x8410
#define COL_FOCUS_BG   0x4008   // Deep purple-black
#define COL_FOCUS_ACC  0xF81F   // Magenta
#define COL_FOCUS_GLO  0xFBE0   // Yellow-gold
#define COL_GREEN      0x07E0

// ─── State ───────────────────────────────────────────────────────
bool focusMode      = false;
bool lastBtnState   = HIGH;
bool btnState       = HIGH;
unsigned long lastDebounce   = 0;
unsigned long lastSensorRead = 0;
unsigned long focusStartTime = 0;
unsigned long lastLedBlink   = 0;
bool ledState = false;

float temperature = 0;
float humidity    = 0;
bool firstDraw    = true;

// ─── Helper: 16-bit color from R,G,B (5,6,5) ────────────────────
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ─── Draw rounded rectangle (manual, since GFX lacks fillRoundRect fill on ST7735) ─
void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  tft.fillRect(x + r, y, w - 2 * r, h, color);
  tft.fillRect(x, y + r, r, h - 2 * r, color);
  tft.fillRect(x + w - r, y + r, r, h - 2 * r, color);
  tft.fillCircle(x + r,         y + r,         r, color);
  tft.fillCircle(x + w - r - 1, y + r,         r, color);
  tft.fillCircle(x + r,         y + h - r - 1, r, color);
  tft.fillCircle(x + w - r - 1, y + h - r - 1, r, color);
}

// ─── Draw a horizontal gradient bar ─────────────────────────────
void drawGradientBar(int16_t x, int16_t y, int16_t w, int16_t h,
                     uint16_t c1, uint16_t c2) {
  for (int i = 0; i < w; i++) {
    uint8_t r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
    uint8_t r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
    uint16_t r = r1 + (r2 - r1) * i / w;
    uint16_t g = g1 + (g2 - g1) * i / w;
    uint16_t b = b1 + (b2 - b1) * i / w;
    tft.drawFastVLine(x + i, y, h, (r << 11) | (g << 5) | b);
  }
}

// ─── Weather Screen ──────────────────────────────────────────────
void drawWeatherScreen(bool fullRedraw) {
  if (fullRedraw) {
    tft.fillScreen(COL_BG);

    // Top header bar
    drawGradientBar(0, 0, 128, 22, rgb(0, 60, 120), rgb(0, 160, 200));
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(2);
    tft.setCursor(8, 5);
    tft.print("WEATHER");

    // Small subtitle
    tft.setTextSize(1);
    tft.setTextColor(rgb(180, 240, 255));
    tft.setCursor(85, 9);
    tft.print("LIVE");

    // Decorative divider
    tft.drawFastHLine(0, 22, 128, COL_ACCENT);
    tft.drawFastHLine(0, 23, 128, rgb(0, 80, 120));

    // Temperature card background
    fillRoundRect(4, 30, 120, 60, 6, COL_CARD);
    tft.drawFastHLine(4,  30, 120, COL_WARM);
    tft.drawFastHLine(4,  89, 120, COL_WARM);
    tft.drawFastVLine(4,  30, 60,  COL_WARM);
    tft.drawFastVLine(123,30, 60,  COL_WARM);

    // Humidity card background
    fillRoundRect(4, 98, 120, 60, 6, COL_CARD);
    tft.drawFastHLine(4,   98, 120, COL_COOL);
    tft.drawFastHLine(4,  157, 120, COL_COOL);
    tft.drawFastVLine(4,   98, 60,  COL_COOL);
    tft.drawFastVLine(123, 98, 60,  COL_COOL);

    // Card labels
    tft.setTextSize(1);
    tft.setTextColor(COL_WARM);
    tft.setCursor(10, 36);
    tft.print("TEMPERATURE");

    tft.setTextColor(COL_COOL);
    tft.setCursor(10, 104);
    tft.print("HUMIDITY");

    // Bottom hint
    tft.setTextColor(COL_GRAY);
    tft.setTextSize(1);
    tft.setCursor(8, 162);
    tft.print("HOLD BTN > FOCUS");
  }

  // ── Update temperature value ──
  tft.fillRect(10, 50, 108, 32, COL_CARD);
  tft.setTextSize(3);
  uint16_t tempColor = (temperature > 35) ? COL_HOT :
                       (temperature < 20) ? COL_COOL : COL_WARM;
  tft.setTextColor(tempColor);
  tft.setCursor(12, 52);
  if (temperature < 10) tft.print(" ");
  tft.print((int)temperature);
  tft.setTextSize(2);
  tft.setCursor(72, 56);
  tft.print("\xF7""C");   // degree symbol

  // ── Update humidity value ──
  tft.fillRect(10, 118, 108, 32, COL_CARD);
  tft.setTextSize(3);
  tft.setTextColor(COL_ACCENT);
  tft.setCursor(12, 120);
  if (humidity < 10) tft.print(" ");
  tft.print((int)humidity);
  tft.setTextSize(2);
  tft.setCursor(72, 124);
  tft.print("%");
}

// ─── Focus Mode Screen ───────────────────────────────────────────
void drawFocusScreen(bool fullRedraw) {
  if (!fullRedraw) return;

  tft.fillScreen(COL_FOCUS_BG);

  // Pulsing ring decoration (static)
  tft.drawCircle(64, 80, 55, COL_FOCUS_ACC);
  tft.drawCircle(64, 80, 52, rgb(80, 0, 80));
  tft.drawCircle(64, 80, 48, COL_FOCUS_ACC);

  // Glow dot center
  tft.fillCircle(64, 80, 10, COL_FOCUS_GLO);
  tft.fillCircle(64, 80, 7,  COL_WHITE);

  // Header
  tft.setTextColor(COL_FOCUS_ACC);
  tft.setTextSize(2);
  tft.setCursor(14, 6);
  tft.print("FOCUS MODE");

  // Divider
  drawGradientBar(0, 24, 128, 2, COL_FOCUS_ACC, rgb(60, 0, 100));

  // "IN SESSION" label
  tft.setTextColor(COL_FOCUS_GLO);
  tft.setTextSize(1);
  tft.setCursor(38, 28);
  tft.print("IN SESSION");

  // Bottom instruction
  tft.setTextColor(rgb(180, 100, 200));
  tft.setTextSize(1);
  tft.setCursor(12, 148);
  tft.print("PRESS BTN TO EXIT");

  // Timer label
  tft.setTextColor(COL_GRAY);
  tft.setCursor(46, 112);
  tft.print("ELAPSED");
}

void updateFocusTimer() {
  unsigned long elapsed = (millis() - focusStartTime) / 1000;
  unsigned int minutes = elapsed / 60;
  unsigned int seconds = elapsed % 60;

  tft.fillRect(20, 122, 88, 20, COL_FOCUS_BG);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  tft.setCursor(24, 124);
  if (minutes < 10) tft.print("0");
  tft.print(minutes);
  tft.print(":");
  if (seconds < 10) tft.print("0");
  tft.print(seconds);
}

// ─── Setup ───────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  pinMode(JOY_BTN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  tft.initR(INITR_BLACKTAB);   // Use INITR_GREENTAB if colors look off
  tft.setRotation(0);
  tft.fillScreen(COL_BG);

  dht.begin();

  // Splash screen
  tft.fillScreen(COL_BG);
  drawGradientBar(0, 60, 128, 40, rgb(0, 60, 120), rgb(0, 160, 200));
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 70);
  tft.print("STARTING...");
  delay(1500);

  // First sensor read
  temperature = dht.readTemperature();
  humidity    = dht.readHumidity();
  if (isnan(temperature)) temperature = 0;
  if (isnan(humidity))    humidity    = 0;

  drawWeatherScreen(true);
  firstDraw = false;
}

// ─── Loop ────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── Button debounce ──
  bool reading = digitalRead(JOY_BTN);
  if (reading != lastBtnState) {
    lastDebounce = now;
  }
  if ((now - lastDebounce) > 50) {
    if (reading != btnState) {
      btnState = reading;
      if (btnState == LOW) {   // Button pressed (active LOW with INPUT_PULLUP)
        focusMode = !focusMode;
        if (focusMode) {
          focusStartTime = now;
          digitalWrite(LED_PIN, HIGH);
          drawFocusScreen(true);
        } else {
          digitalWrite(LED_PIN, LOW);
          drawWeatherScreen(true);
        }
      }
    }
  }
  lastBtnState = reading;

  // ── Weather mode updates ──
  if (!focusMode) {
    if (now - lastSensorRead > 3000) {   // Read sensor every 3 seconds
      lastSensorRead = now;
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      if (!isnan(t)) temperature = t;
      if (!isnan(h)) humidity    = h;
      drawWeatherScreen(false);   // Partial update (values only)
    }
  }

  // ── Focus mode: update timer every second ──
  if (focusMode) {
    static unsigned long lastTimerUpdate = 0;
    if (now - lastTimerUpdate > 1000) {
      lastTimerUpdate = now;
      updateFocusTimer();
    }
  }
}