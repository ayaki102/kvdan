#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "config.h"

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

const bool debug = true;
int menuIndex = 0;
bool inSubmenu = false;


const uint8_t heartBitmap[] PROGMEM = {
  0b01100110,
  0b11111111,
  0b11111111,
  0b11111111,
  0b01111110,
  0b00111100,
  0b00011000,
  0b00000000
};

String menuItems[] = {
  "SKAN SIECI WIFI",
  "MONITOR AP",
  "DEAUTORYZACJA",
  "EVIL TWIN",
  "BEACON SPAM",
  "SNIFFING",
  "USTAWIENIA",
  "INFO"
};

const int MENU_SIZE = sizeof(menuItems) / sizeof(menuItems[0]);

// ===== PROTOTYPES =====
void drawMenu();
void drawSubmenu();
bool buttonPressed(uint8_t pin);
void startupAnimation();


void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);

  display.begin(SCREEN_ADDRESS, true);
  display.clearDisplay();

  display.setTextColor(SH110X_WHITE);
  display.setTextWrap(false);

  display.display();

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  if (!debug) {
      startupAnimation();
  }

  drawMenu();
}


void loop() {

  if (!inSubmenu) {

    if (buttonPressed(BTN_UP)) {
      menuIndex--;
      if (menuIndex < 0) menuIndex = MENU_SIZE - 1;
      drawMenu();
    }

    if (buttonPressed(BTN_DOWN)) {
      menuIndex++;
      if (menuIndex >= MENU_SIZE) menuIndex = 0;
      drawMenu();
    }

    if (buttonPressed(BTN_OK)) {
      inSubmenu = true;
      drawSubmenu();
    }

  } else {

    if (buttonPressed(BTN_BACK)) {
      inSubmenu = false;
      drawMenu();
    }

  }

  delay(10);
}

bool buttonPressed(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    if (millis() - lastDebounceTime > debounceDelay) {
      lastDebounceTime = millis();
      return true;
    }
  }
  return false;
}

// ===== DRAW MAIN MENU =====
void drawMenu() {
  display.clearDisplay();

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("kajdanecek :3"));
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SH110X_WHITE);

  // Centered menu item
  display.setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(menuItems[menuIndex], 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = 30;

  display.setCursor(x, y);
  display.print(menuItems[menuIndex]);

  display.display();
}

void drawSubmenu() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("TRYB (NIEAKTYWNY)"));
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SH110X_WHITE);

  display.setTextSize(2);
  display.setCursor(8, 30);
  display.print(menuItems[menuIndex]);

  display.setTextSize(1);
  display.setCursor(20, 55);
  display.print(F("[BACK] POWROT"));

  display.display();
}


void startupAnimation() {
  for (int frame = 0; frame < 8; frame++) {
    display.clearDisplay();

    display.drawBitmap(20, 50 - frame * 4, heartBitmap, 8, 8, SH110X_WHITE);
    display.drawBitmap(90, 55 - frame * 3, heartBitmap, 8, 8, SH110X_WHITE);
    display.drawBitmap(60, 52 - frame * 5, heartBitmap, 8, 8, SH110X_WHITE);

    display.setTextSize(1);
    display.setCursor(28, 5);
    display.print(F("kajdanek :3"));

    display.display();
    delay(180);
  }

  delay(300);
  display.clearDisplay();
  display.display();
}

