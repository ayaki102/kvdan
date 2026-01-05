#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <ESP8266WiFi.h>
#include "config.h"

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

const bool debug = false;

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

void submenuEnterAnimation() {
  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2;
  
  // explosive zoom + particles
  for (int frame = 0; frame < 12; frame++) {
    display.clearDisplay();
    
    // expanding circle ripples
    int rippleRadius = frame * 12;
    if (rippleRadius < SCREEN_WIDTH) {
      display.drawCircle(centerX, centerY, rippleRadius, SH110X_WHITE);
      if (frame > 2) {
        display.drawCircle(centerX, centerY, rippleRadius - 8, SH110X_WHITE);
      }
    }
    
    // corner brackets explode outward
    int cornerDist = frame * 8;
    int cornerSize = 6;
    
    // top-left
    display.drawLine(cornerDist, cornerDist, cornerDist + cornerSize, cornerDist, SH110X_WHITE);
    display.drawLine(cornerDist, cornerDist, cornerDist, cornerDist + cornerSize, SH110X_WHITE);
    
    // top-right
    display.drawLine(SCREEN_WIDTH - cornerDist, cornerDist, 
                     SCREEN_WIDTH - cornerDist - cornerSize, cornerDist, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - cornerDist, cornerDist, 
                     SCREEN_WIDTH - cornerDist, cornerDist + cornerSize, SH110X_WHITE);
    
    // bottom-left
    display.drawLine(cornerDist, SCREEN_HEIGHT - cornerDist, 
                     cornerDist + cornerSize, SCREEN_HEIGHT - cornerDist, SH110X_WHITE);
    display.drawLine(cornerDist, SCREEN_HEIGHT - cornerDist, 
                     cornerDist, SCREEN_HEIGHT - cornerDist - cornerSize, SH110X_WHITE);
    
    // bottom-right
    display.drawLine(SCREEN_WIDTH - cornerDist, SCREEN_HEIGHT - cornerDist, 
                     SCREEN_WIDTH - cornerDist - cornerSize, SCREEN_HEIGHT - cornerDist, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - cornerDist, SCREEN_HEIGHT - cornerDist, 
                     SCREEN_WIDTH - cornerDist, SCREEN_HEIGHT - cornerDist - cornerSize, SH110X_WHITE);
    
    // particle burst
    for (int i = 0; i < 8; i++) {
      float angle = (i * 45) * (PI / 180.0);
      int particleDist = frame * 6;
      int px = centerX + cos(angle) * particleDist;
      int py = centerY + sin(angle) * particleDist;
      
      if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
        display.fillCircle(px, py, 2, SH110X_WHITE);
        // trailing particles
        if (frame > 3) {
          int px2 = centerX + cos(angle) * (particleDist - 12);
          int py2 = centerY + sin(angle) * (particleDist - 12);
          if (px2 >= 0 && px2 < SCREEN_WIDTH && py2 >= 0 && py2 < SCREEN_HEIGHT) {
            display.drawPixel(px2, py2, SH110X_WHITE);
          }
        }
      }
    }
    
    display.display();
    delay(40);
  }
  
  // flash effect
  display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
  display.display();
  delay(50);
  display.clearDisplay();
  display.display();
  delay(30);
}

void enterSubmenu(int selection) {
  currentMenu->inSubmenu = true;
  currentMenu->selected = selection;
  
  submenuEnterAnimation();
  
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
  int centerX = SCREEN_WIDTH / 2;
  int centerY = SCREEN_HEIGHT / 2;
  
  // sleeping hamster wakes up
  for (int frame = 0; frame < 22; frame++) {
    display.clearDisplay();
    
    if (frame < 6) {
      // hamster sleeping - curled up ball
      int breathe = abs(sin(frame * 1.2)) * 2;
      
      // body (curled)
      display.fillCircle(centerX, centerY + breathe, 10, SH110X_WHITE);
      display.fillCircle(centerX - 3, centerY - 3 + breathe, 6, SH110X_WHITE);
      
      // closed eyes zzz
      display.drawLine(centerX - 2, centerY - 2 + breathe, centerX - 4, centerY - 2 + breathe, SH110X_BLACK);
      display.drawLine(centerX + 2, centerY - 2 + breathe, centerX + 4, centerY - 2 + breathe, SH110X_BLACK);
      
      // zzz floating up
      if (frame > 2) {
        display.setTextSize(1);
        display.setCursor(centerX + 15, centerY - 12 - frame);
        display.print("z");
      }
      
    } else if (frame < 12) {
      // waking up - stretching
      int stretchPhase = frame - 6;
      
      // body starts uncurling
      display.fillCircle(centerX, centerY, 9, SH110X_WHITE);
      display.fillCircle(centerX - 4, centerY - 4, 5, SH110X_WHITE);
      
      // ears pop up
      if (stretchPhase > 2) {
        display.fillCircle(centerX - 7, centerY - 10, 3, SH110X_WHITE);
        display.fillCircle(centerX - 1, centerY - 11, 3, SH110X_WHITE);
      }
      
      // eyes opening
      if (stretchPhase > 1) {
        display.drawPixel(centerX - 3, centerY - 3, SH110X_BLACK);
        display.drawPixel(centerX + 1, centerY - 3, SH110X_BLACK);
      }
      
      // little paws stretching out
      if (stretchPhase > 4) {
        display.fillCircle(centerX - 12, centerY + 3, 2, SH110X_WHITE);
        display.fillCircle(centerX + 8, centerY + 3, 2, SH110X_WHITE);
      }
      
    } else {
      // fully awake and happy
      int wigglePhase = frame - 12;
      int wiggle = (wigglePhase % 2 == 0) ? 1 : -1;
      
      // body
      display.fillCircle(centerX + wiggle, centerY, 9, SH110X_WHITE);
      display.fillCircle(centerX - 4 + wiggle, centerY - 4, 5, SH110X_WHITE);
      
      // ears
      display.fillCircle(centerX - 7 + wiggle, centerY - 10, 3, SH110X_WHITE);
      display.fillCircle(centerX - 1 + wiggle, centerY - 11, 3, SH110X_WHITE);
      
      // happy eyes (^_^)
      display.drawLine(centerX - 4 + wiggle, centerY - 3, centerX - 2 + wiggle, centerY - 3, SH110X_BLACK);
      display.drawLine(centerX + wiggle, centerY - 3, centerX + 2 + wiggle, centerY - 3, SH110X_BLACK);
      
      // nose
      display.drawPixel(centerX - 1 + wiggle, centerY - 1, SH110X_BLACK);
      
      // paws
      display.fillCircle(centerX - 10 + wiggle, centerY + 5, 2, SH110X_WHITE);
      display.fillCircle(centerX + 6 + wiggle, centerY + 5, 2, SH110X_WHITE);
      
      // cheeks
      display.fillCircle(centerX - 8 + wiggle, centerY, 3, SH110X_WHITE);
      display.fillCircle(centerX + 4 + wiggle, centerY, 3, SH110X_WHITE);
      
      // hearts appear
      if (wigglePhase > 4) {
        display.drawBitmap(centerX - 25, centerY - 8, heartBitmap, 8, 8, SH110X_WHITE);
        display.drawBitmap(centerX + 15, centerY - 8, heartBitmap, 8, 8, SH110X_WHITE);
      }
      
      // sparkle effect
      if (wigglePhase > 6) {
        display.drawPixel(centerX - 20, centerY - 15, SH110X_WHITE);
        display.drawPixel(centerX + 18, centerY - 15, SH110X_WHITE);
        display.drawPixel(centerX, centerY - 20, SH110X_WHITE);
      }
    }
    
    display.display();
    delay(frame < 6 ? 200 : (frame < 12 ? 120 : 100));
  }
  
  delay(300);
  
  // text reveals letter by letter from center outward
  const char* text = "kajdanek :3";
  int textLen = strlen(text);
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(1);
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int textX = (SCREEN_WIDTH - w) / 2;
  int textY = centerY - 4;
  
  // sparkle burst before text
  for (int burst = 0; burst < 8; burst++) {
    display.clearDisplay();
    
    for (int i = 0; i < 12; i++) {
      float angle = (i * 30) * (PI / 180.0);
      int dist = burst * 8;
      int px = centerX + cos(angle) * dist;
      int py = centerY + sin(angle) * dist;
      
      if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
        display.fillCircle(px, py, 1, SH110X_WHITE);
      }
    }
    
    display.display();
    delay(35);
  }
  
  // letters appear from center outward with pop effect
  int centerChar = textLen / 2;
  for (int reveal = 0; reveal <= centerChar; reveal++) {
    display.clearDisplay();
    
    // draw revealed characters with bounce
    int bounce = (reveal == 0) ? 0 : 2;
    
    for (int i = 0; i < textLen; i++) {
      int distFromCenter = abs(i - centerChar);
      if (distFromCenter <= reveal) {
        int charX = textX + i * 6;
        int charY = (distFromCenter == reveal) ? textY - bounce : textY;
        display.setCursor(charX, charY);
        display.print(text[i]);
      }
    }
    
    // hearts pop in with text
    if (reveal > centerChar - 2) {
      display.drawBitmap(textX - 12, textY, heartBitmap, 8, 8, SH110X_WHITE);
      display.drawBitmap(textX + w + 4, textY, heartBitmap, 8, 8, SH110X_WHITE);
    }
    
    display.display();
    delay(80);
  }
  
  // final pulse effect
  for (int pulse = 0; pulse < 3; pulse++) {
    display.clearDisplay();
    
    if (pulse % 2 == 0) {
      display.setCursor(textX, textY);
      display.print(text);
      display.drawBitmap(textX - 12, textY, heartBitmap, 8, 8, SH110X_WHITE);
      display.drawBitmap(textX + w + 4, textY, heartBitmap, 8, 8, SH110X_WHITE);
    }
    
    display.display();
    delay(150);
  }
  
  // final display
  display.clearDisplay();
  display.setCursor(textX, textY);
  display.print(text);
  display.drawBitmap(textX - 12, textY, heartBitmap, 8, 8, SH110X_WHITE);
  display.drawBitmap(textX + w + 4, textY, heartBitmap, 8, 8, SH110X_WHITE);
  display.display();
  
  delay(500);
  display.clearDisplay();
  display.display();
}
