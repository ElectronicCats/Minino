#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_PORT       UART_NUM_1
#define TX_PIN          5
#define RX_PIN          4

#define TX_BUF_SIZE     2048   // Habilita DMA TX
#define RX_BUF_SIZE     2048   // Habilita DMA RX

#define PIN_ON 8

static const char *TAG = "UART_DMA";

const char* comandos[] = {
    "$PCAS01,5*19\r\n", // Set UART baud rate to 115200
    "$PCAS02,1000*2E\r\n", // Set Fixed update rate to 1000ms
    "$PCAS04,3*1A\r\n", // Activar GPS y BDS
    "$PCAS04,3*1A\r\n",
    "$PCAS03,0,0,1,1,1,0,0,0*03\r\n",
    "$PCAS00*01\r\n", // Guardar configuracion
    "$PCAS10,0*1C\r\n" // Reinicio rapido
};

const char* comandos_2 [] = {
    "$PMTK220,200*2C\r\n",
};

void app_main(void)
{

    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_ON);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    gpio_set_level(PIN_ON, 1);

    // 1️⃣ Configuración básica del UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    // 2️⃣ Aplica la configuración al puerto
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));

    // 3️⃣ Asigna los pines TX/RX
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, TX_PIN, RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 4️⃣ Instala el driver con buffers > 0 → activa DMA
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT,
                                        RX_BUF_SIZE,
                                        TX_BUF_SIZE,
                                        0,   // sin cola de eventos
                                        NULL,
                                        0)); // sin flags especiales

    ESP_LOGI(TAG, "UART inicializado con DMA (TX=%d, RX=%d)", TX_BUF_SIZE, RX_BUF_SIZE);

    // 5️⃣ Envía datos (usando DMA internamente)
    for(uint8_t i = 0; i < sizeof(comandos) / sizeof(comandos[0]); i++) {
        uart_write_bytes(UART_PORT, comandos[i], strlen(comandos[i]));
        ESP_LOGI(TAG, "Enviado comando: %s", comandos[i]);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 6️⃣ Bucle de lectura (recibe con DMA)
    uint8_t data[1024];
    while (1) {
        int len = uart_read_bytes(UART_PORT, data, sizeof(data), pdMS_TO_TICKS(100));
        if (len > 0) {
            data[len] = '\0';
            ESP_LOGI(TAG, "Recibido: \n%s", (char *)data);
        }
    }
}
