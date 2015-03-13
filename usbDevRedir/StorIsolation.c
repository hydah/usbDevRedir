#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <winioctl.h>
#include <stdio.h>
#include <sddl.h>
#include <tchar.h>
#include <cfgmgr32.h>
#include <Shlwapi.h>
#include "UserAccount.h"
#include "Helper.h"

typedef struct MountList {
	TCHAR driveLetter[MAX_PATH];
	TCHAR devName[MAX_PATH];
	TCHAR volName[MAX_PATH];
	DWORD diskID;
	DWORD partitonID;
	DWORD drvType;
	struct MountList *next;
} MountList;

MountList *head = NULL;

static DWORD getDeviceNumber(HANDLE deviceHandle)
{
   STORAGE_DEVICE_NUMBER sdn;
   DWORD dwBytesReturned = 0;
   sdn.DeviceNumber = -1;
   
   if (!DeviceIoControl(deviceHandle, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                          NULL, 0, &sdn, sizeof(STORAGE_DEVICE_NUMBER), &dwBytesReturned, 
						  NULL)) {
      // handle error - like a bad handle.
      return -1;
   }

   return sdn.DeviceNumber;
}

/* Get the volume object's disk id */
static DWORD getVolumeDiskID(HANDLE deviceHandle,  STORAGE_DEVICE_NUMBER *sdn)
{
   //VOLUME_DISK_EXTENTS sdn;
   DWORD dwBytesReturned = 0;

   
   if (!DeviceIoControl(deviceHandle, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                          NULL, 0, sdn, sizeof(*sdn), &dwBytesReturned,
						  NULL)) {
      // handle error - like a bad handle.
		printf("not !!\n");
      return 0xffffffff;
   }
   printf("is  mounted\n");
   return 0;
}

/* Get one device object's parrent object's device ID */
static void GetParentDevice(PSP_DEVINFO_DATA dev_inf, TCHAR *DevIntID)
{
	 CONFIGRET cr;
	 ULONG ulStatus;
	 ULONG ulProblemNumber;
	 DEVINST dnDevInst;

	 cr = CM_Get_DevNode_Status(&ulStatus, &ulProblemNumber, dev_inf->DevInst, 0);
	 if (cr == CR_SUCCESS) {
		 printf("Ok\n");
		 cr = CM_Get_Parent(&dnDevInst, dev_inf->DevInst, 0);
		 if (cr == CR_SUCCESS) {
			 printf("Ok!\n");
			  cr = CM_Get_Device_ID(dnDevInst, DevIntID, MAX_PATH, 0);
			 if (cr == CR_SUCCESS) {
				 printf("Ok!..%s\n", DevIntID);
			 }
		 }
	 }	
}

/* Get one disk's partition list */
static void GetPartitionList(HANDLE DiskHandle, int *partitionNum)
{
	int ret;
	DWORD ior;
	DWORD i;
	PDRIVE_LAYOUT_INFORMATION_EX partitions;
	DWORD partitionsSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 127 * sizeof(PARTITION_INFORMATION_EX);
	partitions = (PDRIVE_LAYOUT_INFORMATION_EX)malloc(partitionsSize);
	Log("Begin Partition.");
	ret = DeviceIoControl(DiskHandle, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL,
							0, partitions, partitionsSize, &ior, NULL);
	if (ret != 0) {
		switch (partitions->PartitionStyle) {
			case PARTITION_STYLE_MBR: 
				Log("Partition type :MBR\n"); 
				break;
			case PARTITION_STYLE_GPT:
				Log("Partition type :GPT\n");
				break;
			default:
				Log("Partition type :unknown\n");
				break;
		}
		Log("partition count is %d\n", partitions->PartitionCount);
		for (i = 0; i < partitions->PartitionCount; i++) {
			 if (partitions->PartitionEntry[i].PartitionStyle == PARTITION_STYLE_MBR && 
				 partitions->PartitionEntry[i].Mbr.PartitionType != PARTITION_ENTRY_UNUSED &&
				 partitions->PartitionEntry[i].Mbr.RecognizedPartition) {
				 Log("Partition: %d offset: %d length: %dG\n", partitions->PartitionEntry[i].PartitionNumber, 
					 partitions->PartitionEntry[i].StartingOffset.QuadPart/1024,
					 partitions->PartitionEntry[i].PartitionLength.QuadPart/1024/1024);
				 *partitionNum = partitions->PartitionEntry[i].PartitionNumber;
			 } 
		}
	} else {
		Log("partition error!\n");
	}
}

static void DelMountPoint(TCHAR VolumeName[MAX_PATH])
{
	TCHAR pszDriveLetterSlash[MAX_PATH] = _T("C:\\");
	DWORD  CharCount = MAX_PATH + 1;
    PTCHAR Names     = NULL;
    PTCHAR NameIdx   = NULL;
    BOOL   Success   = FALSE;
	
	// Find all drive letter of the very volume.
	for (;;) {
		Names = (TCHAR *) malloc(CharCount * sizeof(TCHAR));
		if (!Names)
			return;
		Success = GetVolumePathNamesForVolumeName(
					VolumeName, Names, CharCount, &CharCount);
		if (Success)
			break;
		if (GetLastError() != ERROR_MORE_DATA )
			break;
		free(Names);
		Names = NULL;
	}

	if (Success) {
		// delete every mount point.
		for (NameIdx = Names; NameIdx[0] != '\0'; NameIdx += lstrlen(NameIdx) + 1) {
			pszDriveLetterSlash[0] = NameIdx[0];
			Log("  %s", NameIdx);
			// DeleteVolumeMountPoint function need Administator's privilege.
			if (DeleteVolumeMountPoint(pszDriveLetterSlash)) {
				Log("delete mount point success");
			} else {
				Log("cannot delete mount point");
			}
		}
	}

	if (Names != NULL) {
		free(Names);
		Names = NULL;
	}
}

static DWORD DelMountPoint2(DWORD sessionid)
{
	BOOL  fResult;
	MountList *nxt = head;
	HANDLE token;

	GetSessionToken(sessionid, &token, NULL);
	ImpersonateLoggedOnUser(token);
	while (head != NULL) {
		fResult = DefineDosDevice( 
						  DDD_RAW_TARGET_PATH|DDD_REMOVE_DEFINITION|
						  DDD_EXACT_MATCH_ON_REMOVE, head->driveLetter,
						  head->devName);
		if (!fResult) {
			Log("delete mount point %s error, %d\n", nxt->driveLetter, GetLastError());
		} else {
			Log("%s\n", head->driveLetter);
			Log("%s\n", head->devName);
			Log("delete success\n");
		}
		nxt = head;
		head = head->next;
		free(nxt);
	}
	RevertToSelf();
	return 0;
}

static DWORD ReMount(DWORD sessionid)
{
	TCHAR DriveLetter[MAX_PATH] = "C:";
	TCHAR deviceName[MAX_PATH] = "";
	HANDLE token;
	BOOL result ;
	MountList *nxt = head;

	GetSessionToken(sessionid, &token, NULL);
	ImpersonateLoggedOnUser(token);
	Log("begin remount\n");

    if (!result) {
        Log(_T("GetLocalVolumes ImpersonateLoggedOnUser Fail %d"), GetLastError());
		goto FAIL;
    }

	while (nxt != NULL) {
		while (DriveLetter[0] < 'Z') {
			if (QueryDosDevice(DriveLetter, deviceName, MAX_PATH)) {
				DriveLetter[0]++;
			} else {
				Log("Drive Letter %s is not mounted\n", DriveLetter);
				break;
			}
		}

		if (DriveLetter[0] > 'Z') {
			Log("driver Letter full, cannot mount device any more\n");
			goto FAIL;
		} else {
			Log("devName is %s, volName is %s\n", nxt->devName, nxt->volName);
			result = DefineDosDevice(DDD_RAW_TARGET_PATH, DriveLetter, nxt->devName);
			if (!result) {
				Log("remount point %s error, %d\n", DriveLetter, GetLastError());
			} else {
				lstrcpy(nxt->driveLetter, DriveLetter);
				Log("%s\n", nxt->driveLetter);
				Log("%s\n", nxt->devName);
				Log("remount success\n");
			}
			nxt = nxt->next;
		}
	}

FAIL:
	RevertToSelf();
	return result;
}

static DWORD FreeMountList()
{
	MountList *nxt = head;
	
	while (head != NULL) {
		nxt = head;
		head = head->next;
		free(nxt);
	}
	return 0;
}

static DWORD ReMountWithBinary(DWORD sessionID, TCHAR *devInsID)
{
	TCHAR processPath[MAX_PATH] = {0};
	TCHAR curPath[MAX_PATH] = {0};
	TCHAR userName[MAX_PATH] = {0};
	MountList *nxt = head;
	DWORD drvType;

	//curPath, sdn.DeviceNumber, sdn.PartitionNumber, userName, volCnt
	usbipGetCurDir(curPath);
	GetUsernameFromSessionID(sessionID, userName);

	while (nxt != NULL) {
		Log("devName is %s, volName is %s\n", nxt->devName, nxt->volName);
		if (nxt->drvType == 2)
			drvType = 1;
		else 
			drvType = 2;
/*
"%s\\SendMessage.exe %d %d %s %d %s %s"), curPath, head->diskID, 
							head->partitonID, head->volName, 
							drvType, 
							userName,
							devName);
							*/
		memset(processPath, sizeof(processPath), 0);
		wsprintf(processPath, _T("%s\\DriveReMount.exe %d %d %s %d %s %s %s"), curPath, nxt->diskID, 
							nxt->partitonID, nxt->volName, drvType, userName, nxt->devName, devInsID);
		Log(_T("ProcessPath is %s"), processPath);
		RunAsSystem(sessionID, processPath);

		nxt = nxt->next;
	}

	return TRUE;
}

static void EdpNotify(DWORD sessionID, DWORD action)
{
	TCHAR processPath[MAX_PATH] = {0};
	TCHAR curPath[MAX_PATH] = {0};	
	TCHAR userName[MAX_PATH] = {0};
	TCHAR logicVolume[MAX_PATH] = {0};
	DWORD diskID, partitionID, drvType;
	BOOL  fResult;
	MountList *nxt = head;
	HANDLE token;

	usbipGetCurDir(curPath);
	GetUsernameFromSessionID(sessionID, userName);

	while (nxt != NULL) {
		Log("devName is %s, volName is %s\n", nxt->devName, nxt->volName);
		if (nxt->drvType == 2)
			drvType = 1;
		else 
			drvType = 2;	
	
		wsprintf(processPath, _T("%s\\SendMessage.exe %d %d %s %d %s %s %d"), curPath, nxt->diskID, 
							nxt->partitonID, nxt->volName, 
							drvType, nxt->driveLetter, 
							userName, action);

		Log(_T("ProcessPath is %s"), processPath);
		RunAsSystem(sessionID, processPath);

		nxt = nxt->next;
	}

}
/* 
 *	Enumerate each volume and get the very one correspond to 
 *	the particular disk, then unmap that volume to delete the 
 *	drive letter.
 */
static BOOL DoEnumVolumeAndUnmount(int diskID, DWORD *volNum, int *partitionID, TCHAR *devName)
{
	TCHAR DeviceName[MAX_PATH] = {0};
	int ret, idx;
	BOOL suc;
	int volDisk;
	HANDLE FindHandle = INVALID_HANDLE_VALUE, hd;
	TCHAR VolumeName[MAX_PATH] = "";
	DWORD charCnt = 0;
	DWORD devstrlen = 0;
	BOOL res = FALSE;
	MountList *nxt = head;
	STORAGE_DEVICE_NUMBER sdn;
	*volNum = 0;	
	
	Log("Begin Enum volume...");
	if (devName == NULL) {
		Log("devName is NULL");
		return res;
	}

	FindHandle = FindFirstVolume(VolumeName, ARRAYSIZE(VolumeName));
    if (FindHandle == INVALID_HANDLE_VALUE) {
        ret = GetLastError();
        Log("FindFirstVolumefailed with error code %d\n", ret);
        return res;
    }

	while (TRUE) {
		idx = lstrlen(VolumeName) - 1;
        if (VolumeName[0]     != '\\' ||
            VolumeName[1]     != '\\' ||
            VolumeName[2]     != '?'  ||
            VolumeName[3]     != '\\' ||
            VolumeName[idx] != '\\') {
            ret = ERROR_BAD_PATHNAME;
            Log("FindFirstVolumeW/FindNextVolumeW returned a bad path: %s\n", VolumeName);
            break;
        }
		//  CreateFile does not allow a trailing backslash,
		//  so temporarily remove it.
		VolumeName[idx] = '\0';
		hd = CreateFile(VolumeName,
							GENERIC_READ,
							FILE_SHARE_READ,
							NULL,
							OPEN_ALWAYS,
							FILE_FLAG_OVERLAPPED,
							NULL);	
		if (hd == INVALID_HANDLE_VALUE) {
			Log("Cannot open %s, Error code is %d", VolumeName, GetLastError());
		} else {  // find a volume in the disk.
			getVolumeDiskID(hd, &sdn);	
			volDisk = sdn.DeviceNumber;

			if (volDisk == diskID) {
				*partitionID = sdn.PartitionNumber;
				res = TRUE;
				Log("Volume %s correspond to disk %d, partition is %d", VolumeName, volDisk, *partitionID);
				charCnt = QueryDosDevice(&VolumeName[4], DeviceName, ARRAYSIZE(DeviceName));        
				if (charCnt == 0) {
					ret = GetLastError();
					Log("QueryDosDevice failed with error code %d\n", ret);
					break;
				}
				Log("\tDevice name is %s", DeviceName);
				lstrcpy(devName+devstrlen, DeviceName);
				
				devstrlen = devstrlen+strlen(DeviceName)+1;
				*volNum = *volNum+1;
				Log("devstrlen is %d, volNum is %d", devstrlen, *volNum);
				VolumeName[idx] = '\\';
				DelMountPoint(VolumeName);

				if (head == NULL) {
					head = (MountList *)malloc(sizeof(MountList));
					nxt = head;
				} else {
					nxt->next = (MountList *)malloc(sizeof(MountList));
					nxt = nxt->next;
				}
				lstrcpy(nxt->devName, DeviceName);
				nxt->diskID = volDisk;
				nxt->partitonID = sdn.PartitionNumber;
				nxt->drvType = sdn.DeviceType;
				lstrcpy(nxt->volName, VolumeName);
				nxt->next = NULL;
			} else {
				Log("Volume %s doesn't correspond to disk %d", VolumeName, volDisk);
			}
		}
		CloseHandle(hd);
		Log("get here1!!!!\n");
		suc = FindNextVolume(FindHandle, VolumeName, ARRAYSIZE(VolumeName));
		Log("get here2!!!!\n");
        if (!suc) {
            ret = GetLastError();
            if (ret != ERROR_NO_MORE_FILES) {
				Log("get here3!!!!\n");
                Log("FindNextVolumeW failed with error code %d\n", ret);
                break;
            }
            ret = ERROR_SUCCESS;
			Log("get here4!!!!\n");
            break;
        }
		Log("VolName is %s\n", VolumeName);
	} // end of while
	devName[devstrlen] = '\0';

	FindVolumeClose(FindHandle);
    FindHandle = INVALID_HANDLE_VALUE;
	return res;
}

static int EnumVolumeAndUnmount(HANDLE chdPro, int diskID, DWORD *volNum, int *partitionID, TCHAR *devName)
{
	int times = 0;
	int ret = -1;
	Log("\n");
	while (WaitForSingleObject(chdPro, 0) == WAIT_TIMEOUT) {
		Log("....enumerate volume %d times", times+1);
		if (DoEnumVolumeAndUnmount(diskID, volNum, partitionID,devName) == TRUE) {
			ret = 0;
			break;
		}
		times++;
		Sleep(100);
	}
	
	return ret;
}

static BOOL DiskCmp(DevAttr *pDevAttr, TCHAR *cmpStr)
{
	TCHAR szDeviceInstanceID[MAX_DEVICE_ID_LEN];

	GetParentDevice(&(pDevAttr->DevInfoData), szDeviceInstanceID);
		// Check if we got the correct device.
	if (lstrcmp(szDeviceInstanceID, cmpStr) != 0)
		return FALSE;
	else
		return TRUE;
}

static BOOL DoEnumDiskAndRemap(HANDLE chdPro, TCHAR *devInsID, DWORD sessionID)
{
	BOOL res = FALSE;
	HANDLE hd;
	int memberIndex = 0, rc = 1;	
	int diskID = -1;
	int partitionID;
	TCHAR partitionName[200] = {0};
	int partitionNum = -1;
	TCHAR *processPath;	
	TCHAR cmd[MAX_PATH] = {0};
	TCHAR curPath[MAX_PATH] = {0};
	TCHAR devPath[MAX_PATH] = {0};
	TCHAR devName[MAX_PATH] = {0};
	TCHAR userName[MAX_PATH] = {0};
	DWORD volCnt = 0;
	PTCHAR devNameIdx   = NULL;
	int ret;
	DevAttr devAttr;
	//STORAGE_DEVICE_NUMBER sdn;
	// Get devices info.
	if (FindDeviceObject("disk", &GUID_DEVINTERFACE_DISK, devInsID, DiskCmp,
							&devAttr) == TRUE) {
		hd = CreateFile(devAttr.devPath,
							GENERIC_READ|GENERIC_WRITE,
							0,
							NULL,
							OPEN_EXISTING,
							FILE_FLAG_OVERLAPPED,
							NULL);

		if (hd == INVALID_HANDLE_VALUE) {
			Log("error code is %d", GetLastError());
			CloseHandle(hd);
		} else {
			diskID = getDeviceNumber(hd);
			Log("disk num is %d", diskID);
			//GetPartitionList(hd, &partitionNum);
			CloseHandle(hd);

			// Delete drive letter.
			ret = EnumVolumeAndUnmount(chdPro, diskID, &volCnt, &partitionID, devName);
			if (ret == -1) { 
				// if the device has been removed, the routine blow should not be executed.
				Log("****Device %s has been removed!", devInsID);
				goto end;
			}

			Log("volCnt is %d, devName is %s", volCnt, devName);

#if 1
			// Remap every Volume.
			res = ReMount(sessionID);
			// Notify all desk
			EdpNotify(sessionID, 1);
#else
			res = ReMountWithBinary(sessionID, devInsID);
#endif
		}
		
	}// end of if

end:
	// Free the detail memory and destroy the device information list.
	if (devAttr.DevInfo != NULL)
		SetupDiDestroyDeviceInfoList(devAttr.DevInfo);
	return res;
}

/*
 * Enumerate every disk in the system and get 
 * the one that is made from the usb device whose
 * vid and pid is assigned. Then use DefineDosDevice to 
 * remap every partition in that disk. So we can map 
 * the partition in the use session space.
 */
void EnumDiskAndRemap(HANDLE chdPro, TCHAR *DevInsID, DWORD sessionID)
{
	int times = 0;
	Log("\n");
	while (WaitForSingleObject(chdPro, 0) == WAIT_TIMEOUT) {
		Log("....Find disk %d times", times+1);
		if (DoEnumDiskAndRemap(chdPro, DevInsID, sessionID) == TRUE)
			break;
		times++;
		Sleep(100);
	}

	WaitForSingleObject(chdPro, INFINITE);
#if 1
	EdpNotify(sessionID, 2);
	DelMountPoint2(sessionID);
#else
	FreeMountList();
#endif
}

