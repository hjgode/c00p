//dlg_info.cpp

// based on http://www.winprog.org/tutorial/modeless_dialogs.html

#include "dlg_info.h"
#include "idleBeeper.h"
#include "resource.h"

HWND hDlgInfo;	//local window handle
HWND g_hDlgInfo;	//global window handle
BOOL bInfoDlgVisible=FALSE;	//show/hide DlgInfo


//forward declaration
BOOL CALLBACK InfoDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

HBRUSH hBackground;

HWND createDlgInfo(HWND hwnd, TCHAR* szMsg, TCHAR* szButton1, TCHAR* szButton2){
	hBackground=CreateSolidBrush(RGB(255,0,0));
	hDlgInfo = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DLGINFO), hwnd, InfoDlgProc);
    if(hDlgInfo != NULL)
    {
		SetWindowPos(hDlgInfo, HWND_TOPMOST, 0,0,0,0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
        ShowWindow(hDlgInfo, SW_HIDE);
		HWND hEdit = GetDlgItem(hDlgInfo, IDC_EDIT1);
		SetDlgItemText(hDlgInfo, IDC_EDIT1, szMsg);
		SetDlgItemText(hDlgInfo, ID_BUTTON1, szButton1);
		SetDlgItemText(hDlgInfo, ID_BUTTON2, szButton2);
    }
    else
    {
        DEBUGMSG(1, (L"CreateDialog returned NULL\nhwnd=0x%08x, error=%i\n", hwnd, GetLastError()));
    }
	
	return hDlgInfo;
}

BOOL CALLBACK InfoDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	HWND hDlg;
	HDC hdcDlg;
    switch(Message)
    {
		case WM_CTLCOLORDLG:
			hdcDlg=(HDC)wParam;
			hDlg=(HWND)lParam;
			return (LONG)hBackground;

			break;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case ID_BUTTON1:	//Snooze button
					stopBeeper();
					resetIdleThread();
                    //MessageBox(hwnd, L"Hi!", L"This is a message", MB_OK | MB_ICONEXCLAMATION);
                break;
                case ID_BUTTON2:
					ShowWindow(hDlgInfo, SW_HIDE);
					bInfoDlgVisible=FALSE;
                    //MessageBox(hwnd, L"Bye!", L"This is also a message", MB_OK | MB_ICONEXCLAMATION);
                break;
            }
			break;
        default:
            return FALSE;
    }
    return TRUE;
}