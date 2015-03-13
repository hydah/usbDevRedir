#include <stdio.h>
#include <Windows.h>
#include <Dbt.h>
#include <tchar.h>

int main(int argc, TCHAR *argv[])
{
    DEV_BROADCAST_VOLUME DevHdr                                  ;
    DWORD dwFlag = BSM_ALLCOMPONENTS;
    DWORD volumeMask;
    WPARAM wparam;
    DWORD ret;
  

    if (argc != 4)
		return 0;

    printf("BroadcastSystemMessage %s\n", argv[1]);

    if (strcmp((const char *)argv[1], "DEVICECHANGE") == 0)
    {
        volumeMask = 1 << (argv[2][0] - 'A');

        DevHdr.dbcv_devicetype  = DBT_DEVTYP_VOLUME;
        DevHdr.dbcv_size        = sizeof(DEV_BROADCAST_VOLUME);
        DevHdr.dbcv_flags       = 0;
        DevHdr.dbcv_unitmask    = volumeMask;

        if (argv[3][0] == '1') {
            wparam = DBT_DEVICEARRIVAL;
            printf(("BroadcastSystemMessage WM_DEVICECHANGE DBT_DEVICEARRIVAL unitmask %x\n"), DevHdr.dbcv_unitmask);
            //广播设备插入
            ret = BroadcastSystemMessage(BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | BSF_POSTMESSAGE ,
                                    &dwFlag,
                                    WM_DEVICECHANGE,
                                    wparam,
                                    (LPARAM)&DevHdr);
        
            if (ret < 0)	{
                printf(("BroadcastSystemMessage Fail %d ret %d\n"), GetLastError(), ret);
                return FALSE;
            } else {
                printf(("BroadcastSystemMessage ret %d error %d\n"), ret, GetLastError());
            }
        } else if (*argv[2] == '0') {
            printf(("BroadcastSystemMessage WM_DEVICECHANGE DBT_DEVICEQUERYREMOVE unitmask %x\n"), DevHdr.dbcv_unitmask);
            wparam = DBT_DEVICEQUERYREMOVE;
            //广播设备删除
            if (BroadcastSystemMessage( BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | BSF_SENDNOTIFYMESSAGE ,
                                    &dwFlag,
                                    WM_DEVICECHANGE,
                                    wparam,
                                    (LPARAM)&DevHdr) < 0)
            {
                printf(("BroadcastSystemMessage Fail %d\n"), GetLastError());
                return FALSE;
            }
            printf(("BroadcastSystemMessage WM_DEVICECHANGE DBT_DEVICEREMOVECOMPLETE unitmask %x\n"), DevHdr.dbcv_unitmask);
            wparam = DBT_DEVICEREMOVECOMPLETE;
            //广播设备删除OK
            if (BroadcastSystemMessage( BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | BSF_SENDNOTIFYMESSAGE ,
                                    &dwFlag,
                                    WM_DEVICECHANGE,
                                    wparam,
                                    (LPARAM)&DevHdr) < 0)
            {
                L_ERROR(_T("BroadcastSystemMessage Fail %d\n"), GetLastError());
                return FALSE;
            }       
        }
    }
}