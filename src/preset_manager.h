/*
 * Preset Manager - プリセット永続化管理
 */

#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include "effect_types.h"

/**
 * @brief Preset Managerを初期化
 * @return 0: 成功, 負値: エラー
 */
int preset_manager_init(void);

/**
 * @brief プリセットを取得
 * @param index プリセット番号 (0-19)
 * @param preset 出力先
 * @return 0: 成功, 負値: エラー
 */
int preset_get(uint8_t index, struct preset_data *preset);

/**
 * @brief プリセットを設定 (RAMのみ)
 * @param index プリセット番号 (0-19)
 * @param preset プリセットデータ
 * @return 0: 成功, 負値: エラー
 */
int preset_set(uint8_t index, const struct preset_data *preset);

/**
 * @brief 全プリセットを不揮発性メモリに保存
 * @return 0: 成功, 負値: エラー
 */
int preset_save_all(void);

/**
 * @brief 現在のプリセット番号を取得
 * @return 現在のプリセット番号 (0-19)
 */
uint8_t preset_get_current_index(void);

/**
 * @brief 現在のプリセット番号を設定
 * @param index プリセット番号 (0-19)
 */
void preset_set_current_index(uint8_t index);

/**
 * @brief 次のプリセットに移動
 * @return 新しいプリセット番号
 */
uint8_t preset_next(void);

/**
 * @brief 前のプリセットに移動
 * @return 新しいプリセット番号
 */
uint8_t preset_prev(void);

#endif /* PRESET_MANAGER_H */
