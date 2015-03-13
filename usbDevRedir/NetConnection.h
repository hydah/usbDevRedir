typedef struct{
	char type[1];
	char server_ip[20];
	char local_ip[20];
	char bus_id[16];
	char idVendor[10];
	char idProduct[10];
	DWORD id;
} SendInfo;

typedef struct {
	char bus_id[16];	
	char idVendor[10];
	char idProduct[10];
	DWORD id;
	char local_ip[20];
} devInsInfo;

typedef struct {
	SOCKET connSocket;
} ConnInfo;

void WaitForConnect();