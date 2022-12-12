#pragma once
#include <cstdint>
#include "driver/spi_master.h"
#include "esp_camera.h"

// ESP32Cam (AiThinker) PIN Map
static constexpr int8_t CAM_PIN_PWDN = 32;
// software reset will be performed
static constexpr int8_t CAM_PIN_RESET = -1;
static constexpr int8_t CAM_PIN_XCLK = 0;
static constexpr int8_t CAM_PIN_SIOD = 26;
static constexpr int8_t CAM_PIN_SIOC = 27;

static constexpr int8_t CAM_PIN_D7 = 35;
static constexpr int8_t CAM_PIN_D6 = 34;
static constexpr int8_t CAM_PIN_D5 = 39;
static constexpr int8_t CAM_PIN_D4 = 36;
static constexpr int8_t CAM_PIN_D3 = 21;
static constexpr int8_t CAM_PIN_D2 = 19;
static constexpr int8_t CAM_PIN_D1 = 18;
static constexpr int8_t CAM_PIN_D0 = 5;
static constexpr int8_t CAM_PIN_VSYNC = 25;
static constexpr int8_t CAM_PIN_HREF = 23;
static constexpr int8_t CAM_PIN_PCLK = 22;

// W5500 config
static constexpr int8_t W5500_MISO_GPIO = 12;
static constexpr int8_t W5500_MOSI_GPIO = 13;
static constexpr int8_t W5500_SCK_GPIO = 14;
static constexpr int8_t W5500_CS_GPIO = 15;
static constexpr int8_t W5500_INT_GPIO = 2;
// Disable PHY chip hardware reset
static constexpr int8_t W5500_RESET_GPIO = -1;

// default for esp32 according to example
static constexpr int32_t W5500_SPI_CLOCK_MHZ = 12;
static constexpr spi_host_device_t W5500_SPI_BUS = SPI2_HOST;
//  uint8_t W5500_MAC_ADDRESS[] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
// default is 1, IDK why
static constexpr int8_t W5500_PHY_ADDR = 1;

static constexpr char IF_KEY_STR[] = "ETH_SPI_0";
static constexpr char IF_DESC_STR[] = "eth0";

// Stream Jpeg
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";


static constexpr camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, // YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,   // QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.
    .jpeg_quality = 12,             // 0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,                  // When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .fb_location = CAMERA_FB_IN_PSRAM,

    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

// miso at GPIO 12, MOSI at GPIO 13, SCK at GPIO 14
static constexpr spi_bus_config_t W5500_BUS_CONFIG = {
    .mosi_io_num = W5500_MOSI_GPIO,
    .miso_io_num = W5500_MISO_GPIO,
    .sclk_io_num = W5500_SCK_GPIO,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
};

static constexpr spi_device_interface_config_t W5500_SPI_DEV_CONFIG = {
    .command_bits = 16, // Actually it's the address phase in W5500 SPI frame
    .address_bits = 8,  // Actually it's the control phase in W5500 SPI frame
    .mode = 0,
    .clock_speed_hz = W5500_SPI_CLOCK_MHZ * 1000 * 1000,
    .spics_io_num = W5500_CS_GPIO,
    .queue_size = 20,
};