#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <tchar.h>
#include "UsbDeviceObject.h"
#include "UserAccount.h"
#include "Helper.h"
#include "UsbPrinterIsolation.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <setupapi.h>
#include <initguid.h>
#include <winioctl.h>
#include <sddl.h>
#include <tchar.h>
#include <cfgmgr32.h>
#include "SetACL.h"

//4d36e979-e325-11ce-bfc1-08002be10318
// {4d36e979-e325-11ce-bfc1-08002be10318}
DEFINE_GUID(GUID_DEVINTERFACE_PRINTER,
			0x4d36e979, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);


//4658ee7e-f050-11d1-b6bd-00c04fa372a7
//DEFINE_GUID(GUID_DEVINTERFACE_PRINTER,
//			0x4658ee7eL, 0xf050, 0x11d1, 0xb6, 0xbd, 0x00, 0xc0, 0x4f, 0xa3, 0x72, 0xa7);

DEFINE_GUID(GUID_DEVINTERFACE_USBPRINT,
			0x28d78fad, 0x5a12, 0x11D1, 0xae, 0x5b, 0x00, 0x00, 0xf8, 0x03, 0xa8, 0xc2);

DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE,
			0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

DEFINE_GUID(GUID_PRINTER_INSTALL_CLASS,
			0x4d36e979, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18);



static void EnumChildDevice(PSP_DEVINFO_DATA dev_inf, char *devID)
{
	 CONFIGRET cr;
	 ULONG ulStatus;
	 ULONG ulProblemNumber;
	 DEVINST dnDevInst;

	 cr = CM_Get_DevNode_Status(&ulStatus, &ulProblemNumber, dev_inf->DevInst, 0);
	 if (cr == CR_FAILURE) {
		 return;
	 }

	 cr = CM_Get_Child(&dnDevInst, dev_inf->DevInst, 0);
	 if (cr == CR_FAILURE) {
		 return;
	 }
	 cr = CM_Get_Device_ID(dnDevInst, devID, MAX_PATH, 0);
	 if (cr == CR_SUCCESS) {
		 Log("Ok!..%s\n", devID);
	 }
	 
}

void create_setacl(char *cmd)
{
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi;
	memset(&si, 0, sizeof(STARTUPINFO));
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = FALSE;
	
	if (CreateProcess(NULL,cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
		Log("Create setacl success!\n");
		return;
	}
	else {
		Log("Create setacl failed\n");
		return;	
	}
}

BOOL RemovePrinterACL(TCHAR *pName, TCHAR *uName)
{
	DWORD res = FALSE;

	Log("get here");


	res = RemoveACE(pName, SE_PRINTER, uName, 
										TRUSTEE_IS_NAME, 0xffffffff,
										SUB_OBJECTS_ONLY_INHERIT);
	if (res != ERROR_SUCCESS) {
		Log("remove %s's access failed", uName);
		return res;
	}

	res = TRUE;
	return res;
}

void DeleteSpecifiedPrinter(TCHAR *pName)
{
	DWORD res = FALSE;
	HANDLE hPrinter;

	Log("start to delete printer '%s'", pName);
	if (!OpenPrinter(pName, &hPrinter, NULL)) {
		Log("delete printer '%s' failed", pName);
		return;	
	}
	if (!DeletePrinter(hPrinter)) {
		Log("delete printer '%s' failed", pName);
		ClosePrinter(hPrinter);
		return;
	}

	ClosePrinter(hPrinter);
	Log("delete printer '%s' success", pName);
}

int SetPrinterACL(TCHAR *pName, TCHAR *uName)
{
	TCHAR curPath[MAX_PATH] = {0};
	DWORD res = FALSE;

	Log("get here");
	usbipGetCurDir(curPath);

	res = RemoveACE(pName, SE_PRINTER, "EVERYONE", 
										TRUSTEE_IS_NAME, PRINTER_ALL_ACCESS,
										SUB_OBJECTS_ONLY_INHERIT);

	if (res != ERROR_SUCCESS) {
		Log("remove everyrone's access failed");
		return res;
	}

#if 1
	res = RemoveACE(pName, SE_PRINTER, "ADMINISTRATORS", 
										TRUSTEE_IS_NAME, PRINTER_ALL_ACCESS,
										SUB_OBJECTS_ONLY_INHERIT);
	if (res != ERROR_SUCCESS) {
		Log("remove administrators' access failed");
		return res;
	}
#endif

	res = SetACE(pName, SE_PRINTER, uName, 
										TRUSTEE_IS_NAME, JOB_ALL_ACCESS,
										SUB_OBJECTS_ONLY_INHERIT|INHERIT_ONLY);
	if (res != ERROR_SUCCESS) {
		Log("set job_all_access failed");
		return res;
	}
		
	res = AddACE(pName, SE_PRINTER, uName, 
										TRUSTEE_IS_NAME, PRINTER_ALL_ACCESS,
										SUB_OBJECTS_ONLY_INHERIT);

	if (res != ERROR_SUCCESS) {
		Log("set printer_all_access failed");
		return res;
	}

	res = SetACE(pName, SE_PRINTER, "SYSTEM", 
										TRUSTEE_IS_NAME, JOB_ALL_ACCESS,
										SUB_OBJECTS_ONLY_INHERIT|INHERIT_ONLY);
	if (res != ERROR_SUCCESS) {
		Log("set job_all_access failed");
		return res;
	}
		
	res = AddACE(pName, SE_PRINTER, "SYSTEM", 
										TRUSTEE_IS_NAME, PRINTER_ALL_ACCESS,
										SUB_OBJECTS_ONLY_INHERIT);

	if (res != ERROR_SUCCESS) {
		Log("set printer_all_access failed");
		return res;
	}

	res = TRUE;
	return res;
}

static BOOL GetPrinteNameFromPort(TCHAR *port, TCHAR *pName)
{
	BOOL ret;
	DWORD dwNeeded = 0;
	DWORD dwReturned = 0;
	PRINTER_INFO_2 *pInfo = NULL;
	DWORD i = 0;
	BOOL res = FALSE;

	Log("枚举系统中的打印机设备\n");
	ret = EnumPrinters(
                PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
                NULL,
                2L,
                (LPBYTE)NULL,
                0L,
                &dwNeeded,
                &dwReturned);
    
    if (dwNeeded > 0) {
        pInfo = (PRINTER_INFO_2 *)HeapAlloc(
                    GetProcessHeap(), 0L, dwNeeded);
    }

    if (NULL != pInfo) {
        dwReturned = 0;
        ret = EnumPrinters(
                PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
                NULL,
                2L,                // printer info level
                (LPBYTE)pInfo,
                dwNeeded,
                &dwNeeded,
                &dwReturned);
    }

    if (ret) {
        for (i=0; i < dwReturned; i++) {
			if (strcmp(pInfo[i].pPortName, port) == 0) {
				lstrcpy(pName, pInfo[i].pPrinterName);
				Log("找到打印机, 打印机名称是%s", pName);
				Log("test!");
				res = TRUE;
				break;
			}
        }
    }

	return res;
}

static BOOL GetPportFromUsb(TCHAR *devInsID, TCHAR *pName, TCHAR *uName,
							DevAttr *pDevAttr)
{
	int memberIndex = 0, rc = 1;
	char hardwareID[1000] = {0};
	int devType = -1;
	TCHAR vid_pid[100] = {0};
	BOOL res = FALSE;
	TCHAR devID[MAX_PATH];
	char *pos = NULL;

	EnumChildDevice(&(pDevAttr->DevInfoData), devID);
	pos = strrchr(devID, '&');
	Log("%s 对应打印机端口是 [ %s ]\n", pDevAttr->devPath, pos+1);

	if (GetPrinteNameFromPort(pos+1, pName) == TRUE) {
		SetPrinterACL(pName, uName);
		res = TRUE;
	} else {
		Log("未找到打印机");
	}

	return res;
}

BOOL UsbPrinterIsolation(HANDLE chdPro, DWORD sessionID,
						 TCHAR *devInsID,
						 DevAttr *pDevAttr)
{
	TCHAR pName[MAX_PATH] = {0};
	TCHAR uName[MAX_PATH] = {0};
	DWORD times = 0;

	GetUsernameFromSessionID(sessionID, uName);

	while (WaitForSingleObject(chdPro, 0) == WAIT_TIMEOUT) {
		Log("....Find printer %d times", times+1);
		if (GetPportFromUsb(devInsID, pName, uName, pDevAttr) == TRUE) {
			Log("isolation succeed");
			break;
		}
		times++;
		Sleep(500);
	}
	
	// detect the remove of the printer device.
	Log("wait for remove the printer");
	WaitForSingleObject(chdPro, INFINITE);
	RemovePrinterACL(pName, uName);
	//FIXME sth wrong
	//DeleteSpecifiedPrinter(pName);

	return TRUE;
}