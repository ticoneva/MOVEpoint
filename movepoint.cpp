// Based on MoveFrameworkSDK

#include "stdafx.h"
#include "movepoint.h"

class MoveObserver : public Move::IMoveObserver
{
	//Default values
	const float scrollThreshold_d = 0.02;		//Threshold of movement before scrolling begins
	const float appScrollThreshold_d = 0.1;		//Threshold of movement before scrolling begins in app-switching and zooming
	const float mouseThreshold_d = 0.005;		//Threshold of movement before cursor moves 1:1 with handset. For reducing cursor jitter.
	const float curPosWeight_d = 0.4;			//Weight of current handset position in calculating moving average. For reducing cursor jitter.
	const int moveDelay_d = 200;				//Time to wait before executing press event in milliseconds. For reducing cursor shake while pressing button.
	
	//variables and objects
	Move::IMoveManager* move;
	int numMoves;

	//Settings
	float scrollThreshold = 0.02;
	float appScrollThreshold = 0.1;
	float mouseThreshold = 0.005;
	float curPosWeight = 0.4;
	int moveDelay = 200; 
	int myMoveDelay, myScrollDelay;

	//Position
	RECT screenSize;
	RECTf ctrlRegion, ctrlRegion_d;
	POINT cursorPos, winCurDiff;
	Move::Vec3 oldPos, curPos, avgPos;
	Move::Quat avgOrient;

	//Timers
	ULARGE_INTEGER	cur_FT,
					lHandler_FT, mouseClick_FT, keyboardClick_FT,
					squareHandler_FT, crossHandler_FT, triangleHandler_FT, circleHandler_FT,
					psClick_FT, selectHandler_FT, startHandler_FT;
	
	//Modes
	bool controllerOn = true;
	bool mouseMode = true;
	bool scrollMode = false;
	bool dragMode = false;
	bool dragMode2 = false;
	bool keyboardMode = false; 
	bool appSwitchMode = false;
	bool appSwitchMode2 = false;
	bool zoomMode = false;
	bool snapMode = false;
	bool tiltMode = false;

	//Status
	bool snapped = false;
	bool targetClosed = false; 
	
	bool squarePressed = false;
	bool crossPressed = false;
	bool trianglePressed = false;
	bool circlePressed = false;
	bool movePressed = false;
	bool LPressed = false; 
		
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
		setupConsole();					//setting up the console window

		move = Move::createDevice();
		pairNewMoves();					//This pairs any unpaired controllers via USB
		numMoves=move->initMoves();
		initCamera();					//Do we have camera?
		move->subsribe(this);
		
		initValues();					//intial values for variables
		restoreDefaults();				//default settings
		readSettings();					//read settings from registry
	}

	void moveKeyPressed(int moveId, Move::MoveButton keyCode)
	{
		printf("MOVE id:%d   button pressed: %d\n", moveId, (int)keyCode);
		moveKeyProc(keyCode, 1);
	
	}

	void moveKeyReleased(int moveId, Move::MoveButton keyCode)
	{
		printf("MOVE id:%d   button released: %d\n", moveId, (int)keyCode);
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

		if (takeInitReading) {
			takeInitOrient(data);
		}
		
		//Check if we are in calibration mode
		if (calibrationMode > 0) {
			calibrateRecordPos(data);
		}
		else if (scrollMode || snapMode || mouseMode || dragMode || keyboardMode) {

			cur_FT = fetchFileTime();

			//Check if we are in scroll mode
			if (scrollMode && (double)(cur_FT.QuadPart - lHandler_FT.QuadPart) > myScrollDelay) {
				scroll(moveId);
			}
			//Check if we are in snap mode
			else if (snapMode && (double)(cur_FT.QuadPart - squareHandler_FT.QuadPart) > myScrollDelay) {
				scroll(moveId);
			}
			//Check if we are in mouse mode
			else if ((mouseMode || dragMode || dragMode2) && (double)(cur_FT.QuadPart - mouseClick_FT.QuadPart) > myMoveDelay) {
				if (tiltMode) {
					moveCursorTilt(moveId, data);
				}
				else {
					moveCursor(moveId);
				}
				if (dragMode || dragMode2) {
					dragWindow(moveId);
				}
			}
			else if (keyboardMode && (double)(cur_FT.QuadPart - keyboardClick_FT.QuadPart) > myMoveDelay) {
				moveArrows(moveId, data);
			}
		}
				
		//Print position information
		if (printPos) {
			printDebugMessage(moveId, data);
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
			if (!tiltMode) {
				keyboardMode = true;
				mouseMode = false;
				printf("To use tilt mode or keyboard mode the controller's orientation must be calibrated.\n");
				printf("Please point the controller towards the screen and press the PS button for 2 seconds.\n");
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
			if ((double)(fetchFileTime().QuadPart - selectHandler_FT.QuadPart) > myScrollDelay) {
				//Long press hide console window
				hideMyself();
			}
			else {
				//Quick click is interpreted as print debug message
				showMyself();
				printPos = !printPos;
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
			squarePressed = true;
			squareHandler_FT = fetchFileTime();	//Record time when button is pressed

			//In scroll mode, initiate Alt-Tab
			if (scrollMode) {
				appSwitchMode = true;
				keyPress(VK_MENU, 1);
				keyPress(VK_TAB, keyState);
			}
			else  {
			//Long press square alone enables snap mode and show desktop.
				snapMode = true;
				myTarget = getTarget();
			}
		}
		else {
			squarePressed = false;

			if (appSwitchMode) {
				//if we entered app switching with L button holding first, release tab
				keyPress(VK_TAB, keyState);
			} 
			else if (appSwitchMode2) {
				//if we entered app switching with square button holding first, release Alt
				keyPress(VK_MENU, keyState);
				appSwitchMode2 = false;
				mouseMode = true;
			}
			else {
				//Long click
				if ((double)(fetchFileTime().QuadPart - squareHandler_FT.QuadPart) > myScrollDelay) {
					if (!snapped) showDesktop();	//if a snap hasn't occured, show desktop
					
				}
				//Quick click
				else {
					keyboardClick(VK_LWIN);
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
			crossPressed = true;
			crossHandler_FT = fetchFileTime();	//Record time when button is pressed
		}
		else {
			crossPressed = false;

			if ((double)(fetchFileTime().QuadPart - crossHandler_FT.QuadPart) > myScrollDelay) {
				//long press
				if (scrollMode){
					//in scroll mode close app
					closeTarget();
				}
				else if (targetClosed) {
					//If we closed a window, don't minimize the next one
					targetClosed = false;
				}
				else {
					//normally minimize app
					minimizeTarget();
				}
			}
			else {
				if (calibrationMode > 0) {
					calibrationMode = 0;
					printf("Calibration canceled. \n");
				}
				else {
					//Quick click is interpreted as Esc
					keyboardClick(VK_ESCAPE);
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
			if ((double)(fetchFileTime().QuadPart - psClick_FT.QuadPart) > myScrollDelay) {
				//Long click starts calibration mode or restore defaults
				if (calibrationMode > 0) {
					restoreDefaults();
					saveSettings();
					printf("Default values restored. \n");
					calibrationMode = 0;
				}
				else {
					takeInitReading = true;
					if (controllerOn) calibrateRegion();
				}
			}
			else {
				//Quick click is interpreted as turning controller on or off
				controllerOn = !controllerOn;
				if (controllerOn) { 
					mouseMode = true;
					/* Can't use because close actions don't do anything
					move->initMoves();
					initCamera();
					*/
				}
				else {
					/*These doesn't seem to do anything
					move->closeCamera();
					move->closeMoves();
					*/
				}
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
		else if (dragMode2 && keyState == 0) {
			dragMode2 = false;
			mouseMode = true;
			mousePress(1, 0);
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
				movePressed = (keyState == 1 ? true : false);
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
			lHandler_FT = fetchFileTime(); //Record time when button is pressed

			if (squarePressed) {
				//app-switching if square is pressed first
				appSwitchMode2 = true;	
				snapMode = false;		//disable snapping 
				keyPress(VK_MENU, 1);
				keyPress(VK_TAB, keyState);
			}
			else if (movePressed) {
				//enter drag mode with move button pressed first
				getDragTarget();
				scrollMode = false;
				dragMode2 = true;
			}
			else if (!crossPressed) {
				//otherwise enter scrollMode
				scrollMode = true;
				focusMyTarget();
			}
		}
		else {
			scrollMode = false;
			if (appSwitchMode) {
				//Exit app-switching (if L is pressed first)
				keyPress(VK_MENU, 0);
				appSwitchMode = false;
				mouseMode = true;
			}
			else if (crossPressed) {
				closeTarget();
			}
			else if (appSwitchMode2){
				//app-switching if square is pressed first
				keyPress(VK_TAB, keyState);
			}
			//Quick click maximizes or restores app window. 
			else if ((double)(fetchFileTime().QuadPart - lHandler_FT.QuadPart) <= myScrollDelay) {
					MaxRestoreTarget();		//Switch between maximized and regular app window
			}
		}
	}

	//Move arrow keys
	void moveArrows(int moveId, Move::MoveData data) {
		/*keyboard mode requires calibration everytime because default orientation seems to change
		with each startup */
		if (data.orientation.v.y - avgOrient.v.y > 0.15) {
			keyboardClick(VK_LEFT);
		}
		else if (data.orientation.v.y - avgOrient.v.y < -0.15) {
			keyboardClick(VK_RIGHT);
		}

		if (data.orientation.v.x - avgOrient.v.x > 0.15) {
			keyboardClick(VK_UP);
		}
		else if (data.orientation.v.x - avgOrient.v.x < -0.15) {
			keyboardClick(VK_DOWN);
		}
		keyboardClick_FT = fetchFileTime();
	}

	//Move cursor subroutine
	void moveCursor(int moveId=0) {

		if (!controllerOn) return;

		float xPosWeight, yPosWeight;

		xPosWeight = min(abs(curPos.x - avgPos.x) / mouseThreshold, 1);
		yPosWeight = min(abs(curPos.y - avgPos.y) / mouseThreshold, 1);

		cursorPos.x = round((1 - xPosWeight) * cursorPos.x + xPosWeight * (curPos.x * screenSize.right + screenSize.left));
		cursorPos.y = round((1 - yPosWeight) * cursorPos.y + yPosWeight * (curPos.y * screenSize.bottom + screenSize.top)); 

		SetPhysicalCursorPos(cursorPos.x, cursorPos.y);
	}

	//Move cursor subroutine
	void moveCursorTilt(int moveId, Move::MoveData data) {

		if (!controllerOn) return;

		if (data.orientation.v.y - avgOrient.v.y > 0.2) {
			cursorPos.x = cursorPos.x - 2;
		}
		else if (data.orientation.v.y - avgOrient.v.y < -0.2) {
			cursorPos.x = cursorPos.x + 2;
		}

		if (data.orientation.v.x - avgOrient.v.x > 0.2) {
			cursorPos.y = cursorPos.y - 2;
		}
		else if (data.orientation.v.x - avgOrient.v.x < -0.2) {
			cursorPos.y = cursorPos.y + 2;
		}

		SetPhysicalCursorPos(cursorPos.x, cursorPos.y);
	}

	//Scrolling subroutine
	void scroll(int moveId=0) {

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
				if (myTarget != NULL) {
					SetForegroundWindow(myTarget);	//target was acquired with square button press
				}
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
	void dragWindow(int moveId=0) {

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
	
	void focusMyTarget(HWND inTarget=NULL) {
		myTarget = (inTarget!=NULL ? inTarget : getTarget());
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
			targetClosed = true;
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

		printf("Orientation calibrated.\n");
		printf("Click the move button to continue calibration of screen area. Click X to quit.\n");
		calibrationMode = 1;
	}

	void showCalibrationSteps() {
		switch (calibrationMode) {
		case 1:
			calibrationMode++;
			printf("To restore default values for all settings long click PS button again anytime during calibration. \n");
			printf("Point the controller towards the top of your screen and click the move button.\n");
			break;
		case 2:
			calibrationMode++;
			printf("Point the controller towards the bottom of your screen and click the move button.\n");
			break;
		case 3:
			calibrationMode++;
			printf("Point the controller towards the left side of your screen and click the move button.\n");
			break;
		case 4:
			calibrationMode++;
			printf("Point the controller towards the right side of your screen and click the move button.\n");
			break;
		case 5:
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

	void calibrateRecordPos(Move::MoveData data){
		switch (calibrationMode){
		case 2:
			ctrlRegion.top = data.position.y;
			break;
		case 3:
			ctrlRegion.bottom = data.position.y;
			break;
		case 4:
			ctrlRegion.left = data.position.x;
			break;
		case 5:
			ctrlRegion.right = data.position.x;
			break;
		}
	}
	
	void takeInitOrient(Move::MoveData data) {
		avgOrient.w = data.orientation.w;
		avgOrient.v.x = data.orientation.v.x;
		avgOrient.v.y = data.orientation.v.y;
		avgOrient.v.z = data.orientation.v.z;
		takeInitReading = false;
	}

	void restoreDefaults() {
		ctrlRegion = ctrlRegion_d;
		scrollThreshold = scrollThreshold_d;
		appScrollThreshold = appScrollThreshold_d;
		mouseThreshold = mouseThreshold_d;
		curPosWeight = curPosWeight_d;
		moveDelay = moveDelay_d;

		calSettings();
	}

	void calSettings() {
		if (mouseThreshold <= 0) mouseThreshold = 0.000001;
		myMoveDelay = moveDelay * 10000;						//movement detection delay in nanoseconds
		myScrollDelay = (moveDelay + 100) * 10000;				//scroll needs slightly more delay
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

		printf("Save Settings. Return value: %d %d %d \n", retVal1, retVal2, retVal3);
		
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
		readFloatFromReg(hKey, TEXT("ctrlRegionR"), &ctrlRegion.right);

		retVal2 = readDWORDFromReg(hKey, TEXT("moveDelay"), (DWORD*)&moveDelay);

		retVal3 = RegCloseKey(hKey);

		calSettings();

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

	void initValues() {
		screenSize.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
		screenSize.right = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		screenSize.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
		screenSize.bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);

		oldPos.x = -99999;
		oldPos.y = -99999;
		oldPos.z = -99999;
		curPos.x = 0;
		curPos.y = 0;
		curPos.z = 0;
		avgPos.x = 0;
		avgPos.y = 0;
		avgPos.z = 0;

		ctrlRegion_d.top = 20;
		ctrlRegion_d.bottom = -20;
		ctrlRegion_d.left = -30;
		ctrlRegion_d.right = 30;
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

	void setupConsole() {
		//Only show console window in debug build
		#ifdef NDEBUG
				hideMyself();
		#endif
		printf("Press [SELECT] to show or hide console. \n", numMoves);
	}

	void pairNewMoves() {
		numMoves = move->pairMoves();
		if (numMoves > 0) {
			showMyself();
			printf("%d controller(s) paired. \n", numMoves);
			printf("Please pair the controller(s) in your Bluetooth settings.\n");
		}
	}

	void initCamera() {
		//Do we have camera?
		if (!move->initCamera(numMoves)) {
			showMyself();
			printf("No PS Eye Camera found. \n", numMoves);
			tiltMode = true;
		}
	}

	void printDebugMessage(int moveId, Move::MoveData data) {
		printf("MOVE id:%d   pos:%.2f %.2f %.2f   ori:%.2f %.2f %.2f %.2f   trigger:%d\n",
			moveId,
			data.position.x, data.position.y, data.position.z,
			data.orientation.w, data.orientation.v.x, data.orientation.v.y, data.orientation.v.z,
			data.trigger);
		printf("AVG NORMALIZED pos:%.2f %.2f %.2f   ori:%.2f %.2f %.2f %.2f\n",
			avgPos.x, avgPos.y, avgPos.z,
			avgOrient.w, avgOrient.v.x, avgOrient.v.y, avgOrient.v.z);
		if (GetPhysicalCursorPos(&cursorPos)) {
			printf("CURSOR pos:%d %d\n", cursorPos.x, cursorPos.y);
		}
		printf("SCREEN left:%d  right:%d top:%d bottom:%d\n",
			screenSize.left, screenSize.right, screenSize.top, screenSize.bottom);
		//printf("TIME %d \n", fetchFileTime().QuadPart / 10000);
		//printf("MOVE pos:%.5f %.5f %.5f %.5f \n", avgPos.x, curPos.x, avgPos.y, curPos.y);
		printPos = false;
	}

};



int main(int argc, char* argv[])
{
	MoveObserver* observer = new MoveObserver();

	getchar();
	return 0;
}


