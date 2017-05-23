#include "pch.hpp"
#include "AlwaysRecordable.hpp"
#include "QIBSettings.hpp"

// ‰B‚µƒIƒvƒVƒ‡ƒ“
bool g_highPrecisionRateEnabled;
bool g_rateColorizationEnabled;
bool g_autoAdjustViewRect;

const int g_winningRatePrecision = 1000;
const int g_winningRateFp = g_winningRatePrecision / 100;

void QIBSettings_Load()
{
	g_highPrecisionRateEnabled = g_settings.ReadInteger(_T("WinningRate"), "HighPrecision", 0) != 0;
	g_rateColorizationEnabled  = g_settings.ReadInteger(_T("WinningRate"), "Colorization",  0) != 0;

	g_autoAdjustViewRect  = g_settings.ReadInteger(_T("QIB.Behavior"), "AutoAjustViewRect",  1) != 0;
}

void QIBSettings_Save()
{
	g_settings.WriteInteger(_T("WinningRate"), "HighPrecision", g_highPrecisionRateEnabled ? 1 : 0); 
	g_settings.WriteInteger(_T("WinningRate"), "Colorization", g_rateColorizationEnabled ? 1 : 0);

	g_settings.WriteInteger(_T("QIB.Behavior"), "AutoAjustViewRect", g_autoAdjustViewRect ? 1 : 0); 
}
