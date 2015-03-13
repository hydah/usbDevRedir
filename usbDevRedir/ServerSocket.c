#include <WinSock2.h>   
#include <windows.h>
#include <TCHAR.h>
#include <process.h>
#include <winnt.h>
#include "NetConnection.h"
#include "Helper.h"
#include "UsbDeviceObject.h"
#include "FileSysPartition.h"
#include "UserAccount.h"

#define USB_REDIRECT_ISOLATION_PORT 56112

static void UsbDeviceIsolation(HANDLE chdPro, DWORD id, TCHAR *host, TCHAR *busid)
{
	Log(_T(">>>>begin device isolation, session id  %d, host %s, busid %s\n"),
		id,
		host,
		busid);

	UsbIsolation(chdPro, id, host, busid);
}

DWORD OSVersionInfo()
{
	OSVERSIONINFOEX os;
	os.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if (GetVersionEx((OSVERSIONINFO *)&os)) {
		if (os.wProductType == VER_NT_DOMAIN_CONTROLLER || os.wProductType == VER_NT_SERVER) {
			if (os.dwMajorVersion == 6) {
				if (os.dwMinorVersion == 3 || os.dwMinorVersion == 2) {
					Log("Windows Server %2d", 12);
					return 12;
				}
				if (os.dwMinorVersion == 0 || os.dwMinorVersion == 1) {
					Log("Windows Server %2d", 8);
					return 8;
				}
			} else if (os.dwMajorVersion == 5 && os.dwMinorVersion == 2) {
				Log("Windows Server %2d", 3);
				return 3;
			} else {
				Log("WARNING: Never get here!");
				Log("Windows Server %2d", 8);
				return 8;
			}
		}
		if (os.wProductType == VER_NT_WORKSTATION) {
			Log("Windows XP/Win7/Win8\n");
			return FALSE;
		}
	}

	return FALSE;
}

unsigned int __stdcall DoConnect(PVOID pM)
{
	ConnInfo *cinfo = (ConnInfo *)pM;
	TCHAR msg[sizeof(SendInfo)];
	SendInfo rece_info;
	TCHAR cmd[100] = {0};
	TCHAR userName[MAX_PATH] = {0};
	TCHAR curPath[MAX_PATH] = {0};
	SOCKET connSocket;
	HANDLE chdPro;
	char sendbuf[1] = {'0'};
	DWORD res;

	if (cinfo == NULL) {
		Log(_T("connInfo is NULL"));
		return 0;
	}

	connSocket = cinfo->connSocket;
	free(cinfo);	
	if(connSocket == INVALID_SOCKET) {
		Log(_T("处理连接时发生错误\n"));
		return 0;
	}

	memset(msg , '\0', sizeof(msg)) ;
	if (recv(connSocket, msg, sizeof(msg) , 0) <= 0) {
		Log(_T("接收消息失败\n"));
	} else {
		memset(&rece_info, '\0', sizeof(SendInfo));
		memcpy(&rece_info, msg, sizeof(msg));
		Log("type: %c", rece_info.type[0]);
		Log("server_ip: %s", rece_info.server_ip);
		Log("local_ip: %s", rece_info.local_ip);
		Log("bus_id: %s", rece_info.bus_id);
		Log("idVendor: %s", rece_info.idVendor);
		Log("idProduct: %s", rece_info.idProduct);

		if (rece_info.type[0] == '1') {  // '1' is query
			res = OSVersionInfo();
			switch (res) {
				case 3:
					sendbuf[0] = '3';
					break;
				case 8:
					sendbuf[0] = '8';
					break;
				case 12:
					sendbuf[0] = 'c';
					break;
				default:
					sendbuf[0] = '0';
					break;
			}
			send(connSocket, sendbuf, sizeof(sendbuf), 0);
		} else if (rece_info.type[0] == '2') { // '2' is request redirection				
			memset(cmd, '\0 ', sizeof(cmd));			
			usbipGetCurDir(curPath);				
			wsprintf(cmd, "%s\\usbip.exe -a %s %s", curPath, rece_info.local_ip, rece_info.bus_id);
			Log(_T("cmd id %s, cur path is %s"), cmd, curPath);

			rece_info.id = GetSessionIDFromIP(rece_info.local_ip);
			if (rece_info.id == -1) {
				Log("****usb device[host:%s,busid:%s] redirection failed because session cannot be created", 
					rece_info.local_ip,
					rece_info.bus_id);
				return 0;
			}
			GetUsernameFromSessionID(rece_info.id, userName);
			Log("session id: %d, user name is %s.", rece_info.id, userName);
			chdPro = RunAsUser(rece_info.id, cmd);
			Log(_T("%s %d %s %s"), rece_info.bus_id, rece_info.id,
									rece_info.idVendor,
									rece_info.idProduct
									);

			UsbDeviceIsolation(chdPro, rece_info.id, rece_info.local_ip, rece_info.bus_id);
			CloseHandle(chdPro);
			}			
	}
	closesocket(connSocket);

	return 0;
}

static void HandleConnWithNewThread(SOCKET connSock)
{
	HANDLE handle;
	ConnInfo *cinfo;
	int i = 0;
	cinfo = (ConnInfo *) malloc(sizeof(ConnInfo));
	cinfo->connSocket = connSock;
	handle= (HANDLE)_beginthreadex(NULL, 0, DoConnect, (PVOID)cinfo, 0, NULL);

	return ;
}

void WaitForConnect()
{
	
    SOCKET serverSock, connectingSock;
    struct sockaddr_in serverAddr , connectingAddr;
    int addrlen;    
	WSADATA wsaData;
	

    if(WSAStartup(0x101 , &wsaData)  != 0) {
		Log(_T("socket服务启动失败\n"));
        exit(1);
    }
    serverSock = socket(AF_INET , SOCK_STREAM , IPPROTO_TCP);
    if(serverSock == INVALID_SOCKET) {
		Log(_T("socket建立失败\n"));
        exit(1);
    }
	addrlen = sizeof(struct sockaddr);
	memset( &serverAddr , 0 , addrlen);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(USB_REDIRECT_ISOLATION_PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		Log(_T("绑定IP 地址到socket失败\n, error num is %d"), WSAGetLastError());
        exit(1);
	} 

	if (listen(serverSock , 10) == SOCKET_ERROR) {
		Log(_T("设置socket为监听状态失败\n"));
		exit(1);
	}

	while (TRUE) { 
		Log(_T(">>>>v1.0.等待客户端连接...\n"));
		connectingSock = accept(serverSock, (struct sockaddr*)&connectingAddr, &addrlen);
		HandleConnWithNewThread(connectingSock);
	}
	closesocket(serverSock);
    WSACleanup();
}