// iLock5.cpp : Defines the entry point for the application.
//
/*
	see history.txt
*/
#pragma warning (disable: 4244)

//custom DEBUG coding?
#undef MYDEBUG

//disable warnings
//performance warning
//#pragma warning( disable : 4800 ) // 4101 )

#include "stdafx.h"
#include "ver_info.h"
#include "iLock5.h"
#include <windows.h>
#include <winver.h>
#include <commctrl.h>
#include "locktaskbar.h"
#include "registry.h"
#include "nclog.h"

#include "getIPtable.h"

#define szPRODUCTVERSION	"version 5.4.1.4"
#define szPRODUCTVERSIONW	L"version 5.4.1.4"
#pragma comment (user , szPRODUCTVERSION)	//see also iLock5ppc.rc !

#ifndef SH_CURPROC
	#define SH_CURPROC      2	//for WaitApiReady
#endif

//for testing we signal when we finished our job
#define ILOCK_READY_EVENT L"iLockReady"
HANDLE hiLockReady=NULL;

//START stop watchdog code
#define STOPEVENTNAME L"STOPILOCK"
#define WM_USER_STOP_ILOCK WM_USER + 4711
DWORD _stopEventWaitThreadID=0;
HANDLE _hStopEventHandle=NULL;
HANDLE _threadWaitStopEventThreadHandle=NULL;
HANDLE _hStopThread=NULL;
//END stop watchdog code

//fix memory problem on WM5 and later, see http://social.msdn.microsoft.com/forums/en-US/vssmartdevicesnative/thread/e91d845d-d51e-45ad-8acf-737e832c20d0/
#ifndef TH32CS_SNAPNOHEAPS
#define TH32CS_SNAPNOHEAPS = 0x40000000
#endif

#define MAX_PROCESSLISTCOUNT_FAILURES	3	//hack for failing CreateToolhelp32Snapshot

//if the listview is clicked the timers are paused
#define LISTVIEWPAUSES
#undef LISTVIEWPAUSES

#define USEMENUBAR
//#undef USEMENUBAR

#define USESHFULLSCREEN
#undef USESHFULLSCREEN

#define USELOCKDESKTOP
//#undef USELOCKDESKTOP		//test!

//documentation in exe file
#ifdef USEMENUBAR
	#pragma comment(user, "USEMENUBAR defined")
#else
	#pragma comment(user, "USEMENUBAR undefined")
#endif
#ifdef USESHFULLSCREEN
	#pragma comment(user, "USESHFULLSCREEN defined")
#else
	#pragma comment(user, "USESHFULLSCREEN undefined")
#endif
#ifdef USELOCKDESKTOP
	#pragma comment(user, "USELOCKDESKTOP defined")
#else
	#pragma comment(user, "USELOCKDESKTOP undefined")
#endif

#define ILOCKBMP L"\\Windows\\iLock5.bmp"


#ifdef APIWAIT
	// for up to wince 5 #include "pkfuncs.h"
	#include "kfuncs.h" //windows mobile 6
	#pragma comment (lib, "coredll.lib")
#endif

#include "sipapi.h"

TCHAR szVersion[MAX_PATH];

#define timer1 1001	//the locker timer
#define timer2 1002 //this will look for the process and window to wait for
#define timer3 1003 //this will automatically exit this app
#define timer4 1004 //this timer will be started to enable a Reboot button
#define timer1intervall 500
#define timer2intervall 5000
#define timer3intervall 1000
#define timer4intervall 1000
#define TIMER3COUNT 10	//default for WaitBeforeExit
int TIMER4COUNT = 60;	//default for ShowRebootTimeout

//watchdog
DWORD pWatchdog=NULL;
BOOL stopWatchdog = FALSE;
#define ILOCK_WATCHDOG WM_USER + 815

//CleanBoot
//This is used the same way the warm and cold boot IOCTLS are used.
#include "oemioctl.h"
#include "pwinreg.h"

//=============== serial number stuff
#include "iphlpapi.h"
#pragma comment (lib, "iphlpapi.lib")


//===================================
//#include <winioctl.h>
#define IOCTL_HAL_REBOOT CTL_CODE(FILE_DEVICE_HAL, 15, METHOD_BUFFERED, FILE_ANY_ACCESS)
extern "C" __declspec(dllimport) void SetCleanRebootFlag(void);

#define IOCTL_CLEAN_BOOT		CTL_CODE(FILE_DEVICE_HAL, 4006, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DllImport __declspec(dllimport)
extern "C" DllImport BOOL KernelIoControl(unsigned long	 dwIoControlCode, 
										  void*			 lpInBuf, 
										  unsigned long	 nInBufSize, 
										  void*			 lpOutBuf, 
										  unsigned long	 nOutBufSize, 
										  unsigned long* lpBytesReturned
										 );
/*
#ifndef CSIDL_PROFILE
	#define CSIDL_PROFILE                   0x0028
#endif
*/

// Global Variables:
static bool			bAppIsRunning=false;
//static bool			LoginFound=false;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE			hInst;			// current instance
HWND				g_hWndMenuBar;		// menu bar handle
HWND				g_hWnd = NULL;	//global main window handle

#ifdef USEMENU
HWND				hwndCB;			// The command bar handle
#endif
//new with v3.1
TCHAR App2waitFor[255];				// the exe name of the app we wait for
TCHAR Title2waitFor[255];			// the title of the window we wait for
TCHAR Class2waitFor[255];			// the class of the window we wait for
int UseFullScreen=0;				// do we have to run in fullscreen mode? HHTaskbar/Title visible or not
int KeepUseFullScreenOnExit=0;		// restore HHTaskbar on exit?
int KeepLockedAfterExit=0;			// shall we leave and keep device locked?
int MaximizeTargetOnExit=0;			// shall we maximize the target window
int UseMenuBar=0;					// shall we show a menu?
int UseDesktopLock=0;				// shall we lock the desktop during runtime?
int FreeDesktopOnExit=1;			// shall we free the desktop on exit?
int WaitBeforeExit=TIMER3COUNT;		// how long should we wait before exit
DWORD TextColor=0x00000000;			// text color to use for list messages
DWORD TextBackColor=0x00ffffff;		// text back color used in listview
DWORD TextListColor=0x00ffffff;		// back color of listview
BOOL bAdminExit=false;				// disable all locking on admin exit
int iHotSpotX=120;					// x-pos of hotspot on screen for doubleclick exit
int iHotSpotY=160;					// y-pos of hotspot on screen for doubleclick exit
									//hotspot DBL_CLICK is recognized within 10 pixels around hotspot
int iUseLogging=0;					// enable logging to file and socket
BOOL bDebugMode=FALSE;				// disable SetTopWindow, enables you to view background

BOOL bRebootExt=FALSE;				// use external app instead of direct warmboot
TCHAR szRebootExtApp[MAX_PATH]=L"\\windows\\fexplore.exe";	//external app to start instead of warmboot
TCHAR szRebootExtParms[MAX_PATH]=L"\\My Documents";	// args for external 'warmboot' application

DWORD ipListXpos=40;
DWORD ipListYpos=60;
DWORD ipListEnabled=1;
DWORD ipListWidth=160;

DWORD progListXpos=20;	//0x14
DWORD progListYpos=200;	//0xC8
DWORD progListWidth=200;

RECT theRect; //store screen size

bool foundSetupWindow=false;
HWND hSetupWindow = NULL;

HINSTANCE g_hInstance=NULL;
int lvNextItem=1;
//new with 5.2.0.0
int g_iScreenWidth=240;
int g_iScreenHeight=320;

// Our DDB handle is a global variable.
HBITMAP hbm;
// HANDLE hbm;

//the password entered in password dialog
TCHAR pw[]=L"................";

// Forward declarations of functions included in this code module:
ATOM			MyRegisterClass(HINSTANCE, LPTSTR);
BOOL			InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	Password		(HWND, UINT, WPARAM, LPARAM);

//######################################################################################################
void toLower(TCHAR *pStr)
{
	while(*pStr++ = _totlower(*pStr));
}

//======================================================================================================
BOOL DoReboot(){
	if(!bRebootExt)
		return KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
	else{
		if(szRebootExtApp!=NULL){
			TCHAR str[MAX_PATH];
			PROCESS_INFORMATION pi;
			if(CreateProcess(szRebootExtApp, szRebootExtParms, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)==0){
				wsprintf(str, L"CreateProcess ('%s'/'%s') failed with 0x%08x\r\n", szRebootExtApp, szRebootExtParms, GetLastError());
				nclog(str);
				return FALSE;
			}
			else{

				wsprintf(str, L"CreateProcess ('%s'/'%s') OK. pid=0x%08x\r\n", szRebootExtApp, szRebootExtParms, pi.dwProcessId);
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
				nclog(str);
				return TRUE;
			}
		}
		else //szRebootExtApp!=NULL
			return FALSE;
	}
}

void ReadRegistry(void)
{
	nclog(L"iLock5: ReadRegistry\r\n");

	RegReadStr(L"App2waitFor", App2waitFor);
	if (wcslen(App2waitFor)==0){
		*App2waitFor=NULL;
		nclog(L"iLock5: App2waitFor=''\r\n");
	}
	else
		nclog(L"iLock5: App2waitFor='%s'\r\n", App2waitFor);

	RegReadStr(L"Title2waitFor", Title2waitFor);
	if (wcslen(Title2waitFor)==0){
		*Title2waitFor=NULL;
		nclog(L"iLock5: Title2waitFor=''\r\n");
	}
	else
		nclog(L"iLock5: Title2waitFor='%s'\r\n", Title2waitFor);

	RegReadStr(L"Class2waitFor", Class2waitFor);
	if (wcslen(Class2waitFor)==0){
		*Class2waitFor=NULL;
		nclog(L"iLock5: Class2waitFor=''\r\n");
	}
	else
		nclog(L"iLock5: Class2waitFor='%s'\r\n", Class2waitFor);

	TCHAR tstr[255];
	RegReadStr(L"UseFullScreen", tstr);
	if (wcscmp(tstr, L"1")==0)
		UseFullScreen=1;
	else
		UseFullScreen=0;
	
	KeepUseFullScreenOnExit=0;
	RegReadStr(L"KeepUseFullScreenOnExit", tstr);
	if (wcscmp(tstr, L"1")==0)
		KeepUseFullScreenOnExit=1;
	else
		KeepUseFullScreenOnExit=0;
	
	RegReadStr(L"KeepLockedAfterExit", tstr);
	if (wcscmp(tstr, L"1")==0)
		KeepLockedAfterExit=1;
	else
		KeepLockedAfterExit=0;
	nclog(L"iLock5: KeepLockedAfterExit='%i'\r\n", KeepLockedAfterExit);

	RegReadStr(L"MaximizeTargetOnExit", tstr);
	if (wcscmp(tstr, L"1")==0)
		MaximizeTargetOnExit=1;
	else
		MaximizeTargetOnExit=0;
	nclog(L"iLock5: MaximizeTargetOnExit='%i'\r\n", MaximizeTargetOnExit);

	RegReadStr(L"UseDesktopLock", tstr);
	if (wcscmp(tstr, L"1")==0)
		UseDesktopLock=1;
	else
		UseDesktopLock=0;
	nclog(L"iLock5: UseDesktopLock='%i'\r\n", UseDesktopLock);

	RegReadStr(L"FreeDesktopOnExit", tstr);
	if (wcscmp(tstr, L"1")==0)
		FreeDesktopOnExit=1;
	else
		FreeDesktopOnExit=0;
	nclog(L"iLock5: FreeDesktopOnExit='%i'\r\n", FreeDesktopOnExit);

	RegReadStr(L"UseMenuBar", tstr);
	if (wcscmp(tstr, L"1")==0)
		UseMenuBar=1;
	else
		UseMenuBar=0;
	nclog(L"iLock5: UseMenuBar='%i'\r\n", UseMenuBar);
	
	DWORD dwVal=10;
	if(RegReadDword(L"WaitBeforeExit", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>0 && dwVal<31)
			WaitBeforeExit=dwVal;
		else
			WaitBeforeExit=TIMER3COUNT;
	}
	else
		WaitBeforeExit=TIMER3COUNT;
	nclog(L"iLock5: WaitBeforeExit='%i'\r\n", WaitBeforeExit);

	//text and listview colors
	dwVal=TextColor;
	if(RegReadDword(L"TextColor", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal<0x01000000)
			TextColor=dwVal;
		else
			TextColor=0x00000000;
	}
	else
		TextColor=0x00000000;
	nclog(L"iLock5: TextColor='%08x'\r\n", TextColor);

	dwVal=TextBackColor;
	if(RegReadDword(L"TextBackColor", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal<0x01000000)
			TextBackColor=dwVal;
		else
			TextBackColor=0x00FFFFFF;
	}
	else
		TextBackColor=0x00FFFFFF;
	nclog(L"iLock5: TextBackColor='%08x'\r\n", TextBackColor);

	dwVal=TextListColor;
	if(RegReadDword(L"TextListColor", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal<0x01000000)
			TextListColor=dwVal;
		else
			TextListColor=0x00FFFFFF;
	}
	else
		TextListColor=0x00FFFFFF;
	nclog(L"iLock5: TextListColor='%08x'\r\n", TextListColor);

	int screenXmax=GetSystemMetrics(SM_CXSCREEN); //240;
	int screenYmax=GetSystemMetrics(SM_CYSCREEN); //320;
	nclog(L"iLock5: ScreenMetrics='%ix%i'\r\n", screenXmax, screenYmax);

	//hotspot location
	dwVal=120;
	if(RegReadDword(L"HotSpotX", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>10 && dwVal<(DWORD)screenXmax)
			iHotSpotX=dwVal;
		else
			iHotSpotX=screenXmax/2;
	}
	else
		iHotSpotX=screenXmax/2;
	nclog(L"iLock5: iHotSpotX='%i'\r\n", iHotSpotX);
	
	dwVal=160;
	if(RegReadDword(L"HotSpotY", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>10 && dwVal<(DWORD)screenYmax)
			iHotSpotY=dwVal;
		else
			iHotSpotY=screenYmax/2;
	}
	else
		iHotSpotY=screenYmax;
	nclog(L"iLock5: iHotSpotY='%i'\r\n", iHotSpotY);

	//ShowRebootTimeout
	dwVal=60;
	if(RegReadDword(L"ShowRebootTimeout", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>=0 && dwVal<601)
			TIMER4COUNT=dwVal;
		else
			TIMER4COUNT=60;
	}
	else
		TIMER4COUNT=60;

	//enable Logging
	dwVal=0;
	if(RegReadDword(L"UseLogging", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal==0)
			iUseLogging = 0;
		else
			iUseLogging = 1;
	}
	else
		iUseLogging = 0;

	//enable Logging
	dwVal=0;
	if(RegReadDword(L"UseSocket", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal==0)
			nclogDisableSocket(TRUE);
		else
			nclogDisableSocket(FALSE);
	}
	else
		nclogDisableSocket(TRUE);

	//enable DebugMode
	dwVal=0;
	if(RegReadDword(L"DebugMode", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal==0)
			bDebugMode = FALSE;
		else{
			bDebugMode = TRUE;
			UseMenuBar=1;
			UseFullScreen=0;
		}
	}
	else
		bDebugMode = FALSE;

	//RebootExt
	dwVal=0;
	if(RegReadDword(L"RebootExt", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal==1)
			bRebootExt = TRUE;
		else
			bRebootExt = FALSE;
	}
	else
		bRebootExt = FALSE;
	nclog(L"iLock5: RebootExt=%i\r\n", bRebootExt);

	//RebootExtApp
	TCHAR szTemp[MAX_PATH];
	RegReadStr(L"RebootExtApp", szTemp);
	if (wcslen(szTemp)==0){
		*szRebootExtApp=NULL;
		nclog(L"iLock5: RebootExtApp=NULL\r\n");
	}
	else{
		wsprintf(szRebootExtApp, L"%s", szTemp);
		nclog(L"iLock5: RebootExtApp='%s'\r\n", szRebootExtApp);
	}

	//RebootExtParms
	wsprintf(szTemp, L"");
	RegReadStr(L"RebootExtParms", szTemp);
	if (wcslen(szTemp)==0){
		*szRebootExtParms=NULL;
		nclog(L"iLock5: RebootExtParms=NULL\r\n");
	}
	else{
		wsprintf(szRebootExtParms, L"%s", szTemp);
		nclog(L"iLock5: RebootExtParms='%s'\r\n", szRebootExtParms);
	}

	//IP list pos X
	dwVal=0;
	if(RegReadDword(L"ipListXpos", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>0)
			ipListXpos = dwVal;
		else
			ipListXpos = 40;
	}
	else
		ipListXpos = 40;
	nclog(L"iLock5: ipListXpos=%i\r\n", ipListXpos);

	//IP list pos Y
	dwVal=0;
	if(RegReadDword(L"ipListYpos", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>0)
			ipListYpos = dwVal;
		else
			ipListYpos = 60;
	}
	else
		ipListYpos = 60;
	nclog(L"iLock5: ipListYpos=%i\r\n", ipListYpos);

	//ipListEnabled, default = 1
	dwVal=0;
	if(RegReadDword(L"ipListEnabled", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>0)
			ipListEnabled = 1;
		else
			ipListEnabled = 0;
	}
	else
		ipListEnabled = 1;
	nclog(L"iLock5: ipListEnabled=%i\r\n", ipListEnabled);

	//IP list width
	dwVal=0;
	if(RegReadDword(L"ipListWidth", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>0)
			ipListWidth = dwVal;
	}
	nclog(L"iLock5: ipListWidth=%i\r\n", ipListWidth);

	//progress list pos X
	dwVal=0;
	if(RegReadDword(L"progListXpos", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>0)
			progListXpos = dwVal;
	}
	else
		progListXpos = 20;
	nclog(L"iLock5: progListXpos=%i\r\n", progListXpos);

	//progress list pos Y
	dwVal=0;
	if(RegReadDword(L"progListYpos", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>0)
			progListYpos = dwVal;
	}
	nclog(L"iLock5: progListYpos=%i\r\n", progListYpos);

	//progress list width
	dwVal=0;
	if(RegReadDword(L"progListWidth", &dwVal)==ERROR_SUCCESS)
	{
		if(dwVal>0)
			progListWidth = dwVal;
	}
	nclog(L"iLock5: progListWidth=%i\r\n", progListWidth);

#ifdef MYDEBUG
	TIMER4COUNT=3;
	iUseLogging=1;
#endif
}

DWORD WINAPI watchdogSTOP(LPVOID param){
	HWND hWndMain=(HWND) param;
	_hStopEventHandle	=	CreateEvent(NULL, TRUE, FALSE, STOPEVENTNAME);	//create a MANUAL reset event handle
	_hStopThread		=	CreateEvent(NULL, FALSE, FALSE, L"STOP iLock stop watchdog");
	if(_hStopEventHandle==NULL)
		return -1;
	HANDLE myHandles[2];
	myHandles[0]=_hStopEventHandle;
	myHandles[1]=_hStopThread;
	DWORD dwWait;
	BOOL bExitThread=FALSE;
	nclog(L"watchdogSTOP: thread started\r\n");
	do{
		dwWait = WaitForMultipleObjects(2, myHandles, FALSE, INFINITE);
		switch(dwWait){
			case WAIT_OBJECT_0:
				nclog(L"watchdogSTOP: event signaled\r\n");
				//send stop message
				SendMessage(hWndMain, WM_USER_STOP_ILOCK, 0, 0);
				ResetEvent(_hStopEventHandle);
				break;
			case WAIT_OBJECT_0+1:
				//stop thread
				nclog(L"watchdogSTOP: exit thread requested\r\n");
				bExitThread=TRUE;
				break;
			case WAIT_ABANDONED:
				nclog(L"watchdogSTOP: WAIT_ABANDONED\r\n");
				break;
			case WAIT_FAILED:
				nclog(L"watchdogSTOP: WAIT_FAILED\r\n");
				break;
			case WAIT_TIMEOUT:
				nclog(L"watchdogSTOP: WAIT_TIMEOUT\r\n");
				break;
			default:
				nclog(L"watchdogSTOP: default\r\n");
				break;
		}
		Sleep(3000);
	}while(!bExitThread);
	nclog(L"watchdogSTOP: thread exit\r\n");
	return 0;
}

int startSTOPwatchdog(HWND hWndMain){
	_threadWaitStopEventThreadHandle = CreateThread(NULL, 0, watchdogSTOP, hWndMain, 0, &_stopEventWaitThreadID);
	if(_threadWaitStopEventThreadHandle==NULL)
		nclog(L"start of stop watchdog failed\r\n");
	else
		nclog(L"start of stop watchdog OK\r\n");

	return 0;
}

int stopSTOPwatchdog(){
	if(_hStopThread!=NULL){
		nclog(L"setting stop STOP watchdog\r\n");
		SetEvent(_hStopThread);
	}
	return 0;
}

DWORD WINAPI watchdogThread(LPVOID param){
	nclog(L"Watchdog Thread started\r\n");
	HWND hWin = (HWND) param;
	int iC=0;
	do{
		iC++;
		SendMessage(hWin, ILOCK_WATCHDOG, (WPARAM) iC, (LPARAM)0);
		Sleep(1000);
		nclog(L"iLock5: Watchdog live sign. hWnd=0x%08x\r\n", hWin);
	}while(!stopWatchdog);
	nclog(L"iLock5: Watchdog Thread ended\r\n");
	return 0;
}

//######### wait for API
#define SH_GDI 16
#define SH_WMGR 17
#define SH_SHELL 21
// IsAPIReady tells whether the specified API set has been registered
extern "C" BOOL IsAPIReady(DWORD hAPI);
void waitforAPIready(){
	do{
		Sleep(1000);
		DEBUGMSG(1, (L"Waiting for API (SH_WMGR, SH_GDI, SH_SHELL) ready...\n"));
		//repeat until all are true;
	}while( !IsAPIReady(SH_WMGR) && !IsAPIReady(SH_GDI) && !IsAPIReady(SH_SHELL));
	DEBUGMSG(1, (L"...done waiting for API ready\n"));
}
//######### end wait for API

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR    lpCmdLine,
                   int       nCmdShow)
{
	//block until APIs are ready
	waitforAPIready();

	ReadRegistry();
	if(iUseLogging==1)
		nclogEnable(TRUE);
	else
		nclogEnable(FALSE);

	nclog(L"iLock5: WinMain\r\n");
	nclog(L"iLock5: ShowRebootTimeout='%i'\r\n", TIMER4COUNT);
	MSG msg;

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow)) 
	{
		nclog(L"iLock5: InitInstance failed. END!\r\n");
		return FALSE;
	}
	g_hInstance=hInstance;

	HWND hwndDesktop = GetDesktopWindow();
	if(hwndDesktop!=INVALID_HANDLE_VALUE){
		RECT deskRect;
		GetWindowRect(hwndDesktop, &deskRect);
		g_iScreenWidth = deskRect.right - deskRect.left;
		g_iScreenHeight=deskRect.bottom - deskRect.top;
	}

	HACCEL hAccelTable;
	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ILOCK5));
	nclog(L"iLock5: SignalStarted(%i)\r\n", _ttoi(lpCmdLine));
	SignalStarted(_ttoi(lpCmdLine)); //return the command line number as started signal

	nclog(L"iLock5: Entering msg loop\r\n");
	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
ATOM MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass)
{
	WNDCLASS wc;

	wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ILOCK5));
	wc.hCursor       = 0;
	wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = szWindowClass;

	return RegisterClass(&wc);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
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
	nclog(L"iLock5: InitInstance\r\n");
    HWND hWnd;
    TCHAR szTitle[MAX_LOADSTRING];		// title bar text
    TCHAR szWindowClass[MAX_LOADSTRING];	// main window class name

    hInst = hInstance; // Store instance handle in our global variable

    // SHInitExtraControls should be called once during your application's initialization to initialize any
    // of the device specific controls such as CAPEDIT and SIPPREF.
    SHInitExtraControls();

    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING); 
    LoadString(hInstance, IDC_ILOCK5, szWindowClass, MAX_LOADSTRING); //ILOCK5

    //If it is already running, then focus on the window, and exit
    hWnd = FindWindow(szWindowClass, szTitle);	
    if (hWnd) 
    {
		nclog(L"iLock5: InitInstance: Found previous iLock5, switching over and ending new instance\r\n");
        // set focus to foremost child window
        // The "| 0x00000001" is used to bring any owned windows to the foreground and
        // activate them.
        SetForegroundWindow((HWND)((ULONG) hWnd | 0x00000001));
        return 0;
    } 

    if (!MyRegisterClass(hInstance, szWindowClass))
    {
		nclog(L"iLock5: InitInstance: MyRegisterClass failed. END!\r\n");
    	return FALSE;
    }

	theRect.right = GetSystemMetrics(SM_CXSCREEN);
	theRect.bottom = GetSystemMetrics(SM_CYSCREEN);
	theRect.top=0;
	theRect.left=0;
	
    hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE,
        0, 
		0, 
		theRect.right, 
		theRect.bottom, 
		NULL, NULL, hInstance, NULL);

	//switch fullscreen or not
	//if (UseFullScreen==1){
	//	//For FULL screen enable following line
	//	HideTaskbar(false);//HideTaskbar(true);
	//	hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE /*| WS_CAPTION*/, 0, 0, 240, 320, NULL, NULL, hInstance, NULL);
	//}
	//else{	
	//	HideTaskbar(false);
	//	//for normal screen size, enable following line
	//	#if defined(WIN32_PLATFORM_PSPC)
	//	#pragma comment(user, "Using WIN32_PLATFORM_PSPC\n")
	//			hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE, 0, 0+26, 240, 320-26-26, NULL, NULL, hInstance, NULL);
	//	#else
	//			hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE, 0, 0, 240, 320-26, NULL, NULL, hInstance, NULL);
	//	#endif
	//}

    if (!hWnd)
    {
		nclog(L"iLock5: InitInstance: CreateWindow failed. END!\r\n");
        return FALSE;
    }
	g_hWnd=hWnd;

#ifdef USEMENUBAR
	nclog(L"iLock5: InitInstance: USEMENUBAR...\r\n");
	if(UseMenuBar==1){
		// When the main window is created using CW_USEDEFAULT the height of the menubar (if one
		// is created is not taken into account). So we resize the window after creating it
		// if a menubar is present
		if (g_hWndMenuBar)
		{
			nclog(L"iLock5: InitInstance: Moving window because of UseMenuBar\r\n");
			RECT rc;
			RECT rcMenuBar;

			GetWindowRect(hWnd, &rc);
			GetWindowRect(g_hWndMenuBar, &rcMenuBar);
			rc.bottom -= (rcMenuBar.bottom - rcMenuBar.top);
			
			MoveWindow(hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
		}
	}
#else
	nclog(L"iLock5: InitInstance: USEMENUBAR undefined...\r\n");
	if(UseMenuBar==0){
		nclog(L"iLock5: InitInstance: Moving window without MenuBar\r\n");
		//move window to upper left logical corner
		MoveWindow(hWnd, 0, 0, theRect.right, theRect.bottom, FALSE);
	}
#endif

	LockTaskbar(true);


#ifdef USESHFULLSCREEN
	if(UseFullScreen==1)
	{
	nclog(L"iLock5: InitInstance: USESHFULLSCREEN defined...\r\n");
	//remove some elements from default window
	SHFullScreen(hWnd, SHFS_HIDETASKBAR | SHFS_HIDESIPBUTTON | SHFS_HIDESTARTICON);
	//remove OK or (X) from window
	SHDoneButton(hWnd, SHDB_HIDE);
	//resize and position
	RECT rc;
	SetRect(&rc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    MoveWindow(hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
	}
#else
	if(UseFullScreen==1)
	{
		//hide the taskbar, resize main window and move
		nclog(L"iLock5: InitInstance: Moving/Resizing window because of UseFullScreen\r\n");
		RECT rc;
		RECT rcMenuBar;
		RECT rcTaskbar;

		HWND hWndTaskbar = FindWindow(L"HHTaskbar", NULL);
		GetWindowRect(hWndTaskbar, &rcTaskbar);			// {top=0 bottom=26 left=0 right=240}
		HideTaskbar(TRUE);

		GetWindowRect(hWnd, &rc);						// {top=0 bottom=294 left=0 right=240}
		if(UseMenuBar==1){
			GetWindowRect(g_hWndMenuBar, &rcMenuBar);	// {top=294 bottom=320 left=0 right=240}
		}
		HideTaskbar(TRUE);
		if(!bDebugMode)
			SetWindowPos(hWnd, HWND_TOPMOST, rc.left, rc.top, rc.right, rc.bottom, SWP_SHOWWINDOW);
		//MoveWindow(hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
	}
	else{
		RECT rc;
		RECT rcMenuBar;
		RECT rcTaskbar;
		HWND hWndTaskbar = FindWindow(L"HHTaskbar", NULL);
		GetWindowRect(hWndTaskbar, &rcTaskbar);			// {top=294 bottom=320 left=0 right=240}
		GetWindowRect(hWnd, &rc);						// {top=0 bottom=294 left=0 right=240}
		if(UseMenuBar==1){
			GetWindowRect(g_hWndMenuBar, &rcMenuBar);	// {top=294 bottom=320 left=0 right=240}
		}
		HideTaskbar(FALSE);
		
		if(!bDebugMode)
			SetWindowPos(hWnd, HWND_TOPMOST, rc.left, rc.top, rc.right, rc.bottom, SWP_SHOWWINDOW);
	}
#endif

	////show above all windows?
	//DWORD dwexstyle = ::GetWindowLong(hWnd, GWL_EXSTYLE);
	//dwexstyle |= WS_EX_ABOVESTARTUP;
	//SetWindowLong(hWnd, GWL_EXSTYLE, dwexstyle); 

	//hide the sip
	ShowSIP(false);
	nclog(L"iLock5: end of InitInstance\r\n");

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    return TRUE;
}

//=========================================================================================
//
void DoCleanBoot()
{
	unsigned long BytesReturned;
	//does NOT clean the registry!
	KernelIoControl(IOCTL_CLEAN_BOOT, 
				NULL, 
				0, 
				NULL,
				0, 
				&BytesReturned);
}
//=========================================================================================
//
LRESULT ClearList(HWND hList)
{
	PostMessage(hList, LVM_DELETEALLITEMS , 0, 0); //Clear List
	lvNextItem=1;
	return 0;
}

//=========================================================================================
//
LRESULT Add2List(HWND hList, TCHAR *text)
{
	if (hList == NULL)
		return -1; //missing handle to list
	TCHAR t[MAX_PATH];
	//Clear List
	//ClearList(hList);
	LVITEM LvItem;
	memset(&LvItem,0,sizeof(LvItem)); // Zero struct's Members
	LvItem.mask=LVIF_TEXT;   // Text Style
	LvItem.cchTextMax = MAX_PATH; // Max size of test
	LvItem.iItem=lvNextItem;          // choose item  
	LvItem.iSubItem=0;       // Put in first coluom
	wsprintf(t, L"%s", text);
	LvItem.pszText = t; //pe.szExeFile; // Text to display (can be from a char variable) (Items)
	//ListView_InsertItem(hList, &LvItem);
	SendMessage(hList,LVM_INSERTITEM,0,(LPARAM)&LvItem); // Send info to the Listview
	lvNextItem++;
	
	ListView_EnsureVisible (hList, ListView_GetItemCount (hList)-1, FALSE);
	//ListView_EnsureVisible (hList, indexToBeVisible, FALSE);
	return 0;
}

/*  returns 1 iff str ends with suffix  */
int str_ends_with(const TCHAR * str, const TCHAR * suffix) {

  if( str == NULL || suffix == NULL )
    return 0;

  size_t str_len = wcslen(str);
  size_t suffix_len = wcslen(suffix);

  if(suffix_len > str_len)
    return 0;

  return 0 == wcsnicmp( str + str_len - suffix_len, suffix, suffix_len );
}

//=========================================================================================
//
LRESULT FindProcess(TCHAR * ExeName)
{
	static int failedProcessListCount=0;	//counter for failing CreateToolhelp32Snapshot() calls

	bool ExeRunning=false;
	TCHAR testName[64];
	//make a snapshot for all processes and find the matching processID
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPNOHEAPS, 0);
	if (hSnap != NULL)
	{
	  nclog(L"iLock5: Processlist\r\n");
	  nclog(L"iLock5: \tProcess\r\n");
	  PROCESSENTRY32 pe;
	  pe.dwSize = sizeof(PROCESSENTRY32);
	  if (Process32First(hSnap, &pe))
	  {
		do
		{
			//does the proc send with '.exe'?
			if(!str_ends_with(pe.szExeFile, L".exe"))
				wsprintf(testName, L"%s.exe", pe.szExeFile);	//add '.exe' to name
			else
				wsprintf(testName, L"%s", pe.szExeFile);
			nclog(L"iLock5: \t%s\r\n", testName);
			//check if target application is running
			if (wcsicmp(ExeName, testName)==0)
			{
				ExeRunning = true;
			}
			//======================================
		} while (Process32Next(hSnap, &pe));
		nclog(L"iLock5: Process32Next done!\r\n");
	  }//processFirst
	  else{
		  nclog(L"iLock5: Process32First returned false!\r\n");
		  failedProcessListCount++;
		  if(failedProcessListCount>=MAX_PROCESSLISTCOUNT_FAILURES){
			nclog(L"iLock5: Process32First failed %i times, failover and continue without process verification\r\n", MAX_PROCESSLISTCOUNT_FAILURES);
			//return 1;	//return a fake success
			ExeRunning=true;
		  }
	  }
	}//hSnap
	else{
		nclog(L"iLock5: CreateToolhelp32Snapshot failed. LastError=0x%08x. Returning -1\r\n", GetLastError());
		failedProcessListCount++;
		if(failedProcessListCount>=MAX_PROCESSLISTCOUNT_FAILURES){
			nclog(L"iLock5: CreateToolhelp32Snapshot failed %i times, failover and continue without process verification\r\n", MAX_PROCESSLISTCOUNT_FAILURES);
			//return 1;	//return a fake success
			ExeRunning=true;
		}
		//return -1; //error getting snapshot
	}
	nclog(L"iLock5: CloseToolhelp32Snapshot...\r\n");
	CloseToolhelp32Snapshot(hSnap);
	if (ExeRunning){
		nclog(L"iLock5: FindProcess() return 1 for '%s'\r\n", ExeName);
		return 1; //found the exe
	}
	else{
		nclog(L"iLock5: FindProcess() return 0 for '%s'\r\n", ExeName);
		return 0; //exe not found
	}
}

//======================================================================
TCHAR caption[MAX_PATH];
wchar_t* pCaption = caption;

TCHAR classname[MAX_PATH];
wchar_t* pClassname = classname;

TCHAR *setupwindowtext = L"setup";
TCHAR *installwindowtext = L"installing";
TCHAR *showwintext = L"showwin";

TCHAR* szInstallerTexts[] = { L"setup", L"installing", L"showwin", NULL };

BOOL isSetupWindow(TCHAR* szClass, TCHAR* szCaption, HWND hwndTest){
	toLower(szCaption);
	toLower(szClass);
	int i=0;
	do{
		if( wcsstr(szCaption, szInstallerTexts[i])!= NULL || 
			wcsstr(szClass, szInstallerTexts[i]) != NULL
			){
			return TRUE;
		}
		i++;
	}while(szInstallerTexts[i]!=NULL);
	return NULL;
}

BOOL CALLBACK EnumWindowsProc2(HWND hwnd, LPARAM lParam)
{
	TCHAR classname[MAX_PATH];
	TCHAR caption[MAX_PATH];
	GetClassName(hwnd, classname, MAX_PATH);
    DEBUGMSG(1, (L"Class name: '%s'\n",classname));
	if(wcsicmp(classname, L"ILOCK5")==0){	//wcsicmp returns 0 for identical strings!
		//enumWindows will crash on getWindowText on self window!
		DEBUGMSG(1, (L"enumWindows found iLock, skipping GetWindowText...\n"));
		//hwnd=GetWindow(hwnd, GW_HWNDNEXT);
		//return TRUE;	//walk on to next main window
		wsprintf(caption, L"iLock5"); //dummy action
	}
	else
		GetWindowText(hwnd, caption, MAX_PATH);
    DEBUGMSG(1, (L"Window title: '%s'\n", caption));
	if(hwnd!=NULL){
		if(isSetupWindow(classname, caption, hwnd)){
			hSetupWindow=hwnd;
			DEBUGMSG(1, (L"### found installer ###\n"));
			return FALSE;//stop enum
		}
	}
	return TRUE;//continue with enum
}

//======================================================================
// Look for windows starting with "Installing... or "Setup ..." and let them come to front
int ShowInstallers()
{
	hSetupWindow=NULL;	//reset
	DEBUGMSG(1, (L"ShowInstallers()...\n"));
	//EnumWindows continues until the last top-level window 
	//is enumerated or the callback function returns FALSE
	BOOL beWin = EnumWindows(EnumWindowsProc2, NULL);
	if(hSetupWindow!=NULL){
		SetTopWindow(hSetupWindow);
		return 1;
	}
	return 0;

}

RECT * getScreenSize(RECT * rect){
	HWND hTaskbar=FindWindow(L"HHTaskBar", NULL);
	if(hTaskbar!=NULL){
		int cx = GetDeviceCaps(GetDC(hTaskbar), HORZRES);
		int cy = GetDeviceCaps(GetDC(hTaskbar), VERTRES);
		rect->top=0;
		rect->left=0;
		rect->right=cx;
		rect->bottom=cy;
	}
	else{
		int iW = GetSystemMetrics(SM_CXSCREEN);
		int iH = GetSystemMetrics(SM_CYSCREEN);
		rect->top=0;
		rect->left=0;
		rect->right=iW;
		rect->bottom=iH;
	}
	return rect;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    HDC hdc;
    //static SHACTIVATEINFO s_sai;
	
	//HGO
	LRESULT a;
	static HWND hProgress=NULL;
	static HWND hProcList=NULL;
	static HWND hIPList=NULL;
	static int iShowIP=10;	//only update IP list every ten seconds
	int iIPcount=0;
	static HWND hBtnReboot=NULL; //handle to reboot button
	TCHAR tstr[2*MAX_PATH];
//	HWND hMenuBar;
//	HWND hTaskbar1;

	HDC hdcMem;//bmp
	HGDIOBJ hOldSel; //bmp
//	TCHAR szHello[MAX_LOADSTRING];
	static bool bFirstRun=true;
	int x, y;
#ifdef LISTVIEWPAUSES
	LV_DISPINFO *lvinfo;
#endif
	static int iCount3=0;
	static int Timer4Count=0; //to track timer4 events

	int screenXmax=GetSystemMetrics(SM_CXSCREEN); //240;
	int screenYmax=GetSystemMetrics(SM_CYSCREEN); //320;
	int xButton;
	int yButton;

	RECT rectScreen;
	DWORD dwC=0;
	UINT uResult;               // SetTimer's return value 
	
	LOGFONT logFont;
	HFONT hFontSmall;

	nclog(L"iLock5: Inside MsgLoop. hWnd=0x%08x\r\n", hWnd);
    switch (message) 
    {
		case WM_USER_STOP_ILOCK:	//gracefully close
			nclog(L"iLock5: WM_USER_STOP_ILOCK...\r\n");
			if(pWatchdog!=0){
				nclog(L"iLock5: WM_USER_STOP_ILOCK. Stopping Watchdog...\r\n");
				stopWatchdog=TRUE;
				Sleep(1000);
			}
			else
				nclog(L"iLock5: WM_USER_STOP_ILOCK. No Watchdog to stop\r\n");

			nclog(L"iLock5: WM_USER_STOP_ILOCK. Killing timers 1, 2 and 4.\r\n");
			KillTimer (hWnd, timer1);
			KillTimer (hWnd, timer2);
			KillTimer (hWnd, timer4);

			//stop stopWatchdog thread
			stopSTOPwatchdog();			

#ifdef USEMENUBAR
			if(UseMenuBar==1){
				CommandBar_Destroy(g_hWndMenuBar);
			}
#endif
#ifdef USESHFULLSCREEN
			SHFullScreen(hWnd, SHFS_SHOWTASKBAR | SHFS_SHOWSIPBUTTON | SHFS_SHOWSTARTICON);
			SHDoneButton(hWnd, SHDB_SHOW); // added with v1.1
#endif
//			if(bAdminExit){
				LockDesktop(false);
				LockTaskbar(false);
				MaximizeTargetOnExit=0;
				HideTaskbar(false);
//			}
			AllKeys(false);

			//stop thread
			if(_hStopEventHandle!=NULL)
				SetEvent(_hStopEventHandle);

            PostQuitMessage(0);
			nclog(L"iLock5: WM_USER_STOP_ILOCK. END\r\n");			
			return 0;
			break;
		case ILOCK_WATCHDOG:
			dwC = wParam;
			nclog(L"iLock5: Watchdog message: %i\r\n", dwC);
			PostMessage(hProgress, PBM_STEPIT, 0, 0);
			return 0;	//5.1.8.1 return messgage has been processed
        case WM_COMMAND:
			nclog(L"iLock5: Within WM_COMMAND\r\n");
            wmId    = LOWORD(wParam); 
            wmEvent = HIWORD(wParam); 
			if(lParam==(LPARAM)hBtnReboot){
				if(MessageBox(hWnd, L"Are you sure?", L"Confirm Warmboot", 
					MB_YESNO | MB_ICONHAND | MB_DEFBUTTON2 | 
					MB_APPLMODAL | MB_SETFOREGROUND | MB_TOPMOST)==IDYES){
					DoReboot();
					return 0;	//5.1.8.1 return messgage has been processed
				}
			}
            // Parse the menu selections:
            switch (wmId)
            {
                case IDM_HELP_ABOUT:
                    DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, About);
                    return 0;	//5.1.8.1 return messgage has been processed
                case IDM_OK:
					nclog(L"iLock5: Within password dialog. Stopping timers.\r\n");
					KillTimer(hWnd, timer1);
					KillTimer(hWnd, timer2);
					KillTimer(hWnd, timer4);
				   //Passwortabfrage nur bei MENUE
					a = DialogBox(hInst, (LPCTSTR)IDD_PWBOX, hWnd,  (DLGPROC)Password);
					if (a == IDCANCEL)
					{
						MessageBeep(MB_ICONEXCLAMATION);
						nclog(L"iLock5: Password dialog canceled. Starting timers.\r\n");
						SetTimer (hWnd, timer1, timer1intervall, NULL) ;
						SetTimer (hWnd, timer2, timer2intervall, NULL) ; //refresh process list all 2 seconds
						if(TIMER4COUNT!=0)
							SetTimer (hWnd, timer4, timer4intervall, NULL) ; //enable reboot button timeout
						//redraw
						UpdateWindow(hWnd);
						//return DefWindowProc(hWnd, message, wParam, lParam); //5.1.8.0
					}
					else	//5.1.8.0 added else
						PostMessage (hWnd, WM_CLOSE, 0, 0);				
                    return 0;	//5.1.8.1 return messgage has been processed
				case IDC_LVIEW:
					return 0;	//5.1.8.1 return messgage has been processed
                default:
                    //return DefWindowProc(hWnd, message, wParam, lParam); //5.1.8.0
					return 0;	//5.1.8.1 return messgage has been processed
            }
            break;
        case WM_CREATE:
//			getScreenSize(&rectScreen);
			//GetClientRect(hWnd, &rectScreen);
			GetWindowRect(hWnd, &rectScreen);
			screenXmax=rectScreen.right;
			screenYmax=rectScreen.bottom;
			RETAILMSG(1, (L"xMax, yMax = %i, %i\r\n", screenXmax, screenYmax));
			AllKeys(true);
#ifdef USEMENUBAR
			if(UseMenuBar==1){
				SHMENUBARINFO mbi;

				memset(&mbi, 0, sizeof(SHMENUBARINFO));
				mbi.cbSize     = sizeof(SHMENUBARINFO);
				mbi.hwndParent = hWnd;
				mbi.nToolBarId = IDR_MENU;
				mbi.hInstRes   = hInst;

				if (!SHCreateMenuBar(&mbi)) 
				{
					g_hWndMenuBar = NULL;
				}
				else
				{
					g_hWndMenuBar = mbi.hwndMB;
				}
			}
#endif
            // Initialize the shell activate info structure
            //memset(&s_sai, 0, sizeof (s_sai));
            //s_sai.cbSize = sizeof (s_sai);

			// The bitmap should be stored as a resource in the exe file.
			// We pass the hInstance of the application, and the ID of the
			// bitmap to the LoadBitmap API function and it returns us an
			// HBITMAP to a DDB created from the resource data.
			// HINSTANCE hInstance = GetWindowInstance(hWnd);
			hbm = SHLoadDIBitmap(ILOCKBMP);
			if (hbm == NULL)
				hbm = LoadBitmap(hInst,MAKEINTRESOURCE(IDB_BITMAP1));
			//create progressbar
			hProgress = CreateWindowEx(0, PROGRESS_CLASS, NULL,
						   WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
					  20*(screenXmax/240),  //x with some scaling
					  40*(screenYmax/320),	//y 
					  screenXmax-40*(screenXmax/240),	//g_iScreenWidth,//-40, //200, screenXmax-40, //width, 
					  20*(screenYmax/320), //height
					  hWnd, NULL, hInst, NULL);
			if (hProgress != NULL)
			{
				PostMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(1, 100)); // set range from 0 to 100
				PostMessage(hProgress, PBM_SETSTEP, (WPARAM) 5, 0);
			}

			if(ipListEnabled==1){
				//Create a listview report for IP addresses
				hIPList = CreateWindowEx(0, WC_LISTVIEW, NULL,
								WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | 
								LVS_NOCOLUMNHEADER, // | LVS_SORTDESCENDING,
								ipListXpos *(screenXmax/240.0),	//x pos
								ipListYpos *(screenYmax/320.0),// 235,  //y pos
								//screenXmax - (screenXmax/240)* 80,// 200, //width
								(screenXmax/240.0)* ipListWidth,// 200, //width
								(screenYmax/320)*320/5,// 60,		//height
								hWnd, 
								//NULL, //hMenu or child window identifier zB 
								(HMENU)IDC_LVIEW, //hMenu or child window identifier
								hInst, NULL);

				if(hIPList!=NULL){
					// set font
					//hFontSmall = (HFONT)GetStockObject(SYSTEM_FONT);
					memset(&logFont, 0, sizeof(LOGFONT));
					logFont.lfHeight=12 * (screenXmax/240);
					wsprintf(logFont.lfFaceName, L"Tahoma");
					hFontSmall=CreateFontIndirect(&logFont);
					SendMessage(hIPList, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

					//listenzeilenfarbe
					ListView_SetTextBkColor(hIPList, TextBackColor);
					//boxhintergrundfarbe
					ListView_SetBkColor(hIPList, TextListColor);
					//textvordergrundfarbe
					ListView_SetTextColor(hIPList, TextColor);

					//Add one column
					LVCOLUMN LvCol; // Make Coluom struct for ListView
					memset(&LvCol,0,sizeof(LvCol)); // Reset Coluom
					LvCol.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; // Type of mask
					//LvCol.cx=screenXmax - (screenXmax/240)*80;//200;
					LvCol.cx=(screenXmax/240.0)*ipListWidth;			// width between each coloum
					LvCol.pszText=L"IP address";                    // First Header
					SendMessage(hIPList,LVM_INSERTCOLUMN,0,(LPARAM)&LvCol); // Insert/Show the coloum
#if DEBUG
					Add2List(hIPList, L"127.0.0.1");
#endif
				}
			}
			//Create a listview report for messages
			hProcList = CreateWindowEx(0, WC_LISTVIEW, NULL,
							WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | 
							LVS_NOCOLUMNHEADER, // | LVS_SORTDESCENDING,
							progListXpos*(screenXmax/240.0),	//x pos
							progListYpos*(screenYmax/320.0),// 235,  //y pos
							//screenXmax - (screenXmax/240)*progListWidth,// 40, //width
							(screenXmax/240.0)*progListWidth,// 40, //width
							(screenYmax/320)*320/5,// 60,		//height
							hWnd, 
							//NULL, //hMenu or child window identifier zB 
							(HMENU)IDC_LVIEW, //hMenu or child window identifier
							hInst, NULL);
			if (hProcList != NULL)
			{
				// set font
				//hFontSmall = (HFONT)GetStockObject(SYSTEM_FONT);
				memset(&logFont, 0, sizeof(LOGFONT));
				logFont.lfHeight=12 * (screenXmax/240);
				wsprintf(logFont.lfFaceName, L"Tahoma");
				hFontSmall=CreateFontIndirect(&logFont);
				SendMessage(hProcList, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

				//setup colors of listview
				//ListView_SetBkColor(hProcList, 0x001F1F1F);
				//ListView_SetTextColor(hProcList, 0x00FFFFFF);
				
				//DWORD colafarbe = 0x002B1CE2;
				//listenzeilenfarbe
				ListView_SetTextBkColor(hProcList, TextBackColor);
				//boxhintergrundfarbe
				ListView_SetBkColor(hProcList, TextListColor);
				//textvordergrundfarbe
				ListView_SetTextColor(hProcList, TextColor);

				//Add one column
				LVCOLUMN LvCol; // Make Coluom struct for ListView
                memset(&LvCol,0,sizeof(LvCol)); // Reset Coluom
				LvCol.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; // Type of mask
				//LvCol.cx=screenXmax - (screenXmax/240)*progListWidth;//40;  // width between each coloum
				LvCol.cx=(screenXmax/240.0)*progListWidth;// width between each coloum
				LvCol.pszText=L"Item";                     // First Header
				SendMessage(hProcList,LVM_INSERTCOLUMN,0,(LPARAM)&LvCol); // Insert/Show the coloum
			
				//read productversion from resources!
				TCHAR szVersionInfo[MAX_PATH];
				TCHAR szProductName[MAX_PATH];
				myGetFileVersionInfo(hInst, szVersionInfo, szPRODUCTVERSIONW);
				myGetFileProductNameInfo(NULL, szProductName);
				wsprintf(szVersion, L"%s %s", szProductName, szVersionInfo);

				wsprintf(tstr, L"iLock %s started", szVersion);

				Add2List(hProcList, tstr);
				//Add2List(hProcList, L"Waiting for application...");
				if(App2waitFor!=NULL)
					wsprintf(tstr, L"Waiting for: '%s'...", App2waitFor);
				else
					wsprintf(tstr, L"missing App2WaitFor from reg");
				Add2List(hProcList, tstr);
			}
			//Start the timers
			nclog(L"iLock5: WM_CREATE. Starting timers 1 and 2.\r\n");
			uResult = SetTimer (hWnd, timer1, timer1intervall, NULL) ;  //relock screen all .5 seconds
			if(uResult==0)
				DEBUGMSG(1, (L"setTimer failed for Timer1: %i\n", GetLastError()));
			else
				DEBUGMSG(1, (L"setTimer OK for Timer1: %i\n", uResult));

			uResult = SetTimer (hWnd, timer2, timer2intervall, NULL) ; //refresh process list all 5 seconds
			if(uResult==0)
				DEBUGMSG(1, (L"setTimer failed for Timer2: %i\n", GetLastError()));
			else
				DEBUGMSG(1, (L"setTimer OK for Timer2: %i\n", uResult));

			//create a button for reboot
			xButton = (screenXmax/2)-(100/2);
			yButton = 40*(screenYmax/320) + (screenYmax/320)*320/5 + 26;//based on progressbar pos and height
			hBtnReboot = CreateWindow(
				L"BUTTON", /* this makes a "button" */
				L"Reboot", /* this is the text which will appear in the button */
				WS_CHILD, // WS_VISIBLE | WS_CHILD,
				xButton, // screenXmax/2-100/2,//	5,		// x /* these four lines are the position and dimensions of the button */
				yButton, // screenYmax/320*65,		// y
				100,	// width
				screenYmax/320*26,		// height
				hWnd, /* this is the buttons parent window */
				NULL, //(HMENU)IDB_CLASS_OPTIONS, /* these next two lines pretty much tell windows what to do when the button is pressed */
				NULL, //(HINSTANCE)GetWindowLong(insert, GWL_HINSTANCE),
				NULL);
			if(hBtnReboot!=NULL){
				if(TIMER4COUNT!=0){
					SetTimer(hWnd, timer4, timer4intervall, NULL); //start the reboot button timer
					nclog(L"iLock5: WM_CREATE. Starting timer 4 (RebootButton).\r\n");
				}else
					nclog(L"iLock5: WM_CREATE. RebootButton disabled.\r\n");

			}

			//start a thread that enables EXIT of iLock
			startSTOPwatchdog(hWnd);

			//start watchdog
			if (CreateThread(NULL, 0, watchdogThread, (LPVOID) hWnd, 0, &pWatchdog))
				nclog(L"iLock5: Watchdog thread created\r\n");
			else
				nclog(L"iLock5: Watchdog thread creation failed\r\n");

			//for TESTING signal when ready
			hiLockReady = CreateEvent(NULL, TRUE, FALSE, ILOCK_READY_EVENT);
			if(hiLockReady!=NULL){
				if(GetLastError()==ERROR_ALREADY_EXISTS){
					//something wrong, the name should not exist
					nclog(L"iLock5: Ready signal event handle already exists!\r\n");
				}
				else{
					nclog(L"iLock5: New ready signal event handle created.\r\n");
				}
			}
			else{
				nclog(L"iLock5: Ready signal event handle creation failed: 0x%08x\r\n", GetLastError());
			}

			return 0;	//5.1.8.1 return messgage has been processed
	   case WM_NOTIFY:
#ifdef LISTVIEWPAUSES
			if ((int)wParam == IDC_LVIEW)
			{ //ListView
				nclog(L"iLock5: WM_NOTIFY: ListView clicked\r\n");
				lvinfo = (LV_DISPINFO *)lParam;
				switch (lvinfo->hdr.code) 
				{
					case NM_CLICK:
						//Pause the timers
						nclog(L"iLock5: NM_CLICK. Pausing timers 1 and 2 and 4.\r\n");
						SetTimer(hWnd, timer1, INFINITE, NULL);
						SetTimer(hWnd, timer2, INFINITE, NULL);
						SetTimer(hWnd, timer4, INFINITE, NULL);
						//wait 5 seconds
						Sleep(5000);
						//restart the timers
						nclog(L"iLock5: NM_CLICK. Starting timers 1 and 2 and 4.\r\n");
						SetTimer(hWnd, timer1, timer1intervall, NULL);
						SetTimer(hWnd, timer2, timer2intervall, NULL);
						SetTimer(hWnd, timer4, timer4intervall, NULL);
						break;
				}
			}
		   return 0;	//5.1.8.1 return messgage has been processed
#endif
        case WM_PAINT:
			PAINTSTRUCT ps;
			hdc = BeginPaint(hWnd, &ps);
            
            // TODO: Add any drawing code here...
			// Calling BeginPaint clears the update region that was set by calls
			// to InvalidateRect(). Once the update region is cleared no more
			// WM_PAINT messages will be sent to the window until InvalidateRect
			// is called again.
			//hdc = BeginPaint(hWnd,&ps);

			// To paint with a DDB it first needs to be associated
			// with a memory device context. We make a DC that
			// is compatible with the screen by passing NULL to
			// CreateCompatibleDC.
			// Then we need to associate our saved bitmap with the
			// device context.

			hdcMem = CreateCompatibleDC(NULL);


			//HBITMAP hbmT = ::SelectBitmap(hdcMem,hbm);
			// Select the bitmap into the compatible device context.
			hbm = SHLoadDIBitmap(ILOCKBMP);
			if (hbm == NULL)
				hbm = LoadBitmap(hInst,MAKEINTRESOURCE(IDB_BITMAP1));

			hOldSel = SelectObject(hdcMem, hbm);
			// Now, the BitBlt function is used to transfer the contents of the 
			// drawing surface from one DC to another. Before we can paint the
			// bitmap however we need to know how big it is. We call the GDI
			// function GetObject to get the relevent details.
			BITMAP bm;
			GetObject(hbm,sizeof(bm),&bm);

			//BitBlt(hdc,0,0,bm.bmWidth,bm.bmHeight,hdcMem,0,0,SRCCOPY);
			RECT rect;
			GetWindowRect(hWnd, &rect);
			//GetClientRect(hWnd, &rect);
			StretchBlt(hdc, 
				rect.left, rect.top, rect.right, rect.bottom,	//target rectangle pos and width/height
				hdcMem,
				0,
				0,
				bm.bmWidth,
				bm.bmHeight,
				SRCCOPY);
			//BitBlt(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hdcMem, 0, 0, SRCCOPY);

			// Now, clean up. A memory DC always has a drawing
			// surface in it. It is created with a 1X1 monochrome
			// bitmap that we saved earlier, and need to put back
			// before we destroy it.
			//SelectBitmap(hdcMem,hbmT);
			// Draw some text on top of bitmap
			////RECT rt;
			////GetClientRect(hWnd, &rt);
			////rt.top=rt.bottom / 3 * 2;
			////LoadString(hInst, IDS_HELLO, szHello, MAX_LOADSTRING);
			////DrawText(hdc, szHello, _tcslen(szHello), &rt, 
			////	// DT_SINGLELINE | DT_VCENTER | 
			////	DT_CENTER);
			// Restore original bitmap selection and destroy the memory DC.
			SelectObject (hdcMem, hOldSel);
			DeleteDC(hdcMem);
			// EndPaint balances off the BeginPaint call.
            
            EndPaint(hWnd, &ps);
			nclog(L"iLock5: WM_PAINT. Leaving\r\n");
            return 0;	//5.1.8.1 return messgage has been processed

        case WM_DESTROY:
			nclog(L"iLock5: WM_DESTROY. Killing timers 1, 2 and 4.\r\n");
			if(pWatchdog!=0){
				nclog(L"iLock5: WM_DESTROY. Stopping Watchdog...\r\n");
				stopWatchdog=TRUE;
				Sleep(1000);
			}
			else
				nclog(L"iLock5: WM_DESTROY. No Watchdog to stop\r\n");

			KillTimer (hWnd, timer1);
			KillTimer (hWnd, timer2);
			KillTimer (hWnd, timer4);

			//stop stopWatchdog thread
			stopSTOPwatchdog();

#ifdef USEMENUBAR
			if(UseMenuBar==1){
				CommandBar_Destroy(g_hWndMenuBar);
			}
#endif
#ifdef USESHFULLSCREEN
			SHFullScreen(hWnd, SHFS_SHOWTASKBAR | SHFS_SHOWSIPBUTTON | SHFS_SHOWSTARTICON);
			SHDoneButton(hWnd, SHDB_SHOW); // added with v1.1
#endif
/*
			hMenuBar = SHFindMenuBar(hWnd);
			if(hWnd!=NULL)
				EnableWindow(hMenuBar, true);

			LockTaskbar(false);	
			HideTaskbar(false);
*/
			// It is good practice to destroy all GDI objects when one is
			// finished with them. Usually the sooner the better. Bitmaps
			// can easilly expand to several megabytes of memory: A DDB is
			// stored in the format of the device it is compatible with:
			// On a 32 bit display a 256 color bitmap would be stored with
			// four bytes per pixel.
			DeleteObject(hbm);
			//bmp
			
			if (KeepLockedAfterExit==0 && !bAdminExit)
			{
				LockTaskbar(false);
			}
			else
			{
				LockTaskbar(true);
			}

#ifdef USELOCKDESKTOP
			if(UseDesktopLock==1 && !bAdminExit){
				//FreeDesktopOnExit
				if(FreeDesktopOnExit==1)
					LockDesktop(false);
				else
					LockDesktop(true);
			}
#endif
/*
			if (UseFullScreen==0)
				HideTaskbar(false);
			else
				HideTaskbar(true);
*/
			if(bAdminExit){
				LockDesktop(false);
				LockTaskbar(false);
				MaximizeTargetOnExit=0;
				HideTaskbar(false);
			}

			if ( (MaximizeTargetOnExit==1) && (wcslen(Title2waitFor)>0) )
				MaximizeWindow(Title2waitFor); //Maximize window

			AllKeys(false);
			if(KeepUseFullScreenOnExit==0){
				if(UseFullScreen)
					HideTaskbar(false);
			}
            PostQuitMessage(0);
			nclog(L"iLock5: WM_DESTROY. END\r\n");
            return 0;	//5.1.8.1 return messgage has been processed

        case WM_ACTIVATE:
            // Notify shell of our activate message
            //SHHandleWMActivate(hWnd, wParam, lParam, &s_sai, FALSE);
            break;
        case WM_SETTINGCHANGE:
            //SHHandleWMSettingChange(hWnd, wParam, lParam, &s_sai);
            return 0;	//5.1.8.1 return messgage has been processed
		case WM_TIMER:
				nclog(L"iLock5: WM_TIMER. Entering...\r\n");
			     switch (wParam)
				 {
					case timer1:
						nclog(L"iLock5: WM_TIMER. Timer1 proc...\r\n");
#ifdef MYDEBUG
						LockTaskbar(false);
#else
						LockTaskbar(true);
#endif
						if (UseFullScreen==1) //changed with 4.0.03
							HideTaskbar(true); //added with v3.3 as ck60 did not hide taskbar and menu was invisble
						else
							HideTaskbar(false);
#ifdef USELOCKDESKTOP
						if(UseDesktopLock==1)
							LockDesktop(true);
						else
							LockDesktop(false);
#endif
						foundSetupWindow=false;
						if (ShowInstallers()==0)
							SetTopWindow(hWnd);

						PostMessage(hProgress, PBM_STEPIT, 0, 0);

						if(ipListEnabled==1){
							if(iShowIP==10){	//update IP list all 10 seconds only
								//build and show IP list
								TCHAR strIPlist[10][64];
								//for(iIPcount=0; iIPcount<10; iIPcount++)
								//	strIPlist[iIPcount]=new TCHAR(64);
								iIPcount=getIpTable(strIPlist);
								ListView_DeleteAllItems(hIPList);
								for(int i=0; i<iIPcount; i++)
									Add2List(hIPList, strIPlist[i]);
								//for(iIPcount=0; iIPcount<10; iIPcount++)
								//	delete strIPlist[iIPcount];
								iShowIP=0;
							}
							iShowIP++;
						}

						return 0;	//5.1.8.1 return messgage has been processed
					case timer2:
						nclog(L"iLock5: WM_TIMER. Timer2 proc...\r\n");
						//ListProcesses(hProcList);
						//bAppIsRunning checks for running evm.exe
						if (!bAppIsRunning)
						{
							if (FindProcess(App2waitFor)==1) //( (FindProcess(L"EVM.EXE")==1) && !bFirstRun )
							{
								bAppIsRunning=true;
								if (bFirstRun)
								{
									//Add2List(hProcList, L"Application found");
									//Add2List(hProcList, L"Waiting for window...");
									if((Class2waitFor!=NULL) && (Title2waitFor!=NULL))
										wsprintf(tstr, L"Waiting for: '%s':'%s'...", Class2waitFor, Title2waitFor);
									else if((Class2waitFor!=NULL)&& (Title2waitFor==NULL))
										wsprintf(tstr, L"Waiting for: '%s'...", Class2waitFor);
									else if((Class2waitFor==NULL)&& (Title2waitFor!=NULL))
										wsprintf(tstr, L"Waiting for: '%s'...", Title2waitFor);
									else if ((Class2waitFor==NULL) && (Title2waitFor==NULL))
										wsprintf(tstr, L"Waiting for: MISSING", Title2waitFor);

									Add2List(hProcList, tstr);
									bFirstRun=false;
								}
							}
						}
						foundSetupWindow=false;
						if ( (bAppIsRunning==true) && (ShowInstallers()==0) )
						{
							//Wait for the window title
							//if (WaitForWindow(Class2waitFor, Title2waitFor)==1)
							if (IsWindow(Class2waitFor, Title2waitFor)==1)
							{
								KillTimer(hWnd, timer2);
								//Add2List(hProcList, L"Window found");
								//Add2List(hProcList, L"will switch over...");
								//Sleep(10000);//wait 10 seconds, 
								//MaximizeWindow(L"Login"); //Maximize window
								//DestroyWindow(hWnd);
								wsprintf(tstr, L"Waiting %i sec...", timer3intervall/1000*WaitBeforeExit);
								Add2List(hProcList, tstr);
								//Add2List(hProcList, L"Waiting 10 sec.");
								SetTimer(hWnd, timer3, timer3intervall, NULL);
								//for TESTINg signal ready event
								SetEvent(hiLockReady);
							}
						}
						PostMessage(hProgress, PBM_STEPIT, 0, 0);
						return 0;	//5.1.8.1 return messgage has been processed
					case timer3:
						nclog(L"iLock5: WM_TIMER. Timer3 proc...\r\n");
						//static int iCount3=0;
						if(iCount3>=WaitBeforeExit){
							KillTimer(hWnd, timer3);
							DestroyWindow(hWnd);
						}
						iCount3++;
						//MaximizeWindow(L"Login"); //Maximize window
						PostMessage(hProgress, PBM_STEPIT, 0, 0);
						return 0;	//5.1.8.1 return messgage has been processed
					case timer4:
						nclog(L"iLock5: WM_TIMER. Timer4 proc...\r\n");
						if(TIMER4COUNT==0)	//reboot button will never show
							return 0;
						Timer4Count++;
						if(Timer4Count>TIMER4COUNT){
							ShowWindow(hBtnReboot, SW_SHOWNORMAL);
						}
						PostMessage(hProgress, PBM_STEPIT, 0, 0);
						return 0;	//5.1.8.1 return messgage has been processed
				 }
			return 0;	//5.1.8.1 return messgage has been processed
		case WM_LBUTTONDOWN:
			x = LOWORD(lParam);
			y = HIWORD(lParam);
			nclog(L"iLock5: WM_LBUTTONDOWN at x/y: %i/%i\r\n",x,y);
			//if(iUseLogging>0){
			//	wsprintf(tstr, L"click: %i/%i", x*(screenXmax/240), y*(screenYmax/320));
			//	Add2List(hProcList, tstr);
			//}
			return 0;	//5.1.8.1 return messgage has been processed
		case WM_LBUTTONDBLCLK:
			x = LOWORD(lParam);
			y = HIWORD(lParam);
			nclog(L"iLock5: WM_LBUTTONDBLCLK at x/y: %i/%i\r\n",x,y);
			if(x<iHotSpotX+10 && x>iHotSpotX-10 && y<iHotSpotY+10 && y>iHotSpotY-10)
			{
				KillTimer(hWnd, timer1);
				KillTimer(hWnd, timer2);
			   //Passwortabfrage nur bei DOPPELKLICK
				a = DialogBox(hInst, (LPCTSTR)IDD_PWBOX, hWnd,  (DLGPROC)Password);
				if (a == IDCANCEL)
				{
					MessageBeep(MB_ICONEXCLAMATION);
					SetTimer (hWnd, timer1, timer1intervall, NULL) ;
					SetTimer (hWnd, timer2, timer2intervall, NULL) ; //refresh process list all 2 seconds
					//redraw
					//UpdateWindow(hWnd);
					return DefWindowProc(hWnd, message, wParam, lParam);
				}
                PostMessage (hWnd, WM_CLOSE, 0, 0);				
                return 0;	//5.1.8.1 return messgage has been processed
			}
			return 0;	//5.1.8.1 return messgage has been processed
        default:
			DEBUGMSG(1, (L"no msg handler for 0x%08x\r\n", message));
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{	
	HWND hVersion=NULL;
    switch (message)
    {
        case WM_INITDIALOG:
            {
				TCHAR szVersion[MAX_LOADSTRING];
				LoadString(g_hInstance, IDS_VERSION, szVersion, MAX_LOADSTRING);
				hVersion = GetDlgItem(hDlg, IDC_STATIC_VERSION);
				if(hVersion!=NULL)
					SetWindowText(hVersion, szVersion);
                // Create a Done button and size it.  
                SHINITDLGINFO shidi;
                shidi.dwMask = SHIDIM_FLAGS;
                shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN | SHIDIF_EMPTYMENU;
                shidi.hDlg = hDlg;
                SHInitDialog(&shidi);
            }
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hDlg, message);
            return TRUE;

#ifdef _DEVICE_RESOLUTION_AWARE
        case WM_SIZE:
            {
		DRA::RelayoutDialog(
			hInst, 
			hDlg, 
			DRA::GetDisplayMode() != DRA::Portrait ? MAKEINTRESOURCE(IDD_ABOUTBOX_WIDE) : MAKEINTRESOURCE(IDD_ABOUTBOX));
            }
            break;
#endif
    }
    return (INT_PTR)FALSE;
}
// Mesage handler for the Password box.
LRESULT CALLBACK Password(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	RECT rt, rt1;
	int DlgWidth, DlgHeight;	// dialog width and height in pixel units
	int NewPosX, NewPosY;

	switch (message)
	{
		case WM_INITDIALOG:
			// trying to center the About dialog
			if (GetWindowRect(hDlg, &rt1)) {
				GetClientRect(GetParent(hDlg), &rt);
				DlgWidth	= rt1.right - rt1.left;
				DlgHeight	= rt1.bottom - rt1.top ;
				NewPosX		= (rt.right - rt.left - DlgWidth)/2;
				NewPosY		= (rt.bottom - rt.top - DlgHeight)/2;
				
				// if the About box is larger than the physical screen 
				// if (NewPosX < 0) NewPosX = 0;
				// if (NewPosY < 0) NewPosY = 0;
				//align the dialog top left
				NewPosX = 0;
				NewPosY = 0;
				SetWindowPos(hDlg, 0, NewPosX, NewPosY,
					0, 0, SWP_NOZORDER | SWP_NOSIZE);
				//show the sip
				ShowSIP(true);
				SetFocus(GetDlgItem(hDlg, IDC_PWTEXT));
			}
			return TRUE;
		case WM_PAINT:
				SetFocus(GetDlgItem(hDlg, IDC_PWTEXT));
				break;			
		case WM_COMMAND:
			if ((LOWORD(wParam) == IDOK) || (LOWORD(wParam) == IDCANCEL))
			{
				//hide the sip
				ShowSIP(false);
                // Get text from edit control.
				GetDlgItemText (hDlg, IDC_PWTEXT, pw, sizeof (pw));
				//PASSWORD!
				if (wcscmp(pw, L"52401") == 0){
					bAdminExit=true;
					EndDialog(hDlg, IDOK);
				}
				else
					EndDialog(hDlg, IDCANCEL); //LOWORD(wParam));
				return TRUE;
			}
			if (LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg, IDCANCEL); //LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}
