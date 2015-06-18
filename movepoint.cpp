// Based on MoveFrameworkSDK
//

#include "stdafx.h"
#include "movepoint.h";

class MoveObserver : public Move::IMoveObserver
{
	//Default values
	float scrollThreshold = 0.01;		//Threshold of movement before scrolling begins
	float mouseThreshold = 0.005;		//Threshold of movement before cursor moves 1:1 with handset. For reducing cursor jitter.
	float curPosWeight = 0.4;			//Weight of current handset position in calculating moving average. For reducing cursor jitter.
	int moveDelay = 200;				//Time to wait before executing press event in milliseconds. For reducing cursor shake while pressing button.
	
	//variables and objects
	Move::IMoveManager* move;
	int numMoves, myMoveDelay, myScrollDelay;
	float appScrollThreshold;

	RECT screenSize;
	RECTf ctrlRegion;
	POINT cursorPos, winCurDiff;
	Move::Vec3 oldPos, curPos, avgPos;
	Move::Quat myOrient;

	ULARGE_INTEGER lHandler_FT, mouseClick_FT, crossHandler_FT, psClick_FT, keyboardClick_FT;
		
	bool controllerOn = true;
	bool mouseMode = true;
	bool scrollMode = false;
	bool dragMode = false;
	bool keyboardMode = false; 
	bool appSwitchMode = false;
	bool zoomMode = false;
	bool printPos = false;
	bool takeInitReading = true;

	byte calibrationMode = 0;

	//For drag operations
	HWND myTarget;
	RECT tRect;
	POINT tSize;


public:
	MoveObserver()
	{
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
		
		if (mouseThreshold <= 0) mouseThreshold = 0.000001;
		myMoveDelay = moveDelay * 10000;			//movement detection delay in nanoseconds
		myScrollDelay = (moveDelay + 100) * 10000;	//scroll needs slightly more delay
		appScrollThreshold = scrollThreshold * 10;	//app-switching scroll needs to be slower

	}

	void moveKeyPressed(int moveId, Move::MoveButton keyCode)
	{
		//printf("MOVE id:%d   button pressed: %d\n", moveId, (int)keyCode);
		moveKeyProc(keyCode, 1);
	
	}

	void moveKeyReleased(int moveId, Move::MoveButton keyCode)
	{
		//printf("MOVE id:%d   button released: %d\n", moveId, (int)keyCode);
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
			//debug message
				if (keyState == 1) printPos = !printPos;
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

	//Triangle maps to right click
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

	//Circle is middle click
	void circleHandler(byte keyState) {

		if (!controllerOn) return;

		if (mouseMode) {
			mousePress(2, keyState);
		}
		else if (keyboardMode) {
			keyPress(VK_SNAPSHOT, keyState);
		}
	}

	//Square maps to Alt-Tab if Button L is not pressed.
	//Maps to Win-key otherwise.
	void squareHandler(byte keyState) {

		if (!controllerOn) return; 
			
		if (scrollMode) {
			if (keyState == 1) {
				keyPress(VK_MENU, 1);
				appSwitchMode = true;
			}
			keyPress(VK_TAB, keyState);
		}
		else {
			keyPress(VK_LWIN, keyState);
		}	
	}

	//Cross button handler. Maps to long press to close app.
	void crossHandler(byte keyState) {

		if (!controllerOn) return;

		if (keyState == 1){
			//Record time when button is pressed
			crossHandler_FT = fetchFileTime();
		}
		else {
			if ((double)(fetchFileTime().QuadPart - crossHandler_FT.QuadPart) <= myScrollDelay) {
				//Quick click is interpreted as Esc
				keyPress(VK_ESCAPE, 1);
				keyPress(VK_ESCAPE, 0);
			}
			else {
				closeTarget();
			}
		}
	}

	/* L button handler.
	L button on its own triggers middle click. 
	Press and hold triggers scrolling.
	With Move button it initiates drag mode. */
	void lHandler(byte keyState) {

		if (!controllerOn) return;

		if (keyState == 1){
			//Record time when button is pressed
			scrollMode = true;
			mouseMode = false;
			lHandler_FT = fetchFileTime();

			getTarget();
		}
		else {
			scrollMode = false;
			if (appSwitchMode) {
			//Exit app-switching
				keyPress(VK_MENU, 0);
				appSwitchMode = false;
				mouseMode = true;
			}
			else if ((double)(fetchFileTime().QuadPart - lHandler_FT.QuadPart) <= myScrollDelay) {
			//Quick click is interpreted as a middle click
				mouseMode = true;
				mouseClick(2);
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
			getTarget();
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

		float myThreshold = (appSwitchMode ? appScrollThreshold : scrollThreshold);

		//scroll up?
		if (curPos.y < oldPos.y - myThreshold || curPos.y == 0) {
			mouse_event(MOUSEEVENTF_WHEEL, 0, 0, WHEEL_DELTA, 0);
			//printf("MOVE id:%d   scroll up %.3f %.3f %.3f \n", moveId, oldPos.y, curPos.y);
			updatePos();
		}
		//scroll down?
		else if (curPos.y > oldPos.y + myThreshold || curPos.y == 1) {
			mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -1 * WHEEL_DELTA, 0);
			//printf("MOVE id:%d   scroll down %.3f %.3f %.3f \n", moveId, oldPos.y, curPos.y);
			updatePos();
		}

		//scroll left?
		if (curPos.x < oldPos.x - myThreshold || curPos.x == 0) {
			mouse_event(MOUSEEVENTF_HWHEEL, 0, 0, -1 * WHEEL_DELTA, 0);
			//printf("MOVE id:%d   scroll left %.3f %.3f %.3f \n", moveId, oldPos.x, curPos.x);
			updatePos();
		}
		//scroll right?
		else if (curPos.x > oldPos.x + myThreshold || curPos.x == 1) {
			mouse_event(MOUSEEVENTF_HWHEEL, 0, 0, WHEEL_DELTA, 0);
			//printf("MOVE id:%d   scroll right  %.3f %.3f %.3f \n", moveId, oldPos.x, curPos.x);
			updatePos();
		}
	}

	//Drag subroutine
	void dragWindow(int moveId) {

		if (!controllerOn) return;

		bool retVal;

		retVal = MoveWindow(myTarget, cursorPos.x-winCurDiff.x, cursorPos.y - winCurDiff.y, tSize.x, tSize.y, true);
		printf("HWND:%d retVal:%d Pos:%d %d \n", myTarget, retVal, cursorPos.x - winCurDiff.x, cursorPos.y - winCurDiff.y);
	}

	//Get handle to the window below cursor and send it to foreground
	void getTarget() {

		if (GetPhysicalCursorPos(&cursorPos)) {

			//RealChildWindowFromPoint gives us the best guess as to which window is relevant
			myTarget = RealChildWindowFromPoint(GetDesktopWindow(),cursorPos);	
			SetForegroundWindow(myTarget);
			GetWindowRect(myTarget, &tRect);
			tSize.x = (tRect.right - tRect.left);
			tSize.y = (tRect.bottom - tRect.top);

			winCurDiff.x = cursorPos.x - tRect.left;
			winCurDiff.y = cursorPos.y - tRect.top;

			//printf("HWND:%d %d %d %d %d %d %d \n", myTarget, tRect.left, tRect.top, tSize.x, tSize.y, winCurDiff.x, winCurDiff.y);
		}

	}

	void closeTarget() {
		HWND cTarget;
		
		if (GetPhysicalCursorPos(&cursorPos)) {
			cTarget = RealChildWindowFromPoint(GetDesktopWindow(), cursorPos);
			//Cannot use SendMessage because the thread will stop processing signals from controller
			PostMessage(cTarget, WM_CLOSE, 0, 0);	
		}

	}

	void calibrateRegion() {
		
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
			printf("Calibration completed. Top:%.2f Bottom:%.2f Left:%.2f Right:%.2f \n", ctrlRegion.top, ctrlRegion.bottom, ctrlRegion.left, ctrlRegion.bottom);
			break;
		}
	}

};

int main(int argc, char* argv[])
{
	MoveObserver* observer = new MoveObserver();

	getchar();
	return 0;
}

