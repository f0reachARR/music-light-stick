/*
 * Effect Engine - エフェクト実行エンジン
 */

#ifndef EFFECT_ENGINE_H
#define EFFECT_ENGINE_H

#include "effect_types.h"

/**
 * @brief Effect Engineを初期化
 * @return 0: 成功, 負値: エラー
 */
int effect_engine_init(void);

/**
 * @brief エフェクトを設定して開始
 * @param preset エフェクトデータ
 */
void effect_engine_set(const struct preset_data *preset);

/**
 * @brief エフェクトを停止
 */
void effect_engine_stop(void);

/**
 * @brief エフェクトが実行中かどうか
 * @return true: 実行中, false: 停止中
 */
bool effect_engine_is_running(void);

#endif /* EFFECT_ENGINE_H */
