#pragma once
#include "stdafx.h"

#include <math.h>
#include <windows.h>
#include <WinUser.h>
#include <process.h>

#include <VersionHelpers.h>
#include <Dbt.h>
#include "resource.h"

#include "movepoint.h"
#include "win_actions.h"

using namespace movepoint;
using namespace win_actions;

//Default values
const float scrollPercent_d = 0.5;			//How finely do we divide scrolling. 1 = one wheel click per scroll action.
const float scrollThreshold_d = 1;			//Threshold of movement before scrolling begins
const float appScrollThreshold_d = 5;		//Threshold of movement before scrolling begins in app-switching and zooming
const float mouseThreshold_d = 0.2;			//Threshold of movement before cursor moves 1:1 with handset. For reducing cursor jitter.
const float curPosWeight_d = 0.4;			//Weight of current handset position in calculating moving average. For reducing cursor jitter.
const int moveDelay_d = 200;				//Time to wait before executing press event in milliseconds. For reducing cursor shake while pressing button.

enum snapStatus
{
	SNAP_NONE = 0,
	SNAP_UP = 1,
	SNAP_DOWN = 2,
	SNAP_LEFT = 3,
	SNAP_RIGHT = 4,
	SNAP_FAILED = 5,
	SNAP_CLOSE = 6
};

class MoveObserver : public Move::IMoveObserver
{
	//variables and objects
	Move::IMoveManager* move;
	int numMoves;

	//Settings
	float scrollPercent = 0.5;
	float scrollThreshold = 1;
	float appScrollThreshold = 5;
	float mouseThreshold = 0.2;
	float curPosWeight = 0.4;
	int moveDelay = 200;
	int autoThreshold = 250000;
	int myMoveDelay, myScrollDelay;

	//Position
	RECT screenSize;
	float screenWHratio;
	RECTf ctrlRegion, ctrlRegion_d;
	POINT cursorPos, winCurDiff;
	Move::Vec3 oldPos, curPosNorm, avgPos;
	Move::Quat avgOrient;

	//Timers - TODO: switch to std::chrono 
	ULARGE_INTEGER	cur_FT, old_FT, 
		lHandler_FT, moveHandler_FT, keyboardClick_FT,
		squareHandler_FT, crossHandler_FT, 
		triangleHandler_FT, circleHandler_FT,
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
	bool desktopMode = false;

	//Status
	snapStatus snapped = SNAP_NONE;
	bool targetClosed = false;

	bool squarePressed = false;
	bool crossPressed = false;
	bool trianglePressed = false;
	bool circlePressed = false;
	bool movePressed = false;
	bool LPressed = false;

	bool printPos = false;
	bool takeInitReading = true;

	long hasSystemSettings = false;
	byte calibrationMode = 0;

	//For drag operations
	HWND myTarget = 0;
	RECT tRect;
	POINT tSize;

	//For console window
	HWND myHWND = 0;
	WINDOWPLACEMENT myWPInfo;
	int curConsoleLine = 0;

public:
	MoveObserver();
	int getNumMoves();
	void pairNewMoves();
	void initializeSystem();
	void terminateSystem();
	void moveKeyPressed(int moveId, Move::MoveButton keyCode);
	void moveKeyReleased(int moveId, Move::MoveButton keyCode);
	void moveUpdated(int moveId, Move::MoveData data);
	void navKeyPressed(int navId, Move::MoveButton keyCode);
	void navKeyReleased(int navId, Move::MoveButton keyCode);
	void navUpdated(int navId, Move::NavData data);

private:
	void updatePos(Move::MoveData data);
	void moveKeyProc(Move::MoveButton keyCode, byte keyState);

	void startHandler(byte keyState);
	void selectHandler(byte keyState);
	void triangleHandler(byte keyState);
	void circleHandler(byte keyState);
	void squareHandler(byte keyState);
	void crossHandler(byte keyState);
	void psHandler(byte keyState);
	void moveHandler(byte keyState);
	void lHandler(byte keyState);

	void moveArrows(int moveId, Move::MoveData data);
	void moveCursor(int moveId, Move::MoveData data);
	void moveCursorTilt(int moveId, Move::MoveData data);

	void scroll(int moveId, Move::MoveData data);
	void snap(int keyCode);
	void zoom(int keyCode);
	void desktop(int keyCode);
	void dragWindow(int moveId = 0);

	void calibrateRegion();
	void showCalibrationSteps();
	void calibrateRecordPos(Move::MoveData data);
	void takeInitOrient(Move::MoveData data);
	void restoreDefaults();
	void calSettings();
	void saveSettings();
	LONG saveSettingsReal(HKEY inKey);
	void readSettings();
	LONG readSettingsReal(HKEY inKey);
	void initValues();
	BOOL showMyself(int showTime = -1);
	BOOL hideMyself();
	static unsigned int __stdcall hideMyself_T(void *p_thread_data);
	void setupConsole();
	void initCamera();
	void printDebugMessage(int moveId, Move::MoveData data);
	void checkAdminRights();
	
	HWND getTarget();
	void focusMyTarget(HWND inTarget = NULL);
	void getDragTarget();
	BOOL closeTarget(HWND cTarget = NULL);
	BOOL minimizeTarget(HWND cTarget = NULL);
	BOOL maximizeTarget(HWND cTarget = NULL);
	BOOL restoreTarget(HWND cTarget = NULL);
	BOOL MaxRestoreTarget();

};
