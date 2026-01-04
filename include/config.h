#ifndef CONFIG_H
#define CONFIG_H

// Pin definitions
#define BTN_UP D4    // GPIO2 - Increment button
#define BTN_DOWN D6  // GPIO12 - Decrement button
#define BTN_OK D7    // GPIO13 - Reset button
#define BTN_BACK D3  // GPIO0 - Back button (also flash button)

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C






// I2C pins for nodeMCU
#define SDA_PIN D2  // GPIO4
#define SCL_PIN D1  // GPIO5

#endif
