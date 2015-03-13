#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <winioctl.h>
#include <stdio.h>
#include <sddl.h>
#include <tchar.h>
#include <cfgmgr32.h>
//#include <usbioctl.h>
#include <Shlwapi.h>
#include "UsbDeviceObject.h"
#include "FileSysPartition.h"
#include "NetConnection.h"
#include "UserAccount.h"
#include "Helper.h"
#include "UsbPrinterIsolation.h"

DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE,
			0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);



static DWORD getDeviceNumber(HANDLE deviceHandle)
{
   STORAGE_DEVICE_NUMBER sdn;
   DWORD dwBytesReturned = 0;
   sdn.DeviceNumber = -1;
   
   if (!DeviceIoControl(deviceHandle, FSCTL_IS_VOLUME_MOUNTED,
                          NULL, 0, NULL, 0, &dwBytesReturned, 
						  NULL)) {
      // handle error - like a bad handle.
		printf("not !!\n");
      return 0xffffffff;
   }
   printf("is mounted\n");
   return sdn.DeviceNumber;
}

static void EnumChildDevice(PSP_DEVINFO_DATA dev_inf, char *devID)
{
	 CONFIGRET cr;
	 ULONG ulStatus;
	 ULONG ulProblemNumber;
	 DEVINST dnDevInst;

	 cr = CM_Get_DevNode_Status(&ulStatus, &ulProblemNumber, dev_inf->DevInst, 0);
	 if (cr == CR_SUCCESS) {
		 printf("Ok\n");
	 }

	 cr = CM_Get_Child(&dnDevInst, dev_inf->DevInst, 0);
	 if (cr == CR_SUCCESS) {
		 printf("Ok!\n");
	 }
	 cr = CM_Get_Device_ID(dnDevInst, devID, MAX_PATH, 0);
	 if (cr == CR_SUCCESS) {
		 printf("Ok!..%s\n", devID);
	 }
	 
}



static void DoIsolation(HANDLE chdPro, int devType, TCHAR *DeviceID,
						DWORD sessionID,
						DevAttr *usbDevAttr)
{
	int rc = 1;
	WCHAR *wssdl;
	char ssdl[1000] = {0};
	size_t converted;
	TCHAR secureID[MAX_PATH] = {0};
	char partitionName[1000] = {0};
	char driveLetter[100] = {0};
	TCHAR devInsID[MAX_DEVICE_ID_LEN];

	// Get the device instance id of the usb device.
	// Device instance id can locate the device in the system.
	CM_Get_Device_ID(usbDevAttr->DevInfoData.DevInst, devInsID, MAX_PATH, 0);

	if (devType == ISStorage) {
		Log(">>>>Begin usb disk Isolation...");
//zc FIXME detect the devivce is safe storage device or not.
		if (0)
			EnumSafeStorageAndRemap(chdPro, devInsID, sessionID);
		else
			EnumDiskAndRemap(chdPro, devInsID, sessionID);

	} else if (devType == ISPRINTER) {
		Log(">>>>Begin usb printer Isolation...");
		UsbPrinterIsolation(chdPro, sessionID, devInsID, usbDevAttr);

	} else if (devType == OTHER) {
		// Must get sid.
		GetUserSecureId(sessionID, secureID, 128);
		wsprintf(ssdl, _T("D:P(A;;GA;;;SY)(A;;GA;;;%s)"), secureID);

		wssdl = (WCHAR *)malloc((strlen(ssdl) + 1) * sizeof(WCHAR));
		mbstowcs_s(&converted, wssdl, strlen(ssdl) + 1, ssdl, _TRUNCATE);
		wprintf(L"the wssdl will be set is: %s\n", wssdl);
		rc = SetupDiSetDeviceRegistryPropertyW(
				usbDevAttr->DevInfo,			/* DeviceInfoSet */
				&(usbDevAttr->DevInfoData),		/* DeviceInfoData */
				SPDRP_SECURITY_SDS,	/* Property */
				(PBYTE)wssdl,	/* PropertyBuffer */
				sizeof(wssdl)	/* PropertyBufferSize */
			);
		
		if (rc == 0)
			Logw(L"%s: \n\t %s\n\tfailed. error code is %d\n", L"set property", wssdl, GetLastError());			
		else 
			Logw(L"%s: \n\t%s\n\tsucceed.\n", L"set property", wssdl);
		Log("wait for remove %s.", usbDevAttr->devPath);
		WaitForSingleObject(chdPro, INFINITE);
		
		wsprintf(ssdl, _T("D:P(A;;GA;;;SY)(A;;GA;;;%s)"), "WD");

		wssdl = (WCHAR *)malloc((strlen(ssdl) + 1) * sizeof(WCHAR));
		mbstowcs_s(&converted, wssdl, strlen(ssdl) + 1, ssdl, _TRUNCATE);
		wprintf(L"the wssdl will be set is: %s\n", wssdl);
		rc = SetupDiSetDeviceRegistryPropertyW(
				usbDevAttr->DevInfo,			/* DeviceInfoSet */
				&(usbDevAttr->DevInfoData),		/* DeviceInfoData */
				SPDRP_SECURITY_SDS,	/* Property */
				(PBYTE)wssdl,	/* PropertyBuffer */
				sizeof(wssdl)	/* PropertyBufferSize */
			);
		
		if (rc == 0)
			Logw(L"%s: \n\t %s\n\tfailed. error code is %d\n", L"set property", wssdl, GetLastError());			
		else 
			Logw(L"%s: \n\t%s\n\tsucceed.\n", L"set property", wssdl);
	}

}

static int FindDevAttr(DevAttr *pDevAttr, DWORD Property, TCHAR *buf)
{
	DWORD bufSize = -1;
	int rc = 1;
	rc = SetupDiGetDeviceRegistryProperty(
				pDevAttr->DevInfo,			/* DeviceInfoSet */
				&(pDevAttr->DevInfoData),		/* DeviceInfoData */
				Property,	/* Property */
				NULL,	/* PropertyBuffer */
				NULL,
				0,
				&bufSize	/* PropertyBufferSize */
			);

	Log("bufSize is %d, rc is %d", bufSize, rc);
	rc = SetupDiGetDeviceRegistryProperty(
				pDevAttr->DevInfo,			/* DeviceInfoSet */
				&(pDevAttr->DevInfoData),		/* DeviceInfoData */
				Property,	/* Property */
				NULL,	/* PropertyBuffer */
				(PBYTE)buf,
				bufSize,
				NULL	/* PropertyBufferSize */
			);
	if (!rc) {
		Log("get devAttr error, %d", GetLastError());
		return -1;
	}

	return 0;
}

/* This routine is a temporary method. It must be changed later. */
static int FindDeviceType(DevAttr *pDevAttr)
{
	TCHAR nbuf[MAX_PATH];
	int rc = 1;
	int baseCls, subCls, prot;
	TCHAR *buf, *pos1;

	rc = FindDevAttr(pDevAttr, SPDRP_COMPATIBLEIDS, nbuf);
	if (rc == -1) {
		Log("get devType error, %d", GetLastError());
		return -1;
	}

	buf = nbuf + strlen(nbuf) + 1;
	Log("device type is %s", buf);

	pos1 = strrchr(buf, '_');
	prot = atoi(pos1+1);	
	pos1 = strrchr(buf, '&');
	*pos1 = '\0';

	pos1 = strrchr(buf, '_');
	subCls = atoi(pos1+1);
	pos1 = strrchr(buf, '&');
	*pos1 = '\0';

	pos1 = strrchr(buf, '_');
	baseCls = atoi(pos1+1);
	
	Log("baseCls %d, subCls %d, prot %d", baseCls, subCls, prot);
	
	if (baseCls == 8) {
		Log("%s is storage.", pDevAttr->devPath);
		return ISStorage;
	} else if (baseCls == 7) {
		Log("%s is printer.", pDevAttr->devPath);
		return ISPRINTER;
	} else {
		return OTHER;
	}

	return -1;
}

static BOOL UsbDevCmp(DevAttr *usbDevAttr, TCHAR *cmpStr)
{
	TCHAR nbuf[MAX_PATH];
	int rc = 1;

	rc = FindDevAttr(usbDevAttr, SPDRP_COMPATIBLEIDS, nbuf);
	if (rc == -1) {
		Log("get compID error, %d", GetLastError());
		return FALSE;
	}

	Log("compID is %s", nbuf);
	Log("cmpStr is %s", cmpStr);
	if (_tcsstr(nbuf, cmpStr) == NULL)
		return FALSE;
	else
		return TRUE;

}

static BOOL DoUsbIsolation(HANDLE chdPro, DWORD sessionID, TCHAR *host, TCHAR *busid)
{
	TCHAR deviceID[MAX_PATH] = {0};
	BOOL res = FALSE;
	int devType = 0;
	DevAttr usbDevAttr;

	wsprintf(deviceID, _T("%s&%s"), host, busid);

	if (FindDeviceObject("usb", &GUID_DEVINTERFACE_USB_DEVICE,
						deviceID, UsbDevCmp,
						&usbDevAttr) == TRUE) {
		res = TRUE;
		devType = FindDeviceType(&usbDevAttr);
		DoIsolation(chdPro, devType, deviceID, sessionID, &usbDevAttr);
	}


	if (usbDevAttr.DevInfo != NULL)
		SetupDiDestroyDeviceInfoList(usbDevAttr.DevInfo);

	return res;
}

void UsbIsolation(HANDLE chdPro, DWORD sessionID, TCHAR *host, TCHAR *busid)
{
	int times = 0;
	
	Log("\n");
	while (WaitForSingleObject(chdPro, 0) == WAIT_TIMEOUT) {
		Log("....Find the usb device %d times", times+1);
		if (DoUsbIsolation(chdPro, sessionID, host, busid) == TRUE)
			break;
		times++;
		Sleep(100);
	}

	Log("usbip terminate or isolation end!");
	WaitForSingleObject(chdPro, INFINITE);
	Log("usbip terminate!");
}