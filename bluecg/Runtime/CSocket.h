#pragma once
#ifndef CSOCKET_H
#define CSOCKET_H

#include <winsock2.h>
#include "CKernel32.h"
#include "CRuntime.h"
#include <mutex>

extern SOCKET* g_sock_ptr;
extern SOCKET* g_blue_sock_ptr;

//make an DWORD address

inline int exit_process()
{
	CKernel32& ker32 = CKernel32::get_instance();
	__try
	{
		if (g_blue_sock_ptr != nullptr)
		{
			if (*g_blue_sock_ptr != INVALID_SOCKET)
			{
				ker32.My_shutdown(*g_blue_sock_ptr, SD_BOTH);
				ker32.My_closesocket(*g_blue_sock_ptr);
			}

			delete g_blue_sock_ptr;
			g_blue_sock_ptr = nullptr;
		}
		if (g_sock_ptr != nullptr)
		{
			if (*g_sock_ptr != INVALID_SOCKET)
			{
				ker32.My_shutdown(*g_sock_ptr, SD_BOTH);
				ker32.My_closesocket(*g_sock_ptr);
			}
			delete g_sock_ptr;
			g_sock_ptr = nullptr;
		}
	}
	__finally
	{
		delete (SOCKET*)0xD0CEA0;
		memset((void*)0xD0CEA0, 0, sizeof(SOCKET*));
		ker32.My_WSACleanup();
		exit(0);
	}

	//return 1;
}

/*
* Reference : http://www.programmer-club.com/pc2020v5/Forum/ShowSameTitleN.asp?URL=N&board_pc2020=vc&id=28140
*/

class CSocket
{
public:
	~CSocket() {
		//std::cout<<"destructor called!"<<std::endl;
		this->close_server_socket();
	}
	CSocket(const CSocket&) = delete;
	CSocket& operator=(const CSocket&) = delete;
	static CSocket& get_instance() {
		static CSocket instance;
		return instance;
	}
private:

	explicit CSocket() {
		s_server = INVALID_SOCKET;
		server_hWnd = NULL;
		ghWnd = NULL;
		gnum = -1;
		m_ip = {};

		if (g_sock_ptr == nullptr)
			g_sock_ptr = new SOCKET;
		else
		{
			delete g_sock_ptr;
			g_sock_ptr = new SOCKET;
		}

		if (g_blue_sock_ptr == nullptr)
			g_blue_sock_ptr = new SOCKET;
		else
		{
			delete g_blue_sock_ptr;
			g_blue_sock_ptr = new SOCKET;
		}
	}

	typedef struct tagIP {
		wchar_t ip[MAX_PATH];
		u_short port;
	} IP;

#define SIO_KEEPALIVE_VALS   _WSAIOW(IOC_VENDOR,4)

	//定義結構及宏
	struct TCP_KEEPALIVE {
		u_long  onoff;
		u_long  keepalivetime;
		u_long  keepaliveinterval;
	};

public:
	IP m_ip = {};
	HWND ghWnd = NULL;
	HWND server_hWnd = NULL;
	int gnum = -1;
	SOCKET s_server = INVALID_SOCKET;

	int WINAPI is_socket_alive(const SOCKET& ss);

	bool WINAPI tcp_initialize();

	bool WINAPI is_server_opened(const bool en = false);

	bool WINAPI is_game_opened(const bool en = false);

	bool WINAPI tcp_open();

	void WINAPI SetSocket(const SOCKET& socket);

	void WINAPI close_server_socket();

	void WINAPI close_game_socket();

	void WINAPI tcp_terminate_all();

	int WINAPI get_local_ip(char* ip);

	int WINAPI wsa_clean();

	bool WINAPI Read(char* recv_buf, const int& len, int& recv_len);

	bool WINAPI Write(char* send_buf, const int& len);

private:
	int WINAPI Recv(SOCKET s, char* buf, int len);

	int WINAPI Send(SOCKET s, char* buf, int len);

	bool WINAPI is_disconnect(const SOCKET& a);

	const bool WINAPI isexist(const SOCKET& a);

	void WINAPI WriteFireWallXP(const LPCTSTR& ruleName, const LPCTSTR& appPath, const bool& NoopIfExist);

	void WINAPI WriteFireWallOverXP(const LPCTSTR& ruleName, const LPCTSTR& appPath, const bool& NoopIfExist);

	void WINAPI WriteFireWall(const LPCTSTR& ruleName, const LPCTSTR& appPath, const bool& NoopIfExist);
};

#endif
