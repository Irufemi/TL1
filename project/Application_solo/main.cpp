#include <Windows.h>
#include <memory>
#include "GameApplication.h"
/**
 * @brief Windowsアプリケーションのエントリーポイント
 * @param hInstance インスタンスハンドル
 * @param hPrevInstance 以前のインスタンスハンドル (常にNULL)
 * @param lpCmdLine コマンドライン引数
 * @param nCmdShow ウィンドウの表示状態
 * @return int 終了コード
 * @details この関数からGameApplicationを生成し、ゲームループを開始します。
 */
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    // ゲームアプリケーションの生成と実行
    auto game = std::make_unique<GameApplication>();
    game->Run();

    return 0;
}