namespace movepoint {

	//Default values
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

	struct RECTf
	{
		float top, bottom, left, right;
	};

}

