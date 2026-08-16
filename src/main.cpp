#include <Arduino.h>
#include "esp_ota_ops.h"
#include <SPI.h>
#include <HTTPClient.h>
#include <esp_rom_crc.h>

char ssid[] = "PARTH";
char pass[] = "iamparth";

char server[] = "http://172.31.202.151:8000/firmware.bin";
char checksum[] = "http://172.31.202.151:8000/firmware.crc32";

esp_ota_handle_t handler;
void setup()
{
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  HTTPClient http;

  // Receiving Checksum number
  http.begin(checksum);
  uint32_t original_crc = 0;

  if (http.GET() > 0)
  {
    original_crc = strtoul(http.getString().c_str(), NULL, 10);
  }
  else
  {
    Serial.println("Unable to receive checksum");
    return;
  }

  // Initiating OTA Update
  http.begin(server);

  int httpResponseCode = http.GET();
  uint32_t crc = ~0xFFFFFFFF;
  if (httpResponseCode > 0)
  {
    int len = http.getSize();

    const esp_partition_t *next_partition = esp_ota_get_next_update_partition(NULL);
    esp_err_t ota_begin_result = esp_ota_begin(next_partition, len, &handler);

    if (ota_begin_result != ESP_OK)
    {
      Serial.println("Unable to begin OTA");
      return;
    }
    else
    {
      Serial.println("OTA Begin");
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

          crc = esp_rom_crc32_le(crc, buffer, bytes_to_read);
          // double progress = ((double)downloaded / total_size) * 100.0;
          // Serial.printf("Downloaded: %u / %u bytes (%.2f%%)\n", downloaded, total_size, progress);
        }
      }

      delay(1);
    }
    Serial.printf("Original CRC: %u\n", original_crc);
    Serial.printf("Final CRC: %u\n", crc);

    esp_err_t ota_end_result = esp_ota_end(handler);

    if (ota_end_result == ESP_OK)
    {
      Serial.println("Firmware successfully downloaded");

      if (original_crc != crc)
      {
        Serial.println("Original CRC and Received File CRC found different");
        return;
      }

      if (esp_ota_set_boot_partition(next_partition) == ESP_OK)
      {
        Serial.print("Next boot partition: ");
      }
      else
      {
        Serial.print("Failed to change the boot partition to: ");
      }
      Serial.println(next_partition->label);
      Serial.println("Rebooting into new firmware...");
      delay(2000);
      esp_restart();
    }
    else
    {
      Serial.printf("Unable to end ota %x\n", ota_end_result);
    }
  }
  else
  {
    Serial.printf("Error code: %x", httpResponseCode);
  }
}

void loop()
{
}