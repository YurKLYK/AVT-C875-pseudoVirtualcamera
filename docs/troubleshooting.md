# トラブルシュート

## 録画中TSを開けない / Permission denied

共有フックが録画開始前に適用されていません。

1. 録画を停止する
2. RECentralを終了する
3. RECentralを起動する
4. `Enable RECentral TS sharing`を管理者実行する
5. 新しい録画を開始する

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

RECentralのバックエンドが管理者権限で動いています。スタートメニューのランチャーから
実行してUACを許可してください。

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
- ハードウェアデコードをOFFにする

## 音が聞こえない

OBSの音声ミキサーにレベルが出ているか確認します。OBSから自分にも聞こえるようにする場合は、
「オーディオの詳細プロパティ」からモニタリングを有効にします。

## 再生がLive Edgeへ追いついて止まる

目標遅延を7000～10000msへ増やしてください。将来版ではLive Edgeとの距離を監視して
自動再同期する予定です。

## RECentralを再起動した

フックはプロセス内だけに存在するため、再度`Enable RECentral TS sharing`を実行してください。

