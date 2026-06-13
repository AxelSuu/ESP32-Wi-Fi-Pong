#include "display.h"
#include "gfx.h"
#include "hw_config.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "display";

static spi_device_handle_t s_spi;

// --- Low-level SPI helpers ---

static void ssd1327_cmd(uint8_t cmd)
{
    gpio_set_level(OLED_DC, 0);
    spi_transaction_t t = {
        .length   = 8,
        .tx_data  = {cmd},
        .flags    = SPI_TRANS_USE_TXDATA,
    };
    spi_device_polling_transmit(s_spi, &t);
}

static void ssd1327_data_buf(const uint8_t *data, size_t len)
{
    if (len == 0) return;
    gpio_set_level(OLED_DC, 1);
    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(s_spi, &t);
}

void display_present(void)
{
    // Set column address 0–63 (128 pixels / 2 per byte)
    ssd1327_cmd(0x15);
    ssd1327_cmd(0x00);
    ssd1327_cmd(0x3F);
    // Set row address 0–95
    ssd1327_cmd(0x75);
    ssd1327_cmd(0x00);
    ssd1327_cmd(0x5F);
    // Send framebuffer
    ssd1327_data_buf(gfx_framebuffer(), gfx_framebuffer_size());
}

void display_set_contrast(uint8_t value)
{
    ssd1327_cmd(0x81);     // set-contrast command
    ssd1327_cmd(value);
}

esp_err_t display_init(void)
{
    // Configure DC and RST as output GPIOs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << OLED_DC) | (1ULL << OLED_RST),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Hardware reset
    gpio_set_level(OLED_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(OLED_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Init SPI bus (SPI2_HOST, IOMUX pins on ESP32-S3)
    spi_bus_config_t buscfg = {
        .mosi_io_num    = OLED_MOSI,
        .miso_io_num    = -1,
        .sclk_io_num    = OLED_SCLK,
        .quadwp_io_num  = -1,
        .quadhd_io_num  = -1,
        .max_transfer_sz = gfx_framebuffer_size(),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode           = 0,
        .spics_io_num   = OLED_CS,
        .queue_size     = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi));

    // SSD1327 init sequence
    ssd1327_cmd(0xAE);        // display off
    ssd1327_cmd(0x81);        // set contrast
    ssd1327_cmd(0x80);
    ssd1327_cmd(0xA0);        // remap: col-remap | no-nibble-remap | COM-flip
    ssd1327_cmd(0x51);        // 0x51 not 0x53: bit1=0 → even pixel in high nibble, matches set_pixel
    ssd1327_cmd(0xA1);        // start line
    ssd1327_cmd(0x00);
    ssd1327_cmd(0xA2);        // display offset
    ssd1327_cmd(128 - SCREEN_HEIGHT); // =32: COM-flip + MUX<128 shifts scan by (128-MUX); cancel it
    ssd1327_cmd(0xA6);        // all pixels off during init
    ssd1327_cmd(0xA8);        // multiplex ratio
    ssd1327_cmd(0x5F);        // 95 = 96 rows − 1 (correct for 128×96 panel)
    ssd1327_cmd(0xAB);        // function selection A
    ssd1327_cmd(0x01);
    ssd1327_cmd(0xB1);        // phase length
    ssd1327_cmd(0x11);        // 0x11 not 0xF1: correct charge/discharge timing
    ssd1327_cmd(0xB3);        // display clock
    ssd1327_cmd(0x00);
    ssd1327_cmd(0xB6);        // second pre-charge
    ssd1327_cmd(0x04);        // 0x04 not 0x0A
    ssd1327_cmd(0xBC);        // pre-charge voltage
    ssd1327_cmd(0x08);
    ssd1327_cmd(0xBE);        // VCOMH
    ssd1327_cmd(0x0F);        // 0x0F not 0x07: higher VCOMH → better contrast
    ssd1327_cmd(0xD5);        // function selection B
    ssd1327_cmd(0x62);
    ssd1327_cmd(0xFD);        // command lock
    ssd1327_cmd(0x12);        // 0x12 = unlock (ensure all commands are accepted)
    ssd1327_cmd(0xA4);        // normal display (show framebuffer)
    ssd1327_cmd(0xAF);        // display on
    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms settling time

    gfx_clear(0x0);
    display_present();

    ESP_LOGI(TAG, "SSD1327 initialized");
    return ESP_OK;
}
