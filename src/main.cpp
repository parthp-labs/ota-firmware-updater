#include <Arduino.h>
#include <esp_rom_crc.h>
#include "OTAUpdater.hpp"
#include <WiFi.h>
#include <string>

char ssid[] = "PARTH";
char pass[] = "iamparth";

std::string server = "http://10.122.132.89:5000/api/v1/check-update?current_version=1.0";
std::string checksum = "http://10.197.157.89:8000/firmware.crc32";
std::string checksum_update = "http://10.197.157.89:8000/firmware";
std::string api = "5556273209c07bc50627abfe9fd914d0ee4edf7743ce2f50451f91007fcce3b8";

#define CHECK_PIN 21
#define START_PIN 22

OTAUpdater updater(server, checksum, checksum_update);

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
  if (digitalRead(CHECK_PIN) == LOW)
  {
    OTAResult result = updater.check_for_update();

    switch (result)
    {
    case OTAResult::HTTP_ERROR:
      Serial.println("HTTP Error");
      break;

    case OTAResult::OK:
      Serial.println("Update Available");
      break;

    case OTAResult::NO_UPDATE_AVAILABLE:
      Serial.println("Update unavailable");
      break;
    }
  }
}