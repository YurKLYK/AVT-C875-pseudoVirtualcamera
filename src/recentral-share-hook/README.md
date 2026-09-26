# RECentral TS read-sharing PoC

RECentralが録画用に開く `.ts` ファイルだけを対象に、Windowsの共有モードへ
`FILE_SHARE_READ`を追加する検証ツールです。RECentral本体のファイルは変更しません。

## Build

```bat
build.cmd
```

初回ビルド時にMinHookを `.deps/minhook` へ取得します。生成物は32bitです。

## Test

1. RECentralを起動する（まだ録画を開始しない）。
2. `inject-admin.cmd`を実行してUACを許可する。あるいは管理者権限の
   ターミナルから次を実行する。

```bat
build\recentral_share_injector.exe
```

3. 新しいTS録画を開始する。
4. 録画中に別のターミナルから `ffprobe` または `ffplay` でTSを開く。

```bat
ffprobe -v warning -show_streams -show_format -of json "C:\path\recording.ts"
```

最新の録画中TSを約5秒遅れで追従再生する場合:

```powershell
powershell -ExecutionPolicy Bypass -File .\test-follow.ps1
```

遅延を変更する場合:

```powershell
powershell -ExecutionPolicy Bypass -File .\test-follow.ps1 -TargetLatencySeconds 10
```

ログは `%TEMP%\recentral-share-hook-<pid>.jsonl` に出力されます。

## Safety

- 書き込みアクセスを要求する `.ts` のみ対象です。
- 元の共有モードへ `FILE_SHARE_READ` を追加する以外は変更しません。
- RECentralを終了するとフックも消えます。
- 録画中ファイルは読み取り専用で扱ってください。

## Later integration

将来OBSと統合する場合は、可視cmdを毎回起動するのではなく、権限を持つ小さな
ヘルパーがRECentralの起動を検出してフックを適用する構成を検討します。
