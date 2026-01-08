#include "config.h"
#include "ui/ui.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

const bool debug = false;

// navigation state - supports nested menus
struct MenuState {
    int index;
    int lastIndex;
    int selected;
    bool inSubmenu;
    MenuState *parent; // for nesting
};

MenuState rootMenu = {0, 0, -1, false, nullptr};
MenuState *currentMenu = &rootMenu;

// WiFi scan state
int networkCount = 0;
int currentNetwork = 0;
bool scanComplete = false;
String selectedAP = "";

// this is just usefull
bool needsAnimation = false;
bool slideRight = false;

// ===== PROTOTYPES =====

void drawMenu();
void drawSubmenu();
bool buttonPressed(uint8_t pin);
void startupAnimation();
void handleWiFiScan();
void handleDeauth();

void enterSubmenu(int selection);
void exitSubmenu();

// PROTOTYPES END

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
            if (currentMenu->index < 0)
                currentMenu->index = MENU_SIZE - 1;
            drawMenu();
        }

        if (buttonPressed(BTN_DOWN)) {
            currentMenu->lastIndex = currentMenu->index;
            currentMenu->index++;
            if (currentMenu->index >= MENU_SIZE)
                currentMenu->index = 0;
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

// ===== MENU NAVIGATION =====
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
    display.getTextBounds(menuItems[currentMenu->index], 0, 0, &x1, &y1, &w,
                          &h);
    int centerX = (SCREEN_WIDTH - w) / 2;
    int x = xOffset + centerX;

    int padding = 4;
    drawSelectionBox(x - padding, y - 2, w + padding * 2, h + 4);

    display.setCursor(x, y);
    display.print(menuItems[currentMenu->index]);
}

void drawMenu() {
    bool slideRight =
        (currentMenu->index < currentMenu->lastIndex) ||
        (currentMenu->lastIndex == 0 && currentMenu->index == MENU_SIZE - 1);

    // animation
    for (int offset = 0; offset <= SCREEN_WIDTH; offset += 16) {
        display.clearDisplay();

        drawHeader("kajdanecek :3", currentMenu->index + 1, MENU_SIZE);
        // drawDecorativeLine(); // current item
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(menuItems[currentMenu->index], 0, 0, &x1, &y1, &w,
                              &h);
        int centerX = (SCREEN_WIDTH - w) / 2;
        int currentX = slideRight ? (-SCREEN_WIDTH + offset + centerX)
                                  : (SCREEN_WIDTH - offset + centerX);

        if (currentX > -w && currentX < SCREEN_WIDTH) {
            int padding = 4;
            drawSelectionBox(currentX - padding, 30, w + padding * 2, h + 4);
            display.setCursor(currentX, 32);
            display.print(menuItems[currentMenu->index]);
        }

        // previous item ghost
        display.getTextBounds(menuItems[currentMenu->lastIndex], 0, 0, &x1, &y1,
                              &w, &h);
        int prevCenterX = (SCREEN_WIDTH - w) / 2;
        int prevX =
            slideRight ? (offset + prevCenterX) : (-offset + prevCenterX);

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
    // drawDecorativeLine();

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(menuItems[currentMenu->index], 0, 0, &x1, &y1, &w,
                          &h);
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

    case deauth_id:
        handleDeauth();
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
    display.println(
        (WiFi.encryptionType(networkIdx) == ENC_TYPE_NONE) ? "Brak" : "Jest");
}

// FEATUREEEEEEEEEEEEES

void handleWiFiScan() {
    static bool showConfirmation = false;
    static unsigned long confirmationTime = 0;
    static unsigned long scrollTime = 0;
    static int scrollOffset = 0;

    if (!scanComplete) {
        scanAnimation();
        networkCount = WiFi.scanNetworks();
        scanComplete = true;
        currentNetwork = 0;
    }
    if (buttonPressed(BTN_UP)) {
        currentNetwork = (currentNetwork - 1 + networkCount) % networkCount;
        showConfirmation = false;
        scrollOffset = 0;
        scrollTime = millis();
    }
    if (buttonPressed(BTN_DOWN)) {
        currentNetwork = (currentNetwork + 1) % networkCount;
        showConfirmation = false;
        scrollOffset = 0;
        scrollTime = millis();
    }
    if (buttonPressed(BTN_OK)) {
        selectedAP = WiFi.SSID(currentNetwork);
        Serial.print(selectedAP);
        showConfirmation = true;
        confirmationTime = millis();
        scrollOffset = 0;
        scrollTime = millis();
    }
    // check if confirmation should disappear
    if (showConfirmation && (millis() - confirmationTime > 1500)) {
        showConfirmation = false;
        scrollOffset = 0;
        scrollTime = millis();
    }

    if (millis() - scrollTime > 300) { // Scroll every 300ms
        scrollTime = millis();
        scrollOffset++;
    }

    display.clearDisplay();
    if (showConfirmation) {
        display.setTextSize(1);
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds("WYBRANO", 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 20);
        display.print("WYBRANO");

        display.setTextSize(1);
        display.getTextBounds(selectedAP.c_str(), 0, 0, &x1, &y1, &w, &h);

        if (w <= SCREEN_WIDTH - 8) {
            display.setCursor((SCREEN_WIDTH - w) / 2, 42);
            display.print(selectedAP);
        } else {
            String scrollText = selectedAP + "   " + selectedAP;
            int charWidth = 6;
            int maxScroll = selectedAP.length() + 3;
            int currentOffset = scrollOffset % maxScroll;

            display.setCursor(4 - (currentOffset * charWidth), 42);
            display.print(scrollText);
        }

        display.setTextSize(1);
        display.display();
    } else {
        // Normal WiFi scan display
        if (networkCount == 0) {
            drawHeader("Brak WiFi");
            // drawDecorativeLine();
            display.setCursor(20, 28);
            display.println("brak sieci...");
            display.setCursor(48, 42);
            display.print("(>_<)");
        } else {
            drawHeader("WiFi", currentNetwork + 1, networkCount);
            // drawDecorativeLine();
            drawWiFiNetwork(0, currentNetwork);
            drawNavigationDots();
        }
        display.display();
    }
}

void handleDeauth() {
    display.clearDisplay();
    drawHeader("selectedAP");
    // drawDecorativeLine();

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(selectedAP, 0, 0, &x1, &y1, &w, &h);

    if (w <= SCREEN_WIDTH - 8) {
        int x = (SCREEN_WIDTH - w) / 2;
        int padding = 4;
        drawSelectionBox(x - padding, 30, w + padding * 2, h + 4);
        display.setCursor(x, 32);
        display.print(selectedAP);
    } else {
        String apName = String(selectedAP);
        int maxWidth = SCREEN_WIDTH - 12;
        int cursorY = 26;
        int lineHeight = h + 2;
        String line = "";

        for (size_t i = 0; i < apName.length(); i++) {
            String testLine = line + apName.charAt(i);
            display.getTextBounds(testLine.c_str(), 0, 0, &x1, &y1, &w, &h);

            if (w > maxWidth && line.length() > 0) {
                display.getTextBounds(line.c_str(), 0, 0, &x1, &y1, &w, &h);
                int x = (SCREEN_WIDTH - w) / 2;
                display.setCursor(x, cursorY);
                display.print(line);
                cursorY += lineHeight;
                line = String(apName.charAt(i));
            } else {
                line = testLine;
            }
        }

        if (line.length() > 0) {
            display.getTextBounds(line.c_str(), 0, 0, &x1, &y1, &w, &h);
            int x = (SCREEN_WIDTH - w) / 2;
            display.setCursor(x, cursorY);
            display.print(line);
        }
    }

    display.display();
}
