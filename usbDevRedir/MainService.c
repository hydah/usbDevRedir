#include <process.h>
#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>

#include "Helper.h"
#include "NetConnection.h"
#include "UserAccount.h"
#include "UsbDeviceObject.h"

#define MAX_NUM_OF_PROCESS  20


TCHAR pLogFile[BUFFERSIZE + 1];
char preActivate[128];

void ServiceMainProc();
void TrueMain();

// service
VOID WINAPI ServiceHandler(DWORD fdwControl);
VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);

// control
VOID Install(TCHAR* pPath, TCHAR* pName);
VOID UnInstall(TCHAR* pName);
BOOL KillService(TCHAR* pName);
BOOL RunService(TCHAR* pName);




CHAR cmdOption[BUFFERSIZE + 1];
TCHAR serviceName[BUFFERSIZE + 1];
TCHAR pExeFile[BUFFERSIZE + 1];


CRITICAL_SECTION		myCS;
SERVICE_TABLE_ENTRY		lpServiceStartTable[] = 
{
	{serviceName, ServiceMain},
	{NULL, NULL}
};

SERVICE_STATUS_HANDLE   hServiceStatusHandle; 
SERVICE_STATUS          ServiceStatus; 
PROCESS_INFORMATION	pProcInfo[MAX_NUM_OF_PROCESS];


VOID Install(TCHAR *pPath, TCHAR *pName)
{
	SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE); 
	long nError;

	if (schSCManager==0) {
		nError = GetLastError();
		//wsprintf(pTemp, _T("OpenSCManager failed, error code = %d\n"), nError);
		Log(_T("OpenSCManager failed, error code = %d\n"), nError);
	} else {
		SC_HANDLE schService = CreateService( 
			schSCManager,	/* SCManager database      */ 
			pName,			/* name of service         */ 
			pName,			/* service name to display */ 
			SERVICE_ALL_ACCESS,        /* desired access          */ 
			SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS , /* service type            */ 
			SERVICE_AUTO_START,      /* start type              */ 
			SERVICE_ERROR_NORMAL,      /* error control type      */ 
			pPath,			/* service's binary        */ 
			NULL,                      /* no load ordering group  */ 
			NULL,                      /* no tag identifier       */ 
			NULL,                      /* no dependencies         */ 
			NULL,                      /* LocalSystem account     */ 
			NULL
		);                     /* no password             */ 
		if (schService==0) {
			nError =  GetLastError();
			
			//wsprintf(pTemp, _T("Failed to create service %s, error code = %d\n"), pName, nError);
			Log(_T("Failed to create service %s, error code = %d\n"), pName, nError);
		} else {
			//wsprintf(pTemp, _T("Service %s installed\n"), pName);
			Log(_T("Service %s installed\n"), pName);
			CloseServiceHandle(schService); 
		}
		CloseServiceHandle(schSCManager);
	}	
}

VOID UnInstall(TCHAR *pName)
{
	SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
	long nError = GetLastError();

	if (schSCManager == 0){
		nError = GetLastError();
		Log(_T("OpenSCManager failed, error code = %d\n"), nError);
	} else {
		SC_HANDLE schService = OpenService( schSCManager, pName, SERVICE_ALL_ACCESS);
		if (schService==0) {
			nError = GetLastError();
			Log(_T("OpenService failed, error code = %d\n"), nError);
		} else {
			if(!DeleteService(schService)) {
				Log(_T("Failed to delete service %s\n"), pName);
			} else {
				Log(_T("Service %s removed\n"),pName);
			}
			CloseServiceHandle(schService); 
		}
		CloseServiceHandle(schSCManager);	
	}

	//DeleteFile(pLogFile);
}

BOOL KillService(TCHAR *pName)
{
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS); 
	long nError;
	SERVICE_STATUS status;

	if (schSCManager==0)  {
		nError = GetLastError();
		Log(_T("OpenSCManager failed, error code = %d\n"), nError);
	} else {
		// open the service
		SC_HANDLE schService = OpenService(schSCManager, pName, SERVICE_ALL_ACCESS);
		if (schService==0) {
			nError = GetLastError();
			Log(_T("OpenService failed, error code = %d\n"), nError);
		} else {
			// call ControlService to kill the given service			
			if(ControlService(schService,SERVICE_CONTROL_STOP,&status)) {
				CloseServiceHandle(schService); 
				CloseServiceHandle(schSCManager); 
				return TRUE;
			} else {
				nError = GetLastError();
				Log(_T("ControlService failed, error code = %d\n"), nError);
			}
			CloseServiceHandle(schService); 
		}
		CloseServiceHandle(schSCManager); 
	}
	return FALSE;
}

BOOL RunService(TCHAR *pName)
{
	// run service with given name
	SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
	long nError;
	SC_HANDLE schService;

	if (schSCManager==0) {
		nError = GetLastError();
		Log(_T("OpenSCManager failed, error code = %d\n"), nError);
	} else {
		// open the service
		schService = OpenService( schSCManager, pName, SERVICE_ALL_ACCESS);
		if (schService==0) {
			nError = GetLastError();
			Log(_T("OpenService failed, error code = %d\n"), nError);
		} else {
			// call StartService to run the service
			if(StartService(schService, 0, (const TCHAR**)NULL)) {
				CloseServiceHandle(schService); 
				CloseServiceHandle(schSCManager); 
				return TRUE;
			} else {
				nError = GetLastError();
				Log(_T("StartService failed, error code = %d\n"), nError);
			}
			CloseServiceHandle(schService); 
		}
		CloseServiceHandle(schSCManager); 
	}
	return FALSE;
}

VOID WINAPI ServiceHandler(DWORD fdwControl)
{
	long nError;
	
	switch(fdwControl) 
	{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			ServiceStatus.dwWin32ExitCode = 0; 
			ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
			ServiceStatus.dwCheckPoint    = 0; 
			ServiceStatus.dwWaitHint      = 0;
			// terminate all processes started by this service before shutdown
			
			break; 
		case SERVICE_CONTROL_PAUSE:
			ServiceStatus.dwCurrentState = SERVICE_PAUSED; 
			break;
		case SERVICE_CONTROL_CONTINUE:
			ServiceStatus.dwCurrentState = SERVICE_RUNNING; 
			break;
		case SERVICE_CONTROL_INTERROGATE:
			break;
		default:
				Log(_T("Unrecognized opcode %d\n"), fdwControl);
	};
    if (!SetServiceStatus(hServiceStatusHandle,  &ServiceStatus)) { 
		nError = GetLastError();
		Log(_T("SetServiceStatus failed, error code = %d\n"), nError);
    } 
}

void WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	DWORD   status = 0; 
    DWORD   specificError = 0xfffffff; 
	long nError;

    ServiceStatus.dwServiceType        = SERVICE_WIN32; 
    ServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
    ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE; 
    ServiceStatus.dwWin32ExitCode      = 0; 
    ServiceStatus.dwServiceSpecificExitCode = 0; 
    ServiceStatus.dwCheckPoint         = 0; 
    ServiceStatus.dwWaitHint           = 0; 
 
    hServiceStatusHandle = RegisterServiceCtrlHandler(serviceName, ServiceHandler); 
	if (hServiceStatusHandle==0) {
		nError = GetLastError();
		Log(_T("RegisterServiceCtrlHandler failed, error code = %d\n"), nError);
        return; 
    } 
	 // Initialization complete - report running status 
    ServiceStatus.dwCurrentState       = SERVICE_RUNNING; 
    ServiceStatus.dwCheckPoint         = 0; 
    ServiceStatus.dwWaitHint           = 0;  
    if(!SetServiceStatus(hServiceStatusHandle, &ServiceStatus)) { 
		nError = GetLastError();
		Log(_T("SetServiceStatus failed, error code = %d\n"), nError);
    } 

	// do something here
	WaitForConnect();
}
void TrueMain()
{
	long nError = GetLastError();
	
	if(!StartServiceCtrlDispatcher(lpServiceStartTable)) {
		nError = GetLastError();
		Log(_T("StartServiceCtrlDispatcher failed, error code = %d\n"), nError);
	}
	DeleteCriticalSection(&myCS);
}

void ServiceMainProc()
{
	TCHAR pModuleFile[BUFFERSIZE + 1];
	int mSize;
	SYSTEMTIME oT;

	InitializeCriticalSection(&myCS);	
	mSize = GetModuleFileName(NULL, pModuleFile, BUFFERSIZE);
	pModuleFile[mSize] = '\0';
	GetLocalTime(&oT);

	if (mSize > 4 && pModuleFile[mSize - 4] == '.') {
		wsprintf(pExeFile, TEXT("%s"), pModuleFile);
		pModuleFile[mSize-4] = '\0';
		wsprintf(pLogFile, TEXT("%s-%04d%02d%02d.log"), pModuleFile, oT.wYear, oT.wMonth, oT.wDay);
	}
	lstrcpy(serviceName, _T("udrDevRedir"));
	if(_stricmp("-i", cmdOption) == 0 || _stricmp("-I", cmdOption) == 0)
		Install(pExeFile, serviceName);
	else if(_stricmp("-k", cmdOption) == 0 || _stricmp("-K",cmdOption) == 0)
		KillService(serviceName);
	else if(_stricmp("-u", cmdOption) == 0 || _stricmp("-U", cmdOption) == 0)
		UnInstall(serviceName);
	else if(_stricmp("-s", cmdOption) == 0 || _stricmp("-S", cmdOption) == 0)
		RunService(serviceName);
	else
		TrueMain();
}

int main(int argc, char *argv[])
{
	if (argc >= 2) {
		strcpy_s(cmdOption, _countof(cmdOption), argv[1]);
	}

	ServiceMainProc();
}