#ifndef __EDP_NOTIFY_H__2014_12_03__
#define __EDP_NOTIFY_H__2014_12_03__

#define EDP_NOTIFY_ID			10
#define U_DISK_INSERT			1
#define U_DISK_PULL_OUT			2

#pragma pack(1)
typedef struct _tagUDiskInfo
{
	// 结构体版本号
	// 当前版本: 0
	long    lVer;

	// 物理卷名
	char*    lpszPhysicsVolume;

	// 逻辑卷名
	char*    lpszLogicVolume;

	// 符号连接
	char*    lpszSymbolicLink;
	
	// 盘符
	char     cDrv;
	
	// sessionId
	long     lSId;
	
	// 当前登录用户名
	char    ucName[ MAX_PATH ];
	
	// 插入/拔出
	// 1:插入 2:拔出
	ULONG            ulAction;
    
	// 物理硬盘号
	char DrvPhysicsNum[ MAX_PATH ];
    
	// 硬盘类型
	// 1:移动磁盘 2:光驱
	int  nDrvType;   

	// 扩展参数1
	unsigned char    ucReserved1[ 128 ];

	// 扩展参数2
	unsigned char    ucReserved2[ 128 ];

	// 扩展参数3
	unsigned char    ucReserved3[ 128 ];


}UDISK_INFO, *LP_UDISK_INFO;
#pragma pack()

#define    LENGTH_UDISK_INFO    sizeof( UDISK_INFO )



class IEdpNotify
{
public:
	/*
	*
	*	AddRef		增加引用计数
	*				返回值：当前引用计数
	*
	*/

	virtual LONG AddRef() = 0;
	
	/*
	*
	*	Release		减少引用计数，用计数 = 0 释放对象
	*				返回值：当前引用计数
	*
	*/

	virtual LONG Release() = 0;
	
	/*
	*
	*	Initialize	对象初始化
	*				返回值：TRUE 成功 FALSE 失败
	*
	*/

	virtual BOOL Initialize() = 0;
	
	/*
	*
	*	Terminate	对象终止，在进程退出时调用
	*				返回值：无
	*
	*/

	virtual void Terminate() = 0;


	/*
		* Parameter(s)  : 
		* Description   : U盘插入/拔出时通知
		* Return        : 成功  --  0
		                  其他  --  失败
	*/
	virtual ULONG Notify( LP_UDISK_INFO lpUDiskInfo ) = 0;


	/*
		* Parameter(s)  : 
		* Description   : 保留接口
		* Return        : 成功  --  0
		                  其他  --  失败
	*/
	virtual ULONG NotifyEx( LP_UDISK_INFO lpUDiskInfo, PVOID pParam1, PVOID pParam2 ) = 0;






	/*
	*
	*	Notify		U盘插入拔出时通知，	SessionId:用户回话ID
	*								UserName: 当前登录用户名称
	*								Drive:    U盘盘符 如 'F'
	*								Action:   U盘动作 1:插入 2:拔出
	*				返回值：0 成功，其他失败
	*
	*/
// 	virtual ULONG Notify(ULONG SessionId, LPCSTR UserName, CHAR Drive, ULONG Action) = 0;
// 	virtual ULONG Notify(ULONG SessionId, LPCWSTR UserName, CHAR Drive, ULONG Action) = 0;
};



IEdpNotify* WINAPI EdpCreateObject(ULONG ObjectId);
typedef IEdpNotify* (WINAPI *EDP_CREATE_OBJECT)(ULONG ObjectId);

/* 调用例子

	IEdpNotify * pEdpObject = EdpCreateObject(EDP_NOTIFY_ID);

	// 进程启动时初始化对象
	if(pEdpObject)
	{
		if(!pEdpObject->Initialize())
		{
			pEdpObject->Release();
			pEdpObject = NULL;
		}
	}


	UDISK_INFO uDiskInfo;
	memset( &uDiskInfo, 0, LENGTH_UDISK_INFO );

	
	// 对uDiskInfo内字段赋值
	// ...
	//

	// 当有 U 盘插入或者拔出时调用
	if(pEdpObject)
	{
		ULONG ulRet = pEdpObject->Notify( &uDiskInfo );

		// 判断 ulRet 返回值，看是否调用成功
		// 成功
		if ( 0 == ulRet )
		{
		}
		// 失败
		else
		{
		}
	}

	// 当进程退出时调用
	if(pEdpObject)
	{
		pEdpObject->Terminate();
		pEdpObject->Release();
	}

*/




#endif // __EDP_NOTIFY_H__2014_12_03__