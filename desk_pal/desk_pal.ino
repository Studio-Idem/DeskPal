#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// Oled: Red=V5, Purple=GND, Blue=SCK (A5), Green=SDA (A4)
#define OLED_RESET -1

// RC522 RFID pins
#define RFID_SS   10
#define RFID_RST  6

// Store RFID strings in flash memory
const char UID_BLINK[] PROGMEM = "A2644F73";
const char UID_HAPPY[] PROGMEM = "70464F73";
const char UID_MAD[] PROGMEM = "A05B4F73";
const char UID_SAD[] PROGMEM = "84634973";
const char UID_SUS[] PROGMEM = "5E3E4F73";

const int PUPIL_RADIUS = 10;
const int PUPIL_SHINE_X = 2;
const int PUPIL_SHINE_Y = 3;
const int EYE_RADIUS = 25;
const int BLINK_DELAY = 200;

// Emotions that are available
enum Emotion { NORMAL, HAPPY, MAD, SAD, SUS };
Emotion currentEmotion = NORMAL;

// Create RFID reader and OLED screen
MFRC522 rfid(RFID_SS, RFID_RST);
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

void fillEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t color) {
  int32_t rx2 = (int32_t)rx * rx;
  int32_t ry2 = (int32_t)ry * ry;
  
  for (int16_t y = -ry; y <= ry; y++) {
    int32_t y2 = (int32_t)y * y;
    int32_t x_span_squared = rx2 * (ry2 - y2) / ry2;
    int16_t x_span = 0;

    if (x_span_squared > 0) {
      int32_t guess = x_span_squared >> 1;
      if (guess == 0) guess = 1;
      for (int i = 0; i < 4; i++) { 
        guess = (guess + x_span_squared / guess) >> 1;
      }
      x_span = (int16_t)guess;
    }
    
    display.drawFastHLine(x0 - x_span, y0 + y, (x_span << 1), color);
  }
}

// Translate RFID code to readable string
void getUIDString(char* buffer) {
  buffer[0] = '\0';
  for (byte i = 0; i < rfid.uid.size; i++){
    char segment[3];
    sprintf(segment, "%02X", rfid.uid.uidByte[i]);
    strcat(buffer, segment);
  }
}

// Helper function to compare UID with PROGMEM strings
bool compareUID(const char* uidStr, const char* progmemStr) {
  char buf[16];
  strcpy_P(buf, progmemStr);
  return strcmp(uidStr, buf) == 0;
}

// Bunch of overlays to draw over the standard eyes
void drawHappyOverlay() {
  display.fillCircle(28, 86, 40, SSD1306_BLACK);
  display.fillCircle(102, 86, 40, SSD1306_BLACK);
}

void drawMadOverlay() {
  fillEllipse(64, -10, 65, 40, SSD1306_BLACK);
}

void drawSadOverlay() {
  fillEllipse(15, 3, 40, 20, SSD1306_BLACK);
  fillEllipse(115, 3, 40, 20, SSD1306_BLACK);
}

void drawSuspiciousOverlay() {
  display.fillRect(0, 0, 128, 25, SSD1306_BLACK);
  display.fillRect(0, 39, 128, 25, SSD1306_BLACK);
}

// Function to draw only eyelids
void drawClosedEyes() {
  display.clearDisplay();
  display.drawLine(10, 32, 50, 32, SSD1306_WHITE); 
  display.drawLine(78, 32, 118, 32, SSD1306_WHITE);
  display.display();
}

void drawEyes(int position){
  display.fillCircle(28, 32, EYE_RADIUS, SSD1306_WHITE);
  display.fillCircle(102, 32, EYE_RADIUS, SSD1306_WHITE);

  // 1 = Top-left, 2 = Bottom-left, 3 = Top-right, 4 = Bottom-right, Default = middle
  switch(position){
    case 1:
      display.fillCircle(18, 28, PUPIL_RADIUS, SSD1306_BLACK);
      display.fillCircle(92, 28, PUPIL_RADIUS, SSD1306_BLACK);
      fillEllipse(13, 25, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      fillEllipse(87, 25, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      break;

    case 2:
      display.fillCircle(18, 36, PUPIL_RADIUS, SSD1306_BLACK);
      display.fillCircle(92, 36, PUPIL_RADIUS, SSD1306_BLACK);
      fillEllipse(13, 33, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      fillEllipse(87, 33, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      break;

    case 3:
      display.fillCircle(40, 28, PUPIL_RADIUS, SSD1306_BLACK);
      display.fillCircle(112, 28, PUPIL_RADIUS, SSD1306_BLACK);
      fillEllipse(35, 25, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      fillEllipse(107, 25, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      break;

    case 4:
      display.fillCircle(40, 36, PUPIL_RADIUS, SSD1306_BLACK);
      display.fillCircle(112, 36, PUPIL_RADIUS, SSD1306_BLACK);
      fillEllipse(35, 33, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      fillEllipse(107, 33, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      break;

    default:
      display.fillCircle(32, 32, PUPIL_RADIUS, SSD1306_BLACK);
      display.fillCircle(98, 32, PUPIL_RADIUS, SSD1306_BLACK);
      fillEllipse(27, 29, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      fillEllipse(93, 29, PUPIL_SHINE_X, PUPIL_SHINE_Y, SSD1306_WHITE);
      break;
  }
}

bool interruptibleDelay(int totalMs) {
  int step = 50;
  for (int i = 0; i < totalMs; i += step) {
    delay(step);
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      return true; // Interrupted
    }
  }
  return false;
}

bool blinkEyes(int times) {
  for (int i = 0; i < times; i++) {
    drawClosedEyes();
    if (interruptibleDelay(200)) return true;
  }
  return false;
}

void setup() {
  Serial.begin(9600);
  SPI.begin();

  // Init RFID
  rfid.PCD_Init();
  delay(10);

  // Init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed to initialize"));
    while (1);
  }
}

unsigned long lastEyeMoveTime = 0;
unsigned long eyeMoveInterval = 3000;
int currentEyeState = 0;
int remainingMoves = 0;

void loop() {
  // Always check RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    char uidStr[16];
    getUIDString(uidStr);

    if (compareUID(uidStr, UID_BLINK)) {
      blinkEyes(3);
    } else if (compareUID(uidStr, UID_HAPPY)) {
      currentEmotion = HAPPY;
    } else if (compareUID(uidStr, UID_MAD)) {
      currentEmotion = MAD;
    } else if (compareUID(uidStr, UID_SAD)) {
      currentEmotion = SAD;
    } else if (compareUID(uidStr, UID_SUS)) {
      currentEmotion = SUS;
    } else {
      currentEmotion = NORMAL;
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  // Calculate how many times the eyes need to move before blinking
  unsigned long now = millis();
  if (remainingMoves == 0) {
    if (blinkEyes(1)) return;
    remainingMoves = random(3, 5);
    currentEyeState = random(1, 5);
    lastEyeMoveTime = now;
  }

  // Pick a random eye position 
  if (now - lastEyeMoveTime >= eyeMoveInterval) {
    currentEyeState = random(1, 5);
    lastEyeMoveTime = now;
    remainingMoves--;
  }

  // Delete last drawing and draw new eyes, with on top of it the emotion
  display.clearDisplay();
  drawEyes(currentEyeState);

  switch (currentEmotion) {
    case HAPPY:
      drawHappyOverlay();
      break;
    case MAD:
      drawMadOverlay();
      break;
    case SAD:
      drawSadOverlay();
      break;
    case SUS:
      drawSuspiciousOverlay();
      break;
    default:
      break;
  }

  display.display();
}