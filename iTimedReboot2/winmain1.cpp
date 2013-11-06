/*
int WINAPI WinMain1(	HINSTANCE hInstance,
					HINSTANCE hPrevInstance,
					LPTSTR    lpCmdLine,
					int       nCmdShow)
{
	MSG      msg      ;  
	HWND     hwnd     ;   
	WNDCLASS wndclass ; 

	if (IsIntermec() != 0)
	{
		MessageBox(NULL, L"This is not an Intermec! Program execution stopped!", L"Fatal Error", MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
		return -1;
	}

	//init array with registry keys and vals
	memset(&rkeys, 0, sizeof(rkeys));
	wsprintf(rkeys[0].kname, L"Interval");
	wsprintf(rkeys[0].ksval, L"0");
	wsprintf(rkeys[1].kname, L"RebootTime");
	wsprintf(rkeys[1].ksval, L"00:00");
	wsprintf(rkeys[2].kname, L"PingInterval");
	wsprintf(rkeys[2].ksval, L"0");
	wsprintf(rkeys[3].kname, L"PingTarget");
	wsprintf(rkeys[3].ksval, L"127.0.0.1");
	wsprintf(rkeys[4].kname, L"EnableLogging");
	wsprintf(rkeys[4].ksval, L"0");
	wsprintf(rkeys[5].kname, L"LastBootDate");
	wsprintf(rkeys[5].ksval, L"19800101");

	//should write a set of sample reg entries
	if (wcsstr(lpCmdLine, L"-writereg") != NULL)
		WriteReg();

	//allow only one instance!
	//obsolete, as hooking itself prevents multiple instances
	HWND hWnd = FindWindow (szAppName, NULL);    
	if (hWnd) 
	{        
		//SetForegroundWindow (hWnd);            
		return -1;
	}



	RegisterClass (&wndclass) ;    
											  
	hwnd = CreateWindow (szAppName , L"iTimedReboot2" ,   
			 WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_OVERLAPPED,          // Style flags                         
			 CW_USEDEFAULT,       // x position                         
			 CW_USEDEFAULT,       // y position                         
			 CW_USEDEFAULT,       // Initial width                         
			 CW_USEDEFAULT,       // Initial height                         
			 NULL,                // Parent                         
			 NULL,                // Menu, must be null                         
			 hInstance,           // Application instance                         
			 NULL);               // Pointer to create
						  // parameters
	if (!IsWindow (hwnd)) 
		return 0; // Fail if not created.

	g_hInstance=hInstance;
	g_hwnd=hwnd;
	//show a hidden window
	ShowWindow   (hwnd , SW_HIDE); // nCmdShow) ;  
	UpdateWindow (hwnd) ;

	//Notification icon
	HICON hIcon;
	hIcon=(HICON) LoadImage (g_hInstance, MAKEINTRESOURCE (IHOOK_STARTED), IMAGE_ICON, 16,16,0);
	nid.cbSize = sizeof (NOTIFYICONDATA);
	nid.hWnd = hwnd;
	nid.uID = g_nUID;
	nid.uFlags = NIF_ICON | NIF_MESSAGE;	// NIF_TIP not supported    
	nid.uCallbackMessage = MYMSG_TASKBARNOTIFY;
	nid.hIcon = hIcon;
	nid.szTip[0] = '\0';
	BOOL res = Shell_NotifyIcon (NIM_ADD, &nid);
	#ifdef DEBUG
	if (!res)
		ShowError(GetLastError());
	#endif

	// TODO: Place code here.


	while (GetMessage (&msg , NULL , 0 , 0))   
	{
		TranslateMessage (&msg) ;         
		DispatchMessage  (&msg) ;         
	} 

	return msg.wParam ;
}
*/
