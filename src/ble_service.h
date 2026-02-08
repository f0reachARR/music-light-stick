/*
 * BLE Service - Penlight Control BLE GATT Service
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <stdint.h>

/**
 * @brief BLE Serviceを初期化・開始
 * @return 0: 成功, 負値: エラー
 */
int ble_service_init(void);

/**
 * @brief BLE広告を開始
 * @return 0: 成功, 負値: エラー
 */
int ble_start_advertising(void);

/**
 * @brief BLE広告を停止
 * @return 0: 成功, 負値: エラー
 */
int ble_stop_advertising(void);

/**
 * @brief Current Presetを通知
 * @param preset_index 現在のプリセット番号
 */
void ble_notify_current_preset(uint8_t preset_index);

/**
 * @brief Battery Levelを通知
 * @param level バッテリー残量 (0-100%)
 */
void ble_notify_battery_level(uint8_t level);

#endif /* BLE_SERVICE_H */
