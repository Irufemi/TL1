@echo off
chcp 65001 > nul
rem 文字化け防止のためUTF-8に設定

set "WORKSPACE_FILE=%~dp0level_editor"
set "BLENDER_DIR=C:\Blender Foundation\Blender 4.5\4.5\scripts\addons_core"
set "BLENDER_FILE=%BLENDER_DIR%\level_editor"

echo ==========================================
echo Blender Level Editor 同期ツール
echo ==========================================
echo 1: 【Deploy】 ワークスペース(ここ) から Blender へ反映する
echo 2: 【Pull】   Blender から ワークスペース(ここ) へ取り込む
echo 0: キャンセル
echo ==========================================
set /p choice="実行する処理の番号を入力してください: "

if "%choice%"=="1" (
    echo.
    echo ワークスペースからBlenderへコピーしています...
    if not exist "%BLENDER_FILE%" mkdir "%BLENDER_FILE%"
    xcopy /Y /E /I "%WORKSPACE_FILE%" "%BLENDER_FILE%"
    if %ERRORLEVEL% equ 0 (
        echo [成功] Blender側に反映しました。Blender上でアドオンをリロードしてください。
    ) else (
        echo [失敗] コピーに失敗しました。管理者権限が必要か、ファイルがロックされている可能性があります。
    )
) else if "%choice%"=="2" (
    echo.
    echo Blenderからワークスペースへコピーしています...
    if not exist "%WORKSPACE_FILE%" mkdir "%WORKSPACE_FILE%"
    xcopy /Y /E /I "%BLENDER_FILE%" "%WORKSPACE_FILE%"
    if %ERRORLEVEL% equ 0 (
        echo [成功] ワークスペースに取り込みました。
    ) else (
        echo [失敗] コピーに失敗しました。
    )
) else (
    echo.
    echo 処理をキャンセルしました。
)

echo.
pause
