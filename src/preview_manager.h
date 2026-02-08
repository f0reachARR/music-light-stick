/*
 * Preview Manager - プレビューモード管理
 */

#ifndef PREVIEW_MANAGER_H
#define PREVIEW_MANAGER_H

#include "effect_types.h"
#include <stdbool.h>

/* プレビュー終了時のコールバック */
typedef void (*preview_exit_callback_t)(void);

/**
 * @brief Preview Managerを初期化
 * @return 0: 成功, 負値: エラー
 */
int preview_manager_init(void);

/**
 * @brief プレビュー終了コールバックを設定
 * @param cb コールバック関数
 */
void preview_set_exit_callback(preview_exit_callback_t cb);

/**
 * @brief プレビューモードを開始
 * @param preset プレビューするエフェクト
 */
void preview_start(const struct preset_data *preset);

/**
 * @brief プレビューモードを終了
 */
void preview_exit(void);

/**
 * @brief プレビュータイムアウトをリセット
 */
void preview_reset_timeout(void);

/**
 * @brief プレビューモード中かどうか
 * @return true: プレビューモード中, false: 通常モード
 */
bool preview_is_active(void);

/**
 * @brief 現在プレビュー中のエフェクトを取得
 * @param preset 出力先
 * @return 0: 成功, -EINVAL: プレビューモードでない
 */
int preview_get_current(struct preset_data *preset);

#endif /* PREVIEW_MANAGER_H */
