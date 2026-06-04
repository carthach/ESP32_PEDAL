// User_Setup.h for ESP32-S3-N16R8 + 2.8" ILI9341 (Wokwi)
#define USER_SETUP_INFO "ESP32-S3-N16R8 ILI9341 Touch (Wokwi)"
#define ILI9341_DRIVER

// ===== Display SPI pins =====
#define TFT_CS   10
#define TFT_DC   8
#define TFT_RST  9
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO 13

// ===== Touch (FT6206) =====
#define TFT_SCL 3
#define TFT_SDA 46

// ===== Fonts =====
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ===== SPI speeds =====
#define SPI_FREQUENCY        40000000
#define SPI_READ_FREQUENCY   6000000
#define SPI_TOUCH_FREQUENCY  2500000

// This is important for ESP32-S3
#define USE_HSPI_PORT 