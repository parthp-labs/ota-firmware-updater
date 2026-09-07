#include "OTAUpdater.hpp"
#include <string>
#include "esp_ota_ops.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>
#include <esp_rom_crc.h>
#include <ArduinoJson.h>

esp_ota_handle_t handler;

OTAUpdater::OTAUpdater(std::string firmware_url, std::string checksum_url, std::string check_update_url, std::string api_key)
{
    this->firmware_url = firmware_url;
    this->checksum_url = checksum_url;
    this->check_update_url = check_update_url;
    this->original_checksum = -1;
    this->firmware_checksum = -1;
    this->update_available = false;
    this->api_key = api_key;
    this->auth_header = "Bearer " + this->api_key;

    if (esp_efuse_mac_get_default(&(this->device_mac)) != ESP_OK)
    {
        this->device_mac = -1;
    }
};

OTAResult OTAUpdater::check_wifi()
{
    if (WiFi.isConnected())
    {
        return OTAResult::OK;
    }

    return OTAResult::WIFI_NOT_CONNECTED;
}

OTAResult OTAUpdater::check_for_update()
{
    if (!WiFi.isConnected())
    {
        return OTAResult::WIFI_NOT_CONNECTED;
    }

    HTTPClient http;
    // When http.begin() is called, esp32 internally clears all the internal states, hence header is also removed, so call addHeader after begin
    http.begin(this->check_update_url.c_str());
    Serial.println(this->auth_header.c_str());
    http.addHeader("Authorization", this->auth_header.c_str());

    JsonDocument response;
    int result = http.GET();

    String payload = http.getString();
    DeserializationError error = deserializeJson(response, payload);

    if (error)
    {
        Serial.print("Parsing failed: ");
        Serial.println(error.c_str());
        return OTAResult::PARSING_ERROR;
    }

    String message = response["message"];
    switch (result)
    {
    case 200:
    {
        this->update_available = response["update_available"];
        this->original_checksum = response["crc32"];

        if (this->update_available)
        {
            return OTAResult::OK;
        }
        return OTAResult::NO_UPDATE_AVAILABLE;
    }
    case 401:
    {
        return OTAResult::INVALID_API_KEY;
    }
    default:
    {
        Serial.println(message);
        return OTAResult::HTTP_ERROR;
    }
    }
    return OTAResult::OK;
}

OTAResult OTAUpdater::verify_checksum()
{
    if (this->original_checksum != this->firmware_checksum)
    {
        return OTAResult::CHECKSUM_MISMATCH;
    }

    return OTAResult::OK;
}

OTAResult OTAUpdater::download_checksum()
{
    if (!WiFi.isConnected())
    {
        return OTAResult::WIFI_NOT_CONNECTED;
    }

    HTTPClient http;
    http.begin(this->checksum_url.c_str());

    if (http.GET() < 0)
    {
        return OTAResult::CHECKSUM_DOWNLOAD_FAILED;
    }

    this->original_checksum = strtoul(http.getString().c_str(), NULL, 10);

    return OTAResult::OK;
};

OTAResult OTAUpdater::download_firmware()
{
    if (!WiFi.isConnected())
    {
        return OTAResult::WIFI_NOT_CONNECTED;
    }

    // Checking if update is available
    if (check_for_update() != OTAResult::OK)
    {
        return OTAResult::NO_UPDATE_AVAILABLE;
    }

    HTTPClient http;
    http.begin(this->firmware_url.c_str());
    http.addHeader("Authorization", this->auth_header.c_str());

    JsonDocument response;
    int result = http.GET();

    String payload = http.getString();
    DeserializationError error = deserializeJson(response, payload);

    switch (result)
    {
    case 200:
    {
        this->firmware_checksum = ~0xFFFFFFFF;
        int len = http.getSize();

        // Getting next partition
        const esp_partition_t *next_partition = esp_ota_get_next_update_partition(NULL);
        esp_err_t ota_begin_result = esp_ota_begin(next_partition, len, &handler);

        if (ota_begin_result != ESP_OK)
        {
            return OTAResult::OTA_BEGIN_FAILED;
        }

        WiFiClient *stream = http.getStreamPtr();
        uint8_t buffer[1024] = {0};

        size_t downloaded = 0;

        while (http.connected() && (len > 0 || len == -1))
        {
            size_t available_bytes = stream->available();

            if (available_bytes)
            {
                size_t bytes_to_read = min(available_bytes, sizeof(buffer));
                int c = stream->readBytes(buffer, bytes_to_read);

                if (c > 0)
                {
                    esp_err_t chunk_result = esp_ota_write(handler, buffer, c);

                    if (chunk_result != ESP_OK)
                    {
                        esp_ota_end(handler);
                        return OTAResult::OTA_WRITE_FAILED;
                    }
                    downloaded += c;

                    if (len > 0)
                        len -= c;

                    this->firmware_checksum = esp_rom_crc32_le(this->firmware_checksum, buffer, bytes_to_read);
                }
            }
            delay(1);
        }

        esp_err_t ota_end_result = esp_ota_end(handler);
        if (ota_end_result != ESP_OK)
        {
            return OTAResult::OTA_END_FAILED;
        }

        return OTAResult::OK;
    }
    case 404:
    {
        return OTAResult::FIRMWARE_NOT_FOUND;
        break;
    }
    default:
    {
        Serial.println(payload);
        return OTAResult::FIRMWARE_REQUEST_FAILED;
        break;
    }
    }
}

OTAResult OTAUpdater::change_bootorder()
{
    const esp_partition_t *next_partition = esp_ota_get_next_update_partition(NULL);
    esp_err_t boot_partition_change_result = esp_ota_set_boot_partition(next_partition);

    if (boot_partition_change_result != ESP_OK)
    {
        return OTAResult::BOOT_PARTITION_CHANGE_FAILED;
    }
    return OTAResult::OK;
}

OTAResult OTAUpdater::start_ota_update_sequence(bool change_order, bool reboot)
{
    OTAResult result;

    result = check_for_update();
    if (result != OTAResult::OK)
        return result;

    result = download_checksum();
    if (result != OTAResult::OK)
        return result;

    result = download_firmware();
    if (result != OTAResult::OK)
        return result;

    result = verify_checksum();
    if (result != OTAResult::OK)
        return result;

    if (change_order)
    {
        result = change_bootorder();
        if (result != OTAResult::OK)
            return result;
    }

    if (reboot)
    {
        ESP.restart();
    }

    return OTAResult::OK;
}