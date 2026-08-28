#include "OTAUpdater.hpp"
#include <string>
#include "esp_ota_ops.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_ota_ops.h>
#include <esp_rom_crc.h>

esp_ota_handle_t handler;

OTAUpdater::OTAUpdater(std::string firmware_url, std::string checksum_url, std::string check_update_url)
{
    this->firmware_url = firmware_url;
    this->checksum_url = checksum_url;
    this->check_update_url = check_update_url;
    this->original_checksum = -1;
    this->firmware_checksum = -1;
    this->update_available = false;
};

bool OTAUpdater::check_for_update()
{
    HTTPClient http;

    http.begin(this->check_update_url.c_str());

    if (http.GET() <= 0)
    {
        Serial.println("Unable to check for update");
        return 0;
    }

    String payload = http.getString();
    Serial.println(payload);

    this->update_available = true;
    Serial.println("Update available");
    return 0;
}

bool OTAUpdater::verify_checksum()
{
    if (this->original_checksum != this->firmware_checksum)
    {
        Serial.println("Checksums are not identical");
        return false;
    }
    Serial.println("Checksum matched");
    return true;
}

void OTAUpdater::download_checksum()
{
    Serial.println("Downloading checksum");

    if (!WiFi.isConnected())
    {
        Serial.println("Wifi not connected");
        return;
    };

    HTTPClient http;
    http.begin(this->checksum_url.c_str());

    if (http.GET() > 0)
    {
        this->original_checksum = strtoul(http.getString().c_str(), NULL, 10);
    }
    else
    {
        Serial.println("Error receiving checksum");
        return;
    }

    Serial.println("Checksum download completed");
};

void OTAUpdater::download_firmware()
{
    if (!WiFi.isConnected())
    {
        Serial.println("Wifi not connected");
        return;
    };

    // Checking if update is available
    if (!update_available)
    {
        Serial.println("Update not available");
        return;
    }

    HTTPClient http;

    http.begin(this->firmware_url.c_str());

    if (http.GET() > 0)
    {
        this->firmware_checksum = ~0xFFFFFFFF;

        int len = http.getSize();

        // Getting next partition
        const esp_partition_t *next_partition = esp_ota_get_next_update_partition(NULL);

        esp_err_t ota_begin_result = esp_ota_begin(next_partition, len, &handler);

        if (ota_begin_result != ESP_OK)
        {
            Serial.println("Fail to begin ota");
            return;
        }

        WiFiClient *stream = http.getStreamPtr();
        uint8_t buffer[1024] = {0};

        Serial.println("Downloading firmware...");
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
                        Serial.printf("OTA write failed: %s\n", esp_err_to_name(chunk_result));
                        esp_ota_end(handler);
                        return;
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
            Serial.println("Failed to end OTA");
            return;
        }
        Serial.println("OTA end success");
    }
    else
    {
        Serial.println("Error receiving firmware");
        return;
    }

    Serial.println("Firmware downloading completed");
}

void OTAUpdater::change_bootorder()
{
    Serial.println("Changing boot partition");

    const esp_partition_t *next_partition = esp_ota_get_next_update_partition(NULL);
    esp_err_t boot_partition_change_result = esp_ota_set_boot_partition(next_partition);

    if (boot_partition_change_result != ESP_OK)
    {
        Serial.println("Failed to change the boot partition");
        return;
    }
    Serial.print("Boot partition changed to: ");
    Serial.println(next_partition->label);
}

void OTAUpdater::start_ota_update_sequence(bool change_order, bool reboot)
{
    Serial.println("Starting OTA Update Sequence");
    check_for_update();
    download_checksum();
    download_firmware();
    verify_checksum();

    if (change_order)
    {
        change_bootorder();
    }

    if (reboot)
    {
        ESP.restart();
    }

    Serial.println("OTA Update Sequence successful");
}