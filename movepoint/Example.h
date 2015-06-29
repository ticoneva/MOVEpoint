#include "stdafx.h"

class MoveObserver : public Move::IMoveObserver
{

public:
	void moveUpdated(int moveId, Move::MoveData data){}
	void moveKeyPressed(int moveId, Move::MoveButton button){}
	void moveKeyReleased(int moveId, Move::MoveButton button){}
	void moveConnected(int numAllMoves){}
	void moveNotify(int moveId, Move::MoveMessage message){}

	void navUpdated(int navId, Move::NavData data){}
	void navKeyPressed(int navId, Move::MoveButton button){}
	void navKeyReleased(int navId, Move::MoveButton button){}

private:
	void moveKeyProc(Move::MoveButton keyCode, int KeyState)
};
