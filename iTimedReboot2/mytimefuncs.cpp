// mytimefuncs.cpp

#include "mytimefuncs.h"


//--------------------------------------------------------------------
// Function name  : getSystemtimeOfString
// Description    : return a SYSTEMTIME by parsing a string
// Argument       : string to parse and convert, ie '201311280923' -> 2013 nov 18 09:23
// Argument       : interval in days
// Return type    : systemtime with new date/time
//--------------------------------------------------------------------
BOOL getSystemtimeOfString(TCHAR* szDateTimeString, SYSTEMTIME *stDateTime){
	SYSTEMTIME st;
	memset(&st,0,sizeof(SYSTEMTIME));
	const int maxLen=14;
	BOOL bConvertOK=FALSE;

	TCHAR str[maxLen];
	memset(str,0,maxLen*sizeof(TCHAR));
	if(wcslen(szDateTimeString)>=8){
		wcsncpy(str, szDateTimeString, 4); 
		int iYear=_wtoi(str);
		memset(str,0,maxLen*sizeof(TCHAR));
		wcsncpy(str, &szDateTimeString[4], 2);
		int iMonth=_wtoi(str);
		memset(str,0,maxLen*sizeof(TCHAR));
		wcsncpy(str, &szDateTimeString[6], 2);
		int iDay=_wtoi(str);
		st.wYear=iYear;
		st.wMonth=iMonth;
		st.wDay=iDay;
		bConvertOK=TRUE;
	}
	if(wcslen(szDateTimeString)==12){
		memset(str,0,maxLen*sizeof(TCHAR));
		wcsncpy(str, &szDateTimeString[8], 2);
		int iHour=_wtoi(str);
		memset(str,0,maxLen*sizeof(TCHAR));
		wcsncpy(str, &szDateTimeString[10], 2);
		int iMinute=_wtoi(str);
		st.wHour=iHour;
		st.wMinute=iMinute;
		bConvertOK=TRUE;
	}

	memcpy(stDateTime, &st, sizeof(SYSTEMTIME));
	DEBUGMSG(1, (L"getSystemtimeOfString() returning with \t"));
	dumpST(st);
	return bConvertOK;
}

//--------------------------------------------------------------------
// Function name  : getNextBootWithInterval
// Description    : calc a new reboot date
// Argument       : current date/time
// Argument       : planned reboot date/time
// Argument       : interval in days
// Return type    : systemtime with new date/time
//--------------------------------------------------------------------
SYSTEMTIME getNextBootWithInterval(SYSTEMTIME stActual, SYSTEMTIME stRebootPlanned, int daysInterval){
	SYSTEMTIME stReturn;
	memcpy(&stReturn, &stRebootPlanned, sizeof(SYSTEMTIME));
	//day diff?
	SYSTEMTIME stPlannedReboot2=addDays(stRebootPlanned, daysInterval);
	int daydiff=getDayDiff(stActual, stPlannedReboot2);
	int minutesdiff=getMinuteDiff(stActual, stPlannedReboot2);
	//if positive, planned is past actual, which is OK
	if(daydiff>=0 && minutesdiff>3){
		DEBUGMSG(1, (L"we are past the planned reboot!\n"));
		//add x number of day intervals until we are before a planned reboot
		stPlannedReboot2=stRebootPlanned;
		do{
			stPlannedReboot2=addDays(stPlannedReboot2,1);
			daydiff=getDayDiff(stActual, stPlannedReboot2);
			minutesdiff=getMinuteDiff(stActual, stPlannedReboot2);
		}while (daydiff>0 && minutesdiff>0);
		stReturn=stPlannedReboot2;
	}
	
	DEBUGMSG(1, (L"Next reboot at %02i.%02i.%04i %02i:%02i!\n",
		stReturn.wDay, stReturn.wMonth, stReturn.wYear,
		stReturn.wHour, stReturn.wMinute
		));

	return stReturn;
}

//--------------------------------------------------------------------
// Function name  : dumpST
// Description    : dump a SYSTEMTIME to DEBUG out 
// Argument       : TCHAR *szNote, string to prepend to output
// Argument       : SYSTEMTIME st, the time to dump
// Return type    : VOID
//--------------------------------------------------------------------
void dumpST(TCHAR* szNote, SYSTEMTIME st){
#if !DEBUG
	return;
#endif
	TCHAR szStr[MAX_PATH];
	wsprintf(szStr, L"%04i%02i%02i %02i:%02i:%02i",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond);
	DEBUGMSG(1, (L"%s:\t%s\n", szNote, szStr));
}
//--------------------------------------------------------------------
// Function name  : dumpST
// Description    : dump a SYSTEMTIME to DEBUG out 
// Argument       : SYSTEMTIME st, the time to dump
// Return type    : VOID
//--------------------------------------------------------------------
void dumpST(SYSTEMTIME st){
#if !DEBUG
	return;
#endif
	TCHAR szStr[MAX_PATH];
	wsprintf(szStr, L"%04i%02i%02i %02i:%02i:%02i",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond);
	DEBUGMSG(1, (L"%s\n", szStr));
}

void dumpFT(FILETIME ft){
	SYSTEMTIME st;
	if(FileTimeToSystemTime(&ft, &st))
		dumpST(st);
}

SYSTEMTIME DT_Add(const SYSTEMTIME& Date, short Years, short Months, short Days, short Hours, short Minutes, short Seconds, short Milliseconds) 
{
	FILETIME ft; SYSTEMTIME st; ULARGE_INTEGER ul1;
	//convert INPUT to filetime
	SYSTEMTIME stStart;
	//memset(&stStart, 0, sizeof(SYSTEMTIME));
	memcpy(&stStart, &Date, sizeof(SYSTEMTIME));
	if (!SystemTimeToFileTime(&Date, &ft))
	{
		DEBUGMSG(1, (L"DT_Add: error in SystemTimeToFileTime: %i\n", GetLastError()));
		return Date;
	}
	ul1.HighPart = ft.dwHighDateTime;
	ul1.LowPart = ft.dwLowDateTime;
	 
	if (Milliseconds) 
		ul1.QuadPart += (Milliseconds * 10000); 

	if (Seconds)
		ul1.QuadPart += (Seconds * (__int64)10000000); 

	if (Minutes>0)
		ul1.QuadPart += (Minutes * (__int64)10000000 * 60); 
	else if (Minutes<0)
		ul1.QuadPart += (Minutes * (__int64)10000000 * 60); 

	if (Hours>0) 
		ul1.QuadPart += (Hours * (__int64)10000000 * 60 * 60);
	else if (Hours<0)
		ul1.QuadPart += (Hours * (__int64)10000000 * 60 * 60);

	if (Days>0)
		ul1.QuadPart += (Days * (__int64)10000000 * 60 * 60 * 24); 
	else if (Days<0)
		ul1.QuadPart += (Days * (__int64)10000000 * 60 * 60 * 24); 
	 
	ft.dwHighDateTime = ul1.HighPart;
	ft.dwLowDateTime = ul1.LowPart;
	
	//try to convert filetime back to a systemtime
	if (!FileTimeToSystemTime(&ft,&st)) {
		return Date;
	}
	 
	if (Months>0) {
		if ((Months += st.wMonth) <= 0) {
			Months *= (-1);
			st.wYear -= ((Months / 12) + 1);
			st.wMonth = 12 - (Months % 12);
		} else {
			st.wMonth = Months % 12;
			st.wYear += Months / 12;
		}
		while (!SystemTimeToFileTime(&st, &ft)) {
			st.wDay -= 1;
		}
	}
	return st;
}

SYSTEMTIME addDays(SYSTEMTIME stOld, int days){
	DEBUGMSG(1, (L"addDays() called with %i days...\n", days));
	TCHAR szPre[MAX_PATH];
	wsprintf(szPre, L"old time: ");
	dumpST(szPre, stOld);
	SYSTEMTIME stNew;

	stNew = DT_Add(stOld,0,0,days,0,0,0,0);

	wsprintf(szPre, L"new time: ");
	dumpST(szPre, stNew);
	DEBUGMSG(1, (L"addDays END\n"));
	return stNew;
}

//--------------------------------------------------------------------
// Function name  : getDateTimeDiff
// Description    : return the diff between two datetimes 
// Argument       : SYSTEMTIME stOld and stNew and the vars to store diff days,hours,minutes
// Return type    : -1 if old is before new datetime, 0 if old is equal new and +1 if old is past new
//--------------------------------------------------------------------
int getDateTimeDiff(SYSTEMTIME stOld, SYSTEMTIME stNew, int *iDays, int *iHours, int *iMinutes){
	int iReturn = 0;
	DEBUGMSG(1, (L"getDateTimeDiff()...\n"));
	DEBUGMSG(1,(L"old date:	")); dumpST(stOld);
	DEBUGMSG(1,(L"new date:	")); dumpST(stNew);

	FILETIME ftNew;
	FILETIME ftOld;
	//convert systemtimes to filetimes
	BOOL bRes = SystemTimeToFileTime(&stNew, &ftNew);
	bRes = SystemTimeToFileTime(&stOld, &ftOld);
	ULARGE_INTEGER tOld, tNew;
	memcpy(&tOld, &ftOld, sizeof(tOld));
	memcpy(&tNew, &ftNew, sizeof(tNew));
	ULONGLONG diff; BOOL bIsNegative=FALSE;
	if(tOld.QuadPart<tNew.QuadPart){// return always positive result
		diff=(tNew.QuadPart-tOld.QuadPart);
		bIsNegative=TRUE;
		iReturn=-1;		//old is before new date/time
		DEBUGMSG(1, (L"old is before new date\n"));
	}
	else if(tOld.QuadPart>tNew.QuadPart){
		diff=(tOld.QuadPart-tNew.QuadPart); 
		DEBUGMSG(1, (L"old is after new date\n"));
		iReturn=1;
	}
	else if(tOld.QuadPart==tNew.QuadPart){
		diff=0;
		iReturn=0;
		DEBUGMSG(1, (L"old is equal new date\n"));
	}
	ULONGLONG diffDays = diff / (24*60*60*(ULONGLONG)10000000);
	ULONGLONG diffHours = diff / (60*60*(ULONGLONG)10000000);
	ULONGLONG diffMinutes = diff / (60*(ULONGLONG)10000000);

	if(bIsNegative){
		diffDays	=(int)(0-diffDays);
		diffHours	=(int)(0-diffHours);
		diffMinutes	=(int)(0-diffMinutes);
	}
	DEBUGMSG(1, (L"### day diff =%i\n", diffDays));
	DEBUGMSG(1, (L"### hour diff=%i\n", diffHours));
	DEBUGMSG(1, (L"### min diff =%i\n", diffMinutes));
	*iDays		=(int)diffDays;
	*iHours		=(int)diffHours;
	*iMinutes	=(int)diffMinutes;
	DEBUGMSG(1, (L"getDateTimeDiff end. Return = %i/%i/%i (days/hours/minutes) diff. Return=%i\n", *iDays, *iHours, *iMinutes, iReturn));
	return iReturn;
}

//=================================================================================
/// get day diff of old to new date
int getDayDiff(SYSTEMTIME stOld, SYSTEMTIME stNew){
	DEBUGMSG(1, (L"getDayDiff()...\n"));
	//TCHAR szText[MAX_PATH];memset(szText,0,MAX_PATH*sizeof(TCHAR));
	//dumpST(szText, stOld);
	//DEBUGMSG(1, (L"old time: %s\n", szText));
	//dumpST(szText, stNew);
	//DEBUGMSG(1, (L"new time: %s\n", szText));

	DWORD dwReturn = 0;
	FILETIME ftNew;
	FILETIME ftOld;
	//convert systemtimes to filetimes
	BOOL bRes = SystemTimeToFileTime(&stNew, &ftNew);
	bRes = SystemTimeToFileTime(&stOld, &ftOld);

	//dumpST(stNew);
	//dumpST(stOld);

	//dumpFT(ftNew);
	//dumpFT(ftOld);

	//date diff
	ULARGE_INTEGER tOld, tNew;
	memcpy(&tOld, &ftOld, sizeof(tOld));
	memcpy(&tNew, &ftNew, sizeof(tNew));
	ULONGLONG diff; BOOL bIsNegative=FALSE;
	if(tOld.QuadPart<tNew.QuadPart){// return always positive result
		diff=(tNew.QuadPart-tOld.QuadPart);
		bIsNegative=TRUE;
	}
	else{
		diff=(tOld.QuadPart-tNew.QuadPart); 	
	}
	//ULONGLONG diff = (tNew.QuadPart-tOld.QuadPart);	// return positive or negative result
	ULONGLONG diffDays = diff / (24*60*60*(ULONGLONG)10000000);
	//ULONGLONG diffHours = diff / (60*60*(ULONGLONG)10000000);
	//ULONGLONG diffMinutes = diff / (60*(ULONGLONG)10000000);
	//DEBUGMSG(1, (L"### day diff=%i\n", bIsNegative? diffDays:0-diffDays));
	//DEBUGMSG(1, (L"### hour diff=%i\n", bIsNegative? diffHours:0-diffHours));
	//DEBUGMSG(1, (L"### min diff=%i\n", bIsNegative? diffMinutes:0-diffMinutes));
	if(bIsNegative)
		dwReturn=(int)(0-diffDays);
	else
		dwReturn=(int)diffDays;
	DEBUGMSG(1, (L"GetDayDiff end. Return = %i days diff\n", dwReturn));
	return dwReturn;
}

//=================================================================================
int getHourDiff(SYSTEMTIME stOld, SYSTEMTIME stNew){
	DEBUGMSG(1, (L"getHourDiff()...\n"));
	//TCHAR szText[MAX_PATH];memset(szText,0,MAX_PATH*sizeof(TCHAR));
	//dumpST(szText, stOld);
	//DEBUGMSG(1, (L"old time: %s\n", szText));
	//dumpST(szText, stNew);
	//DEBUGMSG(1, (L"new time: %s\n", szText));

	DWORD dwReturn = 0;
	FILETIME ftNew;
	FILETIME ftOld;
	//convert systemtimes to filetimes
	BOOL bRes = SystemTimeToFileTime(&stNew, &ftNew);
	bRes = SystemTimeToFileTime(&stOld, &ftOld);

	//dumpST(stNew);
	//dumpST(stOld);

	//dumpFT(ftNew);
	//dumpFT(ftOld);

	//date diff
	ULARGE_INTEGER t1, t2;
	memcpy(&t1, &ftOld, sizeof(t1));
	memcpy(&t2, &ftNew, sizeof(t2));
	ULONGLONG diff; BOOL bIsNegative=FALSE;
	if(t1.QuadPart<t2.QuadPart){// return always positive result
		diff=(t2.QuadPart-t1.QuadPart);
		bIsNegative=TRUE;
	}
	else{
		diff=(t1.QuadPart-t2.QuadPart); 	
	}
	//ULONGLONG diff = (t2.QuadPart-t1.QuadPart);	// return positive or negative result
	//ULONGLONG diffDays = diff / (24*60*60*(ULONGLONG)10000000);
	ULONGLONG diffHours = diff / (60*60*(ULONGLONG)10000000);
	//ULONGLONG diffMinutes = diff / (60*(ULONGLONG)10000000);
	//DEBUGMSG(1, (L"### day diff=%i\n", diffDays));
	//DEBUGMSG(1, (L"### hour diff=%i\n", diffHours));
	//DEBUGMSG(1, (L"### min diff=%i\n", diffMinutes));
	if(bIsNegative)
		dwReturn=(int)(0-diffHours);
	else
		dwReturn=(int)diffHours;
	DEBUGMSG(1, (L"getHourDiff end. Return = %i hours diff\n", dwReturn));
	return dwReturn;
}

//=================================================================================
int getMinuteDiff(SYSTEMTIME stOld, SYSTEMTIME stNew){
	DEBUGMSG(1, (L"getMinuteDiff()...\n"));
	//TCHAR szText[MAX_PATH];memset(szText,0,MAX_PATH*sizeof(TCHAR));
	//dumpST(szText, stOld);
	//DEBUGMSG(1, (L"old time: %s\n", szText));
	//dumpST(szText, stNew);
	//DEBUGMSG(1, (L"new time: %s\n", szText));

	DWORD dwReturn = 0;
	FILETIME ftNew;
	FILETIME ftOld;
	//convert systemtimes to filetimes
	BOOL bRes = SystemTimeToFileTime(&stNew, &ftNew);
	bRes = SystemTimeToFileTime(&stOld, &ftOld);

	//dumpST(stNew);
	//dumpST(stOld);

	//dumpFT(ftNew);
	//dumpFT(ftOld);

	//date diff
	ULARGE_INTEGER t1, t2;
	memcpy(&t1, &ftOld, sizeof(t1));
	memcpy(&t2, &ftNew, sizeof(t2));
	ULONGLONG diff; BOOL bIsNegative=FALSE;
	if(t1.QuadPart<t2.QuadPart){// return always positive result
		diff=(t2.QuadPart-t1.QuadPart);
		bIsNegative=TRUE;
	}
	else{
		diff=(t1.QuadPart-t2.QuadPart); 	
	}
	//ULONGLONG diff = (t2.QuadPart-t1.QuadPart);	// return positive or negative result
	//ULONGLONG diffDays = diff / (24*60*60*(ULONGLONG)10000000);
	//ULONGLONG diffHours = diff / (60*60*(ULONGLONG)10000000);
	ULONGLONG diffMinutes = diff / (60*(ULONGLONG)10000000);
	//DEBUGMSG(1, (L"### day diff=%i\n", diffDays));
	//DEBUGMSG(1, (L"### hour diff=%i\n", diffHours));
	//DEBUGMSG(1, (L"### min diff=%i\n", diffMinutes));
	if(bIsNegative)
		dwReturn=(int)(0-diffMinutes);
	else
		dwReturn=(int)diffMinutes;
	DEBUGMSG(1, (L"getMinuteDiff end. Return = %i minutes diff\n", dwReturn));
	return dwReturn;
}

