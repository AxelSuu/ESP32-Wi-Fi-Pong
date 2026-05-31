#include "network.h"
#include "display.h"
#include "engine.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "main";

#define FRAME_MS 30

static void game_task(void *arg)
{
    xEventGroupWaitBits(net_event_group, NETWORK_READY_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Network ready, starting engine loop");

    for (;;) {
        engine_update(FRAME_MS);
        engine_render();
        vTaskDelay(pdMS_TO_TICKS(FRAME_MS));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    net_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(display_init());
    engine_init();
    network_wifi_init_ap();

    xTaskCreatePinnedToCore(game_task, "game", 4096, NULL, 5, NULL, 0);
}
