// worynim@gmail.com
#include "ds_launcher.h"

#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

namespace ds {

bool returnToLauncher() {
    const esp_partition_t* factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
    if (factory == nullptr || esp_ota_set_boot_partition(factory) != ESP_OK) return false;
    esp_restart();
    return true;  // 도달하지 않음
}

}  // namespace ds
