//dlg_info.h

#include "stdafx.h"

#ifndef _DLG_INFO_H_
#define _DLG_INFO_H_

/*
    while(GetMessage(&Msg, NULL, 0, 0))
    {
        if(!IsDialogMessage(g_hDlgInfo, &Msg))
        {
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
        }
    }
	...
	    case WM_COMMAND:
        switch(LOWORD(wParam))
        {   
            case ID_DIALOG_SHOW:
                ShowWindow(g_hDlgInfo, SW_SHOW);
            break;
            case ID_DIALOG_HIDE:
                ShowWindow(g_hDlgInfo, SW_HIDE);
            break;
	...
*/

HWND createDlgInfo(HWND hwnd, TCHAR* szMsg, TCHAR* szButton1, TCHAR* szButton2);
extern HWND g_hDlgInfo;
extern BOOL bInfoDlgVisible;	//show/hide DlgInfo

#endif