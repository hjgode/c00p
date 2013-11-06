//ping.h
#include "Winsock2.h"
#pragma comment (lib, "ws2.lib")

#include "ICMPAPI.H"
#include "iphlpapi.h"
#pragma comment (lib, "iphlpapi.lib")

int PingBlocks=4;
TCHAR g_sRoundTrip[MAX_PATH];

int PingAddress(LPTSTR lpszPingAddr)
{
    TCHAR lpszOut[64];
	HANDLE hPing;
    BYTE bOut[32];
    BYTE bIn[1024];
    char cOptions[12];
    char szdbAddr[32];
    IP_OPTION_INFORMATION ipoi;
    PICMP_ECHO_REPLY pEr;
    struct in_addr Address;
    INT i, j, rc;
    DWORD adr;
	DWORD ping_good=0;
	DWORD ping_bad=0;
	static int bar_count=0;

    // Convert xx.xx.xx.xx string to a DWORD. First convert the string    
	// to ASCII.    
	wcstombs (szdbAddr, lpszPingAddr, 31);
    if ((adr = inet_addr(szdbAddr)) == -1L)        
		return -1;
    // Open ICMP handle.    
	hPing = IcmpCreateFile ();
    if (hPing == INVALID_HANDLE_VALUE)        
		return -2;
    wsprintf (lpszOut, TEXT ("Pinging: %s:\r\n"), lpszPingAddr);
	DEBUGMSG(true, (lpszOut));
//	AddLog(lpszOut);
	//set Target for LogFile
    // Ping loop    
	for (j = 0;  j < PingBlocks;  j++) 
	{
		//Get Radio Stats
		//GetRadioStats();
		// Initialize the send data buffer.        
		memset (&bOut, 0, sizeof (bOut));
		// Initialize the IP structure.        
		memset (&ipoi, 0, sizeof (ipoi));
        ipoi.Ttl = 32;
        ipoi.Tos = 0;
        ipoi.Flags = IP_FLAG_DF;
        memset (cOptions, 0, sizeof (cOptions));
        // Ping!        
		rc = IcmpSendEcho (hPing, adr, bOut, sizeof (bOut), &ipoi, bIn, sizeof (bIn), 1000);
        if (rc) 
		{            
			ping_good++;
			// Loop through replies.            
			pEr = (PICMP_ECHO_REPLY)bIn;
            for (i = 0; i < rc;  i++) 
			{                 
				Address.S_un.S_addr = (IPAddr)pEr->Address;
                // Format output string.                
				wsprintf (lpszOut, TEXT ("Reply from %hs: bytes:%d time:"), inet_ntoa (Address), pEr->DataSize);
                // Append round-trip time.
				if (pEr->RoundTripTime < 10)                    
					lstrcat (lpszOut, TEXT ("<10mS\r\n"));
                else
					wsprintf (&lpszOut[lstrlen(lpszOut)], TEXT ("%dmS\r\n"), pEr->RoundTripTime);
//				wsprintf(g_sRoundTrip, L"%d", pEr->RoundTripTime);
//				AddLog(lpszOut);
				DEBUGMSG(true, (lpszOut));
//				SetBarVal((long)pEr->RoundTripTime); //Sets value to the bar

                pEr++;
         	}        
		} 
		else 
		{            
			ping_bad++;
			lstrcpy (lpszOut, TEXT ("Request timed out."));
//			AddLog(lpszOut);

//			SetBarVal(0); //Sets value of a bar

			wsprintf(g_sRoundTrip, L"-1");
        }    
	}    IcmpCloseHandle (hPing);
	wsprintf(lpszOut, L"Ping Stats Good: %00i, Bad: %00i\r\n", ping_good, ping_bad); 
	DEBUGMSG(true, (lpszOut));
//	AddLog(lpszOut);
	return ping_good;
}
