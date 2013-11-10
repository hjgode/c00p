#include "stdafx.h"
#include "rebootdlg.h"

//cn50 shutdown stuff
#include <winioctl.h>
extern "C" __declspec(dllimport) BOOL KernelIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);


#define IOCTL_HAL_REBOOT CTL_CODE(FILE_DEVICE_HAL, 15, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define QC_IOCTL_POWER_DOWN_REQ CTL_CODE(257, 3010, METHOD_BUFFERED, METHOD_BUFFERED)

#include <pm.h>

#include "registry.h"

extern BOOL bShowSuspendButton;
extern BOOL bShowShutdownButton;

extern DWORD regValRebootExt;
extern TCHAR regValRebootExtParms[MAX_PATH];
extern TCHAR regValRebootExtApp[MAX_PATH];

extern DWORD regValShutdownExt;
extern TCHAR regValShutdownExtParms[MAX_PATH];
extern TCHAR regValShutdownExtApp[MAX_PATH];

#define TIMER1 10011
UINT TimerID=TIMER1;
void startTimer();

extern HWND g_hWnd_RebootDialog;
extern HANDLE handleEventCloseDialogAndSuspend;

void CN50_Shutdown(){
	if(regValShutdownExt==1)
	{
		//start external app
		TCHAR str[MAX_PATH];
		PROCESS_INFORMATION pi;
		if(CreateProcess(regValShutdownExtApp, regValShutdownExtParms, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)==0){
			wsprintf(str, L"CreateProcess ('%s'/'%s') failed with 0x%08x\r\n", regValShutdownExtApp, regValShutdownExtParms, GetLastError());
			DEBUGMSG(1,(str));
		}
		else{
			wsprintf(str, L"CreateProcess ('%s'/'%s') OK. pid=0x%08x\r\n", regValShutdownExtApp, regValShutdownExtParms, pi.dwProcessId);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			DEBUGMSG(1,(str));
		}
	}
	else if(regValShutdownExt==0){
		KernelIoControl(QC_IOCTL_POWER_DOWN_REQ, NULL, 0, NULL, 0, NULL);
		SetSystemPowerState(NULL, POWER_STATE_OFF, POWER_FORCE);
	}
}

BOOL ResetPocketPC()
{
	if(regValRebootExt==0){
		SetSystemPowerState(NULL, POWER_STATE_RESET, 0);
	}
	else if(regValRebootExt==1){
		//start external app
		TCHAR str[MAX_PATH];
		PROCESS_INFORMATION pi;
		if(CreateProcess(regValRebootExtApp, regValRebootExtParms, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)==0){
			wsprintf(str, L"CreateProcess ('%s'/'%s') failed with 0x%08x\r\n", regValRebootExtApp, regValRebootExtParms, GetLastError());
			DEBUGMSG(1,(str));
			return FALSE;
		}
		else{
			wsprintf(str, L"CreateProcess ('%s'/'%s') OK. pid=0x%08x\r\n", regValRebootExtApp, regValRebootExtParms, pi.dwProcessId);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			DEBUGMSG(1,(str));
			return TRUE;
		}
	}
	return 0;
	//DWORD dwRet=0;
	//return KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, &dwRet);
}

BOOL SuspendPPC(){
	//as we dont like to see the dialog after resume
	//we have to start an external app and terminate ourself
	__try{
	keybd_event(VK_OFF, 0, 0, 0);
	Sleep(5);
	keybd_event(VK_OFF, 0, KEYEVENTF_KEYUP, 0);
	}
	__except(EXCEPTION_EXECUTE_HANDLER){
		DEBUGMSG(1, (L"Exception in keybd_event=%i\r\n", GetLastError()));
	}
	return TRUE;
	/*

	//the external process will sleep 1 secoind and then suspend the device
	PROCESS_INFORMATION procInfo;
	memset(&procInfo, 0, sizeof(PROCESS_INFORMATION));
	TCHAR szImg[MAX_PATH];
	wsprintf(szImg, L"%s", L"\\Windows\\SuspendNow.exe");
	BOOL bRes=FALSE;
	__try{
		bRes = CreateProcess(szImg, NULL, NULL, NULL, NULL, CREATE_NEW_CONSOLE, NULL, NULL, NULL, &procInfo);
	}
	__except(EXCEPTION_EXECUTE_HANDLER){
		DEBUGMSG(1, (L"Exception in CreateProcess=%i\r\n", GetLastError()));
	}
	if(bRes)
	{
		DEBUGMSG(1, (L"Started process with procID=%i\r\n", procInfo.hProcess));
	}
	else
	{
		DEBUGMSG(1, (L"Starting Process FAILED: err=%i\r\n", GetLastError()));
	}
	CloseHandle(procInfo.hThread);
	CloseHandle(procInfo.hProcess);
	return bRes;
	*/
}

BOOL g_bRebootDialogOpen=false;

struct stringTable{
	TCHAR szSuspend[MAX_PATH];
	TCHAR szReboot[MAX_PATH];
	TCHAR szShutdown[MAX_PATH];
	TCHAR szCancel[MAX_PATH];
	TCHAR szQuestion[MAX_PATH];
};

stringTable _stringTable;

BOOL readRegDlg(){
	BOOL bRet=TRUE;
	TCHAR strVal[MAX_PATH];	//changed in 332
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

	DEBUGMSG(1,(L"readReg (rebootDlg): SuspendText='%s', RebootText='%s', ShutdownText='%s', CancelText='%s', QuestionText='%s'\n",
		_stringTable.szSuspend,
		_stringTable.szReboot,
		_stringTable.szShutdown,
		_stringTable.szCancel,
		_stringTable.szQuestion
		));
	return bRet;
}

BOOL writeRegDlg(){
	BOOL bRet=FALSE;
	wsprintf(_stringTable.szSuspend, L"Suspend");
	wsprintf(_stringTable.szReboot, L"Reboot");
	wsprintf(_stringTable.szSuspend, L"Suspend");
	wsprintf(_stringTable.szShutdown, L"Shutdown");
	wsprintf(_stringTable.szCancel, L"Cancel");
	wsprintf(_stringTable.szQuestion, L"Do you really want to reboot?");

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

BOOL CALLBACK RebootDialogProc (
                    HWND hwndDlg,   // Handle to the dialog box.
                    UINT uMsg,      // Message.
                    WPARAM wParam,  // First message parameter.
                    LPARAM lParam)  // Second message parameter.
{
	int iRes;
	TCHAR szBuf[MAX_PATH];
	HINSTANCE hInst;
	HWND hwndButton;
	  switch(uMsg)
	  {
		case WM_INITDIALOG:
			//try to read strings from resource
			hInst=GetModuleHandle(NULL);
			if(iRes=LoadString(hInst, IDS_BTNWARMBOOT, szBuf, MAX_PATH))
				SetDlgItemText(hwndDlg, IDC_BTNWARMBOOT, szBuf);

			if(iRes=LoadString(hInst, IDS_BTNCANCEL, szBuf, MAX_PATH))
				SetDlgItemText(hwndDlg, ID_BTNCANCEL, szBuf);

			if(iRes=LoadString(hInst, IDS_BTNSHUTDOWN, szBuf, MAX_PATH))
				SetDlgItemText(hwndDlg, IDC_BTNSHUTDOWN, szBuf);

			if(iRes=LoadString(hInst, IDS_BTNSUSPEND, szBuf, MAX_PATH))
				SetDlgItemText(hwndDlg, IDC_BTNSUSPEND, szBuf);

			if(iRes=LoadString(hInst, IDS_DIALOGTEXT, szBuf, MAX_PATH))
				SetDlgItemText(hwndDlg, IDC_DIALOGTEXT, szBuf);

			//try to read strings from registry
			if(readRegDlg()){
				SetDlgItemText(hwndDlg, IDC_BTNWARMBOOT, _stringTable.szReboot);
				SetDlgItemText(hwndDlg, ID_BTNCANCEL, _stringTable.szCancel);
				SetDlgItemText(hwndDlg, IDC_BTNSHUTDOWN, _stringTable.szShutdown);
				SetDlgItemText(hwndDlg, IDC_BTNSUSPEND, _stringTable.szSuspend);
				SetDlgItemText(hwndDlg, IDC_DIALOGTEXT, _stringTable.szQuestion);
			}
			else{
				GetDlgItemText(hwndDlg, IDS_BTNWARMBOOT, _stringTable.szReboot, MAX_PATH);
				GetDlgItemText(hwndDlg, ID_BTNCANCEL, _stringTable.szCancel, MAX_PATH);
				GetDlgItemText(hwndDlg, IDC_BTNSHUTDOWN, _stringTable.szShutdown, MAX_PATH);
				GetDlgItemText(hwndDlg, IDC_BTNSUSPEND, _stringTable.szSuspend, MAX_PATH);
				GetDlgItemText(hwndDlg, IDC_DIALOGTEXT, _stringTable.szQuestion, MAX_PATH);
				writeRegDlg();
			}
		  g_bRebootDialogOpen=true;
		  g_hWnd_RebootDialog=hwndDlg;

		  //show or hide suspend button?
		  hwndButton = GetDlgItem(hwndDlg, IDC_BTNSUSPEND);
		  if(bShowSuspendButton){
			  ShowWindow(hwndButton, SW_SHOW);
		  }
		  else
			  ShowWindow(hwndButton, SW_HIDE);

		  //show or hide suspend button?
		  hwndButton = GetDlgItem(hwndDlg, IDC_BTNSHUTDOWN);
		  if(bShowShutdownButton){
			  ShowWindow(hwndButton, SW_SHOW);
		  }
		  else
			  ShowWindow(hwndButton, SW_HIDE);


		  return TRUE;  

//		case WM_DESTROY:
//			  DEBUGMSG(1, (L"Got WM_DESTROY in dialog...\n"));
////			  g_bRebootDialogOpen=false;
////			  g_hWnd_RebootDialog=NULL;
//			  return FALSE;
		case WM_COMMAND:
		  switch (LOWORD(wParam))
		  {
			case IDC_BTNWARMBOOT:
			  DEBUGMSG(1, (L"Warmboot to execute...\n"));
			  ResetPocketPC();
			  //EndDialog(hwndDlg, 0);
			  g_bRebootDialogOpen=false;
			  g_hWnd_RebootDialog=NULL;
			  DestroyWindow(hwndDlg);
			  return TRUE;
			  break;

			case IDC_BTNSHUTDOWN:
			  DEBUGMSG(1, (L"Shutdown to execute...\n"));
			  CN50_Shutdown();
			  //EndDialog(hwndDlg, 0);
			  g_bRebootDialogOpen=false;
			  g_hWnd_RebootDialog=NULL;
			  DestroyWindow(hwndDlg);
			  return TRUE;
			  break;


			case IDC_BTNSUSPEND:
			  DEBUGMSG(1, (L"Suspend to execute...\n"));
			  //SuspendPPC();
				PulseEvent(handleEventCloseDialogAndSuspend);
			  //g_bRebootDialogOpen=false;
			  //g_hWnd_RebootDialog=NULL;

			  __try{
				  //DestroyWindow(hwndDlg);
			  }
			  __except(EXCEPTION_EXECUTE_HANDLER){
				  DEBUGMSG(1, (L"Exception in DestroyWindow in BTNSUSPEND\n"));
			  }
			  EndDialog(hwndDlg, 0);
			  return TRUE;
			  break;

			case ID_BTNCANCEL:
			  DEBUGMSG(1, (L"Cancel to execute...\n"));
			  //EndDialog(hwndDlg, 0);
			  g_bRebootDialogOpen=false;
			  g_hWnd_RebootDialog=NULL;
			  DestroyWindow(hwndDlg);
			  return TRUE;
			  break;

		  }//switch WM_COMMAND
		  return FALSE;
	  }//switch (msg)
	  return FALSE;
}

VOID CALLBACK MyTimerProc( 
    HWND hwnd,        // handle to window for timer messages 
    UINT message,     // WM_TIMER message 
    UINT idTimer,     // timer identifier 
	DWORD dwTime) {
		KillTimer(NULL, TimerID);
		Sleep(100);
		SuspendPPC();
}

void startTimer(){
	TimerID = SetTimer(/*g_hWnd_RebootDialog*/ NULL, TIMER1, 500, MyTimerProc);
}