// iHookIdle.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "hooks.h"
#include "iHookIdle.h"
#include "nclog.h"

TCHAR szAppName[MAX_PATH] = L"iHookIdle v3.6.0";

#define STOPEVENTNAME L"STOPILOCK"

// Names for scan events
#define ITC_SCAN_STATE			_T("StateLeftScan") // 	StateCenterScan
#define ITC_SCAN_DELTA			_T("DeltaLeftScan") // 	DeltaCenterScan

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE			g_hInstance;			// current instance
HWND				g_hWndMenuBar;		// menu bar handle

// Forward declarations of functions included in this code module:
ATOM			MyRegisterClass(HINSTANCE, LPTSTR);
BOOL			InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

//#####################################################################
HANDLE watchScanEventThreadHandle=NULL;
DWORD  watchScanEventThreadID=0;
HANDLE watchScanEventThreadSTOP=NULL;

#include "hooks.h"
#include  <nled.h>
#include "keymap.h"	//the char to vkey mapping

#include "registry.h"
#define REGKEY L"Software\\Intermec\\iHookIdle"

#include "idleBeeper.h"
#include "dlg_info.h"

DWORD regValEnableAlarm=1;
DWORD regValIdleTimeout=300;	//seconds of timeout
DWORD regValAlarmOffKey=0x73;

DWORD regValEnableInfo=1;
TCHAR regVal_InfoText[MAX_PATH]=L"Idle time elapsed alarm!";
TCHAR regVal_InfoButton1[MAX_PATH]=L"Snooze";
TCHAR regVal_InfoButton2[MAX_PATH]=L"Dismiss";

DWORD regValExtApp=0;
TCHAR regValExtAppParms[MAX_PATH]=L"";
TCHAR regValExtAppApp[MAX_PATH]=L"";
UINT  matchTimeout = 3000;  //ms, if zero, no autofallback
DWORD EnableLogging = 1;

TCHAR szKeySeq[10]; //hold a max of ten chars
char szKeySeqA[10]; //same as char list

char szVKeySeq[10];
bool bCharShiftSeq[10];

int iKeyCount=3;
int iMatched=0;

BYTE* pForbiddenKeyList = NULL;

LONG FAR PASCAL WndProc (HWND , UINT , UINT , LONG) ;

void startWatchScanThread(HWND hWnd);
int ReadReg();
void WriteReg();

static bool isStickyOn=false;
static int tID=1011; //TimerID

NOTIFYICONDATA nid;
HWND g_hWnd = NULL;
HINSTANCE	g_hHookApiDLL	= NULL;			// Handle to loaded library (system DLL where the API is located)

int LEDid=3;		//which LED to use for key in sequence active
int VibrateID = 0;	//which LED ID to use for vibrate
int alarmLEDid=5;	//which LED ID to use for alarm

// Global functions: The original Open Source
BOOL g_HookDeactivate();
BOOL g_HookActivate(HINSTANCE hInstance);

//
void ShowIcon(HWND hWnd, HINSTANCE hInst);
void RemoveIcon(HWND hWnd);


#pragma data_seg(".HOOKDATA")									//	Shared data (memory) among all instances.
	HHOOK g_hInstalledLLKBDhook = NULL;						// Handle to low-level keyboard hook
#pragma data_seg()

#pragma comment(linker, "/SECTION:.HOOKDATA,RWS")		//linker directive

// from the platform builder <Pwinuser.h>
extern "C" {
	BOOL WINAPI NLedGetDeviceInfo( UINT     nInfoId, void   *pOutput );
	BOOL WINAPI NLedSetDevice( UINT nDeviceId, void *pInput );
};

int mystrstr(wchar_t *txt1,wchar_t *txt2)
{
    wchar_t *posstr=wcsstr(txt1,txt2);
    if(posstr!=NULL)
    {
        return (posstr-txt1);
    }else
    {
        return -1;
    }
}

// assume: supplied buff is enough to hold generated text
void StringReplace(wchar_t *buff, wchar_t *txt1, wchar_t *txt2)
{
    wchar_t *tmp;
    wchar_t *nextStr;
    int pos;
    tmp=wcsdup(buff);
    pos=mystrstr(tmp,txt1);
    if(pos!=-1)
    {
        buff[0]=0;
        wcsncpy(buff,tmp,pos);
        buff[pos]=0;
        wcscat(buff,txt2);
        nextStr=tmp+pos+wcslen(txt1);
        while(wcslen(nextStr)!=0)
        {
            pos=mystrstr(nextStr,txt1);
            if(pos==-1)
            {
                wcscat(buff,nextStr);
                break;
            }
            wcsncat(buff,nextStr,pos);
            wcscat(buff,txt2);
            nextStr=nextStr+pos+wcslen(txt1);   
        }
    }
    free(tmp);
}

//start process
void startProcess(TCHAR* szImage, TCHAR* szParms){
	//start external app
	PROCESS_INFORMATION pi;
	if(CreateProcess(szImage, szParms, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)==0){
		nclog(L"CreateProcess ('%s'/'%s') failed with 0x%08x\r\n", szImage, szParms, GetLastError());
	}
	else{
		//OK
		nclog(L"CreateProcess ('%s'/'%s') OK\r\n", szImage, szParms);
	}
}

//control the LEDs
void LedOn(int id, int onoff) //onoff=0 LED is off, onoff=1 LED is on, onoff=2 LED blink
{
	TCHAR str[MAX_PATH];
	wsprintf(str,L"Trying to set LED with ID=%i to state=%i\n", id, onoff);
	DEBUGMSG(true,(str));
	/*	
	struct NLED_COUNT_INFO {
	  UINT cLeds; 
	};*/
	NLED_COUNT_INFO cInfo;
	memset(&cInfo, 0, sizeof(cInfo));
	NLedGetDeviceInfo(NLED_COUNT_INFO_ID, &cInfo);
	if (cInfo.cLeds == 0)
	{
		DEBUGMSG(true,(L"NO LEDs supported!"));
		return;
	}
	else
	{
		wsprintf(str,L"Device supports %i LEDs\n",cInfo.cLeds);
		DEBUGMSG(true,(str));
	}

    NLED_SETTINGS_INFO settings; 
	memset(&settings, 0, sizeof(settings));
    settings.LedNum= id;
	/*	0 Off 
		1 On 
		2 Blink */
    settings.OffOnBlink= onoff;
	settings.TotalCycleTime=1000;
	settings.OnTime = 500;
	settings.OffTime=500;
	settings.MetaCycleOn=5;
	settings.MetaCycleOff=5;
	if (!NLedSetDevice(NLED_SETTINGS_INFO_ID, &settings))
        DEBUGMSG(1,(L"NLedSetDevice(NLED_SETTINGS_INFO_ID) failed\n"));
	else
		DEBUGMSG(1,(L"NLedSetDevice(NLED_SETTINGS_INFO_ID) success\n"));
}

DWORD WINAPI watchScanEvent(LPVOID lpParam){
	HWND hWnd=(HWND)lpParam;
	BOOL isReady=FALSE;
	BOOL bStop=FALSE;
	HANDLE hBarcodeEvent = NULL;
	//wait for datacollection engine is ready
	DEBUGMSG(1, (L"watchScanEvent: attaching to SCAN EVENT\r\n"));
	do{
		hBarcodeEvent = CreateEvent(NULL, FALSE, FALSE, ITC_SCAN_STATE);
		if(hBarcodeEvent!=NULL){
			if(GetLastError()==ERROR_ALREADY_EXISTS)
				isReady=TRUE;
		}
		Sleep(1000);
	}while(!isReady);
	DEBUGMSG(1, (L"watchScanEvent: SCAN EVENT found\r\n"));

	DWORD dwWait;
	HANDLE hWaitHandles[2];
	hWaitHandles[0]=hBarcodeEvent;
	hWaitHandles[1]=watchScanEventThreadSTOP;
	do{
		dwWait = WaitForSingleObject(hBarcodeEvent, INFINITE);
		switch(dwWait){
			case WAIT_OBJECT_0:
				DEBUGMSG(1, (L"watchScanEvent: got Scan EVENT\r\n"));
				resetIdleThread();
				break;
			case WAIT_OBJECT_0+1:
				DEBUGMSG(1, (L"watchScanEvent: got STOP EVENT\r\n"));
				bStop=TRUE;
				break;
			default:
				DEBUGMSG(1, (L"watchScanEvent: unknown handle: 0x%08x\r\n", dwWait));
				break;
		}
		Sleep(1);
	}while(!bStop);
	DEBUGMSG(1, (L"watchScanEvent: thread ended\r\n"));
	return 0;
}

//thread to look for barcode scan event
void startWatchScanThread(HWND hWnd){
	DEBUGMSG(1, (L"startWatchScanThread...\r\n"));
	nclog(L"startWatchScanThread...\r\n");
	watchScanEventThreadSTOP=CreateEvent(NULL, FALSE, FALSE, L"watchScanEventThreadSTOP");
	watchScanEventThreadHandle = CreateThread(NULL, 0, watchScanEvent, hWnd, 0, &watchScanEventThreadID);
}


//timer proc which resets isStickyOn after a period
VOID CALLBACK Timer2Proc(
                        HWND hWnd, // handle of window for timer messages
                        UINT uMsg,    // WM_TIMER message
                        UINT idEvent, // timer identifier
                        DWORD dwTime  // current system time
                        )

{
	DEBUGMSG(1, (L"Timout: MATCH missed. Reseting matching\n"));
	iMatched = 0;
	KillTimer(NULL, tID);
	LedOn(LEDid,0);
}

// The command below tells the OS that this EXE has an export function so we can use the global hook without a DLL
__declspec(dllexport) LRESULT CALLBACK g_LLKeyboardHookCallback(
   int nCode,      // The hook code
   WPARAM wParam,  // The window message (WM_KEYUP, WM_KEYDOWN, etc.)
   LPARAM lParam   // A pointer to a struct with information about the pressed key
) 
{
	/*	typedef struct {
	    DWORD vkCode;
	    DWORD scanCode;
	    DWORD flags;
	    DWORD time;
	    ULONG_PTR dwExtraInfo;
	} KBDLLHOOKSTRUCT, *PKBDLLHOOKSTRUCT;*/
	
	// Get out of hooks ASAP; no modal dialogs or CPU-intensive processes!
	// UI code really should be elsewhere, but this is just a test/prototype app
	// In my limited testing, HC_ACTION is the only value nCode is ever set to in CE
	static int iActOn = HC_ACTION;
	static bool isShifted=false;

#ifdef DEBUG
	static TCHAR str[MAX_PATH];
#endif

	PKBDLLHOOKSTRUCT pkbhData = (PKBDLLHOOKSTRUCT)lParam;
	//DWORD vKey;
	if (nCode == iActOn) 
	{ 
		//idle timer reset
		if(wParam==WM_KEYUP)
#ifdef DEBUG
			if(pkbhData->vkCode==VK_O || pkbhData->vkCode==VK_0 )
#else
			if(pkbhData->vkCode==regValAlarmOffKey)// 0x73)	//OFF key
#endif
				stopBeeper();
			else
				resetIdleThread();

		//if(g_bRebootDialogOpen){
		//	return CallNextHookEx(g_hInstalledLLKBDhook, nCode, wParam, lParam);
		//}

		//only process unflagged keys
		if (pkbhData->flags != 0x00)
			return CallNextHookEx(g_hInstalledLLKBDhook, nCode, wParam, lParam);
		//check vkCode against forbidden key list
		if(pForbiddenKeyList!=NULL)
		{
			BOOL bForbidden=false;
			int j=0;
			do{
				if(pForbiddenKeyList[j]==(BYTE)pkbhData->vkCode)
				{
					bForbidden=true;
					DEBUGMSG(1, (L"suppressing forbidden key: 0x%0x\n",pkbhData->vkCode));
					nclog(L"suppressing forbidden key: 0x%0x\n",pkbhData->vkCode);
					continue;
				}
				j++;
			}while(!bForbidden && pForbiddenKeyList[j]!=0x00);
			if(bForbidden){
				return true;
			}
		}

		SHORT sShifted = GetAsyncKeyState(VK_SHIFT);
		if((sShifted & 0x800) == 0x800)
			isShifted = true;
		else
			isShifted = false;

#ifdef DEBUG
			wsprintf(str, L"vkCode=\t0x%0x \n", pkbhData->vkCode);
			DEBUGMSG(true,(str));
			wsprintf(str, L"scanCode=\t0x%0x \n", pkbhData->scanCode);
			DEBUGMSG(true,(str));
			wsprintf(str, L"flags=\t0x%0x \n", pkbhData->flags);
			DEBUGMSG(true,(str));
//			wsprintf(str, L"isStickyOn is=\t0x%0x \n", isStickyOn);
//			DEBUGMSG(true,(str));
			wsprintf(str, L"wParam is=\t0x%0x \n", wParam);
			DEBUGMSG(true,(str));
			wsprintf(str, L"lParam is=\t0x%0x \n", lParam);
			DEBUGMSG(true,(str));

			wsprintf(str, L"iMatched is=\t0x%0x \n", iMatched);
			DEBUGMSG(true,(str));


			if(isShifted)
				DEBUGMSG(true,(L"Shift ON\n"));
			else
				DEBUGMSG(true,(L"Shift OFF\n"));

			DEBUGMSG(true,(L"------------------------------\n"));
#endif

		//check and toggle for Shft Key
/*
		if (pkbhData->vkCode == VK_SHIFT){
			if( wParam==WM_KEYUP ) 
				isShifted=!isShifted;
		}
*/
		//do not process shift key
		if (pkbhData->vkCode == VK_SHIFT){
			DEBUGMSG(1, (L"Ignoring VK_SHIFT\n"));
			return CallNextHookEx(g_hInstalledLLKBDhook, nCode, wParam, lParam);
		}

		//################################################################
		//check if the actual key is a match key including the shift state
		if ((byte)pkbhData->vkCode == (byte)szVKeySeq[iMatched]){
			DEBUGMSG(1 , (L"==== char match\n"));
			if (bCharShiftSeq[iMatched] == isShifted){
				DEBUGMSG(1 , (L"==== shift match\n"));
			}
			else{
				DEBUGMSG(1 , (L"==== shift not match\n"));
			}
		}

		if( wParam == WM_KEYUP ){
			DEBUGMSG(1, (L"---> szVKeySeq[iMatched] = 0x%02x\n", (byte)szVKeySeq[iMatched]));

			if ( ((byte)pkbhData->vkCode == (byte)szVKeySeq[iMatched]) && (isShifted == bCharShiftSeq[iMatched]) ) {
					
				//the first match?
				if(iMatched==0){
					//start the timer and lit the LED
					LedOn(LEDid,1);
					tID=SetTimer(NULL, 0, matchTimeout, (TIMERPROC)Timer2Proc);
				}
				iMatched++;

				DEBUGMSG(1, (L"iMatched is now=%i\n", iMatched));
				//are all keys matched
				if (iMatched == iKeyCount){
					//show modeless dialog
					nclog(L"FULL MATCH, starting ...\n");
					//DO ACTION
					if(regValExtApp==1 && wcslen(regValExtAppApp)>0){
						//start external process
						nclog(L"\trun ext. process '%s'\n", regValExtAppApp);
						startProcess(regValExtAppApp, regValExtAppParms);
					}
					else{
						nclog(L"\tfireing named event\n");
						HANDLE hEvent = CreateEvent(NULL, FALSE,FALSE, STOPEVENTNAME);
						if(hEvent!=NULL){
							SetEvent(hEvent);
							CloseHandle(hEvent);
						}
					}
					//reset match pos and stop timer
					nclog(L"FULL MATCH: Reset matching\n");
					LedOn(LEDid,0);
					iMatched=0; //reset match pos
					KillTimer(NULL, tID);
					//return CallNextHookEx(g_hInstalledLLKBDhook, nCode, wParam, lParam);
				}
				//return -1; //do not forward key?
			}
			else
			{
				KillTimer(NULL, tID);
				LedOn(LEDid,0);
				iMatched=0; //reset match pos
				DEBUGMSG(1, (L"FULL MATCH missed. Reseting matching\n"));
			}
		} //if wParam == WM_KEY..
	}
	return CallNextHookEx(g_hInstalledLLKBDhook, nCode, wParam, lParam);
}

BOOL g_HookActivate(HINSTANCE hInstance)
{
	// We manually load these standard Win32 API calls (Microsoft says "unsupported in CE")
	SetWindowsHookEx		= NULL;
	CallNextHookEx			= NULL;
	UnhookWindowsHookEx	= NULL;

	// Load the core library. If it's not found, you've got serious issues :-O
	nclog(L"LoadLibrary(coredll.dll)...\n");
	g_hHookApiDLL = LoadLibrary(_T("coredll.dll"));
	if(g_hHookApiDLL == NULL) return false;
	else {
		// Load the SetWindowsHookEx API call (wide-char)
		//TRACE(_T("OK\nGetProcAddress(SetWindowsHookExW)..."));
		SetWindowsHookEx = (_SetWindowsHookExW)GetProcAddress(g_hHookApiDLL, _T("SetWindowsHookExW"));
		if(SetWindowsHookEx == NULL) {
			nclog(L"could not load SetWindowsHookEx");
			return false;
		}
		else
		{
			// Load the hook.  Save the handle to the hook for later destruction.
			//TRACE(_T("OK\nCalling SetWindowsHookEx..."));
			g_hInstalledLLKBDhook = SetWindowsHookEx(WH_KEYBOARD_LL, g_LLKeyboardHookCallback, hInstance, 0);
			if(g_hInstalledLLKBDhook == NULL){
				nclog(L"\n######## SetWindowsHookEx failed, GetLastError=%i\n", GetLastError());
				return false;
			}
		}

		// Get pointer to CallNextHookEx()
		//TRACE(_T("OK\nGetProcAddress(CallNextHookEx)..."));
		CallNextHookEx = (_CallNextHookEx)GetProcAddress(g_hHookApiDLL, _T("CallNextHookEx"));
		if(CallNextHookEx == NULL) {
			nclog(L"could not load CallNextHookEx");
			return false;
		}

		// Get pointer to UnhookWindowsHookEx()
		//TRACE(_T("OK\nGetProcAddress(UnhookWindowsHookEx)..."));
		UnhookWindowsHookEx = (_UnhookWindowsHookEx)GetProcAddress(g_hHookApiDLL, _T("UnhookWindowsHookEx"));
		if(UnhookWindowsHookEx == NULL)  {
			nclog(L"could not load UnhookWindowsHookEx");
			return false;
		}
	}

	nclog(L"g_HookActivate: OK\n");
	return true;
}


BOOL g_HookDeactivate()
{
	//TRACE(_T("Uninstalling hook..."));
	if(g_hInstalledLLKBDhook != NULL)
	{
		UnhookWindowsHookEx(g_hInstalledLLKBDhook);		// Note: May not unload immediately because other apps may have me loaded
		g_hInstalledLLKBDhook = NULL;
	}

	//TRACE(_T("OK\nUnloading coredll.dll..."));
	if(g_hHookApiDLL != NULL)
	{
		FreeLibrary(g_hHookApiDLL);
		g_hHookApiDLL = NULL;
	}
	//TRACE(_T("OK\nEverything unloaded OK\n"));
	return true;
}

//return shifted of found entry
bool isShifted(const char* c){
	for (int i=0x20; i<0x80; i++){
		if(strncmp(vkTable[i].txt, c, 1)==0){
			return vkTable[i].kShift;
		}
	}
	return false;
}

byte getVKcode(const char* c){
	for (int i=0x20; i<0x80; i++){
		if(strncmp(vkTable[i].txt, c, 1)==0){
			return vkTable[i].kVKval;
		}
	}
	return 0;
}

//helper to convert ANSI char sequence to vkCode sequence and associated shift sequence
// ie * will have chr code 0x2A BUT vkCode is 0x38 with a VK_SHIFT before
void initVkCodeSeq(){
	DEBUGMSG(1, (L"------------ key seq -------------\n"));
	bool bShift=false;
	char* c = " ";
	wchar_t* t = L" ";

	//lookup the chars in szVKeySeq in keymap and fill bShiftSeq and szVKeySeq
	for(unsigned int i=0; i<strlen(szKeySeqA); i++){
		//check, if the char needs a shifted VK_Code
		const char* cIn = &szKeySeqA[i];
		//strncpy(c, &szKeySeqA[i], 1);
		bShift = isShifted(cIn);
		bCharShiftSeq[i] = bShift;
		
		//now get the VK_code of the key producing the char
		szVKeySeq[i]=getVKcode(cIn); //szKeySeqA[i];
		
		c=new char(2);
		sprintf(c, "%s", cIn);
		t=new wchar_t(2);
		mbstowcs(t, c, 1);

		DEBUGMSG(1, (L"szVKeySeq[%i] = '%s' (0x%02x) \t bCharShiftSeq[%i] = %02x\n", i, t, (byte)cIn/*szVKeySeq[i]*/, i, bCharShiftSeq[i]));
	}
	DEBUGMSG(1, (L"------------ key seq -------------\n"));
}

void WriteReg()
{
	nclog(L"WriteReg()...\n");
	DWORD rc=0;
	DWORD dwVal=0;
	rc = OpenCreateKey(REGKEY);
	if (rc != 0)
		ShowError(rc);

	if (rc=OpenKey(REGKEY) != 0)
		ShowError(rc);
	//logging
	dwVal=EnableLogging;
	rc = RegWriteDword(L"EnableLogging", &dwVal);
	if (rc != 0)
		ShowError(rc);

	//timeout
	dwVal=10;
	rc = RegWriteDword(L"Timeout", &dwVal);
	if (rc != 0)
		ShowError(rc);

	//LedID
	dwVal=1;
	rc = RegWriteDword(L"LEDid", &dwVal);
	if (rc != 0)
		ShowError(rc);
	
	dwVal=2;
	rc = RegWriteDword(L"alarmLEDid", &dwVal);
	if (rc != 0)
		ShowError(rc);

	dwVal=1;
	rc = RegWriteDword(L"VibrateID", &dwVal);
	if (rc != 0)
		ShowError(rc);


	//vkey sequence
	rc=RegWriteStr(L"KeySeq", szKeySeq);
    if (rc != 0)
        ShowError(rc);

	//ForbiddenKeys list
	pForbiddenKeyList=new BYTE(3);
	pForbiddenKeyList[0]=0x72; //F3 key
	pForbiddenKeyList[1]=0x73; //F4 key
	pForbiddenKeyList[2]=0x00; //end marker
	rc=RegWriteBytes(L"ForbiddenKeys", (BYTE*) pForbiddenKeyList, 2);
	if(rc != 0){
		ShowError(rc);
		pForbiddenKeyList=NULL;
	}

	dwVal=regValExtApp;
	rc = RegWriteDword(L"AppExt", &dwVal);
    if (rc != 0)
        ShowError(rc);
	rc=RegWriteStr(L"AppExtApp", regValExtAppApp);
    if (rc != 0)
        ShowError(rc);
	rc=RegWriteStr(L"AppExtParms", regValExtAppParms);
    if (rc != 0)
        ShowError(rc);


/*
	dwVal=regValShutdownExt;
	rc = RegWriteDword(L"ShutdownExt", &dwVal);
    if (rc != 0)
        ShowError(rc);
	rc=RegWriteStr(L"ShutdownExtApp", regValShutdownExtApp);
    if (rc != 0)
        ShowError(rc);
	rc=RegWriteStr(L"ShutdownExtParms", regValShutdownExtParms);
    if (rc != 0)
        ShowError(rc);
*/
	dwVal=regValEnableAlarm;
	rc = RegWriteDword(L"EnableAlarm", &dwVal);
    if (rc != 0)
        ShowError(rc);

	dwVal=regValAlarmOffKey;
	rc = RegWriteDword(L"AlarmOffKey", &dwVal);
    if (rc != 0)
        ShowError(rc);

	//####### info dialog values
	dwVal=regValEnableInfo;
	rc = RegWriteDword(L"InfoEnabled", &dwVal);
    if (rc != 0)
        ShowError(rc);
	rc=RegWriteStr(L"InfoButton1", regVal_InfoButton1);
    if (rc != 0)
        ShowError(rc);
	rc=RegWriteStr(L"InfoButton2", regVal_InfoButton2);
    if (rc != 0)
        ShowError(rc);
	rc=RegWriteStr(L"InfoText", regVal_InfoText);
    if (rc != 0)
        ShowError(rc);

	//#########################
	dwVal=regValIdleTimeout;
	rc = RegWriteDword(L"IdleTimeout", &dwVal);
    if (rc != 0)
        ShowError(rc);
/*
	dwVal=bShowShutdownButton;
	rc = RegWriteDword(L"ShowShutdownButton", &dwVal);
    if (rc != 0)
        ShowError(rc);

	dwVal=bShowSuspendButton;
	rc = RegWriteDword(L"ShowSuspendButton", &dwVal);
    if (rc != 0)
        ShowError(rc);
*/
	CloseKey();
}

int ReadReg()
{
	//for KeyToggleBoot we need to read the stickyKey to react on
	//and the timout for the sticky key
	byte dw=0;
	DWORD dwVal=0;

	OpenKey(REGKEY);
	
	if (RegReadDword(L"EnableLogging", &dwVal)==0){
		if(dwVal>0)
			EnableLogging=1;
		else if (dwVal==0)
			EnableLogging=0;
	}
	DEBUGMSG(1, (L"#### ReadReg(): EnableLogging=%i ####", EnableLogging));
	RETAILMSG(1, (L"#### ReadReg(): EnableLogging=%i ####", EnableLogging));

	if(EnableLogging)
		nclog_LogginEnabled=TRUE;
	else
		nclog_LogginEnabled=FALSE;

	nclog(L"ReadReg()...\n");

	//read the timeout for the StickyKey
	if (RegReadDword(L"Timeout", &dwVal)==0)
	{
		matchTimeout = (UINT) dwVal * 1000;
		nclog(L"ReadReg(): Timeout=%i\n",matchTimeout);
	}
	else
	{
		matchTimeout = 5 * 1000;
		nclog(L"ReadReg(): failed reading, Timeout =%i\n",matchTimeout);
	}

    //read LEDid to use for signaling
    if (RegReadDword(L"LEDid", &dwVal)==0)
    {
        LEDid = dwVal;
		nclog(L"ReadReg(): LEDid=%i\n",LEDid);
    }
    else
    {
        LEDid = 1;
		nclog(L"ReadReg(): failed reading LEDid default=%i\n",LEDid);
    }

	//read LEDid to use for alarm
    if (RegReadDword(L"alarmLEDid", &dwVal)==0)
    {
        alarmLEDid = dwVal;
		nclog(L"ReadReg(): alarmLEDid =%i\n", alarmLEDid);
    }
    else
    {
        alarmLEDid = 2;
		nclog(L"ReadReg(): failed reading alarmLEDid default=%i\n", alarmLEDid);
    }


	//read VibrateID to use for signaling
    if (RegReadDword(L"VibrateID", &dwVal)==0)
    {
        VibrateID = dwVal;
		nclog(L"ReadReg(): VibrateID =%i\n", VibrateID);
    }
    else
    {
        VibrateID = 1;
		nclog(L"ReadReg(): failed reading VibrateID default=%i\n", VibrateID);
    }

	TCHAR szTemp[10];
	TCHAR szTemp2[MAX_PATH];
	wsprintf(szTemp2, L"");

	int iSize=RegReadByteSize(L"KeySeq", iSize);
	int iTableSizeOUT=iSize;
    if(iSize > 20){ //0x14 = 20 bytes, do we have more than 10 bytes?
		nclog(L"ReadReg(): failed reading KeyTable (iSize>20), using default\n");
    }
    else{
        if (RegReadStr(L"KeySeq", szTemp)==ERROR_SUCCESS){
			wcscpy(szKeySeq, szTemp);
			wcstombs(szKeySeqA, szKeySeq, 10);
			nclog(L"ReadReg(): Read KeySeq OK: '%s'\n", szKeySeq);
            //memcpy(szKeySeq, bTemp, iSize);
        }
        else{
            nclog(L"ReadReg(): Failed reading KeySeq, using default '%s'\n", szKeySeq);
        }
    }

/*
	//show or hide Shutdown button?
	if(RegReadDword(L"ShowShutdownButton", &dwVal)==ERROR_SUCCESS){
		if(dwVal==1)
			bShowShutdownButton=TRUE;
		else
			bShowShutdownButton=FALSE;
	}
	else
		bShowSuspendButton=FALSE;

	//show or hide Suspend button?
	//bShowSuspendButton
	if(RegReadDword(L"ShowSuspendButton", &dwVal)==ERROR_SUCCESS){
		if(dwVal==1)
			bShowSuspendButton=TRUE;
		else
			bShowSuspendButton=FALSE;
	}
	else
		bShowSuspendButton=FALSE;
*/
	//alarm enabled?
	dwVal=regValEnableAlarm;
	if(RegReadDword(L"EnableAlarm", &dwVal)==ERROR_SUCCESS)
		regValEnableAlarm=dwVal;
	else
		regValEnableAlarm=0;	//default is 0 = no Alarm

	nclog(L"ReadReg(): EnableAlarm=%i\n", regValEnableAlarm);

	//read beeper alarm idle timeout
	dwVal=regValIdleTimeout;
	if(RegReadDword(L"IdleTimeout", &dwVal)==ERROR_SUCCESS){
		regValIdleTimeout=dwVal;
	}
	else
		regValIdleTimeout=300;	//default is 5 minutes
	nclog(L"ReadReg(): IdleTimeout=%i\n", regValIdleTimeout);

	//read Allarm Off key
	dwVal=regValAlarmOffKey;
	if(RegReadDword(L"AlarmOffKey", &dwVal)==ERROR_SUCCESS)
		regValAlarmOffKey=dwVal;
	else
		regValAlarmOffKey=0x73;	//default is F4 (Phone End key)
	nclog(L"ReadReg(): AlarmOffKey=0x%02x\n", regValAlarmOffKey);

	if(RegReadDword(L"AppExt", &dwVal)==ERROR_SUCCESS){
		DEBUGMSG(1, (L"AppExt = %i\n", dwVal));
		regValExtApp=dwVal;
		nclog(L"ReadReg(): AppExt = %i\n", regValExtApp);
		if(regValExtApp==1){
			if(RegReadStr(L"AppExtApp", szTemp2)==ERROR_SUCCESS)
				wsprintf(regValExtAppApp, L"%s", szTemp2);
			else
				wsprintf(regValExtAppApp, L"");
			nclog(L"ReadReg(): Read AppExtApp ='%s'\n", regValExtAppApp);

			if(RegReadStr(L"AppExtParms", szTemp2)==ERROR_SUCCESS)
				wsprintf(regValExtAppParms, L"%s", szTemp2);
			else
				wsprintf(regValExtAppParms, L"");
			nclog(L"ReadReg(): Read AppExtParms ='%s'\n", regValExtAppParms);
		}
		else
			nclog(L"ReadReg(): rest skipped\n");

	}
	else
		nclog(L"ReadReg(): ReadReg AppExt failed\n");

/*
	if(RegReadDword(L"ShutdownExt", &dwVal)==ERROR_SUCCESS){
		DEBUGMSG(1, (L"ShutdownExt = %i\n", dwVal));
		regValShutdownExt=dwVal;
		if(regValShutdownExt==1){
			if(RegReadStr(L"ShutdownExtApp", szTemp2)==ERROR_SUCCESS)
			{
				wsprintf(regValShutdownExtApp, L"%s", szTemp2);
				DEBUGMSG(1, (L"Read ShutdownExtApp ='%s'\n", regValShutdownExtApp));
			}			
			else
				wsprintf(regValShutdownExtApp, L"");

			if(RegReadStr(L"ShutdownExtParms", szTemp2)==ERROR_SUCCESS)
			{
				wsprintf(regValShutdownExtParms, L"%s", szTemp2);
				DEBUGMSG(1, (L"Read ShutdownExtParms ='%s'\n", regValShutdownExtParms));
			}			
			else
				wsprintf(regValShutdownExtParms, L"");
		}
	}
	else
		DEBUGMSG(1, (L"ReadReg ShutdownExt failed\n"));
*/

	//### INFO dialog strings etc ###
	if(RegReadDword(L"InfoEnabled", &dwVal)==ERROR_SUCCESS)
		regValEnableInfo=dwVal;
	else
		regValEnableInfo=TRUE;
	nclog(L"ReadReg(): InfoEnabled = %i\n", regValEnableInfo);

	//info text
	wsprintf(szTemp2, L"");
	if(RegReadStr(L"InfoText", szTemp2)==ERROR_SUCCESS){
		wsprintf(regVal_InfoText, L"%s", szTemp2, regValIdleTimeout);
		StringReplace(szTemp2, L"\\r", L"\r");
		StringReplace(szTemp2, L"\\n", L"\n");
		wsprintf(regVal_InfoText, L"%s", szTemp2);
	}
	else
		wsprintf(regVal_InfoText, L"Idle time elapsed alarm!");
	//does infotext have a %i inside?
	if(wcsstr(regVal_InfoText, L"%%i")==0)
		nclog(L"ReadReg(): InfoText = '%s'\n", regVal_InfoText, regValIdleTimeout/60);
	else
		nclog(L"ReadReg(): InfoText = '%s'\n", regVal_InfoText);

	//button1
	wsprintf(szTemp2, L"");
	if(RegReadStr(L"InfoButton1", szTemp2)==ERROR_SUCCESS)
		wsprintf(regVal_InfoButton1, L"%s", szTemp2);
	else
		wsprintf(regVal_InfoButton1, L"");//SNOOZE
	nclog(L"ReadReg(): InfoButton1 = '%s'\n", regVal_InfoButton1);

	//button2
	wsprintf(szTemp2, L"");
	if(RegReadStr(L"InfoButton2", szTemp2)==ERROR_SUCCESS)
		wsprintf(regVal_InfoButton2, L"%s", szTemp2);
	else
		wsprintf(regVal_InfoButton2, L"");//DISMISS
	nclog(L"ReadReg(): InfoButton2 = '%s'\n", regVal_InfoButton2);

	//convert from ANSI sequence to vkCode + shift
	nclog(L"ReadReg(): initVkCodeSeq()...\n");
	initVkCodeSeq();

	iKeyCount=wcslen(szKeySeq);

	//read forbidden key list
	//first get size of binary key
	RegReadByteSize(L"ForbiddenKeys", iSize);
	if (iSize>0){
		pForbiddenKeyList=new BYTE(iSize + 1); //add one byte for end marker 0x00
		int rc = RegReadBytes(L"ForbiddenKeys", (BYTE*)pForbiddenKeyList, iSize);
		if(rc != 0){
			ShowError(rc);
			delete(pForbiddenKeyList);
			pForbiddenKeyList=NULL;
			nclog(L"ReadReg(): Reading ForbiddenKeys from REG failed\n");
		}
		else{
			nclog(L"ReadReg(): Reading ForbiddenKeys from REG = OK\n");
			pForbiddenKeyList[iSize+1]=0x00; //end marker
		}
	}
	else{
		pForbiddenKeyList=NULL;
		nclog(L"ReadReg(): Reading ForbiddenKeys from REG failed\n");
	}

	nclog(L"ReadReg(): CloseKey()\n");
	CloseKey();

	TCHAR str[MAX_PATH];

//dump pForbiddenKeyList
#ifdef DEBUG
	char strA[MAX_PATH]; char strB[MAX_PATH];
	sprintf(strA, "ForbiddenKeyList: ");
	int a=0;
	if(pForbiddenKeyList!=NULL){
		while (pForbiddenKeyList[a]!=0x00){
			sprintf(strB, " 0x%02x", pForbiddenKeyList[a]);
			strcat(strA, strB);
			a++;
		};
	}
	mbstowcs(str, strA, strlen(strA));
	str[strlen(strA)]='\0';
	DEBUGMSG(1, (L"%s\n", str));
#endif
	nclog(L"ReadReg: Timeout=%i, , LEDid=%i, KeySeq='%s'\n'", matchTimeout, LEDid, szKeySeq);
	nclog(L"ReadReg: END\n");
	return 0;
}

void ShowIcon(HWND hWnd, HINSTANCE hInst)
{
    NOTIFYICONDATA nid;

    int nIconID=1;
    nid.cbSize = sizeof (NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = nIconID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE;   // NIF_TIP not supported
    nid.uCallbackMessage = MYMSG_TASKBARNOTIFY;
    nid.hIcon = (HICON)LoadImage (g_hInstance, MAKEINTRESOURCE (ID_ICON), IMAGE_ICON, 16,16,0);
    nid.szTip[0] = '\0';

    BOOL r = Shell_NotifyIcon (NIM_ADD, &nid);

}

void RemoveIcon(HWND hWnd)
{
	NOTIFYICONDATA nid;

    memset (&nid, 0, sizeof nid);
    nid.cbSize = sizeof (NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;

    Shell_NotifyIcon (NIM_DELETE, &nid);

}
//#####################################################################

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR    lpCmdLine,
                   int       nCmdShow)
{	
	ReadReg();
	nclog(L"\n\n%s\n#### Starting %s ####\n", logDateTime(), szAppName);
	if (IsIntermec() != 0)
	{
		MessageBox(NULL, L"This is not an Intermec! Program execution stopped!", L"Fatal Error", MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
		nclog(L"This is not an Intermec! Program execution stopped!\n");
		return -1;
	}

	MSG msg;

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	if (wcsstr(lpCmdLine, L"-writereg") != NULL){
		wsprintf(szKeySeq, L"...");// L"*.#");
		wcstombs(szKeySeqA, szKeySeq, 10);
		nclog(L"-writereg recognised...\n");
		WriteReg();
//		writeRegDlg();
	}

	HACCEL hAccelTable;
	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_IHOOKIDLE));

	 	// TODO: Place code here.

	//info dialog creation
	bInfoDlgVisible=regValEnableInfo;
	
	if(wcsstr(regVal_InfoText, L"%%")!=NULL){
		TCHAR newStr[MAX_PATH];
		wsprintf(newStr, regVal_InfoText, regValIdleTimeout/60);
		wsprintf(regVal_InfoText, L"%s", newStr);
	}

	nclog(L"creating info dialog ('%s'/'%s'/'%s'...\n", regVal_InfoText, regVal_InfoButton1, regVal_InfoButton2);
	g_hDlgInfo = createDlgInfo(g_hWnd, regVal_InfoText, regVal_InfoButton1, regVal_InfoButton2);

	while (GetMessage (&msg , NULL , 0 , 0))   
	{
		if ( !IsWindow(g_hDlgInfo) || !IsDialogMessage(g_hDlgInfo, &msg) ) {
		  TranslateMessage (&msg) ;         
		  DispatchMessage  (&msg) ;         
		}
	} 
                                                                              
	stopIdleThread();
	nclog(L"%s\n#### END %s ####\n", logDateTime(), szAppName);
	return msg.wParam ;
/*
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
*/
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

	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IHOOKIDLE));
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
    HWND hWnd;
    TCHAR szTitle[MAX_LOADSTRING];		// title bar text
    TCHAR szWindowClass[MAX_LOADSTRING];	// main window class name

    g_hInstance = hInstance; // Store instance handle in our global variable

    // SHInitExtraControls should be called once during your application's initialization to initialize any
    // of the device specific controls such as CAPEDIT and SIPPREF.
    SHInitExtraControls();

    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING); 
    LoadString(hInstance, IDC_IHOOKIDLE, szWindowClass, MAX_LOADSTRING);

    //If it is already running, then focus on the window, and exit
    hWnd = FindWindow(szWindowClass, szTitle);	
    if (hWnd) 
    {
        // set focus to foremost child window
        // The "| 0x00000001" is used to bring any owned windows to the foreground and
        // activate them.
        SetForegroundWindow((HWND)((ULONG) hWnd | 0x00000001));
        return 0;
    } 

    if (!MyRegisterClass(hInstance, szWindowClass))
    {
    	return FALSE;
    }

    hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        return FALSE;
    }

    // When the main window is created using CW_USEDEFAULT the height of the menubar (if one
    // is created is not taken into account). So we resize the window after creating it
    // if a menubar is present
    if (g_hWndMenuBar)
    {
        RECT rc;
        RECT rcMenuBar;

        GetWindowRect(hWnd, &rc);
        GetWindowRect(g_hWndMenuBar, &rcMenuBar);
        rc.bottom -= (rcMenuBar.bottom - rcMenuBar.top);
		
        MoveWindow(hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
    }

    ShowWindow(hWnd, SW_HIDE);// nCmdShow);
    UpdateWindow(hWnd);

	//ReadReg(); //see InitInstance
	//readRegDlg();	//do it here or in dialog init?

	//INSTALL the HOOK
	if (g_HookActivate(g_hInstance))
	{
		MessageBeep(MB_OK);
		//system bar icon
		//ShowIcon(hwnd, g_hInstance);
		//TRACE(_T("Hook loaded OK"));
	}
	else
	{
		MessageBeep(MB_ICONEXCLAMATION);
		MessageBox(hWnd, L"Could not hook. Already running a copy of KeyToggleBoot? Will exit now.", L"KeyToggleBoot", MB_OK | MB_ICONEXCLAMATION);
		//TRACE(_T("Hook did not success"));
		PostQuitMessage(-1);
	}

	//start the idle alarm thread
	if(regValEnableAlarm==1)
		startIdleThread(regValIdleTimeout);


	//Notification icon
	HICON hIcon;
	hIcon=(HICON) LoadImage (g_hInstance, MAKEINTRESOURCE (IHOOK_STARTED), IMAGE_ICON, 16,16,0);
	nid.cbSize = sizeof (NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE;
	// NIF_TIP not supported    
	nid.uCallbackMessage = MYMSG_TASKBARNOTIFY;
	nid.hIcon = hIcon;
	nid.szTip[0] = '\0';
	BOOL res = Shell_NotifyIcon (NIM_ADD, &nid);
#ifdef DEBUG
	if (!res)
		ShowError(GetLastError());
#endif
	


    return TRUE;
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
    PAINTSTRUCT ps;
    HDC hdc;

    static SHACTIVATEINFO s_sai;
	
    switch (message) 
    {
		case MYMSG_TASKBARNOTIFY:
				switch (lParam) {
					case WM_LBUTTONUP:
						ShowWindow(hWnd, SW_SHOWMAXIMIZED);
						UpdateWindow(hWnd);
						SetWindowPos(hWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOSIZE | SWP_NOREPOSITION | SWP_SHOWWINDOW);
						if (MessageBox(hWnd, L"Hook is loaded. End hooking?", szAppName, 
							MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL | MB_SETFOREGROUND | MB_TOPMOST)==IDYES)
						{
							g_HookDeactivate();
							Shell_NotifyIcon(NIM_DELETE, &nid);
							nclog(L"Exit requested\n");
							PostQuitMessage (0) ; 
						}
						ShowWindow(hWnd, SW_HIDE);
					}
			return 0;
			break;
		case WM_COMMAND:
            wmId    = LOWORD(wParam); 
            wmEvent = HIWORD(wParam); 
            // Parse the menu selections:
            switch (wmId)
            {
                case IDM_HELP_ABOUT:
                    DialogBox(g_hInstance, (LPCTSTR)IDD_ABOUTBOX, hWnd, About);
                    break;
                case IDM_OK:
                    SendMessage (hWnd, WM_CLOSE, 0, 0);				
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        case WM_CREATE:
            SHMENUBARINFO mbi;

            memset(&mbi, 0, sizeof(SHMENUBARINFO));
            mbi.cbSize     = sizeof(SHMENUBARINFO);
            mbi.hwndParent = hWnd;
            mbi.nToolBarId = IDR_MENU;
            mbi.hInstRes   = g_hInstance;

            if (!SHCreateMenuBar(&mbi)) 
            {
                g_hWndMenuBar = NULL;
            }
            else
            {
                g_hWndMenuBar = mbi.hwndMB;
            }

            // Initialize the shell activate info structure
            memset(&s_sai, 0, sizeof (s_sai));
            s_sai.cbSize = sizeof (s_sai);

			startWatchScanThread(hWnd);

            break;
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            
            // TODO: Add any drawing code here...
            
            EndPaint(hWnd, &ps);
            break;
        case WM_DESTROY:
            CommandBar_Destroy(g_hWndMenuBar);
			
			g_HookDeactivate();
			Shell_NotifyIcon(NIM_DELETE, &nid);
		
			if(g_hDlgInfo)
				DestroyWindow(g_hDlgInfo);
			
			if(watchScanEventThreadSTOP!=NULL)
				SetEvent(watchScanEventThreadSTOP);

			PostQuitMessage(0);
            break;

        case WM_ACTIVATE:
            // Notify shell of our activate message
            SHHandleWMActivate(hWnd, wParam, lParam, &s_sai, FALSE);
            break;
        case WM_SETTINGCHANGE:
            SHHandleWMSettingChange(hWnd, wParam, lParam, &s_sai);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
            {
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

    }
    return (INT_PTR)FALSE;
}
