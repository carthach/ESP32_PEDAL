# ESP32-S3_LVGL

LVGL example for ESP32-S3 microcontroller and ILI9341, with Wokwi simulation support and PlatformIO configuration.

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- ESP32-S3 development board with ILI9341 touchscreen
- USB cable for programming and power

### Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/carthach/ESP32-S3_LVGL.git
   cd ESP32-S3_LVGL
   ```

2. Open the project in PlatformIO

3. Configure your board settings in `platformio.ini` if needed

4. Build and upload to your ESP32-S3 board

### Wokwi Simulation

To test the project using Wokwi simulation:

1. Open the project in PlatformIO
2. Use the Wokwi simulation environment to test the dialpad without physical hardware
3. This is useful for rapid development and debugging

## Hardware Requirements

- ESP32-S3 Development Board
- ILI9341 TFT Display with touch support
- Appropriate connection pins configured for your setup (check the TFT_ESPI User_Setups in /include)

## Project Structure

```
ESP32-S3_LVGL/
├── platformio.ini          # PlatformIO configuration
├── src/                    # Source code
└── README.md              # This file
```

## License

This project is provided as-is for learning and development purposes.

## Contributing

Feel free to submit issues and enhancement requests!

## References

- [TFT_ESPI Library](https://github.com/Bodmer/TFT_eSPI)
- [ESP32-S3 Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/)
- [Wokwi Simulator](https://wokwi.com/)
- [PlatformIO](https://platformio.org/)
