#include <Arduino.h>
#include <esp_rom_crc.h>
#include "OTAUpdater.hpp"
#include <WiFi.h>
#include <string>

char ssid[] = "PARTH";
char pass[] = "iamparth";

std::string firmware = "http://10.0.107.68:5000/api/v1/download-firmware";
std::string checksum = "http://10.0.107.68:5000/api/v1/checksum";
std::string check_update = "http://10.0.107.68:5000/api/v1/check-update?current_version=1";
std::string api = "5634e88e05ea12e4f475f2b71542db988031cc6b49541cc982743521dd3b73f2";

#define CHECK_PIN 21
#define START_PIN 22

OTAUpdater updater(firmware, checksum, check_update, api);

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(CHECK_PIN, INPUT_PULLUP);
  pinMode(START_PIN, INPUT);

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
}

void loop()
{
  OTAResult result = updater.check_for_update();

  switch (result)
  {
  case OTAResult::HTTP_ERROR:
    Serial.println("HTTP Error");
    break;
  case OTAResult::OK:
  {
    Serial.println("Update Available");
    OTAResult update = updater.download_firmware();
    switch (update)
    {
    case OTAResult::OK:
      Serial.println("Ok");
      OTAResult change_bootorder_result = updater.change_bootorder();

      if (change_bootorder_result != OTAResult::OK)
      {
        Serial.println("Unable to change bootorder");
        break;
      }

      Serial.println("Bootorder changed successfully");
      break;
    case OTAResult::NO_UPDATE_AVAILABLE:
      Serial.println("Update unavailable");
      break;
    case OTAResult::INVALID_API_KEY:
      Serial.println("Invalid API key");
    default:
      Serial.println("Failed to start");
      break;
    }
    break;
  }
  default:
    break;
  }

  delay(5000);
}