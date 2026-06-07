#pragma once  

#include <fstream> 
#include <ostream>  
#include <string>  

class Log
{
private: // メンバ変数  
    std::ofstream logStream;
public: // メンバ関数  
    /// <summary>  
    /// 初期化  
    /// </summary>  
    void Initialize();

    // ゲッター  
    std::ofstream& GetLogStream() { return logStream; }

    // 出力ウィンドウに文字を出す  
    static void OutPutLog(std::ostream& os, const std::string& message);
};