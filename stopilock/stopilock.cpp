// stopilock.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#define STOPEVENTNAME L"STOPILOCK"

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hStopIlock=NULL;
	if(argc==1)
		 hStopIlock = CreateEvent(NULL, FALSE, FALSE, STOPEVENTNAME);
	else if (argc==2){
		TCHAR eventName[64];
		wsprintf(eventName, L"%s", argv[1]);
		hStopIlock = CreateEvent(NULL, FALSE, FALSE, eventName);
	}

	DEBUGMSG(1, (L"stopilock: CreateEvent returned: %i\r\n", GetLastError()));

	if(hStopIlock!=NULL){
		if(SetEvent(hStopIlock))
			DEBUGMSG(1, (L"stopilock: SetEvent OK, returned: %i\r\n", GetLastError()));
		else
			DEBUGMSG(1, (L"stopilock: SetEvent FALSE, returned: %i\r\n", GetLastError()));
	}

	return 0;
}

