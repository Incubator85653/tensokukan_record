#pragma once

#include "ProfileIO.hpp"

// �R�[�h�l�[���F�I�[���E�F�C�Y�L�^�ł��܂�(special thx:�C�l��) 
#define WINDOW_CLASS _T("AlwaysRecordable")
// �ď́F�V���� (special thx:�N������@���R��) 
#define WINDOW_TEXT  _T("�V���� Rev.14")

bool MainWindow_Initialize();
bool MainWindow_PreTranslateMessage(MSG &msg);
