#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8

// ===== DRIVER =====
#define ILI9341_DRIVER

// ===== SPI PINS =====
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCK  18
#define TFT_CS   5
#define TFT_DC   21
#define TFT_RST  4

#define XPT2046_TOUCH
#define TOUCH_CS 16

// ===== OPTIONAL BACKLIGHT =====
#define TFT_LED 15
#define TFT_BACKLIGHT_ON HIGH

// ===== SPI SPEED =====
#define SPI_FREQUENCY  27000000   // safer than 40MHz
#define SPI_TOUCH_FREQUENCY  2500000
