//idleBeeper.cpp

#include "stdafx.h"

#include <itc50.h>
#pragma comment (lib, "itc50.lib")

#include "idleBeeper.h"

#include "dlg_info.h"
#include "nclog.h"

   #define _SECOND ((INT64) 10000000)
   #define _MINUTE (60 * _SECOND)
   #define _HOUR   (60 * _MINUTE)
   #define _DAY    (24 * _HOUR)

//extern BOOL bInfoDlgVisible;	//show/hide DlgInfo
extern DWORD regValEnableInfo;

/*
	LED CN51
	4	red blink
	7   red
	5	vibrate
	15	vibrate
*/
extern int LEDid;		//which LED to use
extern int VibrateID;
extern int alarmLEDid;
extern void LedOn(int id, int onoff); //onoff=0 LED is off, onoff=1 LED is on
extern DWORD regValTimeOff;
extern DWORD regValTimeOn;
extern DWORD regValSundayAlarmEnabled;

//events to control beeper and idle thread
DWORD vibrateThreadID=0;
HANDLE vibrateThreadHandle=NULL;

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
	nclog(L"startIdleThread entered with timeout=%i\n", iTimeOut);
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
		//DEBUGMSG(1, (L"idle thread started with handle=0x%x\n", h_IdleThread));
		nclog(L"idle thread started with handle=0x%x\n", h_IdleThread);
	}

	//start power watch thread
	if(!h_stopPowerThread)
		h_stopPowerThread=CreateEvent(NULL, FALSE, FALSE, L"h_stopPowerThread");
	DWORD dwPowerTimeout=1000; //check power every 1 second
	if(!h_PowerThread){
		h_PowerThread=CreateThread(NULL, 0, powerWatchThread, (LPVOID)dwPowerTimeout, 0, &threadPowerID);
		//DEBUGMSG(1, (L"power thread started with handle=0x%x\n", h_PowerThread));
		nclog(L"power thread started with handle=0x%x\n", h_PowerThread);
	}
	nclog(L"startIdleThread() done\n");
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
	logTime(); 
	DEBUGMSG(1, (L"resetIdleThread entered\n"));
	if(h_IdleThread==NULL)	//is thread running
		return;	//no timer running
	SetEvent(h_resetIdleThread);	
}

DWORD WINAPI vibrate(LPVOID lpParam){
	int id=(int)lpParam;
	DEBUGMSG(1, (L"vibrate START, vibrateID=%i...\r\n", id));
	LedOn(id,1);
	LedOn(alarmLEDid,1);
	Sleep(300);
	LedOn(id,0);
	LedOn(alarmLEDid,0);
	Sleep(300);
	LedOn(id,1);
	LedOn(alarmLEDid,1);
	Sleep(300);
	LedOn(id,0);
	LedOn(alarmLEDid,0);
	Sleep(300);
	LedOn(id,1);
	LedOn(alarmLEDid,1);
	Sleep(300);
	LedOn(id,0);
	LedOn(alarmLEDid,0);
	DEBUGMSG(1, (L"vibrate END...\r\n"));
	return 0;
}

BOOL bToggleVibrate=TRUE;

void doSound(){
	vibrateThreadHandle = CreateThread(NULL, 0, vibrate, (LPVOID)VibrateID, 0, &vibrateThreadID);

	//issue some sound
#ifdef DEBUG
	//MessageBeep(MB_ICONASTERISK);
	//Sleep(700);
	//MessageBeep(MB_ICONASTERISK);
	//Sleep(300);
	//MessageBeep(MB_ICONERROR);
	//Sleep(700);
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

TCHAR* dumpSystemTime(SYSTEMTIME st, TCHAR* txt){
	wsprintf(txt, L"%02i.%02i.%04i %02i:%02i\n",
		st.wDay, st.wMonth, st.wYear,
		st.wHour, st.wMinute);
	return txt;
}

void ftAddDays(int iDays, FILETIME* ft){
   ULONGLONG qwResult;
   // Copy the time into a quadword.
   qwResult = (((ULONGLONG) ft->dwHighDateTime) << 32) + ft->dwLowDateTime;
   // Add 30 days.
   qwResult += (ULONGLONG)(iDays * _DAY);
   // Copy the result back into the FILETIME structure.
   ft->dwLowDateTime  = (DWORD) (qwResult & 0xFFFFFFFF );
   ft->dwHighDateTime = (DWORD) (qwResult >> 32 );
}

int CompareSystemTime(SYSTEMTIME st1, SYSTEMTIME st2){	
	int int1=st1.wHour*60+st1.wMinute;
	int int2=st2.wHour*60+st2.wMinute;
	if(int1<int2)
		return -1;
	else if(int1>int2)
		return 1;
	else if(int1==int2)
		return 0;
	return 0;
}

BOOL alarmAllowed(){
	BOOL bRet=TRUE;
	SYSTEMTIME stCurrent, stOFF, stON;
	GetLocalTime(&stCurrent);
	//check for sunday, no alarms on sunday
	if(regValSundayAlarmEnabled==0 && stCurrent.wDayOfWeek==0){
		nclog(L"SundayAlarmEnabled=0 and we have a sunday (DayOfWeek=%i)\n", stCurrent.wDayOfWeek);
		return FALSE;
	}
	stOFF=stCurrent;
	stOFF.wHour=(BYTE)(regValTimeOff/100);
	stOFF.wMinute=(BYTE)(regValTimeOff%100);
	stON=stCurrent;
	stON.wHour=(BYTE)(regValTimeOn/100);
	stON.wMinute=(BYTE)(regValTimeOn%100);

	/*	Minutes of the day, 2400?
	if (current >= start && current < (end<start?end+24*60 : end)
	if (current >= start && current < ((end<start)?end+24*60 : end) 
	if (current >= start && current < ((end<start)?end+24*60 : end)) 
	
	if (current >= start && current < ((end<start)?end+2400 : end))  
	*/
	int current=stCurrent.wHour*60+stCurrent.wMinute;
	int start=stOFF.wHour*60+stOFF.wMinute;
	int end=stON.wHour*60+stON.wMinute, newEnd;
	if(end<start){
		newEnd=end+24*60;
		nclog(L"start %i, current %i, newEnd %i\n", start, current, newEnd);
	}else{
		nclog(L"start %i, current %i, end %i\n", start, current, end);
	}

	if (current >= start && current < (end<start ? end+24*60 : end)){
		nclog(L"first OK\n");
		if (current >= start && current < ((end<start) ? end+24*60 : end)){
			nclog(L"second OK\n");
			if (current >= start && current < ((end<start)?end+24*60 : end)){
				nclog(L"third OK\n");
				; 
			}
		}
	}

	FILETIME ftCurrent, ftOff, ftOn;
	SystemTimeToFileTime(&stCurrent, &ftCurrent);
	SystemTimeToFileTime(&stOFF, &ftOff);
	SystemTimeToFileTime(&stON, &ftOn);
	//if OffTime is less then OnTime, we are on the same day
	//if OffTime is after OnTime (ie 2200 and 0500), we need to add one day to OnTime
	//ALL CALCULATIONS have to be done for being on the SAME day
	// 22:00->05:00
	BOOL bCrossingMidnight=FALSE;
	if(CompareFileTime(&ftOff, &ftOn)==1){ //ftOff is after ftOn (same day)?
		ftAddDays(1, &ftOn);
		bCrossingMidnight=TRUE;
	}
/*
0x9609228a: alarmAllowed-OFF time: 01.01.2003 14:00
0x9609228a: alarmAllowed-ON time: 02.01.2003 15:00
*/
	//PROBLEM: Alarm issued although before OFF time
	/*														Alarm	TEST
	0xb5fcaf82: alarmAllowed-OFF time: 12.11.2015 22:00		
	0xb5fcaf82: alarmAllowed-ON time:  13.11.2015 05:00
	0xb5fcaf82: current time:          12.11.2015 00:31		no		OK
									   12.11.2015 05:31		yes		OK
									   12.11.2015 21:31		yes		OK
									   12.11.2015 22:31		no		NOT OK
	bCrossingMidnight=TRUE as ON is before OFF
	*/
	if(bCrossingMidnight)
		ftAddDays(-1, &ftOn);

	//for display
	FileTimeToSystemTime(&ftOff, &stOFF);
	FileTimeToSystemTime(&ftOn, &stON);
	nclog(L"alarmAllowed-OFF time: %02i.%02i.%04i %02i:%02i\n",
		stOFF.wDay, stOFF.wMonth, stOFF.wYear,
		stOFF.wHour, stOFF.wMinute);
	nclog(L"alarmAllowed-ON time:  %02i.%02i.%04i %02i:%02i\n",
		stON.wDay, stON.wMonth, stON.wYear,
		stON.wHour, stON.wMinute);
	nclog(L"current time:          %02i.%02i.%04i %02i:%02i\n",
		stCurrent.wDay, stCurrent.wMonth, stCurrent.wYear,
		stCurrent.wHour, stCurrent.wMinute);

	if(CompareFileTime(&ftCurrent, &ftOff)==1){
		nclog(L"### ftCurrent after ftOff\n");
		if(CompareFileTime(&ftCurrent, &ftOn)==-1){
			nclog(L"### ftCurrent before ftOn. Alarm disabled!\n");
			//CompareFileTime(&first, &second) = -1 first is before second
			//CompareFileTime(&first, &second) = +1 first is after second
			//time		..........OFF-------ON..........
			//current	    ^								ALARM OK
			//                        ^                     ALARM DISABLED (quiet)
			//                                   ^          ALARM OK
			bRet=FALSE;
		}
	}
	nclog(L"alarmAllowed=%i\n", bRet);
	return bRet;
}

void doAlarm(){
	//moved to vibrate alarm thread
	//LedOn(alarmLEDid,2); //blink LED

	if(g_hDlgInfo && bInfoDlgVisible){
		SetWindowPos(g_hDlgInfo, HWND_TOPMOST, 0,0,0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
		//ShowWindow(g_hDlgInfo, SW_SHOW);
	}
	//are TimeOff and TimeOn set?
	if(regValTimeOff!=0 && regValTimeOn!=0){
		if(alarmAllowed()){
			nclog(L"within allowed alarm timespan\n");
			vibrateThreadHandle = CreateThread(NULL, 0, vibrate, (LPVOID)VibrateID, 0, &vibrateThreadID);
			doSound();
		}
		else{
			nclog(L"outside allowed alarm timespan. Alarm sound disabled.\n");
		}
	}
	else { //TimeOff/TimeOn not set
		nclog(L"no alarm timespan set\n");
		vibrateThreadHandle = CreateThread(NULL, 0, vibrate, (LPVOID)VibrateID, 0, &vibrateThreadID);
		doSound();
	}
}

DWORD WINAPI beeperThread(LPVOID lParam){
	nclog(L"beeperThread entered\n");
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
	LedOn(alarmLEDid, 0);	//shut off alarm LED
	LedOn(VibrateID, 0);	//shut off vibrate
	return 0;
}

DWORD WINAPI idleThread(LPVOID lParam){
	HANDLE waitHandles[2];
	waitHandles[0]=h_resetIdleThread;
	waitHandles[1]=h_stopIdleThread;
	BOOL bStopThread=FALSE;
	DWORD dwTimeout=(DWORD) lParam;
	//DEBUGMSG(1,(L"idleThread starting with %i\n", dwTimeout));
	nclog(L"idleThread starting with %i\n", dwTimeout);
	do{
		DWORD dwWait = WaitForMultipleObjects(2, waitHandles, FALSE, dwTimeout);
		switch(dwWait){
			case WAIT_OBJECT_0:
				//reset idle, do nothing
				DEBUGMSG(1,(L"idleThread reset signaled\n"));
				break;
			case WAIT_OBJECT_0+1:
				DEBUGMSG(1,(L"idleThread stop signaled\n"));
				nclog(L"idleThread stop signaled\n");
				bStopThread=TRUE;
				break;
			case WAIT_TIMEOUT:
				//is on AC power?
				if(! isACpower() ){
					//start beeper if not running
					DEBUGMSG(1,(L"idleThread timeout...\n"));
					if(h_BeeperThread==NULL){
						nclog(L"idleThread timeout reached at %s. Creating beeperThread\n", logDateTime());
						DEBUGMSG(1,(L"idleThread Create Beeper Thread...\n"));
						h_BeeperThread = CreateThread(NULL, 0, beeperThread, NULL, 0, &threadIdleID);
					}
					else
						DEBUGMSG(1,(L"idleThread Beeper Thread already running\n"));
				}
				else
					nclog(L"idleThread timeout, skipped start beeper as on AC power\n");
				break;
		}
	}while (!bStopThread);
	h_IdleThread=NULL;
	nclog(L"idleThread ended\n");
	return 0;
}
