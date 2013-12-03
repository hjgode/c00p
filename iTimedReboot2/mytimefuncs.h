// mytimefuncs.h

#ifndef MYTIMEFUNCS
#define MYTIMEFUNCS

#include <windows.h>
#include <time.h>

void dumpFT(FILETIME ft);
void dumpST(SYSTEMTIME st);
void dumpST(TCHAR* szNote, SYSTEMTIME st);

BOOL getSystemtimeOfString(TCHAR* szDateTimeString, SYSTEMTIME *stDateTime);

DWORD DiffInDays(SYSTEMTIME st1, SYSTEMTIME st2);

int getDateTimeDiff(SYSTEMTIME stOld, SYSTEMTIME stNew, int *iDays, int *iHours, int *iMinutes);

SYSTEMTIME getNextBootWithInterval(SYSTEMTIME stActual, SYSTEMTIME stRebootPlanned, int daysInterval);

SYSTEMTIME DT_Add(const SYSTEMTIME& Date, short Years, short Months, short Days, short Hours, short Minutes, short Seconds, short Milliseconds);

SYSTEMTIME addDays(SYSTEMTIME stOld, int days);

int getMinuteDiff(SYSTEMTIME stOld, SYSTEMTIME stNew);

int getHourDiff(SYSTEMTIME stOld, SYSTEMTIME stNew);

int getDayDiff(SYSTEMTIME stOld, SYSTEMTIME stNew);




#endif