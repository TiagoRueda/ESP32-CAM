#include <stdio.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <mbedtls/base64.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"

static const char *HTAG = "HTTP";

extern QueueHandle_t xQueueHttp;

char * localFileName = NULL;

// Calculate the size after conversion to base64
int32_t calcBase64EncodedSize(int origDataSize)
{
	int32_t numBlocks6 = ((origDataSize * 8) + 5) / 6;
	int32_t numBlocks4 = (numBlocks6 + 3) / 4;
	int32_t numNetChars = numBlocks4 * 4;
	return numNetChars;
}

// Convert image to BASE64
esp_err_t Image2Base64(char * filename, size_t fsize, unsigned char * base64_buffer, size_t base64_buffer_len)
{
	unsigned char* image_buffer = NULL;
	image_buffer = malloc(fsize);
	if (image_buffer == NULL) {
		ESP_LOGE(HTAG, "malloc fail. image_buffer %d", fsize);
		return ESP_FAIL;
	}

	FILE * fp;
	if((fp=fopen(filename,"rb"))==NULL){
		ESP_LOGE(HTAG, "fopen fail. [%s]", filename);
		return ESP_FAIL;
	}
	for (int i=0;i<fsize;i++) {
		fread(&image_buffer[i],sizeof(char),1,fp);
	}
	fclose(fp);

	size_t encord_len;
	esp_err_t ret = mbedtls_base64_encode(base64_buffer, base64_buffer_len, &encord_len, image_buffer, fsize);
	ESP_LOGI(HTAG, "mbedtls_base64_encode=%d encord_len=%d", ret, encord_len);
	free(image_buffer);
	return ret;
}

/* root get handler */
static esp_err_t root_get_handler(httpd_req_t *req)
{
	ESP_LOGI(HTAG, "root_get_handler");
	if (localFileName == NULL) {
		httpd_resp_sendstr_chunk(req, NULL);
		return ESP_OK;
	}

	struct stat st;
	if (stat(localFileName, &st) != 0) {
		ESP_LOGE(HTAG, "[%s] not found", localFileName);
		httpd_resp_sendstr_chunk(req, NULL);
		return ESP_OK;
	}

	ESP_LOGI(HTAG, "%s exist st.st_size=%ld", localFileName, st.st_size);
	int32_t base64Size = calcBase64EncodedSize(st.st_size);
	ESP_LOGI(HTAG, "base64Size=%"PRIi32, base64Size);

	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

	unsigned char*	img_src_buffer = NULL;
	size_t img_src_buffer_len = base64Size + 1;
	img_src_buffer = malloc(img_src_buffer_len);
	if (img_src_buffer == NULL) {
		ESP_LOGE(HTAG, "malloc fail. img_src_buffer_len %d", img_src_buffer_len);
	} else {
		esp_err_t ret = Image2Base64(localFileName, st.st_size, img_src_buffer, img_src_buffer_len);
		ESP_LOGI(HTAG, "Image2Base64=%d", ret);
		if (ret != 0) {
			ESP_LOGE(HTAG, "Error in mbedtls encode! ret = -0x%x", -ret);
		} else {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
			httpd_resp_send_chunk(req, (char *)img_src_buffer, base64Size);
			httpd_resp_sendstr_chunk(req, "\" />");
		}
	}
	if (img_src_buffer != NULL) free(img_src_buffer);
	httpd_resp_sendstr_chunk(req, "</tbody></table>");
	httpd_resp_sendstr_chunk(req, "</body></html>");
	httpd_resp_sendstr_chunk(req, NULL);
	return ESP_OK;
}

static esp_err_t favicon_get_handler(httpd_req_t *req)
{
	ESP_LOGI(HTAG, "favicon_get_handler");
	return ESP_OK;
}

esp_err_t start_server(int port)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.lru_purge_enable = true;
    config.server_port = port;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(HTAG, "Failed to start file server!");
        return ESP_FAIL;
    }

    httpd_uri_t _root_get_handler = {
        .uri         = "/",
        .method      = HTTP_GET,
        .handler     = root_get_handler,
    };
    httpd_register_uri_handler(server, &_root_get_handler);

    httpd_uri_t _favicon_get_handler = {
        .uri         = "/favicon.ico",
        .method      = HTTP_GET,
        .handler     = favicon_get_handler,
    };
    httpd_register_uri_handler(server, &_favicon_get_handler);

    return ESP_OK;
}

void http_task(void *pvParameters)
{
	char *task_parameter = (char *)pvParameters;
	ESP_LOGI(HTAG, "Start task_parameter=%s", task_parameter);
	char url[64];
	int port = 8080;
	sprintf(url, "http://%s:%d", task_parameter, port);
	ESP_LOGI(HTAG, "Starting HTTP server on %s", url);
	ESP_ERROR_CHECK(start_server(port));

	HTTP_t httpBuf;
	while(1) {
		if (xQueueReceive(xQueueHttp, &httpBuf, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(HTAG, "httpBuf.localFileName=[%s]", httpBuf.localFileName);
			localFileName = httpBuf.localFileName;
			ESP_LOGW(HTAG, "Open this in your browser %s", url);
		}
	}
	ESP_LOGI(HTAG, "finish");
	vTaskDelete(NULL);
}
