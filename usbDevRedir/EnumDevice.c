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


BOOL FindDeviceObject(IN TCHAR *devCls, IN const GUID *clsGuid, IN TCHAR *cmpStr, IN ClsCmp cmp,
					  OUT DevAttr *pDevAttr)
{
	SP_DEVICE_INTERFACE_DATA dev_interface_data;
	PSP_DEVICE_INTERFACE_DETAIL_DATA pDevInfoDetail = NULL;
	unsigned long len;
	int memberIndex = 0, rc = 1;
	BOOL res = FALSE;
	int i = 0;

	Log("begin enum %s device", devCls);
	// Get devices info.
	pDevAttr->DevInfo = SetupDiGetClassDevs(
		(LPGUID) clsGuid, /* ClassGuid */
		NULL,	/* Enumerator */
		NULL,	/* hwndParent */
		DIGCF_PRESENT|DIGCF_DEVICEINTERFACE /* Flags */
	);
	if (INVALID_HANDLE_VALUE == pDevAttr->DevInfo) {
		Log("SetupDiGetClassDevs failed: %ld", GetLastError());
		return res;
	}
	
	// Prepare some structures.
	dev_interface_data.cbSize = sizeof(dev_interface_data);
	pDevAttr->DevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	
	// Loop reading information from the devices until we get some error.
	while (rc) {
		// Get device info data.
		rc = SetupDiEnumDeviceInterfaces(
			pDevAttr->DevInfo,		/* DeviceInfoSet */
			NULL,
			clsGuid,
			memberIndex,	/* MemberIndex */
			&dev_interface_data	/* DeviceInfoData */
		);
	
		if (!rc) {			
			if (GetLastError() == ERROR_NO_MORE_ITEMS) {
				Log("exit enum %s device.", devCls);
				// No more items. Leave.
				memberIndex = -1;
				break;
			} else {
				// Something else...
				Log("error getting  %s device information.", devCls);
				goto end;
			}
		}
		// Get required length for dev_interface_detail.
		rc = SetupDiGetDeviceInterfaceDetail(
			pDevAttr->DevInfo,				/* DeviceInfoSet */
			&dev_interface_data,	/* DeviceInterfaceData */
			NULL,					/* DeviceInterfaceDetailData */
			0,						/* DeviceInterfaceDetailDataSize */
			&len,					/* RequiredSize */
			NULL					/* DeviceInfoData */
		);
		if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
			Log("Error in SetupDiGetDeviceInterfaceDetail: %ld", GetLastError());
			goto end;
		}
		
		// Allocate the required memory and set the cbSize.
		pDevInfoDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(len);
		if (NULL == pDevInfoDetail) {
			Log("can't malloc %lu size memoery", len);
			goto end;
		}
		pDevInfoDetail->cbSize = sizeof(*pDevInfoDetail);

		// Try to get device details.
		rc = SetupDiGetDeviceInterfaceDetail(
			pDevAttr->DevInfo,				/* DeviceInfoSet */
			&dev_interface_data,	/* DeviceInterfaceData */
			pDevInfoDetail,	/* DeviceInterfaceDetailData */
			len,					/* DeviceInterfaceDetailDataSize */
			&len,					/* RequiredSize */
			&(pDevAttr->DevInfoData)			/* DeviceInfoData */
		);
		if (!rc) {
			// Errors.
			Log("Error in SetupDiGetDeviceInterfaceDetail");
			goto end;
		}
		Log("cur device is %s", pDevInfoDetail->DevicePath);

		// Check if we got the correct device.
		Log("%s", cmpStr);
		lstrcpy(pDevAttr->devPath, pDevInfoDetail->DevicePath);
		if (cmp(pDevAttr, cmpStr) == FALSE) {
			// Wrong hardware ID. Get the next one. 
			memberIndex++;
			i++;
			Log("can not find device.");
			continue;
		} else {
			// Got it!
			Log("find device.");
			break;
		}	
	} // end of while

	if (memberIndex != -1) {
		// Find 
		res = TRUE;
	}

end:
	if (pDevInfoDetail) {
		free(pDevInfoDetail);
	}
	return res;
}