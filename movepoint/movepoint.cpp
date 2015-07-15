// Based on MoveFrameworkSDK

#include "stdafx.h"
#include "MoveObserver.h""

//Move class
MoveObserver* observer;

bool duplicateExist() {
	bool AlreadyRunning;

	HANDLE hMutexOneInstance = CreateMutex(NULL, TRUE,_T("movepoint-124712857"));

	AlreadyRunning = (GetLastError() == ERROR_ALREADY_EXISTS);

	if (hMutexOneInstance != NULL)
	{
		ReleaseMutex(hMutexOneInstance);
	}

	return AlreadyRunning;

}


int main(int argc, char* argv[])
{
	if (!duplicateExist()) {
		//only run if there isn't a duplicate
		observer = new MoveObserver();

		int count;
		for (count = 0; count < argc; count++) {
			
			std::string curArg(argv[count]);
					
			if (curArg == "-sx") {
				observer->extraStableX(true);
				printf("Command line settings: Extra stable X \n");
			}
			else if (curArg == "-sy") {
				observer->extraStableY(true);
				printf("Command line settings: Extra stable Y \n");
			}
		}


		getchar();

	}

	return 0;
}

