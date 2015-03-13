#include <WtsApi32.h>

typedef struct _ActivateSessionInfo
{
	DWORD sid;
	TCHAR userName[25];
	TCHAR sourceIP[25];
	int pid;
	struct _ActivateSessionInfo * next;
} ActSinfo;

DWORD GetSessionIDFromIP(TCHAR *destIP);
int GetUserSecureId(DWORD sessionID, TCHAR *szSid, unsigned long dwSize);
HANDLE _GetUserToken(DWORD sessionID);
HANDLE RunAsUser(DWORD sessionID, TCHAR *processPath);
HANDLE RunAsSystem(DWORD sessionID, TCHAR *processPath);
BOOL GetUsernameFromSessionID(DWORD	sessionID, TCHAR *userName);
BOOL GetSessionToken(DWORD sessionid, HANDLE* userToken, HANDLE *uacToken);