#include "Helper.h"
#include "UserAccount.h"
#include <Userenv.h>
#include <tchar.h>

TCHAR preActivate[128];
#define MAX_NAME 256

BOOL GetUsernameFromSessionID(DWORD	sessionID, TCHAR *userName)
{
	PWTS_SESSION_INFO pSessionInfo = 0;
	DWORD dwCount = 0;
	DWORD BytesReturned;
	LPTSTR  pUserName;
	DWORD i;

	if (userName == NULL)
		Log("userName has not space");
	// Get the list of all terminal sessions 
    WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &dwCount);
    for (i = 0; i < dwCount; ++i) {
		if (pSessionInfo[i].SessionId == sessionID) {
			WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, pSessionInfo[i].SessionId,
									WTSUserName,
									&pUserName,
									&BytesReturned);
			lstrcpy(userName, pUserName);
			Log("...find. %s %d", pUserName,  pSessionInfo[i].SessionId);
			WTSFreeMemory(pUserName);
			return TRUE;
		}		
    } 	
	
	WTSFreeMemory(pSessionInfo);
	return FALSE;
}

static int ConvertSecureId(PSID pSid, char *TextualSid, unsigned long *lpdwBufferLen)
{
	PSID_IDENTIFIER_AUTHORITY psia;
	DWORD dwSubAuthorities;
	DWORD dwSidRev = SID_REVISION;
	DWORD dwCounter;
	DWORD dwSidSize;

	if(!IsValidSid(pSid)) {
		return FALSE;
	}

	psia = GetSidIdentifierAuthority(pSid);
	dwSubAuthorities = *GetSidSubAuthorityCount(pSid);
	dwSidSize=(15 + 12 + (12 * dwSubAuthorities) + 1) * sizeof(TCHAR);

	if (*lpdwBufferLen < dwSidSize) {
		 *lpdwBufferLen = dwSidSize;
		  SetLastError(ERROR_INSUFFICIENT_BUFFER);
		  return FALSE;
	}

	dwSidSize = wsprintf(TextualSid, TEXT("S-%lu-"), dwSidRev);
	if ((psia->Value[0] != 0) || (psia->Value[1] != 0)) {
		dwSidSize += wsprintf(TextualSid + strlen(TextualSid),TEXT("0x%02hx%02hx%02hx%02hx%02hx%02hx"),
                   (USHORT)psia->Value[0],
                           (USHORT)psia->Value[1],
                           (USHORT)psia->Value[2],
                           (USHORT)psia->Value[3],
                           (USHORT)psia->Value[4],
                           (USHORT)psia->Value[5]);
    } else {
		 dwSidSize += wsprintf(TextualSid + strlen(TextualSid),TEXT("%lu"),
                           (ULONG)(psia->Value[5])   +
                           (ULONG)(psia->Value[4] <<  8)   +
                           (ULONG)(psia->Value[3] << 16)   +
                           (ULONG)(psia->Value[2] << 24));
	}

	for (dwCounter=0; dwCounter < dwSubAuthorities; dwCounter++) {
		dwSidSize += wsprintf(TextualSid + dwSidSize, TEXT("-%lu"),*GetSidSubAuthority(pSid, dwCounter));
	}

	return TRUE;
}

int GetUserSecureId(DWORD sessionID, TCHAR *szSid, unsigned long dwSize)
{
	//char szUserName[MAX_NAME] = {0};
	TCHAR szUserSid[MAX_NAME] = {0};
	TCHAR szUserDomain[MAX_NAME] = {0};
	TCHAR szUserName[MAX_NAME] = {0};
	SID_NAME_USE snu;
	unsigned long dwNameLen;
	unsigned long dwSidLen;
	unsigned long dwDomainLen;

	memset(szSid, 0, dwSize);
	dwNameLen = 256;
	dwSidLen = 256;
	dwDomainLen = 256;

	// must get szUserName here.
	GetUsernameFromSessionID(sessionID, szUserName);
	dwNameLen = strlen(szUserName) + 1;

	if (LookupAccountName(NULL,szUserName,(PSID)szUserSid,&dwSidLen,szUserDomain,&dwDomainLen,&snu)) {
		 if (IsValidSid((PSID)szUserSid)) {
			if (ConvertSecureId((PSID)szUserSid, szSid, &dwSize)) {
				Log("%s Sid: %s\n", szUserName, szSid);
				return 1;
			 }
		 }
	 }

	return 0;
}

static HANDLE _GetUserToken(DWORD sessionID)
{
	HANDLE currentToken  = 0;
	HANDLE primaryToken = 0;
    int dwSessionId = 0;
    PHANDLE hUserToken = 0;
    PHANDLE hTokenDup = 0;
	BOOL bRet;
	int dataSize = sizeof(WTS_SESSION_INFO);
    
    // Get token of the particular user by its sesssion ID
    bRet = WTSQueryUserToken(sessionID, &currentToken);
    if (bRet == FALSE) {
		Log("query user token failed!, error code is %d", GetLastError());
        return 0;
    }
    bRet = DuplicateTokenEx(currentToken, 
             TOKEN_ALL_ACCESS,
             0, SecurityImpersonation, TokenPrimary, &primaryToken);

    if (bRet == FALSE) {
		Log("duplicate user token failed!");
        return 0;
    }

    return primaryToken;
}

HANDLE RunAsUser(DWORD sessionID, TCHAR *processPath)
{
	STARTUPINFO StartupInfo;
    PROCESS_INFORMATION processInfo;
	void* lpEnvironment = NULL; 
	BOOL res;
    HANDLE primaryToken;
	TCHAR curPath[MAX_PATH];

	primaryToken = _GetUserToken(sessionID);
    if (primaryToken == 0) {
		Log("primarytoken is 0");
        return FALSE;
    }   

	memset(&StartupInfo, 0, sizeof(StartupInfo));
	memset(&processInfo, 0, sizeof(processInfo));
    StartupInfo.cb = sizeof(STARTUPINFO);
	StartupInfo.lpDesktop = "WinSta0\\Default";
	StartupInfo.wShowWindow = TRUE;
   
	Log("this is RunAsUser, processPath is %s", processPath);
    // Get all necessary environment variables of logged in user
    // to pass them to the process
    res = CreateEnvironmentBlock(&lpEnvironment, primaryToken, FALSE);
    if (res == 0)
		Log("createEnvironment failed , error code is %d", GetLastError());
	
    // Start the process on behalf of the current user 
	usbipGetCurDir(curPath);
    res = CreateProcessAsUser(primaryToken, //HANDLE hToken
				  NULL, //  LPCTSTR lpApplicationName
				  processPath, // LPTSTR lpCommandLine
                  NULL, // LPSECURITY_ATTRIBUTES lpProcessAttributes
				  NULL, // LPSECURITY_ATTRIBUTES lpThreadAttributes
				  FALSE, // BOOL bInheritHandles
				  CREATE_UNICODE_ENVIRONMENT|NORMAL_PRIORITY_CLASS|CREATE_NEW_CONSOLE,  // DWORD dwCreationFlags
				  lpEnvironment, // LPVOID lpEnvironment
				  curPath, // LPCTSTR lpCurrentDirectory
				  &StartupInfo, // LPSTARTUPINFO lpStartupInfo
				  &processInfo); // LPPROCESS_INFORMATION lpProcessInformation
	if (res == 0) {
		Log("createProcess failed , error code is %d", GetLastError());
	}
	Log("where!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	DestroyEnvironmentBlock(lpEnvironment);
    CloseHandle(primaryToken);
    return processInfo.hProcess;
}

static HANDLE _GetSystemToken()
{
	HANDLE currentToken  = 0;
	HANDLE primaryToken = 0;
	BOOL bRet;
	HANDLE hProcess;
	int dataSize = sizeof(WTS_SESSION_INFO);
    
    // Get token of the particular user by its sesssion ID
    hProcess = GetCurrentProcess();
	OpenProcessToken(hProcess, TOKEN_ALL_ACCESS, &currentToken);

    bRet = DuplicateTokenEx(currentToken, 
             TOKEN_ALL_ACCESS,
             0, SecurityImpersonation, TokenPrimary, &primaryToken);

    if (bRet == FALSE) {
		Log("duplicate user token failed!");
        return 0;
    }

    return primaryToken;
}

HANDLE RunAsSystem(DWORD sessionID, TCHAR *processPath)
{
	STARTUPINFO StartupInfo;
    PROCESS_INFORMATION processInfo;
	void* lpEnvironment = NULL; 
	BOOL res;
    HANDLE primaryToken;
	TCHAR curPath[MAX_PATH];

	primaryToken = _GetSystemToken();
    if (primaryToken == 0) {
		Log("primarytoken is 0");
        return FALSE;
    }   
	SetTokenInformation(primaryToken, TokenSessionId, &sessionID, sizeof(DWORD));
	memset(&StartupInfo, 0, sizeof(StartupInfo));
	memset(&processInfo, 0, sizeof(processInfo));
    StartupInfo.cb = sizeof(STARTUPINFO);
	StartupInfo.lpDesktop = "WinSta0\\Default";
	StartupInfo.wShowWindow = TRUE;
   
	Log("this is RunAsUser, processPath is %s", processPath);
    // Get all necessary environment variables of logged in user
    // to pass them to the process
    res = CreateEnvironmentBlock(&lpEnvironment, primaryToken, FALSE);
    if (res == 0)
		Log("createEnvironment failed , error code is %d", GetLastError());
	
    // Start the process on behalf of the current user 
	usbipGetCurDir(curPath);
    res = CreateProcessAsUser(primaryToken, //HANDLE hToken
				  NULL, //  LPCTSTR lpApplicationName
				  processPath, // LPTSTR lpCommandLine
                  NULL, // LPSECURITY_ATTRIBUTES lpProcessAttributes
				  NULL, // LPSECURITY_ATTRIBUTES lpThreadAttributes
				  FALSE, // BOOL bInheritHandles
				  CREATE_UNICODE_ENVIRONMENT|NORMAL_PRIORITY_CLASS|CREATE_NEW_CONSOLE,  // DWORD dwCreationFlags
				  lpEnvironment, // LPVOID lpEnvironment
				  curPath, // LPCTSTR lpCurrentDirectory
				  &StartupInfo, // LPSTARTUPINFO lpStartupInfo
				  &processInfo); // LPPROCESS_INFORMATION lpProcessInformation
	if (res == 0) {
		Log("createProcess failed , error code is %d", GetLastError());
	}
	Log("where!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	DestroyEnvironmentBlock(lpEnvironment);
    CloseHandle(primaryToken);
    return processInfo.hProcess;
}

static int EnumerateSession(PWTS_SESSION_INFO *psinfo, DWORD *pcount)
{
	
	DWORD bytesReturned = 0;
	LPTSTR pBuf = NULL;
	
	if (!WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, psinfo, pcount)) {
		Log("get session info failed!");
		return -1;
	} 

	return 0;
}

static void SetBit(char dest[128], int sid)
{
	int Pos, bitPos;

	Pos = sid >> 3;
	bitPos = sid % 8;
	dest[Pos] |= (1 << bitPos);
}

static void GetActivateSession(char dest[128])
{
	PWTS_SESSION_INFO psinfo = NULL;
	DWORD pcount;
	DWORD i;
	int state, sid;
	DWORD bytesReturned = 0;
	LPTSTR pBuf = NULL;
	int retcode;

	retcode = EnumerateSession(&psinfo, &pcount);
	if (retcode != 0) {
		return ;
	}
	memset(preActivate, 0, sizeof(preActivate));

	for (i = 0; i < pcount; i++) {
		sid = psinfo[i].SessionId;
		if(!WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, sid , WTSConnectState, &pBuf, &bytesReturned)) {
			Log("Get Session's state failed!");
			continue;
		}
		state = *(int *)pBuf;
		if (state == WTSActive) {
			Log("session %d is activate!\n", sid);
			SetBit(dest, sid);
		}
	}
	return;
}

static void _GetUserName(int sid, TCHAR * username, int len)
{
	DWORD bytesReturned = 0;
	LPTSTR pBuf = NULL;

	if(WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, sid , WTSUserName , 
									&pBuf, &bytesReturned)) {
		memcpy(username, pBuf, len);
	}
}

static void GetSourceIP(int sid, TCHAR *sourceip, int len)
{
	DWORD bytesReturned = 0;
	LPTSTR pBuf = NULL;
	
	if(WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, sid , WTSClientAddress , 
									&pBuf, &bytesReturned)) {
		WTS_CLIENT_ADDRESS clientaddress;
		
		memcpy(&clientaddress,pBuf,sizeof(WTS_CLIENT_ADDRESS));
		wsprintf(sourceip, "%d.%d.%d.%d", 
						clientaddress.Address[2],
						clientaddress.Address[3],
						clientaddress.Address[4],
						clientaddress.Address[5]);
	} 
}

static ActSinfo* getSession(int sid)
{
	ActSinfo *newSession;
	newSession = (ActSinfo *) malloc(sizeof(ActSinfo));

	memset(newSession->sourceIP, 0, sizeof(newSession->sourceIP));
	memset(newSession->userName, 0, sizeof(newSession->userName));
	
	// get source ip
	GetSourceIP(sid, newSession->sourceIP, 25);
	// get username
	_GetUserName(sid, newSession->userName, 25);

	return newSession;
}

static BOOL DoGetSessionIDFromIP(TCHAR *destIP, DWORD *ID)
{
	BOOL res = FALSE;
	int sid;
	int i;
	char temp;
	int bitpos;
	int times = 0;
	ActSinfo *newSession = NULL;

	// Must be fixed.
	GetActivateSession(preActivate);

	for (i = 0; i < 128; i++) {
		temp = preActivate[i];
		for (bitpos = 0; bitpos < 8; bitpos++) {
			if (temp & (1 << bitpos)) {
				sid = (i << 3) + bitpos;
				newSession = getSession(sid);
				Log("\tsession %d is activate!, ip is %s, len is %d", sid, newSession->sourceIP, strlen(newSession->sourceIP));
				Log("\tdest ip is %s, len is %d", destIP, strlen(destIP));
				if (strcmp(newSession->sourceIP, destIP) == 0) {
					Log("\tdest ip is %s", destIP);
					free(newSession);
					*ID = sid;
					return TRUE;
				}
				free(newSession);
			}
		}
	}

	return FALSE;
}

DWORD GetSessionIDFromIP(TCHAR *destIP)
{
	DWORD ID = -1;
	int times = 0;

	Log("\n");
	while (times < 40) {
		Log("....waiting for session be created %d times", times);
		if (DoGetSessionIDFromIP(destIP, &ID) == TRUE)
			break;
		times++;
		Sleep(500);
	}

	return ID;
}

BOOL GetSessionToken(DWORD sessionid, HANDLE* userToken, HANDLE *uacToken)
{
    BOOL result;
    TOKEN_STATISTICS tokenStatistics = {0};
    DWORD len;
	//DWORD sessionid;

    if (userToken == NULL) {
        Log(_T("GetSessionToken Param Error\n"));
        return FALSE;
    }
 
	//sessionid = GetCurSessionID();
    result = WTSQueryUserToken(sessionid, userToken);
    
    if (!result) {
        Log(_T("WTSQueryUserToken1 Fail %d\n"), GetLastError());
        return FALSE;
    }

    Log(_T("GetTokenInformation return ret %d handle1 %x\n"), result, *userToken);
    if (uacToken != NULL)
    {
        result = GetTokenInformation(*userToken, TokenLinkedToken, uacToken, sizeof(HANDLE), &len);
        if (!result)
        {
            //某些用户没有uac账户
            Log(_T("GetTokenInformation2 Fail %d\n"), GetLastError());
            *uacToken = NULL;
            return TRUE;
        }

        Log(_T("GetTokenInformation return ret %d handle2 %x Len %d\n"), result, uacToken, len);
    }

    return TRUE;
}