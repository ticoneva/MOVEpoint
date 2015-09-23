#include "stdafx.h"
#include "MoveObserver.h"

	MoveObserver::MoveObserver()
	{
		setupConsole();					//setting up the console window

		checkAdminRights();				//check if program has admin rights and prompt if not

		initValues();					//intial values for variables
		restoreDefaults();				//default settings
		readSettings();					//read settings from registry

		move = Move::createDevice();
		pairNewMoves();					//This pairs any unpaired controllers via USB
		initializeSystem();

	}

	int MoveObserver::getNumMoves() {
		return move->getMoveCount();
	}

	void MoveObserver::pairNewMoves() {
		numMoves = move->pairMoves();
		if (numMoves > 0) {
			showMyself();
			printf("%d controller(s) paired. \n", numMoves);
			printf("Please pair the controller(s) in your Bluetooth settings.\n");
		}
	}

	void MoveObserver::initializeSystem() {
		numMoves = move->initMoves();
		if (numMoves > 0) {
			initCamera();					//Do we have camera?
			move->subsribe(this);
			ShowCursor(true);
		}
		else {
			closeTarget(myHWND);
		}
	}

	void MoveObserver::terminateSystem() {
		//For testing purpose. None of these really do anything.
		//Seems like the only way to terminate is to terminate the whole process.

		if (numMoves > 0) {
			move->unsubsribe(this);
			move->closeCamera();
		}
		move->closeMoves();
		delete move;			//<--This would throw an exception
		move = nullptr;
	}

	void MoveObserver::moveKeyPressed(int moveId, Move::MoveButton keyCode)
	{
#ifdef DEBUG
		printf("MOVE id:%d   button pressed: %d\n", moveId, (int)keyCode);
#endif
		moveKeyProc(keyCode, 1);

	}

	void MoveObserver::moveKeyReleased(int moveId, Move::MoveButton keyCode)
	{
#ifdef DEBUG
		printf("MOVE id:%d   button released: %d\n", moveId, (int)keyCode);
#endif
		moveKeyProc(keyCode, 0);
	}

	void MoveObserver::moveUpdated(int moveId, Move::MoveData data)
	{

		//Update position info
		curPosNorm.x = max(min((data.position.x - ctrlRegion.left) / (ctrlRegion.right - ctrlRegion.left), 1), 0);
		curPosNorm.y = (1 - max(min((data.position.y - ctrlRegion.bottom) / (ctrlRegion.top - ctrlRegion.bottom), 1), 0));
		curPosNorm.z = data.position.z;

		//Update moving average
		avgPos.x = curPosWeight * data.position.x + (1 - curPosWeight) * avgPos.x;
		avgPos.y = curPosWeight * data.position.y + (1 - curPosWeight) * avgPos.y;
		avgPos.z = curPosWeight * data.position.z + (1 - curPosWeight) * avgPos.z;

		if (oldPos.x < -10000) updatePos(data);			//Update previous position
		if (takeInitReading) takeInitOrient(data);		//Initial orientation

														//Check if we are in calibration mode
		if (calibrationMode > 0) {
			calibrateRecordPos(data);
		}
		else if (scrollMode || snapMode || mouseMode || dragMode || keyboardMode) {

			cur_FT = fetchFileTime();

			//Check if we are in scroll mode
			if (scrollMode && (double)(cur_FT.QuadPart - lHandler_FT.QuadPart) > myScrollDelay) {
				scroll(moveId, data);
			}
			//Check if we are in snap mode
			else if ((snapMode || desktopMode) && (double)(cur_FT.QuadPart - squareHandler_FT.QuadPart) > myScrollDelay) {
				scroll(moveId, data);
			}
			//Check if we are in mouse mode
			else if ((mouseMode || dragMode || dragMode2) && (double)(cur_FT.QuadPart - moveHandler_FT.QuadPart) > myMoveDelay) {
				moveCursor(moveId, data);
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

	void MoveObserver::navKeyPressed(int navId, Move::MoveButton keyCode)
	{
		printf("NAV id:%d   button pressed: %d\n", navId, (int)keyCode);
	}

	void MoveObserver::navKeyReleased(int navId, Move::MoveButton keyCode)
	{
		printf("NAV id:%d   button released: %d\n", navId, (int)keyCode);
	}

	void MoveObserver::navUpdated(int navId, Move::NavData data)
	{
		printf("NAV id:%d   trigger1: %d,  trigger2: %d, stick: %d,%d\n", navId, data.trigger1, data.trigger2, data.stickX, data.stickY);
	}

	void MoveObserver::extraStableX(BOOL stabilize) {
		stableX = stabilize;
	}
	void MoveObserver::extraStableY(BOOL stabilize) {
		stableY = stabilize;
	}

	void MoveObserver::updatePos(Move::MoveData data)
	{
		oldPos.x = data.position.x;
		oldPos.y = data.position.y;
		oldPos.z = data.position.z;
		old_FT = fetchFileTime();
	}

	void MoveObserver::moveKeyProc(Move::MoveButton keyCode, byte keyState)
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

	//start enters keyboard mode
	void MoveObserver::startHandler(byte keyState) {
		if (!controllerOn) return;

		//launch Windows on-screen keyboard
		if (keyState == 0) {
			if (amIAdmin()) {
				system("%windir%\\system32\\osk.exe");
			}
			else {
				showMyself();
				printf("In order to use the onscreen keyboard, please launch movepoint with administrative privileges. \n");
			}
		}

		/* Abondon keyboard mode for now
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
		*/
	}

	//debug message
	void MoveObserver::selectHandler(byte keyState) {

		if (keyState == 1) {
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
	void MoveObserver::triangleHandler(byte keyState) {

		/******Standard button routines******/
		if (!controllerOn) return;
		trianglePressed = (keyState == 1 ? true : false);
		if (keyState == 1) {
			triangleHandler_FT = fetchFileTime();			//Record time when button is pressed
			updatePos(move->getMove(0)->getMoveData());		//Record position
		}

		/******Custom stuff******************/
		if (scrollMode || zoomMode) {
			if (keyState == 1) {
				zoomMode = true;
			}
			else {
				zoomMode = false;
				snapped = SNAP_NONE;
			}
			//keyPress(VK_CONTROL, keyState);
		}
		else if (mouseMode) {
			mousePress(3, keyState);
		}
		else if (keyboardMode) {
			keyPress(VK_TAB, keyState);
		}

	}

	/*Circle maps to:
	Mouse mode:		middle click
	Keyboard mode:	Print screen
	scroll mode:
	*/
	void MoveObserver::circleHandler(byte keyState) {

		/******Standard button routines******/
		if (!controllerOn) return;
		circlePressed = (keyState == 1 ? true : false);
		if (keyState == 1) circleHandler_FT = fetchFileTime();	//Record time when button is pressed

		/******Custom stuff******************/
		if (scrollMode || desktopMode) {
			if (keyState == 1) {
				desktopMode = true;
			}
			else {
				desktopMode = false;
				snapped = SNAP_NONE;
			}
		}
		else if (mouseMode) {
			mousePress(2, keyState);
		}
		else if (keyboardMode) {
			keyPress(VK_SNAPSHOT, keyState);
		}
	}

	/*Square maps to:
	Mouse mode:
	click:			Win key
	press and hold:	Enable snap mode
	Keyboard mode:	Win key
	scroll mode:	Initiate Alt-Tab. Subsequent click is Tab.
	*/
	void MoveObserver::squareHandler(byte keyState) {

		/******Standard button routines******/
		if (!controllerOn) return;
		squarePressed = (keyState == 1 ? true : false);
		if (keyState == 1) {
			squareHandler_FT = fetchFileTime();					//Record time when button is pressed
			updatePos(move->getMove(0)->getMoveData());			//Record position
		}

		/******Custom stuff******************/
		if (keyState == 1) {
			//In scroll mode, initiate Alt-Tab
			if (scrollMode) {
				appSwitchMode = true;
				keyPress(VK_MENU, 1);
				keyPress(VK_TAB, keyState);
			}
			else {
				//Long press square alone enables snap mode
				snapMode = true;
				myTarget = getTarget();
			}
		}
		else {
			if (appSwitchMode) {
				//if we entered app switching with L button holding first, release tab
				keyPress(VK_TAB, keyState);
			}
			else if (appSwitchMode2) {
				//if we entered app switching with square button holding first, release Alt
				keyPress(VK_MENU, keyState);
				appSwitchMode2 = false;
			}
			else {
				//Long click
				if ((double)(fetchFileTime().QuadPart - squareHandler_FT.QuadPart) > myScrollDelay) {
					/*Let's try not doing anything in a long click in this version
					if (snapped == SNAP_NONE) {
					if (IsWindows10OrGreater) {
					newDesktop();
					}
					else {
					showDesktop();	//if a snap hasn't occured, show desktop
					}
					}
					*/
				}
				//Quick click
				else {
					keyboardClick(VK_LWIN);
				}
			}

			snapMode = false;
			mouseMode = true;
			snapped = SNAP_NONE;

		}
	}

	/*Cross maps to:
	click:					Esc
	long click:				minimize app
	scroll mode long-click:	close app
	*/
	void MoveObserver::crossHandler(byte keyState) {

		/******Standard button routines******/
		if (!controllerOn) return;
		crossPressed = (keyState == 1 ? true : false);
		if (keyState == 1) {
			crossHandler_FT = fetchFileTime();				//Record time when button is pressed
			updatePos(move->getMove(0)->getMoveData());		//Record position
		}

		/******Custom stuff******************/
		if (keyState == 1) {
			if (squarePressed) {
				//If square button is pressed, try closing current desktop (only works in Windows 10)
				killDesktop();
				snapped = SNAP_CLOSE;
			}
		}
		else {
			if ((double)(fetchFileTime().QuadPart - crossHandler_FT.QuadPart) > myScrollDelay) {
				//long press
				if (scrollMode) {
					//in scroll mode close app
					targetClosed = closeTarget();
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
	void MoveObserver::psHandler(byte keyState) {
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
					initCamera();
				}
				else {
					move->closeCamera();
				}
			}
		}
	}

	//Move button on its own triggers left click in mouse mode and enter in keyboard mode. 
	//With L button it initiates drag mode.
	void MoveObserver::moveHandler(byte keyState) {

		/******Standard button routines******/
		if (!controllerOn) return;
		movePressed = (keyState == 1 ? true : false);
		if (keyState == 1) {
			moveHandler_FT = fetchFileTime();				//Record time when button is pressed
			updatePos(move->getMove(0)->getMoveData());		//Record position
		}

		/******Custom stuff******************/
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
			else {
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
	void MoveObserver::lHandler(byte keyState) {

		/******Standard button routines******/
		if (!controllerOn) return;
		if (keyState == 1) {
			lHandler_FT = fetchFileTime();		//Record time when button is pressed
			updatePos(move->getMove(0)->getMoveData());			//Record position
		}

		/******Custom stuff******************/
		if (keyState == 1) {
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
			mouseMode = true;
			if (appSwitchMode) {
				//Exit app-switching (if L is pressed first)
				keyPress(VK_MENU, 0);
				appSwitchMode = false;
			}
			else if (appSwitchMode2) {
				//app-switching if square is pressed first
				keyPress(VK_TAB, keyState);
			}
			else if (crossPressed) {
				//close target if cross button is already pressed
				targetClosed = closeTarget();
			}
			//Quick click show task view in Win10, otherwise maximizes or restores app window. 
			else if ((double)(fetchFileTime().QuadPart - lHandler_FT.QuadPart) <= myScrollDelay) {
				if (IsWindows10OrGreater) {
					showTaskView();			//launch task view
				}
				else {
					MaxRestoreTarget();		//Switch between maximized and regular app window
				}
			}
		}
	}

	//Move arrow keys
	void MoveObserver::moveArrows(int moveId, Move::MoveData data) {
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
	void MoveObserver::moveCursor(int moveId, Move::MoveData data) {

		if (!controllerOn) return;

		float xPosWeight, yPosWeight, distWeight;

		distWeight = min(1, max(0, (1 / log(max(fabs(data.position.x), fabs(data.position.y)) + 2))*max(1,data.position.z/70)));

		if (!tiltMode) {

			xPosWeight = max(min(fabs(data.position.x - avgPos.x) / mouseThreshold * (stableX ? distWeight : 1), 1), 0);
			yPosWeight = max(min(fabs(data.position.y - avgPos.y) / mouseThreshold * (stableY ? distWeight : 1), 1), 0);

			cursorPos.x = round((1 - xPosWeight) * cursorPos.x + xPosWeight * (curPosNorm.x * screenSize.right + screenSize.left));
			cursorPos.y = round((1 - yPosWeight) * cursorPos.y + yPosWeight * (curPosNorm.y * screenSize.bottom + screenSize.top));

			mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE, cursorPos.x, cursorPos.y, 0, 0);

			//SetPhysicalCursorPos doesn't work for handwritting
			//SetPhysicalCursorPos(cursorPos.x, cursorPos.y);
		}
		else {
			moveCursorTilt(moveId, data);
		}
	}

	//Move cursor by tilt subroutine
	void MoveObserver::moveCursorTilt(int moveId, Move::MoveData data) {

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
	void MoveObserver::scroll(int moveId, Move::MoveData data) {
		//TO DO: Should give preference to up-down in scroll mode but left-right in desktop mode

		if (!controllerOn) return;

		//set the desirable movement threshold
		DWORD myWDelta = floor(WHEEL_DELTA * scrollPercent + 0.5);
		float myThreshold = scrollThreshold * scrollPercent;
		if (appSwitchMode) {
			myThreshold = appScrollThreshold;
		}
		else if (snapMode || desktopMode || zoomMode) {
			myThreshold = appScrollThreshold * 1.5;
		}

		//scroll up?
		if (data.position.y > oldPos.y + myThreshold || data.position.y >= ctrlRegion.top) {

			if (data.position.y >= ctrlRegion.top
				&& (double)(fetchFileTime().QuadPart - old_FT.QuadPart) <= autoThreshold / (1 + exp(-3 + data.position.y - ctrlRegion.top)))
				return;	//Do nothing if the request is coming in too fast)

			if (snapMode) {
				if (oldPos.y < ctrlRegion.bottom) {
					desktop(VK_UP);
				}
				else {
					snap(VK_UP);
				}
			}
			else if (zoomMode) {
				zoom(VK_UP);
			}
			else if (desktopMode) {
				desktop(VK_UP);
			}
			else {
				mouse_event(MOUSEEVENTF_WHEEL, 0, 0, 30, 0);
				mouseMode = false;
			}
			updatePos(data);
		}
		//scroll down?
		else if (data.position.y < oldPos.y - myThreshold || data.position.y <= ctrlRegion.bottom) {

			if (data.position.y <= ctrlRegion.bottom
				&& (double)(fetchFileTime().QuadPart - old_FT.QuadPart) <= autoThreshold / (1 + exp(-3 + ctrlRegion.bottom - data.position.y)))
				return;	//Do nothing if the request is coming in too fast)

			if (snapMode) {
				if (oldPos.y > ctrlRegion.top) {
					desktop(VK_DOWN);
				}
				else {
					snap(VK_DOWN);
				}
			}
			else if (zoomMode) {
				zoom(VK_DOWN);
			}
			else if (desktopMode) {
				desktop(VK_DOWN);
			}
			else {
				mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -30, 0);
				mouseMode = false;
			}
			updatePos(data);
		}
		//scroll left?
		else if (data.position.x < oldPos.x - myThreshold || data.position.x <= ctrlRegion.left) {
			if (snapMode) {

				if (data.position.x <= ctrlRegion.left
					&& (double)(fetchFileTime().QuadPart - old_FT.QuadPart) <= autoThreshold / (1 + exp(-3 + ctrlRegion.left - data.position.x)))
					return;	//Do nothing if the request is coming in too fast)				

				if (oldPos.x > ctrlRegion.right) {
					desktop(VK_LEFT);
				}
				else {
					snap(VK_LEFT);
				}
			}
			else if (zoomMode) {
				zoom(VK_LEFT);
			}
			else if (desktopMode) {
				desktop(VK_LEFT);
			}
			else {
				mouse_event(MOUSEEVENTF_HWHEEL, 0, 0, -30, 0);
				mouseMode = false;
			}
			updatePos(data);
		}
		//scroll right?
		else if (data.position.x > oldPos.x + myThreshold || data.position.x >= ctrlRegion.right) {

			if (data.position.x >= ctrlRegion.right
				&& (double)(fetchFileTime().QuadPart - old_FT.QuadPart) <= autoThreshold / (1 + exp(-3 + data.position.x - ctrlRegion.right)))
				return;	//Do nothing if the request is coming in too fast)		

			if (snapMode) {
				if (oldPos.x < ctrlRegion.left) {
					desktop(VK_RIGHT);
				}
				else {
					snap(VK_RIGHT);
				}
			}
			else if (zoomMode) {
				zoom(VK_RIGHT);
			}
			else if (desktopMode) {
				desktop(VK_RIGHT);
			}
			else {
				mouse_event(MOUSEEVENTF_HWHEEL, 0, 0, 30, 0);
				mouseMode = false;
			}
			updatePos(data);
		}
	}

	void MoveObserver::snap(int keyCode) {

		//Have we already snapped in this direction?
		switch (keyCode) {
		case VK_UP:
			if (snapped == SNAP_UP) return;		//This prevent multiple actions in one movement
			snapped = SNAP_UP;
			printf("%d Snapping up. \n", ++curConsoleLine);
			break;
		case VK_DOWN:
			if (snapped == SNAP_DOWN) return;
			snapped = SNAP_DOWN;
			printf("%d Snapping down. \n", ++curConsoleLine);
			break;
		case VK_LEFT:
			if (snapped == SNAP_LEFT) return;
			snapped = SNAP_LEFT;
			printf("%d Snapping left. \n", ++curConsoleLine);
			break;
		case VK_RIGHT:
			if (snapped == SNAP_RIGHT) return;
			snapped = SNAP_RIGHT;
			printf("%d Snapping right. \n", ++curConsoleLine);
			break;
		}

		SetForegroundWindow(myTarget);	//target was acquired with square button press
		keyPress(VK_LWIN, 1);
		keyboardClick(keyCode);
		keyPress(VK_LWIN, 0);

	}

	void MoveObserver::zoom(int keyCode) {
		switch (keyCode) {
		case VK_UP:
			keyPress(VK_CONTROL, 1);
			mouse_event(MOUSEEVENTF_WHEEL, 0, 0, WHEEL_DELTA, 0);
			keyPress(VK_CONTROL, 0);
			printf("%d Zooming up. \n", ++curConsoleLine);
			break;
		case VK_DOWN:
			keyPress(VK_CONTROL, 1);
			mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -1 * WHEEL_DELTA, 0);
			keyPress(VK_CONTROL, 0);
			printf("%d Zooming down. \n", ++curConsoleLine);
			break;
		case VK_LEFT:
			if (snapped == SNAP_LEFT) return;		//This prevents multiple actions in one movement
			snapped = SNAP_LEFT;
			keyPress(VK_MENU, 1);
			keyboardClick(keyCode);
			keyPress(VK_MENU, 0);
			printf("%d Back. \n", ++curConsoleLine);
			break;
		case VK_RIGHT:
			if (snapped == SNAP_RIGHT) return;
			snapped = SNAP_RIGHT;
			keyPress(VK_MENU, 1);
			keyboardClick(keyCode);
			keyPress(VK_MENU, 0);
			printf("%d Forward. \n", ++curConsoleLine);
			break;
		}
	}

	void MoveObserver::desktop(int keyCode) {
		switch (keyCode) {
		case VK_UP:
			if (snapped == SNAP_UP) return;			//This prevents multiple actions in one movement
			snapped = SNAP_UP;
			newDesktop();
			break;

		case VK_DOWN:
			if (snapped == SNAP_DOWN) return;
			snapped = SNAP_DOWN;
			if (IsWindows10OrGreater) {
				killDesktop();
			}
			else {
				showDesktop();
			}
			break;

		case VK_LEFT:
			if (snapped == SNAP_LEFT) return;
			snapped = SNAP_LEFT;
			nextDesktop();
			break;

		case VK_RIGHT:
			if (snapped == SNAP_RIGHT) return;
			snapped = SNAP_RIGHT;
			prevDesktop();
			break;
		}
	}


	//Drag subroutine
	void MoveObserver::dragWindow(int moveId) {

		if (!controllerOn) return;

		BOOL retVal;
		POINT myCurPPos;

		GetPhysicalCursorPos(&myCurPPos);
		retVal = MoveWindow(myTarget, myCurPPos.x - winCurDiff.x, myCurPPos.y - winCurDiff.y, tSize.x, tSize.y, true);
	}

	//Get handle to the window below cursor and send it to foreground
	HWND MoveObserver::getTarget() {
		return win_actions::getTarget(cursorPos);
	}

	void MoveObserver::focusMyTarget(HWND inTarget) {
		myTarget = (inTarget != NULL ? inTarget : getTarget());
		win_actions::focusMyTarget(myTarget);
	}

	void MoveObserver::getDragTarget() {

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

			POINT myCurPPos;
			GetPhysicalCursorPos(&myCurPPos);

			winCurDiff.x = myCurPPos.x - tRect.left;							//This difference is kept while dragging
			winCurDiff.y = myCurPPos.y - tRect.top;

			//printf("HWND:%d %d %d %d %d %d %d \n", myTarget, tRect.left, tRect.top, tSize.x, tSize.y, winCurDiff.x, winCurDiff.y);
		}

	}

	BOOL MoveObserver::closeTarget(HWND cTarget) {
		cTarget = (cTarget == NULL ? getTarget() : cTarget);
		return win_actions::closeTarget(cTarget);
	}

	BOOL MoveObserver::minimizeTarget(HWND cTarget) {
		cTarget = (cTarget == NULL ? getTarget() : cTarget);
		return win_actions::minimizeTarget(cTarget);
	}

	BOOL MoveObserver::maximizeTarget(HWND cTarget) {
		cTarget = (cTarget == NULL ? getTarget() : cTarget);
		return win_actions::maximizeTarget(cTarget);
	}

	BOOL MoveObserver::restoreTarget(HWND cTarget) {
		cTarget = (cTarget == NULL ? getTarget() : cTarget);
		return win_actions::restoreTarget(cTarget);
	}

	BOOL MoveObserver::MaxRestoreTarget() {
		return win_actions::MaxRestoreTarget(getTarget());
	}

	void MoveObserver::checkAdminRights() {
		if (!amIAdmin()) {
			showMyself(10000);
			printf("***\n");
			printf("Movepoint is launched without administrative privileges. Controls will not work when the cursor is over applications with such privileges.*** \n");
			printf("Hiding console window in 10 seconds. \n");
			printf("***\n\n");
		}
	}

	void MoveObserver::calibrateRegion() {

		showMyself();

		printf("Orientation calibrated.\n");
		printf("Click the move button to continue calibration of screen area. Click X to quit.\n");
		calibrationMode = 1;
	}

	void MoveObserver::showCalibrationSteps() {
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

			//Calculate thresholds
			/*//Since we now use raw data position for thresholds this is no longer necessary:
			scrollThreshold = (ctrlRegion_d.top - ctrlRegion_d.bottom)/(ctrlRegion.top - ctrlRegion.bottom) * scrollThreshold_d;
			appScrollThreshold = (ctrlRegion_d.top - ctrlRegion_d.bottom) / (ctrlRegion.top - ctrlRegion.bottom) * appScrollThreshold_d;
			mouseThreshold = (ctrlRegion_d.top - ctrlRegion_d.bottom) / (ctrlRegion.top - ctrlRegion.bottom) * mouseThreshold_d;
			*/

			saveSettings();
			printf("Calibration completed: Top:%.2f Bottom:%.2f Left:%.2f Right:%.2f \n", ctrlRegion.top, ctrlRegion.bottom, ctrlRegion.left, ctrlRegion.bottom);
			printf("New thresholds: %.4f %.4f %.4f \n", scrollThreshold, appScrollThreshold, mouseThreshold);

			if (fabs(ctrlRegion.top - ctrlRegion.bottom) < 20) {
				printf("WARNING: Cursor might move very fast due to vertical distance being less than 20. A value of around 30 is recommended.");
			}
			if (fabs(ctrlRegion.right - ctrlRegion.left) < 30) {
				printf("WARNING: Cursor might move very fast due to horizontal distance being less than 30. A value of around 60 is recommended.");
			}
			break;
		}
	}

	void MoveObserver::calibrateRecordPos(Move::MoveData data) {
		switch (calibrationMode) {
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

	void MoveObserver::takeInitOrient(Move::MoveData data) {
		avgOrient.w = data.orientation.w;
		avgOrient.v.x = data.orientation.v.x;
		avgOrient.v.y = data.orientation.v.y;
		avgOrient.v.z = data.orientation.v.z;
		takeInitReading = false;
	}

	void MoveObserver::restoreDefaults() {
		ctrlRegion = ctrlRegion_d;
		scrollPercent = scrollPercent_d;
		scrollThreshold = scrollThreshold_d;
		appScrollThreshold = appScrollThreshold_d;
		mouseThreshold = mouseThreshold_d;
		curPosWeight = curPosWeight_d;
		moveDelay = moveDelay_d;

		calSettings();
	}

	void MoveObserver::calSettings() {
		if (mouseThreshold <= 0) mouseThreshold = 0.000001;
		myMoveDelay = moveDelay * 10000;						//movement detection delay in nanoseconds
		myScrollDelay = max((moveDelay + 100),300) * 10000;				//scroll needs slightly more delay
	}

	void MoveObserver::saveSettings() {
		LONG retVal1, retVal2;

		//If system-wide settings does not exist, try writing to it
		if (hasSystemSettings != ERROR_SUCCESS)
			retVal1 = saveSettingsReal(HKEY_LOCAL_MACHINE);

		//Write to current user settings
		retVal2 = saveSettingsReal(HKEY_CURRENT_USER);

		printf("Save Settings. Return value: %d %d \n\n", retVal1, retVal2);

	}

	LONG MoveObserver::saveSettingsReal(HKEY inKey) {

		LONG retVal1, retVal2, retVal3;
		HKEY hKey;
		DWORD dwDisp;

		retVal1 = RegCreateKeyEx(inKey, TEXT("SOFTWARE\\MOVEpoint"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisp);

		retVal2 = writeFloatToReg(hKey, TEXT("scrollPercent"), scrollPercent);
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

		return max(max(retVal1, retVal2), retVal3);

	}

	void MoveObserver::readSettings() {

		LONG retVal1, retVal2;

		//First try reading settings from system-wide settings before reading from current user
		hasSystemSettings = readSettingsReal(HKEY_LOCAL_MACHINE);
		retVal2 = readSettingsReal(HKEY_CURRENT_USER);

		printf("Read Settings: %d %d %.3f %.3f %.3f %.3f %d %.3f %.3f %.3f %.3f \n\n",
			hasSystemSettings, retVal2,
			scrollThreshold, appScrollThreshold, mouseThreshold, curPosWeight, moveDelay,
			ctrlRegion.top, ctrlRegion.bottom, ctrlRegion.left, ctrlRegion.right);
	}

	LONG MoveObserver::readSettingsReal(HKEY inKey) {

		LONG retVal1, retVal2, retVal3;
		HKEY hKey;
		DWORD dwDisp;

		retVal1 = RegCreateKeyEx(inKey, TEXT("SOFTWARE\\MOVEpoint"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ, NULL, &hKey, &dwDisp);

		retVal2 = 5;
		retVal2 = min(readFloatFromReg(hKey, TEXT("scrollPercent"), &scrollPercent), retVal2);
		if (scrollPercent < 0.01) scrollPercent = scrollPercent_d;												//no negative value for scrollPercent
		retVal2 = min(readFloatFromReg(hKey, TEXT("scrollThreshold"), &scrollThreshold), retVal2);
		retVal2 = min(readFloatFromReg(hKey, TEXT("appScrollThreshold"), &appScrollThreshold), retVal2);
		retVal2 = min(readFloatFromReg(hKey, TEXT("mouseThreshold"), &mouseThreshold), retVal2);
		retVal2 = min(readFloatFromReg(hKey, TEXT("curPosWeight"), &curPosWeight), retVal2);
		retVal2 = min(readFloatFromReg(hKey, TEXT("ctrlRegionT"), &ctrlRegion.top), retVal2);
		retVal2 = min(readFloatFromReg(hKey, TEXT("ctrlRegionB"), &ctrlRegion.bottom), retVal2);
		retVal2 = min(readFloatFromReg(hKey, TEXT("ctrlRegionL"), &ctrlRegion.left), retVal2);
		retVal2 = min(readFloatFromReg(hKey, TEXT("ctrlRegionR"), &ctrlRegion.right), retVal2);

		retVal2 = min(readDWORDFromReg(hKey, TEXT("moveDelay"), (DWORD*)&moveDelay), retVal2);

		retVal3 = RegCloseKey(hKey);

		calSettings();

		return max(max(retVal1, retVal2), retVal3);
	}

	//Default settings
	void MoveObserver::initValues() {
		screenSize.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
		screenSize.right = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		screenSize.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
		screenSize.bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		screenWHratio = abs(screenSize.left - screenSize.right) / abs(screenSize.bottom - screenSize.top);

		screenSize.left = round(screenSize.left * 65535 / GetSystemMetrics(SM_CXSCREEN));
		screenSize.right = round(screenSize.right * 65535 / GetSystemMetrics(SM_CXSCREEN));
		screenSize.top = round(screenSize.top * 65535 / GetSystemMetrics(SM_CYSCREEN));
		screenSize.bottom = round(screenSize.bottom * 65535 / GetSystemMetrics(SM_CYSCREEN));

		oldPos.x = -99999;
		oldPos.y = -99999;
		oldPos.z = -99999;
		curPosNorm.x = 0;
		curPosNorm.y = 0;
		curPosNorm.z = 0;
		avgPos.x = 0;
		avgPos.y = 0;
		avgPos.z = 0;

		ctrlRegion_d.left = -30;
		ctrlRegion_d.right = 30;
		ctrlRegion_d.top = max(10, min(20, 30 / screenWHratio));			//default height between 1/3 to 2/3 of width
		ctrlRegion_d.bottom = max(-10, min(-20, -30 / screenWHratio));
	}

	//Restore size and position of console window
	BOOL MoveObserver::showMyself(int showTime) {
		myHWND = (myHWND == NULL ? GetConsoleWindow() : myHWND);

		//Do we need to hide the window after a specific interval?
		if (showTime > 0) {
			unsigned int thread_id = 0;
			_beginthreadex(NULL, 0, hideMyself_T, &showTime, 0, &thread_id);
		}
		return setShowCMD(myHWND, &myWPInfo, SW_RESTORE);
	}

	//Hide console window
	BOOL MoveObserver::hideMyself() {
		myHWND = (myHWND == NULL ? GetConsoleWindow() : myHWND);
		return setShowCMD(myHWND, &myWPInfo, SW_HIDE);
	}

	unsigned int __stdcall MoveObserver::hideMyself_T(void *p_thread_data)
	{
		int showTime = 0;
		showTime = *static_cast<int*>(p_thread_data);

		Sleep(showTime);

		//Do actual stuff
		HWND hwHWND = GetConsoleWindow();
		WINDOWPLACEMENT hmWPInfo;
		if (GetWindowPlacement(hwHWND, &hmWPInfo)) {		//Get window placement info
			hmWPInfo.showCmd = SW_HIDE;						//set showCMD value
			SetWindowPlacement(hwHWND, &hmWPInfo);			//Set the new window placement info
		}

		return 0;
	}

	//Get the handle to the console window and hide it
	void MoveObserver::setupConsole() {

		//always on top
		myHWND = (myHWND == NULL ? GetConsoleWindow() : myHWND);
		SetWindowPos(myHWND, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

		//Only show console window in debug build
#ifdef NDEBUG
		hideMyself();
#endif
		printf("Click [SELECT] to show console. Long click to hide. Close console to stop the controller. \n\n");
	}

	//Check for the presence of an PS Eye camera
	void MoveObserver::initCamera() {
		if (!move->initCamera(numMoves)) {
			showMyself();
			printf("No PS Eye Camera found. Closing in 5 seconds. \n\n");
			Sleep(5000);
			closeTarget(myHWND);
			//tiltMode = true;			//It is possible to use just the orientation data to control the pointer

		}
	}

	//Print a debug message
	void MoveObserver::printDebugMessage(int moveId, Move::MoveData data) {

		POINT debugCurPos;

		printf("MOVE id:%d   pos:%.2f %.2f %.2f   ori:%.2f %.2f %.2f %.2f   trigger:%d\n",
			moveId,
			data.position.x, data.position.y, data.position.z,
			data.orientation.w, data.orientation.v.x, data.orientation.v.y, data.orientation.v.z,
			data.trigger);
		printf("AVG NORMALIZED pos:%.2f %.2f %.2f   ori:%.2f %.2f %.2f %.2f\n",
			avgPos.x, avgPos.y, avgPos.z,
			avgOrient.w, avgOrient.v.x, avgOrient.v.y, avgOrient.v.z);
		if (GetPhysicalCursorPos(&debugCurPos)) {
			printf("CURSOR pos:%d %d\n", debugCurPos.x, debugCurPos.y);
		}
		printf("SCREEN left:%d  right:%d top:%d bottom:%d\n",
			screenSize.left, screenSize.right, screenSize.top, screenSize.bottom);
		printf("\n");
		printPos = false;
	}
