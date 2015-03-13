#include <Windows.h>
#include <stdio.h>
#include <tchar.h>
#include <tchar.h>
#include <stdarg.h>
#include <Dbt.h>
#include <Wtsapi32.h>
#include <Shlwapi.h>
#include <stdlib.h>
#include <iostream>

#include "EdpNotify.h"

#define DEBUG
#ifdef  DEBUG
#define Log printf
#else
#define Log(fmt, ...) do {} while(0)
#endif

static const GUID GuidDevInterfaceUsb = {
	0xA5DCBF10L, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED }
};

const TCHAR *pLogFile = "SendMessage.log";
const char winClass[] = "MyNotifyWindow";
const char winTitle[] = "WindowTitle";
TCHAR usbVidPid[MAX_PATH] = "";
TCHAR devName[MAX_PATH] = "";
HINSTANCE  hInst = NULL;
EDP_CREATE_OBJECT  ECO;
IEdpNotify *pEdpObject;
UDISK_INFO uDiskInfo;

void FLog(TCHAR *pMsg,...)
{
	// write error or other information into log file
	FILE *pLog;
	va_list   arg;
	char pbString[256];

	//EnterCriticalSection(&myCS);
	do {
		SYSTEMTIME oT;
		GetLocalTime(&oT);
		
		fopen_s(&pLog, pLogFile, _T("a+"));
		if (pLog == NULL)
			break;
		va_start(arg, pMsg);
		wvsprintf(pbString, pMsg,arg);
		fprintf(pLog, "[%02d/%02d/%04d, %02d:%02d:%02d]: %s",
				 oT.wMonth, oT.wDay, oT.wYear,
				 oT.wHour, oT.wMinute, oT.wSecond, pbString); 
		va_end(arg);
		fclose(pLog);
	} while(0);

	//LeaveCriticalSection(&myCS);
}


DWORD GetCurSessionID()
{
	DWORD pid;
	DWORD sid;
	pid = GetCurrentProcessId();
	ProcessIdToSessionId(pid, &sid);
	return sid;
}

BOOL GetSessionToken(HANDLE* userToken, HANDLE *uacToken)
{
    BOOL result;
    TOKEN_STATISTICS tokenStatistics = {0};
    DWORD len;
	DWORD sessionid;

    if (userToken == NULL) {
        Log(_T("GetSessionToken Param Error\n"));
        return FALSE;
    }
 
	sessionid = GetCurSessionID();
    result = WTSQueryUserToken(sessionid, userToken);
    
    if (!result) {
        Log(_T("WTSQueryUserToken1 Fail %d\n"), GetLastError());
        return FALSE;
    }

    Log(_T("GetTokenInformation return ret %d handle1 %x\n"), result, *userToken);
    if (uacToken != NULL) {
        result = GetTokenInformation(*userToken, TokenLinkedToken, uacToken, sizeof(HANDLE), &len);
        if (!result) {
            //某些用户没有uac账户
            Log(_T("GetTokenInformation2 Fail %d\n"), GetLastError());
            *uacToken = NULL;
            return TRUE;
        }
        Log(_T("GetTokenInformation return ret %d handle2 %x Len %d\n"), result, uacToken, len);
    }

    return TRUE;
}

BOOL ImpersonateByCurUser()
{
    //仿冒
	HANDLE token;

	GetSessionToken(&token, NULL);
    BOOL result = ImpersonateLoggedOnUser(token);
    if (!result) {
        Log(_T("GetLocalVolumes ImpersonateLoggedOnUser Fail %d"), GetLastError());
        return FALSE;
    }

    return result;
}

BOOL UnMount(UDISK_INFO *uDiskInfo)
{
	BOOL result;
	TCHAR DriveLetter[MAX_PATH] = "C:";

	DriveLetter[0] = uDiskInfo->cDrv;
	Log("unmount %s %s\n", DriveLetter, devName);

	ImpersonateByCurUser();
	result = DefineDosDevice(DDD_RAW_TARGET_PATH|DDD_REMOVE_DEFINITION|
						  DDD_EXACT_MATCH_ON_REMOVE, DriveLetter, devName);
	if (!result)
		Log(_T("DefineDosDevice FAIL %d\n"), GetLastError());
	RevertToSelf();
    return result;
}

BOOL SetReMountImpersonateByCurUser(UDISK_INFO *uDiskInfo, TCHAR *devName)
{
	BOOL result = 1;
    //仿冒
	HANDLE token;
	TCHAR DriveLetter[MAX_PATH] = "C:";
	TCHAR DeviceName[MAX_PATH] = "";

	GetSessionToken(&token, NULL);
    result = ImpersonateLoggedOnUser(token);
    if (!result) {
        Log(_T("GetLocalVolumes ImpersonateLoggedOnUser Fail %d"), GetLastError());
        return FALSE;
    }

	while (DriveLetter[0] < 'Z') {
		if (QueryDosDevice(DriveLetter, DeviceName, MAX_PATH)) {
			DriveLetter[0]++;
		} else {
			Log("Drive Letter %s is not mounted\n", DriveLetter);
			break;
		}
	}
	if (DriveLetter[0] > 'Z') {
		Log("driver Letter full, cannot mount device any more\n");
			return FALSE;
	} else {
		//调用DefineDosDevice
		result = DefineDosDevice(DDD_RAW_TARGET_PATH, DriveLetter, devName);
		if (!result)
			 Log(_T("DefineDosDevice FAIL %d\n"), GetLastError());
    }

    //返回SYSTEM权限
	uDiskInfo->cDrv = DriveLetter[0];
	uDiskInfo->ulAction = 1;
	RevertToSelf();

    return result;
}
void SafeUdiskInitial()
{       
	memset(&uDiskInfo, 0,  sizeof(UDISK_INFO));
	uDiskInfo.lpszPhysicsVolume = (char *)malloc(MAX_PATH);
	uDiskInfo.lpszLogicVolume = (char *)malloc(MAX_PATH);
	uDiskInfo.lpszSymbolicLink = (char *) malloc(MAX_PATH);

	hInst = LoadLibrary("EdpRpcCfg.dll");  
	ECO = (EDP_CREATE_OBJECT)GetProcAddress(hInst, "EdpCreateObject");
	pEdpObject = ECO(EDP_NOTIFY_ID);
	if (pEdpObject)
		Log("load dll succeed\n");
}

void SafeUdiskFinitial()
{           
	// 当进程退出时调用
	if(pEdpObject) 	{
		uDiskInfo.ulAction = 2;
		//ImpersonateByCurUser();
		ULONG ulRet = pEdpObject->Notify(&uDiskInfo);
		//RevertToSelf();
		if (0 == ulRet) {
			Log("通知成功\n");
		} else { // 失败
			Log("通知 失败\n");
		}
		free(uDiskInfo.lpszLogicVolume);
		free(uDiskInfo.lpszPhysicsVolume);
		free(uDiskInfo.lpszSymbolicLink);
		pEdpObject->Terminate();
		pEdpObject->Release();
	}

	FreeLibrary(hInst); 
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	LRESULT lRet = 1;
    static HDEVNOTIFY hDeviceNotify;
    static HWND hEditWnd;
    static ULONGLONG msgCount = 0;
	PDEV_BROADCAST_HDR lpdb;
	PDEV_BROADCAST_DEVICEINTERFACE lpdbv;
	CHAR DriveLetter[MAX_PATH] = "A:";
	int i = 0;

    switch (message)
    {
    case WM_CREATE:
        Log("create window !\n");
        break;

    case WM_SETFOCUS: 
        SetFocus(hEditWnd);
		Log("setFocus !\n");
		break;

    case WM_SIZE: 
        // Make the edit control the size of the window's client area. 
        MoveWindow(hEditWnd, 
                   0, 0,                  // starting x- and y-coordinates 
                   LOWORD(lParam),        // width of client area 
                   HIWORD(lParam),        // height of client area 
                   TRUE);                 // repaint window 
		Log("movewindow !\n");
        break;

    case WM_DEVICECHANGE:
    {
        //
        // This is the actual message from the interface via Windows messaging.
        // This code includes some additional decoding for this particular device type
        // and some common validation checks.
        //
        // Note that not all devices utilize these optional parameters in the same
        // way. Refer to the extended information for your particular device type 
        // specified by your GUID.
        //
        PDEV_BROADCAST_DEVICEINTERFACE b = (PDEV_BROADCAST_DEVICEINTERFACE) lParam;

        // Output some messages to the window.
        switch (wParam)
        {
        case DBT_DEVICEARRIVAL:
            msgCount++;
            Log("arrive count is %d\n", msgCount);
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            msgCount++;
            Log("remove complete count is %d\n", msgCount);
			lpdb = (PDEV_BROADCAST_HDR)lParam;
			Log("type is %x\n", lpdb->dbch_devicetype);
            if (lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)  {
                lpdbv = (PDEV_BROADCAST_DEVICEINTERFACE)lpdb;
				Log("deviceName is %s\n", lpdbv->dbcc_name);
				for (i = 0; i < (int)(strlen(usbVidPid)); i++) {
					if (usbVidPid[i] == '\\')
						usbVidPid[i] = '#';
				}
				
				Log("usbvid is %s", usbVidPid);
				if (StrStrI(lpdbv->dbcc_name, usbVidPid) != NULL) {
					
					UnMount(&uDiskInfo);
					Log("DriveMapping:%d exit\n", GetCurrentProcessId());
					// for safe udisk;
					//RevertToSelf();
					SafeUdiskFinitial();				
					system("pause");
					exit(0);
				}
            }
            break;
        case DBT_DEVNODES_CHANGED:
            msgCount++;
            Log("devnode changed count is %d\n", msgCount);
            break;
		case DBT_DEVICEQUERYREMOVEFAILED :
			msgCount++;
            Log("query remove count is %d\n", msgCount);
            break;
        default:
            msgCount++;
			Log("Message %d: WM_DEVICECHANGE message received, value %d unhandled.\n", 
                msgCount, wParam);
            break;
        }
        
    }
    break;
   
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        // Send all other messages on to the default windows handler.
        lRet = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return lRet;
}

void MySendMessage(TCHAR DriveLetter)
{
	DWORD recipients = BSM_ALLDESKTOPS | BSM_APPLICATIONS;

	DEV_BROADCAST_VOLUME msg;
	ZeroMemory(&msg, sizeof(msg));
	msg.dbcv_size = sizeof(msg);
	msg.dbcv_devicetype = DBT_DEVTYP_VOLUME;
	msg.dbcv_unitmask = 1 << (DriveLetter - 'A');

	long success = BroadcastSystemMessage(0, &recipients, WM_DEVICECHANGE, DBT_DEVICEARRIVAL, (LPARAM)&msg);
	Log("sendmessge succeed!\n");

}

void NotifyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

    ZeroMemory(&wcex, sizeof(wcex));

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= hInstance;
    wcex.hIcon			= NULL;
    wcex.hCursor		= NULL;
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= winClass;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void RegisterNotification()
{
	DEV_BROADCAST_DEVICEINTERFACE notificationFilter;
	HDEVNOTIFY hDevNotify;
	HWND hWnd;
	HINSTANCE hInstance;

	hInstance= GetModuleHandle(NULL);
	if (hInstance == NULL) {
		Log("GetModule failed!\n");
	}
	NotifyRegisterClass(hInstance);
	hWnd = CreateWindow(winClass, winTitle, WS_MAXIMIZE, 0, 0,
        100, 80, NULL, NULL, hInstance, NULL);
	if (hWnd == NULL) {
		Log("CreateWindow failed\n");
	}
	ShowWindow(hWnd, SW_HIDE);
	UpdateWindow(hWnd);

    ZeroMemory(&notificationFilter, sizeof(notificationFilter) );
    notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    notificationFilter.dbcc_classguid = GuidDevInterfaceUsb;

	hDevNotify = RegisterDeviceNotification(hWnd, &notificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
	if (!hDevNotify) {
		Log("Register failed!\n");
	}
}
/*
"%s\\SendMessage.exe %d %d %s %d %s %s"), curPath, head->diskID, 
							head->partitonID, head->volName, 
							drvType, 
							userName,
							devName);
*/
void main(int argc, char *argv[])
{
	DWORD diskID, partitionID, drvType,	retVal;
	MSG msg;
	if (argc != 8)
		return ;
	
	RegisterNotification();
	// for safe udsik
	//SafeUdiskInitial();
	memset(&uDiskInfo, 0,  sizeof(UDISK_INFO));
	uDiskInfo.lpszPhysicsVolume = (char *)malloc(MAX_PATH);
	uDiskInfo.lpszLogicVolume = (char *)malloc(MAX_PATH);
	uDiskInfo.lpszSymbolicLink = (char *) malloc(MAX_PATH);

	hInst = LoadLibrary("EdpRpcCfg.dll");  
	ECO = (EDP_CREATE_OBJECT)GetProcAddress(hInst, "EdpCreateObject");
	pEdpObject = ECO(EDP_NOTIFY_ID);
	if(pEdpObject) {
		if(!pEdpObject->Initialize()) {
			pEdpObject->Release();
			pEdpObject = NULL;
		}
	}

	if (pEdpObject)
		Log("load dll succeed\n");

	

	strcpy_s(devName, MAX_PATH, argv[6]);
	strcpy_s(usbVidPid, MAX_PATH, argv[7]);

	diskID =  atoi(argv[1]);
	partitionID = atoi(argv[2]);
	drvType = atoi(argv[4]);
	
	uDiskInfo.lVer = 0;
	wsprintf(uDiskInfo.lpszPhysicsVolume, "\\Device\\Harddisk%d\\Partition%d", diskID, partitionID);
	strcpy_s(uDiskInfo.lpszLogicVolume, MAX_PATH, argv[3]);
	strcpy_s(uDiskInfo.lpszSymbolicLink, MAX_PATH, argv[3]);
	strcpy_s(uDiskInfo.ucName, MAX_PATH, argv[5]);
	uDiskInfo.lSId = GetCurSessionID();
	wsprintf(uDiskInfo.DrvPhysicsNum, "%d", diskID);
	uDiskInfo.nDrvType = drvType;
	

	SetReMountImpersonateByCurUser(&uDiskInfo, devName);

	Log("uDiskInfo: lVer %d \n\
		PhysicsVolumeusername %s\n \
		LogicVolume %s\n \
		SymbolicLink %s\n \
		Drv %c\n \
		Sid %d\n \
		Action %d\n \
		Name %s\n \
		DrvPhysicsNum %s\n \
		DrvType %d\n", uDiskInfo.lVer,
		uDiskInfo.lpszPhysicsVolume,
		uDiskInfo.lpszLogicVolume,
		uDiskInfo.lpszSymbolicLink,
		uDiskInfo.cDrv,
		uDiskInfo.lSId,
		uDiskInfo.ulAction,
		uDiskInfo.ucName,
		uDiskInfo.DrvPhysicsNum,
		uDiskInfo.nDrvType);
	
	if(pEdpObject) 	{
		//ImpersonateByCurUser();
		ULONG ulRet = pEdpObject->Notify(&uDiskInfo);
		//RevertToSelf();
		if (0 == ulRet) {
			Log("通知成功\n");
			MySendMessage(uDiskInfo.cDrv);	
			//SendMessage(HWND_BROADCAST, WM_DEVICECHANGE, DBT_DEVICEARRIVAL, NULL);
		} else { // 失败
			Log("通知失败, error %d\n", GetLastError());
			//MySendMessage(uDiskInfo.cDrv);	
		}
	}

	//ImpersonateByCurUser();
	// Get all messages for any window that belongs to this thread,
	// without any filtering. Potential optimization could be
	// obtained via use of filter values if desired.
	while ((retVal = GetMessage(&msg, NULL, 0, 0)) != 0) { 
		//ImpersonateByCurUser();
		if (retVal == -1) {
			Log("Get Message failed!\n");
				break;
		} else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return;
}