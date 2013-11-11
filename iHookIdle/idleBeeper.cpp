//idleBeeper.cpp

#include "stdafx.h"

#include <itc50.h>
#pragma comment (lib, "itc50.lib")

#include "idleBeeper.h"

#include "dlg_info.h"
//extern BOOL bInfoDlgVisible;	//show/hide DlgInfo
extern DWORD regValEnableInfo;

extern int LEDid;		//which LED to use
extern void LedOn(int id, int onoff); //onoff=0 LED is off, onoff=1 LED is on

//events to control beeper and idle thread
//	Idle thread control
HANDLE	h_resetIdleThread=NULL;	//event handle
HANDLE	h_stopIdleThread=NULL;	//event handle
HANDLE	h_IdleThread=NULL;		//thread handle
DWORD	threadIdleID=0;			//thread ID

//power watch thread
HANDLE	h_stopPowerThread=NULL;	//event handle to stop power thread
HANDLE	h_PowerThread=NULL;		//powerthread handle
DWORD	threadPowerID=0;		//powerthread ID

//	Beeper thread control
HANDLE	h_stopBeeperThread=NULL;	//event handle
HANDLE	h_BeeperThread=NULL;		//thread handle
DWORD	threadBeeperID=0;			//thread ID

//HANDLE hBeeperThread=NULL;

//forward declarations
DWORD WINAPI beeperThread(LPVOID lParam);
DWORD WINAPI idleThread(LPVOID lParam);

void logTime(){
	TCHAR lpTimeStr[32];
	wsprintf(lpTimeStr, L"");
	//Read the system time
	HRESULT res = GetTimeFormat(LOCALE_SYSTEM_DEFAULT,
							TIME_FORCE24HOURFORMAT,
							NULL,
							L"hh:mm:ss",
							lpTimeStr,
							sizeof (lpTimeStr ) * sizeof(TCHAR));
	if(res>0)
		DEBUGMSG(1, (L"%s ", lpTimeStr));
}

//
BOOL isACpower(){
#if DEBUG
	return FALSE;
#endif
	DWORD lpdwLineStatus=0;
	DWORD lpdwBatteryStatus=0;
	DWORD lpdwBackupStatus=0;
	UINT puFuelGauge=0;
	HRESULT hr = ITCPowerStatus (&lpdwLineStatus,&lpdwBatteryStatus,&lpdwBackupStatus,&puFuelGauge);
	if(hr==ITC_SUCCESS){
		if(lpdwLineStatus==ITC_ACLINE_CONNECTED)
			return TRUE;
		else
			return FALSE;
	}
	else
		return FALSE;
}

///start a thread to watch for external power changes
///should reset idle thread to avoid alarm beep
DWORD WINAPI powerWatchThread(LPVOID lParam){
	HANDLE waitHandles[1];
	waitHandles[0]=h_stopPowerThread;
	BOOL bStopThread=FALSE;
	DWORD dwTimeout=(DWORD) lParam;
	DEBUGMSG(1,(L"powerWatchThread starting with %i\n", dwTimeout));
	do{
		DWORD dwWait = WaitForMultipleObjects(1, waitHandles, FALSE, dwTimeout);
		switch(dwWait){
			case WAIT_OBJECT_0:
				//stop
				DEBUGMSG(1,(L"powerWatchThread stop signaled\n"));
				bStopThread=TRUE;
				break;
			case WAIT_TIMEOUT:
				if(isACpower()){
					DEBUGMSG(1,(L"powerWatchThread timeout. On AC power: Reset Idle Timer\n"));
					if(h_BeeperThread)
						stopBeeper();
					if(h_IdleThread)
						resetIdleThread();
				}
				else{
					DEBUGMSG(1,(L"powerWatchThread timeout. NOT on AC power\n"));
				}
				break;
		}
	}while(!bStopThread);
	DEBUGMSG(1,(L"powerWatchThread stopped\n"));
	h_PowerThread=NULL;
	return 0;
}

///start a timer with a interval of x seconds
void startIdleThread(UINT iTimeOut){
	DEBUGMSG(1,(L"startIdleThread entered\n"));
#ifdef DEBUG
	iTimeOut=5*1000;	//we need seconds
#else
	iTimeOut=iTimeOut*1000;	//we need seconds
#endif
	//init event HANDLES
	if(!h_resetIdleThread)
		h_resetIdleThread=CreateEvent(NULL, FALSE, FALSE, L"h_resetIdleThread");
	if(!h_stopIdleThread)
		h_stopIdleThread=CreateEvent(NULL, FALSE, FALSE, L"h_stopIdleThread");
	if(!h_stopBeeperThread)
		h_stopBeeperThread=CreateEvent(NULL, FALSE, FALSE, L"h_stopBeeperThread");

	if(!h_IdleThread){
		h_IdleThread = CreateThread(NULL, 0, idleThread, (LPVOID)iTimeOut, 0, &threadIdleID);
		DEBUGMSG(1, (L"idle thread started with handle=0x%x\n", h_IdleThread));
	}

	//start power watch thread
	if(!h_stopPowerThread)
		h_stopPowerThread=CreateEvent(NULL, FALSE, FALSE, L"h_stopPowerThread");
	DWORD dwPowerTimeout=1000; //check power every 1 second
	if(!h_PowerThread){
		h_PowerThread=CreateThread(NULL, 0, powerWatchThread, (LPVOID)dwPowerTimeout, 0, &threadPowerID);
		DEBUGMSG(1, (L"power thread started with handle=0x%x\n", h_PowerThread));
	}

}


void stopIdleThread(){
	DEBUGMSG(1, (L"stopIdleThread entered\n"));
	if(h_IdleThread!=NULL)	//is thread running
		if(h_stopIdleThread)
			SetEvent(h_stopIdleThread);	//stop timer thread	

	//stop power thread
	if(h_PowerThread!=NULL)
		if(h_stopPowerThread)
			SetEvent(h_stopPowerThread);
}

void stopBeeper(){
	DEBUGMSG(1, (L"stopBeeper entered\n"));
	if(h_BeeperThread==NULL)	//is thread running
		return;	//no timer running
	SetEvent(h_stopBeeperThread);	

	if(g_hDlgInfo && bInfoDlgVisible)
		ShowWindow(g_hDlgInfo, SW_HIDE);
}

void resetIdleThread(){
	logTime(); DEBUGMSG(1, (L"resetIdleThread entered\n"));
	if(h_IdleThread==NULL)	//is thread running
		return;	//no timer running
	SetEvent(h_resetIdleThread);	
}

void doAlarm(){
	LedOn(LEDid,2); //blink LED

	if(g_hDlgInfo && bInfoDlgVisible){
		SetWindowPos(g_hDlgInfo, HWND_TOPMOST, 0,0,0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
		//ShowWindow(g_hDlgInfo, SW_SHOW);
	}

	//issue some sound
#ifdef DEBUG
	MessageBeep(MB_ICONASTERISK);
	Sleep(700);
	MessageBeep(MB_ICONASTERISK);
	Sleep(300);
	MessageBeep(MB_ICONERROR);
	Sleep(700);
	MessageBeep(MB_ICONERROR);
#else
	if( ITCIsAudioToneSupported() )
	{
		HRESULT hr;
		hr = ITCAudioPlayTone( 2400, 200, ITC_GetToneVolumeVeryLoud() );
		hr = ITCAudioPlayTone( 1200, 200, ITC_GetToneVolumeVeryLoud() );
		hr = ITCAudioPlayTone( 2400, 200, ITC_GetToneVolumeVeryLoud() );
		hr = ITCAudioPlayTone( 0, 1000, 0 );
		hr = ITCAudioPlayTone( 2400, 200, ITC_GetToneVolumeVeryLoud() );
		hr = ITCAudioPlayTone( 1200, 200, ITC_GetToneVolumeVeryLoud() );
		hr = ITCAudioPlayTone( 2400, 200, ITC_GetToneVolumeVeryLoud() );
	}
	else{
		MessageBeep(MB_ICONASTERISK);
		Sleep(300);
		MessageBeep(MB_ICONERROR);
	}

#endif
}

DWORD WINAPI beeperThread(LPVOID lParam){
	DEBUGMSG(1, (L"beeperThread entered\n"));
	BOOL bStopThread=FALSE;
	HANDLE waitHandles[1];
	waitHandles[0]=h_stopBeeperThread;
	bInfoDlgVisible=regValEnableInfo;	//reset info dlg setting, possibly override by Dismiss button
	//issue first alarm
	doAlarm();
	do{
		DWORD dwWait = WaitForMultipleObjects(1, waitHandles, FALSE, 5000);
		switch(dwWait){
			case WAIT_OBJECT_0:
				//stop
				DEBUGMSG(1,(L"beeperThread stop signaled\n"));
				bStopThread=TRUE;
				break;
			case WAIT_TIMEOUT:
				logTime(); DEBUGMSG(1,(L"beeperThread timeout. Beeping\n"));
				doAlarm();
				break;
		}
	}while(!bStopThread);
	DEBUGMSG(1,(L"beeperThread stopped\n"));
	h_BeeperThread=NULL;
	LedOn(LEDid, 0);	//shut off blinking LED
	return 0;
}

DWORD WINAPI idleThread(LPVOID lParam){
	HANDLE waitHandles[2];
	waitHandles[0]=h_resetIdleThread;
	waitHandles[1]=h_stopIdleThread;
	BOOL bStopThread=FALSE;
	DWORD dwTimeout=(DWORD) lParam;
	DEBUGMSG(1,(L"idleThread starting with %i\n", dwTimeout));
	do{
		DWORD dwWait = WaitForMultipleObjects(2, waitHandles, FALSE, dwTimeout);
		switch(dwWait){
			case WAIT_OBJECT_0:
				//reset idle, do nothing
				DEBUGMSG(1,(L"idleThread reset signaled\n"));
				break;
			case WAIT_OBJECT_0+1:
				DEBUGMSG(1,(L"idleThread stop signaled\n"));
				bStopThread=TRUE;
				break;
			case WAIT_TIMEOUT:
				//is on AC power?
				if(! isACpower() ){
					//start beeper if not running
					DEBUGMSG(1,(L"idleThread timeout...\n"));
					if(h_BeeperThread==NULL){
						DEBUGMSG(1,(L"idleThread Create Beeper Thread...\n"));
						h_BeeperThread = CreateThread(NULL, 0, beeperThread, NULL, 0, &threadIdleID);
					}
					else
						DEBUGMSG(1,(L"idleThread Beeper Thread already running\n"));
				}
				else
					DEBUGMSG(1, (L"idleThread timeout, skipped start beeper as on AC power\n"));
				break;
		}
	}while (!bStopThread);
	h_IdleThread=NULL;
	DEBUGMSG(1,(L"idleThread ended\n"));
	return 0;
}
