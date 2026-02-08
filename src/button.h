/*
 * Button Handler - ボタン入力処理
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

/* ボタンID */
enum button_id {
    BUTTON_NEXT = 0,
    BUTTON_PREV = 1,
    BUTTON_COUNT
};

/* ボタンイベント種類 */
enum button_event {
    BUTTON_EVT_SHORT_PRESS,   /* 短押し */
    BUTTON_EVT_LONG_PRESS,    /* 長押し (3秒) */
};

/* ボタンイベントコールバック */
typedef void (*button_callback_t)(enum button_id btn, enum button_event evt);

/**
 * @brief Button Handlerを初期化
 * @return 0: 成功, 負値: エラー
 */
int button_init(void);

/**
 * @brief ボタンイベントコールバックを登録
 * @param cb コールバック関数
 */
void button_set_callback(button_callback_t cb);

/**
 * @brief ボタンの現在の押下状態を取得
 * @param btn ボタンID
 * @return true: 押下中, false: 離されている
 */
bool button_is_pressed(enum button_id btn);

#endif /* BUTTON_H */
