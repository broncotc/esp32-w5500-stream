#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include "esp_event.h"
#include <sys/param.h>
#include <string.h>
#include <esp_eth.h>
#include "esp_netif.h"
#include <sdkconfig.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "ethernet.h"

// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

static const char *TAG = "main.cc";

static httpd_handle_t server = nullptr;

static esp_err_t init_camera()
{
    // initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

esp_err_t jpg_stream_httpd_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t *_jpg_buf;
    char *part_buf[64];
    static int64_t last_frame = 0;
    if (!last_frame)
    {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }

    while (true)
    {
        fb = esp_camera_fb_get();
        if (!fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        if (fb->format != PIXFORMAT_JPEG)
        {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if (!jpeg_converted)
            {
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
        }
        else
        {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK)
        {
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);

            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (fb->format != PIXFORMAT_JPEG)
        {
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);
        if (res != ESP_OK)
        {
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        ESP_LOGI(TAG, "MJPG: %uKB %ums (%.1ffps)",
                 (uint32_t)(_jpg_buf_len / 1024),
                 (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
    }

    last_frame = 0;
    return res;
}

static const httpd_uri_t stream = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = jpg_stream_httpd_handler,
};

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &stream);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        httpd_stop(server);
        server = nullptr;
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    if (server == nullptr)
    {
        ESP_LOGI(TAG, "Starting webserver");
        server = start_webserver();
    }
}

void init_ethernet()
{
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create instance(s) of esp-netif for SPI Ethernet(s)
    esp_netif_inherent_config_t esp_netif_base_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_base_config.if_key = IF_KEY_STR;
    esp_netif_base_config.if_desc = IF_DESC_STR;
    esp_netif_base_config.route_prio = 30 - 0;
    esp_netif_config_t netif_cfg = {
        .base = &esp_netif_base_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };

    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    ESP_ERROR_CHECK(spi_bus_initialize(W5500_SPI_BUS, &W5500_BUS_CONFIG, SPI_DMA_CH_AUTO));

    spi_device_handle_t spi_handle = nullptr;
    ESP_ERROR_CHECK(spi_bus_add_device(W5500_SPI_BUS, &W5500_SPI_DEV_CONFIG, &spi_handle));

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_handle);
    w5500_config.int_gpio_num = W5500_INT_GPIO;
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = W5500_PHY_ADDR;
    phy_config.reset_gpio_num = W5500_RESET_GPIO;
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    esp_eth_handle_t eth_handle = nullptr;
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));

    /* The SPI Ethernet module might not have a burned factory MAC address, we cat to set it manually.
   02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
    */

    uint8_t mac_address[] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, &mac_address));

    // attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, nullptr));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, nullptr));

    /* start Ethernet driver state machine */

    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    // static ip
    // esp_netif_dhcps_stop(eth_netif);
    esp_netif_dhcpc_stop(eth_netif);
    char ip[] = "192.168.1.2";
    char gateway[] = "192.168.1.1";
    char netmask[] = "255.255.255.0";
    esp_netif_ip_info_t info_t;
    memset(&info_t, 0, sizeof(esp_netif_ip_info_t));
    ip4addr_aton(ip, reinterpret_cast<ip4_addr_t *>(&info_t.ip));
    ip4addr_aton(gateway, reinterpret_cast<ip4_addr_t *>(&info_t.gw));
    ip4addr_aton(netmask, reinterpret_cast<ip4_addr_t *>(&info_t.netmask));
    esp_netif_set_ip_info(eth_netif, &info_t);
}

void cppmain()
{
    // Install GPIO interrupt service (as the SPI-Ethernet module is interrupt driven)
    gpio_install_isr_service(0);

    ESP_ERROR_CHECK(init_camera());
    init_ethernet();

    while (1)
    {
        // ESP_LOGI(TAG, "Taking picture...");
        // camera_fb_t *pic = esp_camera_fb_get();

        // // use pic->buf to access the image
        // ESP_LOGI(TAG, "Picture taken! Its size was: %zu bytes", pic->len);
        // esp_camera_fb_return(pic);

        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}

extern "C"
{
    void app_main()
    {
        cppmain();
    }
}
