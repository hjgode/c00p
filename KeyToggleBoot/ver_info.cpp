//ver_info.cpp
/*	USAGE
	//read productversion from resources!
	TCHAR szVersion[MAX_PATH];
	TCHAR szProductName[MAX_PATH];
	myGetFileVersionInfo(NULL, szVersion);
	myGetFileProductNameInfo(NULL, szProductName);
	wsprintf(szAppName, L"%s v%s", szProductName, szVersion);
*/

#include "ver_info.h"

BOOL myGetFileVersionInfo(TCHAR* szVersion){
	return myGetFileVersionInfo(NULL, szVersion);
}

BOOL myGetFileVersionInfo(HMODULE hLib, TCHAR* szVersion)
{
	BOOL bRet=FALSE;
	DWORD  verHandle = NULL;
	UINT   size      = 0;
	LPBYTE lpBuffer  = NULL;
	//get the app name and path
	TCHAR strPath[MAX_PATH];
	GetModuleFileName (NULL, strPath, MAX_PATH);

	DWORD  verSize   = GetFileVersionInfoSize( strPath, &verHandle);

	if (verSize != NULL)
	{
		LPTSTR verData = new TCHAR[verSize];

		if (GetFileVersionInfo( strPath, verHandle, verSize, verData))
		{
			if (VerQueryValue(verData,L"\\",(VOID FAR* FAR*)&lpBuffer,&size))
			{
				if (size)
				{
					VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
					if (verInfo->dwSignature == 0xfeef04bd)
					{
						int major = (WORD)(((DWORD)(verInfo->dwFileVersionMS) >> 16) & 0xFFFF);
						int minor = (WORD)verInfo->dwFileVersionMS;
						int release = (WORD)(((DWORD)(verInfo->dwFileVersionLS) >> 16) & 0xFFFF);
						int build = (WORD)verInfo->dwFileVersionLS;
						wsprintf(szVersion, L"%i.%i.%i.%i", major, minor, release, build);
						bRet=TRUE;
					}
				}
			}
		}
		delete[] verData;
	}
	return bRet;
}

BOOL myGetFileProductNameInfo(TCHAR* szVersion){
	return myGetFileProductNameInfo(NULL, szVersion);
}

BOOL myGetFileProductNameInfo(HMODULE hLib, TCHAR* szVersion)
{
	BOOL bRet=FALSE;
	DWORD  verHandle = NULL;
	UINT   size      = 0;
	LPBYTE lpBuffer  = NULL;
	//get the app name and path
	TCHAR strPath[MAX_PATH];
	GetModuleFileName (NULL, strPath, MAX_PATH);

	DWORD  verSize   = GetFileVersionInfoSize( strPath, &verHandle);

	if (verSize != NULL)
	{
		BYTE* pBlock = new BYTE[verSize];

		if (GetFileVersionInfo( strPath, verHandle, verSize, (LPVOID)pBlock))
		{
			// Structure used to store enumerated languages and code pages.
			HRESULT hr;

			struct LANGANDCODEPAGE {
			  WORD wLanguage;
			  WORD wCodePage;
			} *lpTranslate;

			// Read the list of languages and code pages.
			UINT cbTranslate;
			VerQueryValue(pBlock, 
						  TEXT("\\VarFileInfo\\Translation"),
						  (LPVOID*)&lpTranslate,
						  &cbTranslate);

			// Read the file description for each language and code page.
			for(UINT i=0; i < (cbTranslate/sizeof(struct LANGANDCODEPAGE)); i++ )
			{
				TCHAR SubBlock[MAX_PATH];
				hr = StringCchPrintf(SubBlock, 50,
					TEXT("\\StringFileInfo\\%04x%04x\\ProductName"),
					lpTranslate[i].wLanguage,
					lpTranslate[i].wCodePage);
				if (FAILED(hr))
				{
					// TODO: write error handler.
					bRet=FALSE;
					break; //exit FOR
				}
				UINT uiBytes = 0;
				// Retrieve file description for language and code page "i". 
				LPVOID szBuffer=0;
				hr = VerQueryValue(pBlock, 
						SubBlock, 
						&szBuffer, 
						&uiBytes); 
				
				if(hr!=0){
					DEBUGMSG(1, (L"%s\n", szBuffer));
					if(uiBytes!=0){
						wsprintf(szVersion, L"%s", szBuffer);
						break; //exit for, we are only interested in first match
					}
					else{
						DEBUGMSG(1, (L"VerQueryValue OK but no data for '%s'\n", SubBlock));
					}
				}
				else{
					DEBUGMSG(1, (L"VerQueryValue failed with 0x%08x\n", GetLastError()));
				}
			}
		}
		delete[] pBlock;
	}
	return bRet;
}

BOOL myGetProductVersionInfo(TCHAR* szVersion){
	return myGetProductVersionInfo(NULL, szVersion);
}

BOOL myGetProductVersionInfo(HMODULE hLib, TCHAR* szVersion){
	BOOL bRet=FALSE;
	DWORD  verHandle = NULL;
	UINT   size      = 0;
	LPBYTE lpBuffer  = NULL;
	//get the app name and path
	TCHAR strPath[MAX_PATH];
	GetModuleFileName (NULL, strPath, MAX_PATH);

	DWORD  verSize   = GetFileVersionInfoSize( strPath, &verHandle);

	if (verSize != NULL)
	{
		LPTSTR verData = new TCHAR[verSize];

		if (GetFileVersionInfo( strPath, verHandle, verSize, verData))
		{
			if (VerQueryValue(verData,L"\\",(VOID FAR* FAR*)&lpBuffer,&size))
			{
				if (size)
				{
					VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
					if (verInfo->dwSignature == 0xfeef04bd)
					{
						int major = (WORD)(((DWORD)(verInfo->dwFileVersionMS) >> 16) & 0xFFFF);
						int minor = (WORD)verInfo->dwFileVersionMS;
						int release = (WORD)(((DWORD)(verInfo->dwFileVersionLS) >> 16) & 0xFFFF);
						int build = (WORD)verInfo->dwFileVersionLS;
						wsprintf(szVersion, L"%i.%i.%i.%i", major, minor, release, build);
						bRet=TRUE;
					}
				}
			}
		}
		delete[] verData;
	}
	return bRet;
}