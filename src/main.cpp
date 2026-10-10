#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

// ------------------- LED Setup -------------------
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS 194
#define DATA_PIN 2  // ESP32 GPIO16 (avoids boot strapping pins)

CRGB leds[NUM_LEDS];

// --- Fire color palette ---
const CRGB firePalette[] = {
  CRGB(0x10, 0x00, 0x00), // deep red
  CRGB(0x80, 0x10, 0x00), // red
  CRGB(0xFF, 0x40, 0x00), // orange
  CRGB(0xFF, 0xA0, 0x00), // yellow-orange
  CRGB(0xFF, 0xFF, 0x20), // yellow
  CRGB(0xFF, 0xFF, 0xFF)  // white
};
const int firePaletteSize = sizeof(firePalette) / sizeof(CRGB);

// --- Electricity effect globals ---
uint8_t NUM_BOLTS = 10;
struct Bolt {
  int pos;
  int len;
  int speed;
  bool dirUp;
};
static Bolt* bolts = nullptr;
static bool boltsInitialized = false;
uint8_t plasmaBaseSpeed = 3; // baseline speed, can be changed via API or code

// --- Effect state ---
enum DisplayMode {
  MODE_NONE,
  MODE_RAINBOW,
  MODE_ELECTRICITY,
  MODE_ELECTRICITY2,
  MODE_FIRE,
  MODE_FIRERAINBOW,
  MODE_RED,
  MODE_GREEN,
  MODE_BLUE,
  MODE_PURPLE,
  MODE_PINK,
  MODE_YELLOW,
  MODE_ORANGE,
  MODE_WAVE,
  MODE_HALLOWEEN0,
  MODE_HALLOWEEN1,
  MODE_HALLOWEEN2,
  MODE_HALLOWEEN3,
  MODE_HALLOWEEN4
};

// Halloween effect declarations
void showHalloween0();
void showHalloween1();
void showHalloween2();
void showHalloween3();
void handleHalloween0();
void handleHalloween1();
void handleHalloween2();
void handleHalloween3();
void showHalloween4();
void handleHalloween4();
DisplayMode currentMode = MODE_NONE;
int electricityPos = 0;
bool electricityDirUp = true;

// --- WiFi Setup ---





// Home Configuration

const char* ssid = "Rumblechop-IoT";
const char* password = "1MillionBoobies";
bool useDHCP = false; // Set to false to use the static IP configuration below
IPAddress local_IP(192, 168, 1, 107);      // Change last octet as needed
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0); // Standard subnet for home networks

// Portable Configuration

// const char* ssid = "danlers1234";
// const char* password = "Danlers1234";
// IPAddress subnet(255, 255, 255, 240);
// IPAddress local_IP(172, 20, 10, 10);      // Change last octet as needed
// IPAddress gateway(172, 20, 10, 0);


unsigned long lastFrameTime = 0;
const int frameInterval = 30; // ms

WebServer server(80);
const char* relayHosts[] = {"172.20.10.11", "172.20.10.12", "172.20.10.13"};

bool repeatEnabled = false;
unsigned long repeatIntervalMs = 15000;
unsigned long lastRepeatTime = 0;
String lastRelayPath = "";
String lastRelayQuery = "";

bool isRelayRequest() {
  if (!server.hasArg("relay")) return false;
  String relayValue = server.arg("relay");
  return relayValue.equalsIgnoreCase("true") || relayValue == "1";
}

String buildRelayQueryString() {
  String query = "";
  for (int i = 0; i < server.args(); i++) {
    String name = server.argName(i);
    if (name.equalsIgnoreCase("relay") || name.equalsIgnoreCase("repeat")) continue;
    if (query.length() == 0) {
      query += "?";
    } else {
      query += "&";
    }
    query += name + "=" + server.arg(i);
  }
  return query;
}

void relayRequestToHost(const char* host, const String& path, const String& query) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Relay skipped: WiFi not connected");
    return;
  }

  String url = String("http://") + host + path + query;
  Serial.print("Relay request URL: ");
  Serial.println(url);

  HTTPClient http;
  WiFiClient client;
  if (!http.begin(client, url)) {
    Serial.print("Relay begin failed for host: ");
    Serial.println(host);
    return;
  }

  int httpCode = http.GET();
  Serial.print("Relay host ");
  Serial.print(host);
  Serial.print(" returned code ");
  Serial.println(httpCode);
  http.end();
}

void maybeRelay() {
  if (server.uri() == "/repeat" || server.uri() == "/stopRelaying") {
    Serial.print("maybeRelay: skipping relay for control endpoint ");
    Serial.println(server.uri());
    return;
  }

  if (!isRelayRequest()) {
    Serial.println("maybeRelay: relay disabled");
    return;
  }

  lastRelayPath = server.uri();
  lastRelayQuery = buildRelayQueryString();
  Serial.print("Stored last relay command: ");
  Serial.print(lastRelayPath);
  Serial.print(" ");
  Serial.println(lastRelayQuery);

  Serial.println("maybeRelay: relay enabled");
  String currentPath = server.uri();
  String currentQuery = buildRelayQueryString();
  for (unsigned int i = 0; i < sizeof(relayHosts) / sizeof(relayHosts[0]); i++) {
    relayRequestToHost(relayHosts[i], currentPath, currentQuery);
  }
}

void relayLastCommand() {
  if (!repeatEnabled) {
    return;
  }
  if (lastRelayPath.length() == 0) {
    Serial.println("relayLastCommand: no last command stored");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("relayLastCommand: WiFi not connected");
    return;
  }
  Serial.print("relayLastCommand: repeating ");
  Serial.print(lastRelayPath);
  Serial.print(" ");
  Serial.println(lastRelayQuery);
  for (unsigned int i = 0; i < sizeof(relayHosts) / sizeof(relayHosts[0]); i++) {
    relayRequestToHost(relayHosts[i], lastRelayPath, lastRelayQuery);
  }
}

void handleRepeat() {
  if (lastRelayPath.length() == 0) {
    server.send(400, "text/plain", "No last command to repeat");
    return;
  }
  repeatEnabled = true;
  lastRepeatTime = millis();
  server.send(200, "text/plain", "Repeat relay started");
  Serial.println("Repeat relay started");
}

void handleStopRelaying() {
  repeatEnabled = false;
  server.send(200, "text/plain", "Repeat relay stopped");
  Serial.println("Repeat relay stopped");
}

// ------------------- Function Declarations -------------------
void setup_wifi();
void showRainbow();
void showElectricity();
void showBootStatus(CRGB color);
void handleRainbow();
void handleElectricity();
void handleOff();
void handleBrightness();
void showFire();
void showFireRainbow();
void handleFire() {
  currentMode = MODE_FIRE;
  server.send(200, "text/plain", "Fire effect triggered");
}
void handleFireRainbow() {
  currentMode = MODE_FIRERAINBOW;
  server.send(200, "text/plain", "FireRainbow effect triggered");
}
void showRed();
void handleRed() {
  currentMode = MODE_RED;
  server.send(200, "text/plain", "Red effect triggered");
}
void showGreen();
void handleGreen() {
  currentMode = MODE_GREEN;
  server.send(200, "text/plain", "Green effect triggered");
}
void showBlue();
void handleBlue() {
  currentMode = MODE_BLUE;
  server.send(200, "text/plain", "Blue effect triggered");
}
void showPurple();
void handlePurple() {
  currentMode = MODE_PURPLE;
  server.send(200, "text/plain", "Purple effect triggered");
}
void showPink();
void handlePink() {
  currentMode = MODE_PINK;
  server.send(200, "text/plain", "Pink effect triggered");
}
void showYellow();
void handleYellow() {
  currentMode = MODE_YELLOW;
  server.send(200, "text/plain", "Yellow effect triggered");
}
void showOrange();
void handleOrange() {
  currentMode = MODE_ORANGE;
  server.send(200, "text/plain", "Orange effect triggered");
}

// ------------------- Boot Status Lights -------------------
void showBootStatus(CRGB color) {
  FastLED.clear();
  for (int i = 0; i < 10; i++) {
    int idx = i * (NUM_LEDS / 10);
    if (idx < NUM_LEDS) {
      leds[idx] = color;
    }
  }
  FastLED.show();
}

// ------------------- Rainbow Effect -------------------
void showRainbow() {
  static uint8_t startHue = 0;
  // Only compute for first half, then mirror
  const int half = NUM_LEDS / 2;
  for (int i = 0; i < half; i++) {
    uint8_t hue = startHue + (i * 255 / (half - 1));
    leds[i] = CHSV(hue, 255, 255);
    leds[NUM_LEDS - 1 - i] = CHSV(hue, 255, 255);
  }
  startHue++;
}

// ------------------- Electricity Effect -------------------
// --- Improved Plasma Electricity Effect ---
void showElectricity() {
  if (!bolts) return;
  if (!boltsInitialized) {
    for (int i = 0; i < NUM_BOLTS; i++) {
      bolts[i].len = 8 + random8(8); // 8-15
      bolts[i].pos = random(NUM_LEDS - bolts[i].len);
      bolts[i].speed = plasmaBaseSpeed + random8(3); // baseline + random
      bolts[i].dirUp = random8(2);
    }
    boltsInitialized = true;
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int b = 0; b < NUM_BOLTS; b++) {
    for (int i = 0; i < bolts[b].len; i++) {
      int idx = bolts[b].dirUp ? bolts[b].pos + i : bolts[b].pos - i;
      if (idx >= 0 && idx < NUM_LEDS) {
        // Plasma color: flicker between blue/white
        uint8_t blue = 180 + random8(75);
        uint8_t white = 200 + random8(55);
        if (random8(3) == 0) {
          leds[idx] = CRGB(white, white, blue); // white-blue
        } else {
          leds[idx] = CRGB(50, 100, blue); // blue tone
        }
      }
    }
    // Move bolt
    if (bolts[b].dirUp) {
      bolts[b].pos += bolts[b].speed;
      if (bolts[b].pos > NUM_LEDS - bolts[b].len) {
        bolts[b].dirUp = false;
        bolts[b].pos = NUM_LEDS - bolts[b].len;
        bolts[b].len = 8 + random8(8);
        bolts[b].speed = plasmaBaseSpeed + random8(3);
      }
    } else {
      bolts[b].pos -= bolts[b].speed;
      if (bolts[b].pos < 0) {
        bolts[b].dirUp = true;
        bolts[b].pos = 0;
        bolts[b].len = 8 + random8(8);
        bolts[b].speed = plasmaBaseSpeed + random8(3);
      }
    }
  }
}

// --- Smoother, more colorful electricity effect ---
void showElectricity2() {
  if (!bolts) return;
  if (!boltsInitialized) {
    for (int i = 0; i < NUM_BOLTS; i++) {
      bolts[i].len = 10 + random8(10); // 10-19, longer bolts
      bolts[i].pos = random(NUM_LEDS - bolts[i].len);
      bolts[i].speed = plasmaBaseSpeed + random8(2); // slightly slower
      bolts[i].dirUp = random8(2);
    }
    boltsInitialized = true;
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int b = 0; b < NUM_BOLTS; b++) {
    // Pick a base color for the bolt
    uint8_t baseHue = random8();
    for (int i = 0; i < bolts[b].len; i++) {
      int idx = bolts[b].dirUp ? bolts[b].pos + i : bolts[b].pos - i;
      if (idx >= 0 && idx < NUM_LEDS) {
        // Smoother color gradient along the bolt
        uint8_t hue = baseHue + i * 8 + random8(8); // gradient with randomness
        uint8_t sat = 200 + random8(55); // vibrant
        uint8_t val = 180 + random8(75); // bright
        leds[idx] = CHSV(hue, sat, val);
        // Occasional white-hot flicker
        if (random8(12) == 0) {
          leds[idx] = CRGB(255, 255, 255);
        }
      }
    }
    // Move bolt smoothly (fractional speed)
    if (bolts[b].dirUp) {
      bolts[b].pos += bolts[b].speed;
      if (bolts[b].pos > NUM_LEDS - bolts[b].len) {
        bolts[b].dirUp = false;
        bolts[b].pos = NUM_LEDS - bolts[b].len;
        bolts[b].len = 10 + random8(10);
        bolts[b].speed = plasmaBaseSpeed + random8(2);
      }
    } else {
      bolts[b].pos -= bolts[b].speed;
      if (bolts[b].pos < 0) {
        bolts[b].dirUp = true;
        bolts[b].pos = 0;
        bolts[b].len = 10 + random8(10);
        bolts[b].speed = plasmaBaseSpeed + random8(2);
      }
    }
  }
}

void showFire() {
  for (int i = 0; i < NUM_LEDS; i++) {
    // Flicker: random palette index, plus some bias for lower LEDs
    int baseIdx = map(i, 0, NUM_LEDS - 1, 0, firePaletteSize - 2);
    int flicker = random8(3) - 1; // -1, 0, 1
    int colorIdx = constrain(baseIdx + flicker, 0, firePaletteSize - 1);
    // Extra white flicker at top
    if (i > NUM_LEDS * 3 / 4 && random8(8) == 0) colorIdx = firePaletteSize - 1;
    leds[i] = firePalette[colorIdx];
  }
  // Randomly dim some pixels for more flicker
  for (int i = 0; i < NUM_LEDS / 10; i++) {
    int idx = random(NUM_LEDS);
    leds[idx].fadeToBlackBy(random8(128));
  }
}

void showFireRainbow() {
  // 🌀 Animate by offsetting the position along the strip
  static uint8_t positionOffset = 0;

  // 🎨 Define the hue range for the effect
  const uint8_t hueStart = 0;    // Red
  const uint8_t hueEnd = 64;     // Yellow
  const uint8_t hueRange = hueEnd - hueStart;

  // 💡 Total LEDs to fill before mirroring
  const int halfCount = NUM_LEDS / 2;

  for (int i = 0; i < halfCount; i++) {
    // 🧠 Calculate a position that wraps around [0, halfCount)
    int shiftedIndex = (i + positionOffset) % halfCount;

    // 🎯 Map this position into the desired hue range
    uint8_t hue = hueStart + ((shiftedIndex * hueRange) / (halfCount - 1));

    // 🖍️ Set symmetric/mirrored colors
    leds[i] = CHSV(hue, 255, 255);
    leds[NUM_LEDS - 1 - i] = CHSV(hue, 255, 255);
  }

  // 🔁 Increment position for smooth scrolling effect
  positionOffset++;
}

void showRed() {
  // Fill with deep red
  fill_solid(leds, NUM_LEDS, CRGB(180, 0, 0));
  // Add random reddish flickers
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t r = 180 + random8(75); // 180-255
    uint8_t g = random8(30);       // 0-29
    uint8_t b = random8(20);       // 0-19
    leds[idx] = CRGB(r, g, b);
  }
}

void showGreen() {
  fill_solid(leds, NUM_LEDS, CRGB(0, 180, 0));
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t g = 180 + random8(75);
    uint8_t r = random8(30);
    uint8_t b = random8(20);
    leds[idx] = CRGB(r, g, b);
  }
}

void showBlue() {
  fill_solid(leds, NUM_LEDS, CRGB(0, 0, 180));
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t b = 180 + random8(75);
    uint8_t r = random8(20);
    uint8_t g = random8(30);
    leds[idx] = CRGB(r, g, b);
  }
}

void showPurple() {
  fill_solid(leds, NUM_LEDS, CRGB(120, 0, 180));
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t r = 120 + random8(60);
    uint8_t b = 180 + random8(75);
    uint8_t g = random8(20);
    leds[idx] = CRGB(r, g, b);
  }
}

void showPink() {
  fill_solid(leds, NUM_LEDS, CRGB(255, 20, 120));
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t r = 200 + random8(55);
    uint8_t g = 20 + random8(30);
    uint8_t b = 120 + random8(60);
    leds[idx] = CRGB(r, g, b);
  }
}

void showYellow() {
  // Warm yellow fill with subtle amber flicker
  fill_solid(leds, NUM_LEDS, CRGB(200, 170, 0));
  // Add brighter yellow/amber highlights
  for (int i = 0; i < NUM_LEDS / 6; i++) {
    int idx = random(NUM_LEDS);
    uint8_t r = 220 + random8(35); // 220-255
    uint8_t g = 180 + random8(60); // 180-239
    uint8_t b = random8(24);       // small blue component
    leds[idx] = CRGB(r, g, b);
  }
  // Occasional white-hot sparkles
  for (int i = 0; i < NUM_LEDS / 40; i++) {
    int idx = random(NUM_LEDS);
    leds[idx] = CRGB(255, 255, 200);
  }
}

void showOrange() {
}

// --- Tree Wave Effect (LED tree mapping ported from mapping.ts) ---
struct TreeNode {
  const uint8_t* stem;
  uint8_t stemLen;
  uint8_t firstChild;  // children are contiguous in treeNodes
  uint8_t numChildren;
};

static const uint8_t s_1[] = {0, 1, 3, 4, 5, 6, 7, 8};
static const uint8_t s_1_1[] = {9, 10, 11, 12, 13};
static const uint8_t s_1_1_1[] = {14};
static const uint8_t s_1_1_1_1[] = {15, 16};
static const uint8_t s_1_1_1_2[] = {20, 19, 18, 17};
static const uint8_t s_1_1_2[] = {21, 22, 23, 24, 25};
static const uint8_t s_1_2[] = {46, 45, 44, 43, 42, 41};
static const uint8_t s_1_2_1[] = {26, 27, 28, 29, 30, 31, 32, 33};
static const uint8_t s_1_2_2[] = {40, 39, 38, 37, 36, 35, 34};
static const uint8_t s_1_3[] = {47, 48, 49, 50, 51, 52, 53, 54};
static const uint8_t s_1_3_1[] = {55, 56, 57, 58, 59};
static const uint8_t s_1_3_2[] = {68};
static const uint8_t s_1_3_2_1[] = {69, 70, 71, 72, 73};
static const uint8_t s_1_3_2_2[] = {67, 66, 65, 64, 63, 62, 61, 60};
static const uint8_t s_2[] = {98, 99, 100, 101, 102, 103, 104, 105, 106,
                              107, 108, 109, 110, 111, 112, 113, 114, 115};
static const uint8_t s_2_1[] = {80, 79, 78, 77, 76, 75, 74};
static const uint8_t s_2_2[] = {116, 117, 118, 119, 120, 121, 122, 123};
static const uint8_t s_3[] = {97, 96, 95, 94, 93, 92, 91, 90, 89,
                              88, 87, 86, 85, 84, 83, 82, 81};
static const uint8_t s_3_2[] = {144, 143, 142, 141, 129};
static const uint8_t s_3_2_1[] = {127, 126, 125, 124};
static const uint8_t s_3_2_2[] = {132};
static const uint8_t s_3_3[] = {145, 146, 147};
static const uint8_t s_3_3_1[] = {148, 139};
static const uint8_t s_3_3_1_1[] = {140, 128};
static const uint8_t s_3_3_1_2[] = {133};
static const uint8_t s_3_3_2[] = {149, 150, 151, 152, 153};
static const uint8_t s_4[] = {193, 192, 191, 190, 189, 188, 187, 186};
static const uint8_t s_4_1[] = {169, 170, 171, 172};
static const uint8_t s_4_1_1[] = {173, 174};
static const uint8_t s_4_1_2[] = {175, 153, 159, 156};
static const uint8_t s_4_1_2_1[] = {157};
static const uint8_t s_4_1_2_2[] = {155, 154};
static const uint8_t s_4_2[] = {168, 167, 166, 165, 164, 163, 162, 161, 160, 176, 177, 178, 179};
static const uint8_t s_4_2_1[] = {179, 180};
static const uint8_t s_4_2_2[] = {183, 184, 185};
static const uint8_t s_4_2_3[] = {181, 182};

#define TREE_NODE(s, first, n) {s, sizeof(s), first, n}
// Breadth-first order so each node's children sit in one contiguous range
static const TreeNode treeNodes[] = {
  {nullptr, 0, 1, 4},            // 0  root
  TREE_NODE(s_1, 5, 3),          // 1  '1'
  TREE_NODE(s_2, 8, 2),          // 2  '2'
  TREE_NODE(s_3, 10, 3),         // 3  '3'
  TREE_NODE(s_4, 13, 2),         // 4  '4'
  TREE_NODE(s_1_1, 15, 2),       // 5  '1.1'
  TREE_NODE(s_1_2, 17, 2),       // 6  '1.2'
  TREE_NODE(s_1_3, 19, 2),       // 7  '1.3'
  TREE_NODE(s_2_1, 0, 0),        // 8  '2.1'
  TREE_NODE(s_2_2, 0, 0),        // 9  '2.2'
  TREE_NODE(s_2_2, 0, 0),        // 10 '3.1' (same LEDs as 2.2 in the original mapping)
  TREE_NODE(s_3_2, 21, 2),       // 11 '3.2'
  TREE_NODE(s_3_3, 23, 2),       // 12 '3.3'
  TREE_NODE(s_4_1, 25, 2),       // 13 '4.1'
  TREE_NODE(s_4_2, 27, 3),       // 14 '4.2'
  TREE_NODE(s_1_1_1, 30, 2),     // 15 '1.1.1'
  TREE_NODE(s_1_1_2, 0, 0),      // 16 '1.1.2'
  TREE_NODE(s_1_2_1, 0, 0),      // 17 '1.2.1'
  TREE_NODE(s_1_2_2, 0, 0),      // 18 '1.2.2'
  TREE_NODE(s_1_3_1, 0, 0),      // 19 '1.3.1'
  TREE_NODE(s_1_3_2, 32, 2),     // 20 '1.3.2'
  TREE_NODE(s_3_2_1, 0, 0),      // 21 '3.2.1'
  TREE_NODE(s_3_2_2, 0, 0),      // 22 '3.2.2'
  TREE_NODE(s_3_3_1, 34, 2),     // 23 '3.3.1'
  TREE_NODE(s_3_3_2, 0, 0),      // 24 '3.3.2'
  TREE_NODE(s_4_1_1, 0, 0),      // 25 '4.1.1'
  TREE_NODE(s_4_1_2, 36, 2),     // 26 '4.1.2'
  TREE_NODE(s_4_2_1, 0, 0),      // 27 '4.2.1'
  TREE_NODE(s_4_2_2, 0, 0),      // 28 '4.2.2'
  TREE_NODE(s_4_2_3, 0, 0),      // 29 '4.2.3'
  TREE_NODE(s_1_1_1_1, 0, 0),    // 30 '1.1.1.1'
  TREE_NODE(s_1_1_1_2, 0, 0),    // 31 '1.1.1.2'
  TREE_NODE(s_1_3_2_1, 0, 0),    // 32 '1.3.2.1'
  TREE_NODE(s_1_3_2_2, 0, 0),    // 33 '1.3.2.2'
  TREE_NODE(s_3_3_1_1, 0, 0),    // 34 '3.3.3.1'
  TREE_NODE(s_3_3_1_2, 0, 0),    // 35 '3.3.3.2'
  TREE_NODE(s_4_1_2_1, 0, 0),    // 36 '4.1.2.1'
  TREE_NODE(s_4_1_2_2, 0, 0),    // 37 '4.1.2.2'
};

struct WaveCursor {
  uint8_t node;
  int8_t pos;  // -1 = not started
  bool done;
  CRGB color;
};
const int MAX_WAVE_CURSORS = 64;
static WaveCursor waveCursors[MAX_WAVE_CURSORS];
static int waveCursorCount = 0;
unsigned long waveSpawnIntervalMs = 1100;
unsigned long lastWaveSpawn = 0;

void spawnWaveCursor(uint8_t node, CRGB color) {
  if (waveCursorCount >= MAX_WAVE_CURSORS) return;
  waveCursors[waveCursorCount++] = {node, -1, false, color};
}

void showWave() {
  unsigned long now = millis();
  if (now - lastWaveSpawn >= waveSpawnIntervalMs) {
    lastWaveSpawn = now;
    spawnWaveCursor(0, CRGB(random8(), random8(), random8()));
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  // Children spawned here are advanced in this same pass, so branches light without a gap
  for (int i = 0; i < waveCursorCount; i++) {
    WaveCursor& c = waveCursors[i];
    const TreeNode& n = treeNodes[c.node];
    if (c.pos < 0) {
      c.pos = 0;
    } else if (c.pos + 1 < n.stemLen) {
      c.pos++;
    } else {
      for (uint8_t k = 0; k < n.numChildren; k++) {
        spawnWaveCursor(n.firstChild + k, c.color);
      }
      c.done = true;
    }
    if (c.pos < n.stemLen) leds[n.stem[c.pos]] = c.color;
  }
  int kept = 0;
  for (int i = 0; i < waveCursorCount; i++) {
    if (!waveCursors[i].done) waveCursors[kept++] = waveCursors[i];
  }
  waveCursorCount = kept;
}

void handleWave() {
  if (server.hasArg("interval")) {
    waveSpawnIntervalMs = constrain(server.arg("interval").toInt(), 100, 60000);
  }
  waveCursorCount = 0;
  lastWaveSpawn = millis() - waveSpawnIntervalMs;
  currentMode = MODE_WAVE;
  server.send(200, "text/plain", "Wave effect triggered");
}
// --- Halloween Effects ---
// Halloween 0: Alternating orange and purple, with random white flashes
void showHalloween0() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if ((i / 10) % 2 == 0) {
      leds[i] = CRGB(255, 40, 10); // more red-orange
    } else {
      leds[i] = CRGB(120, 0, 180); // purple
    }
    if (random8(60) == 0) {
      leds[i] = CRGB::White;
    }
  }
}

// Halloween 1: Orange base, purple "ghost" pulses moving along the strip
void showHalloween1() {
  static uint8_t ghostPos = 0;
  fill_solid(leds, NUM_LEDS, CRGB(255, 40, 10));
  for (int i = 0; i < 5; i++) {
    int pos = (ghostPos + i * (NUM_LEDS / 5)) % NUM_LEDS;
    for (int j = -2; j <= 2; j++) {
      int idx = pos + j;
      if (idx >= 0 && idx < NUM_LEDS) {
        leds[idx] = CRGB(120, 0, 180); // purple ghost
      }
    }
  }
  ghostPos = (ghostPos + 1) % NUM_LEDS;
}

// Halloween 2: Purple base, random orange and green "pumpkin" flashes
void showHalloween2() {
  fill_solid(leds, NUM_LEDS, CRGB(120, 0, 180));
  for (int i = 0; i < NUM_LEDS / 12; i++) {
    int idx = random(NUM_LEDS);
    if (random8(2) == 0) {
      leds[idx] = CRGB(255, 40, 10); // more red-orange
    } else {
      leds[idx] = CRGB(0, 255, 40); // green
    }
  }
}

// Halloween 3: Flickering candle effect with orange/yellow, occasional purple flashes
void showHalloween3() {
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t flicker = random8(40);
    leds[i] = CRGB(255, 60 + flicker, 10 + random8(20)); // more red-orange flicker
    if (random8(80) == 0) {
      leds[i] = CRGB(120, 0, 180); // purple flash
    }
  }
}
// Halloween 4: Blocks of color flash up and down randomly
void showHalloween4() {
  const int blockSize = 10;
  const int numBlocks = (NUM_LEDS + blockSize - 1) / blockSize;
  static uint8_t blockState[numBlocks] = {0};
  static int blockDir[numBlocks] = {0};
  static uint8_t blockColor[numBlocks] = {0};
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  if (now - lastUpdate > 80) {
    lastUpdate = now;
    for (int b = 0; b < numBlocks; b++) {
      if (blockState[b] == 0 && random8(18) == 0) {
        blockDir[b] = random8(2) ? 1 : -1;
        blockColor[b] = random8(3); // 0=orange, 1=purple, 2=green
        blockState[b] = blockDir[b] > 0 ? 1 : blockSize;
      } else if (blockState[b] > 0) {
        blockState[b] += blockDir[b];
        if (blockState[b] > blockSize || blockState[b] < 1) blockState[b] = 0;
      }
    }
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int b = 0; b < numBlocks; b++) {
    if (blockState[b] > 0) {
      CRGB color = CRGB::Black;
      if (blockColor[b] == 0) color = CRGB(255, 40, 10); // more red-orange
      else if (blockColor[b] == 1) color = CRGB(120, 0, 180); // purple
      else color = CRGB(0, 255, 40); // green
      int start = b * blockSize;
      int end = start + blockState[b] * blockDir[b];
      if (blockDir[b] > 0) {
        for (int i = start; i < start + blockState[b]; i++) {
          if (i < NUM_LEDS) leds[i] = color;
        }
      } else {
        for (int i = start + blockSize - 1; i >= start + blockSize - blockState[b]; i--) {
          if (i < NUM_LEDS && i >= 0) leds[i] = color;
        }
      }
    }
  }
}

void handleHalloween0() {
  currentMode = MODE_HALLOWEEN0;
  server.send(200, "text/plain", "Halloween 0 effect triggered");
}
void handleHalloween1() {
  currentMode = MODE_HALLOWEEN1;
  server.send(200, "text/plain", "Halloween 1 effect triggered");
}
void handleHalloween2() {
  currentMode = MODE_HALLOWEEN2;
  server.send(200, "text/plain", "Halloween 2 effect triggered");
}
void handleHalloween3() {
  currentMode = MODE_HALLOWEEN3;
  server.send(200, "text/plain", "Halloween 3 effect triggered");
}
void handleHalloween4() {
  currentMode = MODE_HALLOWEEN4;
  server.send(200, "text/plain", "Halloween 4 effect triggered");
}

void handleRainbow() {
  currentMode = MODE_RAINBOW;
  server.send(200, "text/plain", "Rainbow updated");
}

void handleElectricity() {
  // Parse query params
  String speedParam = server.hasArg("speed") ? server.arg("speed") : "";
  String boltsParam = server.hasArg("bolts") ? server.arg("bolts") : "";
  if (speedParam.length() > 0) {
    plasmaBaseSpeed = constrain(speedParam.toInt(), 1, 10);
  }
  if (boltsParam.length() > 0) {
    uint8_t newBolts = constrain(boltsParam.toInt(), 1, 30);
    if (newBolts != NUM_BOLTS) {
      NUM_BOLTS = newBolts;
      if (bolts) delete[] bolts;
      bolts = new Bolt[NUM_BOLTS];
      boltsInitialized = false;
    }
  }
  // Ensure bolts array exists so the effect can run immediately after triggering
  if (!bolts) {
    bolts = new Bolt[NUM_BOLTS];
    boltsInitialized = false;
  }
  currentMode = MODE_ELECTRICITY;
  server.send(200, "text/plain", "Electricity effect triggered");
}

void handleElectricity2() {
  // Parse query params
  String speedParam = server.hasArg("speed") ? server.arg("speed") : "";
  String boltsParam = server.hasArg("bolts") ? server.arg("bolts") : "";
  if (speedParam.length() > 0) {
    plasmaBaseSpeed = constrain(speedParam.toInt(), 1, 10);
  }
  if (boltsParam.length() > 0) {
    uint8_t newBolts = constrain(boltsParam.toInt(), 1, 30);
    if (newBolts != NUM_BOLTS) {
      NUM_BOLTS = newBolts;
      if (bolts) delete[] bolts;
      bolts = new Bolt[NUM_BOLTS];
      boltsInitialized = false;
    }
  }
  // Ensure bolts array exists so the effect can run immediately after triggering
  if (!bolts) {
    bolts = new Bolt[NUM_BOLTS];
    boltsInitialized = false;
  }
  currentMode = MODE_ELECTRICITY2;
  server.send(200, "text/plain", "Electricity2 effect triggered");
}

void handleOff() {
  currentMode = MODE_NONE;
  FastLED.clear();
  FastLED.show();
  server.send(200, "text/plain", "LEDs turned off");
}

void handleBrightness() {
  String val = server.hasArg("value") ? server.arg("value") : "";
  if (val.length() > 0) {
    int brightness = constrain(val.toInt(), 1, 255);
    FastLED.setBrightness(brightness);
    FastLED.show();
    server.send(200, "text/plain", String("Brightness set to ") + brightness);
  } else {
    server.send(400, "text/plain", "Missing value param");
  }
}

void handleRelayCallback(void (*handler)()) {
  Serial.print("Incoming request: ");
  Serial.print(server.uri());
  Serial.print(" args=");
  Serial.print(server.args());
  Serial.print(" relay=");
  Serial.println(server.hasArg("relay") ? server.arg("relay") : "false");
  maybeRelay();
  handler();
}

void setup() {
  server.on("/halloween4", HTTP_GET, []() { handleRelayCallback(handleHalloween4); });
  server.on("/halloween0", HTTP_GET, []() { handleRelayCallback(handleHalloween0); });
  server.on("/halloween1", HTTP_GET, []() { handleRelayCallback(handleHalloween1); });
  server.on("/halloween2", HTTP_GET, []() { handleRelayCallback(handleHalloween2); });
  server.on("/halloween3", HTTP_GET, []() { handleRelayCallback(handleHalloween3); });
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting setup...");
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(70);
  showBootStatus(CRGB::Red); // Show red status lights
  FastLED.show();
  setup_wifi();
  server.on("/rainbow", HTTP_GET, []() { handleRelayCallback(handleRainbow); });
  server.on("/electricity", HTTP_GET, []() { handleRelayCallback(handleElectricity); });
  server.on("/electricity2", HTTP_GET, []() { handleRelayCallback(handleElectricity2); });
  server.on("/off", HTTP_GET, []() { handleRelayCallback(handleOff); });
  server.on("/brightness", HTTP_GET, []() { handleRelayCallback(handleBrightness); });
  server.on("/fire", HTTP_GET, []() { handleRelayCallback(handleFire); });
  server.on("/firerainbow", HTTP_GET, []() { handleRelayCallback(handleFireRainbow); });
  server.on("/red", HTTP_GET, []() { handleRelayCallback(handleRed); });
  server.on("/green", HTTP_GET, []() { handleRelayCallback(handleGreen); });
  server.on("/blue", HTTP_GET, []() { handleRelayCallback(handleBlue); });
  server.on("/purple", HTTP_GET, []() { handleRelayCallback(handlePurple); });
  server.on("/pink", HTTP_GET, []() { handleRelayCallback(handlePink); });
  server.on("/yellow", HTTP_GET, []() { handleRelayCallback(handleYellow); });
  server.on("/orange", HTTP_GET, []() { handleRelayCallback(handleOrange); });
  server.on("/wave", HTTP_GET, []() { handleRelayCallback(handleWave); });
  server.on("/repeat", HTTP_GET, handleRepeat);
  server.on("/stopRelaying", HTTP_GET, handleStopRelaying);
  // Add more endpoints here as needed
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (repeatEnabled && now - lastRepeatTime >= repeatIntervalMs) {
    lastRepeatTime = now;
    relayLastCommand();
  }
  if (now - lastFrameTime >= frameInterval) {
    lastFrameTime = now;
    switch (currentMode) {
      case MODE_RAINBOW:
        showRainbow();
        FastLED.show();
        break;
      case MODE_ELECTRICITY:
        showElectricity();
        FastLED.show();
        break;
      case MODE_ELECTRICITY2:
        showElectricity2();
        FastLED.show();
        break;
      case MODE_FIRE:
        showFire();
        FastLED.show();
        break;
      case MODE_FIRERAINBOW:
        showFireRainbow();
        FastLED.show();
        break;
      case MODE_RED:
        showRed();
        FastLED.show();
        break;
      case MODE_GREEN:
        showGreen();
        FastLED.show();
        break;
      case MODE_BLUE:
        showBlue();
        FastLED.show();
        break;
      case MODE_PURPLE:
        showPurple();
        FastLED.show();
        break;
      case MODE_PINK:
        showPink();
        FastLED.show();
        break;
      case MODE_YELLOW:
        showYellow();
        FastLED.show();
        break;
      case MODE_ORANGE:
        showOrange();
        FastLED.show();
        break;
      case MODE_WAVE:
        showWave();
        FastLED.show();
        break;
      case MODE_HALLOWEEN0:
        showHalloween0();
        FastLED.show();
        break;
      case MODE_HALLOWEEN1:
        showHalloween1();
        FastLED.show();
        break;
      case MODE_HALLOWEEN2:
        showHalloween2();
        FastLED.show();
        break;
      case MODE_HALLOWEEN3:
        showHalloween3();
        FastLED.show();
        break;
      case MODE_HALLOWEEN4:
        showHalloween4();
        FastLED.show();
        break;
      case MODE_NONE:
      default:
        // Only show status lights
        break;
    }
  }
}

// ------------------- WiFi Connection -------------------
void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.persistent(false); // don't wear out flash / avoid stale stored config
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);  // clear any previous connection/static config
  delay(100);
  // Print exact reason on every disconnect (e.g. 15=4-way handshake timeout/wrong
  // password, 202=auth fail, 2xx=PMF/WPA3 mismatch)
  WiFi.onEvent(
      [](WiFiEvent_t event, WiFiEventInfo_t info) {
        Serial.print(" [disconnect reason=");
        Serial.print(info.wifi_sta_disconnected.reason);
        Serial.print("]");
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  if (useDHCP) {
    // Force DHCP by clearing any static config
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  } else {
    WiFi.config(local_IP, gateway, subnet);
  }
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++attempts % 20 == 0) {
      Serial.print(" status=");
      Serial.println(WiFi.status()); // 1=no SSID found,4=auth/connect fail,6=disconnected
    }
    if (attempts == 40) {
      // Scan and report visible networks to help diagnose (band/SSID issues)
      Serial.println("\nStill not connected. Scanning for networks...");
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; i++) {
        Serial.printf("  %s (RSSI %d)%s\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                      WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? " [open]" : "");
      }
      WiFi.scanDelete();
      Serial.print("Retrying begin() for SSID: ");
      Serial.println(ssid);
      WiFi.begin(ssid, password);
    }
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  // Change status lights to green
  showBootStatus(CRGB::Green);
  FastLED.show();
}
