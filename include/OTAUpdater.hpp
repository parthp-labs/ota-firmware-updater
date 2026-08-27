#pragma once
#include <stdlib.h>
#include <string>
#include "esp_ota_ops.h"

class OTAUpdater
{
private:
    std::string firmware_url;
    std::string checksum_url;
    std::string check_update_url;
    uint32_t original_checksum;
    uint32_t firmware_checksum;
    bool update_available;

public:
    OTAUpdater(std::string firmware_url, std::string checksum_url, std::string check_update_url);
    bool check_for_update();
    void download_checksum();
    void download_firmware();
    bool verify_checksum();
    void change_bootorder();
};