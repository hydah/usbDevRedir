#include <Windows.h>
#include <Dbt.h>
#include "EdpNotify.h"
#include <stdio.h>
#include <tchar.h>
#include <Wtsapi32.h>

#define DEBUG
#ifdef  DEBUG
#define Log printf
#else
#define Log(fmt, ...) do {} while(0)
#endif

const TCHAR *pLogFile = "SendMessage.log";


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

/*
BOOL CALLBACK EnumProcTxt(HWND hwnd, LPARAM lParam)
{
    if(hwnd == NULL) {
		printf("get window error\n");
        return FALSE;
	}
    else{
		SendMessage(HWND_BROADCAST, WM_SETTEXT, NULL, (LPARAM)lParam);
		return TRUE;
	}
}
*/

DWORD NotifyDeviceArrival(TCHAR DriveLetter, DWORD action)
{
	BOOL result;
	DWORD dwFlag = BSM_ALLCOMPONENTS;
    DWORD volumeMask;
    WPARAM wparam;
    DWORD ret;
	DEV_BROADCAST_VOLUME DevHdr;
	    
    volumeMask = 1 << (DriveLetter - 'A');
	DevHdr.dbcv_devicetype  = DBT_DEVTYP_VOLUME;
	DevHdr.dbcv_size        = sizeof(DEV_BROADCAST_VOLUME);
	DevHdr.dbcv_flags       = 0;
	DevHdr.dbcv_unitmask    = volumeMask;

	if (action == 1) {
		wparam = DBT_DEVICEARRIVAL;
		Log(("BroadcastSystemMessage WM_DEVICECHANGE DBT_DEVICEARRIVAL unitmask %x\n"), DevHdr.dbcv_unitmask);
		//广播设备插入
		ret = BroadcastSystemMessage(BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | BSF_POSTMESSAGE ,
                                    &dwFlag,
                                    WM_DEVICECHANGE,
                                    wparam,
                                    (LPARAM)&DevHdr);
		if (ret < 0) {
			Log(("BroadcastSystemMessage Fail %d ret %d\n"), GetLastError(), ret);
			return FALSE;
		} else {
			Log(("BroadcastSystemMessage ret %d error %d\n"), ret, GetLastError());
		}
	} else if (action == 2) {
		Log(("BroadcastSystemMessage WM_DEVICECHANGE DBT_DEVICEQUERYREMOVE unitmask %x\n"), DevHdr.dbcv_unitmask);
		wparam = DBT_DEVICEQUERYREMOVE;
		//广播设备删除
		if (BroadcastSystemMessage( BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | BSF_SENDNOTIFYMESSAGE ,
                                    &dwFlag,
                                    WM_DEVICECHANGE,
                                    wparam,
                                    (LPARAM)&DevHdr) < 0)
		{
			Log(("BroadcastSystemMessage Fail %d\n"), GetLastError());
			return FALSE;
		}
		Log(("BroadcastSystemMessage WM_DEVICECHANGE DBT_DEVICEREMOVECOMPLETE unitmask %x\n"), DevHdr.dbcv_unitmask);
		wparam = DBT_DEVICEREMOVECOMPLETE;
		//广播设备删除OK
		if (BroadcastSystemMessage( BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | BSF_SENDNOTIFYMESSAGE ,
                                    &dwFlag,
                                    WM_DEVICECHANGE,
                                    wparam,
                                    (LPARAM)&DevHdr) < 0)
		{
			Log(_T("BroadcastSystemMessage Fail %d\n"), GetLastError());
			return FALSE;
		}       
	}  

    return result;
}

BOOL SetNotifyImpersonateByCurUser(char drvLetter, DWORD action)
{
	BOOL result;
	HANDLE token;

	GetSessionToken(&token, NULL);
    result = ImpersonateLoggedOnUser(token);
    if (!result) {
        Log(_T("GetLocalVolumes ImpersonateLoggedOnUser Fail %d"), GetLastError());
        return FALSE;
    }
	    
    NotifyDeviceArrival(drvLetter, action);

    //返回SYSTEM权限
    RevertToSelf();
    
    return result;
}
/*
"%s\\SendMessage.exe %d %d %s %d %s %s %d"), curPath, head->diskID, 
							head->partitonID, head->volName, 
							drvType, head->driveLetter, 
							userName, action);
*/
int main(int argc, char *argv[])
{
	HINSTANCE  hInst = NULL;
	EDP_CREATE_OBJECT  ECO;
	IEdpNotify *pEdpObject;
	UDISK_INFO uDiskInfo;
	DWORD diskID, partitionID, drvType, action;
	if (argc != 8)
		return 0;

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
	else 
		return 0;

	memset(&uDiskInfo, 0,  sizeof(UDISK_INFO));
	uDiskInfo.lpszPhysicsVolume = (char *)malloc(MAX_PATH);
	uDiskInfo.lpszLogicVolume = (char *)malloc(MAX_PATH);
	uDiskInfo.lpszSymbolicLink = (char *) malloc(MAX_PATH);

	diskID =  atoi(argv[1]);
	partitionID = atoi(argv[2]);
	drvType = atoi(argv[4]);
	action = atoi(argv[7]);

	uDiskInfo.lVer = 0;
	wsprintf(uDiskInfo.lpszPhysicsVolume, "\\Device\\Harddisk%d\\Partition%d", diskID, partitionID);
	strcpy_s(uDiskInfo.lpszLogicVolume, MAX_PATH, argv[3]);
	strcpy_s(uDiskInfo.lpszSymbolicLink, MAX_PATH, argv[3]);
	uDiskInfo.cDrv = argv[5][0];
	strcpy_s(uDiskInfo.ucName, MAX_PATH, argv[6]);
	uDiskInfo.lSId = GetCurSessionID();
	uDiskInfo.ulAction = action;
	wsprintf(uDiskInfo.DrvPhysicsNum, "%d", diskID);
	uDiskInfo.nDrvType = drvType;
	

	SetNotifyImpersonateByCurUser(uDiskInfo.cDrv, action);

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
		ULONG ulRet = pEdpObject->Notify(&uDiskInfo);
		if (0 == ulRet) {
			Log("通知成功\n");
			NotifyDeviceArrival(uDiskInfo.cDrv, action);			
		} else { // 失败
			Log("通知失败, error %d\n", GetLastError());
		}
	}

	if (action == 1)
		SendMessage(HWND_BROADCAST, WM_DEVICECHANGE, DBT_DEVICEARRIVAL, NULL);

	free(uDiskInfo.lpszLogicVolume);
	free(uDiskInfo.lpszPhysicsVolume);
	free(uDiskInfo.lpszSymbolicLink);
	if(pEdpObject) 	{
		pEdpObject->Terminate();
		pEdpObject->Release();
	}
	FreeLibrary(hInst); 

	system("pause");
	return 0;
}