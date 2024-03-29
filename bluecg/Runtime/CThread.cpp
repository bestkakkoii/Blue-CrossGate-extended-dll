#include "CThread.h"
#include <string.h>

#include "CMap.h"

void set_speed(const bool isbattle)
{
	double* speed_ptr = (double*)SUPERSPEED;
	//double* speed2_ptr = (double*)SUPERSPEEDFAKE2;
	CKernel32& ker32 = CKernel32::get_instance();
	if (!isbattle)
	{
		if (speed_ptr != nullptr)
		{
			ker32.My_NtMoveMemory(speed_ptr, &CAsm::global_static_normal_speed_cache, sizeof(double));
		}
	}
	else
	{
		if (speed_ptr != nullptr)
		{
			ker32.My_NtMoveMemory(speed_ptr, &CAsm::global_static_battle_speed_cache, sizeof(double));
		}
	}
}

void CMain::run()
{
	casm = this->create();//實例化 CSocket 和 CAsm
	CSocket& skt = CSocket::get_instance();
	CKernel32& ker32 = CKernel32::get_instance();
	skt.ghWnd = CRuntime::get_window_handle();//獲取自身窗口句柄
	bool bret = true;
	DATA d;
	do
	{
		ker32.My_Sleep(100);
		if (!skt.tcp_initialize()) //將自身加入防火牆規則 WSASocket 初始化
			continue;
		send_log(L"TCP initalization complete!");
		while (!skt.tcp_open()) { if (this->isInterruptionRequested()) { break; } }

		//實例化收包子線程
		receiver = new CReceiver(casm);//收包線程

		receiver->start();
		//casm->announcement(L"Wait for server reply user's information.", 4, 3);
		bret = true;
		do
		{
			ker32.My_Sleep(500);

			// 向伺服器請求 index
			d = {};
			d.identify.cbsize = sizeof(DATA);
			d.identify.msg = DA_MULTIPLEDATA;
			d.identify.gnum = skt.gnum;
			d.additional = DA_CLIENTINFO;
			d.hWnd = (DWORD)skt.ghWnd;
			CRuntime::char2wchar((char*)USERNAMEADDR, d.sz);

			skt.Write((char*)&d, sizeof(DATA));
			if (!skt.is_server_opened())//檢查外掛伺服器
			{
				bret = false;
				break;
			}
		} while (skt.server_hWnd == NULL);

		if (bret)
		{
			//實例化子線程
			sender = new CSender(casm);//發包線程
			awalk = new CAutoWalk(casm);//走路遇敵線程
			acode = new CAntiCode(casm);//防掛機驗證碼線程
			sender->start();
			awalk->start();
			acode->start();
			do
			{
				ker32.My_Sleep(1000);
			} while (!sender->isInterruptionRequested());//阻塞等待 sender 線程結束
		}

		send_log(L"Thread closing...");

		receiver->requestInterruption();
		awalk->requestInterruption();
		acode->requestInterruption();

		delete sender;
		sender = nullptr;
		receiver->wait();
		delete receiver;
		receiver = nullptr;
		awalk->wait();
		delete awalk;
		awalk = nullptr;
		acode->wait();
		delete acode;
		acode = nullptr;

		skt.close_server_socket();//徹底關閉與小幫手的連線
		this->clear(); //清除asm_private
		this->create();//重新實例化 asm_private

		send_log(L"remainding to initalize...");
	} while (!this->isInterruptionRequested());

	skt.tcp_terminate_all();
	this->clear();
}

void CSender::run()
{
	//casm->announcement("Connect to server successfully! You're now ready to go.", 4, 3);
	bool isHookInstalled = false;
	CKernel32& ker32 = CKernel32::get_instance();
	CSocket& skt = CSocket::get_instance();
	while (!this->isInterruptionRequested())
	{
		ker32.My_Sleep(200);

		if (this->cycle_send() && !isHookInstalled)
		{
			//完成所有前置作業，開始安裝鉤子
			casm->HOOK_global_install_hook();
			casm->is_hook_installed = 0xA000000;
			CAsm::asm_ptr->is_hook_installed = casm->is_hook_installed;
			isHookInstalled = true;
		}
		//檢查遊戲伺服器連線狀態 和 外掛伺服器連線狀態
		if (!skt.is_server_opened() || ((skt.server_hWnd != NULL) && !ker32.My_IsWindow(skt.server_hWnd)))//檢查外掛
		{
			break;
		}
	}
	casm->HOOK_global_unhook();

	std::wstring wstr;
	CFormat& format = CFormat::get_instance();
	format.set(FORMATION::THREAD_LOSTFORMAT, wstr);
	casm->announcement(wstr, 24, 3);
}

bool CSender::cycle_send()
{
	CSocket& skt = CSocket::get_instance();
	if (skt.server_hWnd == NULL)
		return false;
	CKernel32& ker32 = CKernel32::get_instance();
	//檢測調試器
	TDebug& dbg = TDebug::get_instance();
	dbg.TPEBChecker();

	//循環發送玩家資訊給伺服器
	//table = casm->get_table_info(skt.gnum, skt.is_socket_alive(*BLUESOCKET));
	//casm->memory_move((DWORD)CAsm::asm_ptr->g_info_ptr.tableinfo, &table, sizeof(TableInfo));
	this->tcp_send_all_data();
	this->update_title();
	//if (!skt.Write((char*)&table, sizeof(table)))
		//return false;
	//定時釋放內存
	if (++free_memory_counter_cache > 6000)
	{
		ker32.FreeProcessMemory();
		free_memory_counter_cache = NULL;
	}

	//未登入
	if (table.status != 2 && table.status != 3)
	{
		if (!disablesocket && *BLUESOCKET == INVALID_SOCKET)
		{
			ker32.My_closesocket(*BLUESOCKET);

			*BLUESOCKET = INVALID_SOCKET;
			disablesocket = true;
		}

		LoginInformation info = {};
		info.identify.cbsize = sizeof(LoginInformation);
		info.identify.msg = DA_USERLOGIN;
		info.identify.gnum = skt.gnum;
		if (!skt.Write((char*)&info, sizeof(info)))
			return false;

		if (casm->is_hook_installed)
		{
			casm->auto_login(skt.ghWnd);
			ker32.My_Sleep(200);
		}

		if (!first)
			first = true;
		return true;
	}

	//戰鬥動作省略
	if (CAsm::asm_ptr->g_info_ptr.aiinfo->disableBattleEffect)
	{
		casm->disable_battle_anime_effect(true);
		*(double*)SUPERSPEED = (double)16.6666666666667;
	}
	else
		casm->disable_battle_anime_effect(false);
	std::wstring wstr;
	CFormat& format = CFormat::get_instance();
	format.set(FORMATION::THREAD_HOTKETFORMAT, wstr);

	//平時
	if (table.status == 2)
	{
		disablesocket = false;
		if (first)//非戰鬥一次
		{
			first = false;
			ker32.My_Sleep(3000);
			if (!casm->is_hd_mode())
			{
				ker32.My_PostMessageW(skt.ghWnd, WM_MOUSEMOVE, 0, MAKELPARAM(320, 240));
				ker32.My_PostMessageW(skt.ghWnd, WM_LBUTTONUP, 0, MAKELPARAM(320, 240));
			}
			else
			{
				ker32.My_PostMessageW(skt.ghWnd, WM_MOUSEMOVE, 0, MAKELPARAM(480, 360));
				ker32.My_PostMessageW(skt.ghWnd, WM_LBUTTONUP, 0, MAKELPARAM(320, 240));
			}
			//ker32->FreeProcessMemory();
			ker32.My_CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CAsm::update_pet_position, NULL, 0, NULL);

			std::wstring tl;
			format.set(FORMATION::THREAD_WELCOMEFORMAT, tl);
			std::wstring wwstr(std::format(tl,
				casm->dateline.yyyy,
				casm->dateline.MM,
				casm->dateline.dd,
				casm->dateline.hh,
				casm->dateline.mm,
				casm->dateline.ss,
				casm->dateline.mmm));
			casm->announcement(wwstr, 35, 3);
			send_log(wwstr);
		}

		set_speed(false);

		//回復戰鬥緩存
		tickcount_cache = NULL;
		tickcount_cache_tmp = NULL;
		local_round_cache = NULL;
		if (!battlefirst)
		{
			battlefirst = true;
		}

		//過圖高切
		if (CAsm::asm_ptr->g_info_ptr.aiinfo->disableDrawEffect)
			casm->disable_draw_effect(true);
		else
			casm->disable_draw_effect(false);

		casm->auto_heal(&table.status);
		casm->auto_throw_item(&table.status);
	}
	//戰鬥
	else if (table.status == 3)
	{
		set_speed(true);

		if (battlefirst)
		{
			battlefirst = false;
			tickcount_cache = (long long)ker32.My_GetTickCount64();
			totalround_cache++;
		}
		int* addr = (int*)MAKEADDR(0x5, 0x9, 0x8, 0x8, 0xa, 0xC);//0x05988AC
		if (local_round_cache != *addr + 1)
		{
			totalduration_cache += tickcount_cache_tmp;
			local_round_cache = *addr + 1;
		}
		if (tickcount_cache > 0)
			tickcount_cache_tmp = (static_cast<long long>(ker32.My_GetTickCount64()) - tickcount_cache);

		BattleInfo btinfo = {};
		btinfo.identify.cbsize = sizeof(BattleInfo);
		btinfo.identify.msg = DA_BATTLEINFO;
		btinfo.identify.gnum = skt.gnum;
		btinfo.total = totalround_cache;
		btinfo.tickcount = tickcount_cache_tmp;
		btinfo.totalduration = totalduration_cache;
		casm->get_battle_info(btinfo);
		*CAsm::asm_ptr->g_info_ptr.battleinfo = btinfo;
	}

	return true;
}

void CSender::tcp_send_all_data()
{
	CSocket& skt = CSocket::get_instance();
	std::unique_lock lock(m_mutex);
	ClientInfo* Client = new ClientInfo;
	Client->identify.cbsize = sizeof(ClientInfo);
	Client->identify.msg = DA_ClientAllInfo;
	Client->identify.gnum = skt.gnum;
	//發送技能資訊
	SkillInfo* skillinfos = new SkillInfo;
	*skillinfos = {};
	casm->get_char_skill_info(*skillinfos);
	skillinfos->identify.gnum = skt.gnum;
	//send_len = skt.Write((char*)skillinfos, sizeof(SkillInfo));
	*casm->g_info_ptr.skillinfo = *skillinfos;
	Client->skillinfo = *skillinfos;
	delete skillinfos;
	skillinfos = nullptr;

	//發送道具資訊
	ItemInfo* iteminfo = new ItemInfo;
	casm->get_all_item_info(*iteminfo);
	iteminfo->identify.gnum = skt.gnum;
	*casm->g_info_ptr.iteminfo = *iteminfo;
	Client->iteminfo = *iteminfo;
	delete iteminfo;
	iteminfo = nullptr;

	//發送人物資訊
	CharInfos* charinfos = new CharInfos;
	casm->get_char_info(charinfos->charinfo);
	charinfos->identify.cbsize = sizeof(CharInfos);
	charinfos->identify.gnum = skt.gnum;
	charinfos->identify.msg = DA_CHARINFO;
	*casm->g_info_ptr.charinfo = charinfos->charinfo;
	Client->charinfo = charinfos->charinfo;
	delete charinfos;
	charinfos = nullptr;

	//發送寵物資訊
	PetInfos* pinfos = new PetInfos;
	pinfos->identify.cbsize = sizeof(PetInfos);
	pinfos->identify.msg = DA_PETINFO;
	pinfos->identify.gnum = skt.gnum;
	for (int i = 0; i < 5; i++)
	{
		casm->get_pet_info(i, pinfos->petinfo[i]);
		*casm->g_info_ptr.petinfo[i] = pinfos->petinfo[i];
		Client->petinfo[i] = pinfos->petinfo[i];
	}
	delete pinfos;
	pinfos = nullptr;

	Client->battleinfo = *(BattleInfo*)CAsm::asm_ptr->g_info_ptr.battleinfo;

	//循環發送玩家資訊給伺服器
	table = casm->get_table_info(skt.gnum, skt.is_socket_alive(*BLUESOCKET));
	Client->tableinfo = table;

	skt.Write((char*)Client, sizeof(ClientInfo));
	delete Client;
	Client = nullptr;
}

void CReceiver::run()
{
	//CKernel32& ker32 = CKernel32::get_instance();
	CSocket& skt = CSocket::get_instance();
	casm->login = {};
	autobattleswitcher_cache = REMOTE_AIOFF;
	auto_walk_cache = false;

	global_static_battle_speed_cache = NULL;

	while (!this->isInterruptionRequested())
	{
		char* recv_buf = new char[65535];
		memset(recv_buf, 0, 65535);
		int recv_len = NULL;

		if (skt.Read(recv_buf, 65535, recv_len))
		{
			this->tcp_receiver(recv_buf);
		}
		delete[] recv_buf;
		recv_buf = nullptr;
	}

	//test(L"-1");
}

void CReceiver::tcp_receiver(const char* recv_buf)
{
	CKernel32& ker32 = CKernel32::get_instance();
	CSocket& skt = CSocket::get_instance();

	SocketCheck sc = {};
	ker32.My_NtMoveMemory(&sc, (void*)recv_buf, sizeof(SocketCheck));
	if ((sc.cbsize == sizeof(Ann)) && (sc.msg == ACT_ANNOUNCE) && (sc.gnum == skt.gnum))
	{
		Ann an = {};
		ker32.My_NtMoveMemory(&an, recv_buf, sizeof(Ann));
		casm->announcement(an.sz, an.color, an.size);
	}
	else if ((sc.cbsize == sizeof(ScriptCommand)) && (sc.msg == ACT_SCRIPT) && (sc.gnum == skt.gnum))
	{
		casm->script_thread((char*)recv_buf);
	}
	else if ((sc.cbsize == sizeof(DATA)) && (sc.msg == DA_MULTIPLEDATA))
	{
		DATA data = {};
		ker32.My_NtMoveMemory(&data, recv_buf, sizeof(DATA));

		//傳送窗口句柄給伺服器
		//if (data.additional == DA_HWND)
		//{
			//data.identify.cbsize = sizeof(DATA);
			//data.identify.msg = DA_MULTIPLEDATA;
			//data.identify.gnum = skt.gnum;
			//data.additional = DA_HWND;
			//data.hWnd = skt.ghWnd;
			//send_len = skt.Write((char*)&data, sizeof(DATA));
		//}
		//獲取從伺服器得到的index和窗口句柄
		if (data.additional == DA_INDEX)
		{
			skt.gnum = data.identify.gnum;//接收伺服器配發的index
			skt.server_hWnd = (HWND)data.hWnd;//伺服器端句柄

			//發送遊戲客戶端給句柄伺服器
			data.identify.cbsize = sizeof(DATA);
			data.identify.msg = DA_MULTIPLEDATA;
			data.identify.gnum = skt.gnum;
			data.additional = DA_HWND;
			data.hWnd = (DWORD)skt.ghWnd;//客戶端句柄
			skt.Write((char*)&data, sizeof(DATA));
		}
		//收到伺服器要求解除多開
		else if ((data.additional == ACT_ANTIMUTEX) && (data.identify.gnum == skt.gnum))
		{
			//向伺服器請求datetime
			data.identify.cbsize = sizeof(DATA);
			data.identify.msg = DA_MULTIPLEDATA;
			data.identify.gnum = skt.gnum;
			data.additional = DA_DATETIME;
			data.hWnd = (DWORD)skt.ghWnd;
			skt.Write((char*)&data, sizeof(DATA));
			try
			{
				ker32.CloseMutex();
			}
			catch (...)
			{
			}
		}
		//}
		//收到伺服器要求回傳技能資訊
		//else if (data.additional == DA_SKILLINFO && data.identify.gnum == skt.gnum)
		//{
			////發送技能資訊
			//SkillInfo* skillinfos = new SkillInfo;
			//*skillinfos = {};
			//casm->get_char_skill_info(*skillinfos);
			//skillinfos->identify.gnum = skt.gnum;
			//send_len = skt.Write((char*)skillinfos, sizeof(SkillInfo));
			//casm->memory_move((DWORD)casm->g_info_ptr.skillinfo, skillinfos, sizeof(SkillInfo));
			//delete skillinfos;
			//skillinfos = nullptr;
		//}
		//收到伺服器要求回傳道具資訊
		//else if (data.additional == DA_ITEMINFO && data.identify.gnum == skt.gnum)
		//{
			////發送道具資訊
			//ItemInfo iteminfo = {};
			//casm->get_all_item_info(iteminfo);
			//iteminfo.identify.gnum = skt.gnum;
			//send_len = skt.Write((char*)&iteminfo, sizeof(ItemInfo));
			//casm->memory_move((DWORD)casm->g_info_ptr.iteminfo, &iteminfo, sizeof(ItemInfo));
		//}
		//收到伺服器要求回傳人物資訊
		//else if (data.additional == DA_CHARINFO && data.identify.gnum == skt.gnum)
		//{
			////發送人物資訊
			//CharInfos charinfos = {};
			//casm->get_char_info(charinfos.charinfo);
			//charinfos.identify.cbsize = sizeof(CharInfos);
			//charinfos.identify.gnum = skt.gnum;
			//charinfos.identify.msg = DA_CHARINFO;
			//send_len = skt.Write((char*)&charinfos, sizeof(CharInfos));
			//casm->memory_move((DWORD)casm->g_info_ptr.charinfo, &charinfos.charinfo, sizeof(CharInfo));
		//}
		//收到伺服器要求回傳寵物資訊
		//else if (data.additional == DA_PETINFO && data.identify.gnum == skt.gnum)
		//{
			////發送寵物資訊
			//PetInfos pinfos = {};
			//pinfos.identify.cbsize = sizeof(PetInfos);
			//pinfos.identify.msg = DA_PETINFO;
			//pinfos.identify.gnum = skt.gnum;
			//for (int i = 0; i < 5; i++)
			//{
			//	casm->get_pet_info(i, pinfos.petinfo[i]);
			//	casm->memory_move((DWORD)casm->g_info_ptr.petinfo[i], &pinfos.petinfo[i], sizeof(PetInfo));
			//}
			//send_len = skt.Write((char*)&pinfos, sizeof(PetInfos));
		//}
		//伺服器請求關閉客戶端
		else if (data.additional == ACT_EXIT)
		{
			exit_process();
		}
	}
	//接收來自伺服器的datetime資訊
	else if ((sc.cbsize == sizeof(DateTime)) && (sc.msg == DA_DATETIME))
	{
		ker32.My_NtMoveMemory(&casm->dateline, recv_buf, sizeof(DateTime));
	}
	else if ((sc.cbsize == sizeof(LoginInformation)) && (sc.msg == DA_USERLOGIN) && (sc.gnum == skt.gnum))
	{
		LoginInformation logininfo = {};
		ker32.My_NtMoveMemory(&logininfo, recv_buf, sizeof(LoginInformation));
		if ((wcslen(logininfo.username) >= 6) && (wcslen(logininfo.password) >= 6))
		{
			casm->login = logininfo;

			//_snwprintf_s(casm->login.username, 32, _TRUNCATE, L"%s\0", logininfo.username);
			//_snwprintf_s(casm->login.password, 32, _TRUNCATE, L"%s\0", logininfo.password);
			//casm->login.autoLoginCheckState = logininfo.autoLoginCheckState;
			//casm->login.server = logininfo.server;
			//casm->login.subserver = logininfo.subserver;
			//casm->login.pos = logininfo.pos;
		}
		else
		{
			ZeroMemory(casm->login.username, sizeof(casm->login.username));
			ZeroMemory(casm->login.password, sizeof(casm->login.password));
			casm->login.autoLoginCheckState = 0;
			casm->login.server = -1;
			casm->login.subserver = -1;
			casm->login.pos = -1;
		}
	}
	//接收來自伺服器的AI資訊
	else if ((sc.cbsize == sizeof(AI)) && (sc.msg == DA_AIINFO) && (sc.gnum == skt.gnum))
	{
		AI ai = {};
		ker32.My_NtMoveMemory(&ai, recv_buf, sizeof(AI));
		*casm->g_info_ptr.aiinfo = ai;
		*CAsm::asm_ptr->g_info_ptr.aiinfo = ai;

		if (ai.autoBattleCheckState != autobattleswitcher_cache)
		{
			autobattleswitcher_cache = ai.autoBattleCheckState;
			if (ai.autoBattleCheckState == REMOTE_AION)
				CAsm::asm_ptr->is_auto_battle = CAsm::asm_ptr->AIOFF;
			else if (ai.autoBattleCheckState == REMOTE_AIOFF)
				CAsm::asm_ptr->is_auto_battle = CAsm::asm_ptr->AION;
			casm->hook_hotkey_extend(0xb8);
		}

		if (ai.auto_walk != auto_walk_cache)
		{
			auto_walk_cache = ai.auto_walk;

			CAsm::asm_ptr->is_auto_walk_cache = !ai.auto_walk;

			casm->hook_hotkey_extend(0x80);
		}

		if (ai.battleSpeed != global_static_battle_speed_cache)
		{
			global_static_battle_speed_cache = 16.6666666666667 - ai.battleSpeed;
			CAsm::global_static_battle_speed_cache = 16.6666666666667 - ai.battleSpeed;
		}

		if (ai.normalSpeed != normalsave)
		{
			normalsave = 16.6666666666667 - ai.normalSpeed;
			CAsm::global_static_normal_speed_cache = 16.6666666666667 - ai.normalSpeed;
		}
	}
}

//constexpr std::uint64_t basis = 0xCBF29CE484222325ull;
//constexpr std::uint64_t prime = 0x100000001B3ull;
//std::uint64_t h_(char const* str)
//{
//	std::uint64_t ret{ basis };
//
//	while (*str) {
//		ret ^= *str;
//		ret *= prime;
//		str++;
//	}
//
//	return ret;
//}
//constexpr std::uint64_t h_compile_time(char const* str, std::uint64_t last_value = basis)
//{
//	return *str ? h_compile_time(str + 1, (*str ^ last_value) * prime) : last_value;
//}
//constexpr unsigned long long operator"" _h(char const* p, size_t)
//{
//	return h_compile_time(p);
//}