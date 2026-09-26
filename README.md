# AVT-C875 Recording Follow Source for OBS

AVerMedia Live Gamer Portable C875 / AVT-C875を、RECentralの録画中MPEG-TSへ
追従することでOBS Studioから利用するWindows向けツールです。

> [!IMPORTANT]
> 現在は実機検証中のアルファ版です。RECentralやOBSの更新で動かなくなる可能性があります。
> 録画ファイルを読み取り専用で扱い、RECentral本体のファイルは変更しません。

## 仕組み

```text
ゲーム機 → HDMI → C875
                    ├─ HDMI OUT → モニター（プレイヤー用）
                    └─ USB → RECentral → 録画中TS
                                           ↓
                               C875 録画追従ソース
                                           ↓
                                          OBS
```

C875のLGP Stream EngineをOBSから直接利用せず、C875のハードウェアH.264録画を
再利用します。映像には約5～10秒の遅延がありますが、滑らかさと安定性を優先します。

## 実機で確認できた内容

- Windows 10 22H2
- OBS Studio 32.2.1 64bit
- RECentral 1.3.0.118
- MPEG-TS / H.264 High Profile / 1920x1080 30fps
- AAC-LC / 48kHz / stereo
- 約6秒遅延で録画中TSへ追従
- 録画中の映像・音声デコード

## インストール

まずは[Windowsインストーラー](release/AVT-C875-Follow-Source-Setup.exe)をダウンロードし、OBSと
RECentralを終了した状態で実行します。管理者権限が必要です。

インストーラーを使わない場合は[手動インストール用ZIP](release/AVT-C875-Follow-Source-manual.zip)も
利用できます。

インストーラーは次を配置します。

- OBSプラグイン: `C:\ProgramData\obs-studio\plugins\c875-follow-source`
- ロック解除ツール: `C:\Program Files\AVT-C875 Follow Source`
- スタートメニュー: `AVT-C875 Follow Source`

## 使い方

### 1. OBSを起動する

OBSで`C875 録画追従ソース`を追加します。標準設定ではプラグインがRECentralを起動し、
共有解除を適用してから録画ホットキーを送信します。RECentralを先に手動起動する必要はありません。

### 2. 自動起動・共有解除・録画開始を確認する

プラグインがRECentralを検出すると、録画TSを別プロセスから読めるように共有フックを
自動適用します。通常は操作不要です。権限不足の場合だけUACが表示されるため、`はい`を
選択してください。

共有解除が成功するとRECentral標準の録画ホットキー`L`を送信します。RECentral側で録画
ホットキーを変更している場合は、現在のアルファ版では`L`へ戻してください。

OBSログに次の表示があれば成功です。

```text
[c875-follow] RECentral recording sharing was unlocked automatically
```

この処理は、RECentralが書き込み目的で開く`.ts`だけに`FILE_SHARE_READ`を追加します。
RECentralを再起動した場合も、新しいプロセスを検出して自動的に再適用します。
うまくいかない場合に限り、スタートメニューの`Enable RECentral TS sharing`を手動実行できます。

### 3. 新しいTSへ自動追従する

RECentral側の録画形式はあらかじめMPEG-TSに設定してください。プラグインは録画開始後に
作られた新しいTSを検出し、自動的に再生対象を切り替えます。

### 4. OBSへソースを追加する

OBSのソース欄で次を選択します。

```text
＋ → C875 録画追従ソース
```

設定項目:

- `RECentral 録画フォルダ`: 通常は`%USERPROFILE%\Videos\Captures`
- `目標遅延`: 初期値5000ms。安定性重視なら7000～10000ms
- `ハードウェアデコード`: 問題がある場合だけOFF
- `RECentralを自動起動`: 通常はON
- `自動起動したRECentralで録画を開始・停止`: 通常はON
- `終了時に今回のTSファイルを削除`: 初期値OFF。容量を残したくない場合だけON
- `RECentral.exeの場所`: 通常は自動入力されたままで利用可能

OBSを終了するかソースを削除すると、プラグインが開始した録画だけを停止します。削除設定が
ONの場合も、今回のセッションで新しく作成されたTSに限定し、RECentralがファイルを解放して
削除可能になった後だけ削除します。既存TSや書き込み中のTSを強制削除することはありません。

ソース作成時にフォルダ内で最後に更新されたTSを選び、Live Edgeのおよそ5秒前から再生します。

### 5. 音声を確認する

OBSの音声ミキサーに`C875 録画追従ソース`が表示されることを確認します。
モニターからも音を出したい場合は、OBSの「オーディオの詳細プロパティ」で
音声モニタリングを設定してください。

## 現在の制限

- Windowsの権限状態によっては自動共有解除時にUAC確認が表示されます。
- RECentralの録画ホットキーは現在`L`を前提にしています。
- 長時間動作時のLive Edge再同期は未実装です。
- 現在はWindows 10/11・OBS 64bit専用です。
- C875以外のRECentral環境は未検証です。

## トラブルシュート

詳細は[docs/troubleshooting.md](docs/troubleshooting.md)を参照してください。

## ビルド

必要なもの:

- Visual Studio 2022 Build Tools（C++）
- Windows 10/11 SDK
- CMake 3.30以降
- Inno Setup 6
- Git

管理者でないPowerShellから次を実行します。

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-release.ps1
```

OBSプラグインの初回構成では、OBSソースとビルド依存関係をダウンロードします。

## ライセンス

このプロジェクトはGPL-2.0で配布します。MinHookなどの依存関係については
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を参照してください。
