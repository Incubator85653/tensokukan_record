#pragma once

#include "ProfileIO.hpp"

// コードネーム：オールウェイズ記録できます(special thx:イネ科) 
#define WINDOW_CLASS _T("AlwaysRecordable")
// 呼称：天則観 (special thx:誰かさん@下コメ) 
#define WINDOW_TEXT  _T("天則観 Rev.14")

bool MainWindow_Initialize();
bool MainWindow_PreTranslateMessage(MSG &msg);
