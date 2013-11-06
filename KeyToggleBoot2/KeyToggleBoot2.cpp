// KeyToggleBoot2.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "KeyToggleBoot2.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE	g_hInst;			// current instance
HWND		g_hWnd = NULL;		// main window handle

// Forward declarations of functions included in this code module:
ATOM			MyRegisterClass(HINSTANCE, LPTSTR);
BOOL			InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

// ##################### custom stuff *START*
#include "hooks.h"
#include  <nled.h>
#include "keymap.h"	//the char to vkey mapping

#include "registry.h"
#define REGKEY L"Software\\Intermec\\KeyToggleBoot"
#define REGNAME_TIMEOUT L"Timeout"
#define REGNAME_LEDID L"LEDid"
#define REGNAME_KEYSEQ L"KeySeq"
#define REGNAME_FORBIDDENKEYS L"ForbiddenKeys"

#define WM_SHOWMYDIALOG WM_USER + 5241

UINT  matchTimeout = 3000;  //if zero, no autofallback

HINSTANCE	g_hHookApiDLL	= NULL;			// Handle to loaded library (system DLL where the API is located)

TCHAR szAppName[] = L"KeyToggleBoot2 v1.01";
TCHAR szKeySeq[10]; //hold a max of ten chars
char szKeySeqA[10]; //same as char list

char szVKeySeq[10];
bool bCharShiftSeq[10];

int iKeyCount=3;
int iMatched=0;

BYTE* pForbiddenKeyList = NULL;
static bool isStickyOn=false;
static int tID=1011; //TimerID

NOTIFYICONDATA nid;

//ITC 7xx supports 5 LEDs?
//using 0 gives slow responsive by left amber LED light
//using 1 gives slow responsive by left red LED light
//using 2 gives immediate left red LED light
//using 3 gives immediate left green LED light
//using 4 gives slow responsive right green LED light

int LEDid=3; //which LED to use

// Global functions: The original Open Source
BOOL g_HookDeactivate();
BOOL g_HookActivate(HINSTANCE hInstance);

#pragma data_seg(".HOOKDATA")									//	Shared data (memory) among all instances.
	HHOOK g_hInstalledLLKBDhook = NULL;						// Handle to low-level keyboard hook
#pragma data_seg()

#pragma comment(linker, "/SECTION:.HOOKDATA,RWS")		//linker directive

// from the platform builder <Pwinuser.h>
extern "C" {
BOOL WINAPI NLedGetDeviceInfo( UINT     nInfoId, void   *pOutput );
BOOL WINAPI NLedSetDevice( UINT nDeviceId, void *pInput );
};

//control the LEDs
void LedOn(int id, int onoff) //onoff=0 LED is off, onoff=1 LED is on
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
    if (!NLedSetDevice(NLED_SETTINGS_INFO_ID, &settings))
        DEBUGMSG(true,(L"NLedSetDevice(NLED_SETTINGS_INFO_ID) failed\n"));
	else
		DEBUGMSG(true,(L"NLedSetDevice(NLED_SETTINGS_INFO_ID) success\n"));
}

//timer proc which resets isStickyOn after a period
void CALLBACK Timer2Proc(
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
					DEBUGMSG(1, (L"FULL MATCH, starting ...\n"));
					PostMessage(g_hWnd, WM_SHOWMYDIALOG, 0, 0);
					//reset match pos and stop timer
					DEBUGMSG(1, (L"FULL MATCH: Reset matching\n"));
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

	// Load the core library. If it's not found, you've got CErious issues :-O
	//TRACE(_T("LoadLibrary(coredll.dll)..."));
	g_hHookApiDLL = LoadLibrary(_T("coredll.dll"));
	if(g_hHookApiDLL == NULL) return false;
	else {
		// Load the SetWindowsHookEx API call (wide-char)
		//TRACE(_T("OK\nGetProcAddress(SetWindowsHookExW)..."));
		SetWindowsHookEx = (_SetWindowsHookExW)GetProcAddress(g_hHookApiDLL, _T("SetWindowsHookExW"));
		if(SetWindowsHookEx == NULL) 
			return false;
		else
		{
			// Load the hook.  Save the handle to the hook for later destruction.
			//TRACE(_T("OK\nCalling SetWindowsHookEx..."));
			g_hInstalledLLKBDhook = SetWindowsHookEx(WH_KEYBOARD_LL, g_LLKeyboardHookCallback, hInstance, 0);
			if(g_hInstalledLLKBDhook == NULL){
				DEBUGMSG(1, (L"\n######## SetWindowsHookEx failed, GetLastError=%i\n", GetLastError()));
				return false;
			}
		}

		// Get pointer to CallNextHookEx()
		//TRACE(_T("OK\nGetProcAddress(CallNextHookEx)..."));
		CallNextHookEx = (_CallNextHookEx)GetProcAddress(g_hHookApiDLL, _T("CallNextHookEx"));
		if(CallNextHookEx == NULL) 
			return false;

		// Get pointer to UnhookWindowsHookEx()
		//TRACE(_T("OK\nGetProcAddress(UnhookWindowsHookEx)..."));
		UnhookWindowsHookEx = (_UnhookWindowsHookEx)GetProcAddress(g_hHookApiDLL, _T("UnhookWindowsHookEx"));
		if(UnhookWindowsHookEx == NULL) 
			return false;
	}
	DEBUGMSG(1, (L"g_HookActivate: OK\nEverything loaded OK\n"));
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
	DWORD rc=0;
	DWORD dwVal=0;
	rc = OpenCreateKey(REGKEY);
	if (rc != 0)
		ShowError(rc);

	if (rc=OpenKey(REGKEY) != 0)
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
	CloseKey();
}

int ReadReg()
{
	//for KeyToggleBoot we need to read the stickyKey to react on
	//and the timout for the sticky key
	byte dw=0;
	DWORD dwVal=0;

	OpenKey(REGKEY);
	
	//read the timeout for the StickyKey
	if (RegReadDword(L"Timeout", &dwVal)==0)
	{
		matchTimeout = (UINT) dwVal * 1000;
		DEBUGMSG(true,(L"Reading Timeout from REG = OK\n"));
	}
	else
	{
		matchTimeout = 5 * 1000;
		DEBUGMSG(true,(L"Reading Timeout from REG = FAILED, assigned 5 seconds\n"));
	}

    //read LEDid to use for signaling
    if (RegReadDword(L"LEDid", &dwVal)==0)
    {
        LEDid = dwVal;
        DEBUGMSG(true,(L"Reading LEDid from REG = OK\n"));
    }
    else
    {
        LEDid = 1;
        DEBUGMSG(true,(L"Reading LEDid from REG = FAILED, using default LedID\n"));
    }

	TCHAR szTemp[10];
    int iSize=RegReadByteSize(L"KeySeq", iSize);
	int iTableSizeOUT=iSize;
    if(iSize > 20){ //0x14 = 20 bytes, do we have more than 10 bytes?
        DEBUGMSG(1, (L"Failed reading KeyTable (iSize>20), using default\n"));
    }
    else{
        if (RegReadStr(L"KeySeq", szTemp)==ERROR_SUCCESS){
            DEBUGMSG(1, (L"Read KeySeq OK\n"));
			wcscpy(szKeySeq, szTemp);
			wcstombs(szKeySeqA, szKeySeq, 10);
            //memcpy(szKeySeq, bTemp, iSize);
        }
        else{
            DEBUGMSG(1, (L"Failed reading KeySeq, using default\n"));
        }
    }

	//convert from ANSI sequence to vkCode + shift
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
			DEBUGMSG(1,(L"Reading ForbiddenKeys from REG failed\n"));
		}
		else{
			DEBUGMSG(1,(L"Reading ForbiddenKeys from REG = OK\n"));
			pForbiddenKeyList[iSize+1]=0x00; //end marker
		}
	}
	else{
		pForbiddenKeyList=NULL;
		DEBUGMSG(1,(L"Reading ForbiddenKeys from REG failed\n"));
	}

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
	wsprintf(str,L"\nReadReg: Timeout=%i, , LEDid=%i, KeySeq='%s'\n'", matchTimeout, LEDid, szKeySeq);
	DEBUGMSG(true,(str));
	return 0;
}

// ##################### custom stuff *END*


int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR    lpCmdLine,
                   int       nCmdShow)
{
	MSG msg;

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
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

	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_KEYTOGGLEBOOT2));
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

    g_hInst = hInstance; // Store instance handle in our global variable

    // SHInitExtraControls should be called once during your application's initialization to initialize any
    // of the device specific controls such as CAPEDIT and SIPPREF.
    SHInitExtraControls();

    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING); 
    LoadString(hInstance, IDC_KEYTOGGLEBOOT2, szWindowClass, MAX_LOADSTRING);

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

	g_hWnd=hWnd;

    ShowWindow(hWnd, SW_HIDE);// nCmdShow);
    UpdateWindow(hWnd);

	if (g_HookActivate(hInstance))
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
		PostQuitMessage(-1);
	}

			//Notification icon
			HICON hIcon;
			hIcon=(HICON) LoadImage (hInstance, MAKEINTRESOURCE (IHOOK_STARTED), IMAGE_ICON, 16,16,0);
			nid.cbSize = sizeof (NOTIFYICONDATA);
			nid.hWnd = hWnd;
			nid.uID = 1;
			nid.uFlags = NIF_ICON | NIF_MESSAGE;
			// NIF_TIP not supported    
			nid.uCallbackMessage = MYMSG_TASKBARNOTIFY;
			nid.hIcon = hIcon;
			nid.szTip[0] = '\0';
			BOOL bRes = Shell_NotifyIcon (NIM_ADD, &nid);
			if(bRes)
				DEBUGMSG(1, (L"Init Shell_NotifyIcon OK\r\n"));
			else
				DEBUGMSG(1, (L"Init Shell_NotifyIcon FAILED\r\n"));


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
	static BOOL bRes;
	
    switch (message) 
    {
        case WM_COMMAND:
            wmId    = LOWORD(wParam); 
            wmEvent = HIWORD(wParam); 
            // Parse the menu selections:
            switch (wmId)
            {
                case IDM_HELP_ABOUT:
                    DialogBox(g_hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, About);
                    break;
                case IDM_OK:
                    SendMessage (hWnd, WM_CLOSE, 0, 0);				
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        case WM_CREATE:
			ReadReg();
			////Notification icon
			//HICON hIcon;
			//hIcon=(HICON) LoadImage (g_hInst, MAKEINTRESOURCE (IHOOK_STARTED), IMAGE_ICON, 16,16,0);
			//nid.cbSize = sizeof (NOTIFYICONDATA);
			//nid.hWnd = g_hWnd;
			//nid.uID = 1;
			//nid.uFlags = NIF_ICON | NIF_MESSAGE;
			//// NIF_TIP not supported    
			//nid.uCallbackMessage = MYMSG_TASKBARNOTIFY;
			//nid.hIcon = hIcon;
			//nid.szTip[0] = '\0';
			//bRes = Shell_NotifyIcon (NIM_ADD, &nid);
			//if(bRes)
			//	DEBUGMSG(1, (L"Init Shell_NotifyIcon OK\r\n"));
			//else
			//	DEBUGMSG(1, (L"Init Shell_NotifyIcon FAILED\r\n"));

			//g_HookActivate(g_hInst);
			break;
        case WM_PAINT:
			//PAINTSTRUCT ps;    
			RECT rect;    
			//HDC hdc;     // Adjust the size of the client rectangle to take into account    
			// the command bar height.    
			GetClientRect (hWnd, &rect);    
			hdc = BeginPaint (hWnd, &ps);     
			DrawText (hdc, TEXT ("KeyToggleBoot loaded"), -1, &rect,
				DT_CENTER | DT_VCENTER | DT_SINGLELINE);    
			EndPaint (hWnd, &ps);     
			return 0;
			break;
		case MYMSG_TASKBARNOTIFY:
				switch (lParam) {
					case WM_LBUTTONUP:
						//ShowWindow(hwnd, SW_SHOWNORMAL);
						SetWindowPos(hWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOSIZE | SWP_NOREPOSITION | SWP_SHOWWINDOW);
						if (MessageBox(hWnd, L"Hook is loaded. End hooking?", szAppName, 
							MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL | MB_SETFOREGROUND | MB_TOPMOST)==IDYES)
						{
							g_HookDeactivate();
							Shell_NotifyIcon(NIM_DELETE, &nid);
							PostQuitMessage (0) ; 
						}
						ShowWindow(hWnd, SW_HIDE);
					}
			return 0;
			break;

        case WM_DESTROY:
			g_HookDeactivate();
            PostQuitMessage(0);
            break;
		case WM_SHOWMYDIALOG:
			PROCESS_INFORMATION procInfo;
			memset(&procInfo, 0, sizeof(PROCESS_INFORMATION));
			TCHAR szImg[MAX_PATH];
			wsprintf(szImg, L"%s", L"\\Windows\\RebootDialog.exe");
			bRes = CreateProcess(szImg, NULL, NULL, NULL, NULL, CREATE_NEW_CONSOLE, NULL, NULL, NULL, &procInfo);
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
