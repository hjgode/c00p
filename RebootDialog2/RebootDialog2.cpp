//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include "stdafx.h"
#include "RebootDialog2.h"
#include <windows.h>
#include <commctrl.h>
#include <pm.h>

//############# CleanBoot START #################
#include "pkfuncs.h"

#define PLATFORM_ITC_IOCTL_BASE					0x8B0	// 0x8B0 - 0xF9F
#define IOCTL_HAL_ITC_CLEAN_DEVICE              CTL_CODE(FILE_DEVICE_HAL, PLATFORM_ITC_IOCTL_BASE + 60, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define CORE_OEM_IOCTL_BASE						0x800	// 0x800 - 0x8AF
#define IOCTL_HAL_WARMBOOT						CTL_CODE( FILE_DEVICE_HAL, CORE_OEM_IOCTL_BASE + 4,  METHOD_BUFFERED, FILE_ANY_ACCESS)

#define HIVECLEANFLAG_SYSTEM			 1
#define HIVECLEANFLAG_USERS			 2
#define CLEANFLAG_CLEANBOOT			16
#define CLEANFLAG_DEFAULT_DATE			17
#define CLEANFLAG_FORMAT_OBJECT_STORE		18
#define CLEANFLAG_FORMAT_SECONDARY_STORAGE	19

#define ITC_CLEAR_RESTORE_FLAGS		0
#define ITC_FORMAT_OBJECT_STORE		( 1 << CLEANFLAG_FORMAT_OBJECT_STORE )
#define ITC_FORMAT_SECONDARY_STORAGE	( 1 << CLEANFLAG_FORMAT_SECONDARY_STORAGE )
#define ITC_RESTORE_USER_HIVE		( 1 << HIVECLEANFLAG_USERS )
#define ITC_RESTORE_SYSTEM_HIVE		( 1 << HIVECLEANFLAG_SYSTEM )
#define ITC_RESTORE_SYSTEM_TIME		( 1 << CLEANFLAG_DEFAULT_DATE )
#define ITC_CLEAN_BOOT			( ITC_FORMAT_OBJECT_STORE | ITC_RESTORE_USER_HIVE | ITC_RESTORE_SYSTEM_HIVE | ITC_RESTORE_SYSTEM_TIME )

DWORD	dwDevRestoreVal = ITC_RESTORE_USER_HIVE | ITC_RESTORE_SYSTEM_HIVE | ITC_FORMAT_OBJECT_STORE | ITC_RESTORE_SYSTEM_TIME;

BOOL SetCleanBootFlags() 
{
	BOOL  status;
	DWORD bytes_returned;
	status = KernelIoControl( IOCTL_HAL_ITC_CLEAN_DEVICE, ( void * )&dwDevRestoreVal, sizeof( DWORD ), NULL, 0, &bytes_returned );
	if( status ) {
		RETAILMSG( ( -1 ), ( TEXT( "\tKernelIoControl successfull!\r\n" ) ) );
	}
	else {
		RETAILMSG( ( -1 ), ( TEXT( "\tKernelIoControl failed!\r\n" ) ) );
	}

	RETAILMSG( ( -1 ), ( TEXT( "--RestoreDevice App\r\n" ) ) );
	return status;
}

void OnWarmBoot() 
{
	BOOL  status;
	DWORD bytes_returned;
	status = KernelIoControl( IOCTL_HAL_WARMBOOT, NULL, 0, NULL, 0, &bytes_returned );

	//TODO: Add check to see that changes haven't been made since last "OnUpdate()" & alert if necessary
}
//############# CleanBoot ENDE #################

//cn50 shutdown stuff
#include <winioctl.h>
/*
extern "C" __declspec(dllimport) BOOL KernelIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
*/
#define IOCTL_HAL_REBOOT CTL_CODE(FILE_DEVICE_HAL, 15, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define QC_IOCTL_POWER_DOWN_REQ CTL_CODE(257, 3010, METHOD_BUFFERED, METHOD_BUFFERED)

#include "registry.h"

TCHAR tszAppName[] = TEXT("REBOOTDIALOG2");
TCHAR tszTitle[]   = TEXT("RebootDialog 2");

#define WM_DISPOSEDIALOG	WM_USER + 1010
#define WM_STEP				WM_USER + 1011

HINSTANCE g_hInst = NULL;                     // Local copy of hInstance
HANDLE    g_hEventShutDown = NULL;            
HANDLE    g_hPowerNotificationThread = NULL;  
HWND      g_hSystemState = NULL;

HWND	g_hWnd=NULL;

DWORD PowerNotificationThread(LPVOID pVoid);

//***************************************************************************
//progress bar
HWND hProgress=NULL;

#define PROGRESS_TIMER 10210

int iTimerIDProgress=0;
int iMaxTime=15;
int iProgressHeight=26;
HWND hCountdown=NULL;
static int iAutoend = 1;

int WINAPI fnTimerProc(HWND hWnd, UINT uMsg, UINT * uTimerID, DWORD dwTime){
	if(hProgress!=NULL){
		//SendMessage(hProgress, PBM_STEPIT, 0, 0); //crash?
		PostMessage(hWnd, WM_STEP, 0, 0);
	}
	if(hCountdown!=NULL){
		TCHAR sStr[16];
		_itow(iMaxTime - iAutoend, sStr, 10);
		SetWindowText(hCountdown, sStr);
	}
	if(hCountdown==NULL){
        InvalidateRect(g_hWnd, NULL, TRUE); //force a repaint
	}
	iAutoend++;
	if(iAutoend >= iMaxTime)
		PostMessage(g_hWnd, WM_DISPOSEDIALOG, 0, 0);// EndDialog(hWnd, IDCANCEL);
	return 0;
}

void CN50_Shutdown(){
	KernelIoControl(QC_IOCTL_POWER_DOWN_REQ, NULL, 0, NULL, 0, NULL);
	SetSystemPowerState(NULL, POWER_STATE_OFF, POWER_FORCE);
}

BOOL ResetPocketPC()
{
	SetSystemPowerState(NULL, POWER_STATE_RESET, 0);
	return 0;
	//DWORD dwRet=0;
	//return KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, &dwRet);
}

BOOL SuspendPPC(){
	keybd_event(VK_OFF, 0, 0, 0);
	Sleep(5);
	keybd_event(VK_OFF, 0, KEYEVENTF_KEYUP, 0);
	return TRUE;
}

struct stringTable{
	TCHAR szSuspend[MAX_PATH];
	TCHAR szReboot[MAX_PATH];
	TCHAR szShutdown[MAX_PATH];
	TCHAR szCancel[MAX_PATH];
	TCHAR szQuestion[MAX_PATH];
	TCHAR szCleanBoot[MAX_PATH];
};
stringTable _stringTable;

BOOL readReg(){
	BOOL bRet=TRUE;
	TCHAR* strVal = new TCHAR(MAX_PATH);
	if(RegReadStr(L"CleanBootText", strVal)==ERROR_SUCCESS){
		wsprintf(_stringTable.szCleanBoot, L"%s", strVal);
	}
	else
		bRet=FALSE;
	if(RegReadStr(L"SuspendText", strVal)==ERROR_SUCCESS){
		wsprintf(_stringTable.szSuspend, L"%s", strVal);
	}
	else
		bRet=FALSE;
	if(RegReadStr(L"RebootText", strVal)==ERROR_SUCCESS){
		wsprintf(_stringTable.szReboot, L"%s", strVal);
	}
	else
		bRet=FALSE;
	if(RegReadStr(L"ShutdownText", strVal)==ERROR_SUCCESS){
		wsprintf(_stringTable.szShutdown, L"%s", strVal);
	}
	else
		bRet=FALSE;
	if(RegReadStr(L"CancelText", strVal)==ERROR_SUCCESS){
		wsprintf(_stringTable.szCancel, L"%s", strVal);
	}
	else
		bRet=FALSE;
	if(RegReadStr(L"QuestionText", strVal)==ERROR_SUCCESS){
		wsprintf(_stringTable.szQuestion, L"%s", strVal);
	}
	else
		bRet=FALSE;

	return bRet;
}

BOOL writeReg(){
	BOOL bRet=FALSE;
	OpenCreateKey(L"Software\\Intermec\\RebootDialog2");
	if(! RegWriteStr(L"CleanBootText", _stringTable.szCleanBoot)==ERROR_SUCCESS)
		bRet=FALSE;
	if(! RegWriteStr(L"SuspendText", _stringTable.szSuspend)==ERROR_SUCCESS)
		bRet=FALSE;
	if(! RegWriteStr(L"RebootText", _stringTable.szReboot)==ERROR_SUCCESS)
		bRet=FALSE;
	if(! RegWriteStr(L"ShutdownText", _stringTable.szShutdown)==ERROR_SUCCESS)
		bRet=FALSE;
	if(! RegWriteStr(L"CancelText", _stringTable.szCancel)==ERROR_SUCCESS)
		bRet=FALSE;
	if(! RegWriteStr(L"QuestionText", _stringTable.szQuestion)==ERROR_SUCCESS)
		bRet=FALSE;
	return bRet;
}

void startProgress(HWND hWnd){
	iTimerIDProgress = SetTimer(hWnd, PROGRESS_TIMER, 1000, (TIMERPROC) fnTimerProc);
}

//***************************************************************************
// Function Name: SetBacklightRequirement
//
// Purpose: Sets or releases the device power requirement to keep the 
//          backlight at D0
//
// Arguments:
//  IN BOOL fBacklightOn - TRUE to leave the backlight on
//
void SetBacklightRequirement(BOOL fBacklightOn)
{
    // the name of the backlight device
    TCHAR tszBacklightName[] = TEXT("BKL1:"); 

    static HANDLE s_hBacklightReq = NULL;
    
    if (fBacklightOn) 
    {
        if (NULL == s_hBacklightReq) 
        {
            // Set the requirement that the backlight device must remain
            // in device state D0 (full power)
            s_hBacklightReq = SetPowerRequirement(tszBacklightName, D0, 
                                                  POWER_NAME, NULL, 0);
            if (!s_hBacklightReq)
            {
                RETAILMSG(1, (L"SetPowerRequirement failed: %X\n", 
                              GetLastError()));
            }
        }
    } 
    else 
    {
        if (s_hBacklightReq) 
        {
            if (ERROR_SUCCESS != ReleasePowerRequirement(s_hBacklightReq))
            {
                RETAILMSG(1, (L"ReleasePowerRequirement failed: %X\n",
                              GetLastError()));
            }
            s_hBacklightReq = NULL;
        }
    }
}


//***************************************************************************
// Function Name: UpdatePowerState
//
// Purpose: Updates the name of the current power state in the dialog
//
void UpdatePowerState( void ) 
{
	return;

    TCHAR szState[MAX_PATH];
    DWORD dwState;

    if (g_hSystemState) 
    {
        if (ERROR_SUCCESS == GetSystemPowerState(szState, MAX_PATH, &dwState)) 
        {
            SetWindowText(g_hSystemState, szState);
        }
    }

}

//***************************************************************************
// Function Name: InitWindow
//
// Purpose: Initializes dialog and soft key menus
//
BOOL InitWindow(const HWND hDlg, UINT nToolBarId)
{
        // Specify that the dialog box should stretch full screen
        SHINITDLGINFO shidi;
        ZeroMemory(&shidi, sizeof(shidi));
        shidi.dwMask = SHIDIM_FLAGS;
        shidi.dwFlags = SHIDIF_SIZEDLGFULLSCREEN;
        shidi.hDlg = hDlg;

        // set up Soft Keys menu
        SHMENUBARINFO mbi;
        ZeroMemory(&mbi, sizeof(SHMENUBARINFO));
        mbi.cbSize = sizeof(SHMENUBARINFO);
        mbi.hwndParent = hDlg;
        mbi.nToolBarId = nToolBarId;
        mbi.hInstRes = g_hInst;

        // If we could not initialize the dialog box, return an error
        if (FALSE == (SHInitDialog(&shidi) && SHCreateMenuBar(&mbi)))
        {
            return FALSE;
        }
		//SHDoneButton(hDlg, FALSE);
		SHFullScreen(hDlg, SHFS_HIDETASKBAR | SHFS_HIDESTARTICON | SHFS_HIDESIPBUTTON);

        // set the title bar
        SetWindowText(hDlg, tszTitle);

        return TRUE;
}

//***************************************************************************
int iMargin=8;
int iButtonWidth=60;
int iButtonHeight=30;
int iTopNext=4;
int iTextHeight=35;

//***************************************************************************
// Function Name: WndProc(HWND, unsigned, WORD, LONG)
//
// Purpose: Processes messages for the main window.
//
BOOL CALLBACK DlgProc (
        HWND hWnd, 
        UINT Msg, 
        WPARAM wParam, 
        LPARAM lParam)
{
    BOOL fReturn = TRUE;
    HANDLE hThread = NULL;
    HWND hwndBacklight = NULL;
    static BOOL s_fIdled = FALSE;
	int iRes;
	TCHAR szBuf[MAX_PATH];
	RECT rect; 	RECT rechteck;

    HDC hdc;
//	int iWidth;
    PAINTSTRUCT ps;
	int iW;
	static HBRUSH hBrush;
	HBRUSH hBrushOld;

    switch(Msg)
    {
		case WM_INITDIALOG:
			g_hWnd = hWnd;
            // Initialize the dialog and softkey menu
            if (FALSE == InitWindow(hWnd, IDR_MENU))
            {
                EndDialog(hWnd, -1);
            }

            // Start up the thread to get the power notifications
            //g_hEventShutDown = CreateEvent(NULL, FALSE, FALSE, NULL);
            //if (NULL != g_hEventShutDown)  
            //{
            //    g_hPowerNotificationThread = 
            //        CreateThread(0, 0, PowerNotificationThread, NULL, 0, 0);
            //}

            ///g_hSystemState = GetDlgItem(hwnd, IDC_SYSTEMSTATE);

			//try to read strings from resource			
			if(iRes=LoadString(g_hInst, IDS_BTNWARMBOOT, szBuf, MAX_PATH))
				wcscpy(_stringTable.szReboot, szBuf);

			if(iRes=LoadString(g_hInst, IDS_BTNCANCEL, szBuf, MAX_PATH))
				wcscpy(_stringTable.szCancel, szBuf);

			if(iRes=LoadString(g_hInst, IDS_BTNSHUTDOWN, szBuf, MAX_PATH))
				wcscpy(_stringTable.szShutdown, szBuf);

			if(iRes=LoadString(g_hInst, IDS_BTNSUSPEND, szBuf, MAX_PATH))
				wcscpy(_stringTable.szSuspend, szBuf);

			if(iRes=LoadString(g_hInst, IDS_BTNCLEANBOOT, szBuf, MAX_PATH))
				wcscpy(_stringTable.szCleanBoot, szBuf);

			if(iRes=LoadString(g_hInst, IDS_DIALOGTEXT, szBuf, MAX_PATH))
				wcscpy(_stringTable.szQuestion, szBuf);

			//try to read strings from registry
			if(! readReg()){
				writeReg();
			}

			//add code to check for CN50
			TCHAR platFormName[MAX_PATH];
			if(ReadPlatformName(platFormName)==0){
				if(wcsicmp(platFormName, L"CN50")==0)
					ShowWindow(GetDlgItem(hWnd, IDC_BTNSHUTDOWN), SW_SHOW);
			}
			else
				ShowWindow(GetDlgItem(hWnd, IDC_BTNSHUTDOWN), SW_HIDE);

			SetWindowText(GetDlgItem(hWnd, IDC_DIALOGTEXT), _stringTable.szQuestion);
			SetWindowText(GetDlgItem(hWnd, IDC_BTNWARMBOOT), _stringTable.szReboot);
			SetWindowText(GetDlgItem(hWnd, IDC_BTNCANCEL), _stringTable.szCancel);
			SetWindowText(GetDlgItem(hWnd, IDC_BTNSHUTDOWN), _stringTable.szShutdown);
			SetWindowText(GetDlgItem(hWnd, IDC_BTNSUSPEND), _stringTable.szSuspend);
			SetWindowText(GetDlgItem(hWnd, IDC_BTNCLEANBOOT), _stringTable.szCleanBoot);

			hCountdown= GetDlgItem(hWnd, IDC_EDIT2);
			SetWindowText(hCountdown, L"-");

			hProgress = GetDlgItem(hWnd, IDC_PROGRESS1);

			if (hProgress != NULL)
			{
				SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, iMaxTime)); // set range from 0 to 30// 100
				SendMessage(hProgress, PBM_SETSTEP, MAKEWPARAM(1, 0), 0); 
			}
			startProgress(hWnd);

            // Update the current state in the dialog
            //UpdatePowerState();
            break;

        case WM_COMMAND:
			DEBUGMSG(1, (L"WM_COMMAND with wmId=%04x\r\n", wParam));
			EnableWindow(hWnd, FALSE);
			SetCursor(LoadCursor(NULL, IDC_WAIT));
			ShowCursor(TRUE);

            switch (wParam)
            {
                case IDM_QUIT:
                    EndDialog(hWnd, IDOK);
                    break;

                //default: fReturn = FALSE;

                case IDC_BTNCANCEL:
                    EndDialog(hWnd, IDCANCEL);
                    PostQuitMessage(1);
                    break;
				case IDC_BTNSHUTDOWN:
                    EndDialog(hWnd, IDC_BTNSHUTDOWN);
					//PostQuitMessage(2);
					//CN50_Shutdown();
					break;
                case IDC_BTNSUSPEND:
                    EndDialog(hWnd, IDC_BTNSUSPEND);
					//PostQuitMessage(3);
					//SuspendPPC();
                    break;
				case IDC_BTNWARMBOOT:
                    EndDialog(hWnd, IDC_BTNWARMBOOT);
					//PostQuitMessage(4);
					//ResetPocketPC();
					break;
				case IDC_BTNCLEANBOOT:
                    EndDialog(hWnd, IDC_BTNCLEANBOOT);
					break;
                default:
					EnableWindow(hWnd, TRUE);
					SetCursor(LoadCursor(NULL, IDC_ARROW));
					ShowCursor(FALSE);
                    return DefWindowProc(hWnd, Msg, wParam, lParam);
            }
			EnableWindow(hWnd, TRUE);
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			ShowCursor(FALSE);
            break;
		case WM_DISPOSEDIALOG:
			EndDialog(hWnd, IDCANCEL);
			break;
		case WM_STEP:
			SendMessage(hProgress, PBM_STEPIT, 0, 0);
			break;
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code here...
            //add rectangle instead of progressbar
			GetWindowRect(hWnd, &rect);
            //InvalidateRect(hWnd, &rect, FALSE);
			rechteck.top=iMargin;
			rechteck.left=iMargin;

			iW=/* rect.right-iMargin - */iAutoend*((rect.right-iMargin)/iMaxTime);

			rechteck.bottom=iProgressHeight;
			hBrushOld = (HBRUSH) SelectObject(hdc, hBrush);

			rechteck.right=iW;
			iW = FillRect(hdc, &rechteck, hBrush); //(HBRUSH)COLOR_ACTIVECAPTION
			SelectObject(hdc, hBrushOld);
			
            EndPaint(hWnd, &ps);
			ReleaseDC(hWnd, hdc);
            break;

        //case WM_TIMER:
        //    // Set the system state to full power
        //    KillTimer(hWnd, 1);
        //    SetSystemPowerState(NULL, POWER_STATE_ON, 0);
        //    s_fIdled = FALSE;
        //    break;

        case WM_DESTROY:
            SetBacklightRequirement(FALSE);

			EnableWindow(hWnd, TRUE);
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			ShowCursor(FALSE);

            // Kill the power notification thread
            if (NULL != g_hEventShutDown)
            {
                SetEvent(g_hEventShutDown);

                if (NULL != g_hPowerNotificationThread)
                {
                    WaitForSingleObject(g_hPowerNotificationThread , INFINITE);
                    CloseHandle(g_hPowerNotificationThread);
                }

                CloseHandle(g_hEventShutDown);
            }    
			DeleteObject(hBrush);
            break;

        default:
            fReturn = FALSE;
            break;
    }

    return fReturn;
}

//***************************************************************************
// Function: WinMain
//  
// Purpose: Standard Win32 Entry point
//      
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPTSTR    lpCmdLine,
                   int       nCmdShow)
{
    int nResult = 0;

    // Create mutex to track whether or not an application is already running
    HANDLE hMutex = CreateMutex(0, 0, _T("_PMSAMPLE_EXE_MUTEX_"));

    // check the result code
    if (NULL != hMutex)
    {
        if (ERROR_ALREADY_EXISTS != GetLastError())
        {
            // If the mutex doesn't already exist, create the dialog and start it.
            g_hInst = hInstance;

			//the call will block until some button or menu is clicked
			InitCommonControls();
            nResult = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_REBOOTDIALOG
				),
                                NULL, (DLGPROC)DlgProc);
			switch(nResult){
				case IDC_BTNCANCEL:
				case IDCANCEL:
					break;
				case IDC_BTNSHUTDOWN:
					CN50_Shutdown();
					break;
				case IDC_BTNSUSPEND:
					SuspendPPC();
					break;
				case IDC_BTNWARMBOOT:
					ResetPocketPC();
					break;
				case IDC_BTNCLEANBOOT:
					if (SetCleanBootFlags())
						OnWarmBoot();
					break;
				default:
					break;
			}
        }
        else
        {
            // Already an instance running - attempt to switch to it and then exit
            HWND hWndExistingInstance = FindWindow(_T("Dialog"), tszTitle);

            if (NULL != hWndExistingInstance)
            {
                SetForegroundWindow(hWndExistingInstance);
            }
        }

        CloseHandle(hMutex);
    }

    return ( nResult );// >= 0 );
}


//***************************************************************************
// Function Name: PowerNotificationThread
//
// Purpose: listens for power change notifications
//
DWORD PowerNotificationThread(LPVOID pVoid)
{
    // size of a POWER_BROADCAST message
    DWORD cbPowerMsgSize = sizeof POWER_BROADCAST + (MAX_PATH * sizeof TCHAR);

    // Initialize our MSGQUEUEOPTIONS structure
    MSGQUEUEOPTIONS mqo;
    mqo.dwSize = sizeof(MSGQUEUEOPTIONS); 
    mqo.dwFlags = MSGQUEUE_NOPRECOMMIT;
    mqo.dwMaxMessages = 4;
    mqo.cbMaxMessage = cbPowerMsgSize;
    mqo.bReadAccess = TRUE;              
                                         
    // Create a message queue to receive power notifications
    HANDLE hPowerMsgQ = CreateMsgQueue(NULL, &mqo);
    if (NULL == hPowerMsgQ) 
    {
        RETAILMSG(1, (L"CreateMsgQueue failed: %x\n", GetLastError()));
        goto Error;
    }

    // Request power notifications 
    HANDLE hPowerNotifications = RequestPowerNotifications(hPowerMsgQ,
                                                           PBT_TRANSITION | 
                                                           PBT_RESUME | 
                                                           PBT_POWERINFOCHANGE);
    if (NULL == hPowerNotifications) 
    {
        RETAILMSG(1, (L"RequestPowerNotifications failed: %x\n", GetLastError()));
        goto Error;
    }

    HANDLE rgHandles[2] = {0};
    rgHandles[0] = hPowerMsgQ;
    rgHandles[1] = g_hEventShutDown;

    // Wait for a power notification or for the app to exit
    while(WaitForMultipleObjects(2, rgHandles, FALSE, INFINITE) == WAIT_OBJECT_0)
    {
        DWORD cbRead;
        DWORD dwFlags;
        POWER_BROADCAST *ppb = (POWER_BROADCAST*) new BYTE[cbPowerMsgSize];
            
        // loop through in case there is more than 1 msg 
        while(ReadMsgQueue(hPowerMsgQ, ppb, cbPowerMsgSize, &cbRead, 
                           0, &dwFlags))
        {
            switch (ppb->Message)
            {
                case PBT_TRANSITION:
                    RETAILMSG(1,(L"Power Notification Message: PBT_TRANSITION\n"));
                    RETAILMSG(1,(L"Flags: %lx", ppb->Flags));
                    RETAILMSG(1,(L"Length: %d", ppb->Length));
                    if (ppb->Length)
                    {
                        RETAILMSG(1,(L"SystemPowerState: %s\n", ppb->SystemPowerState));
                    }
                    break;

                case PBT_RESUME:
                    RETAILMSG(1,(L"Power Notification Message: PBT_RESUME\n"));
                    break;

                case PBT_POWERINFOCHANGE:
                {
                    RETAILMSG(1,(L"Power Notification Message: PBT_POWERINFOCHANGE\n"));

                    // PBT_POWERINFOCHANGE message embeds a 
                    // POWER_BROADCAST_POWER_INFO structure into the 
                    // SystemPowerState field
                    PPOWER_BROADCAST_POWER_INFO ppbpi =
                        (PPOWER_BROADCAST_POWER_INFO) ppb->SystemPowerState;
                    if (ppbpi) 
                    {
                        RETAILMSG(1,(L"Length: %d", ppb->Length));
                        RETAILMSG(1,(L"BatteryLifeTime = %d\n",ppbpi->dwBatteryLifeTime));
                        RETAILMSG(1,(L"BatterFullLifeTime = %d\n",
                                     ppbpi->dwBatteryFullLifeTime));
                        RETAILMSG(1,(L"BackupBatteryLifeTime = %d\n",
                                     ppbpi->dwBackupBatteryLifeTime));
                        RETAILMSG(1,(L"BackupBatteryFullLifeTime = %d\n",
                                     ppbpi->dwBackupBatteryFullLifeTime));
                        RETAILMSG(1,(L"ACLineStatus = %d\n",ppbpi->bACLineStatus));
                        RETAILMSG(1,(L"BatteryFlag = %d\n",ppbpi->bBatteryFlag));
                        RETAILMSG(1,(L"BatteryLifePercent = %d\n",
                                     ppbpi->bBatteryLifePercent));
                        RETAILMSG(1,(L"BackupBatteryFlag = %d\n",
                                     ppbpi->bBackupBatteryFlag));
                        RETAILMSG(1,(L"BackupBatteryLifePercent = %d\n",
                                     ppbpi->bBackupBatteryLifePercent));
                    }
                    break;
                }

                default:
                    break;
            }

            UpdatePowerState();
        }

        delete[] ppb;
    }

Error:
    if (hPowerNotifications)
        StopPowerNotifications(hPowerNotifications);

    if (hPowerMsgQ)
        CloseMsgQueue(hPowerMsgQ);

    return NULL;
}

