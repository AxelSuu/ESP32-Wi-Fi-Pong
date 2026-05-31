#include "network.h"
#include "net_config.h"
#include "engine.h"
#include "game_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define WS_RX_BUF_SIZE 128

static const char *TAG = "network";

EventGroupHandle_t net_event_group;

static httpd_handle_t s_server = NULL;
static int s_ws_fds[MAX_WS_CLIENTS];
static SemaphoreHandle_t s_ws_fd_mutex;

// --- WS client fd table; the slot index is the player id ---

static int ws_fd_add(int fd)
{
    int slot = -1;
    xSemaphoreTake(s_ws_fd_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] == -1) {
            s_ws_fds[i] = fd;
            slot = i;
            break;
        }
    }
    xSemaphoreGive(s_ws_fd_mutex);
    return slot;
}

static void ws_fd_remove(int fd)
{
    xSemaphoreTake(s_ws_fd_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) {
            s_ws_fds[i] = -1;
            break;
        }
    }
    xSemaphoreGive(s_ws_fd_mutex);
}

static int slot_of_fd(int fd)
{
    int slot = -1;
    xSemaphoreTake(s_ws_fd_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) { slot = i; break; }
    }
    xSemaphoreGive(s_ws_fd_mutex);
    return slot;
}

int net_player_count(void)
{
    int n = 0;
    xSemaphoreTake(s_ws_fd_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) if (s_ws_fds[i] != -1) n++;
    xSemaphoreGive(s_ws_fd_mutex);
    return n;
}

// --- async send / broadcast ---

typedef struct { httpd_handle_t hd; int fd; char *json; } ws_send_ctx_t;

static void ws_async_send(void *arg)
{
    ws_send_ctx_t *c = arg;
    httpd_ws_frame_t f = {
        .type    = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)c->json,
        .len     = strlen(c->json),
    };
    httpd_ws_send_frame_async(c->hd, c->fd, &f);
    free(c->json);
    free(c);
}

// queue a copy of json to a single fd
static void ws_send_one(int fd, const char *json)
{
    if (!s_server) return;
    ws_send_ctx_t *c = malloc(sizeof *c);
    if (!c) return;
    c->hd = s_server;
    c->fd = fd;
    c->json = strdup(json);
    if (!c->json) { free(c); return; }
    if (httpd_queue_work(s_server, ws_async_send, c) != ESP_OK) {
        free(c->json);
        free(c);
    }
}

// queue a copy of json to every connected fd
static void ws_broadcast(const char *json)
{
    if (!s_server) return;
    int fds[MAX_WS_CLIENTS];
    int n = 0;
    xSemaphoreTake(s_ws_fd_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) if (s_ws_fds[i] != -1) fds[n++] = s_ws_fds[i];
    xSemaphoreGive(s_ws_fd_mutex);
    for (int i = 0; i < n; i++) ws_send_one(fds[i], json);
}

void net_broadcast_active(const char *game_id, int players)
{
    char buf[64];
    snprintf(buf, sizeof buf, "{\"t\":\"active\",\"game\":\"%s\",\"players\":%d}",
             game_id, players);
    ws_broadcast(buf);
}

void net_broadcast_waiting(int need, int have)
{
    char buf[48];
    snprintf(buf, sizeof buf, "{\"t\":\"waiting\",\"need\":%d,\"have\":%d}", need, have);
    ws_broadcast(buf);
}

void net_broadcast_over(int winner)
{
    char buf[40];
    snprintf(buf, sizeof buf, "{\"t\":\"over\",\"winner\":%d}", winner);
    ws_broadcast(buf);
}

// --- tiny JSON field extractors for our flat, trusted schema ---

// find "key": then return pointer just past the colon (skipping spaces), or NULL.
static const char *json_value(const char *msg, const char *key)
{
    char pat[20];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(msg, pat);
    if (!p) return NULL;
    p = strchr(p + strlen(pat), ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static bool json_find_str(const char *msg, const char *key, char *out, size_t cap)
{
    const char *p = json_value(msg, key);
    if (!p || *p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < cap - 1) out[i++] = *p++;
    out[i] = '\0';
    return true;
}

static bool json_find_num(const char *msg, const char *key, double *out)
{
    const char *p = json_value(msg, key);
    if (!p) return false;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p) return false;
    *out = v;
    return true;
}

// --- WebSocket handler ---

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        int fd   = httpd_req_to_sockfd(req);
        int slot = ws_fd_add(fd);
        ESP_LOGI(TAG, "WS handshake fd=%d slot=%d", fd, slot);
        return ESP_OK;
    }

    int fd = httpd_req_to_sockfd(req);

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        int slot = slot_of_fd(fd);
        ws_fd_remove(fd);
        if (slot >= 0) engine_on_player_disconnect(slot);
        return ESP_OK;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_PING || ws_pkt.len == 0) {
        return ESP_OK;
    }

    uint8_t buf[WS_RX_BUF_SIZE] = {0};
    ws_pkt.payload  = buf;
    size_t recv_len = ws_pkt.len < sizeof(buf) - 1 ? ws_pkt.len : sizeof(buf) - 1;
    ret = httpd_ws_recv_frame(req, &ws_pkt, recv_len);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.type != HTTPD_WS_TYPE_TEXT) return ESP_OK;

    char *msg = (char *)buf;
    char t[16];
    if (!json_find_str(msg, "t", t, sizeof t)) return ESP_OK;

    int player = slot_of_fd(fd);
    if (player < 0) player = 0;

    if (strcmp(t, "hello") == 0) {
        char w[40];
        snprintf(w, sizeof w, "{\"t\":\"welcome\",\"player\":%d}", player);
        ws_send_one(fd, w);
        engine_on_player_connect(player);
        return ESP_OK;
    }

    input_event_t ev = { .kind = INPUT_NONE, .player = player, .analog = 0 };

    if (strcmp(t, "nav") == 0) {
        double d = 1;
        json_find_num(msg, "dir", &d);
        ev.kind = INPUT_NAV;
        ev.analog = (float)d;
    } else if (strcmp(t, "select") == 0) {
        ev.kind = INPUT_SELECT;
    } else if (strcmp(t, "back") == 0) {
        ev.kind = INPUT_BACK;
    } else if (strcmp(t, "tilt") == 0) {
        double g;
        if (json_find_num(msg, "g", &g)) { ev.kind = INPUT_TILT; ev.analog = (float)g; }
    } else if (strcmp(t, "input") == 0) {
        char e[12];
        if (json_find_str(msg, "ev", e, sizeof e)) {
            if      (strcmp(e, "up") == 0)      ev.kind = INPUT_UP;
            else if (strcmp(e, "down") == 0)    ev.kind = INPUT_DOWN;
            else if (strcmp(e, "left") == 0)    ev.kind = INPUT_LEFT;
            else if (strcmp(e, "right") == 0)   ev.kind = INPUT_RIGHT;
            else if (strcmp(e, "primary") == 0) ev.kind = INPUT_PRIMARY;
        }
    }

    if (ev.kind != INPUT_NONE) engine_dispatch_input(&ev);
    return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    FILE *f = fopen(SPIFFS_BASE_PATH "/index.html", "r");
    if (!f) {
        ESP_LOGE(TAG, "index.html not found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "index.html missing");
        return ESP_FAIL;
    }
    char chunk[512];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        httpd_resp_send_chunk(req, chunk, (ssize_t)n);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static void start_webserver(void)
{
    httpd_config_t config    = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets  = 7;

    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start httpd");
        return;
    }

    httpd_uri_t uri_index = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = index_handler,
    };
    httpd_register_uri_handler(s_server, &uri_index);

    httpd_uri_t uri_ws = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &uri_ws);

    ESP_LOGI(TAG, "HTTP server started");
}

static void wifi_ap_start_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "AP started");

    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path              = SPIFFS_BASE_PATH,
        .partition_label        = "storage",
        .max_files              = 5,
        .format_if_mount_failed = false,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS mounted");
    }

    start_webserver();
    xEventGroupSetBits(net_event_group, NETWORK_READY_BIT);
}

void network_wifi_init_ap(void)
{
    s_ws_fd_mutex = xSemaphoreCreateMutex();
    for (int i = 0; i < MAX_WS_CLIENTS; i++) s_ws_fds[i] = -1;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_AP_START, wifi_ap_start_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {
        .ap = {
            .ssid            = WIFI_SSID,
            .ssid_len        = sizeof(WIFI_SSID) - 1,
            .password        = WIFI_PASSWORD,
            .channel         = WIFI_AP_CHANNEL,
            .max_connection  = WIFI_MAX_CONN,
            .authmode        = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started: SSID=%s", WIFI_SSID);
}
