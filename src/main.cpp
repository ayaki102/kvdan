#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ESP8266WiFi.h>
#include "config.h"

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

const bool debug = true;

// Navigation state - supports nested menus
struct MenuState {
  int index;
  int lastIndex;
  int selected;
  bool inSubmenu;
  MenuState* parent;  // for nesting
};

MenuState rootMenu = {0, 0, -1, false, nullptr};
MenuState* currentMenu = &rootMenu;

// WiFi scan state
int networkCount = 0;
int currentNetwork = 0;
bool scanComplete = false;

// ===== PROTOTYPES =====
void drawHeader(const char* title, int current = -1, int total = -1);
void drawDecorativeLine();
void drawNavigationDots();
void drawSelectionBox(int x, int y, int w, int h);
void slideAnimation(void (*drawCurrent)(int), void (*drawPrevious)(int), 
                    int current, int previous, bool slideRight);

void drawMenu();
void drawSubmenu();
bool buttonPressed(uint8_t pin);
void startupAnimation();
void handleWiFiScan();
void scanAnimation();

void enterSubmenu(int selection);
void exitSubmenu();

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

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
  if (!currentMenu->inSubmenu) {
    if (buttonPressed(BTN_UP)) {
      currentMenu->lastIndex = currentMenu->index;
      currentMenu->index--;
      if (currentMenu->index < 0) currentMenu->index = MENU_SIZE - 1;
      drawMenu();
    }

    if (buttonPressed(BTN_DOWN)) {
      currentMenu->lastIndex = currentMenu->index;
      currentMenu->index++;
      if (currentMenu->index >= MENU_SIZE) currentMenu->index = 0;
      drawMenu();
    }

    if (buttonPressed(BTN_OK)) {
      enterSubmenu(currentMenu->index);
    }
  } else {
    drawSubmenu();
    
    if (buttonPressed(BTN_BACK)) {
      exitSubmenu();
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

// ===== UI COMPONENTS =====

void drawHeader(const char* title, int current, int total) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("["));
  display.setCursor(7, 0);
  display.print(title);
  
  // counter if provided
  if (current >= 0 && total > 0) {
    display.setCursor(SCREEN_WIDTH - 32, 0);
    display.print("<");
    display.print(current);
    display.print("/");
    display.print(total);
    display.print(">");
  }
}

void drawDecorativeLine() {
  display.drawLine(0, 11, SCREEN_WIDTH, 11, SH110X_WHITE);
  display.drawLine(0, 13, SCREEN_WIDTH, 13, SH110X_WHITE);
  display.drawLine(0, 10, 5, 10, SH110X_WHITE);
  display.drawLine(SCREEN_WIDTH - 5, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
  display.drawLine(0, 14, 5, 14, SH110X_WHITE);
  display.drawLine(SCREEN_WIDTH - 5, 14, SCREEN_WIDTH, 14, SH110X_WHITE);
}

void drawNavigationDots() {
  display.setTextSize(1);
  display.setCursor(2, SCREEN_HEIGHT - 8);
  display.print("<");
  display.setCursor(SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8);
  display.print(">");
  display.fillCircle(15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
  display.fillCircle(SCREEN_WIDTH - 15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
}

void drawSelectionBox(int x, int y, int w, int h) {
  // corner brackets only
  display.drawLine(x, y, x + 5, y, SH110X_WHITE);
  display.drawLine(x, y, x, y + 5, SH110X_WHITE);
  display.drawLine(x + w - 5, y, x + w, y, SH110X_WHITE);
  display.drawLine(x + w, y, x + w, y + 5, SH110X_WHITE);
  display.drawLine(x, y + h - 5, x, y + h, SH110X_WHITE);
  display.drawLine(x, y + h, x + 5, y + h, SH110X_WHITE);
  display.drawLine(x + w, y + h - 5, x + w, y + h, SH110X_WHITE);
  display.drawLine(x + w - 5, y + h, x + w, y + h, SH110X_WHITE);
}

// Generic slide animation - pass drawing functions
void slideAnimation(void (*drawCurrent)(int), void (*drawPrevious)(int), 
                    int current, int previous, bool slideRight) {
  for (int offset = 0; offset <= SCREEN_WIDTH; offset += 16) {
    display.clearDisplay();
    
    // current slides in
    int currentX = slideRight ? (-SCREEN_WIDTH + offset) : (SCREEN_WIDTH - offset);
    if (currentX > -SCREEN_WIDTH && currentX < SCREEN_WIDTH) {
      display.setCursor(currentX, 0);
      drawCurrent(currentX);
    }
    
    // previous slides out
    int prevX = slideRight ? offset : -offset;
    if (prevX > -SCREEN_WIDTH && prevX < SCREEN_WIDTH) {
      display.setCursor(prevX, 0);
      drawPrevious(prevX);
    }
    
    display.display();
    delay(20);
  }
}

// ===== MENU NAVIGATION =====

void enterSubmenu(int selection) {
  currentMenu->inSubmenu = true;
  currentMenu->selected = selection;
  
  // reset submenu-specific state
  scanComplete = false;
  networkCount = 0;
  currentNetwork = 0;
  
  drawSubmenu();
}

void exitSubmenu() {
  currentMenu->inSubmenu = false;
  currentMenu->selected = -1;
  scanComplete = false;
  networkCount = 0;
  currentNetwork = 0;
  drawMenu();
}

// ===== MAIN MENU =====

void drawMenuContent(int xOffset) {
  int16_t x1, y1;
  uint16_t w, h;
  int y = 32;
  
  display.setTextSize(1);
  display.getTextBounds(menuItems[currentMenu->index], 0, 0, &x1, &y1, &w, &h);
  int centerX = (SCREEN_WIDTH - w) / 2;
  int x = xOffset + centerX;
  
  int padding = 4;
  drawSelectionBox(x - padding, y - 2, w + padding * 2, h + 4);
  
  display.setCursor(x, y);
  display.print(menuItems[currentMenu->index]);
}

void drawMenu() {
  bool slideRight = (currentMenu->index < currentMenu->lastIndex) || 
                    (currentMenu->lastIndex == 0 && currentMenu->index == MENU_SIZE - 1);
  
  // animation
  for (int offset = 0; offset <= SCREEN_WIDTH; offset += 16) {
    display.clearDisplay();
    
    drawHeader("kajdanecek :3", currentMenu->index + 1, MENU_SIZE);
    drawDecorativeLine();
    
    // current item
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(menuItems[currentMenu->index], 0, 0, &x1, &y1, &w, &h);
    int centerX = (SCREEN_WIDTH - w) / 2;
    int currentX = slideRight ? (-SCREEN_WIDTH + offset + centerX) : (SCREEN_WIDTH - offset + centerX);
    
    if (currentX > -w && currentX < SCREEN_WIDTH) {
      int padding = 4;
      drawSelectionBox(currentX - padding, 30, w + padding * 2, h + 4);
      display.setCursor(currentX, 32);
      display.print(menuItems[currentMenu->index]);
    }
    
    // previous item ghost
    display.getTextBounds(menuItems[currentMenu->lastIndex], 0, 0, &x1, &y1, &w, &h);
    int prevCenterX = (SCREEN_WIDTH - w) / 2;
    int prevX = slideRight ? (offset + prevCenterX) : (-offset + prevCenterX);
    
    if (prevX > -w && prevX < SCREEN_WIDTH) {
      display.setCursor(prevX, 32);
      display.print(menuItems[currentMenu->lastIndex]);
    }
    
    drawNavigationDots();
    display.display();
    delay(20);
  }
  
  // final frame
  display.clearDisplay();
  drawHeader("kajdanecek :3", currentMenu->index + 1, MENU_SIZE);
  drawDecorativeLine();
  
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(menuItems[currentMenu->index], 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int padding = 4;
  
  drawSelectionBox(x - padding, 30, w + padding * 2, h + 4);
  display.setCursor(x, 32);
  display.print(menuItems[currentMenu->index]);
  
  drawNavigationDots();
  display.display();
  
  currentMenu->lastIndex = currentMenu->index;
}

// ===== SUBMENUS =====

void drawSubmenu() {
  display.clearDisplay();

  switch (currentMenu->selected) {
    case wifi_scan_id:
      handleWiFiScan();
      break;
      
    // template for nested submenus
    case 100: {
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print(menuItems[currentMenu->index]);
      drawDecorativeLine();

      display.setTextSize(2);
      display.setCursor(8, 30);
      display.print(menuItems[currentMenu->index]);

      display.setTextSize(1);
      display.setCursor(20, 55);
      display.print(F("[BACK] POWROT"));
      break;
    }
  }

  display.display();
}

// ===== WiFi SCANNER =====

void drawWiFiNetwork(int xOffset, int networkIdx) {
  display.setCursor(xOffset + 2, 18);
  display.println("Nazwa:");
  display.setCursor(xOffset + 2, 28);
  display.println(WiFi.SSID(networkIdx));
  
  display.setCursor(xOffset + 2, 40);
  display.print("Sygnal: ");
  display.print(WiFi.RSSI(networkIdx));
  display.println(" dBm");
  
  display.setCursor(xOffset + 2, 50);
  display.print("Haslo: ");
  display.println((WiFi.encryptionType(networkIdx) == ENC_TYPE_NONE) ? "Brak" : "Jest");
}

void handleWiFiScan() {
  static int lastNetwork = 0;
  
  if (!scanComplete) {
    scanAnimation();
    networkCount = WiFi.scanNetworks();
    scanComplete = true;
    currentNetwork = 0;
    lastNetwork = 0;
  }
  
  bool needsAnimation = false;
  bool slideRight = false;
  
  if (buttonPressed(BTN_UP)) {
    lastNetwork = currentNetwork;
    currentNetwork = (currentNetwork - 1 + networkCount) % networkCount;
    needsAnimation = true;
    slideRight = true;
  }
  
  if (buttonPressed(BTN_DOWN)) {
    lastNetwork = currentNetwork;
    currentNetwork = (currentNetwork + 1) % networkCount;
    needsAnimation = true;
    slideRight = false;
  }
  
  // slide between networks
  if (needsAnimation && networkCount > 0) {
    for (int offset = 0; offset <= SCREEN_WIDTH; offset += 16) {
      display.clearDisplay();
      
      drawHeader("WiFi", currentNetwork + 1, networkCount);
      drawDecorativeLine();
      
      int currentX = slideRight ? (-SCREEN_WIDTH + offset) : (SCREEN_WIDTH - offset);
      if (currentX > -SCREEN_WIDTH && currentX < SCREEN_WIDTH) {
        drawWiFiNetwork(currentX, currentNetwork);
      }
      
      int prevX = slideRight ? offset : -offset;
      if (prevX > -SCREEN_WIDTH && prevX < SCREEN_WIDTH) {
        drawWiFiNetwork(prevX, lastNetwork);
      }
      
      drawNavigationDots();
      display.display();
      delay(20);
    }
  }
  
  // final frame
  display.clearDisplay();
  
  if (networkCount == 0) {
    drawHeader("Brak WiFi");
    drawDecorativeLine();
    
    display.setCursor(20, 28);
    display.println("brak sieci...");
    display.setCursor(48, 42);
    display.print("(>_<)");
  } else {
    drawHeader("WiFi", currentNetwork + 1, networkCount);
    drawDecorativeLine();
    drawWiFiNetwork(0, currentNetwork);
    drawNavigationDots();
  }
  
  display.display();
}

void scanAnimation() {
  for (int frame = 0; frame < 15; frame++) {
    display.clearDisplay();
    
    drawHeader("SKANUJE");
    display.setCursor(SCREEN_WIDTH - 30, 0);
    display.print((frame * 100) / 15);
    display.print("%");
    
    drawDecorativeLine();
    
    // bouncing dots
    int dotSpacing = 12;
    int startX = 52;
    int baseY = 30;
    
    for (int i = 0; i < 3; i++) {
      float jumpPhase = (frame + i * 2) * 0.6;
      int jumpHeight = abs(sin(jumpPhase)) * 8;
      display.fillCircle(startX + (i * dotSpacing), baseY - jumpHeight, 3, SH110X_WHITE);
    }
    
    // progress bar
    int barWidth = (frame * 106) / 15;
    display.drawRect(10, 48, 108, 10, SH110X_WHITE);
    
    // corner accents
    drawSelectionBox(10, 48, 108, 10);
    display.fillRect(11, 49, barWidth, 8, SH110X_WHITE);
    
    display.display();
    delay(120);
  }
  
  // success screen
  display.clearDisplay();
  drawHeader("SUKCES");
  drawDecorativeLine();
  
  display.setCursor(38, 28);
  display.print("Koniec!");
  display.setCursor(42, 42);
  display.print("(^_^)");
  
  drawSelectionBox(35, 40, 52, 12);
  
  display.display();
  delay(500);
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
