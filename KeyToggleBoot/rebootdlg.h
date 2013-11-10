//rebootdlg.h

#include "resource.h"

BOOL CALLBACK RebootDialogProc (
                    HWND ,   // Handle to the dialog box.
                    UINT ,      // Message.
                    WPARAM ,  // First message parameter.
                    LPARAM )  // Second message parameter.
;

BOOL writeRegDlg();
BOOL readRegDlg();
BOOL SuspendPPC();
