// iTimedReboot2.cpp : Defines the entry point for the application.
//

// iTimedReboot2 v0.3

//history
//version	change
// 1.0		initial release
/*
[HKEY_LOCAL_MACHINE\SOFTWARE\Intermec\iTimedReboot2]
"Interval"="0"			//0=OFF, seconds between time checks, do not use >30000
"RebootDays"="0"		//days between reboots
"RebootTime"="00:00"	//time to reboot in hh:mm
"PingInterval"="0"		//0=OFF, seconds between pings
"PingTarget"="127.0.0.1"//target IP to ping
"EnableLogging"="0"		//0=disable logging file, 1=enable
"LastBootDate"=yyyymmdd	//date of last boot
*/

#include "stdafx.h"
#include "registry.h"
#include "resource.h"
#include "ping.h"

//#include "log2file.h"
#include "nclog.h"

#include <time.h>

#include "mytimefuncs.h"

#define IDC_BUTTON_OK 201 
#define IDC_BUTTON_CANCEL 202 

#ifndef WM_TIMECHANGE
#define WM_TIMECHANGE 0x1E
#endif

int iTESTMODE = 0;

#define MY_MUTEX L"ITIMEDREBOOT_RUNNING"
HANDLE hMutex=NULL;

//FILETIME helpers
#define _SECOND ((ULONGLONG) 10000000)
#define _MINUTE (60 * _SECOND)
#define _HOUR   (60 * _MINUTE)
#define _DAY    (24 * _HOUR)

#define		MAX_LOADSTRING			100

RECT rect;

// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

int		ReadReg();
void	WriteReg();
bool	StrIsNumber(TCHAR *str);

// Global Variables:
HINSTANCE		g_hInstance;								// current instance
TCHAR			szTitle[MAX_LOADSTRING];			// The title bar text
TCHAR			szWindowClass[MAX_LOADSTRING];		// The title bar text
HWND g_hwnd;

//Notification icon stuff
NOTIFYICONDATA	IconData;
const int		IconSlot = 0;
#define			MYMSG_TASKBARNOTIFY  (WM_USER + 100)

// List of Icon that is created and to be animated
#define			NUM_ICON_FOR_ANIMATION	9
static int		IconResourceArray[NUM_ICON_FOR_ANIMATION] = {IDI_ICON0,	//the standard tray icon (a black bomb)
IDI_ICON1, //a red X (used for no ping reply)
IDI_ICON2, //one small yellow bar (one ping returned)
IDI_ICON3, //two green bars
IDI_ICON4, //three green bars (three ping returned)
IDI_ICON5, //four green bars
IDI_ICON6, //a question mark
IDI_ICON7, //a black bomb
IDI_ICON8};//a red bomb
const int ico_app		=0;
const int ico_redx		=1;
const int ico_ping1		=2;
const int ico_ping2		=3;
const int ico_ping3		=4;
const int ico_ping4		=5;
const int ico_question	=6;
const int ico_bomb		=7;
const int ico_redbomb	=8;

// Current icon displayed index values.
static	int		m_nCounter = 0;
VOID AnimateIcon(HINSTANCE hInstance, HWND hWnd, DWORD dwMsgType,UINT nIndexOfIcon);

const int		ID_RebootTimeCheck=100;		//Timer IDs
const int		ID_PingIntervalTimer=101;

int				g_iTimerReboot;		//how often check for Reboot Time
int				g_iTimerPing;;		//how often Ping

//values to/from registry
int				g_iRebootTimerCheck;			//0=OFF, seconds between time checks
SYSTEMTIME		g_stRebootTime;
TCHAR			g_sRebootTime[MAX_PATH];		//time to reboot in hh:mm
int				g_iPingTimeInterval;
int				g_iRebootDays;
TCHAR			g_sPingTarget[MAX_PATH];		//target IP to ping
bool			g_bEnableLogging=false;
TCHAR			g_sEnableLogging[MAX_PATH];	//0=disable logging file, 1=enable

SYSTEMTIME newTime;							//calculated random reboot time

TCHAR			g_LastBootDate[MAX_PATH];	//string holding the last reboot date
SYSTEMTIME		g_stLastBootDateTime;		//SYSTEMTIME of last reboot date/time
SYSTEMTIME		g_stRebootDateTime;			//the date/time we have to reboot

BOOL			g_DoReboot = TRUE;
TCHAR			g_ExeName[MAX_PATH];
TCHAR			g_ExeArgs[MAX_PATH];

//registry shortcuts using an enum
enum REGKEYS{
	Interval=0,
	RebootTime=1,
	PingInterval=2,
	PingTarget=3,
	EnableLogging=4,
	LastBootDate=5,
	RebootExt=6,
	RebootExtApp=7,
	RebootExtParms=8,
	RebootDays=9,
	NewTime=10,
};

//number of entries in the registry
const int		RegEntryCount=11;				//how many entries are in the registry, with v2 changed from 6 to 9
TCHAR*			g_regName = L"Software\\Intermec\\iTimedReboot2";

//struct to hold reg names and values
typedef struct
{
	TCHAR kname[MAX_PATH];
	TCHAR ksval[MAX_PATH];
	DWORD kstype;
} rkey;
rkey			rkeys[RegEntryCount];	//an array with the reg settings

//things needed to request a warmboot
#include <winioctl.h>
extern "C" __declspec(dllimport) BOOL KernelIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
#define IOCTL_HAL_REBOOT CTL_CODE(FILE_DEVICE_HAL, 15, METHOD_BUFFERED, FILE_ANY_ACCESS)
extern "C" __declspec(dllimport) void SetCleanRebootFlag(void);

//predefined functions
void nclog (TCHAR * t);
BOOL DoBootAction();


//=================================================================================
//
//  FUNCTION:	WarmBoot()
//
//  PURPOSE:	Reboot the device
//
//  COMMENTS:	none
//
BOOL WarmBoot()
{
	//return KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
	return DoBootAction();
}

//=====================================================================================================
//
//  FUNCTION:	DoBootAction()
//
//  PURPOSE:	Start an exe or do a Warmboot
//
//  COMMENTS:	none
//
BOOL DoBootAction(){
	if(g_DoReboot){
		nclog(L"Rebooting now...\n",NULL);
		Sleep(1000);
#ifndef DEBUG
		if(iTESTMODE==0)
			return KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
#else
		nclog(L"DEBUGMODE no warmboot\n",NULL);
#endif
	}
	else{
		TCHAR str[MAX_PATH];
		PROCESS_INFORMATION pi;
		if(CreateProcess(g_ExeName, g_ExeArgs, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)==0){
			wsprintf(str, L"CreateProcess ('%s'/'%s') failed with 0x%08x\r\n", g_ExeName, g_ExeArgs, GetLastError());
			nclog(str,NULL);
			return FALSE;
		}
		else{

			wsprintf(str, L"CreateProcess ('%s'/'%s') OK. pid=0x%08x\r\n", g_ExeName, g_ExeArgs, pi.dwProcessId);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			nclog(str,NULL);
			return TRUE;
		}
	}
	return TRUE;
}

SYSTEMTIME getRandomTime(SYSTEMTIME lt){
	// we need some var to add a random time (0 to 120 minutes)
	// see also http://support.microsoft.com/kb/188768
	static SYSTEMTIME newTime;
	memset(&newTime, 0, sizeof(SYSTEMTIME));

	FILETIME ft;
	ULONGLONG ut;
	BOOL bRes = SystemTimeToFileTime(&lt, &ft);	//convert SystemTime to FileTime
	//convert FileTime to ULONG
	ut = (((ULONGLONG) ft.dwHighDateTime) << 32) + ft.dwLowDateTime;
	// add some time
	//RAND_MAX; //0x7fff	
	srand((int)GetTickCount());	//initialize random seed:
	int randomMinutes = rand() % 120 + 1;
	nclog(L"### TimedReboot: Using random minutes: %i\r\n", randomMinutes); 

	ut += (ULONGLONG) (randomMinutes * _MINUTE);
	// Copy the result back into the FILETIME structure.
	ft.dwLowDateTime  = (DWORD) (ut & 0xFFFFFFFF );
	ft.dwHighDateTime = (DWORD) (ut >> 32 );
	//convert back to filetime and systemtime
	bRes=FileTimeToSystemTime(&ft, &newTime);

	return newTime;	
}

void writeLastBootDate(SYSTEMTIME stBoot){
	TCHAR szDate[MAX_PATH];

	int rc = GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, &stBoot, L"yyyyMMdd", szDate, MAX_PATH-1);
	wsprintf(szDate, L"%04i%02i%02i", 
		stBoot.wYear, stBoot.wMonth, stBoot.wDay);

	OpenKey();
	if(RegWriteStr(rkeys[LastBootDate].kname, szDate)!=0)
		DEBUGMSG(1, (L"save lastbootdate to reg failed!\n"));
	CloseKey();
	wsprintf(rkeys[LastBootDate].ksval, szDate);

	//update glovbal vars
	wsprintf(g_LastBootDate, szDate);
	TCHAR szLasteDateTime[13];
	wsprintf(szLasteDateTime, L"%s%02i%02i", g_LastBootDate, newTime.wHour, newTime.wMinute);

	//TODO check implementation!
	if(getSystemtimeOfString(szLasteDateTime, &g_stLastBootDateTime)){
		DEBUGMSG(true, (L"date string converted to SYSTEMTIME\n"));
	}
	else{
		DEBUGMSG(true, (L"date string conversion to SYSTEMTIME FAILED!\n"));
	}

	if(iTESTMODE==1)
		nclog(L"Updated registry with last reboot on:\n\t%s, %s\n", szDate, g_sRebootTime);
}

void doAnimateAndReboot(SYSTEMTIME stCurrentTime){
	if(iTESTMODE==0){
		AnimateIcon(g_hInstance, g_hwnd, NIM_MODIFY, ico_redbomb);
		Sleep(3000);
	}
	TCHAR sDateNow[MAX_PATH];
	int er=0;
	//produce a mathematic date
	int rc = GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, &stCurrentTime, L"yyyyMMdd", sDateNow, MAX_PATH-1);
	nclog(L"##### Will reboot now. Last reboot on:\n\t%s, %s #####\n", sDateNow, g_sRebootTime);
	DEBUGMSG(true, (L"\r\nREBOOT...\r\n"));
	WarmBoot();				//ITCWarmBoot() was not used due to itc50.dll dependency
}

//=================================================================================
//
//  FUNCTION:	TimedReboot()
//
//  PURPOSE:	if last reboot +daysInterval is within 3 minutes = reboot
//				else update registry with next reboot date
//  COMMENTS:	none
//
int TimedReboot(void)
{
	if(iTESTMODE==1)
		nclog(L"__TimedReboot Check__\n");

	int iReturn=0;	//return 0 for normal operation, return 1 to exit (reboot time), 2 for past reboot time, -1 for before reboot time

	SYSTEMTIME stCurrentTime;
	memset(&stCurrentTime, 0, sizeof(stCurrentTime));
	GetLocalTime(&stCurrentTime);	//stCurrentTime is now the actual datetime
	if(iTESTMODE==1){
		nclog(L"--- last reboot date is:  %s, reboot time is %02i:%02i\n",
			g_LastBootDate, newTime.wHour, newTime.wMinute);
		nclog(L"+++ current time is: %04i%02i%02i %02i:%02i\n",
			stCurrentTime.wYear, stCurrentTime.wMonth, stCurrentTime.wDay,
			stCurrentTime.wHour, stCurrentTime.wMinute);
		nclog(L"+++ reboot scheduled for: %04i%02i%02i %02i:%02i\n",
			g_stRebootDateTime.wYear, g_stRebootDateTime.wMonth, g_stRebootDateTime.wDay,
			g_stRebootDateTime.wHour, g_stRebootDateTime.wMinute);
	}

	int iDayDiff = 0;
	int iHoursDiff = 0;
	int iMinutesDiff = 0;
	int iDiff=0;

	// g_stRebootDateTime is the planned reboot time
	iDiff = DiffInMinutes(g_stRebootDateTime, stCurrentTime, &iDayDiff, &iHoursDiff, &iMinutesDiff);
	//if minutes is > 0 and < 3 (we are later than reboot time) but within timespan, we need to update the registry and do a warmboot
	//if minutes is < 0 we are before reboot time
	//if minutes is > 3 we are behind reboot time and have to recalculate the next reboot time with day interval 

	//iDiff = getDateTimeDiff(g_stRebootDateTime, stCurrentTime, &iDayDiff, &iHoursDiff, &iMinutesDiff);
	if(iDiff < 0){
		if(iTESTMODE==1)
			nclog(L"WE ARE BEFORE REBOOT TIME\n");
		DEBUGMSG(1, (L"WE ARE BEFORE REBOOT TIME\n"));
		return iReturn;
	}
	if(iDiff <= 3 && iDiff >= 0){
		DEBUGMSG(1, (L"WE ARE WITHIN REBOOT TIMESPAN\n"));
		if(iTESTMODE==1)
			nclog(L"WE ARE WITHIN REBOOT TIMESPAN\n");
		//write current date as last reboot date to reg
		writeLastBootDate(stCurrentTime);
		//update of vars not needed as we do a reboot
		//do a warmboot
		doAnimateAndReboot(stCurrentTime);
		iReturn=1;
		return iReturn;
	}
	if(iDiff > 0){
		if(iTESTMODE==1)
			nclog(L"WE ARE AFTER REBOOT TIME\n");
		DEBUGMSG(1, (L"WE ARE AFTER REBOOT TIME\n"));
		//calc next day to reboot using day interval and last reboot date
		SYSTEMTIME stLastReboot = getNextBootWithInterval(stCurrentTime, g_stLastBootDateTime, g_iRebootDays);
		
		//the new date will be the next time to boot, we need to calc the last 'theoretic' reboot date
		//update registry and global vars
		g_stLastBootDateTime=addDays(stLastReboot, -g_iRebootDays);

		wsprintf(g_LastBootDate, L"%04i%02i%02i", g_stLastBootDateTime.wYear, g_stLastBootDateTime.wMonth, g_stLastBootDateTime.wDay);

		//	g_stRebootDateTime holds time as read by registry
		//	+ needs to be updated as used for diff calculating, if there is no reboot (which would read the reg with new value)
		g_stRebootDateTime=g_stLastBootDateTime;
		
		writeLastBootDate(g_stLastBootDateTime);
		
		iReturn=2;
		return iReturn;
	}

	//globals
	/*

	g_LastBootDate;	//string with date of last reboot
	g_iRebootDays;	//days interval between reboots
	g_stRebootTime;			// time of planned reboot, the planned reboot time + a radom time, only hh:mm are valid
	g_stLastBootDateTime;		//SYSTEMTIME of last reboot date/time
	g_stRebootDateTime;			//SYSTEMTIME of next reboot
	*/

	if(iTESTMODE==1)
		nclog(L"--- TimedReboot Check END ---\n");
	return iReturn;
}

//=====================================================================================================
//
//  FUNCTION:	initRKEYS()
//
//  PURPOSE:	init the rkeys struct with defaults
//
//  COMMENTS:	none
//
void initRKEYS()
{
	//init array with registry keys and vals
	memset(&rkeys, 0, sizeof(rkeys));

	wsprintf(rkeys[Interval].kname, L"Interval");
	wsprintf(rkeys[Interval].ksval, L"30");
	rkeys[Interval].kstype=REG_SZ;

	wsprintf(rkeys[RebootTime].kname, L"RebootTime");
	wsprintf(rkeys[RebootTime].ksval, L"00:00");
	rkeys[RebootTime].kstype=REG_SZ;

	wsprintf(rkeys[PingInterval].kname, L"PingInterval");
	wsprintf(rkeys[PingInterval].ksval, L"60");
	rkeys[PingInterval].kstype=REG_SZ;

	wsprintf(rkeys[PingTarget].kname, L"PingTarget");
	wsprintf(rkeys[PingTarget].ksval, L"127.0.0.1");
	rkeys[PingTarget].kstype=REG_SZ;

	wsprintf(rkeys[EnableLogging].kname, L"EnableLogging");
	wsprintf(rkeys[EnableLogging].ksval, L"1");
	rkeys[EnableLogging].kstype=REG_SZ;

	wsprintf(rkeys[LastBootDate].kname, L"LastBootDate");
	wsprintf(rkeys[LastBootDate].ksval, L"19800101");
	rkeys[LastBootDate].kstype=REG_SZ;
	//new with v2
	wsprintf(rkeys[RebootExt].kname, L"RebootExt");
	wsprintf(rkeys[RebootExt].ksval, L"1");
	rkeys[RebootExt].kstype=REG_SZ;
	wsprintf(rkeys[RebootExtApp].kname, L"RebootExtApp");
	wsprintf(rkeys[RebootExtApp].ksval, L"\\Windows\\fexplore.exe");
	rkeys[RebootExtApp].kstype=REG_SZ;

	wsprintf(rkeys[RebootExtParms].kname, L"RebootExtParms");
	wsprintf(rkeys[RebootExtParms].ksval, L"\\Flash File Store");
	rkeys[RebootExtParms].kstype=REG_SZ;
	//new with v3
	wsprintf(rkeys[RebootDays].kname, L"RebootDays");
	wsprintf(rkeys[RebootDays].ksval, L"0");
	rkeys[RebootDays].kstype=REG_SZ;

	wsprintf(rkeys[NewTime].kname, L"NewTime");
	wsprintf(rkeys[NewTime].ksval, L"00:11");
	rkeys[NewTime].kstype=REG_SZ;

}
//=================================================================================
//
//  FUNCTION:	TimedPing()
//
//  PURPOSE:	Ping target and change icon depending on replies
//
//  COMMENTS:	none
//
void TimedPing(void)
{
	int rc = PingAddress(rkeys[PingTarget].ksval); //rc will be number of good pings
	if (rc==-1)
	{
		//Bad IP!
		AnimateIcon(g_hInstance, g_hwnd , NIM_MODIFY, ico_question);	//show a question mark
		nclog(L"### Bad IP address\n");
		DEBUGMSG(true, (L"Bad IP address format?\r\n"));
	}
	else
		AnimateIcon(g_hInstance, g_hwnd , NIM_MODIFY, rc+1);//rc+1 is number of returned pings
	Sleep(1000);
	AnimateIcon(g_hInstance, g_hwnd , NIM_MODIFY, ico_app);
}



//=================================================================================
//
//  FUNCTION:	WinMain()
//
//  PURPOSE:	main entry point
//
//  COMMENTS:	none
//
int APIENTRY WinMain(	HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPTSTR    lpCmdLine,
					 int       nCmdShow)
{
	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	if (wcsstr(lpCmdLine, L"-test") != NULL){
		iTESTMODE=1;
		initRKEYS();

		if(ReadReg()==-1){
			//DEBUGMSG(1, (L"\n!!!!!!!!!! UNABLE to read Registry settings !!!!!!!!!!!!!!\n"));
			nclog(L"\n!!!!!!!!!! UNABLE to read Registry settings !!!!!!!!!!!!!!\n");
			return -22;
		}
		g_bEnableLogging=TRUE;
		TimedReboot();
		return -11;
	}
	//allow only one instance
	//##################### dont run if already running #############################
	nclog(L"Checking for Mutex (single instance allowed only)...\n");

	hMutex=CreateMutex(NULL, TRUE, MY_MUTEX);
	if(hMutex==NULL){
		//this should never happen
		nclog(L"Error in CreateMutex! GetLastError()=%i\n", GetLastError());
		nclog(L"-------- END -------\n");
		return -99;
	}
	DWORD dwLast = GetLastError();
	if(dwLast== ERROR_ALREADY_EXISTS){//mutex already exists, EXIT!
		nclog(L"\tAttached to existing mutex\n");
		return -2;
	}
	else{
		nclog(L"\tCreated new mutex\n");
	}

	//init the rkeys struct with defaults
	initRKEYS();

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDS_WINDOWCLASSNAME/*IDC_iTimedReboot2*/, szWindowClass, MAX_LOADSTRING);

	//allow only one instance!
	HWND hWnd = FindWindow (NULL, szWindowClass);    
	if (hWnd) 
	{        
		//SetForegroundWindow (hWnd);            
		return -1;
	}

	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_iTimedReboot2);

	//should write a set of sample reg entries
	if (wcsstr(lpCmdLine, L"-writereg") != NULL)
		WriteReg();

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}

//=====================================================================================================
//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASS wndclass;
	wndclass.style			= CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc	= (WNDPROC)WndProc;
	wndclass.cbClsExtra		= 0;
	wndclass.cbWndExtra		= 0;
	wndclass.hInstance		= hInstance;
	wndclass.hIcon			= NULL; //not supported: LoadIcon(hInstance, (LPCTSTR)IDI_TRAYICON);
	wndclass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wndclass.lpszMenuName	= NULL              ;
	wndclass.lpszClassName	= szWindowClass;

	ATOM rc = RegisterClass(&wndclass);
	if (rc==0)
		ShowError(GetLastError());
	return rc;
}

//=====================================================================================================
//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;

	g_hInstance = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindow (szWindowClass, szTitle  ,   
		WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_OVERLAPPED,          // Style flags                         
		CW_USEDEFAULT,       // x position                         
		CW_USEDEFAULT,       // y position                         
		CW_USEDEFAULT,       // Initial width                         
		CW_USEDEFAULT,       // Initial height                         
		NULL,                // Parent                         
		NULL,                // Menu, must be null                         
		hInstance,           // Application instance                         
		NULL);               // Pointer to create

	if (!hWnd)
	{
		ShowError(GetLastError());
		return FALSE;
	}

	g_hwnd=hWnd;
	g_hInstance=hInstance;

	ShowWindow(hWnd, SW_HIDE);
	UpdateWindow(hWnd);

	// Add icon on system tray during initialzation
	AnimateIcon(hInstance, hWnd, NIM_ADD, ico_app);

	return TRUE;
}

//=================================================================================
//
//   FUNCTION:	WndProc(...)
//
//   PURPOSE:	the windows msg handler
//
//   COMMENTS:
//
LONG FAR PASCAL WndProc (HWND hwnd   , UINT message , 
						 UINT wParam , LONG lParam)                
{ 

	static HWND hButtonOK=NULL;
	static HWND hButtonCancel=NULL;
	switch (message)         
	{
	case WM_WININICHANGE:
		DEBUGMSG(1, (L"Got WM_MYTIMECHANGED mesg\n"));
		break;
	case WM_CREATE:
		ReadReg();

		nclog(L"WM_CREATE: szWindowClass: %s\n\n", szWindowClass);
		nclog(L"WM_CREATE: ReadReg:\r\n", NULL);
		for (int i=0; i<RegEntryCount; i++)
		{
			nclog(L"%s\t%s\n", rkeys[i].kname, rkeys[i].ksval );
		}

		//set the timer that checks the clock for reboot
		if (g_iRebootTimerCheck != 0)
			g_iTimerReboot = SetTimer(hwnd, ID_RebootTimeCheck, g_iRebootTimerCheck, NULL);

		if (g_iPingTimeInterval != 0)
			g_iTimerPing = SetTimer(hwnd, ID_PingIntervalTimer, g_iPingTimeInterval, NULL); 

		if(GetClientRect(hwnd, &rect)){
			hButtonOK = CreateWindow(L"BUTTON", L"eXit", WS_TABSTOP|WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON, 
				rect.left+10 , rect.bottom-30-10, //x, y pos
				60,30,	//width and height
				hwnd,
				(HMENU)IDC_BUTTON_OK,
				g_hInstance,
				NULL);
			hButtonCancel = CreateWindow(L"BUTTON", L"CANCEL", WS_TABSTOP|WS_VISIBLE|WS_CHILD|BS_DEFPUSHBUTTON, 
				rect.left+10+10+60 , rect.bottom-30-10, //x, y pos
				60,30,	//width and height
				hwnd,
				(HMENU)IDC_BUTTON_CANCEL,
				g_hInstance,
				NULL);
		}
		else
			DEBUGMSG(1, (L"#### GetWindowRect failed %i ####\n", GetLastError()));

		return 0;
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_BUTTON_OK:
			Shell_NotifyIcon(NIM_DELETE, &IconData);
			PostQuitMessage (0) ; 
			break;
		case IDC_BUTTON_CANCEL:
			ShowWindow(hwnd, SW_HIDE);
			break;
		}
		break;
	case WM_TIMER:
		switch (wParam)
		{
		case ID_RebootTimeCheck:
			//check the time against g_sRebootTime
			TimedReboot();
			break;
		case ID_PingIntervalTimer:
			//try a ping;
			TimedPing();
			break;
		}
		return 0;
		break;
	case WM_PAINT:
		PAINTSTRUCT ps;    
		RECT rect;    
		HDC hdc;  
		TCHAR str[MAX_PATH];
		// Adjust the size of the client rectangle to take into account    
		// the command bar height.    
		GetClientRect (hwnd, &rect);    
		hdc = BeginPaint (hwnd, &ps);
		wsprintf(str, L"%s loaded.\nDays interval: %i\nBoottime: %s\nLast Boot date: %s\nPing Target: %s\nLogging: %i\nTime check interval: %i\nPing time interval: %i", 
			szWindowClass,
			g_iRebootDays,
			g_sRebootTime,
			g_LastBootDate,
			g_sPingTarget, 
			g_bEnableLogging,
			g_iRebootTimerCheck,
			g_iPingTimeInterval ); 
		DrawText (hdc, str, -1, &rect,
			DT_CENTER /* |DT_VCENTER | DT_SINGLELINE*/);    
		EndPaint (hwnd, &ps);     
		return 0;
		break;
	case MYMSG_TASKBARNOTIFY:
		switch (lParam) {
	case WM_LBUTTONUP:
		SetWindowPos(hwnd, HWND_TOPMOST, 0,0,0,0, SWP_NOSIZE | SWP_NOREPOSITION | SWP_SHOWWINDOW);
		ShowWindow(hwnd, SW_MAXIMIZE);
		//if (MessageBox(hwnd, L"iTimedReboot2 is loaded. End Application?", szTitle , 
		//	MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL | MB_SETFOREGROUND | MB_TOPMOST)==IDYES)
		//{
		//	Shell_NotifyIcon(NIM_DELETE, &IconData);
		//	PostQuitMessage (0) ; 
		//}
		//ShowWindow(hwnd, SW_HIDE);
		}
		return 0;
		break;
	case WM_DESTROY:
		MessageBeep(MB_OK);

		//Kill the timers
		if (g_iRebootTimerCheck != 0)
			KillTimer(hwnd, g_iTimerReboot);
		if (g_iPingTimeInterval != 0)
			KillTimer(hwnd,g_iTimerPing);

		AnimateIcon(g_hInstance, hwnd, NIM_DELETE, ico_app);
		//force redraw of taskbar
		SendMessage(HWND_BROADCAST, WM_SETTINGCHANGE, NULL, NULL);
		PostQuitMessage (0); 
		return 0;
		break;
	}

	return DefWindowProc (hwnd , message , wParam , lParam) ;
}

//=====================================================================================================
//
//   FUNCTION:	WriteReg(...)
//
//   PURPOSE:	write defaults to registry
//
//   COMMENTS:
//
void WriteReg()
{
	int i;
	//	[HKEY_LOCAL_MACHINE\SOFTWARE\Intermec\iTimedReboot2]
	OpenCreateKey(g_regName);
	for (i=0; i<RegEntryCount; i++)
		RegWriteStr(rkeys[i].kname, rkeys[i].ksval );
	CloseKey();
}

//================================================================
//
//   FUNCTION:	ReadReg(...)
//
//   PURPOSE:	read the registry and place the vals in globals and the rkeys struct
//
//   COMMENTS:
//
int ReadReg()
{
	int i;
	TCHAR str[MAX_PATH+1];
	TCHAR name[MAX_PATH+1];
	LONG rc;
	if(OpenKey(g_regName)!=0){
		RETAILMSG(1, (L"Could not open registry key '%s'!\r\n", g_regName));
		//create default reg
		RETAILMSG(1, (L"Creating default registry keys '%s'!\r\n", g_regName));
		WriteReg();
		//second try
		if(OpenKey(g_regName)!=0){
			RETAILMSG(1, (L"2nd try: Could not open registry key '%s'!\r\n", g_regName));
			return -1;
		}
	}

	//enable logging?
	g_bEnableLogging = true;
	if (wcscmp(rkeys[EnableLogging].ksval, L"0") == 0)
		g_bEnableLogging=false;
	if(iTESTMODE==1)
		g_bEnableLogging=1;
	//set nclog global loggingEnabled var
	nclog_LogginEnabled=g_bEnableLogging;

	for (i=0; i<RegEntryCount; i++)
	{
		wsprintf(name, rkeys[i].kname);
		wsprintf(str, rkeys[i].ksval);
		rc = RegReadStr(name, str);
		if(rc == 0)
		{
			wsprintf(rkeys[i].ksval, L"%s", str);
		}
		else
		{
			wsprintf(rkeys[i].ksval, L"error in RegRead");
			ShowError(rc);
			nclog(L"Error in ReadReg-RegReadStr. Missing Key or value? Last key read: '%s'\n\t", rkeys[i].kname);
			break; //no reg vals !!!
		}
	}
	CloseKey();
#ifdef DEBUG
	for (i=0; i<RegEntryCount; i++)
	{
		DEBUGMSG(true, (rkeys[i].kname) );
		DEBUGMSG(true, (L"\t") );
		DEBUGMSG(true, (rkeys[i].ksval) );
		DEBUGMSG(true, (L"\r\n") );
	}
#endif
	//copy reg values to globals, more readable
	wsprintf(g_sPingTarget, rkeys[PingTarget].ksval);

	//check boot time interval
	//test if >30000
	if (_wtol(rkeys[Interval].ksval) > 30000)
	{	
		DEBUGMSG(true, (L"g_iRebootTimerCheck limited to 30000\n"));
		g_iRebootTimerCheck = 30000;
		nclog(L"g_iRebootTimerCheck limited to %i\n", g_iRebootTimerCheck);
	}
	else
	{
		g_iRebootTimerCheck = _wtoi(rkeys[Interval].ksval) * 1000;	 //timer uses milliseconds, registry has seconds
		nclog(L"Using Time Check Interval:\t%i seconds\n", g_iRebootTimerCheck/1000);
		DEBUGMSG(true,(str));
	}

	//process boot time (hh:mm)
	//convert the string to a systemtime
	SYSTEMTIME lt;
	memset(&lt, 0, sizeof(lt));
	GetLocalTime(&lt);								//need pre-filled time struct
	wcsncpy(str, rkeys[RebootTime].ksval, 2);				//get hour part of string
	str[2]=0;										//add termination \0
	lt.wHour = (ushort) _wtoi(str);							//convert to number
	wcsncpy(str, rkeys[RebootTime].ksval + 3, 2);			//get minutes part of string
	str[2]=0;
	lt.wMinute = (ushort) _wtoi(str);
	if( iTESTMODE==1){	//do not use a random time in TEST mode
		memcpy(&newTime, &lt, sizeof(SYSTEMTIME));
	}
	else{
		//get random time within timespan
		newTime = getRandomTime(lt);
	}
	//g_stRebootTime=lt;	//store RebootTime in global time var
	g_stRebootTime=newTime;	//store RebootTime including a random time offset
	nclog(L"Reboot time will be:\t%02i:%02i\n", newTime.wHour, newTime.wMinute); 
	//convert the newTime to a string 'hh:mm'
	wsprintf(rkeys[NewTime].ksval, L"%02i:%02i", newTime.wHour, newTime.wMinute);
	//store reboot time in global var
	wsprintf(g_sRebootTime, L"%s", rkeys[NewTime].ksval); 
	//save to reg for review
	OpenCreateKey(g_regName);
	RegWriteStr(L"newTime", g_sRebootTime);
	CloseKey();

	//ping interval
	//test if >30000
	if (_wtol(rkeys[PingInterval].ksval) > 30000)
	{
		DEBUGMSG(true, (L"g_iPingTimeInterval limited to 30000\n"));
		g_iPingTimeInterval = 30*1000;
		nclog(L"g_iPingTimeInterval limited to %i\n", g_iPingTimeInterval);
	}
	else
	{
		g_iPingTimeInterval = _wtoi(rkeys[PingInterval].ksval) * 1000; //timer uses milliseconds, registry has seconds
		wsprintf(str, L"Using Ping Interval:\t%i seconds\n", g_iPingTimeInterval/1000);
		nclog(L"Using Ping Interval:\t%i seconds\n", g_iPingTimeInterval/1000);
		DEBUGMSG(true,(str));
	}

	//lastBootDate
	memset(&g_stLastBootDateTime,0,sizeof(SYSTEMTIME));
	if (StrIsNumber(rkeys[LastBootDate].ksval))
	{
		wsprintf(g_LastBootDate, rkeys[LastBootDate].ksval);
		wsprintf(str, L"Last boot date:\t%s\n", g_LastBootDate);
		nclog(L"Last boot date:\t%s\n", g_LastBootDate);
		DEBUGMSG(true, (L"Found valid date string in reg\n"));

		//add newTime/g_sRebootTime to date string
		TCHAR szLasteDateTime[13];
		wsprintf(szLasteDateTime, L"%s%02i%02i", g_LastBootDate, newTime.wHour, newTime.wMinute);
		if(getSystemtimeOfString(szLasteDateTime, &g_stLastBootDateTime)){
			DEBUGMSG(true, (L"date string converted to SYSTEMTIME\n"));
		}
		else{
			DEBUGMSG(true, (L"date string conversion to SYSTEMTIME FAILED!\n"));
		}
	}
	else
	{
		DEBUGMSG(true, (L"### Error in date string in reg. Replaced by 19800101\n"));
		nclog(L"### Error in date string in reg. Replaced by 19800101\n", NULL);
		wsprintf(rkeys[LastBootDate].ksval, L"19800101");
		wsprintf(g_LastBootDate, rkeys[LastBootDate].ksval);

		if(getSystemtimeOfString(g_LastBootDate, &g_stLastBootDateTime)){
			DEBUGMSG(true, (L"date string converted to SYSTEMTIME\n"));
		}
		else{
			DEBUGMSG(true, (L"date string conversion to SYSTEMTIME FAILED!\n"));
		}
	}

	//do a reboot or start executable?
	wsprintf(g_ExeName, L"");
	wsprintf(g_ExeArgs, L"");
	if(wcscmp(rkeys[RebootExt].ksval, L"0")==0)
	{
		g_DoReboot=TRUE;
	}else if(wcscmp(rkeys[RebootExt].ksval, L"1")==0)
	{
		g_DoReboot=FALSE;
		wsprintf(g_ExeName, L"%s", rkeys[RebootExtApp].ksval);
		wsprintf(g_ExeArgs, L"%s", rkeys[RebootExtParms].ksval);
	}

	//days interval
	//test if >28 //4 weeks
	if (_wtol(rkeys[RebootDays].ksval) > 28)
	{
		DEBUGMSG(true, (L"g_iRebootDays limited to 28\n"));
		nclog(L"g_iRebootDays limited to 28. Now using %i.\n",1);
		g_iRebootDays = 1;
	}
	else
	{
		g_iRebootDays = _wtoi(rkeys[RebootDays].ksval); //days between reboots
		if(g_iRebootDays==0){
			nclog(L"reboot days interval calculated as 0. Changed to valid 1 value\n");
			g_iRebootDays=1;			
		}
		wsprintf(str, L"Using Days Interval:\t%i\n", g_iRebootDays);
		nclog( L"Using Days Interval:\t%i\n", g_iRebootDays );
		DEBUGMSG(true,(str));
		wsprintf( rkeys[RebootDays].ksval, L"%i", g_iRebootDays );
	}

	//TODO check if works OK
	//we need the date and time of when to reboot
	g_stRebootDateTime=addDays(g_stLastBootDateTime, g_iRebootDays);
	nclog(L"### next time to reboot: %04i%02i%02i %02i:%02i\n", 
		g_stRebootDateTime.wYear, g_stRebootDateTime.wMonth, g_stRebootDateTime.wDay,
		g_stRebootDateTime.wHour, g_stRebootDateTime.wMinute);

	return 0;
}

//=====================================================================================================
/*	Function Name   : AnimateIcon
Description		: Function which will act based on the message type that is received as parameter
like ADD, MODIFY, DELETE icon in the system tray. Also send a message to display
the icon in title bar as well as in the task bar application.
Function Called	: Shell_NotifyIcon	-	API which will Add, Modify, Delete icon in tray.
SendMessage - Send a message to windows
Variable		: NOTIFYICONDATA - Structure which will have the details of the tray icons
*/
void AnimateIcon(HINSTANCE hInstance, HWND hWnd, DWORD dwMsgType,UINT nIndexOfIcon)
{
	//HICON hIconAtIndex = LoadIcon(hInstance, (LPCTSTR) MAKEINTRESOURCE(IconResourceArray[nIndexOfIcon]));
	HICON hIconAtIndex=(HICON) LoadImage (g_hInstance, (LPCTSTR) MAKEINTRESOURCE(IconResourceArray[nIndexOfIcon]), IMAGE_ICON, 16,16,0);
	if (hIconAtIndex==0)
		ShowError(GetLastError());

	//NOTIFYICONDATA IconData;

	IconData.uID	= IconSlot;
	IconData.cbSize = sizeof(NOTIFYICONDATA);
	IconData.hIcon  = hIconAtIndex;
	IconData.hWnd   = hWnd;
	IconData.uCallbackMessage = MYMSG_TASKBARNOTIFY;
	IconData.uFlags = NIF_MESSAGE | NIF_ICON ;

	int rc = Shell_NotifyIcon(dwMsgType, &IconData);
	if (rc==0)
		ShowError(GetLastError());
	SendMessage(hWnd, WM_SETICON, NULL, (long) hIconAtIndex);

	if(hIconAtIndex)
		DestroyIcon(hIconAtIndex);
}

//=====================================================================================================
/*	Function Name   : StrIsNumber
Description		: Function which will if str contains a date like string
Variable		: str to test
*/
bool StrIsNumber(TCHAR *str)
{
	//convert str to long
	long l = _wtol(str);
	//print long back in string
	TCHAR s[MAX_PATH];
	wsprintf(s, L"%u", l);
	if (wcscmp(str, s)==0)
		return true;
	else
		return false;
}
