#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <SetupAPI.h>

#define DEBUG

#ifdef  DEBUG
#define Log FLog
#else
#define Log(fmt, ...) do {} while(0)
#endif

#define BUFFERSIZE 500
extern TCHAR pLogFile[BUFFERSIZE + 1];
extern CRITICAL_SECTION		myCS;
void FLog(TCHAR* pMsg,...);
void Logw(WCHAR *pMsg,...);
void usbipGetCurDir(TCHAR *curPath);
wchar_t* CharToWchar(const char* c);
char* WcharToChar(const wchar_t* wp) ;


enum {
	ISStorage = 0,
	ISCamera,
	ISPRINTER,
	OTHER
};

typedef struct _DevAttr {
	HDEVINFO DevInfo;
	SP_DEVINFO_DATA DevInfoData;
	TCHAR devPath[MAX_PATH];
} DevAttr;

typedef BOOL (*ClsCmp)(DevAttr *devAttr, TCHAR *cmpStr);
BOOL FindDeviceObject(IN TCHAR *devCls, IN const GUID *clsGuid, IN TCHAR *cmpStr, IN ClsCmp cmp,
					  OUT DevAttr *devAttr);


/*
 * >>>> represent "task begining"
 * **** represent "some error"
 * ==== represent "success"
 */