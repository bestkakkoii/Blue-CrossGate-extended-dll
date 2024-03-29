#pragma once
#ifndef CTHREAD_H
#define CTHREAD_H
#include "CSocket.h"
#include "CAsm.h"
#include "CRuntime.h"
#include "../TDebug/TDebuger.h"
#include "CKernel32.h"
#include <thread>
#include <condition_variable>
#include <mutex>

#define test(str) MessageBox(0, TEXT(str), L"", 0)

#pragma region CThread

class CThread
{
public:
	explicit CThread(const bool& detaching) : is_detach(detaching)
	{
	}
	virtual ~CThread()
	{
		//if (t != nullptr)
		//{
		//	delete t; t = nullptr;
		//}
	}

	virtual void WINAPI run()
	{
	}
	std::mutex m_mutex;
private:

	std::jthread t;
	std::stop_token stok;
	bool isInterruptionRequest = false;

	const bool is_detach;

	DWORD WINAPI prerun()
	{
		LPVOID lpvData;
		// Initialize the TLS index for this thread.
		lpvData = (LPVOID)LocalAlloc(LPTR, 256);
		if (TlsSetValue(dwTlsIndex, lpvData))
		{
			run();
			set();
		}

		// Release the dynamic memory before the thread returns.
		lpvData = TlsGetValue(dwTlsIndex);
		if (lpvData != 0)
			LocalFree((HLOCAL)lpvData);
		return 0;
	}

	void set()
	{
		m_mutex.lock();
		isInterruptionRequest = true;
		m_mutex.unlock();
	}
	int get()
	{
		m_mutex.lock();
		int nret = isInterruptionRequest;
		m_mutex.unlock();
		return nret;
	}

public:
	bool WINAPI isInterruptionRequested()
	{
		bool ret = false;
		//if (t != nullptr)
		//{
		try
		{
			stok = t.get_stop_token();
			ret = stok.stop_requested();
			int tmp = get();
			if (tmp)
				ret = tmp;
		}
		catch (...)
		{
		}
		//}
		return ret;
	}
	void WINAPI requestInterruption()
	{
		//if (t != nullptr)
		//{
		try
		{
			set();
			t.request_stop();
		}
		catch (...)
		{
		}
		//}
	}
	void WINAPI wait()
	{
		try
		{
			if (t.joinable())
			{
				t.join();
				//delete t; t = nullptr;
			}
		}
		catch (...)
		{
		}
	}

public:
	void WINAPI start()
	{
		t = std::jthread(&CThread::prerun, this);
		if (is_detach)
			t.detach();
	}
	static DWORD dwTlsIndex;
	/// <summary>
	/// ///////////////////////////////////////////////////////////////
	/// </summary>
private:
	CAsm* asm_private = nullptr;
public:

	CAsm* WINAPI create()
	{
		if (asm_private != nullptr)
		{
			delete asm_private;
			asm_private = nullptr;
		}
		asm_private = new CAsm;
		return asm_private;
	}

	void WINAPI clear()
	{
		if (g_blue_sock_ptr != nullptr)
			*g_blue_sock_ptr = *BLUESOCKET;
		if (asm_private != nullptr)
		{
			delete asm_private;
			asm_private = nullptr;
		}
	}
};
#pragma endregion

class CAutoWalk : public CThread
{
public:
	explicit CAutoWalk(CAsm* asmptr, const bool& is_detach = true) : CThread(is_detach)
	{
		casm = asmptr;
		pai = CAsm::asm_ptr->g_info_ptr.aiinfo;
	}

	//virtual ~CAutoWalk()
	//{
	//}

private:
	DWORD is_auto_walk = NULL;

	CMap Map;

	CAsm* casm = nullptr;

	AI* pai = nullptr;

	AI ai = {};

	int flag = false;

	int x = 0, y = 0;

	int predir = NULL;

	int i = NULL;

	int status = NULL;

	void WINAPI run() override
	{
		CKernel32& ker32 = CKernel32::get_instance();

		while (!this->isInterruptionRequested())
		{
			ai = *pai;
			is_auto_walk = CAsm::asm_ptr->is_auto_walk_cache;
			status = CAsm::asm_ptr->g_info_ptr.tableinfo->status;
			if (!is_auto_walk)
			{
				flag = false;
				for (i = 0; i < 2; i++)
				{
					ker32.My_Sleep(100);
					if (this->isInterruptionRequested())//檢查外掛
					{
						return;
					}
				}
				continue;
			}

			if (!flag)
			{
				//if (Map->AddNewMap())
				flag = true;
				casm->get_char_xy(x, y);
			}
			if (status != 2)
			{
				ker32.My_Sleep(100);
				continue;
			}
			ker32.My_Sleep(ai.auto_walkFreq);

			casm->auto_walk(Map, ai.auto_walkDirection, ai.auto_walkLen, x, y, predir);
			if (ai.auto_walkFreq <= 0 || ai.auto_walkFreq > 3000)
				ai.auto_walkFreq = 1000;
		}
	}
};

class CAntiCode : public CThread
{
public:
	explicit CAntiCode(CAsm* asmptr, const bool& is_detach = true) : CThread(is_detach)
	{
		casm = asmptr;
	}

	//virtual ~CAntiCode()
	//{
	//}

private:

	int anti_code_counter_cache = 5;
	std::wstring lastoutput_testcode = L"";

	CAsm* casm = nullptr;
	int i = NULL;
	int status = NULL;
	void WINAPI run() override
	{
		CKernel32& ker32 = CKernel32::get_instance();
		while (!this->isInterruptionRequested())
		{
			for (i = 0; i < 20; i++)
			{
				ker32.My_Sleep(10);
				//檢查遊戲伺服器連線狀態 和 外掛伺服器連線狀態
				if (this->isInterruptionRequested())//檢查外掛
				{
					return;
				}
			}

			status = casm->g_info_ptr.tableinfo->status;
			if (status != 2 && status != 3)
				continue;
			casm->anti_test_code(anti_code_counter_cache, lastoutput_testcode);
		}
	}
};

class CSender : public CThread
{
public:
	explicit CSender(CAsm* asmptr, const bool& is_detach = true) : CThread(is_detach)
	{
		casm = asmptr;
	}

	virtual ~CSender()
	{
	}

private:
	CAsm* casm = nullptr;

	bool first = true;

	int send_len = NULL;

	int free_memory_counter_cache = NULL;

	int n = NULL;
	TableInfo table = {};

	int local_round_cache = NULL;

	long long tickcount_cache = NULL;

	long long tickcount_cache_tmp = NULL;

	int totalround_cache = NULL;

	long long totalduration_cache = NULL;

	bool battlefirst = true;

	bool disablesocket = false;

	void WINAPI run() override;

	bool WINAPI cycle_send();

	void WINAPI tcp_send_all_data();

	void WINAPI update_title()
	{
		CKernel32& ker32 = CKernel32::get_instance();
		CSocket& skt = CSocket::get_instance();
		//更新視窗標題
		constexpr WCHAR sblue[MAX_PATH] = { TEXT("Blue\0") };
		constexpr WCHAR slv[MAX_PATH] = { TEXT("Lv\0") };
		constexpr WCHAR shp[MAX_PATH] = { TEXT("HP\0") };
		constexpr WCHAR smp[MAX_PATH] = { TEXT("MP\0") };
		constexpr WCHAR sip[MAX_PATH] = { TEXT("IP\0") };
		//char ip[MAX_PATH] = {};
		//socket->get_local_ip(ip);
		//WCHAR* wip = rt->char2wchar(ip);
		WCHAR wname[MAX_PATH] = {};
		CRuntime::char2wchar(table.name, wname);
		std::wstring det;
		CFormat& format = CFormat::get_instance();
		format.set(FORMATION::THREAD_TITLEFORMAT, det);
		std::wstring title(std::format(det,
			sblue, skt.gnum,
			slv, table.level, wname,
			shp, table.hp, table.maxhp,
			smp, table.mp, table.maxmp,
			sip, skt.m_ip.ip, skt.m_ip.port));
		ker32.My_SetWindowTextW(skt.ghWnd, title.c_str());
	}
};

class CReceiver : public CThread
{
public:
	explicit CReceiver(CAsm* asmptr, const bool& is_detach = false) : CThread(is_detach)
	{
		casm = asmptr;
	}

	virtual ~CReceiver()
	{
	}

private:

	CAsm* casm = nullptr;

	long long autobattleswitcher_cache = REMOTE_AIOFF;

	int auto_walk_cache = false;

	double global_static_battle_speed_cache = NULL;

	double normalsave = NULL;

	void WINAPI run() override;

	void WINAPI tcp_receiver(const char* recv_buf);
};

class CMain : public CThread
{
public:
	explicit CMain(const bool& is_detach = true) : CThread(is_detach)
	{
	}

	virtual ~CMain()
	{
	}

	CAsm* casm = nullptr;

private:

	//實例化四個子線程
	CSender* sender = nullptr;//發包線程

	CReceiver* receiver = nullptr;//收包線程

	CAutoWalk* awalk = nullptr;//走路遇敵線程

	CAntiCode* acode = nullptr;//防掛機驗證碼線程

	void WINAPI run() override;
};

#endif
//void sub(CSocket* socket);
//void cycle_send(CSocket* socket);