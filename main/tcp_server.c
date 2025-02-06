#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include "driver/gpio.h"
#include "esp_netif_ip_addr.h"

// Configuración de red y hardware
#define SERVER_PORT 3333
#define GPIO_LED_PIN GPIO_NUM_2
#define WIFI_SSID "PUCESE_WIFI"
#define WIFI_PASS "Euskadi1981"

static const char *LOG_TAG = "ESP32_TCP_Server";
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_FLAG BIT0

// Manejador de eventos Wi-Fi
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(LOG_TAG, "Conexión Wi-Fi establecida, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_FLAG);
    }
}

// Inicialización de Wi-Fi
void initialize_wifi(void) {
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_FLAG, pdFALSE, pdTRUE, portMAX_DELAY);
}

// Tarea del servidor TCP
void tcp_server_task(void *pvParameters) {
    char buffer[128];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int server_socket, client_socket;

    // Crear socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        ESP_LOGE(LOG_TAG, "Error al crear el socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // Configurar dirección del servidor
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Asociar socket a la dirección
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(LOG_TAG, "Error al asociar el socket: errno %d", errno);
        close(server_socket);
        vTaskDelete(NULL);
        return;
    }

    // Escuchar conexiones entrantes
    if (listen(server_socket, 1) < 0) {
        ESP_LOGE(LOG_TAG, "Error al escuchar: errno %d", errno);
        close(server_socket);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(LOG_TAG, "Servidor escuchando en el puerto %d", SERVER_PORT);

    while (1) {
        // Aceptar conexión de cliente
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            ESP_LOGE(LOG_TAG, "Error al aceptar conexión: errno %d", errno);
            break;
        }

        ESP_LOGI(LOG_TAG, "Cliente conectado");
        while (1) {
            int len = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (len <= 0) {
                ESP_LOGI(LOG_TAG, "Cliente desconectado");
                close(client_socket);
                break;
            }

            buffer[len] = '\0';
            ESP_LOGI(LOG_TAG, "Mensaje recibido: %s", buffer);

            // Controlar LED según el comando recibido
            if (strcmp(buffer, "ON") == 0) {
                gpio_set_level(GPIO_LED_PIN, 1);
                send(client_socket, "LED ENCENDIDO", 13, 0);
            } else if (strcmp(buffer, "OFF") == 0) {
                gpio_set_level(GPIO_LED_PIN, 0);
                send(client_socket, "LED APAGADO", 11, 0);
            } else {
                send(client_socket, "COMANDO INVALIDO", 16, 0);
            }
        }
    }
    close(server_socket);
    vTaskDelete(NULL);
}

// Función principal
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    initialize_wifi();

    gpio_reset_pin(GPIO_LED_PIN);
    gpio_set_direction(GPIO_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED_PIN, 0);

    xTaskCreate(tcp_server_task, "tcp_server_task", 4096, NULL, 5, NULL);

    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));
    ESP_LOGI(LOG_TAG, "Dirección IP asignada: " IPSTR, IP2STR(&ip_info.ip));
}