#ifndef SWRSADDRDEF_H_INCLUDED
#define SWRSADDRDEF_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define SWRS_ADDR_PROFSIZE   0x20

enum SWRSCBMSG {
	SWRSMSG_STATECHANGE,
	SWRSMSG_PARAMCHANGE,
};

enum SWRSSTATE {
	SWRSSTATE_NOTFOUND,
	SWRSSTATE_WATCH,
};

enum SWRSSCENE {
	SWRSSCENE_LOGO = 0,
	SWRSSCENE_OPENING = 1,
	SWRSSCENE_TITLE = 2,
	SWRSSCENE_SELECT = 3,
	SWRSSCENE_BATTLE = 5,
	SWRSSCENE_LOADING = 6,
	SWRSSCENE_SELECTSV = 8,
	SWRSSCENE_SELECTCL = 9,
	SWRSSCENE_LOADINGSV = 10,
	SWRSSCENE_LOADINGCL = 11,
	SWRSSCENE_LOADINGWATCH = 12,
	SWRSSCENE_BATTLESV = 13,
	SWRSSCENE_BATTLECL = 14,
	SWRSSCENE_BATTLEWATCH = 15,
	SWRSSCENE_SELECTSENARIO = 16,
	SWRSSCENE_ENDING = 20,
	SWRSSCENE_MAX,
};

enum SWRSSCHAR {
	SWRSCHAR_REIMU = 0,
	SWRSCHAR_MARISA = 1,
	SWRSCHAR_SAKUYA = 2,
	SWRSCHAR_ALICE = 3,
	SWRSCHAR_PATCHOULI = 4,
	SWRSCHAR_YOUMU = 5,
	SWRSCHAR_REMILIA = 6,
	SWRSCHAR_YUYUKO = 7,
	SWRSCHAR_YUKARI = 8,
	SWRSCHAR_SUICA = 9,
	SWRSCHAR_REISEN = 10,
	SWRSCHAR_AYA = 11,
	SWRSCHAR_KOMACHI = 12,
	SWRSCHAR_IKU = 13,
	SWRSCHAR_TENSHI = 14,
	SWRSCHAR_SANAE = 15,
	SWRSCHAR_CIRNO = 16,
	SWRSCHAR_MEILING = 17,
	SWRSCHAR_UTSUHO = 18,
	SWRSCHAR_SUWAKO = 19,
	SWRSCHAR_ROMAN = 20,
	SWRSCHAR_MAX = SWRSCHAR_ROMAN,
};

enum SWRSPARAM {
	SWRSPARAM_SCENE = 0,
	SWRSPARAM_COMMMODE,
	SWRSPARAM_BATTLEMODE,
	SWRSPARAM_LCHARID,
	SWRSPARAM_RCHARID,
	SWRSPARAM_LWINCOUNT,
	SWRSPARAM_RWINCOUNT,
	SWRSPARAM_TOADDR_SERVER,
	SWRSPARAM_TOADDR_CLIENT,
	SWRSPARAM_MAX,
	SWRSPARAM_LPROFNAME,
	SWRSPARAM_RPROFNAME,
};

enum SWRSCOMMMODE {
	SWRSCOMMMODE_SERVER = 4,
	SWRSCOMMMODE_CLIENT = 5,
	SWRSCOMMMODE_WATCH  = 6
};

typedef void (__cdecl *SWRSADDRCALLBACKPROC)(int, int, int, void *);

int SWRSAddrInit(HWND, int);
int SWRSAddrFinish();
int SWRSGetParam(int);
SWRSSTATE SWRSAddrGetState();

#ifdef __cplusplus
}
#endif

#endif /* SWRSADDRDEF_H_INCLUDED */
