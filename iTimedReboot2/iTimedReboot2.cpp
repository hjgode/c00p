// iTimedReboot2.cpp : Defines the entry point for the application.
//

//history
//version	change
// 1.0		initial release
/*
			[HKEY_LOCAL_MACHINE\SOFTWARE\Intermec\iTimedReboot2]
			"Interval"="0"			//0=OFF, seconds between time checks, do not use >30000
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

#include "log2file.h"

#include "time.h"

#define		MAX_LOADSTRING			100

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
TCHAR			g_sPingTarget[MAX_PATH];		//target IP to ping
bool			g_bEnableLogging=false;
TCHAR			g_sEnableLogging[MAX_PATH];	//0=disable logging file, 1=enable

TCHAR			g_LastBootDate[MAX_PATH];

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
};

//number of entries in the registry
const int		RegEntryCount=9;				//how many entries are in the registry, with v2 changed from 6 to 9
TCHAR*			g_regName = L"Software\\Intermec\\iTimedReboot2";

//struct to hold reg names and values
typedef struct
{
	TCHAR kname[MAX_PATH];
	TCHAR ksval[MAX_PATH];
} rkey;
rkey			rkeys[RegEntryCount];	//an array with the reg settings

//things needed to request a warmboot
#include <winioctl.h>
extern "C" __declspec(dllimport) BOOL KernelIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
#define IOCTL_HAL_REBOOT CTL_CODE(FILE_DEVICE_HAL, 15, METHOD_BUFFERED, FILE_ANY_ACCESS)
extern "C" __declspec(dllimport) void SetCleanRebootFlag(void);

//predefined functions
void Log2File (TCHAR * t);
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
		Log2File(L"Rebooting now...");
		Sleep(1000);
		#ifndef DEBUG
			return KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
		#else
			Log2File(L"DEBUGMODE no warmboot");
		#endif
	}
	TCHAR str[MAX_PATH];
	PROCESS_INFORMATION pi;
	if(CreateProcess(g_ExeName, g_ExeArgs, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)==0){
		wsprintf(str, L"CreateProcess ('%s'/'%s') failed with 0x%08x\r\n", g_ExeName, g_ExeArgs, GetLastError());
		Log2File(str);
		return FALSE;
	}
	else{

		wsprintf(str, L"CreateProcess ('%s'/'%s') OK. pid=0x%08x\r\n", g_ExeName, g_ExeArgs, pi.dwProcessId);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		Log2File(str);
		return TRUE;
	}
}

//=====================================================================================================
//
//  FUNCTION:	Log2File(TCHAR)
//
//  PURPOSE:	Add text to the logfile if logging=true
//
//  COMMENTS:	none
//
void Log2File (TCHAR * t)
{
	if (g_bEnableLogging)
		Add2Log(t, false);
}

//=================================================================================
//
//  FUNCTION:	TimedReboot()
//
//  PURPOSE:	check time and date and reboot, if time is reached
//
//  COMMENTS:	none
//
void TimedReboot(void)
{
	SYSTEMTIME lt;
	memset(&lt, 0, sizeof(lt));
	GetLocalTime(&lt);	//lt is now the actual datetime

	//check if already booted this day
	TCHAR sDateNow[MAX_PATH];
	int er=0;
	long lDateNow, lDateBooted;
	//produce a mathematic date
	int rc = GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, &lt, L"yyyyMMdd", sDateNow, MAX_PATH-1);
	if (rc!=0)
	{
		DEBUGMSG(true, (sDateNow));
		DEBUGMSG(true, (L"\r\n"));
	}
	else
	{
		er=GetLastError();
		ShowError(er);
		DEBUGMSG(true, (L"TimedReboot: Error in GetDateFormat()\r\n"));
		Log2File(L"### TimedReboot: Error in GetDateFormat(). Exiting TimedReboot function.\n");
		return;
	}
	lDateNow=_wtol(sDateNow);
	lDateBooted=_wtol(g_LastBootDate);
	if (lDateNow > lDateBooted)
	{
		Log2File(L"__TimedReboot Check__\n\tDate now is greater than last boot date!\n");
		if (g_stRebootTime.wHour <= lt.wHour) 
		{
			Log2File(L"\tHour now is greater than hour to reboot!\n");
			if (g_stRebootTime.wMinute <= lt.wMinute)
			{
				Log2File(L"\tMinutes now is greater than minute to reboot!\n");
				DEBUGMSG(true, (L"\r\nREBOOT...\r\n"));
				//update registry with new boot date
 				RegWriteStr(rkeys[5].kname, sDateNow);
				wsprintf(rkeys[5].ksval, sDateNow);
				wsprintf(g_LastBootDate, sDateNow);
				AnimateIcon(g_hInstance, g_hwnd, NIM_MODIFY, ico_redbomb);
				Sleep(3000);
				Log2File(L"Will reboot now. Next reboot on:\n\t");
				Log2File(sDateNow);Log2File(L", ");
				Log2File(g_sRebootTime);
				Log2File(L"\n");


//				if (!g_bEnableLogging)		//we only boot if no logging is defined
					WarmBoot();				//ITCWarmBoot() was not used due to itc50.dll dependency
			}
		}
	}
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
	wsprintf(rkeys[0].kname, L"Interval");
	wsprintf(rkeys[0].ksval, L"30");
	wsprintf(rkeys[1].kname, L"RebootTime");
	wsprintf(rkeys[1].ksval, L"00:00");
	wsprintf(rkeys[2].kname, L"PingInterval");
	wsprintf(rkeys[2].ksval, L"60");
	wsprintf(rkeys[3].kname, L"PingTarget");
	wsprintf(rkeys[3].ksval, L"127.0.0.1");
	wsprintf(rkeys[4].kname, L"EnableLogging");
	wsprintf(rkeys[4].ksval, L"1");
	wsprintf(rkeys[5].kname, L"LastBootDate");
	wsprintf(rkeys[5].ksval, L"19800101");
	//new with v2
	wsprintf(rkeys[6].kname, L"RebootExt");
	wsprintf(rkeys[6].ksval, L"1");
	wsprintf(rkeys[7].kname, L"RebootExtApp");
	wsprintf(rkeys[7].ksval, L"\\Windows\\fexplore.exe");
	wsprintf(rkeys[8].kname, L"RebootExtParms");
	wsprintf(rkeys[8].ksval, L"\\Flash File Store");
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
	int rc = PingAddress(rkeys[3].ksval); //rc will be number of good pings
	if (rc==-1)
	{
		//Bad IP!
		AnimateIcon(g_hInstance, g_hwnd , NIM_MODIFY, ico_question);	//show a question mark
		Log2File(L"### Bad IP address\n");
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

	initRKEYS();

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDS_WINDOWCLASSNAME/*IDC_iTimedReboot2*/, szWindowClass, MAX_LOADSTRING);

	//allow only one instance!
	HWND hWnd = FindWindow (NULL, szWindowClass);    
	if (hWnd) 
	{        
		SetForegroundWindow (hWnd);            
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

  switch (message)         
  {
	case WM_CREATE:
		ReadReg();

		if (g_bEnableLogging)
			newfile(L"\\iTimedReboot2.log.txt");
		Log2File(szWindowClass);
		Log2File(L"\n\n");
		if (g_bEnableLogging)
			Log2File(L"ReadReg:\r\n");
		if (g_bEnableLogging)
		{
			for (int i=0; i<RegEntryCount; i++)
			{
				Log2File(rkeys[i].kname);
				Log2File(L"\t");
				Log2File(rkeys[i].ksval );
				Log2File(L"\n");
			}
		}

		//set the timer that checks the clock for reboot
		if (g_iRebootTimerCheck != 0)
		{
			g_iTimerReboot = SetTimer(hwnd, ID_RebootTimeCheck, g_iRebootTimerCheck, NULL);
		}
		if (g_iPingTimeInterval != 0)
			g_iTimerPing = SetTimer(hwnd, ID_PingIntervalTimer, g_iPingTimeInterval, NULL); 
		return 0;
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
		wsprintf(str, L"%s loaded.\nBoottime: %s\nLast Boot date: %s\nPing Target: %s\nLogging: %i\nTime check interval: %i\nPing time interval: %i", 
						szWindowClass,			
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
					if (MessageBox(hwnd, L"iTimedReboot2 is loaded. End Application?", szTitle , 
						MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL | MB_SETFOREGROUND | MB_TOPMOST)==IDYES)
					{
						Shell_NotifyIcon(NIM_DELETE, &IconData);
						PostQuitMessage (0) ; 
					}
					ShowWindow(hwnd, SW_HIDE);
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
		PostQuitMessage (0) ; 
		return 0            ;
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
		Log2File(L"Could not open registry key!\r\n");
		return -1;
	}
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
			Log2File(L"Error in ReadReg-RegReadStr. Missing Key or value? Last key read:\n\t");	
			Log2File(rkeys[i].kname);
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
	wsprintf(g_sPingTarget, rkeys[3].ksval);
	
	//check boot time interval
	//test if >30000
	if (_wtol(rkeys[0].ksval) > 30000)
	{	
		DEBUGMSG(true, (L"g_iRebootTimerCheck limited to 30000\n"));
		Log2File(L"g_iRebootTimerCheck limited to 30000\n");
		g_iRebootTimerCheck = 30000;
	}
	else
	{
		g_iRebootTimerCheck = _wtoi(rkeys[0].ksval) * 1000;	 //timer uses milliseconds, registry has seconds
		wsprintf(str, L"Using Time Check Interval:\t%i seconds\n", g_iRebootTimerCheck/1000);
		Log2File(str);
		DEBUGMSG(true,(str));
	}

	//convert the string to a systemtime
	SYSTEMTIME lt;
	memset(&lt, 0, sizeof(lt));
	GetLocalTime(&lt);								//need pre-filled time struct
	wcsncpy(str, rkeys[1].ksval, 2);				//get hour part of string
	str[2]=0;										//add termination \0
	lt.wHour = (ushort) _wtoi(str);							//convert to number
	wcsncpy(str, rkeys[1].ksval + 3, 2);			//get minutes part of string
	str[2]=0;
	lt.wMinute = (ushort) _wtoi(str);
	g_stRebootTime=lt;	//store RebootTime in global time var
	wsprintf(str, L"Reboot time will be:\t%00i:%00i\n", lt.wHour, lt.wMinute);  
	Log2File(str);
	DEBUGMSG(true,(str));
	wsprintf(g_sRebootTime, L"%s", rkeys[1].ksval); 

	//ping interval
	//test if >30000
	if (_wtol(rkeys[2].ksval) > 30000)
	{
		DEBUGMSG(true, (L"g_iPingTimeInterval limited to 30000\n"));
		Log2File(L"g_iPingTimeInterval limited to 30000\n");
		g_iPingTimeInterval = 30*1000;
	}
	else
	{
		g_iPingTimeInterval = _wtoi(rkeys[2].ksval) * 1000; //timer uses milliseconds, registry has seconds
		wsprintf(str, L"Using Ping Interval:\t%i seconds\n", g_iPingTimeInterval/1000);
		Log2File(str);
		DEBUGMSG(true,(str));
	}
	//enable logging?
	g_bEnableLogging = true;
	if (wcscmp(rkeys[4].ksval, L"0") == 0)
		g_bEnableLogging=false;
	
	//lastBootDate
	if (StrIsNumber(rkeys[5].ksval))
	{
		wsprintf(g_LastBootDate, rkeys[5].ksval);
		wsprintf(str, L"Using date:\t%s\n", g_LastBootDate);
		Log2File(str);
		DEBUGMSG(true, (L"Found valid date string in reg\n"));
	}
	else
	{
		DEBUGMSG(true, (L"### Error in date string in reg. Replaced by 19800101\n"));
		Log2File(L"### Error in date string in reg. Replaced by 19800101\n");
		wsprintf(rkeys[5].ksval, L"19800101");
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
