#include <tchar.h>
#include <stdarg.h>
#include "Helper.h"

void FLog(TCHAR *pMsg,...)
{
	// write error or other information into log file
	FILE *pLog;
	va_list   arg;
	char pbString[256];
	SYSTEMTIME oT;

	EnterCriticalSection(&myCS);
	do {
		
		GetLocalTime(&oT);
		
		fopen_s(&pLog, pLogFile, _T("a+"));
		if (pLog == NULL)
			break;
		va_start(arg, pMsg);
		wvsprintf(pbString, pMsg,arg);
		fprintf(pLog, "[%02d:%02d:%02d]: %s\n",
				oT.wHour, oT.wMinute, oT.wSecond, pbString); 
		va_end(arg);
		fclose(pLog);
	} while(0);

	LeaveCriticalSection(&myCS);
}

void Logw(WCHAR *pMsg,...)
{
	// write error or other information into log file
	FILE *pLog;
	va_list   arg;
	WCHAR pbString[256];

	EnterCriticalSection(&myCS);
	do {
		SYSTEMTIME oT;
		GetLocalTime(&oT);
		
		fopen_s(&pLog, pLogFile, _T("a+"));
		if (pLog == NULL)
			break;
		va_start(arg, pMsg);
		vswprintf(pbString, sizeof(pbString), pMsg,arg);
		fwprintf(pLog, L"%02d/%02d/%04d, %02d:%02d:%02d\n    %s",
				 oT.wMonth, oT.wDay, oT.wYear,
				 oT.wHour, oT.wMinute, oT.wSecond,
				 pbString); 
		va_end(arg);
		fclose(pLog);
	} while(0);

	LeaveCriticalSection(&myCS);
}

// User must free the space.
char* WcharToChar(const wchar_t* wp)  
{  
    char *m_char;
    int len= WideCharToMultiByte(CP_ACP,0,wp,wcslen(wp),NULL,0,NULL,NULL);  
    m_char= (char *) malloc(len+1);  
    WideCharToMultiByte(CP_ACP,0,wp,wcslen(wp),m_char,len,NULL,NULL);  
    m_char[len]='\0';  
    return m_char;  
}

wchar_t* CharToWchar(const char* c)  
{  
   wchar_t *m_wchar;
   int len = MultiByteToWideChar(CP_ACP,0,c,strlen(c),NULL,0);  
   m_wchar= (wchar_t *)malloc(len+1);  
   MultiByteToWideChar(CP_ACP,0,c,strlen(c),m_wchar,len);  
   m_wchar[len]='\0';  
   return m_wchar;  
} 

void usbipGetCurDir(TCHAR *curPath)
{
	TCHAR *pos;

	if (curPath == NULL) {
		Log("curPath is NULL");
		return;
	}

	GetModuleFileName(NULL, curPath, MAX_PATH);
	pos = _tcsrchr(curPath, '\\');
	*pos = '\0';
	Log("curpath is %s", curPath);
	return;
}