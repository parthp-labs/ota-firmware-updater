#include <Arduino.h>
#include <esp_rom_crc.h>
#include "OTAUpdater.hpp"
#include <WiFi.h>
#include <string>

char ssid[] = "PARTH";
char pass[] = "iamparth";

std::string server = "http://10.197.157.89:8000/firmware.bin";
std::string checksum = "http://10.197.157.89:8000/firmware.crc32";
std::string checksum_update = "http://10.197.157.89:8000/firmware";

OTAUpdater updater(server, checksum, checksum_update);

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

  updater.start_ota_update_sequence(true);
}

void loop()
{
}