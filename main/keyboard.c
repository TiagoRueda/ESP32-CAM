#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#if CONFIG_SHUTTER_ENTER
extern QueueHandle_t xQueueCmd;

static const char *KTAG = "KEYBOARD";

void keyin(void *pvParameters)
{
	ESP_LOGI(KTAG, "Start");
	CMD_t cmdBuf;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();
	cmdBuf.command = CMD_TAKE;

	uint16_t c;
	while (1) {
		c = fgetc(stdin);
		if (c == 0xffff) {
			vTaskDelay(10);
			continue;
		}
		if (c == 0x0a) {
			ESP_LOGI(KTAG, "Push Enter");
			if (xQueueSend(xQueueCmd, &cmdBuf, 10) != pdPASS) {
				ESP_LOGE(KTAG, "xQueueSend fail");
			}
		}
	}

	vTaskDelete( NULL );
}
#endif

