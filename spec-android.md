# ペンライトコントロールアプリ Android仕様書

## 1. 概要

RGBW LED搭載ペンライトをBluetooth Low Energy (BLE)で制御するAndroidアプリケーションの仕様書。

### 1.1 主な機能

- BLE経由でペンライトとの接続・切断
- カラーピッカーによる色選択とリアルタイムプレビュー
- 20個のカラープリセットの管理（読み出し・保存・削除）
- 現在選択中のプリセットの表示
- バッテリー残量の表示
- ペンライト本体のボタン操作との同期

---

## 2. アーキテクチャ

### 2.1 推奨アーキテクチャ

- **MVVM (Model-View-ViewModel)** パターン
- **Repository** パターンでBLE通信を抽象化
- **Kotlin Coroutines / Flow** で非同期処理

### 2.2 主要コンポーネント

```
┌─────────────────┐
│   UI Layer      │  Jetpack Compose / Fragment
│  (View/Screen)  │
└────────┬────────┘
         │
┌────────▼────────┐
│   ViewModel     │  状態管理、UIロジック
└────────┬────────┘
         │
┌────────▼────────┐
│   Repository    │  データ操作の抽象化
└────────┬────────┘
         │
┌────────▼────────┐
│  BLE Service    │  BluetoothGatt通信
└─────────────────┘
```

---

## 3. 画面設計

### 3.1 画面一覧

| 画面名 | 概要 |
|--------|------|
| デバイススキャン画面 | BLEデバイスの検索・接続 |
| メイン画面 | カラー選択・プリセット管理 |
| プリセット一覧画面 | 全プリセットの閲覧・編集 |
| 設定画面（オプション） | アプリ設定 |

### 3.2 デバイススキャン画面

**機能**:

- BLEデバイスのスキャン開始/停止
- 検出されたデバイスをリスト表示
- デバイス名、アドレス、信号強度（RSSI）を表示
- タップで接続

**UI要素**:

```
┌─────────────────────────────────┐
│  ペンライトを検索              │
├─────────────────────────────────┤
│  [スキャン開始/停止ボタン]      │
├─────────────────────────────────┤
│  ┌───────────────────────────┐  │
│  │ Penlight #1234           │  │
│  │ XX:XX:XX:XX:XX:XX        │  │
│  │ RSSI: -65 dBm            │  │
│  └───────────────────────────┘  │
│  ┌───────────────────────────┐  │
│  │ Penlight #5678           │  │
│  │ XX:XX:XX:XX:XX:XX        │  │
│  │ RSSI: -72 dBm            │  │
│  └───────────────────────────┘  │
└─────────────────────────────────┘
```

**動作**:

1. スキャン開始ボタンタップ
2. BLEスキャン開始（サービスUUID `0000ff00-...` でフィルタリング推奨）
3. 検出されたデバイスをリスト表示
4. デバイスタップで接続処理開始
5. 接続成功でメイン画面へ遷移

### 3.3 メイン画面

**機能**:

- RGBWカラーピッカー
- リアルタイムプレビュー
- プリセット番号選択（0-19）
- プリセット保存/読み込み
- プレビュー終了
- バッテリー残量表示
- 接続状態表示

**UI要素**:

```
┌─────────────────────────────────┐
│  🔋 85%        [切断]            │
├─────────────────────────────────┤
│                                 │
│  ┌─────────────────────────┐    │
│  │                         │    │
│  │    カラーピッカー       │    │
│  │    (RGBW調整)          │    │
│  │                         │    │
│  └─────────────────────────┘    │
│                                 │
│  R: ███████████░░░░░ 180       │
│  G: █████░░░░░░░░░░░  80       │
│  B: ███████████████░ 240       │
│  W: ████░░░░░░░░░░░░  60       │
│                                 │
├─────────────────────────────────┤
│  現在のプリセット: [5 ▼]       │
│                                 │
│  [プレビュー終了] [保存]       │
│                                 │
│  [プリセット一覧を表示]        │
└─────────────────────────────────┘
```

**動作**:

1. **カラーピッカー操作時**
   - RGBWスライダー操作でPreview Color送信
   - 50-100ms間隔で送信（連続送信時）
   - LEDがリアルタイムで色を反映

2. **保存ボタン押下**
   - 選択中のプリセット番号に現在のRGBW値を書き込み
   - Preset Write送信: `[preset][R][G][B][W]`
   - 保存完了をトースト/Snackbarで通知

3. **プレビュー終了ボタン押下**
   - Exit Preview送信: `[0x01]`
   - ペンライトが元のプリセット色に戻る

4. **プリセット番号変更（ドロップダウン）**
   - 選択されたプリセット番号の色を読み出し
   - Preset Read送信: `[preset]`
   - 受信したRGBW値をカラーピッカーに反映

5. **Current Preset通知受信時**
   - ペンライト本体のボタン操作を検知
   - UIのプリセット番号を自動更新
   - 該当プリセットの色をカラーピッカーに反映

### 3.4 プリセット一覧画面

**機能**:

- 全20個のプリセットをグリッド/リスト表示
- 各プリセットのカラープレビュー
- タップで選択・編集
- 長押しで削除（初期値に戻す）

**UI要素**:

```
┌─────────────────────────────────┐
│  プリセット一覧                 │
├─────────────────────────────────┤
│  ┌───┬───┬───┬───┬───┐          │
│  │ 0 │ 1 │ 2 │ 3 │ 4 │          │
│  │███│   │███│███│   │          │
│  │赤 │緑 │黄 │青 │紫 │          │
│  ├───┼───┼───┼───┼───┤          │
│  │ 5 │ 6 │ 7 │ 8 │ 9 │          │
│  │███│   │   │   │   │          │
│  │橙 │   │   │   │   │          │
│  ├───┼───┼───┼───┼───┤          │
│  │...│...│...│...│...│          │
│  └───┴───┴───┴───┴───┘          │
│                                 │
│  [すべて読み込み]               │
└─────────────────────────────────┘
```

**動作**:

1. **画面表示時**
   - 全プリセット（0-19）をPreset Readで順次読み出し
   - 各カードに色をプレビュー表示

2. **プリセットタップ**
   - メイン画面に戻り、該当プリセットを選択状態に
   - カラーピッカーに色を反映

3. **プリセット長押し**
   - 削除確認ダイアログ表示
   - 削除実行: `[preset][0x00][0x00][0x00][0x00]` で初期化

---

## 4. BLE通信仕様

### 4.1 対応サービス・キャラクタリスティック

| 名称 | UUID | Property | 説明 |
|------|------|----------|------|
| Penlight Control Service | `0000ff00-0000-1000-8000-00805f9b34fb` | - | メインサービス |
| Preset Write | `0000ff01-...` | Write | プリセット書き込み |
| Preset Read | `0000ff02-...` | Read, Write | プリセット読み出し |
| Preview Color | `0000ff03-...` | Write | カラープレビュー |
| Current Preset | `0000ff04-...` | Read, Notify | 現在のプリセット |
| Exit Preview | `0000ff05-...` | Write | プレビュー終了 |
| Battery Level | `0000ff06-...` | Read, Notify | バッテリー残量 |

### 4.2 BLE Service実装概要

**クラス構成例**:

```kotlin
class PenlightBleService {
    private var bluetoothGatt: BluetoothGatt? = null
    private val serviceUuid = UUID.fromString("0000ff00-0000-1000-8000-00805f9b34fb")

    // キャラクタリスティックUUID
    private val presetWriteUuid = UUID.fromString("0000ff01-...")
    private val presetReadUuid = UUID.fromString("0000ff02-...")
    private val previewColorUuid = UUID.fromString("0000ff03-...")
    private val currentPresetUuid = UUID.fromString("0000ff04-...")
    private val exitPreviewUuid = UUID.fromString("0000ff05-...")
    private val batteryLevelUuid = UUID.fromString("0000ff06-...")

    suspend fun connect(device: BluetoothDevice): Result<Unit>
    suspend fun disconnect()
    suspend fun writePreset(preset: Int, r: Int, g: Int, b: Int, w: Int): Result<Unit>
    suspend fun readPreset(preset: Int): Result<RgbwColor>
    suspend fun sendPreviewColor(r: Int, g: Int, b: Int, w: Int): Result<Unit>
    suspend fun exitPreview(): Result<Unit>
    suspend fun readBatteryLevel(): Result<Int>
    fun observeCurrentPreset(): Flow<Int>
    fun observeBatteryLevel(): Flow<Int>
}
```

**データクラス**:

```kotlin
data class RgbwColor(
    val r: Int, // 0-255
    val g: Int, // 0-255
    val b: Int, // 0-255
    val w: Int  // 0-255
)

data class PresetData(
    val presetNumber: Int, // 0-19
    val color: RgbwColor
)
```

### 4.3 通信フロー

**接続フロー**:

```
1. BluetoothDevice.connectGatt()
2. onConnectionStateChange(STATE_CONNECTED)
3. discoverServices()
4. onServicesDiscovered()
5. enableNotifications(currentPresetUuid)
6. enableNotifications(batteryLevelUuid)
7. 接続完了
```

**プリセット書き込みフロー**:

```
1. UIから保存リクエスト
2. writePreset(preset=5, r=255, g=128, b=0, w=0)
3. ByteArray[5] = [0x05, 0xFF, 0x80, 0x00, 0x00]
4. BluetoothGattCharacteristic.setValue(data)
5. BluetoothGatt.writeCharacteristic()
6. onCharacteristicWrite()コールバック
7. 成功/失敗を通知
```

**プレビュー送信フロー**:

```
1. カラーピッカー操作
2. sendPreviewColor(r=100, g=200, b=50, w=30)
3. ByteArray[4] = [0x64, 0xC8, 0x32, 0x1E]
4. BluetoothGattCharacteristic.setValue(data)
5. BluetoothGatt.writeCharacteristic()
6. (連続送信時は前回の完了を待つ)
```

**通知受信フロー**:

```
1. onCharacteristicChanged()コールバック
2. UUIDで判定 (currentPresetUuid or batteryLevelUuid)
3. ByteArrayをパース
4. Flow/LiveDataで状態を更新
5. UIに反映
```

---

## 5. データ管理

### 5.1 ViewModel

**MainViewModel**:

```kotlin
class MainViewModel : ViewModel() {
    private val bleService = PenlightBleService()

    // UI State
    val connectionState: StateFlow<ConnectionState>
    val currentPreset: StateFlow<Int>
    val currentColor: StateFlow<RgbwColor>
    val batteryLevel: StateFlow<Int>
    val presets: StateFlow<List<PresetData>>

    // Actions
    fun connect(device: BluetoothDevice)
    fun disconnect()
    fun updatePreviewColor(color: RgbwColor)
    fun saveToPreset(preset: Int, color: RgbwColor)
    fun loadPreset(preset: Int)
    fun exitPreview()
    fun loadAllPresets()
}

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
}
```

### 5.2 Repository

**PenlightRepository**:

```kotlin
class PenlightRepository(
    private val bleService: PenlightBleService
) {
    suspend fun writePreset(presetData: PresetData): Result<Unit>
    suspend fun readPreset(presetNumber: Int): Result<RgbwColor>
    suspend fun previewColor(color: RgbwColor): Result<Unit>
    suspend fun exitPreview(): Result<Unit>
    fun observeCurrentPreset(): Flow<Int>
    fun observeBatteryLevel(): Flow<Int>
}
```

---

## 6. 権限管理

### 6.1 必要な権限

**AndroidManifest.xml**:

```xml
<!-- Android 12 (API 31) 以上 -->
<uses-permission android:name="android.permission.BLUETOOTH_SCAN"
    android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

<!-- Android 11 (API 30) 以下 -->
<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />

<uses-feature android:name="android.hardware.bluetooth_le" android:required="true"/>
```

### 6.2 ランタイム権限リクエスト

**Android 12以上**:

- `BLUETOOTH_SCAN`
- `BLUETOOTH_CONNECT`

**Android 11以下**:

- `ACCESS_FINE_LOCATION` (BLEスキャンに必須)

**実装例**:

```kotlin
fun requestBluetoothPermissions(activity: Activity) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        ActivityCompat.requestPermissions(
            activity,
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            ),
            REQUEST_CODE_BLE
        )
    } else {
        ActivityCompat.requestPermissions(
            activity,
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION),
            REQUEST_CODE_BLE
        )
    }
}
```

---

## 7. エラーハンドリング

### 7.1 エラーケース

| エラー種別 | 検出タイミング | 対処 |
|-----------|--------------|------|
| BLE未対応 | 起動時 | エラーダイアログ→アプリ終了 |
| Bluetooth無効 | スキャン開始時 | Bluetooth有効化リクエスト |
| 権限なし | スキャン開始時 | 権限リクエスト |
| 接続タイムアウト | 接続試行30秒後 | エラー表示、再接続ボタン |
| 接続切断 | 通信中 | 再接続ダイアログ |
| 書き込み失敗 | 各Write操作時 | リトライ（最大3回） |
| サービス未発見 | 接続後 | 非対応デバイスエラー |

### 7.2 リトライロジック

```kotlin
suspend fun writeWithRetry(
    writeAction: suspend () -> Result<Unit>,
    maxRetries: Int = 3
): Result<Unit> {
    repeat(maxRetries) { attempt ->
        writeAction().onSuccess {
            return Result.success(Unit)
        }
        if (attempt < maxRetries - 1) {
            delay(500)
        }
    }
    return Result.failure(Exception("Write failed after $maxRetries attempts"))
}
```

---

## 8. UI/UX仕様

### 8.1 カラーピッカー

**実装オプション**:

1. **4本のスライダー方式**（推奨）
   - R, G, B, W 各チャンネルを独立スライダーで調整
   - 各スライダー: 0-255の範囲
   - リアルタイムプレビュー表示

2. **RGBカラーホイール + Wスライダー**
   - RGBはカラーホイールで選択
   - Wは独立スライダー

**プレビュー送信の最適化**:

```kotlin
val colorFlow = MutableStateFlow(RgbwColor(0, 0, 0, 0))

colorFlow
    .debounce(100) // 100ms間隔で間引き
    .distinctUntilChanged()
    .collect { color ->
        bleService.sendPreviewColor(color.r, color.g, color.b, color.w)
    }
```

### 8.2 接続状態表示

| 状態 | アイコン/色 | 説明 |
|------|-----------|------|
| 切断中 | 灰色 | BLE未接続 |
| 接続中 | 黄色アニメーション | 接続処理中 |
| 接続済み | 緑色 | 通信可能 |
| エラー | 赤色 | 接続失敗 |

### 8.3 フィードバック

- **Snackbar**: 一時的な操作結果通知
  - "プリセット5に保存しました"
  - "接続が切断されました"

- **Toast**: 簡易通知
  - "プレビュー終了"

- **Progress Indicator**: 長時間処理
  - 全プリセット読み込み時
  - 接続中

- **Haptic Feedback**: 触覚フィードバック
  - ボタン押下時（軽い振動）

---

## 9. セキュリティ・プライバシー

### 9.1 BLEペアリング

- **ペアリング不要**: 本仕様ではボンディング不要
- **暗号化なし**: プロトコル上暗号化されていないため、機密情報は送信しない

### 9.2 位置情報

- Android 11以下ではBLEスキャンに位置情報権限が必要
- 実際の位置情報は取得・送信しない
- プライバシーポリシーに明記

---

## 10. テスト仕様

### 10.1 ユニットテスト

- **BLE通信レイヤー**:
  - データ変換（Int → ByteArray）
  - ByteArrayパース
  - UUID定義の正確性

- **ViewModel**:
  - 状態管理ロジック
  - Flow/LiveDataの更新

### 10.2 UIテスト

- **デバイススキャン**:
  - スキャン開始/停止
  - デバイスリスト表示

- **メイン画面**:
  - カラーピッカー操作
  - プリセット保存/読み込み
  - プレビュー終了

### 10.3 統合テスト

- **実機テスト**:
  - 実ペンライトデバイスとの接続
  - プリセット書き込み/読み出し
  - プレビュー表示
  - ボタン押下時の通知受信

---

## 11. ビルド・デプロイ

### 11.1 依存ライブラリ

**build.gradle (app)**:

```gradle
dependencies {
    // AndroidX
    implementation "androidx.core:core-ktx:1.12.0"
    implementation "androidx.lifecycle:lifecycle-viewmodel-ktx:2.6.2"
    implementation "androidx.lifecycle:lifecycle-runtime-ktx:2.6.2"

    // Jetpack Compose (推奨)
    implementation platform("androidx.compose:compose-bom:2024.01.00")
    implementation "androidx.compose.ui:ui"
    implementation "androidx.compose.material3:material3"
    implementation "androidx.compose.ui:ui-tooling-preview"

    // Coroutines
    implementation "org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3"

    // BLE (Android標準APIを使用)
    // 追加ライブラリ不要

    // Optional: BLE通信補助ライブラリ
    implementation "com.github.NordicSemiconductor:Android-BLE-Library:2.6.1"
}
```

### 11.2 ProGuard設定

**proguard-rules.pro**:

```proguard
# Bluetooth classes
-keep class android.bluetooth.** { *; }
-keep class java.util.UUID { *; }

# Coroutines
-keepnames class kotlinx.coroutines.internal.MainDispatcherFactory {}
-keepnames class kotlinx.coroutines.CoroutineExceptionHandler {}
```

---

## 12. 将来拡張

### 12.1 追加機能候補

- **プリセット共有**: JSON/QRコードで他ユーザーと共有
- **カラーパレット**: よく使う色をお気に入り登録
- **パターンモード**: 点滅、グラデーションパターンの作成・送信
- **複数デバイス制御**: 同時に複数のペンライトを制御
- **OTA更新対応**: ファームウェア更新機能

### 12.2 UI改善

- **ダークモード**: システム設定に追従
- **アニメーション**: スムーズな画面遷移
- **アクセシビリティ**: TalkBack対応、カラーブラインド対応

---

## 13. リファレンス

### 13.1 参照ドキュメント

- [Android BLE公式ガイド](https://developer.android.com/guide/topics/connectivity/bluetooth-le)
- [Bluetooth GATT仕様](https://www.bluetooth.com/specifications/gatt/)
- ペンライトBLEプロトコル仕様書 (spec.md)

### 13.2 サンプルコード

**BLE接続サンプル**:

```kotlin
private val gattCallback = object : BluetoothGattCallback() {
    override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
        when (newState) {
            BluetoothProfile.STATE_CONNECTED -> {
                gatt.discoverServices()
            }
            BluetoothProfile.STATE_DISCONNECTED -> {
                // Handle disconnection
            }
        }
    }

    override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
        if (status == BluetoothGatt.GATT_SUCCESS) {
            val service = gatt.getService(serviceUuid)
            service?.let {
                // Setup characteristics
                enableNotifications(it.getCharacteristic(currentPresetUuid))
            }
        }
    }

    override fun onCharacteristicChanged(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic
    ) {
        when (characteristic.uuid) {
            currentPresetUuid -> {
                val preset = characteristic.value[0].toInt() and 0xFF
                // Update UI
            }
            batteryLevelUuid -> {
                val battery = characteristic.value[0].toInt() and 0xFF
                // Update UI
            }
        }
    }
}

fun connect(device: BluetoothDevice) {
    bluetoothGatt = device.connectGatt(context, false, gattCallback)
}
```

**プリセット書き込みサンプル**:

```kotlin
suspend fun writePreset(preset: Int, r: Int, g: Int, b: Int, w: Int): Result<Unit> {
    val service = bluetoothGatt?.getService(serviceUuid) ?: return Result.failure(Exception("Service not found"))
    val characteristic = service.getCharacteristic(presetWriteUuid) ?: return Result.failure(Exception("Characteristic not found"))

    val data = byteArrayOf(
        preset.toByte(),
        r.toByte(),
        g.toByte(),
        b.toByte(),
        w.toByte()
    )

    characteristic.value = data

    return suspendCoroutine { continuation ->
        if (bluetoothGatt?.writeCharacteristic(characteristic) == true) {
            // Wait for callback in onCharacteristicWrite
            continuation.resume(Result.success(Unit))
        } else {
            continuation.resume(Result.failure(Exception("Write failed")))
        }
    }
}
```

---

## 14. バージョン情報

| バージョン | 日付 | 変更内容 |
|-----------|------|---------|
| 1.0 | 2025-10-19 | 初版作成 |
