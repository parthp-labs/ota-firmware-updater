#pragma once
#include <stdlib.h>
#include <string>
#include "esp_ota_ops.h"

enum class OTAResult
{
    OK,
    WIFI_NOT_CONNECTED,
    HTTP_ERROR,
    CHECK_UPDATE_REQUEST_FAILED,
    NO_UPDATE_AVAILABLE,
    CHECKSUM_DOWNLOAD_FAILED,
    FIRMWARE_REQUEST_FAILED,
    OTA_BEGIN_FAILED,
    OTA_WRITE_FAILED,
    OTA_END_FAILED,
    CHECKSUM_MISMATCH,
    GET_MAC_FAILED,
    BOOT_PARTITION_CHANGE_FAILED
};
class OTAUpdater
{
private:
    std::string firmware_url;
    std::string checksum_url;
    std::string check_update_url;
    uint8_t device_mac;
    uint32_t original_checksum;
    uint32_t firmware_checksum;
    bool update_available;

public:
    OTAUpdater(std::string firmware_url, std::string checksum_url, std::string check_update_url);
    OTAResult check_wifi();
    OTAResult check_for_update();
    OTAResult download_checksum();
    OTAResult download_firmware();
    OTAResult verify_checksum();
    OTAResult change_bootorder();
    OTAResult start_ota_update_sequence(bool change_boot_order = false, bool reboot = false);
};