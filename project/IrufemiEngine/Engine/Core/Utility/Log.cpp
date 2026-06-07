#include "Log.h"
#include <Windows.h>

/*ログを出そう*/

#include <format>

//ファイルやディレクトリに関する操作を行うライブラリ
#include <filesystem>

//ファイルに書いたり読んだりするライブラリ
#include <fstream>

//時間を扱うライブラリ
#include <chrono>

void Log::Initialize() {

    /*ログを出そう*/

    //ログのディレクトリを用意
    std::filesystem::create_directories("../../logs/app");

    //現在時刻を取得
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    //ログファイルの名前にコンマ何秒はいらないので、削って秒にする
    std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    // 日本時間(PCの設定時間)に変換
    std::chrono::zoned_time localTime{ std::chrono::current_zone(),nowSeconds };
    //formatを使って毎月日_時分秒の文字列に変換
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
    //時刻を使ってファイル名を決定
    std::string logFilePath = std::string("../../logs/app/") + dateString + ".log";
    //ファイルを使って書き込み準備
    logStream.open(logFilePath, std::ios::out | std::ios::trunc);

    //出力ウィンドウへの文字出力
    OutputDebugStringA("Hello,DirectX!\n");
}


/*ログを出そう*/

//出力ウィンドウに文字を出す
void Log::OutPutLog(std::ostream& os, const std::string& message) {
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
}
