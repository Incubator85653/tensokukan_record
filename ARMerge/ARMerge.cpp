#include "pch.hpp"

#define MINIMAL_USE_PROCESSHEAPSTRING
#include "MinimalString.hpp"
#ifdef _DEBUG
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
	TCHAR srcDatabase[MAX_PATH];
	TCHAR destDatabase[MAX_PATH];
	OPENFILENAME ofn;

	memset(srcDatabase, 0, sizeof(srcDatabase));
	memset(destDatabase, 0, sizeof(destDatabase));
	memset(&ofn, 0, sizeof(ofn));

	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrTitle = _T("結合元のデータベースファイルを指定");
	ofn.lpstrFile = srcDatabase;
	ofn.nMaxFile = _countof(srcDatabase) - 1;
	if (GetOpenFileName(&ofn) == 0) return 1;

	ofn.lpstrTitle = _T("結合先のデータベースファイルを指定");
	ofn.lpstrFile = destDatabase;
	ofn.nMaxFile = _countof(destDatabase) - 1;
	if (GetOpenFileName(&ofn) == 0) return 1;


	sqlite3 *destDb;
	int rc = sqlite3_open(Minimal::MinimalT2UTF8(destDatabase), &destDb);
	if (rc) {
		MessageBox(NULL, _T("結合先のデータベースファイルが開けません"), NULL, MB_OK | MB_ICONSTOP);
		return 1;
	}

	char *attachSql = sqlite3_mprintf("ATTACH '%q' as SRCDB", static_cast<const char *>(Minimal::MinimalT2UTF8(srcDatabase)));
	MessageBoxA(NULL, attachSql, NULL, MB_OK | MB_ICONSTOP);
	char *errmsg;
	rc = sqlite3_exec(destDb, attachSql, NULL, NULL, NULL);
	sqlite3_free(attachSql);
	if (rc != SQLITE_OK) {
		MessageBox(NULL, _T("結合元のデータベースファイルが開けません"), NULL, MB_OK | MB_ICONSTOP);
		sqlite3_close(destDb);
		return 1;
	}

	rc = sqlite3_exec(destDb, "INSERT INTO trackrecord123 SELECT * FROM SRCDB.trackrecord123", NULL, NULL, &errmsg);
	if (rc) {
		MessageBox(NULL, _T("結合出来ませんでした"), NULL, MB_OK | MB_ICONSTOP);
		sqlite3_close(destDb);
		return 1;
	}

	MessageBox(NULL, _T("結合しました。"), NULL, MB_OK);
	sqlite3_close(destDb);
	return 0;
}