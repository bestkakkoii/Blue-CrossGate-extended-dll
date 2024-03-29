#include "CSocket.h"
#include <iostream>
#include <stdio.h>
#include <netfw.h>
#include <versionhelpers.h>
#include "CAsm.h"

//#pragma comment(lib, "ws2_32.lib")
SOCKET* g_sock_ptr = nullptr;
SOCKET* g_blue_sock_ptr = nullptr;

std::mutex mutex_socket_write;

///******************************************************************************************
//Function:        ConvertLPWSTRToLPSTR
//Description:     LPWSTR转char*
//Input:           lpwszStrIn:待转化的LPWSTR类型
//Return:          转化后的char*类型
//*******************************************************************************************/
//char* ConvertLPWSTRToLPSTR(LPWSTR lpwszStrIn)
//{
//	LPSTR pszOut = NULL;
//	try
//	{
//		if (lpwszStrIn != NULL)
//		{
//			int nInputStrLen = wcslen(lpwszStrIn);
//
//			// Double NULL Termination
//			int nOutputStrLen = WideCharToMultiByte(CP_ACP, 0, lpwszStrIn, nInputStrLen, NULL, 0, 0, 0) + 2;
//			pszOut = new char[nOutputStrLen];
//
//			if (pszOut)
//			{
//				memset(pszOut, 0x00, nOutputStrLen);
//				WideCharToMultiByte(CP_ACP, 0, lpwszStrIn, nInputStrLen, pszOut, nOutputStrLen, 0, 0);
//			}
//		}
//	}
//	catch (std::exception e)
//	{
//	}
//
//	return pszOut;
//}

int CSocket::is_socket_alive(const SOCKET& ss)
{
	if (ss == INVALID_SOCKET)
		return 0;
	//KeepAlive實現
	TCP_KEEPALIVE inKeepAlive = {}; //輸入參數
	unsigned long ulInLen = sizeof(TCP_KEEPALIVE);

	TCP_KEEPALIVE outKeepAlive = {}; //輸出參數
	unsigned long ulOutLen = sizeof(TCP_KEEPALIVE);

	unsigned long ulBytesReturn = 0;

	//設置socket的keep alive爲5秒，並且發送次數爲3次
	inKeepAlive.onoff = 1;
	inKeepAlive.keepaliveinterval = 100; //兩次KeepAlive探測間的時間間隔
	inKeepAlive.keepalivetime = 100; //開始首次KeepAlive探測前的TCP空閉時間

	if (WSAIoctl((unsigned int)ss, SIO_KEEPALIVE_VALS,
		(LPVOID)&inKeepAlive, ulInLen,
		(LPVOID)&outKeepAlive, ulOutLen,
		&ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
	{
		return is_disconnect(ss);
	}
	else
		return 1;
}

//異步檢查 tcp 是否斷線
bool CSocket::is_disconnect(const SOCKET& a)
{
	if (a == INVALID_SOCKET)
		return true;
	unsigned long i = 0;
	CKernel32& ker32 = CKernel32::get_instance();
	int iResult = ker32.My_ioctlsocket(a, FIONREAD, &i);
	if (iResult == SOCKET_ERROR)
		return true;
	return false;
}

bool CSocket::tcp_initialize()
{
	s_server = INVALID_SOCKET;
	server_hWnd = NULL;
	m_ip = {};
	wchar_t szAppPath[MAX_PATH] = {};
	GetModuleFileName(NULL, szAppPath, MAX_PATH);
	constexpr wchar_t a[7] = { 'b','l','u','e','c','g', '\0' };
	WriteFireWall(a, szAppPath, true);
	//初始化套接字庫
	const WORD w_req = MAKEWORD(2, 2);//版本號
	WSADATA wsadata;
	CKernel32& ker32 = CKernel32::get_instance();
	int err;
	err = ker32.My_WSAStartup(w_req, &wsadata);
	if (err != 0) {
		//cout << "初始化套接字庫失敗！" << endl;
		return FALSE;
	}

	//檢測版本號
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wHighVersion) != 2) {
		//cout << "套接字庫版本號不符！" << endl;
		ker32.My_WSACleanup();
		return FALSE;
	}

	return TRUE;
}

void CSocket::close_server_socket()
//說明：關閉與Server的連線
{
	//關閉套接字
	if (s_server != INVALID_SOCKET)
	{
		CKernel32& ker32 = CKernel32::get_instance();
		ker32.My_shutdown(s_server, SD_BOTH);
		ker32.My_closesocket(s_server);
	}
}

void CSocket::close_game_socket()
{
	//關閉套接字

	SOCKET* socket = BLUESOCKET;
	if (*socket != INVALID_SOCKET)
	{
		CKernel32& ker32 = CKernel32::get_instance();
		ker32.My_shutdown(*socket, SD_BOTH);
		ker32.My_closesocket(*socket);
	}
	*socket = INVALID_SOCKET;
	delete socket;
	socket = nullptr;
}

int CSocket::wsa_clean()
//說明：關閉與Server的連線
{
	CKernel32& ker32 = CKernel32::get_instance();
	return ker32.My_WSACleanup();
}

const bool CSocket::isexist(const SOCKET& a)
{
	return (a != INVALID_SOCKET) && (is_socket_alive(a)) ? true : false;
}

bool CSocket::is_server_opened(const bool en)
//說明：檢測Socket是否已開啟
//傳回：檢測結果
{
	if (en)
	{
		CKernel32& ker32 = CKernel32::get_instance();
		if (server_hWnd != NULL && ker32.My_IsWindow(server_hWnd))
			return false;
	}
	return isexist(s_server);
}

bool CSocket::is_game_opened(const bool en)
{
	if (en)
	{
		CKernel32& ker32 = CKernel32::get_instance();
		if (ghWnd != NULL && ker32.My_IsWindow(ghWnd))
			return false;
	}
	return isexist(*BLUESOCKET);
}

/*
* WSAECONNREFUSED (10061): 無法連線
* WSAEADDRINUSE (10048): 沒有可用的port
* WSAETIMEDOUT (10060): 連線逾時
*/
bool CSocket::tcp_open()
//說明：開啟與Server的連線
//輸入：hostname,port = Server位址與通訊埠
//傳回：失敗傳回false
{
	if (is_server_opened())
		close_server_socket();

	//获取系统temp目录

	wchar_t	strTmpPath[MAX_PATH] = {};
	GetTempPath(MAX_PATH, strTmpPath);
	const std::wstring suffix = L"\\BlueCgHP\\config.ini";
	const std::wstring wtemppath = strTmpPath + suffix;
	GetPrivateProfileString(L"BlueCgHP", L"ip", L"", m_ip.ip, MAX_PATH, wtemppath.c_str());
	m_ip.port = (u_short)GetPrivateProfileInt(L"BlueCgHP", L"port", 0, wtemppath.c_str());
	char ip[MAX_PATH] = {};
	CKernel32& ker32 = CKernel32::get_instance();
	CRuntime::wchar2char(m_ip.ip, ip);
	SOCKADDR_IN server_addr = {};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.S_un.S_addr = ker32.My_inet_addr(ip);
	server_addr.sin_port = ker32.My_htons(m_ip.port);
	//创建套接字
	s_server = ker32.My_socket(AF_INET, SOCK_STREAM, 0);
	if (s_server == INVALID_SOCKET)
		return FALSE;

	*g_sock_ptr = s_server;
	int nagle_status = 1;
	setsockopt(s_server, IPPROTO_TCP, TCP_NODELAY, (char*)&nagle_status, sizeof(int));
	int keepAlive = 1;
	setsockopt(s_server, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(int));
	int sendlowat = 13;
	setsockopt(s_server, SOL_SOCKET, SO_SNDLOWAT, (char*)&sendlowat, sizeof(int));
	int recvlowat = 13;
	setsockopt(s_server, SOL_SOCKET, SO_RCVLOWAT, (char*)&recvlowat, sizeof(int));
	int sendbuffsize = 65535;
	setsockopt(s_server, SOL_SOCKET, SO_SNDBUF, (char*)&sendbuffsize, sizeof(int));
	int recvbuffsize = 65535;
	setsockopt(s_server, SOL_SOCKET, SO_RCVBUF, (char*)&recvbuffsize, sizeof(int));
	int timeout = 10;
	setsockopt(s_server, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(int));

	if (ker32.My_connect(s_server, (SOCKADDR*)&server_addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		close_server_socket();
		return FALSE;
	}
	return TRUE;
}

void CSocket::SetSocket(const SOCKET& socket)
//說明：設定連線的socket
//輸入：socket = 連線的socket
{
	s_server = socket;
}

//異步 tcp 接收數據
char* _szRecv = new char[65535];
//消息缓冲区
char* _szMsgBuf = new char[65535 * 10];
int _lastPos = 0;
int CSocket::Recv(SOCKET s, char* buf, int len)
{
	//5 首先接收数据包头
	CKernel32& ker32 = CKernel32::get_instance();
	int nlen = ker32.My_recv(s, _szRecv, len, 0); //接受客户端的数据 第一个参数应该是客户端的socket对象
	//cout << "nlen = " << nlen << endl;
	if (nlen <= 0)
	{
		//客户端退出
		//cout << "客户端:Socket = " << _sock << " 与服务器断开连接，任务结束" << endl;
		return -1;
	}
	//内存拷贝 将收取的数据拷贝到消息缓冲区中
	ker32.My_NtMoveMemory(_szMsgBuf + _lastPos, _szRecv, nlen);
	//消息缓冲区尾部偏移量
	_lastPos += nlen;
	while (_lastPos >= sizeof(SocketCheck))		//当前接收的消息长度大于数据头  循环处理粘包
	{
		SocketCheck* header = (SocketCheck*)_szMsgBuf;
		if (_lastPos >= header->cbsize)		//判断是否少包
		{
			//处理剩余未处理缓冲区数据的长度
			int nSize = header->cbsize;
			//处理网络消息

			//将剩余消息 前移方便下一次处理
			ker32.My_NtMoveMemory(_szMsgBuf, _szMsgBuf + nSize, _lastPos - nSize);
			_lastPos = _lastPos - nSize;//位置迁移
		}
		else
		{
			//剩余数据不够一条完整消息
			break;
		}
	}

	ker32.My_NtMoveMemory(buf, _szMsgBuf, len);
	return nlen;
}

//int CSocket::Recv(SOCKET s, char* buf, int len)
//{
//	int ret = 0;
//	int retry = 0;
//	while (retry < 5)
//	{
//		CKernel32& ker32 = CKernel32::get_instance();
//		ret = ker32.My_recv(s, buf, len, 0);
//		if (ret == SOCKET_ERROR)
//		{
//			if (WSAGetLastError() == WSAEWOULDBLOCK)
//			{
//				Sleep(10);
//				retry++;
//				continue;
//			}
//			else
//				return -1;
//		}
//		else
//			return ret;
//	}
//	return -1;
//}

//異步 tcp 發送數據
int CSocket::Send(SOCKET s, char* buf, int len)
{
	int ret = 0;
	int retry = 0;
	while (retry < 5)
	{
		CKernel32& ker32 = CKernel32::get_instance();
		ret = ker32.My_send(s, buf, len, 0);
		if (ret == SOCKET_ERROR)
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				Sleep(10);
				retry++;
				continue;
			}
			else
				return -1;
		}
		else
			return ret;
	}
	return -1;
}

bool CSocket::Read(char* recv_buf, const int& len, int& recv_len)
//說明：讀取資料
//輸入：data, len = 資料緩衝區與大小
//輸出：data = 讀取的資料, ret_len = 實際讀取的資料大小，0表對方已斷線
//傳回：失敗傳回false
//備註：本函數會一直等到有讀取資料或結束連線時才傳回
{
	recv_len = Recv(s_server, recv_buf, len);
	return (recv_len < 0 && errno != EINTR) ? FALSE : TRUE;
}

bool CSocket::Write(char* send_buf, const int& len)
//說明：送出資料
//輸入：data, len = 資料緩衝區與大小
//傳回：失敗傳回false
{
	///mutex_socket_write.lock();

	int send_len = Send(s_server, (char*)send_buf, len);
	///mutex_socket_write.unlock();
	return (send_len != SOCKET_ERROR);
}

void CSocket::WriteFireWallXP(const LPCTSTR& ruleName, const LPCTSTR& appPath, const bool& NoopIfExist)
{
	HRESULT hr = S_OK;
	HRESULT comInit = E_FAIL;
	INetFwProfile* fwProfile = NULL;
	INetFwMgr* fwMgr = NULL;
	INetFwPolicy* fwPolicy = NULL;
	INetFwAuthorizedApplication* fwApp = NULL;
	INetFwAuthorizedApplications* fwApps = NULL;
	BSTR bstrRuleName = SysAllocString(ruleName);
	BSTR bstrAppName = SysAllocString(appPath);

	// tcp_initialize COM.
	comInit = CoInitializeEx(
		0,
		COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE
	);

	// Ignore RPC_E_CHANGED_MODE; this just means that COM has already been
	// initialized with a different mode. Since we don't care what the mode is,
	// we'll just use the existing mode.
	if (comInit != RPC_E_CHANGED_MODE)
	{
		hr = comInit;
		if (FAILED(hr))
		{
			//printf("CoInitializeEx failed: 0x%08lx\n", hr);
			goto error;
		}
	}

	// Create an instance of the firewall settings manager.
	hr = CoCreateInstance(
		__uuidof(NetFwMgr),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(INetFwMgr),
		(void**)&fwMgr
	);
	if (FAILED(hr))
	{
		//printf("CoCreateInstance failed: 0x%08lx\n", hr);
		goto error;
	}
	// Retrieve the local firewall policy.
	hr = fwMgr->get_LocalPolicy(&fwPolicy);
	if (FAILED(hr))
	{
		//printf("get_LocalPolicy failed: 0x%08lx\n", hr);
		goto error;
	}

	// Retrieve the firewall profile currently in effect.
	hr = fwPolicy->get_CurrentProfile(&fwProfile);
	if (FAILED(hr))
	{
		//printf("get_CurrentProfile failed: 0x%08lx\n", hr);
		goto error;
	}

	// Retrieve the authorized application collection.
	hr = fwProfile->get_AuthorizedApplications(&fwApps);
	if (FAILED(hr))
	{
		//printf("get_AuthorizedApplications failed: 0x%08lx\n", hr);
		goto error;
	}

	//check if exist
	if (NoopIfExist)
	{
		hr = fwApps->Item(bstrRuleName, &fwApp);
		if (hr == S_OK)
		{
			//printf("item is exist");
			goto error;
		}
	}

	// Create an instance of an authorized application.
	hr = CoCreateInstance(
		__uuidof(NetFwAuthorizedApplication),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(INetFwAuthorizedApplication),
		(void**)&fwApp
	);
	if (FAILED(hr))
	{
		//printf("CoCreateInstance failed: 0x%08lx\n", hr);
		goto error;
	}
	// Set the process image file name.
	hr = fwApp->put_ProcessImageFileName(bstrAppName);
	if (FAILED(hr))
	{
		//printf("put_ProcessImageFileName failed: 0x%08lx\n", hr);
		goto error;
	}

	// Set the application friendly name.
	hr = fwApp->put_Name(bstrRuleName);
	if (FAILED(hr))
	{
		//printf("put_Name failed: 0x%08lx\n", hr);
		goto error;
	}

	// Add the application to the collection.
	hr = fwApps->Add(fwApp);
	if (FAILED(hr))
	{
		//printf("Add failed: 0x%08lx\n", hr);
		goto error;
	}

error:
	// Release the local firewall policy.
	if (fwPolicy != NULL)
	{
		fwPolicy->Release();
	}

	// Release the firewall settings manager.
	if (fwMgr != NULL)
	{
		fwMgr->Release();
	}
	SysFreeString(bstrRuleName);
	SysFreeString(bstrAppName);
	if (fwApp != NULL)
	{
		fwApp->Release();
	}

	// Release the authorized application collection.
	if (fwApps != NULL)
	{
		fwApps->Release();
	}

	if (fwProfile != NULL)
	{
		fwProfile->Release();
	}

	// Uninitialize COM.
	if (SUCCEEDED(comInit))
	{
		CoUninitialize();
	}
}

//寫入防火墻，最低支持版本Windows Vista
void CSocket::WriteFireWallOverXP(const LPCTSTR& ruleName, const LPCTSTR& appPath, const bool& NoopIfExist)
{
	HRESULT hrComInit = S_OK;
	HRESULT hr = S_OK;

	INetFwPolicy2* pNetFwPolicy2 = NULL;
	INetFwRules* pFwRules = NULL;
	INetFwRule* pFwRule = NULL;

	BSTR bstrRuleName = SysAllocString(ruleName);
	BSTR bstrAppName = SysAllocString(appPath);

	// tcp_initialize COM.
	hrComInit = CoInitializeEx(
		0,
		COINIT_APARTMENTTHREADED
	);

	// Ignore RPC_E_CHANGED_MODE; this just means that COM has already been
	// initialized with a different mode. Since we don't care what the mode is,
	// we'll just use the existing mode.
	if (hrComInit != RPC_E_CHANGED_MODE)
	{
		if (FAILED(hrComInit))
		{
			//printf("CoInitializeEx failed: 0x%08lx\n", hrComInit);
			goto Cleanup;
		}
	}

	// Retrieve INetFwPolicy2
	hr = CoCreateInstance(
		__uuidof(NetFwPolicy2),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(INetFwPolicy2),
		(void**)&pNetFwPolicy2);

	if (FAILED(hr))
	{
		//printf("CoCreateInstance for INetFwPolicy2 failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

	// Retrieve INetFwRules
	hr = pNetFwPolicy2->get_Rules(&pFwRules);
	if (FAILED(hr))
	{
		//printf("get_Rules failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

	//see if existed
	if (NoopIfExist)
	{
		hr = pFwRules->Item(bstrRuleName, &pFwRule);
		if (hr == S_OK)
		{
			//printf("Item existed", hr);
			goto Cleanup;
		}
	}

	// Create a new Firewall Rule object.
	hr = CoCreateInstance(
		__uuidof(NetFwRule),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(INetFwRule),
		(void**)&pFwRule);
	if (FAILED(hr))
	{
		//printf("CoCreateInstance for Firewall Rule failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

	// Populate the Firewall Rule object

	pFwRule->put_Name(bstrRuleName);
	pFwRule->put_ApplicationName(bstrAppName);
	pFwRule->put_Action(NET_FW_ACTION_ALLOW);
	pFwRule->put_Enabled(VARIANT_TRUE);

	// Add the Firewall Rule
	hr = pFwRules->Add(pFwRule);
	if (FAILED(hr))
	{
		//printf("Firewall Rule Add failed: 0x%08lx\n", hr);
		goto Cleanup;
	}

Cleanup:

	// Free BSTR's
	SysFreeString(bstrRuleName);
	SysFreeString(bstrAppName);

	// Release the INetFwRule object
	if (pFwRule != NULL)
	{
		pFwRule->Release();
	}

	// Release the INetFwRules object
	if (pFwRules != NULL)
	{
		pFwRules->Release();
	}

	// Release the INetFwPolicy2 object
	if (pNetFwPolicy2 != NULL)
	{
		pNetFwPolicy2->Release();
	}

	// Uninitialize COM.
	if (SUCCEEDED(hrComInit))
	{
		CoUninitialize();
	}
}

void CSocket::WriteFireWall(const LPCTSTR& ruleName, const LPCTSTR& appPath, const bool& NoopIfExist)
{
	if (IsWindowsVistaOrGreater())  //600為vista
	{
		WriteFireWallOverXP(ruleName, appPath, NoopIfExist);
	}
}

int CSocket::get_local_ip(char* ip)
{
	char name[MAX_PATH] = {};
	PHOSTENT hostinfo;
	CKernel32& ker32 = CKernel32::get_instance();
	if (ker32.My_gethostname(name, sizeof(name)) == 0)
	{
		if ((hostinfo = ker32.My_gethostbyname(name)) != NULL)
		{
			const char* p = ker32.My_inet_ntoa(*((struct in_addr*)hostinfo->h_addr_list[0]));
			_snprintf_s(ip, MAX_PATH, _TRUNCATE, "%s\0", p);
		}
	}
	return 1;
}

void process_event(MSG& msg)
{
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);//translates virtual-key messages into character messages
		DispatchMessage(&msg);//dispatches a message to a window procedure
	}
}
void TDelay(long nTime, bool en)
{
	HANDLE hTimer = 0;
	MSG msg = { 0 };
	LARGE_INTEGER lar = { 0 };

	lar.QuadPart = -10 * static_cast<LONGLONG>(nTime) * 1000;

	if ((hTimer = CreateWaitableTimer(NULL, FALSE, NULL)) != NULL)
	{
		SetWaitableTimer(hTimer, &lar, NULL, NULL, NULL, FALSE);

		while (MsgWaitForMultipleObjects(1, &hTimer, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
		{
			if (!en)
				process_event(msg);
		}
		CloseHandle(hTimer);
	}
}

void CSocket::tcp_terminate_all()
{
	close_game_socket();
	close_server_socket();
	wsa_clean();
}