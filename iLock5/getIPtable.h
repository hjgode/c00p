// getIPtable.h
#include "stdafx.h"

#pragma once

#ifndef _GETIPTABLES_
#define _GETIPTABLES_

#include <iphlpapi.h>
#include <winsock2.h>
#pragma comment (lib, "ws2.lib")

//int getIpTable(TCHAR* sIPtable);
int getIpTable(TCHAR strIPlist[10][64]);

#endif