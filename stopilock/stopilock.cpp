// stopilock.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#define STOPEVENTNAME L"STOPILOCK"

int _tmain(int argc, _TCHAR* argv[])
{
	HANDLE hStopIlock = CreateEvent(NULL, FALSE, FALSE, STOPEVENTNAME);
	if(hStopIlock!=NULL)
		SetEvent(hStopIlock);
	return 0;
}

