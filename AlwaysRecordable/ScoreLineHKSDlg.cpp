#include "pch.hpp"
#include "AlwaysRecordable.hpp"
#include "MainWnd.hpp"
#include "ScoreLineDlg.hpp"
#include "ScoreLineQIBFilterDlg.hpp"
#include "RankProfDlg.hpp"
#include "TrackRecordDlg.hpp"
#include "ScoreLine.hpp"
#include "Shortcut.hpp"
#include "Characters.hpp"
#include "Formatter.hpp"
#include "Memento.hpp"
#include "SWRSAddrDef.h"
#include "resource.h"

#define MINIMAL_USE_PROCESSHEAPARRAY
#include "MinimalArray.hpp"


enum {
	 UC_VIEWALL  = 0xDEA0,
	 UC_VIEWL30  = 0xDEA1,
	 UC_VIEWL50  = 0xDEA2,
	 UC_VIEWL100 = 0xDEA3,
	 UC_VIEWTOD  = 0xDEA4,
	 UC_VIEWYES  = 0xDEA5,
	 UC_VIEWCST  = 0xDEA6,
	 UC_VIEWMAX  = 0xDEA7,

	 UC_DIRP1	= 0xDCE0,
	 UC_DIRP2	= 0xDCE1,

	 UC_OPENHIT = 0xDEE3,

	 UC_UNDOSCR = 0xBEEF,
	 UC_REDOSCR = 0xBEAF,

	 UC_RNKPROF = 0xFEED,
	 UC_TRKRECD = 0xFEEE
};

enum STD_VIEWMODE {
	STD_VIEW_ALL,
	STD_VIEW_L30,
	STD_VIEW_L50,
	STD_VIEW_L100,
	STD_VIEW_TODAY,
	STD_VIEW_YESTERDAY,
	STD_VIEW_CUSTOM,
};


static Minimal::ProcessHeapStringT<char> s_labelSpecific[SWRSCHAR_MAX + 1];
static Minimal::ProcessHeapStringT<char> s_labelOverall[SWRSCHAR_MAX + 1];

static HMENU s_hSysMenu;
static BOOL  s_trDirLeft;	/* ScoreLineモジュールに組み込んだ方が良い？ */
static int   s_tabIndex;
static int   s_viewMode;
static SCORELINE_FILTER_DESC s_filterDesc;

// タブドラッグフック用
static WNDPROC s_origTabProc;
static HCURSOR s_origCursor;
static int  s_dragTabIndex;
static BOOL s_downBeforeMove;

static bool ScoreLineHKSDialog_Reflesh(HWND hDlg);

LRESULT TabCtrl_OnLButtonDown(HWND hWnd, BOOL F, int x, int y, WPARAM wParam)
{
	TC_HITTESTINFO tchi = {{x, y}, TCHT_ONITEM};
	int i = TabCtrl_HitTest(hWnd, &tchi);
	if(i != -1) {
		// 押したら素直に選択されろ
		TabCtrl_SetCurSel(hWnd, i);
		NMHDR nmhdr = { hWnd, IDC_TAB, TCN_SELCHANGE };
		::SendMessage(::GetParent(hWnd), WM_NOTIFY, IDC_TAB, (LPARAM)&nmhdr);
		return TRUE;
	} else {
		return ::CallWindowProc(s_origTabProc, hWnd, WM_LBUTTONDOWN, wParam, MAKELONG(x, y));
	}
}

LRESULT TabCtrl_OnMouseMove(HWND hWnd, int x, int y, UINT keyFlags)
{
	if(keyFlags == MK_LBUTTON) {	// ドラッグ確認
		if(!s_origCursor) { // ファーストインパクト
			TC_HITTESTINFO tchi = {{x, y}, TCHT_ONITEM};
			int i = TabCtrl_HitTest(hWnd, &tchi);
			if(i != -1) {
				s_origCursor = SetCursor(LoadCursor(NULL, IDC_HAND));
				s_dragTabIndex = i;
				TabCtrl_SetCurSel(hWnd, i);
				NMHDR nmhdr = { hWnd, IDC_TAB, TCN_SELCHANGE };
				::SendMessage(::GetParent(hWnd), WM_NOTIFY, IDC_TAB, (LPARAM)&nmhdr);

				::SetCapture(hWnd);
			}
		} else {			// セカンドインパクト
			TC_HITTESTINFO tchi = {{x, y}, TCHT_ONITEM};
			int i = TabCtrl_HitTest(hWnd, &tchi);
			if(i != -1) {	// 範囲内ドラッグ
				::SetCursor(::LoadCursor(NULL, IDC_HAND));
				if(i != s_dragTabIndex) {
					TC_ITEM item;
					char itemText[64];
					item.mask = TCIF_TEXT | TCIF_PARAM;
					item.pszText = itemText;
					item.cchTextMax = sizeof itemText;
					TabCtrl_GetItem(hWnd, s_dragTabIndex, &item);
					TabCtrl_DeleteItem(hWnd, s_dragTabIndex);
					TabCtrl_InsertItem(hWnd, i, &item);
					s_dragTabIndex = i;
				}
			} else {		// 範囲外ドラッグ
				// は、ダメダメ！ぜ～ったいダメ！
				::SetCursor(::LoadCursor(NULL, IDC_NO));
			}
		}
	}
	return TRUE;
}

LRESULT TabCtrl_OnLButtonUp(HWND hWnd, int x, int y, WPARAM wParam)
{
	if(s_origCursor) {
		/* カーソルを解放し、アイコンを元に戻す */
		::ReleaseCapture();
		::SetCursor(s_origCursor);
		s_origCursor = NULL;
	}

	return ::CallWindowProc(s_origTabProc, hWnd, WM_LBUTTONUP, wParam, MAKELONG(x, y));
}


static LRESULT CALLBACK TabCtrl_WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg) {
	HANDLE_MSG(hWnd, WM_LBUTTONDOWN, TabCtrl_OnLButtonDown);
	HANDLE_MSG(hWnd, WM_MOUSEMOVE, TabCtrl_OnMouseMove);
	HANDLE_MSG(hWnd, WM_LBUTTONUP, TabCtrl_OnLButtonUp);
	}
	return ::CallWindowProc(s_origTabProc, hWnd, Msg, wParam, lParam);
}

static void StaticName_OnLButtonDoubleClick(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	int nID = GetDlgCtrlID(hwnd);
	int index = (nID - IDC_NAME01);

	SCORELINE_FILTER_DESC filterDesc = s_filterDesc;
	if (s_tabIndex == 0) {
		if (index != SWRSCHAR_MAX) {
			if (s_trDirLeft) {
				filterDesc.mask |= SCORELINE_FILTER__P1ID;
				filterDesc.p1id = index;
			} else {
				filterDesc.mask |= SCORELINE_FILTER__P2ID;
				filterDesc.p2id = index;
			}
		}
	} else {
		if (index != SWRSCHAR_MAX) {
			filterDesc.mask |= SCORELINE_FILTER__P1ID | SCORELINE_FILTER__P2ID;
			if (s_trDirLeft) {
				filterDesc.p1id = s_tabIndex - 1;
				filterDesc.p2id = index;
			} else {
				filterDesc.p1id = index;
				filterDesc.p2id = s_tabIndex  - 1;
			}
		} else {
			if (s_trDirLeft) {
				filterDesc.mask |= SCORELINE_FILTER__P1ID;
				filterDesc.p1id = s_tabIndex - 1;
			} else {
				filterDesc.mask |= SCORELINE_FILTER__P2ID;
				filterDesc.p2id = s_tabIndex - 1;
			}
		}
	}
	// 苦し紛れ・・・
	char winStr[30], lostStr[30];
	HWND hDlg = ::GetParent(hwnd);
	::GetDlgItemText(hDlg, IDC_WIN01 + index * 2, winStr, sizeof winStr);
	::GetDlgItemText(hDlg, IDC_LOST01 + index * 2, lostStr, sizeof winStr);
	filterDesc.mask |= SCORELINE_FILTER__LIMIT;
	filterDesc.limit = ::StrToInt(winStr) + ::StrToInt(lostStr);

	TrackRecordDialog_ShowModeless(::GetParent(hwnd), &filterDesc);
}

static void StaticName_OnRButtonDoubleClick(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	int nID = GetDlgCtrlID(hwnd);
	int index = (nID - IDC_NAME01);

	SCORELINE_FILTER_DESC filterDesc = s_filterDesc;
	if (s_tabIndex == 0) {
		if (index != SWRSCHAR_MAX) {
			if (s_trDirLeft) {
				filterDesc.mask |= SCORELINE_FILTER__P1ID;
				filterDesc.p1id = index;
			} else {
				filterDesc.mask |= SCORELINE_FILTER__P2ID;
				filterDesc.p2id = index;
			}
		}
	} else {
		if (index != SWRSCHAR_MAX) {
			filterDesc.mask |= SCORELINE_FILTER__P1ID | SCORELINE_FILTER__P2ID;
			if (s_trDirLeft) {
				filterDesc.p1id = s_tabIndex - 1;
				filterDesc.p2id = index;
			} else {
				filterDesc.p1id = index;
				filterDesc.p2id = s_tabIndex  - 1;
			}
		} else {
			if (s_trDirLeft) {
				filterDesc.mask |= SCORELINE_FILTER__P1ID;
				filterDesc.p1id = s_tabIndex - 1;
			} else {
				filterDesc.mask |= SCORELINE_FILTER__P2ID;
				filterDesc.p2id = s_tabIndex - 1;
			}
		}
	}
	RankProfDialog_ShowModeless(::GetParent(hwnd), &filterDesc);
}

static LRESULT CALLBACK StaticName_WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg) {
	HANDLE_MSG(hWnd, WM_RBUTTONDBLCLK, StaticName_OnRButtonDoubleClick);
	HANDLE_MSG(hWnd, WM_LBUTTONDBLCLK, StaticName_OnLButtonDoubleClick);
	case WM_RBUTTONDOWN: return TRUE;
	case WM_RBUTTONUP:   return TRUE;
	}
	WNDPROC origProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hWnd, GWL_USERDATA));
	return ::CallWindowProc(origProc, hWnd, Msg, wParam, lParam);
}

static void StaticWinLost_OnLButtonDoubleClick(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	if(s_tabIndex != 0) {
		int nID = GetDlgCtrlID(hwnd);
		int index = (nID - IDC_WIN01);

		if (index >= 0 && index < SWRSCHAR_MAX) {
			SCORELINE_ITEM item;

			SYSTEMTIME now;
			::GetLocalTime(&now);
			::SystemTimeToFileTime(&now, (LPFILETIME)&item.timestamp);

			if(s_trDirLeft) {
				item.p1name[0] = 0;
				item.p1id  = s_tabIndex - 1;
				item.p1win = (index & 1) ? 0: 2;
				item.p2name[0] = 0;
				item.p2id  = index >> 1;
				item.p2win = (index & 1) ? 2 : 0;
			} else {
				item.p1name[0] = 0;
				item.p1id  = index >> 1;
				item.p1win = (index & 1) ? 2 : 0;
				item.p2name[0] = 0;
				item.p2id  = s_tabIndex - 1;
				item.p2win = (index & 1) ? 0: 2;
			}

			ScoreLine_Enter();
			bool failed = !ScoreLine_Append(&item);
			ScoreLine_Leave(failed);
			if (!failed) {
				Memento_Append(MEMENTO_CMD_APPEND, &item);
				ScoreLineHKSDialog_Reflesh(::GetParent(hwnd));
			}
		}
	}
}

static void StaticWinLost_OnRButtonDoubleClick(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	if(s_tabIndex != 0) {
		int nID = GetDlgCtrlID(hwnd);
		int index = (nID - IDC_WIN01);

		if (index >= 0 && index < SWRSCHAR_MAX) {
			SCORELINE_FILTER_DESC filterDesc = s_filterDesc;
			filterDesc.mask |= SCORELINE_FILTER__P1ID | SCORELINE_FILTER__P2ID;
			if (s_trDirLeft) {
				filterDesc.p1id = s_tabIndex - 1;
				filterDesc.p2id = index >> 1;
				if (index & 1) {
					filterDesc.mask |= SCORELINE_FILTER__P2WIN;
					filterDesc.p2win = 2;
				} else {
					filterDesc.mask |= SCORELINE_FILTER__P1WIN;
					filterDesc.p1win = 2;
				}
			} else {
				filterDesc.mask |= SCORELINE_FILTER__P1ID | SCORELINE_FILTER__P2ID;
				filterDesc.p1id = index >> 1;
				filterDesc.p2id = s_tabIndex - 1;
				if (index & 1) {
					filterDesc.mask |= SCORELINE_FILTER__P1WIN;
					filterDesc.p1win = 2;
				} else {
					filterDesc.mask |= SCORELINE_FILTER__P2WIN;
					filterDesc.p2win = 2;
				}
			}
			SCORELINE_ITEM item;
			if (ScoreLine_QueryTrackRecordTop(filterDesc, item)) {
				ScoreLine_Enter();
				bool failed = !ScoreLine_Remove(item.timestamp);
				ScoreLine_Leave(failed);
				if (!failed) {
					Memento_Append(MEMENTO_CMD_REMOVE, &item);
					ScoreLineHKSDialog_Reflesh(::GetParent(hwnd));
					::PostMessage(GetParent(hwnd), UM_UPDATESCORELINE, 0, 0);
				}
			}
		}
	}
}

static LRESULT CALLBACK StaticWinLost_WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg) {
	HANDLE_MSG(hWnd, WM_RBUTTONDBLCLK, StaticWinLost_OnRButtonDoubleClick);
	HANDLE_MSG(hWnd, WM_LBUTTONDBLCLK, StaticWinLost_OnLButtonDoubleClick);
	case WM_RBUTTONDOWN: return TRUE;
	case WM_RBUTTONUP:   return TRUE;
	}
	WNDPROC origProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hWnd, GWL_USERDATA));
	return ::CallWindowProc(origProc, hWnd, Msg, wParam, lParam);
}

static void ScoreLine_AppendViewFilter(SCORELINE_FILTER_DESC &ret)
{
	SYSTEMTIME loctime, begin, end;
	SCORELINE_FILTER_DESC flt;
	switch(s_viewMode) {
	case STD_VIEW_TODAY:
		flt.mask |= SCORELINE_FILTER__TIMESTAMP_BEGIN | SCORELINE_FILTER__TIMESTAMP_END;
		// 始点
		GetLocalTime(&loctime);
		begin = loctime;
		begin.wHour =
		begin.wMinute = 
		begin.wSecond =
		begin.wMilliseconds = 0;
		::SystemTimeToFileTime(&begin, reinterpret_cast<LPFILETIME>(&flt.t_begin));
		// 終点
		end = loctime;
		end.wHour = 23;
		end.wMinute = 59;
		end.wSecond = 59;
		end.wMilliseconds = 999;
		::SystemTimeToFileTime(&end, reinterpret_cast<LPFILETIME>(&flt.t_end));
		break;
	case STD_VIEW_YESTERDAY:
		flt.mask |= SCORELINE_FILTER__TIMESTAMP_BEGIN | SCORELINE_FILTER__TIMESTAMP_END;
		// 始点
		GetLocalTime(&loctime);
		begin = loctime;
		begin.wHour =
		begin.wMinute = 
		begin.wSecond =
		begin.wMilliseconds = 0;
		::SystemTimeToFileTime(&begin, reinterpret_cast<LPFILETIME>(&flt.t_begin));
		/* 24時間 x 60分 x 60秒 x 1000ミリ秒 x 1000マイクロ秒 x 10ナノ秒 */
		flt.t_begin -= 864000000000ULL; 
		// 終点
		end = loctime;
		end.wHour = 23;
		end.wMinute = 59;
		end.wSecond = 59;
		end.wMilliseconds = 999;
		::SystemTimeToFileTime(&end, reinterpret_cast<LPFILETIME>(&flt.t_end));
		flt.t_end -= 864000000000ULL;
		break;
	case STD_VIEW_CUSTOM:
		ret <<= s_filterDesc;
		break;
	}
}

static void ScoreLineHKSDialog_RefleshUnit(HWND hDlg, int index, int sumWin, int sumLose, BOOL edittable)
{
	int winRate;

	::SetDlgItemInt(hDlg, IDC_WIN01 + index * 2, sumWin, FALSE);
	::SetDlgItemInt(hDlg, IDC_LOST01 + index * 2, sumLose, FALSE);
	if(sumWin + sumLose == 0) {
		::SetDlgItemText(hDlg, IDC_RATE01 + index, _T("--.--%"));
	} else {
		winRate = ::MulDiv(sumWin, 10000, sumWin + sumLose);
		::SetDlgItemText(hDlg, IDC_RATE01 + index, 
			Formatter(_T("%d.%02d%%"), winRate / 100, winRate % 100));
	}
}

static void ScoreLineHKSDialog_RefleshUnits(HWND hDlg)
{
	int i, j;
	int sumWin, sumLose;
	int sumWinAll, sumLoseAll;

	if(s_tabIndex == 0) {	// 全体
		// 各ユニット名称を更新
		for(i = 0; i < SWRSCHAR_MAX + 1; ++i)
			::SetDlgItemText(hDlg, IDC_NAME01 + i, (s_trDirLeft ? s_labelOverall : s_labelSpecific)[i]);
		// 各情報ユニットを更新
		sumWinAll = sumLoseAll = 0;
		for(i = 0; i < SWRSCHAR_MAX; ++i) {
			sumWin = sumLose = 0;
			for(j = 0; j < SWRSCHAR_MAX; ++j) {
				sumWin  += s_trDirLeft ? ScoreLine_Read(i, j, 0) : ScoreLine_Read(j, i, 0);
				sumLose += s_trDirLeft ? ScoreLine_Read(i, j, 1) : ScoreLine_Read(j, i, 1);
			}
			sumWinAll  += sumWin;
			sumLoseAll += sumLose;

			ScoreLineHKSDialog_RefleshUnit(hDlg, i, sumWin, sumLose, FALSE);
		}
	} else { // 個別
		// 各ユニット名称を更新
		for(i = 0; i < SWRSCHAR_MAX + 1; ++i)
			::SetDlgItemText(hDlg, IDC_NAME01 + i, (s_trDirLeft ? s_labelSpecific : s_labelOverall)[i]);
		// 各情報ユニットを更新
		sumWinAll = sumLoseAll = 0;
		for(i = 0; i < SWRSCHAR_MAX; ++i) {
			sumWin  = s_trDirLeft ? ScoreLine_Read(s_tabIndex - 1, i, 0) : ScoreLine_Read(i, s_tabIndex - 1, 0);
			sumLose = s_trDirLeft ? ScoreLine_Read(s_tabIndex - 1, i, 1) : ScoreLine_Read(i, s_tabIndex - 1, 1);
			sumWinAll  += sumWin;
			sumLoseAll += sumLose;

			ScoreLineHKSDialog_RefleshUnit(hDlg, i, sumWin, sumLose, FALSE);
		}
	}
	// 合計ユニットを更新
	ScoreLineHKSDialog_RefleshUnit(hDlg, SWRSCHAR_MAX, sumWinAll, sumLoseAll, FALSE);
}

static bool ScoreLineHKSDialog_Reflesh(HWND hDlg)
{
	ScoreLine_Enter();

	bool failed = !ScoreLine_QueryTrackRecord(s_filterDesc);

	ScoreLine_Leave(failed);
	if (failed) return false;

	Minimal::ProcessHeapStringT<char> baseName;
	baseName = ScoreLine_GetPath();
	::PathStripPath(baseName);
	::PathRemoveExtension(baseName);
	baseName.Repair();
	::SetWindowText(hDlg,
		Formatter(WINDOW_TEXT _T(" - %s"),
			static_cast<LPCSTR>(baseName)));

	ScoreLineHKSDialog_RefleshUnits(hDlg);
	return true;
}


static BOOL CALLBACK ScoreLineHKSDialog_CloseSubDialogProc(HWND hwnd, LPARAM lParam)
{
	if (
	::GetParent(hwnd) == reinterpret_cast<HWND>(lParam) &&
	(::GetWindowLong(hwnd, GWL_STYLE) & WS_POPUPWINDOW)) {
		DestroyWindow(hwnd);
	}
	return TRUE;
}

static void ScoreLineHKSDialog_CloseSubDialog(HWND hDlg)
{
	::EnumThreadWindows(::GetCurrentThreadId(), ScoreLineHKSDialog_CloseSubDialogProc, reinterpret_cast<LPARAM>(hDlg));
}

static void ScoreLineHKSDialog_OpenProfile(HWND hDlg, LPCSTR fileName)
{
	Minimal::ProcessHeapStringA oldPath(ScoreLine_GetPath());
	ScoreLine_SetPath(fileName);
	if(ScoreLine_Open(true)) {
		Memento_Reset();
		ScoreLineHKSDialog_CloseSubDialog(hDlg);
		ScoreLineHKSDialog_Reflesh(hDlg);
	} else {
		ScoreLine_SetPath(oldPath);
		::MessageBox(hDlg, _T("プロファイルのマッピングに失敗しました。"), NULL, MB_OK | MB_ICONSTOP);
	}
}

static void SysMenu_OnClose(HWND hDlg, int x, int y)
{
	::ShowWindow(hDlg, SW_HIDE);
}

static void SysMenu_OnView(HWND hDlg, int nID, int x, int y)
{
	int newMode = nID - UC_VIEWALL;
	if(newMode != s_viewMode) {
		::CheckMenuItem(s_hSysMenu, nID, MF_CHECKED);
		for(int i = UC_VIEWALL; i < UC_VIEWMAX; ++i)
			if(i != nID) ::CheckMenuItem(s_hSysMenu, i, MF_UNCHECKED);
		s_viewMode = newMode;

		SYSTEMTIME loctime, begin, end;
		switch(s_viewMode) {
		case STD_VIEW_ALL:
			s_filterDesc.mask = 0;
			break;
		case STD_VIEW_L30:
			s_filterDesc.mask = SCORELINE_FILTER__LIMIT;
			s_filterDesc.limit = 30;
			break;
		case STD_VIEW_L50:
			s_filterDesc.mask = SCORELINE_FILTER__LIMIT;
			s_filterDesc.limit = 50;
			break;
		case STD_VIEW_L100:
			s_filterDesc.mask = SCORELINE_FILTER__LIMIT;
			s_filterDesc.limit = 100;
			break;
		case STD_VIEW_TODAY:
			s_filterDesc.mask = SCORELINE_FILTER__TIMESTAMP_BEGIN | SCORELINE_FILTER__TIMESTAMP_END;
			// 始点
			GetLocalTime(&loctime);
			begin = loctime;
			begin.wHour =
			begin.wMinute = 
			begin.wSecond =
			begin.wMilliseconds = 0;
			::SystemTimeToFileTime(&begin, reinterpret_cast<LPFILETIME>(&s_filterDesc.t_begin));
			// 終点
			end = loctime;
			end.wHour = 23;
			end.wMinute = 59;
			end.wSecond = 59;
			end.wMilliseconds = 999;
			::SystemTimeToFileTime(&end, reinterpret_cast<LPFILETIME>(&s_filterDesc.t_end));
			break;
		case STD_VIEW_YESTERDAY:
			s_filterDesc.mask = SCORELINE_FILTER__TIMESTAMP_BEGIN | SCORELINE_FILTER__TIMESTAMP_END;
			// 始点
			GetLocalTime(&loctime);
			begin = loctime;
			begin.wHour =
			begin.wMinute = 
			begin.wSecond =
			begin.wMilliseconds = 0;
			::SystemTimeToFileTime(&begin, reinterpret_cast<LPFILETIME>(&s_filterDesc.t_begin));
			/* 24時間 x 60分 x 60秒 x 1000ミリ秒 x 1000マイクロ秒 x 10ナノ秒 */
			s_filterDesc.t_begin -= 864000000000ULL; 
			// 終点
			end = loctime;
			end.wHour = 23;
			end.wMinute = 59;
			end.wSecond = 59;
			end.wMilliseconds = 999;
			::SystemTimeToFileTime(&end, reinterpret_cast<LPFILETIME>(&s_filterDesc.t_end));
			s_filterDesc.t_end -= 864000000000ULL;
			break;
		}

		ScoreLineHKSDialog_Reflesh(hDlg);
	}
}

static void SysMenu_OnViewCustom(HWND hDlg, int nID, int x, int y)
{

	if(ScoreLineQIBFilterDialog_ShowModal(hDlg, (LPVOID)&s_filterDesc) == IDOK) {
		::CheckMenuItem(s_hSysMenu, nID, MF_CHECKED);
		for(int i = UC_VIEWALL; i < UC_VIEWMAX; ++i)
			if(i != nID) ::CheckMenuItem(s_hSysMenu, i, MF_UNCHECKED);
		s_viewMode = STD_VIEW_CUSTOM;
		ScoreLineHKSDialog_Reflesh(hDlg);
	}
}


static void SysMenu_OnOpenProfile(HWND hDlg, int x, int y)
{
	char fileName[1025];
	OPENFILENAME ofn;
	::ZeroMemory(&ofn, sizeof ofn);
	ofn.lStructSize = sizeof ofn;
	ofn.hwndOwner = hDlg;
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = sizeof fileName;
	ofn.lpstrDefExt = _T("db");
	ofn.lpstrFilter = _T("Trackrecord Database (*.db)\0*.db\0");
	ofn.Flags = OFN_CREATEPROMPT | OFN_NOCHANGEDIR;
	fileName[0] = 0;
	if(::GetOpenFileName(&ofn)) {
		ScoreLineHKSDialog_OpenProfile(hDlg, fileName);
	}
}

static void SysMenu_OnDirectionP1(HWND hDlg, int x, int y)
{
	if(!s_trDirLeft) {
		::CheckMenuItem(s_hSysMenu, UC_DIRP1, MF_CHECKED);
		::CheckMenuItem(s_hSysMenu, UC_DIRP2, MF_UNCHECKED);
		s_trDirLeft = TRUE;
		ScoreLineHKSDialog_Reflesh(hDlg);
	}
}

static void SysMenu_OnDirectionP2(HWND hDlg, int x, int y)
{
	if(s_trDirLeft) {
		::CheckMenuItem(s_hSysMenu, UC_DIRP1, MF_UNCHECKED);
		::CheckMenuItem(s_hSysMenu, UC_DIRP2, MF_CHECKED);
		s_trDirLeft = FALSE;
		ScoreLineHKSDialog_Reflesh(hDlg);
	}
}

static void SysMenu_OnUndo(HWND hDlg, int x, int y)
{
	Memento_Undo();
	ScoreLineHKSDialog_Reflesh(hDlg);
}

static void SysMenu_OnRedo(HWND hDlg, int x, int y)
{
	Memento_Redo();
	ScoreLineHKSDialog_Reflesh(hDlg);
}

static void SysMenu_OnRankProfile(HWND hDlg, int x, int y)
{
	RankProfDialog_ShowModeless(hDlg, NULL);
}

static void SysMenu_OnTrackRecord(HWND hDlg, int x, int y)
{

	TrackRecordDialog_ShowModeless(hDlg, NULL);
}

static void ScoreLineHKSDialog_InitSysMenu(HWND hDlg)
{
	int itemIndex = 0;
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_OPENHIT,     _T("プロファイルを読み込む"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_SEPARATOR | MF_BYPOSITION, 0, NULL);
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_UNDOSCR,     _T("元に戻す"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_REDOSCR,     _T("やり直し"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_SEPARATOR | MF_BYPOSITION, 0, NULL);
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_DIRP1,       _T("自分別戦績"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_DIRP2,       _T("相手別戦績"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_SEPARATOR | MF_BYPOSITION, 0, NULL);
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION | MF_CHECKED,	UC_VIEWALL,     _T("全ての対戦"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_VIEWL30,     _T("過去３０回の対戦"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_VIEWL50,     _T("過去５０回の対戦"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_VIEWL100,    _T("過去１００回の対戦"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_VIEWTOD,     _T("今日の対戦"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_VIEWYES,     _T("昨日の対戦"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_VIEWCST,		_T("カスタム対戦"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_SEPARATOR | MF_BYPOSITION, 0, NULL);
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_RNKPROF,     _T("相手プロファイル表"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_STRING | MF_BYPOSITION,                UC_TRKRECD,     _T("過去対戦履歴表"));
	::InsertMenu(s_hSysMenu, itemIndex++, MF_SEPARATOR | MF_BYPOSITION, 0, NULL);
}

static void ScoreLineHKSDialog_InitTabCtrl(HWND hDlg)
{
	HWND hTabCtrl = ::GetDlgItem(hDlg, IDC_TAB);
	TCITEM tcItem;
	::ZeroMemory(&tcItem, sizeof tcItem);
	for(int i = 0; i < SWRSCHAR_MAX + 1; ++i)
		TabCtrl_InsertItem(hTabCtrl, 0, &tcItem);

	tcItem.mask = TCIF_TEXT | TCIF_PARAM;
	for(int i = 0; i < SWRSCHAR_MAX + 1; ++i) {
		int tabId = g_settings.ReadInteger(_T("DefaultDlg.Tab.Arrangement"), Formatter(_T("%d"), i), i);
		if (i < 0) i = 0;
		if (i > SWRSCHAR_MAX) i = SWRSCHAR_MAX;
		if(tabId == 0) {	// システム
			tcItem.pszText = _T("全体");
			tcItem.lParam = 0;
		} else {	// キャラクタ
			tcItem.pszText = const_cast<LPSTR>(g_characters[tabId - 1].abbr);
			tcItem.lParam = tabId;
		}
		TabCtrl_SetItem(hTabCtrl, i, &tcItem);
	}
	TabCtrl_SetCurSel(hTabCtrl, 0);

	RECT rct, rct1st, rct2nd;
	::GetClientRect(hTabCtrl, &rct);
	TabCtrl_AdjustRect(hTabCtrl, 0, &rct);
	TabCtrl_GetItemRect(hTabCtrl, 0, &rct1st);
	TabCtrl_GetItemRect(hTabCtrl, 1, &rct2nd);
	TabCtrl_SetItemSize(hTabCtrl, 
		(rct.right - rct.left) / 11 - (rct2nd.left - rct1st.right), rct1st.bottom - rct1st.top);
	s_origTabProc = (WNDPROC)GetWindowLong(hTabCtrl, GWL_WNDPROC);
	::SetWindowLong(hTabCtrl, GWL_WNDPROC, (LONG)TabCtrl_WindowProc);
}

static void ScoreLineHKSDialog_InitStaticCtrls(HWND hDlg)
{
	for (int i = IDC_WIN01; i <= IDC_LOSTSS; ++i) {
		HWND hStaticCtrl = GetDlgItem(hDlg, i);
		WNDPROC origProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hStaticCtrl, GWL_WNDPROC));

		::SetWindowLongPtr(hStaticCtrl, GWL_USERDATA, reinterpret_cast<LONG_PTR>(origProc));
		::SetWindowLongPtr(hStaticCtrl, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(StaticWinLost_WindowProc));

	}

	for (int i = IDC_NAME01; i <= IDC_NAMESS; ++i) {
		HWND hStaticCtrl = GetDlgItem(hDlg, i);
		WNDPROC origProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hStaticCtrl, GWL_WNDPROC));

		::SetWindowLongPtr(hStaticCtrl, GWL_USERDATA, reinterpret_cast<LONG_PTR>(origProc));
		::SetWindowLongPtr(hStaticCtrl, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(StaticName_WindowProc));
	}

}

static BOOL ScoreLineHKSDialog_OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
	for(int i = 0; i < SWRSCHAR_MAX; ++i) {
		s_labelSpecific[i] = _T("vs ");
		s_labelSpecific[i] += g_characters[i].abbr;
		s_labelOverall[i] = _T("with ");
		s_labelOverall[i] += g_characters[i].abbr;
	}
	s_labelSpecific[SWRSCHAR_MAX] = _T("小計");
	s_labelOverall[SWRSCHAR_MAX]  = _T("合計");

	s_hSysMenu = ::GetSystemMenu(hDlg, FALSE);
	if(s_hSysMenu == NULL) return FALSE;

	ScoreLineHKSDialog_InitSysMenu(hDlg);
	ScoreLineHKSDialog_InitTabCtrl(hDlg);
	ScoreLineHKSDialog_InitStaticCtrls(hDlg);

	HWND hTabCtrl = ::GetDlgItem(hDlg, IDC_TAB);
	TCITEM tcItem;
	tcItem.mask = TCIF_PARAM;
	TabCtrl_GetItem(hTabCtrl, 
		TabCtrl_GetCurSel(hTabCtrl), &tcItem);

	s_trDirLeft = g_settings.ReadInteger(
		_T("DefaultDlg.Trackrecord"), _T("DirLeft"), 1) != 0;
	::CheckMenuItem(s_hSysMenu, UC_DIRP1,  s_trDirLeft ? MF_CHECKED: MF_UNCHECKED);
	::CheckMenuItem(s_hSysMenu, UC_DIRP2, !s_trDirLeft ? MF_CHECKED: MF_UNCHECKED);

	int x, y;
	x = g_settings.ReadInteger(_T("DefaultDlg.Display"), _T("X"), CW_USEDEFAULT);
	y = g_settings.ReadInteger(_T("DefaultDlg.Display"), _T("Y"), CW_USEDEFAULT);
	if(x != CW_USEDEFAULT && y != CW_USEDEFAULT)
		::SetWindowPos(hDlg, 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	s_viewMode = STD_VIEW_ALL;
	s_tabIndex = tcItem.lParam;
	return ScoreLineHKSDialog_Reflesh(hDlg);
}

static void ScoreLineHKSDialog_OnSysCommand(HWND hDlg, UINT nID, int x, int y)
{
	if(nID == SC_CLOSE) {
		SysMenu_OnClose(hDlg, x, y);
	} else if(nID >= UC_VIEWALL && nID <= UC_VIEWYES) {
		SysMenu_OnView(hDlg, nID, x, y);
	} else if(nID == UC_VIEWCST) {
		SysMenu_OnViewCustom(hDlg, nID, x, y);
	} else if(nID == UC_OPENHIT) {
		SysMenu_OnOpenProfile(hDlg, x, y);
	} else if(nID == UC_DIRP1) {
		SysMenu_OnDirectionP1(hDlg, x, y);
	} else if(nID == UC_DIRP2) {
		SysMenu_OnDirectionP2(hDlg, x, y);
	} else if(nID == UC_UNDOSCR) {
		SysMenu_OnUndo(hDlg, x, y);
	} else if(nID == UC_REDOSCR) {
		SysMenu_OnRedo(hDlg, x, y);
	} else if(nID == UC_RNKPROF) {
		SysMenu_OnRankProfile(hDlg, x, y);
	} else if(nID == UC_TRKRECD) {
		SysMenu_OnTrackRecord(hDlg, x, y);
	}
}

static void ScoreLineHKSDialog_OnShortcut(HWND hDlg, int id)
{
	SHORTCUT sc;
	Shortcut_GetElement(sc, id - ID_SHORTCUT_BASE);
	ScoreLineHKSDialog_OpenProfile(hDlg, sc.path);
}

static void ScoreLineHKSDialog_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
	if(id >= ID_SHORTCUT_BASE && id < ID_SHORTCUT_BASE + MAX_SHORTCUT) {
		ScoreLineHKSDialog_OnShortcut(hDlg, id);
	}
}

static void ScoreLineHKSDialog_OnUpdateScoreLine(HWND hDlg)
{
	ScoreLineHKSDialog_Reflesh(hDlg);
}

static LRESULT ScoreLineHKSDialog_OnNotify(HWND hDlg, int idCtrl, LPNMHDR pNMHdr)
{
	TCITEM tcItem;
	if(pNMHdr->idFrom == IDC_TAB) {
		switch(pNMHdr->code) {
		case TCN_SELCHANGE:
			tcItem.mask = TCIF_PARAM;
			TabCtrl_GetItem(pNMHdr->hwndFrom, 
				TabCtrl_GetCurSel(pNMHdr->hwndFrom), &tcItem);
			s_tabIndex = tcItem.lParam;
			ScoreLineHKSDialog_Reflesh(hDlg);
			return TRUE;
		}
	}
	return FALSE;
}

static void ScoreLineHKSDialog_OnDestroy(HWND hDlg)
{
	HWND hTabCtrl = ::GetDlgItem(hDlg, IDC_TAB);
	if(s_origTabProc) {
		::SetWindowLongPtr(hTabCtrl, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(s_origTabProc));
	}

	for(int i = IDC_WIN01; i <= IDC_LOSTSS; ++i) {
		HWND hStaticCtrl = ::GetDlgItem(hDlg, i);
		WNDPROC origProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hStaticCtrl, GWL_USERDATA));
		if (origProc != NULL) {
			::SetWindowLongPtr(hStaticCtrl, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(origProc));
		}
	}

	for(int i = IDC_NAME01; i <= IDC_NAMESS; ++i) {
		HWND hStaticCtrl = ::GetDlgItem(hDlg, i);
		WNDPROC origProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hStaticCtrl, GWL_USERDATA));
		if (origProc != NULL) {
			::SetWindowLongPtr(hStaticCtrl, GWL_WNDPROC, reinterpret_cast<LONG_PTR>(origProc));
		}
	}

	int count = TabCtrl_GetItemCount(hTabCtrl);
	TCITEM tcItem;
	tcItem.mask = TCIF_PARAM;
	for(int i = 0; i < count; ++i) {
		TabCtrl_GetItem(hTabCtrl, i, &tcItem);
		g_settings.WriteInteger(
			_T("DefaultDlg.Tab.Arrangement"), Formatter(_T("%d"), i),
			tcItem.lParam);
	}
	RECT rct;
	::GetWindowRect(hDlg, &rct);
	g_settings.WriteInteger(_T("DefaultDlg.Display"), _T("X"), rct.left);
	g_settings.WriteInteger(_T("DefaultDlg.Display"), _T("Y"), rct.top);

	/* おまじない */
	s_tabIndex = 0;
}

static void ScoreLineHKSDialog_OnNCRButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT codeHitTest)
{
	if(codeHitTest == HTCAPTION) {
		::EnableMenuItem(s_hSysMenu, UC_UNDOSCR, Memento_Undoable() ? MF_ENABLED : MF_GRAYED);
		::EnableMenuItem(s_hSysMenu, UC_REDOSCR, Memento_Redoable() ? MF_ENABLED : MF_GRAYED);
	}
}

BOOL CALLBACK ScoreLineHKSDialog_DlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg) {
	HANDLE_DLG_MSG(hDlg, WM_INITDIALOG, ScoreLineHKSDialog_OnInitDialog);
	HANDLE_DLG_MSG(hDlg, WM_DESTROY, ScoreLineHKSDialog_OnDestroy);
	HANDLE_DLG_MSG(hDlg, WM_SYSCOMMAND, ScoreLineHKSDialog_OnSysCommand);
	HANDLE_DLG_MSG(hDlg, WM_COMMAND, ScoreLineHKSDialog_OnCommand);
	HANDLE_DLG_MSG(hDlg, WM_NCRBUTTONDOWN, ScoreLineHKSDialog_OnNCRButtonDown);
	HANDLE_DLG_MSG(hDlg, WM_NOTIFY, ScoreLineHKSDialog_OnNotify);
	case UM_UPDATESCORELINE: ScoreLineHKSDialog_OnUpdateScoreLine(hDlg); break;
	}
	return FALSE;
}
