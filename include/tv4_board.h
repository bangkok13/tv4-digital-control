#pragma once

// Also included by the TFT_eSPI translation units via platformio.ini.
#if defined(TV4_ST7789) == defined(TV4_ILI9341)
#error Select exactly one verified CYD display profile.
#endif
#define USE_HSPI_PORT
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define SPI_FREQUENCY 24000000
#define LOAD_GLCD
#define LOAD_FONT2
#define DISABLE_ALL_LIBRARY_WARNINGS
#ifdef TV4_ST7789
#define ST7789_DRIVER
#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_OFF
#define TV4_PROFILE_NAME "CYD 2USB / ST7789"
#define TV4_CAL_KEY "cal-st"
#else
#define ILI9341_2_DRIVER
#define TV4_PROFILE_NAME "CYD R / ILI9341"
#define TV4_CAL_KEY "cal-ili"
#endif

#ifdef __cplusplus
#include "version.h"
namespace tv4::board {
constexpr int backlight = 21, pwmChannel = 0;
constexpr int touchClock = 25, touchMosi = 32, touchMiso = 39, touchCs = 33, touchIrq = 36;
constexpr int rpmInput = 27;  // CN1, active low, also biased when the cable is absent.
constexpr int width = 240, height = 320;
constexpr char version[] = TV4_VERSION;
}
#endif
