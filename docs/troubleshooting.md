# トラブルシュート

## 録画中TSを開けない / Permission denied

共有フックが録画開始前に適用されていない可能性があります。

1. 録画を停止する
2. RECentralを終了する
3. RECentralを起動する
4. OBSで`C875 録画追従ソース`を表示し、UACが出た場合は許可する
5. 新しい録画を開始する

自動適用に失敗する場合は、スタートメニューの`Enable RECentral TS sharing`を管理者実行して
手動で適用できます。

すでに開かれているファイルの共有モードは後から変更できません。

ログは次に出力されます。

```text
%TEMP%\recentral-share-hook-<PID>.jsonl
```

正常例:

```json
{"event":"create_file","original_share_mode":0,"patched_share_mode":1,"success":true}
```

## インジェクタがAccess deniedになる

RECentralのバックエンドが管理者権限で動いている場合、自動処理がUACを表示します。
`はい`を選択してください。失敗する場合はスタートメニューのランチャーを管理者実行します。

## OBSのソース一覧に表示されない

- OBSが64bit版か確認する
- OBSを完全に終了して再起動する
- 次のDLLが存在するか確認する

```text
C:\ProgramData\obs-studio\plugins\c875-follow-source\bin\64bit\c875-follow-source.dll
```

OBSログは`%APPDATA%\obs-studio\logs`にあります。`c875-follow`で検索してください。

## 映像が表示されない

- RECentralがTS形式で録画中か確認する
- 録画フォルダが正しいか確認する
- TSのファイルサイズが増加しているか確認する
- ソースのプロパティを開き、再度OKを押して最新TSを読み直す

## RECentralは起動するが録画が始まらない

RECentralの録画ホットキーが`L`になっているか確認してください。現在のアルファ版は、インストール
済みの`Profile.xml`で確認できた標準設定の`L`を使用します。また、C875の入力準備が完了する前に
ホットキーが送られると開始できない場合があります。その場合はOBSログの
`Sent RECentral recording hotkey`付近を確認してください。

すでに手動起動されていたRECentralには、録画中・停止中を誤判定してトグルを反転させないため、
プラグインから録画ホットキーを送りません。完全自動で使う場合はRECentralを終了してからOBSの
ソースを有効にしてください。

## 終了時にTSが削除されない

安全のため、削除対象はプラグインが録画開始後に新規検出したTSだけです。録画停止後もRECentralや
デコーダーがファイルを保持している場合は削除せず、OBSログへ`Session TS deletion failed`を残します。
- ハードウェアデコードをOFFにする

## 音が聞こえない

OBSの音声ミキサーにレベルが出ているか確認します。OBSから自分にも聞こえるようにする場合は、
「オーディオの詳細プロパティ」からモニタリングを有効にします。

## 再生がLive Edgeへ追いついて止まる

目標遅延を7000～10000msへ増やしてください。将来版ではLive Edgeとの距離を監視して
自動再同期する予定です。

## RECentralを再起動した

フックはプロセス内だけに存在しますが、プラグインが新しいRECentralプロセスを検出して
自動的に再適用します。録画は自動適用後に開始してください。
