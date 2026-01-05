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
int menuIndex = 0;
int selected;
bool inSubmenu = false;

int networkCount = 0;
int currentNetwork = 0;
bool scanComplete = false;



// ===== PROTOTYPES =====
void drawMenu();
void drawSubmenu();
bool buttonPressed(uint8_t pin);
void startupAnimation();
void scan_nearby_wifi();
void scan_animation();


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
      selected = menuIndex;
      drawSubmenu();
    }

  } else {
    // keep calling drawSubmenu to handle button presses inside submenus
    drawSubmenu();
    
    if (buttonPressed(BTN_BACK)) {
        inSubmenu = false;
        // Reset wifi scan state
        scanComplete = false;
        networkCount = 0;
        currentNetwork = 0;
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
  static int lastMenuIndex = 0;
  bool isMovingRight = (menuIndex < lastMenuIndex) || (lastMenuIndex == 0 && menuIndex == MENU_SIZE - 1);
  
  for (int offset = 0; offset <= SCREEN_WIDTH; offset += 16) {
    display.clearDisplay();
    
    display.setTextSize(1);
    
    display.setCursor(0, 0);
    display.print(F("["));
    display.setCursor(7, 0);
    display.print(F("kajdanecek"));
    display.setCursor(73, 0);
    display.print(F(":3"));
    display.setCursor(85, 0);
    display.print(F("]"));
    
    display.setCursor(SCREEN_WIDTH - 32, 0);
    display.print("<");
    display.print(menuIndex + 1);
    display.print("/");
    display.print(MENU_SIZE);
    display.print(">");
    
    display.drawLine(0, 11, SCREEN_WIDTH, 11, SH110X_WHITE);
    display.drawLine(0, 13, SCREEN_WIDTH, 13, SH110X_WHITE);
    
    display.drawLine(0, 10, 5, 10, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - 5, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
    display.drawLine(0, 14, 5, 14, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - 5, 14, SCREEN_WIDTH, 14, SH110X_WHITE);
    
    int16_t x1, y1;
    uint16_t w, h;
    int y = 32;
    
    display.setTextSize(1);
    display.getTextBounds(menuItems[menuIndex], 0, 0, &x1, &y1, &w, &h);
    int centerX = (SCREEN_WIDTH - w) / 2;
    int currentX = isMovingRight ? 
                   (-SCREEN_WIDTH + offset + centerX) : 
                   (SCREEN_WIDTH - offset + centerX);
    
    int boxPadding = 4;
    int boxX = currentX - boxPadding;
    int boxY = y - 2;
    int boxW = w + (boxPadding * 2);
    int boxH = h + 4;
    
    // draw fancy selection box with corners
    if (currentX > -w && currentX < SCREEN_WIDTH) {
      // corner brackets
      display.drawLine(boxX, boxY, boxX + 5, boxY, SH110X_WHITE);
      display.drawLine(boxX, boxY, boxX, boxY + 5, SH110X_WHITE);
      
      display.drawLine(boxX + boxW - 5, boxY, boxX + boxW, boxY, SH110X_WHITE);
      display.drawLine(boxX + boxW, boxY, boxX + boxW, boxY + 5, SH110X_WHITE);
      
      display.drawLine(boxX, boxY + boxH - 5, boxX, boxY + boxH, SH110X_WHITE);
      display.drawLine(boxX, boxY + boxH, boxX + 5, boxY + boxH, SH110X_WHITE);
      
      display.drawLine(boxX + boxW, boxY + boxH - 5, boxX + boxW, boxY + boxH, SH110X_WHITE);
      display.drawLine(boxX + boxW - 5, boxY + boxH, boxX + boxW, boxY + boxH, SH110X_WHITE);
      
      // menu text
      display.setCursor(currentX, y);
      display.print(menuItems[menuIndex]);
    }
    
    // previous item sliding out (ghost effect)
    display.getTextBounds(menuItems[lastMenuIndex], 0, 0, &x1, &y1, &w, &h);
    int prevCenterX = (SCREEN_WIDTH - w) / 2;
    int prevX = isMovingRight ? 
                (offset + prevCenterX) : 
                (-offset + prevCenterX);
    
    if (prevX > -w && prevX < SCREEN_WIDTH) {
      display.setCursor(prevX, y);
      display.print(menuItems[lastMenuIndex]);
    }
    
    // navigation arrows at bottom
    display.setTextSize(1);
    display.setCursor(2, SCREEN_HEIGHT - 8);
    display.print("<");
    display.setCursor(SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8);
    display.print(">");
    
    // pixel dots for style
    display.fillCircle(15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
    display.fillCircle(SCREEN_WIDTH - 15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
    
    display.display();
    delay(20);
  }
  
  display.clearDisplay();
  
  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("["));
  display.setCursor(7, 0);
  display.print(F("kajdanecek"));
  display.setCursor(73, 0);
  display.print(F(":3"));
  display.setCursor(85, 0);
  display.print(F("]"));
  
  display.setCursor(SCREEN_WIDTH - 32, 0);
  display.print("<");
  display.print(menuIndex + 1);
  display.print("/");
  display.print(MENU_SIZE);
  display.print(">");
  
  display.drawLine(0, 11, SCREEN_WIDTH, 11, SH110X_WHITE);
  display.drawLine(0, 13, SCREEN_WIDTH, 13, SH110X_WHITE);
  display.drawLine(0, 10, 5, 10, SH110X_WHITE);
  display.drawLine(SCREEN_WIDTH - 5, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
  display.drawLine(0, 14, 5, 14, SH110X_WHITE);
  display.drawLine(SCREEN_WIDTH - 5, 14, SCREEN_WIDTH, 14, SH110X_WHITE);
  
  // menu item with selection box
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(menuItems[menuIndex], 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = 32;
  
  int boxPadding = 4;
  int boxX = x - boxPadding;
  int boxY = y - 2;
  int boxW = w + (boxPadding * 2);
  int boxH = h + 4;
  
  // Corner brackets
  display.drawLine(boxX, boxY, boxX + 5, boxY, SH110X_WHITE);
  display.drawLine(boxX, boxY, boxX, boxY + 5, SH110X_WHITE);
  display.drawLine(boxX + boxW - 5, boxY, boxX + boxW, boxY, SH110X_WHITE);
  display.drawLine(boxX + boxW, boxY, boxX + boxW, boxY + 5, SH110X_WHITE);
  display.drawLine(boxX, boxY + boxH - 5, boxX, boxY + boxH, SH110X_WHITE);
  display.drawLine(boxX, boxY + boxH, boxX + 5, boxY + boxH, SH110X_WHITE);
  display.drawLine(boxX + boxW, boxY + boxH - 5, boxX + boxW, boxY + boxH, SH110X_WHITE);
  display.drawLine(boxX + boxW - 5, boxY + boxH, boxX + boxW, boxY + boxH, SH110X_WHITE);
  
  display.setCursor(x, y);
  display.print(menuItems[menuIndex]);
  
  // bottom elements
  display.setTextSize(1);
  display.setCursor(2, SCREEN_HEIGHT - 8);
  display.print("<");
  display.setCursor(SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8);
  display.print(">");
  display.fillCircle(15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
  display.fillCircle(SCREEN_WIDTH - 15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
  
  display.display();
  
  lastMenuIndex = menuIndex;
}

void drawSubmenu() {
  display.clearDisplay();


  switch (selected) {

    case wifi_scan_id: {
      scan_nearby_wifi();
      break;
    }

  case 100: {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(menuItems[menuIndex]);
    display.drawLine(0, 12, SCREEN_WIDTH, 12, SH110X_WHITE);

    display.setTextSize(2);
    display.setCursor(8, 30);
    display.print(menuItems[menuIndex]);

    display.setTextSize(1);
    display.setCursor(20, 55);
    display.print(F("[BACK] POWROT"));
    break;
  }
}



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

void scan_nearby_wifi(){
  display.setTextSize(1);
  static int lastNetwork = 0;
  
  if (!scanComplete) {
    scan_animation();
    
    networkCount = WiFi.scanNetworks();
    scanComplete = true;
    currentNetwork = 0;
    lastNetwork = 0;
  }
 
  bool buttonUp = buttonPressed(BTN_UP);
  bool buttonDown = buttonPressed(BTN_DOWN);
  bool needsAnimation = false;
  bool slideRight = false;
  
  if (buttonUp) {
    lastNetwork = currentNetwork;
    currentNetwork--;
    if (currentNetwork < 0) {
      currentNetwork = networkCount - 1; 
    }
    needsAnimation = true;
    slideRight = true;
  }
  if (buttonDown) {
    lastNetwork = currentNetwork;
    currentNetwork++;
    if (currentNetwork >= networkCount) {
      currentNetwork = 0; 
    }
    needsAnimation = true;
    slideRight = false;
  }
  
  // slide animation
  if (needsAnimation && networkCount > 0) {
    for (int offset = 0; offset <= SCREEN_WIDTH; offset += 16) {
      display.clearDisplay();
      
      // static header
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("[WiFi: ");
      display.print(currentNetwork + 1);
      display.print("/");
      display.print(networkCount);
      display.print("]");
      
      display.drawLine(0, 11, SCREEN_WIDTH, 11, SH110X_WHITE);
      display.drawLine(0, 13, SCREEN_WIDTH, 13, SH110X_WHITE);
      display.drawLine(0, 10, 5, 10, SH110X_WHITE);
      display.drawLine(SCREEN_WIDTH - 5, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
      display.drawLine(0, 14, 5, 14, SH110X_WHITE);
      display.drawLine(SCREEN_WIDTH - 5, 14, SCREEN_WIDTH, 14, SH110X_WHITE);
      
      // ==== CURRENT NETWORK SLIDING IN ====
      int currentX = slideRight ? 
                     (-SCREEN_WIDTH + offset) : 
                     (SCREEN_WIDTH - offset);
      
      if (currentX > -SCREEN_WIDTH && currentX < SCREEN_WIDTH) {
        display.setCursor(currentX + 2, 18);
        display.println("Nazwa:");
        display.setCursor(currentX + 2, 28);
        display.println(WiFi.SSID(currentNetwork));
        
        display.setCursor(currentX + 2, 40);
        display.print("Sygnal: ");
        display.print(WiFi.RSSI(currentNetwork));
        display.println(" dBm");
        
        display.setCursor(currentX + 2, 50);
        display.print("Haslo: ");
        display.println(
          (WiFi.encryptionType(currentNetwork) == ENC_TYPE_NONE) ? "Brak" : "Jest"
        );
      }
      
      // ==== PREVIOUS NETWORK SLIDING OUT ====
      int prevX = slideRight ? 
                  offset : 
                  -offset;
      
      if (prevX > -SCREEN_WIDTH && prevX < SCREEN_WIDTH) {
        display.setCursor(prevX + 2, 18);
        display.println("Nazwa:");
        display.setCursor(prevX + 2, 28);
        display.println(WiFi.SSID(lastNetwork));
        
        display.setCursor(prevX + 2, 40);
        display.print("Sygnal: ");
        display.print(WiFi.RSSI(lastNetwork));
        display.println(" dBm");
        
        display.setCursor(prevX + 2, 50);
        display.print("Haslo: ");
        display.println(
          (WiFi.encryptionType(lastNetwork) == ENC_TYPE_NONE) ? "Brak" : "Jest"
        );
      }
      
      // navigation arrows
      display.setCursor(2, SCREEN_HEIGHT - 8);
      display.print("<");
      display.setCursor(SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8);
      display.print(">");
      display.fillCircle(15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
      display.fillCircle(SCREEN_WIDTH - 15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
      
      display.display();
      delay(20);
    }
  }
  
  display.clearDisplay();
  display.setCursor(0, 0);
  
  if (networkCount == 0) {
    display.setCursor(0, 0);
    display.print("[Brak WiFi]");
    
    display.drawLine(0, 11, SCREEN_WIDTH, 11, SH110X_WHITE);
    display.drawLine(0, 13, SCREEN_WIDTH, 13, SH110X_WHITE);
    display.drawLine(0, 10, 5, 10, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - 5, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
    display.drawLine(0, 14, 5, 14, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - 5, 14, SCREEN_WIDTH, 14, SH110X_WHITE);
    
    display.setCursor(20, 28);
    display.println("brak sieci...");
    display.setCursor(48, 42);
    display.print("(>_<)");
  } else {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("[WiFi: ");
    display.print(currentNetwork + 1);
    display.print("/");
    display.print(networkCount);
    display.print("]");
    
    display.drawLine(0, 11, SCREEN_WIDTH, 11, SH110X_WHITE);
    display.drawLine(0, 13, SCREEN_WIDTH, 13, SH110X_WHITE);
    display.drawLine(0, 10, 5, 10, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - 5, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
    display.drawLine(0, 14, 5, 14, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - 5, 14, SCREEN_WIDTH, 14, SH110X_WHITE);
    
    display.setCursor(2, 18);
    display.println("Nazwa:");
    display.setCursor(2, 28);
    display.println(WiFi.SSID(currentNetwork));
    
    display.setCursor(2, 40);
    display.print("Sygnal: ");
    display.print(WiFi.RSSI(currentNetwork));
    display.println(" dBm");
    
    display.setCursor(2, 50);
    display.print("Haslo: ");
    display.println(
      (WiFi.encryptionType(currentNetwork) == ENC_TYPE_NONE) ? "Brak" : "Jest"
    );
    
    display.setCursor(2, SCREEN_HEIGHT - 8);
    display.print("<");
    display.setCursor(SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8);
    display.print(">");
    display.fillCircle(15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
    display.fillCircle(SCREEN_WIDTH - 15, SCREEN_HEIGHT - 4, 1, SH110X_WHITE);
  }
  
  display.display();
}

void scan_animation(){
    for (int frame = 0; frame < 15; frame++) {
      display.clearDisplay();
      
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print(F("["));
      display.setCursor(7, 0);
      display.print(F("SKANUJE"));
      display.setCursor(55, 0);
      display.print(F("]"));
      
      display.setCursor(SCREEN_WIDTH - 30, 0);
      display.print((frame * 100) / 15);
      display.print("%");
      
      display.drawLine(0, 11, SCREEN_WIDTH, 11, SH110X_WHITE);
      display.drawLine(0, 13, SCREEN_WIDTH, 13, SH110X_WHITE);
      display.drawLine(0, 10, 5, 10, SH110X_WHITE);
      display.drawLine(SCREEN_WIDTH - 5, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
      display.drawLine(0, 14, 5, 14, SH110X_WHITE);
      display.drawLine(SCREEN_WIDTH - 5, 14, SCREEN_WIDTH, 14, SH110X_WHITE);
      
      int dotSpacing = 12;
      int startX = 52;
      int baseY = 30;
      
      for (int i = 0; i < 3; i++) {
        float jumpPhase = (frame + i * 2) * 0.6;
        int jumpHeight = abs(sin(jumpPhase)) * 8;
        
        int dotX = startX + (i * dotSpacing);
        int dotY = baseY - jumpHeight;
        
        display.fillCircle(dotX, dotY, 3, SH110X_WHITE);
      }
      
      int barWidth = (frame * 106) / 15;
      
      display.drawRect(10, 48, 108, 10, SH110X_WHITE);
      
      display.drawLine(10, 48, 13, 48, SH110X_WHITE);
      display.drawLine(10, 48, 10, 51, SH110X_WHITE);
      display.drawLine(115, 48, 118, 48, SH110X_WHITE);
      display.drawLine(118, 48, 118, 51, SH110X_WHITE);
      display.drawLine(10, 58, 13, 58, SH110X_WHITE);
      display.drawLine(10, 55, 10, 58, SH110X_WHITE);
      display.drawLine(115, 58, 118, 58, SH110X_WHITE);
      display.drawLine(118, 55, 118, 58, SH110X_WHITE);
      
      display.fillRect(11, 49, barWidth, 8, SH110X_WHITE);
      
      display.display();
      delay(120);
    }
    
    display.clearDisplay();
    
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("["));
    display.setCursor(7, 0);
    display.print(F("SUKCES"));
    display.setCursor(49, 0);
    display.print(F("]"));
    
    display.drawLine(0, 11, SCREEN_WIDTH, 11, SH110X_WHITE);
    display.drawLine(0, 13, SCREEN_WIDTH, 13, SH110X_WHITE);
    display.drawLine(0, 10, 5, 10, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - 5, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
    display.drawLine(0, 14, 5, 14, SH110X_WHITE);
    display.drawLine(SCREEN_WIDTH - 5, 14, SCREEN_WIDTH, 14, SH110X_WHITE);
    
    display.setTextSize(1);
    display.setCursor(38, 28);
    display.print("Koniec!");
    
    display.setCursor(42, 42);
    display.print("(^_^)");
    
    display.drawLine(35, 40, 40, 40, SH110X_WHITE);
    display.drawLine(35, 40, 35, 45, SH110X_WHITE);
    display.drawLine(82, 40, 87, 40, SH110X_WHITE);
    display.drawLine(87, 40, 87, 45, SH110X_WHITE);
    display.drawLine(35, 52, 40, 52, SH110X_WHITE);
    display.drawLine(35, 47, 35, 52, SH110X_WHITE);
    display.drawLine(82, 52, 87, 52, SH110X_WHITE);
    display.drawLine(87, 47, 87, 52, SH110X_WHITE);
    
    display.display();
    delay(500);
}
