#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>
#include <stdint.h>

// Pin definitions
#define BTN_UP D4   // GPIO2 - Increment button
#define BTN_DOWN D6 // GPIO12 - Decrement button
#define BTN_OK D7   // GPIO13 - Reset button
#define BTN_BACK D3 // GPIO0 - Back button (also flash button)

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// I2C pins for nodeMCU
#define SDA_PIN D2 // GPIO4
#define SCL_PIN D1 // GPIO5

constexpr const char wifi_scan[] = "SKAN SIECI WIFI";
constexpr const char deauth[] = "DEAUTORYZACJA";
constexpr const char evil_twin[] = "EVIL TWIN";
constexpr const char beacon_spam[] = "BEACON_SPAM";
constexpr const char sniffing[] = "SNIFFING";
constexpr const char settings[] = "USTAWIENIA";
constexpr const char info[] = "INFO";

constexpr const char wifi_scan_id = 0;
constexpr const char deauth_id = 1;
constexpr const char evil_twin_id = 2;
constexpr const char beacon_spam_id = 3;
constexpr const char sniffing_id = 4;
constexpr const char settings_id = 5;
constexpr const char info_id = 6;

constexpr const char *menuItems[] = {
    wifi_scan, deauth, evil_twin, beacon_spam, sniffing, settings, info,
};

constexpr int MENU_SIZE = sizeof(menuItems) / sizeof(menuItems[0]);

const uint8_t heartBitmap[] PROGMEM = {0b01100111, 0b10000000, 0b10000000,
                                       0b11111111, 0b01111110, 0b00111100,
                                       0b00011000, 0b00000000};

#endif
