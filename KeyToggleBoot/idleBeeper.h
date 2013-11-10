// idleBeeper.h
#include "stdafx.h"

#ifndef _IDLE_BEEPER_H_
#define _IDLE_BEEPER_H_

extern HWND		g_hWnd;

void startIdleThread(UINT iTimeOut);
void stopIdleThread();
void resetIdleThread();
void stopBeeper();

#endif