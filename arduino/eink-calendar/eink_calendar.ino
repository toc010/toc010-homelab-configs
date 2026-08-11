// ============================================================
//  ESP32 E-Ink Desk Calendar
//  Hardware: Adafruit HUZZAH32 + Waveshare 4.2" B/W (400x300)
//  Display driver: GxEPD2 (ZinggJM)
//  Quote source: Hardcoded Basho haiku, selected by day-of-year
// ============================================================

#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansOblique9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <math.h>

// ===== EDIT THESE TWO LINES =====
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ===== UTC offset =====
// Eastern Standard Time (Nov-Mar): -18000
// Eastern Daylight Time (Mar-Nov): -14400
const long utcOffsetSeconds = -14400;

// ===== Display: Waveshare 4.2" B/W V2 on HUZZAH32 =====
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT>
  display(GxEPD2_420_GDEY042T81(/*CS*/15, /*DC*/33, /*RST*/27, /*BUSY*/32));

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetSeconds);

// ============================================================
//  Calendar helpers
// ============================================================
const char* dayNames[] = {
  "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};
const char* monthNames[] = {
  "January","February","March","April","May","June",
  "July","August","September","October","November","December"
};
const int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};

bool isLeap(int y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}
int daysInMon(int m, int y) {
  return (m == 2 && isLeap(y)) ? 29 : daysInMonth[m - 1];
}
int firstDayOfMonth(int m, int y) {
  if (m < 3) { m += 12; y--; }
  int k = y % 100, j = y / 100;
  return ((1 + (13*(m+1)/5) + k + k/4 + j/4 + 5*j) % 7 + 6) % 7;
}

// ============================================================
//  Battery monitoring
//  HUZZAH32 exposes a resistor-divided (÷2) battery voltage on
//  pin A13 (GPIO35). Reference is 3.3V over a 12-bit (0-4095) ADC.
// ============================================================
#define VBAT_PIN A13

float readBatteryVoltage() {
  // Average a handful of samples -- the ESP32 ADC is noisy.
  const int SAMPLES = 8;
  uint32_t total = 0;
  for (int i = 0; i < SAMPLES; i++) {
    total += analogRead(VBAT_PIN);
    delay(2);
  }
  float raw = (float)total / SAMPLES;
  return raw * 2.0f * 3.3f / 4096.0f;
}

// Rough LiPo discharge curve (rest voltage -> % remaining).
// Voltage-based fuel gauges are only approximate -- no current
// sensing/coulomb counting here -- but this tracks the real
// curve far better than a straight 3.3-4.2V linear map.
int batteryPercentFromVoltage(float v) {
  static const float VOLTS[]   = {4.20, 4.06, 3.98, 3.92, 3.87, 3.82, 3.79, 3.77, 3.74, 3.68, 3.45, 3.00};
  static const int   PERCENT[] = { 100,   90,   80,   70,   60,   50,   40,   30,   20,   10,    5,    0};
  const int N = 12;

  if (v >= VOLTS[0]) return 100;
  if (v <= VOLTS[N - 1]) return 0;

  for (int i = 0; i < N - 1; i++) {
    if (v <= VOLTS[i] && v >= VOLTS[i + 1]) {
      float span = VOLTS[i] - VOLTS[i + 1];
      float frac = (v - VOLTS[i + 1]) / span;
      return PERCENT[i + 1] + (int)round(frac * (PERCENT[i] - PERCENT[i + 1]));
    }
  }
  return 0;
}

int readBatteryPercent() {
  float v = readBatteryVoltage();
  int pct = batteryPercentFromVoltage(v);
  Serial.printf("Battery: %.2fV -> %d%%\n", v, pct);
  return pct;
}

// Draws a small battery glyph (outline + terminal nub + fill level).
// x, y = top-left of the glyph body (nub excluded).
void drawBatteryIcon(int x, int y, int w, int h, int pct) {
  const int NUB_W = 3;
  const int NUB_H = h / 2;

  display.drawRect(x, y, w, h, GxEPD_BLACK);
  display.fillRect(x + w, y + (h - NUB_H) / 2, NUB_W, NUB_H, GxEPD_BLACK);

  const int PAD = 2;
  int innerW = w - PAD * 2;
  int innerH = h - PAD * 2;
  int fillW  = (innerW * pct) / 100;
  if (pct > 0 && fillW < 1) fillW = 1;   // keep a sliver visible if >0%
  if (fillW > 0) {
    display.fillRect(x + PAD, y + PAD, fillW, innerH, GxEPD_BLACK);
  }
}

// ============================================================
//  Pre-wrapped haiku lines
//
//  All 30 haiku pre-wrapped at 22 chars to fit FreeSansOblique9pt7b
//  in the 194px right column. This replaces runtime pixel-measurement
//  wrapping which was causing lines to be silently dropped.
//
//  Each entry has 6 line slots (max needed is 6 for haiku[07]).
//  Empty string "" = unused slot. haikuLineCount[] holds actual count.
// ============================================================
static const char haikuLines[30][6][24] = {
  { // [00] An old silent pond
    "An old silent pond...",
    "A frog jumps into the",
    "pond.",
    "Splash! Silence again.",
    "",
    "",
  },
  { // [01] Over the wintry
    "Over the wintry",
    "forest, winds howl in",
    "rage",
    "with no leaves to",
    "blow.",
    "",
  },
  { // [02] In the twilight rain
    "In the twilight rain",
    "these brilliant-hued",
    "hibiscus--",
    "a lovely sunset.",
    "",
    "",
  },
  { // [03] A lightning gleam
    "A lightning gleam:",
    "into darkness travels",
    "a night heron's",
    "scream.",
    "",
    "",
  },
  { // [04] Won't you come and see
    "Won't you come and see",
    "loneliness? Just one",
    "leaf",
    "from the kiri tree.",
    "",
    "",
  },
  { // [05] Temple bells die out
    "Temple bells die out.",
    "The fragrant blossoms",
    "remain.",
    "A perfect evening!",
    "",
    "",
  },
  { // [06] The first cold shower
    "The first cold shower.",
    "Even the monkey seems",
    "to want",
    "a little coat of",
    "straw.",
    "",
  },
  { // [07] Clouds come from time — LONGEST (6 lines)
    "Clouds come from time",
    "to time",
    "and bring to men a",
    "chance to rest",
    "from looking at the",
    "moon.",
  },
  { // [08] A caterpillar
    "A caterpillar,",
    "this deep in fall--",
    "still not a butterfly.",
    "",
    "",
    "",
  },
  { // [09] The oak tree stands
    "The oak tree stands",
    "nobly on the hill,",
    "indifferent",
    "to the cherry",
    "blossoms.",
    "",
  },
  { // [10] Nothing in the cry
    "Nothing in the cry",
    "of cicadas suggests",
    "they",
    "are about to die.",
    "",
    "",
  },
  { // [11] Poverty's child
    "Poverty's child--",
    "he starts to grind the",
    "rice,",
    "and gazes at the moon.",
    "",
    "",
  },
  { // [12] In all the rains of May
    "In all the rains of",
    "May",
    "there is one thing not",
    "hidden--",
    "the bridge at Seta.",
    "",
  },
  { // [13] Sick on my journey
    "Sick on my journey,",
    "only my dreams will",
    "wander",
    "these desolate moors.",
    "",
    "",
  },
  { // [14] The winds that blow
    "The winds that blow--",
    "ask them which leaf on",
    "the tree",
    "will be next to go.",
    "",
    "",
  },
  { // [15] From all these trees
    "From all these trees--",
    "in the salads, the",
    "soup, everywhere--",
    "cherry blossoms fall.",
    "",
    "",
  },
  { // [16] A bee staggers out
    "A bee staggers out",
    "of the peony.",
    "",
    "",
    "",
    "",
  },
  { // [17] The old pond
    "The old pond--",
    "a frog jumps in,",
    "sound of water.",
    "",
    "",
    "",
  },
  { // [18] Awakened at midnight
    "Awakened at midnight",
    "by the sound of the",
    "water jar",
    "cracking from the ice.",
    "",
    "",
  },
  { // [19] Over the sea-waves
    "Over the sea-waves,",
    "wildly blowing all day",
    "long,",
    "the autumn storm",
    "comes.",
    "",
  },
  { // [20] How admirable
    "How admirable,",
    "he who thinks not life",
    "is fleeting",
    "when he sees the",
    "lightning.",
    "",
  },
  { // [21] Harvest moon
    "Harvest moon:",
    "around the pond I",
    "wander",
    "and the night is gone.",
    "",
    "",
  },
  { // [22] On the one-ton temple bell
    "On the one-ton temple",
    "bell",
    "a moon-moth, folded",
    "into sleep,",
    "sits still.",
    "",
  },
  { // [23] The sun of spring
    "The sun of spring--",
    "a small man walking",
    "across a large moor.",
    "",
    "",
    "",
  },
  { // [24] Wrapping dumplings
    "Wrapping dumplings in",
    "bamboo leaves, with",
    "one finger",
    "she tidies her hair.",
    "",
    "",
  },
  { // [25] Along the roadside
    "Along the roadside,",
    "blossoming wild",
    "roses--",
    "my horse eats them.",
    "",
    "",
  },
  { // [26] With bland serenity
    "With bland serenity",
    "gazing at the far",
    "mountains:",
    "a tiny skylark.",
    "",
    "",
  },
  { // [27] In my new clothing
    "In my new clothing",
    "I feel so different, I",
    "must",
    "look like someone",
    "else.",
    "",
  },
  { // [28] Moonless night
    "Moonless night--",
    "a powerful wind",
    "embraces",
    "the ancient cedars.",
    "",
    "",
  },
  { // [29] Summer grasses
    "Summer grasses--",
    "all that remains",
    "of soldiers' dreams.",
    "",
    "",
    "",
  },
};

static const int haikuLineCount[30] = {
  4, 5, 4, 4, 4, 4, 5, 6, 3, 5, 4, 4, 5, 4, 4, 4, 2, 3, 4, 5, 5, 4, 5, 3, 4, 4, 4, 5, 4, 3
};

int dailyHaikuIndex(int mon, int day) {
  const int daysPerMonth[] = {0,31,59,90,120,151,181,212,243,273,304,334};
  int doy = daysPerMonth[mon - 1] + day;
  return (doy - 1) % 30;
}

// ============================================================
//  drawDisplay
// ============================================================
void drawDisplay(int dow, int mon, int day, int yr, int haikuIdx, int battPct) {

  const int W       = 400;
  const int H       = 300;
  const int COL_DIV = 200;
  const int ROW1_H  = 100;

  // Right column boundaries
  const int RC_X0 = COL_DIV + 3;
  const int RC_XR = W - 3;
  const int RC_Y0 = 3;
  const int RC_Y1 = ROW1_H - 3;   // hard ceiling y=97

  // Haiku geometry — fixed constants for FreeSansOblique9pt7b
  // Ascent ~11px, descent ~3px, so full line height = 14px
  // Attribution (FreeSans9pt7b): ascent ~10px
  const int LINE_ASCENT  = 11;   // pixels above baseline
  const int LINE_H       = 14;   // full line height including descent
  const int ATTR_ASCENT  = 10;
  const int ATTR_MARGIN  = 2;    // gap above attribution baseline

  // Attribution baseline is pinned at RC_Y1
  // Haiku zone: RC_Y0 to (RC_Y1 - ATTR_ASCENT - ATTR_MARGIN)
  const int HAIKU_ZONE_BOTTOM = RC_Y1 - ATTR_ASCENT - ATTR_MARGIN;
  const int HAIKU_ZONE_H      = HAIKU_ZONE_BOTTOM - RC_Y0;  // ~74px

  int wCount = haikuLineCount[haikuIdx];

  // Compute lineH: spread lines if space allows, compress if needed
  int lineH;
  if (wCount <= 1) {
    lineH = LINE_H;
  } else {
    // Block height = (wCount-1)*lineH + LINE_ASCENT
    // Solve for lineH that fills HAIKU_ZONE_H:
    //   lineH = (HAIKU_ZONE_H - LINE_ASCENT) / (wCount - 1)
    lineH = (HAIKU_ZONE_H - LINE_ASCENT) / (wCount - 1);
    if (lineH > LINE_H + 4) lineH = LINE_H + 4;  // cap: don't over-spread
    if (lineH < LINE_H - 2) lineH = LINE_H - 2;  // floor: don't crush
  }

  // Vertically centre the block in HAIKU_ZONE_H
  int blockH  = (wCount == 1) ? LINE_ASCENT : (wCount - 1) * lineH + LINE_ASCENT;
  int topPad  = (HAIKU_ZONE_H - blockH) / 2;
  if (topPad < 0) topPad = 0;
  int startY  = RC_Y0 + topPad + LINE_ASCENT;

  const int RCX = COL_DIV + (W - COL_DIV) / 2;  // x=300, right col centre

  display.setFullWindow();
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    // ----------------------------------------------------------
    //  Borders & dividers
    // ----------------------------------------------------------
    display.drawRect(0, 0, W, H, GxEPD_BLACK);
    display.drawFastVLine(COL_DIV, 0, ROW1_H, GxEPD_BLACK);
    display.drawFastHLine(0, ROW1_H, W, GxEPD_BLACK);

    // ----------------------------------------------------------
    //  ROW 1 LEFT
    //  Top half  y=0..50:    Day name      FreeSansBold18pt7b
    //  Divider at y=50
    //  Bottom half y=50..99: Numeric date  FreeSansBold18pt7b
    // ----------------------------------------------------------
    int16_t bx, by; uint16_t bw, bh;

    display.setFont(&FreeSansBold18pt7b);
    display.getTextBounds("Ag", 0, 0, &bx, &by, &bw, &bh);
    int fh18 = (int)bh;

    display.getTextBounds(dayNames[dow], 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((COL_DIV - (int)bw) / 2 - bx, (50 + fh18) / 2);
    display.print(dayNames[dow]);

    display.drawFastHLine(4, 50, COL_DIV - 8, GxEPD_BLACK);

    // Bottom-left row (y=50..99) is split into two zones:
    //   left  (x=0..100)   -> numeric date
    //   right (x=100..200) -> battery icon + numeric %
    // separated by a vertical divider.
    const int BOTTOM_DIV_X = 100;
    display.drawFastVLine(BOTTOM_DIV_X, 51, ROW1_H - 51, GxEPD_BLACK);

    char numDate[12];
    snprintf(numDate, sizeof(numDate), "%d/%d/%02d", mon, day, yr % 100);
    display.setFont(&FreeSansBold18pt7b);
    display.getTextBounds(numDate, 0, 0, &bx, &by, &bw, &bh);

    const int DATE_ZONE_W = BOTTOM_DIV_X - 8;  // leave margin before divider
    if ((int)bw > DATE_ZONE_W) {
      // Falls back to a smaller weight if the date is too wide
      // for the now-halved column (e.g. "12/31/26").
      display.setFont(&FreeSansBold12pt7b);
      display.getTextBounds(numDate, 0, 0, &bx, &by, &bw, &bh);
      display.setCursor((BOTTOM_DIV_X - (int)bw) / 2 - bx, 75 + fh18 / 4);
    } else {
      display.setCursor((BOTTOM_DIV_X - (int)bw) / 2 - bx, 75 + fh18 / 2);
    }
    display.print(numDate);

    // Battery indicator, centred in the right zone (x=100..200)
    {
      const int ICON_W = 26, ICON_H = 14;
      char pctStr[6];
      snprintf(pctStr, sizeof(pctStr), "%d%%", battPct);

      display.setFont(&FreeSansBold12pt7b);
      int16_t px, py; uint16_t pw, ph;
      display.getTextBounds(pctStr, 0, 0, &px, &py, &pw, &ph);

      const int GAP = 6;
      int groupW = ICON_W + GAP + (int)pw;
      int zoneCX = BOTTOM_DIV_X + (COL_DIV - BOTTOM_DIV_X) / 2;
      int groupX = zoneCX - groupW / 2;
      int rowCY  = 50 + (ROW1_H - 50) / 2;  // vertical centre of bottom row

      int iconX = groupX;
      int iconY = rowCY - ICON_H / 2;
      drawBatteryIcon(iconX, iconY, ICON_W, ICON_H, battPct);

      int textX = iconX + ICON_W + GAP - px;
      int textY = rowCY + (int)ph / 2;
      display.setCursor(textX, textY);
      display.print(pctStr);
    }

    // ----------------------------------------------------------
    //  ROW 1 RIGHT — haiku (italic, centred) + attribution
    // ----------------------------------------------------------

    // Draw haiku lines
    display.setFont(&FreeSansOblique9pt7b);
    for (int i = 0; i < wCount; i++) {
      if (haikuLines[haikuIdx][i][0] == '\0') continue;
      int curY = startY + i * lineH;
      if (curY > HAIKU_ZONE_BOTTOM) break;  // hard ceiling

      int16_t tx, ty; uint16_t tw, th;
      display.getTextBounds(haikuLines[haikuIdx][i], 0, 0, &tx, &ty, &tw, &th);
      int cx = RCX - (int)tw / 2 - tx;
      if (cx < RC_X0) cx = RC_X0;
      display.setCursor(cx, curY);
      display.print(haikuLines[haikuIdx][i]);
    }

    // Attribution: right-aligned, baseline pinned at RC_Y1
    display.setFont(&FreeSans9pt7b);
    int16_t ax, ay; uint16_t aw, ah;
    display.getTextBounds("- Basho", 0, 0, &ax, &ay, &aw, &ah);
    display.setCursor(RC_XR - (int)aw - ax, RC_Y1);
    display.print("- Basho");

    // ----------------------------------------------------------
    //  ROW 2 — Calendar grid  (y=100..300)
    // ----------------------------------------------------------
    const int CAL_Y0   = ROW1_H + 4;
    const int CELL_W   = W / 7;
    const int HEADER_H = 22;
    const int DOW_H    = 20;
    const int GRID_TOP = CAL_Y0 + HEADER_H + DOW_H;
    const int ROWS     = 6;
    const int CELL_H   = (H - GRID_TOP - 2) / ROWS;

    char hdr[20];
    snprintf(hdr, sizeof(hdr), "%s %d", monthNames[mon-1], yr);
    display.setFont(&FreeSansBold12pt7b);
    display.getTextBounds(hdr, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((W - (int)bw) / 2 - bx, CAL_Y0 + 17);
    display.print(hdr);

    const char* dows[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};
    int dowY = CAL_Y0 + HEADER_H + DOW_H - 3;
    for (int i = 0; i < 7; i++) {
      display.getTextBounds(dows[i], 0, 0, &bx, &by, &bw, &bh);
      int cx = i * CELL_W + (CELL_W - (int)bw) / 2 - bx;
      display.setCursor(cx, dowY);
      display.print(dows[i]);
    }

    display.drawFastHLine(2, GRID_TOP, W - 4, GxEPD_BLACK);
    for (int i = 1; i < 7; i++)
      display.drawFastVLine(i * CELL_W, GRID_TOP, H - GRID_TOP - 1, GxEPD_BLACK);
    for (int r = 1; r <= ROWS; r++) {
      int ry = GRID_TOP + r * CELL_H;
      if (ry < H) display.drawFastHLine(0, ry, W, GxEPD_BLACK);
    }

    display.setFont(&FreeSansBold12pt7b);
    int startCol = firstDayOfMonth(mon, yr);
    int total    = daysInMon(mon, yr);
    int col = startCol, row = 0;

    for (int d = 1; d <= total; d++) {
      int cellX = col * CELL_W;
      int cellY = GRID_TOP + row * CELL_H;

      char ds[3];
      snprintf(ds, sizeof(ds), "%d", d);
      display.getTextBounds(ds, 0, 0, &bx, &by, &bw, &bh);
      int tx2 = cellX + (CELL_W - (int)bw) / 2 - bx;
      int ty2 = cellY + (CELL_H + (int)bh) / 2;

      if (d == day) {
        display.fillRect(cellX + 1, cellY + 1,
                         CELL_W - 2, CELL_H - 2, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(tx2, ty2);
        display.print(ds);
        display.setTextColor(GxEPD_BLACK);
      } else {
        display.setCursor(tx2, ty2);
        display.print(ds);
      }

      col++;
      if (col == 7) { col = 0; row++; }
    }

  } while (display.nextPage());
}

// ============================================================
//  Deep sleep — unsigned throughout to avoid 2106 overflow bug
// ============================================================
unsigned long secondsUntilMidnight(unsigned long epoch) {
  return 86400UL - (epoch % 86400UL);
}

// ============================================================
//  setup()
// ============================================================
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi failed -- sleeping 1hr");
    esp_sleep_enable_timer_wakeup(3600ULL * 1000000ULL);
    esp_deep_sleep_start();
  }
  Serial.println("WiFi connected");

  // NTP with year sanity check — unsigned long prevents 2106 bug
  timeClient.begin();
  unsigned long epoch = 0;
  int ntpYear = 0;
  for (int attempt = 0; attempt < 3; attempt++) {
    timeClient.update();
    epoch   = timeClient.getEpochTime();
    setTime((time_t)epoch);
    ntpYear = year();
    Serial.printf("NTP attempt %d: epoch=%lu  year=%d\n",
                  attempt + 1, epoch, ntpYear);
    if (ntpYear >= 2024 && ntpYear <= 2099) break;
    delay(2000);
  }
  if (ntpYear < 2024 || ntpYear > 2099) {
    Serial.println("NTP year out of range -- sleeping 1hr");
    esp_sleep_enable_timer_wakeup(3600ULL * 1000000ULL);
    esp_deep_sleep_start();
  }

  int dow = weekday() - 1;
  int mon = month();
  int d   = ::day();
  int yr  = year();
  Serial.printf("Date: %s %d/%d/%d\n", dayNames[dow], mon, d, yr);

  int idx = dailyHaikuIndex(mon, d);
  Serial.printf("Haiku index: %d (%d lines)\n", idx, haikuLineCount[idx]);

  int battPct = readBatteryPercent();

  display.init(115200, true, 2, false);
  drawDisplay(dow, mon, d, yr, idx, battPct);
  display.hibernate();
  Serial.println("Display updated -- going to sleep");

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  unsigned long sleepSecs = secondsUntilMidnight(epoch);
  Serial.printf("Sleeping %lu seconds until midnight\n", sleepSecs);
  esp_sleep_enable_timer_wakeup((uint64_t)sleepSecs * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {}
