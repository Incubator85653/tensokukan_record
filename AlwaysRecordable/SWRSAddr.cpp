#include "pch.hpp"
#include "SWRSAddrDef.h"
#include "AlwaysRecordable.hpp"
#include "ProfileIO.hpp"

#define MINIMAL_USE_PROCESSHEAPSTRING
#include "MinimalPath.hpp"

#define POLL_INTERVAL 50


#define DEF_SWRS_WINDOW_CLASS _T("th123_110a")
#define DEF_SWRS_WINDOW_TEXT  _T("ìåï˚îÒëzìVë• Å` í¥úWãâÉMÉjÉáÉãÇÃì‰Çí«Ç¶ Ver1.10a")

#define DEF_SWRS_ADDR_PBATTLEMGR 0x008985E4
#define DEF_SWRS_ADDR_PNETOBJECT 0x008986A0
#define DEF_SWRS_ADDR_COMMMODE   0x00898690
#define DEF_SWRS_ADDR_LCHARID    0x00899D10
#define DEF_SWRS_ADDR_RCHARID    0x00899D30
#define DEF_SWRS_ADDR_SCENEID    0x008A0044

#define DEF_SWRS_ADDR_LCHAROFS   0x0c
#define DEF_SWRS_ADDR_RCHAROFS   0x10
#define DEF_SWRS_ADDR_BTLMODEOFS 0x88

#define DEF_SWRS_ADDR_LPROFOFS   0x04
#define DEF_SWRS_ADDR_RPROFOFS   0x24

#define DEF_SWRS_ADDR_NETUDPOFS  0x3BC

/* For Server (it may be also used in client mode.) */
#define DEF_SWRS_ADDR_ADRBEGOFS  (DEF_SWRS_ADDR_NETUDPOFS + 0x10C)
/* For Client */
#define DEF_SWRS_ADDR_TOADDROFS  (DEF_SWRS_ADDR_NETUDPOFS + 0x3C)

#define DEF_SWRS_ADDR_WINCNTOFS  0x573

static TCHAR SWRS_WINDOW_CLASS[256];
static TCHAR SWRS_WINDOW_TEXT[256];

static DWORD SWRS_ADDR_PBATTLEMGR;
static DWORD SWRS_ADDR_PNETOBJECT;
static DWORD SWRS_ADDR_COMMMODE;
static DWORD SWRS_ADDR_LCHARID;
static DWORD SWRS_ADDR_RCHARID;
static DWORD SWRS_ADDR_SCENEID;

static DWORD SWRS_ADDR_LCHAROFS;
static DWORD SWRS_ADDR_RCHAROFS;
static DWORD SWRS_ADDR_BTLMODEOFS;

static DWORD SWRS_ADDR_LPROFOFS;
static DWORD SWRS_ADDR_RPROFOFS;

static DWORD SWRS_ADDR_NETUDPOFS;

/* For Server (it may be also used in client mode.) */
static DWORD SWRS_ADDR_ADRBEGOFS;
/* For Client */
static DWORD SWRS_ADDR_TOADDROFS;

static DWORD SWRS_ADDR_WINCNTOFS;

static HANDLE s_ThProc;
static HWND   s_ThWnd;

static HWND s_callbackWnd;
static int  s_callbackMsg;

static HANDLE s_userThread;
static HANDLE s_thread;
static DWORD s_threadId;

static int s_paramOld[SWRSPARAM_MAX];
static char s_paramStr[260];

static SWRSSTATE s_SWRSState;

static void SWRSLoadProfile()
{
#define LoadAddress(N) N = profile.ReadInteger(_T("SWRSAddress"), _T(#N), DEF_##N)
#define LoadChar(N) profile.ReadString(_T("SWRSName"), _T(#N), DEF_##N, N, sizeof N)

	Minimal::ProcessHeapPath profPath = g_appPath;
	CProfileIO profile(profPath /= _T("SWRSAddr.ini"));
	LoadChar(SWRS_WINDOW_CLASS);
	LoadChar(SWRS_WINDOW_TEXT);

	if (::StrStr(SWRS_WINDOW_TEXT, _T("ÉJÉIÉX")) || ::StrStr(SWRS_WINDOW_TEXT, _T("∂µΩ")) || ::StrStr(SWRS_WINDOW_TEXT, _T("Ex")) || ::StrStr(SWRS_WINDOW_TEXT, _T("ex"))) {
		// (Å@ÅLﬂÅ[^ÅM)Å‹Åô
		::TerminateProcess(GetCurrentProcess(), 42731);
	}

	LoadAddress(SWRS_ADDR_PBATTLEMGR);
	LoadAddress(SWRS_ADDR_PNETOBJECT);
	LoadAddress(SWRS_ADDR_COMMMODE);
	LoadAddress(SWRS_ADDR_LCHARID);
	LoadAddress(SWRS_ADDR_RCHARID);
	LoadAddress(SWRS_ADDR_SCENEID);

	LoadAddress(SWRS_ADDR_LCHAROFS);
	LoadAddress(SWRS_ADDR_RCHAROFS);
	LoadAddress(SWRS_ADDR_BTLMODEOFS);

	LoadAddress(SWRS_ADDR_LPROFOFS);
	LoadAddress(SWRS_ADDR_RPROFOFS);

	LoadAddress(SWRS_ADDR_NETUDPOFS);

	LoadAddress(SWRS_ADDR_ADRBEGOFS);
	LoadAddress(SWRS_ADDR_TOADDROFS);

	LoadAddress(SWRS_ADDR_WINCNTOFS);
}

static void APIENTRY SWRSAPCCallback(ULONG_PTR)
{
}

static void SWRSCallback(short Msg, short param1, int param2)
{
	if (s_callbackWnd) {
		::PostMessage(s_callbackWnd, s_callbackMsg, MAKELONG(Msg, param1), param2);
	}
}


static SWRSSTATE SWRSStateFindWindow()
{
	s_ThWnd = ::FindWindow(SWRS_WINDOW_CLASS, SWRS_WINDOW_TEXT);
	if (s_ThWnd != NULL) {
		DWORD dwThId;
		::GetWindowThreadProcessId(s_ThWnd, &dwThId);
		s_ThProc = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwThId);
		SWRSCallback(SWRSMSG_STATECHANGE, SWRSSTATE_WATCH, 0);
		::ZeroMemory(s_paramOld, sizeof s_paramOld);
		return SWRSSTATE_WATCH;
	} else {
		return SWRSSTATE_NOTFOUND;
	}
}

static SWRSSTATE SWRSStateWatchSWRS()
{
	DWORD ret = ::WaitForSingleObject(s_ThProc, 0);
	if (ret != WAIT_TIMEOUT) {
		::CloseHandle(s_ThProc);
		s_ThProc = NULL;
		SWRSCallback(SWRSMSG_STATECHANGE, SWRSSTATE_NOTFOUND, 0);
		return SWRSSTATE_NOTFOUND;
	}

	int param;
	for (int i = 0; i < SWRSPARAM_MAX; ++i) {
		if ((param = SWRSGetParam(i)) != -1) {
			if (param != s_paramOld[i])
				SWRSCallback(SWRSMSG_PARAMCHANGE, i, param);
			s_paramOld[i] = param;
		} else s_paramOld[i] = 0;
	}
	return SWRSSTATE_WATCH;
}

static DWORD WINAPI SWRSAddrWorkThread(LPVOID)
{
	MSG msg;

	s_SWRSState = SWRSSTATE_NOTFOUND;
	while(!::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) || msg.message != WM_QUIT) {
		switch(s_SWRSState) {
		case SWRSSTATE_NOTFOUND: s_SWRSState = SWRSStateFindWindow(); break;
		case SWRSSTATE_WATCH:    s_SWRSState = SWRSStateWatchSWRS();   break;
		};
		::SleepEx(POLL_INTERVAL, TRUE);
	}
	::ExitThread(0);
	return 0;
}

int SWRSAddrInit(HWND callbackWnd, int callbackMsg)
{
	SWRSLoadProfile();
	s_callbackWnd = callbackWnd;
	s_callbackMsg = callbackMsg;
	s_thread = ::CreateThread(NULL, 0, SWRSAddrWorkThread, NULL, 0, &s_threadId);
	s_userThread = ::GetCurrentThread();
	return s_thread != NULL;
}

int SWRSAddrFinish()
{
	if (s_thread) {
		::PostThreadMessage(s_threadId, WM_QUIT, 0, 0);
		::QueueUserAPC(SWRSAPCCallback, s_thread, 0);
		::WaitForSingleObject(s_thread, INFINITE);
		::CloseHandle(s_thread);
	}
	return 0;
}

SWRSSTATE SWRSAddrGetState()
{
	return s_SWRSState;
}

int SWRSGetParam(int param)
{
	DWORD dwReadSize;
	DWORD dwordData, dwordData2;
	BYTE byteData;

	switch(param) {
	case SWRSPARAM_SCENE:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_SCENEID, &dwordData, 4, &dwReadSize)) {
			return dwordData;
		}
		break;
	case SWRSPARAM_COMMMODE:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_COMMMODE, &dwordData, 4, &dwReadSize)) {
			return dwordData;
		}
		break;
	case SWRSPARAM_BATTLEMODE:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_PBATTLEMGR, &dwordData, 4, &dwReadSize) &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_BTLMODEOFS), &dwordData, 4, &dwReadSize)) {
			return dwordData;
		}
		break;
	case SWRSPARAM_LCHARID:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_LCHARID, &dwordData, 4, &dwReadSize)) {
			return dwordData;
		}
		break;
	case SWRSPARAM_RCHARID:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_RCHARID, &dwordData, 4, &dwReadSize)) {
			return dwordData;
		}
		break;
	case SWRSPARAM_LWINCOUNT:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_PBATTLEMGR, &dwordData, 4, &dwReadSize) &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_LCHAROFS), &dwordData, 4, &dwReadSize) &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_WINCNTOFS), &byteData, 1, &dwReadSize)) {
			return byteData;
		}
		break;
	case SWRSPARAM_RWINCOUNT:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_PBATTLEMGR, &dwordData, 4, &dwReadSize) &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_RCHAROFS), &dwordData, 4, &dwReadSize) &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_WINCNTOFS), &byteData, 1, &dwReadSize)) {
			return byteData;
		}
		break;
	case SWRSPARAM_LPROFNAME:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_PNETOBJECT, &dwordData, 4, &dwReadSize) &&
			::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_LPROFOFS), s_paramStr, SWRS_ADDR_PROFSIZE, &dwReadSize)) {
			return (int)s_paramStr;
		}
		break;
	case SWRSPARAM_RPROFNAME:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_PNETOBJECT, &dwordData, 4, &dwReadSize) &&
			::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_RPROFOFS), s_paramStr, SWRS_ADDR_PROFSIZE, &dwReadSize)) {
			return (int)s_paramStr;
		}
		break;
	case SWRSPARAM_TOADDR_CLIENT:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_PNETOBJECT, &dwordData, 4, &dwReadSize) &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_TOADDROFS), &dwordData2, 4, &dwReadSize) && dwordData2 != 0 &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_TOADDROFS + 4), &dwordData2, 4, &dwReadSize)) {
			return dwordData2;
		}
	case SWRSPARAM_TOADDR_SERVER:
		if (::ReadProcessMemory(s_ThProc, (LPVOID)SWRS_ADDR_PNETOBJECT, &dwordData, 4, &dwReadSize) &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + SWRS_ADDR_ADRBEGOFS), &dwordData, 4, &dwReadSize) &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + 0x10), &dwordData2, 4, &dwReadSize) && dwordData2 != -1 &&
		   ::ReadProcessMemory(s_ThProc, (LPVOID)(dwordData + 0x04), &dwordData, 4, &dwReadSize)) {
			return dwordData;
		}
		break;
	}
	return -1;
}
