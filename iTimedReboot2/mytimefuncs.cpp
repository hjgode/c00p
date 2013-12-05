// mytimefuncs.cpp

#include "mytimefuncs.h"
#include "nclog.h"

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
	DEBUGMSG(1, (L"#####################################################\n"));
	SYSTEMTIME stReturn;
	memcpy(&stReturn, &stRebootPlanned, sizeof(SYSTEMTIME));
	//day diff?
	SYSTEMTIME stPlannedReboot2=addDays(stRebootPlanned, daysInterval);
	DEBUGMSG(1, (L"getNextBootWithInterval():\n\treboot planned for %02i.%02i.%04i %02i:%02i!\n\tdays interval: %i\n\tstActual: %04i%02i%02i %02i:%02i\n",
		stRebootPlanned.wDay, stRebootPlanned.wMonth, stRebootPlanned.wYear,
		stRebootPlanned.wHour, stRebootPlanned.wMinute,
		daysInterval,
		stActual.wDay, stActual.wMonth, stActual.wDay,
		stActual.wHour, stActual.wMinute
		));

	int iDays,iHours,iMinutes;
	//result will be negative if stActual is past stRebootPlanned
	int minutesdiff=DiffInMinutes(stActual, stPlannedReboot2, &iDays, &iHours, &iMinutes);
	//repeat until minutesdiff>0
	SYSTEMTIME newRebootTime;
	do{
		newRebootTime=addDays(stPlannedReboot2, daysInterval);
		minutesdiff=DiffInMinutes(stActual, stPlannedReboot2, &iDays, &iHours, &iMinutes);
		stPlannedReboot2=newRebootTime;
	}while (minutesdiff<=3); //(daysInterval*24*60));

	//subtract one days interval
	stReturn=addDays(stPlannedReboot2, -daysInterval);

	DEBUGMSG(1, (L"getNextBootWithInterval():\n\tNext reboot calculated to %02i.%02i.%04i %02i:%02i!\n\tdays interval: %i\n\tstActual: %04i%02i%02i %02i:%02i\n",
		stReturn.wDay, stReturn.wMonth, stReturn.wYear,
		stReturn.wHour, stReturn.wMinute,
		daysInterval,
		stActual.wDay, stActual.wMonth, stActual.wDay,
		stActual.wHour, stActual.wMinute
		));
	DEBUGMSG(1, (L"#####################################################\n"));

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
// Function name  : DiffInMinutes
// Description    : return the diff between two datetimes 
// Argument       : SYSTEMTIME st1 and st2 and the vars to store diff days,hours,minutes
// Return type    : number of minutes from st1 before st2, 
//				  : will be negative if st1 is past st2
//--------------------------------------------------------------------
int DiffInMinutes(SYSTEMTIME st1, SYSTEMTIME st2, int *iDays, int *iHours, int *iMinutes)
{
	DEBUGMSG(1, (L"### DiffInMinutes start ###\n"));
	DEBUGMSG(1, (L"+++ st1 is: %04i%02i%02i %02i:%02i\n",
		st1.wYear, st1.wMonth, st1.wDay,
		st1.wHour, st1.wMinute));
	DEBUGMSG(1, (L"+++ st2 is: %04i%02i%02i %02i:%02i\n",
		st2.wYear, st2.wMonth, st2.wDay,
		st2.wHour, st2.wMinute));

	FILETIME ft1, ft2;
	SystemTimeToFileTime(&st1, &ft1);
	SystemTimeToFileTime(&st2, &ft2);

	LARGE_INTEGER li1;
	li1.LowPart  = ft1.dwLowDateTime;
	li1.HighPart = ft1.dwHighDateTime;
	LARGE_INTEGER li2;
	li2.LowPart  = ft2.dwLowDateTime;
	li2.HighPart = ft2.dwHighDateTime;

	LARGE_INTEGER li3;

	// time st2 should be after st1
	li3.QuadPart = li2.QuadPart - li1.QuadPart;

	//TODO: check differences with test date time
	DWORD dwDays=0, dwHours=0, dwMinutes=0;
	li3.QuadPart /= 10; // => now us
	li3.QuadPart /= 1000; // now ms
	li3.QuadPart /= 1000; // now sec
	li3.QuadPart /= 60; // now min
	DEBUGMSG(1,(L"minutes diff=%i\n",li3.QuadPart));
	dwMinutes=li3.LowPart; 

	li3.QuadPart /= 60; // now h
	dwHours=li3.LowPart;
	DEBUGMSG(1,(L"hours diff=%i\n",li3.QuadPart));

	li3.QuadPart /= 24; // now days
	dwDays=li3.LowPart; 
	DEBUGMSG(1,(L"days diff=%i\n",li3.QuadPart));

	*iDays=dwDays;
	*iHours=dwHours;
	*iMinutes=dwMinutes;

	DEBUGMSG(1, (L"--- DiffInMinutes end ---\n\tReturn = %i/%i/%i (days/hours/minutes) diff. Return=%i\n", *iDays, *iHours, *iMinutes, *iMinutes));

	return dwMinutes;
}

