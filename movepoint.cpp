// Based on MoveFrameworkSDK

#include "stdafx.h"
#include "movepoint.h"

class MoveObserver : public Move::IMoveObserver
{
	//Default values
	float scrollThreshold = 0.02;		//Threshold of movement before scrolling begins
	float appScrollThreshold = 0.1;		//Threshold of movement before scrolling begins in app-switching and zooming
	float mouseThreshold = 0.005;		//Threshold of movement before cursor moves 1:1 with handset. For reducing cursor jitter.
	float curPosWeight = 0.4;			//Weight of current handset position in calculating moving average. For reducing cursor jitter.
	int moveDelay = 200;				//Time to wait before executing press event in milliseconds. For reducing cursor shake while pressing button.
	
	//variables and objects
	Move::IMoveManager* move;
	int numMoves, myMoveDelay, myScrollDelay;

	RECT screenSize;
	RECTf ctrlRegion;
	POINT cursorPos, winCurDiff;
	Move::Vec3 oldPos, curPos, avgPos;
	Move::Quat myOrient;

	ULARGE_INTEGER	lHandler_FT, mouseClick_FT,
					squareHandler_FT, crossHandler_FT, triangleHandler_FT, circleHandler_FT,
					psClick_FT, selectHandler_FT, startHandler_FT,
					keyboardClick_FT;
		
	bool controllerOn = true;
	bool mouseMode = true;
	bool scrollMode = false;
	bool dragMode = false;
	bool keyboardMode = false; 
	bool appSwitchMode = false;
	bool zoomMode = false;
	bool snapMode = false;
	bool snapped = false;
	bool printPos = false;
	bool takeInitReading = true;

	byte calibrationMode = 0;

	//For drag operations
	HWND myTarget = 0;
	RECT tRect;
	POINT tSize;

	//For console window
	HWND myHWND = 0;
	WINDOWPLACEMENT myWPInfo;


public:
	MoveObserver()
	{
		hideMyself();

		move = Move::createDevice();

		numMoves=move->initMoves();
		move->initCamera(numMoves);

		move->subsribe(this);
		
		screenSize.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
		screenSize.right = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		screenSize.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
		screenSize.bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);

		printf("SCREEN left:%d  right:%d top:%d bottom:%d\n", 
			screenSize.left, screenSize.right, screenSize.top, screenSize.bottom);

		//initial values
		oldPos.x = -99999;
		oldPos.y = -99999;
		oldPos.z = -99999;
		curPos.x = 0;
		curPos.y = 0;
		curPos.z = 0;
		avgPos.x = 0;
		avgPos.y = 0;
		avgPos.z = 0;

		ctrlRegion.top = 20;
		ctrlRegion.bottom = -20;
		ctrlRegion.left = -30;
		ctrlRegion.right = 30;

		//read settings
		readSettings();
		
		if (mouseThreshold <= 0) mouseThreshold = 0.000001;
		myMoveDelay = moveDelay * 10000;			//movement detection delay in nanoseconds
		myScrollDelay = (moveDelay + 100) * 10000;	//scroll needs slightly more delay
	}

	void moveKeyPressed(int moveId, Move::MoveButton keyCode)
	{
		moveKeyProc(keyCode, 1);
	
	}

	void moveKeyReleased(int moveId, Move::MoveButton keyCode)
	{
		moveKeyProc(keyCode, 0);
	}

	void moveUpdated(int moveId, Move::MoveData data)
	{
		
		//Update position info
		curPos.x = max(min((data.position.x - ctrlRegion.left) / (ctrlRegion.right - ctrlRegion.left), 1), 0);
		curPos.y = (1 - max(min((data.position.y - ctrlRegion.bottom) / (ctrlRegion.top - ctrlRegion.bottom), 1), 0));
		curPos.z = data.position.z;

		//Update moving average
		avgPos.x = curPosWeight * curPos.x + (1 - curPosWeight) * avgPos.x;
		avgPos.y = curPosWeight * curPos.y + (1 - curPosWeight) * avgPos.y;
		avgPos.z = curPosWeight * curPos.z + (1 - curPosWeight) * avgPos.z;

		//Update previous position
		if (oldPos.x < -10000) oldPos.x = curPos.x;
		if (oldPos.y < -10000) oldPos.y = curPos.y;
		float oWeight = 0.99;
		myOrient.w = oWeight * myOrient.w + (1 - oWeight) * data.orientation.w;
		myOrient.v.x = oWeight * myOrient.v.x + (1 - oWeight) * data.orientation.v.x;
		myOrient.v.y = oWeight * myOrient.v.y + (1 - oWeight) * data.orientation.v.y;
		myOrient.v.z = oWeight * myOrient.v.z + (1 - oWeight) * data.orientation.v.z;

		if (takeInitReading) {
			myOrient.w = data.orientation.w;
			myOrient.v.x = data.orientation.v.x;
			myOrient.v.y = data.orientation.v.y;
			myOrient.v.z = data.orientation.v.z;
			takeInitReading = false;
		}
		
		if (calibrationMode > 0) {
			switch (calibrationMode){
				case 1:
					ctrlRegion.top = data.position.y;
					break;
				case 2:
					ctrlRegion.bottom = data.position.y;
					break;
				case 3:
					ctrlRegion.left = data.position.x;
					break;
				case 4:
					ctrlRegion.right = data.position.x;
					break;
			}
		}
		//Check if we are in scroll mode
		else if (scrollMode && (double)(fetchFileTime().QuadPart - lHandler_FT.QuadPart) > myScrollDelay) {
			scroll(moveId);
		}
		//Check if we are in snap mode
		else if (snapMode && (double)(fetchFileTime().QuadPart - squareHandler_FT.QuadPart) > myScrollDelay) {
			scroll(moveId);
		}
		//Check if we are in mouse mode
		else if ((mouseMode || dragMode) && (double)(fetchFileTime().QuadPart - mouseClick_FT.QuadPart) > myMoveDelay) {
			moveCursor(moveId);
			if (dragMode) {
				dragWindow(moveId);
			}
		} 
		else if (keyboardMode && (double)(fetchFileTime().QuadPart -keyboardClick_FT.QuadPart) > myMoveDelay) {
		/*keyboard uses a very slow moving average to determine what's the untilted position is. 
		  Not ideal but haven't figure out what would be a better solution.*/
			if (data.orientation.v.y - myOrient.v.y > 0.15) {
				keyboardClick(VK_LEFT);
			}
			else if (data.orientation.v.y - myOrient.v.y < -0.15) {
				keyboardClick(VK_RIGHT);
			}
			
			if (data.orientation.v.x - myOrient.v.x > 0.15) {
				keyboardClick(VK_UP);
			}
			else if (data.orientation.v.x - myOrient.v.x < -0.15) {
				keyboardClick(VK_DOWN);
			}
			keyboardClick_FT = fetchFileTime();
		}
				
		//Print position information
		if (printPos) {
			printf("MOVE id:%d   pos:%.2f %.2f %.2f   ori:%.2f %.2f %.2f %.2f   trigger:%d\n",
				moveId,
				data.position.x, data.position.y, data.position.z,
				data.orientation.w, data.orientation.v.x, data.orientation.v.y, data.orientation.v.z,
				data.trigger);
			printf("AVG id:%d   pos:%.2f %.2f %.2f   ori:%.2f %.2f %.2f %.2f   trigger:%d\n",
				moveId,
				curPos.x, curPos.y, curPos.z,
				myOrient.w, myOrient.v.x, myOrient.v.y, myOrient.v.z,
				data.trigger);
			if (GetPhysicalCursorPos(&cursorPos)) {
				printf("CURSOR pos:%d %d\n", cursorPos.x, cursorPos.y);				
			}
			//printf("TIME %d \n", fetchFileTime().QuadPart / 10000);
			//printf("MOVE pos:%.5f %.5f %.5f %.5f \n", avgPos.x, curPos.x, avgPos.y, curPos.y);
			printPos = false; 
			
		}

	}

	void navKeyPressed(int navId, Move::MoveButton keyCode)
	{
		printf("NAV id:%d   button pressed: %d\n", navId, (int)keyCode);
	}

	void navKeyReleased(int navId, Move::MoveButton keyCode)
	{
		printf("NAV id:%d   button released: %d\n", navId, (int)keyCode);
	}

	void navUpdated(int navId, Move::NavData data)
	{
		printf("NAV id:%d   trigger1: %d,  trigger2: %d, stick: %d,%d\n", navId, data.trigger1, data.trigger2, data.stickX, data.stickY);
	}

private:
	void updatePos()
	{
		oldPos.x = curPos.x;
		oldPos.y = curPos.y;
		oldPos.z = curPos.z;
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
		
	void moveKeyProc(Move::MoveButton keyCode, byte keyState)
	{		
		switch (keyCode)
		{
			case Move::B_NONE:
				break;
			case Move::B_T1:
				break;
			case Move::B_T2:
				break;

			case Move::B_TRIANGLE:
				triangleHandler(keyState);
				break;

			case Move::B_CIRCLE:
				circleHandler(keyState);
				break;

			case Move::B_CROSS:
				crossHandler(keyState);
				break;

			case Move::B_SQUARE:
				squareHandler(keyState);
				break;

			case Move::B_SELECT:
				selectHandler(keyState);
				break;

			case Move::B_START:
				startHandler(keyState);
				break;

			case Move::B_STICK:
					break;
			case Move::B_UP:
					break;
			case Move::B_RIGHT:
					break;
			case Move::B_DOWN:
					break;
			case Move::B_LEFT:
					break;

			case Move::B_PS:
				psHandler(keyState);
				break;

			case Move::B_MOVE:
				moveHandler(keyState);
				break;

			case Move::B_T:
				lHandler(keyState);
				break;
		}
	
	}

	//Keypress subroutine
	void keyPress(byte bVk, byte keyState)
	{
		if (!controllerOn) return;

		if (keyState == 1) {
			keybd_event(bVk, 0, 0, 0);
		}
		else {
			keybd_event(bVk, 0, KEYEVENTF_KEYUP, 0);
		}
	}

	void mousePress(byte button, byte keyState){

		if (!controllerOn) return;

		DWORD myMButton = -1;

		switch (button) {
			case 1:
				myMButton = (keyState == 1 ? MOUSEEVENTF_LEFTDOWN: MOUSEEVENTF_LEFTUP);
				//printf("Left press. Time:%d \n", fetchFileTime().QuadPart/10000);
				break;
			case 2:
				myMButton = (keyState == 1 ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
				//printf("Middle press. Time:%d \n", fetchFileTime().QuadPart / 10000);
				break;
			case 3:
				myMButton = (keyState == 1 ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
				//printf("Right press. Time:%d \n", fetchFileTime().QuadPart / 10000);
				break;
		}

		if (myMButton != -1) {
			mouse_event(myMButton, 0, 0, 0, 0);
			mouseClick_FT = fetchFileTime();
		}
	}

	//Mouse click subroutine
	void mouseClick(byte button){
		mousePress(button, 1);
		mousePress(button, 0);
	}

	//Keyboard click subroutine
	void keyboardClick(byte bVk){
		keyPress(bVk, 1);
		keyPress(bVk, 0);
	}

	//start enters keyboard mode
	void startHandler(byte keyState) {
		if (!controllerOn) return;

		if (keyState == 1) {
			if (!keyboardMode) {
				keyboardMode = true;
				mouseMode = false;
			}
			else {
				keyboardMode = false;
				mouseMode = true;
			}
		}
	}

	//debug message
	void selectHandler(byte keyState) {

		if (keyState == 1){
			//Record time when button is pressed
			selectHandler_FT = fetchFileTime();
		}
		else {
			if ((double)(fetchFileTime().QuadPart - selectHandler_FT.QuadPart) <= myScrollDelay) {
				//Quick click is interpreted as print debug message
				showMyself();
				printPos = !printPos;
			}
			else {
				//Long press hide console window
				hideMyself();
			}
		}
	}

	/*Triangle maps to:
		Mouse mode:		Right click
		Keyboard mode:	Tab
		scroll mode:	Control (to trigger zooming)
	*/
	void triangleHandler(byte keyState) {

		if (!controllerOn) return;

		if (scrollMode) {
			zoomMode = !zoomMode;
			keyPress(VK_CONTROL, keyState);
		}
		else {
			if (mouseMode) {
				mousePress(3, keyState);
			}
			else if (keyboardMode) {
				keyPress(VK_TAB, keyState);
			}
		}
	}

	/*Circle maps to:
		Mouse mode:		middle click
		Keyboard mode:	Print screen
		scroll mode:	
	*/
	void circleHandler(byte keyState) {

		if (!controllerOn) return;

		if (mouseMode) {
			mousePress(2, keyState);
		}
		else if (keyboardMode) {
			keyPress(VK_SNAPSHOT, keyState);
		}
	}

	/*Square maps to:
		Mouse mode: 
			click:		Win key
			long-click:	show desktop
		Keyboard mode:	Win key
		scroll mode:	Initiate Alt-Tab. Subsequent click is Tab.
	*/
	void squareHandler(byte keyState) {

		if (!controllerOn) return; 

		if (keyState == 1){
			//Record time when button is pressed
			squareHandler_FT = fetchFileTime();

			//In scroll mode, initiate Alt-Tab
			if (scrollMode) {
				appSwitchMode = true;
				keyPress(VK_MENU, 1);
				keyPress(VK_TAB, keyState);
			}
			else  {
				snapMode = true;
				focusMyTarget();
			}
		}
		else {
			if (appSwitchMode) {
				keyPress(VK_TAB,keyState);
			}
			else {
				//Quick click
				if ((double)(fetchFileTime().QuadPart - squareHandler_FT.QuadPart) <= myScrollDelay) {
					keyboardClick(VK_LWIN);
				}
				//long click
				else if (!snapped) {
					showDesktop();
				}

				snapMode = false;
				snapped = false;

			}
		}
	}

	/*Cross maps to:
		click:					Esc
		long click:				minimize app
		scroll mode long-click:	close app
	*/
	void crossHandler(byte keyState) {

		if (!controllerOn) return;

		if (keyState == 1){
			//Record time when button is pressed
			crossHandler_FT = fetchFileTime();
		}
		else {
			if ((double)(fetchFileTime().QuadPart - crossHandler_FT.QuadPart) <= myScrollDelay) {
			//Quick click is interpreted as Esc
				keyboardClick(VK_ESCAPE);
			}
			else {
			//long press
				if (scrollMode){
					//in scroll mode close app
					closeTarget();
				}
				else {
					//normally minimize app
					minimizeTarget();
				}
			}
		}
	}

	//PS button handler. Switch the controls on and off.
	//NOTE: PS long press CANNOT be used because the system will reset orientation.
	void psHandler(byte keyState) {
		if (keyState == 1) {
			psClick_FT = fetchFileTime();
		}
		else {
			if ((double)(fetchFileTime().QuadPart - psClick_FT.QuadPart) <= myScrollDelay) {
			//Quick click is interpreted as turning controller on or off
				controllerOn = !controllerOn;
				if (controllerOn) mouseMode = true;
			}
			else {
				takeInitReading = true;
				if (controllerOn) calibrateRegion();
			}
		}
	}

	//Move button on its own triggers left click in mouse mode and enter in keyboard mode. 
	//With L button it initiates drag mode.
	void moveHandler(byte keyState) {

		if (!controllerOn) return;

		if (calibrationMode > 0) {
			if (keyState == 0) {
				showCalibrationSteps();
			}
		}
		else if (scrollMode && keyState == 1) {
			//Enter drag mode when L button is already pressed
			getDragTarget();
			scrollMode = false;
			dragMode = true;
		}
		else if (dragMode && keyState == 0) {
			//Exit drag mode when Move button is released
			dragMode = false;
			mouseMode = true;
		}
		else if (keyboardMode) {
			//In keyboard mode, send an enter signal
			keyPress(VK_RETURN, keyState);
		}
		else {
			if (mouseMode == false) {
				//If mouse mode is off, turn it on
				mouseMode = true;
			}
			else{
				//If mouse mode is already on, send a left click
				mousePress(1, keyState);
			}
		}
	}

	/* L button handler.
		Click:					maximizes or restores app window
		Press and hold:			triggers scrolling
		With Move button:		triggers drag mode
		With Triangle button:	triggers zoom mode
	*/
	void lHandler(byte keyState) {

		if (!controllerOn) return;

		if (keyState == 1){
			//Record time when button is pressed
			scrollMode = true;
			lHandler_FT = fetchFileTime();

			focusMyTarget();
		}
		else {
			scrollMode = false;
			if (appSwitchMode) {
				//Exit app-switching
				keyPress(VK_MENU, 0);
				appSwitchMode = false;
				mouseMode = true;
			}
			//Quick click maximizes or restores app window
			else if ((double)(fetchFileTime().QuadPart - lHandler_FT.QuadPart) <= myScrollDelay) {
				MaxRestoreTarget();		//Switch between maximized and regular app window
			}
		}
	}

	//Move cursor subroutine
	void moveCursor(int moveId) {

		if (!controllerOn) return;

		float xPosWeight, yPosWeight;

		xPosWeight = min(abs(curPos.x - avgPos.x) / mouseThreshold,1);
		yPosWeight = min(abs(curPos.y - avgPos.y) / mouseThreshold,1);
		
		cursorPos.x = round((1 - xPosWeight) * cursorPos.x + xPosWeight * (curPos.x * screenSize.right + screenSize.left));
		cursorPos.y = round((1 - yPosWeight) * cursorPos.y + yPosWeight * (curPos.y * screenSize.bottom + screenSize.top));

		SetPhysicalCursorPos(cursorPos.x, cursorPos.y);
	}

	//Scrolling subroutine
	void scroll(int moveId) {

		if (!controllerOn) return;

		//set the desirable movement threshold
		float myThreshold = scrollThreshold;
		if (appSwitchMode || zoomMode) {
			myThreshold = appScrollThreshold;
		}
		else if (snapMode) {
			myThreshold = appScrollThreshold * 2;
		}

		//scroll up?
		if (curPos.y < oldPos.y - myThreshold || curPos.y == 0) {
			if (snapMode) {
				keyPress(VK_LWIN, 1);
				keyboardClick(VK_UP);
				keyPress(VK_LWIN, 0);
				snapped = true;
			}
			else {
				mouse_event(MOUSEEVENTF_WHEEL, 0, 0, WHEEL_DELTA, 0);
				mouseMode = false;
			}
			//printf("MOVE id:%d   scroll up %.3f %.3f %.3f \n", moveId, oldPos.y, curPos.y);
			updatePos();
		}
		//scroll down?
		else if (curPos.y > oldPos.y + myThreshold || curPos.y == 1) {
			if (snapMode) {
				keyPress(VK_LWIN, 1);
				keyboardClick(VK_DOWN);
				keyPress(VK_LWIN, 0);
				snapped = true;
			}
			else {
				mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -1 * WHEEL_DELTA, 0);
				mouseMode = false;
			}
			//printf("MOVE id:%d   scroll down %.3f %.3f %.3f \n", moveId, oldPos.y, curPos.y);
			updatePos();
		}

		//scroll left?
		if (curPos.x < oldPos.x - myThreshold || curPos.x == 0) {
			if (snapMode) {
				keyPress(VK_LWIN, 1);
				keyboardClick(VK_LEFT);
				keyPress(VK_LWIN, 0);
				snapped = true;
			}
			else {
				mouse_event(MOUSEEVENTF_HWHEEL, 0, 0, -1 * WHEEL_DELTA, 0);
				mouseMode = false;
			}
			//printf("MOVE id:%d   scroll left %.3f %.3f %.3f \n", moveId, oldPos.x, curPos.x);
			updatePos();
		}
		//scroll right?
		else if (curPos.x > oldPos.x + myThreshold || curPos.x == 1) {
			if (snapMode) {
				keyPress(VK_LWIN, 1);
				keyboardClick(VK_RIGHT);
				keyPress(VK_LWIN, 0);
				snapped = true;
			}
			else {
				mouse_event(MOUSEEVENTF_HWHEEL, 0, 0, WHEEL_DELTA, 0);
				mouseMode = false;
			}
			//printf("MOVE id:%d   scroll right  %.3f %.3f %.3f \n", moveId, oldPos.x, curPos.x);
			updatePos();
		}
	}

	//Drag subroutine
	void dragWindow(int moveId) {

		if (!controllerOn) return;

		BOOL retVal;

		retVal = MoveWindow(myTarget, cursorPos.x-winCurDiff.x, cursorPos.y - winCurDiff.y, tSize.x, tSize.y, true);
	}

	//Get handle to the window below cursor and send it to foreground
	HWND getTarget() {
		if (GetPhysicalCursorPos(&cursorPos)) {
			return RealChildWindowFromPoint(GetDesktopWindow(), cursorPos);	//RealChildWindowFromPoint gives us the best guess as to which window is relevant	
		}
	}
	
	void focusMyTarget() {
		WINDOWPLACEMENT tWPInfo;
		myTarget = getTarget();

		if (myTarget != NULL) {
			SetForegroundWindow(myTarget);										//Send target to foreground
		}
	}

	void getDragTarget() {
		
		WINDOWPLACEMENT tWPInfo;
		myTarget = getTarget();

		if (myTarget != NULL) {
			SetForegroundWindow(myTarget);										//Send target to foreground
			GetWindowPlacement(myTarget, &tWPInfo);								//Get window placement info
			tWPInfo.showCmd = SW_RESTORE;										//Restore size and position (if maximized)
			SetWindowPlacement(myTarget, &tWPInfo);								//Set the new window placement info
			GetWindowRect(myTarget, &tRect);									//Get the restored size and position
			tSize.x = (tRect.right - tRect.left);								//Calculate distance from cursor
			tSize.y = (tRect.bottom - tRect.top);

			winCurDiff.x = cursorPos.x - tRect.left;							//This difference is kept while dragging
			winCurDiff.y = cursorPos.y - tRect.top;

			//printf("HWND:%d %d %d %d %d %d %d \n", myTarget, tRect.left, tRect.top, tSize.x, tSize.y, winCurDiff.x, winCurDiff.y);
		}

	}

	void closeTarget() {
		HWND cTarget = getTarget();
		if (cTarget!=NULL) {
			//Cannot use SendMessage because the thread will stop processing signals from controller
			PostMessage(cTarget, WM_CLOSE, 0, 0);	
		}
	}

	BOOL minimizeTarget() {
		WINDOWPLACEMENT tWPInfo;
		HWND cTarget = getTarget();
		if (cTarget != NULL) {
			return setShowCMD(cTarget, &tWPInfo, SW_MINIMIZE);
		}
	}

	BOOL maximizeTarget() {
		WINDOWPLACEMENT tWPInfo;
		HWND cTarget = getTarget();
		if (cTarget != NULL) {
			return setShowCMD(cTarget, &tWPInfo, SW_MAXIMIZE);
		}
	}

	BOOL restoreTarget() {
		WINDOWPLACEMENT tWPInfo;
		HWND cTarget = getTarget();
		if (cTarget != NULL) {
			return setShowCMD(cTarget, &tWPInfo, SW_RESTORE);
		}
	}

	BOOL setShowCMD(HWND tHandle, WINDOWPLACEMENT * ptWP, UINT showCMD) {
		if (GetWindowPlacement(tHandle, ptWP)) {				//Get window placement info
			(*ptWP).showCmd = showCMD;							//set showCMD value
			return SetWindowPlacement(tHandle, ptWP);			//Set the new window placement info
		}
	}

	BOOL MaxRestoreTarget() {
		WINDOWPLACEMENT tWPInfo;
		HWND cTarget = getTarget();
		if (cTarget != NULL) {
			if (GetWindowPlacement(cTarget, &tWPInfo)) {
				tWPInfo.showCmd = (tWPInfo.showCmd == SW_MAXIMIZE ? SW_RESTORE : SW_MAXIMIZE);
				return SetWindowPlacement(cTarget, &tWPInfo);
			}
		}
	}

	void showDesktop() {
		keyPress(VK_LWIN, 1);
		keyboardClick(68);		//'D'
		keyPress(VK_LWIN, 0);
	}

	void calibrateRegion() {
		
		showMyself();

		printf("Calibration started.\n");
		calibrationMode = 1;
		printf("Point the controller towards the top of your screen and click the move button.\n");
		
	}

	void showCalibrationSteps() {
		switch (calibrationMode) {
		case 1:
			calibrationMode++;
			printf("Point the controller towards the bottom of your screen and click the move button.\n");
			break;
		case 2:
			calibrationMode++;
			printf("Point the controller towards the left side of your screen and click the move button.\n");
			break;
		case 3:
			calibrationMode++;
			printf("Point the controller towards the right side of your screen and click the move button.\n");
			break;
		case 4:
			calibrationMode = 0;
			saveSettings();
			printf("Calibration completed. Top:%.2f Bottom:%.2f Left:%.2f Right:%.2f \n", ctrlRegion.top, ctrlRegion.bottom, ctrlRegion.left, ctrlRegion.bottom);

			if (abs(ctrlRegion.top - ctrlRegion.bottom) < 30) {
				printf("WARNING: Cursor might move very fast due to vertical distance being less than 30. A value of around 40 is recommended.");
			}
			if (abs(ctrlRegion.right - ctrlRegion.left) < 30) {
				printf("WARNING: Cursor might move very fast due to horizontal distance being less than 40. A value of around 60 is recommended.");
			}
			break;
		}
	}

	void saveSettings() {
		
		LONG retVal1, retVal2, retVal3;
		HKEY hKey;
		DWORD dwDisp;

		retVal1 = RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\MOVEpoint"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisp);

		retVal2 = writeFloatToReg(hKey, TEXT("scrollThreshold"), scrollThreshold);
		retVal2 = writeFloatToReg(hKey, TEXT("appScrollThreshold"), appScrollThreshold);
		retVal2 = writeFloatToReg(hKey, TEXT("mouseThreshold"), mouseThreshold);
		retVal2 = writeFloatToReg(hKey, TEXT("curPosWeight"), curPosWeight);
		retVal2 = RegSetValueEx(hKey, TEXT("moveDelay"), 0, REG_DWORD, (const BYTE*)&moveDelay, sizeof(moveDelay));

		retVal2 = writeFloatToReg(hKey, TEXT("ctrlRegionT"), ctrlRegion.top);
		retVal2 = writeFloatToReg(hKey, TEXT("ctrlRegionB"), ctrlRegion.bottom);
		retVal2 = writeFloatToReg(hKey, TEXT("ctrlRegionL"), ctrlRegion.left);
		retVal2 = writeFloatToReg(hKey, TEXT("ctrlRegionR"), ctrlRegion.right);

		retVal3 = RegCloseKey(hKey);

		printf("Save Settings: %d %d %d \n", retVal1, retVal2, retVal3);
		
	}

	LONG writeFloatToReg(HKEY hKey, LPTSTR subkey, float value) {
		char buffer[32];
		_snprintf(buffer, sizeof(buffer), "%f", value);
		return RegSetValueEx(hKey, subkey, 0, REG_SZ, (BYTE*)buffer, strlen(buffer));
	}

	void readSettings() {

		LONG retVal1, retVal2, retVal3;
		HKEY hKey;
		DWORD dwDisp;

		retVal1 = RegCreateKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\MOVEpoint"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, &dwDisp);

		readFloatFromReg(hKey, TEXT("scrollThreshold"), &scrollThreshold);
		readFloatFromReg(hKey, TEXT("appScrollThreshold"), &appScrollThreshold);
		readFloatFromReg(hKey, TEXT("mouseThreshold"), &mouseThreshold);
		readFloatFromReg(hKey, TEXT("curPosWeight"), &curPosWeight);
		readFloatFromReg(hKey, TEXT("ctrlRegionT"), &ctrlRegion.top);
		readFloatFromReg(hKey, TEXT("ctrlRegionB"), &ctrlRegion.bottom);
		readFloatFromReg(hKey, TEXT("ctrlRegionL"), &ctrlRegion.left);
		retVal2 = readFloatFromReg(hKey, TEXT("ctrlRegionR"), &ctrlRegion.right);

		readDWORDFromReg(hKey, TEXT("moveDelay"), (DWORD*)&moveDelay);

		retVal3 = RegCloseKey(hKey);

		printf("Read Settings: %d %d %d %.3f %.3f %.3f %.3f %d %.3f %.3f %.3f %.3f \n", 
				retVal1, retVal2, retVal3,
				scrollThreshold, appScrollThreshold, mouseThreshold, curPosWeight, moveDelay, 
				ctrlRegion.top, ctrlRegion.bottom,ctrlRegion.left,ctrlRegion.right);
	}

	LONG readFloatFromReg(HKEY hKey, LPTSTR subkey, float * vp){
	//Only overwrite settings if it's available in registry.

		LONG retVal;
		char buffer[32];
		
		DWORD sz = sizeof(buffer);
		DWORD type = 0;
		retVal = RegQueryValueEx(hKey, subkey, 0, &type, (BYTE*)buffer, &sz);
		
		if (retVal==ERROR_SUCCESS && type == REG_SZ)
			*vp = atof(buffer);
		return retVal;
	}

	DWORD readDWORDFromReg(HKEY hKey, LPTSTR subkey, DWORD * vp){
		LONG retVal;
		DWORD sz = sizeof(*vp);

		retVal = RegQueryValueEx(hKey, subkey, 0, 0, (BYTE*)vp, &sz);
		
		return retVal;
	}

	//Restore size and position of console window
	BOOL showMyself() {
		myHWND = (myHWND == NULL ? GetConsoleWindow() : myHWND);
		return setShowCMD(myHWND, &myWPInfo, SW_RESTORE);
	}

	//Hide console window
	BOOL hideMyself() {
		myHWND = (myHWND==NULL ? GetConsoleWindow() : myHWND);
		return setShowCMD(myHWND, &myWPInfo, SW_HIDE);
	}

};



int main(int argc, char* argv[])
{
	MoveObserver* observer = new MoveObserver();

	getchar();
	return 0;
}


