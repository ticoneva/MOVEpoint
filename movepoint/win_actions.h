#pragma once
#include "stdafx.h"

#include <math.h>
#include <windows.h>
#include <WinUser.h>
#include <process.h>

#include <VersionHelpers.h>
#include <Dbt.h>
#include "resource.h"

namespace win_actions {

	//Keyboard and cursor functions
	void keyPress(byte bVk, byte keyState);
	void mousePress(byte button, byte keyState);
	void mouseClick(byte button);
	void keyboardClick(byte bVk);

	BOOL amIAdmin();
	BOOL RunAsAdmin(HWND hWnd, LPTSTR lpFile, LPTSTR lpParameters);

	void showDesktop();

	//only works for Windows 10
	void showTaskView();
	void newDesktop();
	void killDesktop();
	void prevDesktop();
	void nextDesktop();

	//Window operations
	HWND getTarget(POINT cursorPos);
	void focusMyTarget(HWND cTarget = NULL);
	BOOL closeTarget(HWND cTarget = NULL);
	BOOL minimizeTarget(HWND cTarget = NULL);
	BOOL maximizeTarget(HWND cTarget = NULL);
	BOOL restoreTarget(HWND cTarget = NULL);
	BOOL setShowCMD(HWND tHandle, WINDOWPLACEMENT * ptWP, UINT showCMD);
	BOOL MaxRestoreTarget(HWND cTarget = NULL);


	ULARGE_INTEGER fetchFileTime();

	//Registry functions
	LONG writeFloatToReg(HKEY hKey, LPTSTR subkey, float value);
	LONG readFloatFromReg(HKEY hKey, LPTSTR subkey, float * vp);
	DWORD readDWORDFromReg(HKEY hKey, LPTSTR subkey, DWORD * vp);

}