/*
 * Power Manager - 電力管理
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

/* スリープ復帰時のコールバック */
typedef void (*power_wakeup_callback_t)(void);

/**
 * @brief Power Managerを初期化
 * @return 0: 成功, 負値: エラー
 */
int power_manager_init(void);

/**
 * @brief スリープ復帰コールバックを設定
 * @param cb コールバック関数
 */
void power_set_wakeup_callback(power_wakeup_callback_t cb);

/**
 * @brief スリープモードに移行
 * BLE広告停止、LED消灯、Deep Sleepへ遷移
 * NEXTボタン長押しで復帰
 */
void power_enter_sleep(void);

/**
 * @brief Deep Sleepからの復帰かどうかを確認
 * @return true: Deep Sleepからの復帰, false: 通常起動
 */
bool power_is_wakeup_from_sleep(void);

/**
 * @brief 復帰時のボタン長押し判定を実行
 * ボタンが長押しされていなければ再度スリープに戻る
 * @return true: 長押し確認済み（通常起動へ）, false: 再スリープ（戻らない）
 */
bool power_check_wakeup_condition(void);

/**
 * @brief バッテリー残量を取得 (0-100%)
 * @return バッテリー残量
 */
uint8_t power_get_battery_level(void);

/**
 * @brief バッテリー監視を開始
 */
void power_start_battery_monitor(void);

/**
 * @brief バッテリー監視を停止
 */
void power_stop_battery_monitor(void);

#endif /* POWER_MANAGER_H */
