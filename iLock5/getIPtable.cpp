// getIPtable.cpp

#include "getIPtable.h"

int getIpTable(TCHAR* sIPtable){
	int iCount=0;
	// IP address table list
	MIB_IPADDRTABLE *pIpAddressTable = NULL;
	DWORD dwIPTableSize = 0;

	// Find out the size of the IP table
	if(GetIpAddrTable(NULL, &dwIPTableSize, FALSE) !=
		ERROR_INSUFFICIENT_BUFFER)
		return FALSE;

	pIpAddressTable = (MIB_IPADDRTABLE *)LocalAlloc(LPTR,
		dwIPTableSize);
	if(!pIpAddressTable)
		return FALSE;

	// Get the IP table
	if(GetIpAddrTable(pIpAddressTable, &dwIPTableSize, TRUE) !=
		NO_ERROR) {
			LocalFree(pIpAddressTable);
			return FALSE;
	}

	// Enumerate the IP addresses. Convert the IP address
	// from a DWORD to a string using inet_ntoa
	TCHAR tchIPTableEntry[256] = TEXT("\0");
	for(DWORD dwIP = 0; dwIP < pIpAddressTable->dwNumEntries; dwIP++) {

		MIB_IPADDRROW *pIpAddrRow = NULL;
		struct in_addr sAddr;

		pIpAddrRow = (MIB_IPADDRROW *)&pIpAddressTable->table[dwIP];
		sAddr.S_un.S_addr = (IPAddr)pIpAddrRow->dwAddr;

		wsprintf(tchIPTableEntry, TEXT("IP Address: %hs"), inet_ntoa(sAddr));
		iCount++;
	}

	LocalFree(pIpAddressTable);
	wsprintf(sIPtable, L"%s", tchIPTableEntry);
	return iCount;
}