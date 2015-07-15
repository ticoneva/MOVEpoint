#include "stdafx.h"
#include "win_actions.h"

namespace win_actions {

	//Keypress subroutine
	void keyPress(byte bVk, byte keyState)
	{
		if (keyState == 1) {
			keybd_event(bVk, 0, 0, 0);
		}
		else {
			keybd_event(bVk, 0, KEYEVENTF_KEYUP, 0);
		}
	}

	void mousePress(byte button, byte keyState) {

		DWORD myMButton = -1;

		switch (button) {
		case 1:
			myMButton = (keyState == 1 ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
			break;
		case 2:
			myMButton = (keyState == 1 ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
			break;
		case 3:
			myMButton = (keyState == 1 ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
			break;
		}

		if (myMButton != -1) {
			mouse_event(myMButton, 0, 0, 0, 0);
		}
	}

	//Mouse click subroutine
	void mouseClick(byte button) {
		mousePress(button, 1);
		mousePress(button, 0);
	}

	//Keyboard click subroutine
	void keyboardClick(byte bVk) {
		keyPress(bVk, 1);
		keyPress(bVk, 0);
	}

	BOOL amIAdmin() {
		HANDLE hToken;
		OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hToken);

		DWORD infoLen;
		TOKEN_ELEVATION elevation;
		GetTokenInformation(
			hToken, TokenElevation,
			&elevation, sizeof(elevation), &infoLen);
		return (elevation.TokenIsElevated ? true : false);
	}

	BOOL RunAsAdmin(HWND hWnd, LPTSTR lpFile, LPTSTR lpParameters)
	{
		SHELLEXECUTEINFO   sei;
		ZeroMemory(&sei, sizeof(sei));

		sei.cbSize = sizeof(SHELLEXECUTEINFOW);
		sei.hwnd = hWnd;
		sei.fMask = SEE_MASK_FLAG_DDEWAIT | SEE_MASK_FLAG_NO_UI;
		sei.lpVerb = _TEXT("runas");
		sei.lpFile = lpFile;
		sei.lpParameters = lpParameters;
		sei.nShow = SW_SHOWNORMAL;

		if (!ShellExecuteEx(&sei))
		{
			//printf("Error: ShellExecuteEx failed 0x%x\n", GetLastError());
			return FALSE;
		}
		return TRUE;
	}

	void showDesktop() {
		keyPress(VK_LWIN, 1);
		keyboardClick(68);		//'D'
		keyPress(VK_LWIN, 0);
	}

	void showTaskView() {
		keyPress(VK_LWIN, 1);
		keyboardClick(VK_TAB);
		keyPress(VK_LWIN, 0);
	}

	//only works for Windows 10
	void newDesktop() {
		keyPress(VK_LWIN, 1);
		keyPress(VK_CONTROL, 1);
		keyboardClick(68);		//'D'
		keyPress(VK_CONTROL, 0);
		keyPress(VK_LWIN, 0);
	}

	//only works for Windows 10
	void killDesktop() {
		keyPress(VK_LWIN, 1);
		keyPress(VK_CONTROL, 1);
		keyboardClick(VK_F4);
		keyPress(VK_CONTROL, 0);
		keyPress(VK_LWIN, 0);
	}

	//only works for Windows 10
	void prevDesktop() {
		keyPress(VK_LWIN, 1);
		keyPress(VK_CONTROL, 1);
		keyboardClick(VK_LEFT);
		keyPress(VK_CONTROL, 0);
		keyPress(VK_LWIN, 0);
	}

	//only works for Windows 10
	void nextDesktop() {
		keyPress(VK_LWIN, 1);
		keyPress(VK_CONTROL, 1);
		keyboardClick(VK_RIGHT);
		keyPress(VK_CONTROL, 0);
		keyPress(VK_LWIN, 0);
	}

	//Get handle to the window below cursor and send it to foreground
	HWND getTarget(POINT cursorPos) {
		if (GetPhysicalCursorPos(&cursorPos)) {
			return RealChildWindowFromPoint(GetDesktopWindow(), cursorPos);	//RealChildWindowFromPoint gives us the best guess as to which window is relevant	
		}
		else {
			return NULL;
		}
	}

	void focusMyTarget(HWND cTarget) {
		if (cTarget != NULL) {
			SetForegroundWindow(cTarget);										//Send target to foreground
		}
	}

	BOOL closeTarget(HWND cTarget) {
		if (cTarget != NULL) {
			//Cannot use SendMessage because the thread will stop processing signals from controller
			PostMessage(cTarget, WM_CLOSE, 0, 0);
			return true;
		}
		return false;
	}

	BOOL minimizeTarget(HWND cTarget) {
		WINDOWPLACEMENT tWPInfo;
		if (cTarget != NULL) {
			printf("Windw minimized \n");
			return setShowCMD(cTarget, &tWPInfo, SW_MINIMIZE);
		}

		return false;
	}

	BOOL maximizeTarget(HWND cTarget) {
		WINDOWPLACEMENT tWPInfo;
		if (cTarget != NULL) {
			printf("Windw maximized. \n");
			return setShowCMD(cTarget, &tWPInfo, SW_MAXIMIZE);
		}
		return false;
	}

	BOOL restoreTarget(HWND cTarget) {
		WINDOWPLACEMENT tWPInfo;
		if (cTarget != NULL) {
			printf("Windw restored. \n");
			return setShowCMD(cTarget, &tWPInfo, SW_RESTORE);
		}
		return false;
	}

	BOOL setShowCMD(HWND tHandle, WINDOWPLACEMENT * ptWP, UINT showCMD) {
		if (GetWindowPlacement(tHandle, ptWP)) {				//Get window placement info
			(*ptWP).showCmd = showCMD;							//set showCMD value
			return SetWindowPlacement(tHandle, ptWP);			//Set the new window placement info
		}
		return false;
	}

	BOOL MaxRestoreTarget(HWND cTarget) {
		WINDOWPLACEMENT tWPInfo;
		if (cTarget != NULL) {
			if (GetWindowPlacement(cTarget, &tWPInfo)) {
				tWPInfo.showCmd = (tWPInfo.showCmd == SW_MAXIMIZE ? SW_RESTORE : SW_MAXIMIZE);
				return SetWindowPlacement(cTarget, &tWPInfo);
			}
		}

		return false;

	}



	ULARGE_INTEGER fetchFileTime()
	{
		FILETIME myFT;
		ULARGE_INTEGER oFT;
		GetSystemTimeAsFileTime(&myFT);
		oFT.HighPart = myFT.dwHighDateTime;
		oFT.LowPart = myFT.dwLowDateTime;
		return oFT;
	}

	LONG writeFloatToReg(HKEY hKey, LPTSTR subkey, float value) {
		char buffer[32];
		_snprintf(buffer, sizeof(buffer), "%f", value);
		return RegSetValueEx(hKey, subkey, 0, REG_SZ, (BYTE*)buffer, strlen(buffer));
	}

	LONG readFloatFromReg(HKEY hKey, LPTSTR subkey, float * vp) {
		//Only overwrite settings if it's available in registry.

		LONG retVal;
		char buffer[32];

		DWORD sz = sizeof(buffer);
		DWORD type = 0;
		retVal = RegQueryValueEx(hKey, subkey, 0, &type, (BYTE*)buffer, &sz);

		if (retVal == ERROR_SUCCESS && type == REG_SZ)
			*vp = atof(buffer);
		return retVal;
	}

	DWORD readDWORDFromReg(HKEY hKey, LPTSTR subkey, DWORD * vp) {
		LONG retVal;
		DWORD sz = sizeof(*vp);

		retVal = RegQueryValueEx(hKey, subkey, 0, 0, (BYTE*)vp, &sz);

		return retVal;
	}

}