#include "CAsm.h"
#include "CKernel32.h"
#include "../TDebug/TMessage.h"
#include "../TDebug/TDebuger.h"
#include <format>

#include "CRuntime.h"

#include <regex>
#include <algorithm>
#include <thread>

constexpr DWORD JMP = 0xE9;
constexpr DWORD CALL = 0xE8;
constexpr DWORD NOP = 0x90;
constexpr DWORD JE = 0x74;

std::mutex global_auto_walk_mutex;//防止補血/丟物品時走路鎖

//DWORD CAsm::xcodeaddr = 0;

double CAsm::global_static_battle_speed_cache = 16.6666666666667;
double CAsm::global_static_normal_speed_cache = 16.6666666666667;

DWORD CAsm::m_pfnNewXcodeDis = NULL;

CAsm* CAsm::asm_ptr = nullptr;

MoveXY g_movexy = {};

DWORD m_pfnOrigWalk = NULL;

const DWORD BLUECDECL makeaddress(HideValue a, HideValue b, HideValue c, HideValue d, HideValue e, HideValue f)
{
	return ((a.get() * 0x100000) + (b.get() * 0x10000) + (c.get() * 0x1000) + (d.get() * 0x100) + (e.get() * 0x10) + (f.get() * 0x1));
}

__forceinline const int get_table_value(DWORD dw)
{
	typedef int(__cdecl* get_table)();
	const get_table table = (get_table)MAKEADDR(0x4, 0x0, 0x1, 0xE, 0xA, 0x0);//0x0401EA0;
	_asm
	{
		mov ecx, dw
		call table
	}
}

__forceinline constexpr int get_percent(const int& min, const int& max)
{
	return (max > 0) ? (min * 100 / max) : 0;
}

//根據 std::vector<unsigned int> 返回最小值
__forceinline constexpr unsigned int get_lowest(const std::vector<unsigned int>& v, int& index)
{
	unsigned int min = v[0];
	index = 0;
	unsigned int size = v.size();
	for (unsigned int i = 1; i < size; i++)
	{
		if (v[i] < min)
		{
			min = v[i];
			index = i;
		}
	}
	return min;
}

//根據 std::vector<unsigned int> 返回最大值
__forceinline constexpr unsigned int get_highest(const std::vector<unsigned int>& v, int& index)
{
	unsigned int max = v[0];
	index = 0;
	unsigned int size = v.size();
	for (unsigned int i = 1; i < size; i++)
	{
		if (v[i] > max)
		{
			max = v[i];
			index = (int)i;
		}
	}
	return max;
}

#pragma region initaialize
CAsm::CAsm()
{
	this->asm_ptr = this;

	this->is_auto_battle = CAsm::asm_ptr->AIOFF;
	this->is_auto_login = false;
	this->is_auto_walk = false;

	this->battle_screen_type_cache = NULL;

	this->g_info_ptr.battleinfo = new BattleInfo;
	this->g_info_ptr.iteminfo = new ItemInfo;
	this->g_info_ptr.skillinfo = new SkillInfo;
	this->g_info_ptr.charinfo = new CharInfo;
	for (int i = 0; i < 5; i++)
		this->g_info_ptr.petinfo[i] = new PetInfo;
	this->g_info_ptr.tableinfo = new TableInfo;
	this->g_info_ptr.aiinfo = new AI;
}
CAsm::~CAsm()
{
	delete this->g_info_ptr.battleinfo;
	delete this->g_info_ptr.iteminfo;
	delete this->g_info_ptr.skillinfo;
	delete this->g_info_ptr.charinfo;
	for (int i = 0; i < 5; i++)
		delete this->g_info_ptr.petinfo[i];
	delete this->g_info_ptr.tableinfo;
	delete this->g_info_ptr.aiinfo;

	this->g_info_ptr.battleinfo = nullptr;
	this->g_info_ptr.iteminfo = nullptr;
	this->g_info_ptr.skillinfo = nullptr;
	this->g_info_ptr.charinfo = nullptr;
	for (int i = 0; i < 5; i++)
		this->g_info_ptr.petinfo[i] = nullptr;
	this->g_info_ptr.tableinfo = nullptr;
	this->g_info_ptr.aiinfo = nullptr;
}
#pragma endregion

#pragma region HOOK

void CAsm::memory_move(const DWORD& dis, const void* source, const size_t& size)
{
	bool bret = false;
	DWORD dwOldProtect = 0;
	CKernel32& ker32 = CKernel32::get_instance();
	HANDLE hProcess = ker32.My_GetCurrentProcess();
	bret = ker32.ElevatePrivileges();
	__try
	{
		ker32.My_ProtectVirtualMemory(hProcess, (void*)dis, size, PAGE_EXECUTE_READWRITE, &dwOldProtect);
		ker32.My_NtMoveMemory((void*)dis, (void*)source, size);
		ker32.My_ProtectVirtualMemory(hProcess, (void*)dis, size, dwOldProtect, &dwOldProtect);
		//ker32->My_FlushInstructionCache(hProcess, (void*)dis, size);
	}
	__finally
	{
		return;
	}
}

#pragma region installHook_overloading
void CAsm::install_hook(ptrasm pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest)
{
	DWORD hookfunAddr = 0;
	GetMemberFuncAddr_VC6(hookfunAddr, pfnHookFunc);
	DWORD dwOffset = (hookfunAddr + offest) - (DWORD)bOri - nsize;
	memory_move((DWORD)&bNew[nsize - 4], &dwOffset, sizeof(dwOffset));
	memory_move((DWORD)bOld, (void*)bOri, nsize);
	memory_move((DWORD)bOri, bNew, nsize);
}

void CAsm::install_hook(ptrasmnoreturn pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest)
{
	DWORD hookfunAddr = 0;
	GetMemberFuncAddr_VC6(hookfunAddr, pfnHookFunc);
	DWORD dwOffset = (hookfunAddr + offest) - (DWORD)bOri - nsize;
	memory_move((DWORD)&bNew[nsize - 4], &dwOffset, sizeof(dwOffset));
	memory_move((DWORD)bOld, (void*)bOri, nsize);
	memory_move((DWORD)bOri, bNew, nsize);
}

void CAsm::install_hook(ptrasmwitharg pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest)
{
	DWORD hookfunAddr = 0;
	GetMemberFuncAddr_VC6(hookfunAddr, pfnHookFunc);
	DWORD dwOffset = (hookfunAddr + offest) - (DWORD)bOri - nsize;
	memory_move((DWORD)&bNew[nsize - 4], &dwOffset, sizeof(dwOffset));
	memory_move((DWORD)bOld, (void*)bOri, nsize);
	memory_move((DWORD)bOri, bNew, nsize);
}

void CAsm::install_hook(fackmessagebox pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest)
{
	DWORD hookfunAddr = 0;
	GetMemberFuncAddr_VC6(hookfunAddr, pfnHookFunc);
	DWORD dwOffset = (hookfunAddr + offest) - (DWORD)bOri - nsize;
	memory_move((DWORD)&bNew[nsize - 4], &dwOffset, sizeof(dwOffset));
	memory_move((DWORD)bOld, (void*)bOri, nsize);
	memory_move((DWORD)bOri, bNew, nsize);
}

void CAsm::install_hook(ptra pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest)
{
	DWORD hookfunAddr = 0;
	GetMemberFuncAddr_VC6(hookfunAddr, pfnHookFunc);
	DWORD dwOffset = (hookfunAddr + offest) - (DWORD)bOri - nsize;
	memory_move((DWORD)&bNew[nsize - 4], &dwOffset, sizeof(dwOffset));
	memory_move((DWORD)bOld, (void*)bOri, nsize);
	memory_move((DWORD)bOri, bNew, nsize);
}
#pragma endregion

void CAsm::unhook(const DWORD& ori, const BYTE* oldBytes, const SIZE_T& size)
{
	//bool bret = false;

	//DWORD dwOldProtect = 0;
	//HANDLE hProcess = ker32.My_GetCurrentProcess();
	//bret = ker32.ElevatePrivileges();
	//ker32.My_ProtectVirtualMemory(hProcess, (void*)ori, size, PAGE_EXECUTE_READWRITE, &dwOldProtect);
	memory_move(ori, oldBytes, size);
	//ker32.My_ProtectVirtualMemory(hProcess, (void*)ori, size, dwOldProtect, &dwOldProtect);
	//.My_FlushInstructionCache(hProcess, (void*)ori, size);
}

void CAsm::HOOK_global_install_hook()
{
	//HOOK走路
	BYTE  m_bNewWalkBytesX[5] = { JMP };
	m_pfnOrigWalk = (DWORD)MAKEADDR(0x4, 0x6, 0x4, 0x4, 0xA, 0x8);//0x04644A8;
	install_hook(&CAsm::hook_movement, m_pfnOrigWalk, m_bOldWalkBytesX, m_bNewWalkBytesX, sizeof(m_bOldWalkBytesX), 0);

	//HOOK平時血量
	BYTE  m_bNewHpOutBytes[5] = { CALL };
	m_pfnOrigHpOUT = (DWORD)MAKEADDR(0x4, 0x6, 0x9, 0x9, 0x7, 0x4);//0x00469974;
	install_hook(&CAsm::hook_hp_mp_in_normal, m_pfnOrigHpOUT, m_bOldHpBytesOUT, m_bNewHpOutBytes, sizeof(m_bOldHpBytesOUT));

	//HOOK戰鬥血量
	BYTE  m_bNewHpInBytes[5] = { CALL };
	m_pfnOrigHpIN = (DWORD)MAKEADDR(0x4, 0x6, 0xA, 0x6, 0xD, 0xE);//0x0046A6DE;
	install_hook(&CAsm::hook_hp_mp_in_battle, m_pfnOrigHpIN, m_bOldHpBytesIN, m_bNewHpInBytes, sizeof(m_bOldHpBytesIN));

	//HOOK遊戲關閉消息框
	BYTE  m_bNewMessageBoxBytes[12 + 5] = { NOP, NOP, NOP, NOP , NOP ,NOP ,NOP ,NOP ,NOP ,NOP ,NOP ,NOP , CALL };
	m_pfnOrigMessageBox = (DWORD)MAKEADDR(0x4, 0x6, 0x0, 0x0, 0x8, 0x1);//0x046008D;
	install_hook(&exit_process, m_pfnOrigMessageBox, m_bOldMessageBoxBytes, m_bNewMessageBoxBytes, sizeof(m_bOldMessageBoxBytes));//00460081

	//HOOK人物戰鬥面板
	BYTE  m_bNewCharBytes[5] = { CALL };
	m_pfnOrigCreateCharScreen = (DWORD)MAKEADDR(0x4, 0x9, 0x0, 0xE, 0xC, 0x3);//0x0490EC3;
	install_hook(&CAsm::hook_create_battle_screen, m_pfnOrigCreateCharScreen, m_bOldCreateCharScreenBytes, m_bNewCharBytes, sizeof(m_bOldCreateCharScreenBytes));

	//HOOK寵物戰鬥面板
	BYTE  m_bNewPetBytes[5] = { CALL };
	m_pfnOrigCreatePetScreen = (DWORD)MAKEADDR(0x4, 0x9, 0x1, 0x2, 0x7, 0x4);//0x0491274;
	install_hook(&CAsm::hook_create_battle_screen, m_pfnOrigCreatePetScreen, m_bOldCreatePetScreenBytes, m_bNewPetBytes, sizeof(m_bOldCreatePetScreenBytes));

	//HOOK騎寵戰鬥面板
	BYTE  m_bNewRideBytes[5] = { CALL };
	m_pfnOrigCreateRideScreen = (DWORD)MAKEADDR(0x4, 0x9, 0x0, 0xD, 0x1, 0x1);//0x0490D11;
	install_hook(&CAsm::hook_create_battle_screen, m_pfnOrigCreateRideScreen, m_bOldCreateRideScreenBytes, m_bNewRideBytes, sizeof(m_bOldCreateRideScreenBytes));

	//HOOK快捷鍵
	BYTE  m_bNewHotKeyBytes[7] = { NOP, NOP, CALL };
	m_pfnOrigHotKey = (DWORD)MAKEADDR(0x4, 0x4, 0x6, 0x7, 0xF, 0x7);//0x04467F7;
	install_hook(&CAsm::hook_hotkey, m_pfnOrigHotKey, m_bOldHotKeyBytes, m_bNewHotKeyBytes, sizeof(m_bOldHotKeyBytes));

	//CKernel32 ker32;
	//BYTE m_bNewXCodeByte[5] = { JMP };
	//HMODULE hmod = GetModuleHandle(TEXT("sfc32.dll"));
	//char xed[MAX_PATH] = { "XEDParseAssemble\0" };
	//DWORD XEDParseAssembleProc = ker32.GetFunAddr((DWORD*)hmod, xed);
	//DWORD m_pfnXcodeFun = XEDParseAssembleProc + 0xA3E3;
	//m_pfnNewXcodeDis = XEDParseAssembleProc + 0xA28E;
	//install_hook((PROC)CAsm::asm_ptr->on_HookXcode, m_pfnXcodeFun, m_bOldXcodeBytes, m_bNewXCodeByte, sizeof(m_bOldXcodeBytes));

	//戰鬥到計時秒數
	constexpr BYTE t[5] = { 0x5, 0xB8, 0x82, 0x1, 0x0 };
	memory_move(MAKEADDR(0x4, 0x3, 0x8, 0x1, 0xC, 0x1)/*0x04381C1*/, (void*)t, sizeof(t));

	//BYTE d[2] = { JE, 0x4B };
	//memory_move(0x04043F9, (void*)d, sizeof(d));//加速

	//防加速回復
	constexpr BYTE tt7[2] = { 0xEB, 0x38 };
	memory_move(MAKEADDR(0x4, 0x0, 0x4, 0x4, 0x0, 0xC)/*0x040440C*/, (void*)tt7, sizeof(tt7));

	//防加速回復2
	constexpr BYTE tt8[5] = { 0x83, 0xF8, 0x1, NOP, NOP };
	memory_move(MAKEADDR(0x4, 0x0, 0x4, 0x3, 0xB, 0xD)/*0x04043BD*/, (void*)tt8, sizeof(tt8));

	//DWORD* ntdllModule = (DWORD*)ker32.My_GetModuleHandleW(L"Ntdll.dll");
	//DWORD pfnRtlCreateUserThread = ker32.GetFunAddr(ntdllModule, "RtlCreateUserThread");
	//BYTE antihook0[8] = { 0xB8, 0x0, 0x0, 0x0, 0x0, 0xC2, 0x28, 0x0 };
	//memory_move(pfnRtlCreateUserThread, (void*)antihook0, sizeof(antihook0));

	//DWORD pfnNtOpenProcess = ker32.GetFunAddr(ntdllModule, "NtOpenProcess");
	//BYTE antihook1[5] = { 0xC2, 0x10, 0x0, NOP, NOP };
	//memory_move(pfnNtOpenProcess, (void*)antihook1, sizeof(antihook1));

	//DWORD pfnNtWriteVirtualMemory = ker32.GetFunAddr(ntdllModule, "NtWriteVirtualMemory");
	//BYTE antihook2[6] = { 0xC2, 0x14, 0x0, NOP, NOP, NOP };
	//memory_move(pfnNtWriteVirtualMemory + 0xA, (void*)antihook2, sizeof(antihook2));

	//DWORD pfnNtReadVirtualMemory = ker32.GetFunAddr(ntdllModule, "NtReadVirtualMemory");
	//BYTE antihook3[6] = { 0xC2, 0x14, 0x0, NOP, NOP, NOP };
	//memory_move(pfnNtReadVirtualMemory + 0xA, (void*)antihook3, sizeof(antihook3));
}

void CAsm::HOOK_global_unhook()
{
	unhook(m_pfnOrigMessageBox, m_bOldMessageBoxBytes, sizeof(m_bOldMessageBoxBytes));

	*(double*)SUPERSPEED = 16.6666666666667;
	this->is_hook_installed = 0;//初始化標誌位
	CAsm::asm_ptr->is_hook_installed = this->is_hook_installed;
	//this->serverhWnd = NULL;
	is_auto_battle = CAsm::asm_ptr->AIOFF;
	CAsm::asm_ptr->is_auto_login = false;
	CAsm::asm_ptr->is_auto_walk_cache = false;
	CAsm::asm_ptr->is_auto_battle = CAsm::asm_ptr->AIOFF;
	set_battle_memory_data(true);
	constexpr BYTE tt7[2] = { 0x74, 0x38 };
	memory_move(MAKEADDR(0x4, 0x0, 0x4, 0x4, 0x0, 0xC)/*0x040440C*/, (void*)tt7, sizeof(tt7));//防加速回復
	constexpr BYTE tt8[5] = { 0x3D, 0x64, 0x5, 0x0, 0x0 };
	memory_move(MAKEADDR(0x4, 0x0, 0x4, 0x3, 0xB, 0xD)/*0x04043BD*/, (void*)tt8, sizeof(tt8));//防加速回復2
	constexpr BYTE t[5] = { 0x5, 0x30, 0x75, 0x0, 0x0 };
	memory_move(MAKEADDR(0x4, 0x3, 0x8, 0x1, 0xC, 0x1)/*0x04381C1*/, (void*)t, sizeof(t));//秒數
	disable_draw_effect(false);
	disable_battle_anime_effect(false);

	//BYTE wx[5] = { 0xB9, 0xC8, 0x9A, 0x92, 0x0 };
	//memory_move(0x0464477, (void*)wx, sizeof(wx));
	//unhook(m_pfnOrigWalk, m_bOldWalkBytesX, sizeof(m_bOldWalkBytesX));
	//BYTE wy[5] = { 0xB9, 0xD8, 0x9A, 0x92, 0x0 };
	//memory_move(0x0464483, (void*)wy, sizeof(wy));
	//unhook(m_pfnOrigWalkY, m_bOldWalkBytesY, sizeof(m_bOldWalkBytesY));

	unhook(m_pfnOrigHpOUT, m_bOldHpBytesOUT, sizeof(m_bOldHpBytesOUT));
	unhook(m_pfnOrigHpIN, m_bOldHpBytesIN, sizeof(m_bOldHpBytesIN));
	unhook(m_pfnOrigCreateCharScreen, m_bOldCreateCharScreenBytes, sizeof(m_bOldCreateCharScreenBytes));
	unhook(m_pfnOrigCreatePetScreen, m_bOldCreatePetScreenBytes, sizeof(m_bOldCreatePetScreenBytes));
	unhook(m_pfnOrigCreateRideScreen, m_bOldCreateRideScreenBytes, sizeof(m_bOldCreateRideScreenBytes));
	unhook(m_pfnOrigHotKey, m_bOldHotKeyBytes, sizeof(m_bOldHotKeyBytes));
}

void CAsm::set_battle_memory_data(const bool& en)
{
	if (!en)
	{
		constexpr BYTE tt[5] = { NOP, NOP, NOP, NOP, NOP };
		memory_move(MAKEADDR(0x4, 0x9, 0x1, 0x1, 0x3, 0xE)/*0x049113E*/, (void*)tt, sizeof(tt));//戰鬥讀秒顯示
		memory_move(MAKEADDR(0x4, 0x9, 0x1, 0x2, 0x5, 0x3)/*0x0491253*/, (void*)tt, sizeof(tt));//戰鬥寵物技能面板
		memory_move(MAKEADDR(0x4, 0x9, 0x0, 0xE, 0xD, 0x7)/*0x0490ED7*/, (void*)tt, sizeof(tt));//戰鬥人物面板音效
		memory_move(MAKEADDR(0x4, 0x9, 0x1, 0x2, 0x8, 0x5)/*0x0491285*/, (void*)tt, sizeof(tt));//戰鬥寵物面板音效
		memory_move(MAKEADDR(0x4, 0x9, 0x0, 0xD, 0x2, 0x2)/*0x0490D22*/, (void*)tt, sizeof(tt));//騎寵戰鬥中第一次騎寵/第二回合面板音效
		memory_move(MAKEADDR(0x4, 0xF, 0xA, 0xA, 0x5, 0xD)/*0x04FAA5D*/, (void*)tt, sizeof(tt));//經驗結算面板
	}
	else
	{
		constexpr BYTE tt[5] = { CALL, 0x4D, 0xF7, 0xFF, 0xFF };
		memory_move(MAKEADDR(0x4, 0x9, 0x1, 0x1, 0x3, 0xE), (void*)tt, sizeof(tt));//戰鬥讀秒顯示
		constexpr BYTE tt2[5] = { CALL, 0x28, 0x20, 0xFE, 0xFF };
		memory_move(MAKEADDR(0x4, 0x9, 0x1, 0x2, 0x5, 0x3), (void*)tt2, sizeof(tt2));//戰鬥寵物技能面板
		constexpr BYTE tt3[5] = { CALL, 0x24, 0xE, 0x9, 0x0 };
		memory_move(MAKEADDR(0x4, 0x9, 0x0, 0xE, 0xD, 0x7), (void*)tt3, sizeof(tt3));//戰鬥人物面板音效
		constexpr BYTE tt4[5] = { CALL, 0x76, 0xA, 0x9, 0x0 };
		memory_move(MAKEADDR(0x4, 0x9, 0x1, 0x2, 0x8, 0x5), (void*)tt4, sizeof(tt4));//戰鬥寵物面板音效
		constexpr BYTE tt5[5] = { CALL, 0xD9, 0xF, 0x9, 0x0 };
		memory_move(MAKEADDR(0x4, 0x9, 0x0, 0xD, 0x2, 0x2), (void*)tt5, sizeof(tt5));//騎寵戰鬥中第一次騎寵/第二回合面板音效
		constexpr BYTE tt6[5] = { CALL, 0xCE, 0x3D, 0xF7, 0xFF };
		memory_move(MAKEADDR(0x4, 0xF, 0xA, 0xA, 0x5, 0xD), (void*)tt6, sizeof(tt6));//經驗結算面板
	}
}

void CAsm::disable_draw_effect(const bool& en)
{
	if (en)
	{
		constexpr BYTE e[10] = { 0xC7, 0x5, 0x54, 0x29, 0xF6, 0x0, 0x3, 0x0, 0x0, 0x0 };
		memory_move(MAKEADDR(0x4, 0x6, 0x3, 0xB, 0x9, 0x7)/*0x0463B97*/, (void*)e, sizeof(e));//高切
	}
	else
	{
		constexpr BYTE e[10] = { 0xC7, 0x5, 0x54, 0x29, 0xF6, 0x0, 0xC8, 0x0, 0x0, 0x0 };
		memory_move(MAKEADDR(0x4, 0x6, 0x3, 0xB, 0x9, 0x7)/*0x0463B97*/, (void*)e, sizeof(e));//高切
	}
}

void CAsm::disable_battle_anime_effect(const bool& en)
{
	if (en)
	{
		constexpr BYTE e[2] = { 0x74, 0x4D };
		memory_move(MAKEADDR(0x4, 0x3, 0x7, 0x1, 0x6, 0xA)/*0x043716A*/, (void*)e, sizeof(e));//高切
	}
	else
	{
		constexpr BYTE e[2] = { 0x75, 0x4D };
		memory_move(MAKEADDR(0x4, 0x3, 0x7, 0x1, 0x6, 0xA)/*0x043716A*/, (void*)e, sizeof(e));//高切
	}
}
#pragma endregion

#pragma region HOOK_Proc
int CAsm::hook_hotkey_extend(const BYTE& nret)
{
	CFormat& format = CFormat::get_instance();
	try
	{
		//SHIFT+F8
		if (nret == 0xB8 && CAsm::asm_ptr->is_hook_installed)
		{
			bool bret = false;
			std::wstring wstr;
			if (CAsm::asm_ptr->is_auto_battle == CAsm::asm_ptr->AIOFF)
			{
				format.set(FORMATION::ASM_AUTOONFORMAT, wstr);
				CAsm::asm_ptr->AION = (DWORD)CRuntime::rand(0, 0x7ffffff);
				CAsm::asm_ptr->AIOFF = (DWORD)CRuntime::rand(0, 0x7ffffff);
				CAsm::asm_ptr->is_auto_battle = CAsm::asm_ptr->AION;
				bret = true;
				this->set_battle_memory_data(false);
				if (this->is_battle_screen_ready_for_hotkey())
				{
					this->hook_create_battle_screen(CAsm::asm_ptr->battle_screen_type_cache, 0x155, 0x0);
				}
			}
			else if (CAsm::asm_ptr->is_auto_battle == CAsm::asm_ptr->AION)
			{
				format.set(FORMATION::ASM_AUTOOFFFORMAT, wstr);
				CAsm::asm_ptr->AION = (DWORD)CRuntime::rand(0, 0x7ffffff);
				CAsm::asm_ptr->AIOFF = (DWORD)CRuntime::rand(0, 0x7ffffff);
				CAsm::asm_ptr->is_auto_battle = CAsm::asm_ptr->AIOFF;
				bret = true;
				this->set_battle_memory_data(true);
				if (this->is_battle_screen_ready_for_hotkey())
				{
					this->hook_create_battle_screen(CAsm::asm_ptr->battle_screen_type_cache, 0x155, 0x0);
				}
			}
			if (bret)
				this->announcement(wstr, 4, 3);

			return 0xc8;
		}
		//SHIFT+F7
		else if (nret == 0xb0 && CAsm::asm_ptr->is_hook_installed)
		{
			std::string sstr;

			double pSpeed = CAsm::global_static_battle_speed_cache;
			if ((int)(16.6666666666667 - pSpeed) >= 16)
				pSpeed = 16.6666666666667;
			else
				pSpeed -= 1.0;

			CAsm::global_static_battle_speed_cache = pSpeed;
			std::wstring wstr;
			format.set(FORMATION::ASM_SPEEDCHANGED, wstr);
			WCHAR wstr2[MAX_PATH] = { TEXT("倍\0") };
			std::wstring wstr3(std::format(TEXT("{}{}{}"), wstr, (int)(16.6666666666667 - pSpeed), wstr2));
			this->announcement(wstr3, 4, 3);
			return 0xc8;
		}
		//SHIFT+F6
		else if (nret == 0xa8 && CAsm::asm_ptr->is_hook_installed)
		{
			double pSpeed = CAsm::global_static_battle_speed_cache;
			if ((int)(16.6666666666667 - pSpeed) <= 0)
				pSpeed = 0.0;
			else
				pSpeed += 1.0;

			CAsm::global_static_battle_speed_cache = pSpeed;
			std::wstring wstr;
			format.set(FORMATION::ASM_SPEEDCHANGED, wstr);
			WCHAR wstr2[MAX_PATH] = { TEXT("倍\0") };
			std::wstring wstr3(std::format(TEXT("{}{}{}"), wstr, (int)(16.6666666666667 - pSpeed), wstr2));
			this->announcement(wstr3, 4, 3);
			return 0xc8;
		}
		//SHIFT+F5
		else if (nret == 0xa0 && CAsm::asm_ptr->is_hook_installed)
		{
			//std::wstring wstr;
			//if (!CAsm::asm_ptr->is_auto_login)
			//{
			//	format.set(FORMATION::ASM_AUTOLOGINONFORMAT, wstr);
			//	CAsm::asm_ptr->is_auto_login = true;
			//}
			//else
			//{
			//	format.set(FORMATION::ASM_AUTOLOGINOFFFORMAT, wstr);
			//	CAsm::asm_ptr->is_auto_login = false;
			//}
			//this->announcement(wstr, 4, 3);
			//return 0xc8;
		}
		//SHIFT+F1
		else if (nret == 0x80 && CAsm::asm_ptr->is_hook_installed)
		{
			std::wstring wstr;
			if (!CAsm::asm_ptr->is_auto_walk_cache)
			{
				CAsm::asm_ptr->is_auto_walk_cache = true;
				format.set(FORMATION::ASM_auto_walkSWITCHER_ON, wstr);
			}
			else
			{
				CAsm::asm_ptr->is_auto_walk_cache = false;
				format.set(FORMATION::ASM_auto_walkSWITCHER_OFF, wstr);
			}
			this->announcement(wstr, 4, 3);

			return 0xc8;
		}
	}
	catch (...)
	{
		return nret;
	}
	return nret;
}

__declspec(naked) int CAsm::hook_movement()
{
	if (CAsm::asm_ptr->is_auto_walk && !*(int*)0xcda984 && !*(int*)0xc0c2dc)
	{
		__asm
		{
			mov eax, g_movexy.x
			mov ebx, eax
			mov[esp + 0x10], ebx
			mov eax, g_movexy.y
			mov ebp, eax
			mov[esp + 0x1C], ebp
		}
	}
	else
	{
		__asm
		{
			mov ecx, 0x0929AC8
			mov eax, 0x0401EA0
			call eax
			mov ebx, eax
			mov[esp + 0x10], ebx
			mov ecx, 0x0929AD8
			mov eax, 0x0401EA0
			call eax
			mov ebp, eax
			mov[esp + 0x1C], ebp
		}
	}
	__asm
	{
		mov ecx, 0x09703E8
		mov eax, 0x0401EA0
		call eax
		mov esi, eax
		mov ecx, 0x0970400
		mov eax, 0x0401EA0
		call eax
		mov ecx, offset m_pfnOrigWalk
		mov ecx, dword ptr ss : [ecx]
		lea ecx, dword ptr ss : [ecx + 0x5]
		jmp ecx//跳轉回原地址
	}
}

int CAsm::hook_hotkey()
{
	int nret = 0xff;
	_asm
	{
		lea eax, [ebx * 8 + 0x0000000]
		mov nret, eax
	}
	CAsm::asm_ptr->hook_hotkey_extend((BYTE)nret);
}

int CAsm::hook_hp_mp_in_battle()
{
	CRSA& rsa = CRSA::get_instance();
	unsigned int data[5] = {};
	unsigned int tmp = 0;

	int myecx = 0;
	int tmpecx = 0;

	__try
	{
		data[s_status] = (unsigned int)(3 ^ rsa.getXorKey());

		_asm
		{
			mov ecx, dword ptr ds : [0x05989DC]
			lea edx, dword ptr ss : [ecx * 4 + 0x058C6C4]
			mov ecx, dword ptr ss : [edx]
			mov myecx, ecx
		}
		tmpecx = myecx + 0x180;
		tmp = (unsigned int)get_table_value(tmpecx);
		if (tmp > 0)
			data[s_maxhp] = (unsigned int)(tmp ^ rsa.getXorKey());
		tmpecx = myecx + 0x170;
		tmp = (unsigned int)get_table_value(tmpecx);
		if (tmp > 0)
			data[s_hp] = (unsigned int)(tmp ^ rsa.getXorKey());
		tmpecx = myecx + 0x1C0;
		tmp = (unsigned int)get_table_value(tmpecx);
		if (tmp > 0)
			data[s_maxmp] = (unsigned int)(tmp ^ rsa.getXorKey());
		tmpecx = myecx + 0x1B0;
		tmp = (unsigned int)get_table_value(tmpecx);
		if (tmp > 0)
			data[s_mp] = (unsigned int)(tmp ^ rsa.getXorKey());

		memmove(CAsm::asm_ptr->global_status_caches, data, sizeof(data));
	}
	__finally
	{
		tmpecx = myecx + 0x170;
		if (tmpecx != NULL)
			return get_table_value(tmpecx);
		else
			return 0;
	}

	//_asm
	//{
	//	mov eax, offset ub.status
	//	mov[eax], 0x3//設置狀態標誌

	//	mov ebx, 0x0401EA0//保存call指針

	//	mov ecx, 0x0F4C338
	//	call ebx//獲取maxmp
	//	mov dword ptr ds : [ub.maxmp] , eax
	//	mov ecx, 0x0F4C328
	//	call ebx//獲取mp
	//	mov dword ptr ds : [ub.mp] , eax

	//	mov ecx, dword ptr ds : [0x05989DC]
	//	lea edx, dword ptr ss : [ecx * 4 + 0x058C6C4]
	//	mov ecx, dword ptr ss : [edx]
	//	add ecx, 0x0000180
	//	call ebx//獲取maxhp
	//	mov dword ptr ds : [ub.maxhp] , eax
	//	mov ecx, dword ptr ds : [0x05989DC]
	//	lea edx, dword ptr ss : [ecx * 4 + 0x058C6C4]
	//	mov ecx, dword ptr ss : [edx]
	//	add ecx, 0x0000170
	//	call ebx//獲取maxhp
	//	mov dword ptr ds : [ub.hp] , eax
	//	mov ecx, offset m_pfnOrigHpIN
	//	mov ecx, dword ptr ss : [ecx]
	//	lea ecx, dword ptr ss : [ecx + 0x5]
	//	jmp ecx//跳轉回原地址
	//}
}

int CAsm::hook_hp_mp_in_normal()
{
	CRSA& rsa = CRSA::get_instance();
	const DWORD args = MAKEADDR(0xF, 0x4, 0xC, 0x3, 0x3, 0x8);
	unsigned int data[5] = {};
	unsigned int tmp = 0;
	__try
	{
		data[s_status] = (unsigned int)(2 ^ rsa.getXorKey());
		tmp = (unsigned int)get_table_value(args);
		if (tmp > 0)
			data[s_maxmp] = (unsigned int)(tmp ^ rsa.getXorKey());
		tmp = (unsigned int)get_table_value(args - 0x10);
		if (tmp > 0)
			data[s_mp] = (unsigned int)(tmp ^ rsa.getXorKey());
		tmp = (unsigned int)get_table_value(args - 0x20);
		if (tmp > 0)
			data[s_maxhp] = (unsigned int)(tmp ^ rsa.getXorKey());
		tmp = (unsigned int)get_table_value(args - 0x30);
		if (tmp > 0)
			data[s_hp] = (unsigned int)(tmp ^ rsa.getXorKey());

		memmove(CAsm::asm_ptr->global_status_caches, data, sizeof(data));
	}
	__finally
	{
		return get_table_value(args - 0x10);
	}

	//_asm
	//{
	//	mov ebx, 0x0401EA0//保存call指針
	//	mov eax, offset ub.status
	//	mov[eax], 0x2//設置狀態標誌

	//	mov ecx, 0x0F4C318//maxhp
	//	call ebx
	//	mov dword ptr ds : [ub.maxhp] , eax
	//	mov ecx, 0x0F4C308//hp
	//	call ebx
	//	mov dword ptr ds : [ub.hp] , eax
	//	mov ecx, 0x0F4C338//maxmp
	//	call ebx
	//	mov dword ptr ds : [ub.maxmp] , eax
	//	mov ecx, 0x0F4C328//mp
	//	call ebx
	//	mov dword ptr ds : [ub.mp] , eax
	//	mov ebx, offset m_pfnOrigHpOUT
	//	mov ebx, dword ptr ss : [ebx]
	//	lea ebx, dword ptr ss : [ebx + 0x5]
	//	mov edx, 0x10
	//	mov ecx, 0x0F4C328
	//	mov eax, 0x0401EA0
	//	call eax
	//	jmp ebx//跳轉回原地址
	//}
}

//void CAsm::on_HookMessageBox()//0x04600C0
//{
//}

//void __declspec(naked) CAsm::on_HookXcode()
//{
//	_asm
//	{
//		mov ecx, [eax]
//		mov dword ptr ds : [CAsm::xcodeaddr] , ecx
//		jmp m_pfnNewXcodeDis
//	}
//}
#pragma endregion

#pragma region Memory_get_info
void CAsm::get_char_xy(int& east, int& south)
{
	const int x = get_table_value(MAKEADDR(0x9, 0x7, 0x0, 0x3, 0xE, 0x8)/*0x09703E8*/);
	const int y = get_table_value(MAKEADDR(0x9, 0x7, 0x0, 0x4, 0x0, 0x0)/*0x0970400*/);
	east = x;
	south = y;
}

inline bool WINAPI GetItemExist(const int& index)
{
	if (index < 0 || index > 19)
		return false;
	const DWORD base = (DWORD)MAKEADDR(0xC, 0xC, 0x7, 0x2, 0x8, 0x0);//0x0CC7280;
	return (*(BYTE*)(base + (0xC4C * index)) > 0);
}

inline int WINAPI GetItemId(const int& index)
{
	if (index < 0 || index > 19)
		return 0;
	const DWORD base = (DWORD)MAKEADDR(0xC, 0xC, 0x7, 0x2, 0x8, 0x4);//0x0CC7284;
	return *(int*)(base + (0xC4C * index));
}

inline void WINAPI GetItemName(const int& index, char* str, const size_t& size)
{
	if (index < 0 || index > 19)
		return;
	if (GetItemExist(index))
	{
		const DWORD base = (DWORD)MAKEADDR(0xC, 0xC, 0x7, 0x2, 0x9, 0x0);//0x0CC7290;
		DWORD addr = base + (0xC4C * index);
		_snprintf_s(str, size, _TRUNCATE, "%s\0", (char*)addr);
	}
	else
	{
		memset(str, 0, 24);
	}
}

inline bool WINAPI GetBankItemExist(const int& index)
{
	if (index < 0 || index > 19)
		return false;
	const DWORD base = (DWORD)MAKEADDR(0xC, 0x4, 0x6, 0x3, 0x4, 0x8);//0xc46348;
	return (*(BYTE*)(base + (0xC44 * index)) > 0);
}

inline const bool GetBankItemName(const int& index, std::wstring& wstr)
{
	if (index < 0 || index > 19)
		return false;
	if (!GetBankItemExist(index))
		return false;
	const DWORD base = MAKEADDR(0xc, 0x4, 0x6, 0x3, 0x5, 0x4);//00C46354
	std::wstring tmp = CRuntime::char2wchar((char*)(base + (index * 0xC44)));
	if (tmp.empty())
		return false;

	wstr = tmp;
	return true;
}

inline int find_bank_empty_spot()
{
	for (int i = 0; i < 20; i++)
	{
		if (!GetBankItemExist(i))
		{
			return i;
		}
	}
	return -1;
}

inline bool find_bank_empty_spot(std::vector<int>& v)
{
	v.clear();
	bool bret = false;
	for (int i = 0; i < 20; i++)
	{
		if (!GetBankItemExist(i))
		{
			v.push_back(i);
			bret = true;
		}
	}
	return bret;
}

inline int WINAPI find_empyt_spot()
{
	for (int i = 0; i < 20; i++)
	{
		if (!GetItemExist(i))
		{
			return i;
		}
	}
	return -1;
}

inline bool WINAPI find_empyt_spot(std::vector<int>& v)
{
	v.clear();
	bool bret = false;
	for (int i = 0; i < 20; i++)
	{
		if (!GetItemExist(i))
		{
			v.push_back(i);

			bret = true;
		}
	}

	return bret;
}

inline int WINAPI GetItemStack(const int& index)
{
	if (index == -1)
		return 0;
	if (GetItemExist(index))
	{
		const DWORD base = (DWORD)MAKEADDR(0xC, 0xC, 0x7, 0x2, 0x8, 0x8);//0x0CC7288;
		const DWORD addr = base + (0xC4C * index);
		int stack = *(int*)(addr);
		if (stack == 0)
			stack = 1;
		return stack;
	}
	return 0;
}

inline void WINAPI GetSkill(const int& index, char* str, const size_t& size)
{
	if (index == -1)
		return;
	const DWORD base = (DWORD)MAKEADDR(0xE, 0x8, 0xD, 0x6, 0xE, 0xC);//0x0E8D6EC;
	const DWORD addr = base + (0x4C4C * index);
	_snprintf_s(str, size, _TRUNCATE, "%s\0", (char*)addr);
}

inline void WINAPI GetSubSkill(const int& index, const int& subindex, char* str, const size_t& size)
{
	if (index == -1 || subindex == -1)
		return;
	const DWORD base = (DWORD)MAKEADDR(0xE, 0x8, 0xD, 0x7, 0x2, 0x8);//0x0E8D728;//724 = posID
	const DWORD addr = base + (0x4C4C * index) + (0x94 * subindex);

	_snprintf_s(str, size, _TRUNCATE, "%s\0", (char*)addr);
}

inline void WINAPI GetChildSkill(const int& skill, const int& j, char* str, const size_t& size)
{
	if (skill == -1 || j == -1)
		return;
	const DWORD base = (DWORD)MAKEADDR(0xE, 0x8, 0xD, 0xF, 0xE, 0x8);//0xe8dfe8
	const DWORD addr = base + (0x4C4C * skill) + (0x134 * j);

	_snprintf_s(str, size, _TRUNCATE, "%s\0", (char*)addr);
}

inline void WINAPI GetSkillExp(const int& index, int& min, int& max)
{
	if (index == -1)
		return;
	const DWORD minaddr = MAKEADDR(0xE, 0x8, 0xD, 0x7, 0x1, 0x0)/*0x0E8D710*/ + (0x4C4C * index);
	const DWORD maxaddr = MAKEADDR(0xE, 0x8, 0xD, 0x7, 0x1, 0x4)/*0x0E8D714*/ + (0x4C4C * index);
	min = *(int*)minaddr;
	max = *(int*)maxaddr;
}

inline bool WINAPI isEnemyExist(const int& index)
{
	if (index == -1)
		return 0;
	const DWORD ebase = MAKEADDR(0x5, 0x8, 0xC, 0x6, 0xC, 0x7)/*0x058C6C7*/ + (index * 4);
	BYTE result = *(BYTE*)ebase;
	return result > 0;
}

void CAsm::get_char_skill_info(SkillInfo& charskill)
{
	SkillInfo skill = {};
	skill.identify.cbsize = sizeof(SkillInfo);
	skill.identify.msg = DA_SKILLINFO;
	unsigned int j = 0;
	int min = 0;
	int max = 0;
	for (unsigned int i = 0; i < 14; i++)
	{
		GetSkill(i, skill.name[i], sizeof(skill.name[i]));
		GetSkillExp(i, min, max);
		for (j = 0; j < 11; j++)
			GetSubSkill(i, j, skill.subname[i][j], sizeof(skill.subname[i][j]));
	}
	charskill = skill;
}
void CAsm::get_all_item_info(ItemInfo& iinfo)
{
	ItemInfo item = {};
	item.identify.cbsize = sizeof(ItemInfo);
	item.identify.msg = DA_ITEMINFO;
	//sizeof(ItemInfo);
	for (unsigned int i = 0; i < 28; i++)
	{
		if (i < 20)
		{
			item.exist[i] = GetItemExist(i);
			item.id[i] = GetItemId(i);
			item.stack[i] = GetItemStack(i);
			GetItemName(i, item.name[i], sizeof(item.name[i]));
		}
		else
		{
			item.exist[i] = *(BYTE*)(MAKEADDR(0xF, 0x4, 0xC, 0x4, 0x9, 0x4)/*0x0F4C494*/ + (0xC5C * (i - 20))) > 0;
			if (item.exist[i])
			{
				item.id[i] = *(int*)(MAKEADDR(0xF, 0x4, 0xD, 0x0, 0xC, 0xC)/*0x0F4D0CC*/ + (0xC5C * (i - 20)));
				item.stack[i] = 0;
				_snprintf_s(item.name[i], sizeof(item.name[i]), _TRUNCATE, "%s\0", (char*)(MAKEADDR(0xF, 0x4, 0xC, 0x4, 0x9, 0x6)/*0x0F4C496*/ + (0xC5C * (i - 20))));
			}
			else
			{
				item.id[i] = 0;
				item.stack[i] = 0;
				memset(item.name[i], 0, sizeof(item.name[i]));
				_snprintf_s((char*)(MAKEADDR(0xF, 0x4, 0xC, 0x4, 0x9, 0x6)/*0x0F4C496*/ + (0xC5C * (i - 20))), 1, _TRUNCATE, "%s", (char*)"\0");
			}
		}
	}
	iinfo = item;
}

int CAsm::update_pet_position()
{
	typedef int(__cdecl* pfnUpdatePetPos)(int a, int b, int* c, int d);
	const pfnUpdatePetPos UpdatePetPos = (pfnUpdatePetPos)MAKEADDR(0x4, 0x8, 0x5, 0xA, 0x9, 0x0);//0x0485A90;
	__try
	{
		__asm
		{
			mov ebx, 0x0
			mov ecx, 0x1
			mov edx, 0xC
			mov esi, 0x4EC
			mov edi, 0x0
		}
		UpdatePetPos(0, 0x1, (int*)MAKEADDR(0xC, 0x0, 0xC, 0xB, 0xD, 0x4)/*0xc0cbd4*/, 0xc);
	}
	__finally
	{
		return *(int*)MAKEADDR(0xC, 0x4, 0x6, 0x2, 0xC, 0x8);//0x0C462C8;
	}
}

void CAsm::get_char_info(CharInfo& charinfo)
{
	const DWORD base = MAKEADDR(0xF, 0x4, 0xC, 0x3, 0x6, 0x8);

	CharInfo cinfo = *(CharInfo*)base;//0x0F4C368;
	unsigned int data[5] = {};
	CRSA& rsa = CRSA::get_instance();
	memmove(data, CAsm::asm_ptr->global_status_caches, sizeof(data));

	cinfo.level = (int)(*(int*)LEVELADDR);
	cinfo.health = (int)(*(int*)MAKEADDR(0xF, 0x4, 0xC, 0x3, 0xE, 0x4)/*0x0F4C3E4*/);//健康度，0为绿色，100为红色
	cinfo.souls = (int)(*(int*)MAKEADDR(0xF, 0x4, 0xC, 0x3, 0xE, 0x8)/*0x0F4C3E8*/);//掉魂数，0~5
	cinfo.gold = (int)(*(int*)GOLDADDR);
	cinfo.unitid = (int)(*(int*)MAKEADDR(0xF, 0x4, 0xC, 0x3, 0xF, 0x4)/*0x0F4C3F4*/);
	cinfo.petpos = (signed int)(*(int*)MAKEADDR(0xC, 0x4, 0x6, 0x2, 0xC, 0x8)/*0x0C462C8*/);
	cinfo.direction = (int)(*(int*)MAKEADDR(0xF, 0x4, 0xC, 0x4, 0x7, 0x4)/*0x0F4C474*/);
	cinfo.punchclock = (int)(*(int*)MAKEADDR(0xF, 0x4, 0xC, 0x2, 0xF, 0xC)/*0x0F4C2FC*/);//卡時
	cinfo.isusingpunchclock = (int)(*(BYTE*)MAKEADDR(0xF, 0x4, 0xC, 0x3, 0x0, 0x0)/*0x0F4C300*/);//是否已打卡
	cinfo.point = (int)*(int*)MAKEADDR(0xC, 0xB, 0x0, 0xA, 0xF, 0x4);//0x0CB0AF4;
	_snprintf_s(cinfo.name, sizeof(cinfo.name), _TRUNCATE, "%s\0", (char*)CHARNAMEADDR);
	_snprintf_s(cinfo.nickname, sizeof(cinfo.nickname), _TRUNCATE, "%s\0", (char*)CHARNAMEADDR + 17);
	if ((data[s_maxhp] != 0) && (data[s_maxhp] ^ rsa.getXorKey()) > 10000)
	{
		cinfo.hp = 0;
		cinfo.maxhp = 0;
		cinfo.mp = 0;
		cinfo.maxmp = 0;
	}
	else
	{
		if (data[s_hp] != 0)
			cinfo.hp = (int)(data[s_hp] ^ rsa.getXorKey());
		if (data[s_maxhp] != 0)
			cinfo.maxhp = (int)(data[s_maxhp] ^ rsa.getXorKey());
		if (data[s_mp] != 0)
			cinfo.mp = (int)(data[s_mp] ^ rsa.getXorKey());
		if (data[s_maxmp] != 0)
			cinfo.maxmp = (int)(data[s_maxmp] ^ rsa.getXorKey());
	}

	charinfo = cinfo;
}

void CAsm::get_pet_info(const int& index, PetInfo& petinfo)
{
	struct PetStatus
	{
		int hp;
		int maxhp;
		int mp;
		int maxmp;
	};
	typedef void(_cdecl* pfnPetStatusUpdate)();
	const pfnPetStatusUpdate petStatusUpdate = (pfnPetStatusUpdate)MAKEADDR(0x4, 0x8, 0xE, 0xB, 0x3, 0x0);//0x048EB30;
	petStatusUpdate();
	const DWORD base = (MAKEADDR(0xE, 0xD, 0x4, 0xF, 0xE, 0x8)/*0x0ED4FE8*/ + (0x5110 * index));
	PetInfo pinfo = *(PetInfo*)base;

	PetStatus petstatus = *(PetStatus*)(MAKEADDR(0xC, 0x7, 0x6, 0x0, 0x1, 0x8)/*0x0C76018*/ + (0x10 * index));
	//wchar_t a[1024] = {};
	//_snwprintf_s(a, 1024, L"HP:%d/%d\r\nMP:%d/%d", petstatus.hp, petstatus.maxhp, petstatus.mp, petstatus.maxmp);
	//MessageBox(0, a, L"petinfo", MB_OK);
	//wchar_t a[256] = {};
	pinfo.hp = petstatus.hp;
	pinfo.maxhp = petstatus.maxhp;
	pinfo.mp = petstatus.mp;
	pinfo.maxmp = petstatus.maxmp;
	petinfo = pinfo;
}

bool CAsm::get_battle_info(BattleInfo& btinfo)
{
	WCHAR wstr[MAX_PATH] = {};
	get_private_sfc32dll(wstr);
	CKernel32& ker32 = CKernel32::get_instance();
	const HMODULE mod = ker32.My_GetModuleHandleW(wstr);
	if (mod == NULL)
		return false;
	const DWORD base = (DWORD)mod + MAKEADDR(0x5, 0x8, 0xA, 0x0, 0x2, 0x0);// 0x58A020;
	const std::vector<std::string> v = CRuntime::char_split((char*)base, "|");
	const int count = v.size();
	if (count <= 0)
		return false;
	int n = 0;
	btinfo.round = (*(int*)0x05988AC) + 1;
	for (int i = 0; i < (int)(count / 12) * 12; i += 12)
	{
		n = std::stoi(v.at(i + 2), nullptr, 16);
		btinfo.exist[n] = isEnemyExist(n);
		btinfo.unknown0[n] = std::stoll(v.at(i), nullptr, 16);
		btinfo.unknown1[n] = std::stoll(v.at(i + 1), nullptr, 16);
		btinfo.pos[n] = n;
		_snprintf_s(btinfo.name[n], sizeof(btinfo.name[n]), _TRUNCATE, "%s\0", v.at(i + 3).c_str());
		btinfo.model[n] = std::stoi(v.at(i + 4), nullptr, 16);
		btinfo.unknown2[n] = std::stoll(v.at(i + 5), nullptr, 16);
		btinfo.level[n] = std::stoi(v.at(i + 6), nullptr, 16);
		btinfo.hp[n] = std::stoi(v.at(i + 7), nullptr, 16);
		btinfo.maxhp[n] = std::stoi(v.at(i + 8), nullptr, 16);
		btinfo.mp[n] = std::stoi(v.at(i + 9), nullptr, 16);
		btinfo.maxmp[n] = std::stoi(v.at(i + 10), nullptr, 16);
		btinfo.status[n] = std::stoll(v.at(i + 11), nullptr, 16);
	}
	btinfo.totlaEnemy = 0;
	for (int i = 10; i < 20; i++)
	{
		if (btinfo.exist[i] > 0)
		{
			btinfo.totlaEnemy++;
		}
	}
	return true;
}

const TableInfo CAsm::get_table_info(const int& num, const int& prestatus)
{
	TableInfo tinfo = {};
	tinfo.identify.cbsize = sizeof(TableInfo);
	tinfo.identify.msg = DA_USERTABLE;
	tinfo.identify.gnum = num;

	//解密全局HP MP 及其他数据
	unsigned int data[5] = {};
	CRSA& rsa = CRSA::get_instance();
	memmove(data, CAsm::asm_ptr->global_status_caches, sizeof(data));
	if (prestatus == 0)
		memset(&data, 0, sizeof(data));
	int tmp_status = prestatus;
	int* s[3] = { (int*)0x0D15718,(int*)0x072BA24, (int*)0x0C0CBD4 };
	if (tmp_status == 0)
		for (int i = 0; i < 3; i++)
			*s[i] = 0;
	if (!*(BYTE*)s[0] && !*(BYTE*)s[1] && !*(BYTE*)s[2])
		tmp_status = 0;

	if (tmp_status > 0)
	{
		if (data[s_status] != 0)
			tinfo.status = (int)(data[s_status] ^ rsa.getXorKey());
		//獲取 HP MP
		if (data[s_hp] != 0)
			tinfo.hp = (int)(data[s_hp] ^ rsa.getXorKey());
		if (data[s_maxhp] != 0)
			tinfo.maxhp = (int)(data[s_maxhp] ^ rsa.getXorKey());
		if (data[s_mp] != 0)
			tinfo.mp = (int)(data[s_mp] ^ rsa.getXorKey());
		if (data[s_maxmp] != 0)
			tinfo.maxmp = (int)(data[s_maxmp] ^ rsa.getXorKey());
		tinfo.level = (int)(*(int*)LEVELADDR);//等級
		tinfo.gold = (int)(*(int*)GOLDADDR);//金幣
		tinfo.time = (int)(*(int*)TIMEADDR);//運行時間
		tinfo.mapid = (int)(*(int*)(MAPADDR + 0x28));//地圖id
		get_char_xy(tinfo.east, tinfo.south);//座標
		_snprintf_s(tinfo.map, sizeof(tinfo.map), _TRUNCATE, "%s\0", (char*)MAPADDR);//地圖名
		_snprintf_s(tinfo.account, sizeof(tinfo.account), _TRUNCATE, "%s\0", (char*)USERNAMEADDR);//遊戲帳號
		_snprintf_s(tinfo.name, sizeof(tinfo.name), _TRUNCATE, "%s\0", (char*)CHARNAMEADDR);//腳色名稱
	}
	else
		tinfo.status = 0;

	table = tinfo;
	*CAsm::asm_ptr->g_info_ptr.tableinfo = tinfo;
	return tinfo;
}
#pragma endregion

#pragma region ASM_action

inline void WINAPI close_screen()
{
	typedef int(__cdecl* pfnCloseScreen)(IN int type);
	pfnCloseScreen CloseScreen = (pfnCloseScreen)MAKEADDR(0x4, 0x6, 0xA, 0xF, 0xC, 0x0);

	CloseScreen(7);
}

inline void WINAPI clear_screen()
{
	typedef int(__cdecl* pfnClearScreen)();
	pfnClearScreen ClearScreen = (pfnClearScreen)MAKEADDR(0x4, 0x4, 0x6, 0x5, 0x6, 0x0);//00446560

	ClearScreen();
}

inline void WINAPI bank_set_page(const int& index)
{
	if (index != 1 && index != 0)
		return;
	typedef int(__cdecl* pfnBankSetPage)(IN SOCKET skt, IN int page);
	pfnBankSetPage BankSetPage = (pfnBankSetPage)MAKEADDR(0x5, 0x0, 0x8, 0x6, 0x3, 0x0);//508630
	int* page_ptr = (int*)MAKEADDR(0xC, 0xD, 0x9, 0x1, 0xD, 0x0);//00CD91D0
	SOCKET skt = *BLUESOCKET;

	*page_ptr = index;
	BankSetPage(skt, index + 1);
}

inline void WINAPI move_item(const int& src, const int& dst)
{
	if (src < 0 || src > 19 || dst < 0 || dst > 19)
		return;

	typedef int(__cdecl* pfnMoveItem)(IN SOCKET skt, IN int src, IN int dst, IN int unknown);
	pfnMoveItem MoveItem = (pfnMoveItem)MAKEADDR(0x5, 0x0, 0x6, 0xD, 0x7, 0x0);//00506D70
	SOCKET skt = *BLUESOCKET;

	MoveItem(skt, src + 0x8, dst + 0x8, -1);
}

void CAsm::bank_move_item(const int& src, const int& dst, const bool& isdeposit)
{
	if (src < 0 || src > 19 || dst < 0 || dst > 19)
		return;

	typedef int(__cdecl* pfnMoveItem)(IN SOCKET skt, IN int x, IN int y, IN int type, IN int id, IN int unknown, IN char* str);
	pfnMoveItem MoveItem = (pfnMoveItem)MAKEADDR(0x5, 0x0, 0x8, 0x4, 0xA, 0x0);//005084A0
	SOCKET skt = *BLUESOCKET;
	char* str = nullptr;
	if (isdeposit)
	{
		std::string s(std::format("I\\z{}\\z{}\\z-1", src + 0x8, dst + 0x64));
		str = (char*)s.c_str();
	}
	else
	{
		std::string s(std::format("I\\z{}\\z{}\\z-1", dst + 0x64, src + 0x8));
		str = (char*)s.c_str();
	}
	int* type = (int*)MAKEADDR(0xC, 0x1, 0xD, 0x2, 0x7, 0xC);//C1D27C//13045;//隨倉13045銀行17107
	int x, y;
	this->get_char_xy(x, y);

	MoveItem(skt, x, y, 339, *type, 0x0, str);
}

inline void WINAPI LogOut()
{
	typedef int(__cdecl* pfnLogOut)(int a);
	const pfnLogOut logout = (pfnLogOut)MAKEADDR(0x5, 0x1, 0x2, 0x3, 0xC, 0x0)/*0x05123C0*/;

	__asm
	{
		lea ecx, ss: [esp + 0xC]
		mov eax, 0x072B9E8
		mov dword ptr ds : [eax] , edx
		mov eax, 0x0401CF0
		call eax
	}
	logout(7);
	if (CAsm::asm_ptr->g_info_ptr.tableinfo->status == 3)
		*(int*)ANIMEADDR = 7;
	CKernel32& ker32 = CKernel32::get_instance();
	ker32.My_shutdown(*BLUESOCKET, SD_BOTH);
	ker32.My_closesocket(*BLUESOCKET);
	*BLUESOCKET = NULL;
}

//說話
inline void WINAPI talk(const std::wstring& wstr)
{
	if (wstr.empty())
		return;
	typedef int(__cdecl* pfnTalk)(char* str, int color);
	const pfnTalk Talk = (pfnTalk)MAKEADDR(0x5, 0x0, 0x2, 0x8, 0x2, 0x0);//0x0502820;
	char str[MAX_PATH] = {};
	int* color = (int*)MAKEADDR(0x6, 0x1, 0x3, 0x9, 0x1, 0x8);
	CRuntime::wchar2char(wstr, str);

	Talk((char*)str, *color);// 0x0613918);
}

//丟棄物品
void CAsm::throw_items(const int index)
{
	if (index < 0 || index > 19)
		return;
	typedef int(__cdecl* pfnThrowItem)(IN int unknown, IN int unknown2, IN int pos);
	pfnThrowItem ThrowItem = (pfnThrowItem)MAKEADDR(0x4, 0x8, 0x4, 0x2, 0xC, 0x0);

	CKernel32& ker32 = CKernel32::get_instance();
	ker32.My_Sleep(300);
	ThrowItem(2, 0, index + 0x8);
	ker32.My_Sleep(300);
}

//物品補血選擇頁面
inline void WINAPI preItemHeal(const int& index)
{
	if (index < 0 || index > 4)
		return;
	typedef int(__cdecl* pfnPreItemHeal)(SOCKET socket, int index);
	const pfnPreItemHeal preItemHeal = (pfnPreItemHeal)MAKEADDR(0x5, 0x0, 0x7, 0xB, 0x9, 0x0);//0x0507B90;
	SOCKET skt = *BLUESOCKET;

	preItemHeal(skt, index);

	*(int*)MAKEADDR(0xC, 0x3, 0x2, 0x5, 0x2, 0x8)/*0x0C32528*/ = index;
}

//物品補血
void CAsm::normal_heal_by_item(const int& pos, const int& target)
{
	if (pos < 0 || pos > 19)
		return;
	if (target < 0 || target > 5)
		return;
	typedef int(__cdecl* pfnItemHeal)(SOCKET socket, int x, int y, int pos, int target);
	const pfnItemHeal ItemHeal = (pfnItemHeal)MAKEADDR(0x5, 0x0, 0x6, 0x9, 0x7, 0x0);//0x0506970;
	SOCKET skt = *BLUESOCKET;
	int x, y;
	this->get_char_xy(x, y);

	ItemHeal(skt, x, y, pos + 0x8, target);
}

//使用物品
void CAsm::use_item(const int& pos)
{
	if (pos < 0 || pos > 19)
		return;
	typedef int(__cdecl* pfnuse_item)(SOCKET socket, int x, int y, int pos, int unknown);
	const pfnuse_item use_item = (pfnuse_item)MAKEADDR(0x5, 0x0, 0x6, 0x8, 0xB, 0x0);// 0x05068B0;
	SOCKET skt = *BLUESOCKET;
	int x, y;
	this->get_char_xy(x, y);

	use_item(skt, x, y, pos + 0x8, 0);
}

//公告
int CAsm::announcement(const std::wstring& wstr, const int& color, const int& size)
{
	typedef int(__cdecl* pfnAnnounce)(IN PCSTR type, IN int color, IN int size);
	const pfnAnnounce Announce = (pfnAnnounce)MAKEADDR(0x4, 0x4, 0x6, 0x4, 0x4, 0x0);//0x0446440;
	char str[MAX_PATH] = {};
	CRuntime::wchar2char(wstr, str);

	Announce(str, color, size);

	return 0;
}

//人物移動
void CAsm::move_char_to(const int& x, const int& y)
{
	if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500)
		return;
	MoveXY m = {};
	m.x = x;
	m.y = y;
	g_movexy = m;
	CAsm::asm_ptr->is_auto_walk = 1;
	std::unique_lock lock(global_auto_walk_mutex);

	__asm
	{
		mov eax, 0x0464470
		call eax
	}

	CAsm::asm_ptr->is_auto_walk = 0;
}

//技能治療選擇頁面 (選擇我方或隊友)
inline void WINAPI preSkillCure(const int& index)
//治療字符串4BYTE = 4173362858
{
	typedef int(__cdecl* pfnPreSkillCure)(SOCKET socket, int index);
	const pfnPreSkillCure PreSkillCure = (pfnPreSkillCure)MAKEADDR(0x5, 0x0, 0x7, 0xB, 0x0, 0x0);// 0x0507B00;
	SOCKET skt = *BLUESOCKET;

	PreSkillCure(skt, index);
	*(int*)MAKEADDR(0xC, 0x4, 0x6, 0x1, 0x0, 0x4)/*0x0C46104*/ = index;
	*(BYTE*)MAKEADDR(0xC, 0x0, 0xC, 0xA, 0x9, 0xF)/*0x0C0CA9F*/ = 0x1;
}

//技能治療 (選擇治療目標)
inline void WINAPI skillCure(const int& skillindex, const int& target)
{
	typedef int(__cdecl* pfnSkillCure)(SOCKET socket, int skillindex, int unknown0, int target, int* unknown1);
	const pfnSkillCure SkillCure = (pfnSkillCure)MAKEADDR(0x5, 0x0, 0x7, 0xD, 0x7, 0x0);// 0x0507D70;
	SOCKET skt = *BLUESOCKET;
	int a = 0;

	SkillCure(skt, skillindex, *(int*)MAKEADDR(0xC, 0x2, 0x2, 0x5, 0xA, 0x4)/*0x0C225A4*/, target, &a);
}

//平時使用技能 (使用技能打開選擇列表)
inline void WINAPI useSkill(const int& pos, const int& level)
{
	typedef int(__cdecl* pfnUseSkill)(SOCKET socket, int pos, int unknown);
	const pfnUseSkill UseSkill = (pfnUseSkill)MAKEADDR(0x5, 0x0, 0x7, 0xA, 0xB, 0x0);//0x0507AB0;
	SOCKET skt = *BLUESOCKET;

	UseSkill(skt, pos, level);
	*(int*)MAKEADDR(0xC, 0xB, 0x0, 0x6, 0x9, 0x4)/*0x0CB0694*/ = pos;
	*(int*)MAKEADDR(0xC, 0x2, 0x2, 0x5, 0xA, 0x4)/*0x0C225A4*/ = 0;
}

inline void WINAPI create_produce_product_screen(const int& skill, const int& type)
{
	typedef int(__cdecl* pfnCreateProduceProductScreen)(IN int skill, IN int type, IN int unknown);
	const pfnCreateProduceProductScreen CreateProduceProductScreen = (pfnCreateProduceProductScreen)MAKEADDR(0x4, 0x7, 0x6, 0x3, 0x1, 0x0);//00476310;

	CreateProduceProductScreen(type, skill, 0);
}

#pragma endregion

#pragma region Auto_action
//分割字符串並依次精確查找物品編號
int CAsm::find_item_by_divided_string(const std::wstring& wstr)
{
	const std::wstring_view words{ wstr };
	const std::wstring_view delim{ L"|" };

	auto view = words
		| std::ranges::views::split(delim)
		| std::ranges::views::transform([](auto&& rng) {
		return std::wstring_view(&*rng.begin(), std::ranges::distance(rng));
			});

	int index = -1;
	for (const std::wstring_view& it : view)
	{
		index = this->find_item_locate_spot((std::wstring)it, true);
		if (index != -1)
		{
			return index;
		}
	}
	return -1;
}

void CAsm::auto_throw_item(int* status)
{
	if (*status != 2)
		return;
	AI* ai = this->g_info_ptr.aiinfo;

	if (!ai->outBattleAutoThrowItemCheckState)
		return;
	if (wcslen(ai->outBattleAutoThrowItemEdit) <= 0)
	{
		return;
	}

	int index = this->find_item_by_divided_string(ai->outBattleAutoThrowItemEdit);
	if (index < 0 || index > 19)
		return;

	this->throw_items(index);
}

//自動吃料
void CAsm::auto_heal(int* status)
{
	if (*status != 2)
		return;

	PetInfo Pet = {};
	CKernel32& ker32 = CKernel32::get_instance();
	std::unique_lock lock(global_auto_walk_mutex);
	AI* ai = this->g_info_ptr.aiinfo;

	//BattleInfo battle = *CAsm::asm_ptr->g_info_ptr.battleinfo;

	CharInfo Char = *CAsm::asm_ptr->g_info_ptr.charinfo;
	if (Char.petpos != -1)
	{
		Pet = *CAsm::asm_ptr->g_info_ptr.petinfo[Char.petpos];
	}

	// 平時物品補血
	if (ai->outBattleItemHealHpCheckState)
	{
		int index = this->find_item_by_divided_string(ai->outBattlePetItemHealHpEdit);
		if (index != -1)
		{
			int hp_percent = get_percent(Char.hp, Char.maxhp);
			if ((*status == 2) && (ai->outBattleCharItemHealHpSpin > 0) && (hp_percent > 0) && (hp_percent < ai->outBattleCharItemHealHpSpin))
			{
				ker32.My_Sleep(100);
				this->use_item(index);
				ker32.My_Sleep(200);
				preItemHeal(0);
				ker32.My_Sleep(100);
				do
				{
					this->normal_heal_by_item(index, 0);
					ker32.My_Sleep(500);
					index = find_item_by_divided_string(ai->outBattlePetItemHealMpEdit);
					if (index == -1)
						break;
					this->get_char_info(Char);
					hp_percent = get_percent(Char.hp, Char.maxhp);
					if (!ai->outBattleItemHealHpCheckState)
						return;
				} while ((*status == 2) && (ai->outBattleCharItemHealHpSpin > 0) && hp_percent < ai->outBattleCharItemHealHpSpin);
				ker32.My_Sleep(200);
				close_screen();
			}

			int pet_hp_percent = get_percent(Pet.hp, Pet.maxhp);

			//std::wstring tmp = std::format(L"pos:{} | in:{} | percent:{} | value:{}", Char.petpos, index, pet_hp_percent, ai.outBattlePetItemHealHpSpin);
			//announcement(tmp, 4, 3);
			if ((*status == 2) && (Char.petpos != -1) && (ai->outBattlePetItemHealHpSpin > 0) && (pet_hp_percent > 0) && (pet_hp_percent < ai->outBattlePetItemHealHpSpin))
			{
				ker32.My_Sleep(100);
				this->use_item(index);
				ker32.My_Sleep(200);
				preItemHeal(0);
				ker32.My_Sleep(200);
				do
				{
					this->normal_heal_by_item(index, Char.petpos + 1);
					ker32.My_Sleep(500);
					index = this->find_item_by_divided_string(ai->outBattlePetItemHealMpEdit);
					if (index == -1)
						break;
					this->get_char_info(Char);
					this->get_pet_info(Char.petpos + 1, Pet);
					pet_hp_percent = get_percent(Pet.hp, Pet.maxhp);
					if (!ai->outBattleItemHealHpCheckState)
						return;
				} while ((*status == 2) && (Char.petpos != -1) && (ai->outBattlePetItemHealHpSpin > 0) && (pet_hp_percent > 0) && (pet_hp_percent < ai->outBattlePetItemHealHpSpin));
				ker32.My_Sleep(200);
				close_screen();
			}
		}
	}
	// 平時物品補魔
	if (ai->outBattleItemHealMpCheckState)
	{
		int index = this->find_item_by_divided_string(ai->outBattlePetItemHealMpEdit);
		if (index != -1)
		{
			int mp_percent = get_percent(Char.mp, Char.maxmp);
			if ((*status == 2) && (ai->outBattleCharItemHealMpSpin > 0) && (mp_percent > 0) && (mp_percent < ai->outBattleCharItemHealMpSpin))
			{
				ker32.My_Sleep(100);
				this->use_item(index);
				ker32.My_Sleep(200);
				preItemHeal(0);
				ker32.My_Sleep(200);
				do
				{
					this->normal_heal_by_item(index, 0);
					ker32.My_Sleep(500);
					index = find_item_by_divided_string(ai->outBattlePetItemHealMpEdit);
					if (index == -1)
						return;
					this->get_char_info(Char);
					mp_percent = get_percent(Char.mp, Char.maxmp);
					if (!ai->outBattleItemHealMpCheckState)
						return;
				} while ((*status == 2) && (ai->outBattleCharItemHealMpSpin > 0) && (mp_percent > 0) && mp_percent < ai->outBattleCharItemHealMpSpin);
				ker32.My_Sleep(200);
				close_screen();
			}

			int pet_mp_percent = get_percent(Char.mp, Char.maxmp);
			if ((*status == 2) && (Char.petpos != -1) && (ai->outBattlePetItemHealMpSpin > 0) && (pet_mp_percent > 0) && (pet_mp_percent < ai->outBattlePetItemHealMpSpin))
			{
				ker32.My_Sleep(100);
				use_item(index);
				ker32.My_Sleep(200);
				preItemHeal(0);
				ker32.My_Sleep(200);
				do
				{
					this->normal_heal_by_item(index, Char.petpos + 1);
					ker32.My_Sleep(500);
					index = this->find_item_by_divided_string(ai->outBattlePetItemHealMpEdit);
					if (index == -1)
						return;
					this->get_char_info(Char);
					this->get_pet_info(Char.petpos + 1, Pet);
					pet_mp_percent = get_percent(Char.mp, Char.maxmp);
					if (!ai->outBattleItemHealMpCheckState)
						return;
				} while ((*status == 2) && (Char.petpos != -1) && (ai->outBattlePetItemHealMpSpin > 0) && (pet_mp_percent > 0) && (pet_mp_percent < ai->outBattlePetItemHealMpSpin));
				ker32.My_Sleep(200);
				close_screen();
			}
		}
	}
}

//自動走路遇敵
void CAsm::auto_walk(CMap Map, const int& dir, const int& len, const int& x, const int& y, int& predir)
{
	if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500 || len <= 0)
		return;
	if (CAsm::asm_ptr->g_info_ptr.tableinfo->status != 2)
		return;
	int wx = 0, wy = 0;
	if (dir == 2)
	{
		const int a[2] = { -len, len };

		wx += x + a[CRuntime::rand(0, 1)];
		wy += y + a[CRuntime::rand(0, 1)];
		this->move_char_to(wx, wy);
	}
	else
	{
		wx = x;
		wy = y;
		if (dir == 0)
		{
			if (predir)
			{
				predir = 0;
				wx += len;
			}
			else
			{
				predir = 1;
				wx -= len;
			}
		}
		else if (dir == 1)
		{
			if (predir)
			{
				predir = 0;
				wy += len;
			}
			else
			{
				predir = 1;
				wy -= len;
			}
		}
		this->move_char_to(wx, wy);
	}
}

//自動輸入驗證碼
int CAsm::anti_test_code(int& anti_code_counter_cache, std::wstring& lastoutput)
{
	anti_code_counter_cache--;
	if (anticode_cache_vec.size() > 0 && anti_code_counter_cache <= 0)
	{
		try
		{
			for (auto it : anticode_cache_vec)
			{
				char str[] = "                                                               \0";
				_snprintf_s((char*)it, sizeof(str), _TRUNCATE, "%s\0", str);
			}
		}
		catch (...)
		{
		}
		anticode_cache_vec.clear();
	}
	const DWORD base = (DWORD)MAKEADDR(0x6, 0x0, 0x0, 0x0, 0x0, 0x0);// 0x600000;

	std::string result = "";

	std::wstring wstr;
	CFormat& format = CFormat::get_instance();
	format.set(FORMATION::ASM_ANTICODEPATTERN, wstr);
	char cstr[MAX_PATH] = {};
	CRuntime::wchar2char(wstr, cstr);
	std::string pattern = cstr;
	std::regex express(pattern);

	for (int i = 0; i < 0x100000; i++)
	{
		DWORD dwTmp = ((DWORD)base + i);
		std::string tmp = (char*)dwTmp;
		if (!tmp.empty() && tmp.contains("[Code]"))
		{
			std::smatch match;
			if (std::regex_search(tmp, match, express))
			{
				anticode_cache_vec.push_back((char*)dwTmp);
				if (result.empty())
					result = match[0];
				char str[] = "                                                               \0";
				_snprintf_s((char*)dwTmp, sizeof(str), _TRUNCATE, "%s\0", str);
			}
		}
	}

	if (anticode_cache_vec.size() > 0)
	{
		if (!result.empty() && anti_code_counter_cache <= 0)
		{
			anti_code_counter_cache = 300;

			std::wstring wwstr;
			format.set(FORMATION::ASM_CLEARDATAFORMAT, wwstr);
			this->announcement(wwstr, 29, 3);
			const char* sstr = result.c_str();
			const std::wstring wcstr(CRuntime::char2wchar(sstr));
			if (wcstr.compare(lastoutput) != 0)
			{
				lastoutput = wcstr;
				talk(wcstr);
			}
			try
			{
				for (auto it : anticode_cache_vec)
				{
					char str[] = "                                                               \0";
					_snprintf_s((char*)it, sizeof(str), _TRUNCATE, "%s\0", str);
				}
			}
			catch (...)
			{
			}
			return true;
		}
	}

	return false;
}
#pragma endregion

#pragma region Login
//自動登入
void CAsm::auto_login(const HWND& hWnd)
{
	//const int s[5] = { *(int*)0x061B9CB ,*(BYTE*)0x0F66AFD, *(int*)0x0F66BE9 , *(int*)0x0F66AAB };
	const int s[4] = {
		*(int*)MAKEADDR(0x6,0x1,0xB,0x9,0xC,0xB),//0x061B9CB
		*(BYTE*)MAKEADDR(0xF,0x6,0x6,0xA,0xF,0xD),//0x0F66AFD
		*(int*)MAKEADDR(0xF,0x6,0x6,0xB,0xE,0x9),//0x0F66BE9
		*(int*)MAKEADDR(0xF,0x6,0x6,0xA,0xA,0xB)//0x0F66AAB
	};

	int* anime = (int*)ANIMEADDR;
	//跳過開頭動畫面
	if (*(int*)ANIMEADDR == 0 && (!s[0] && !s[1] && !s[2]))
	{
		*(int*)MOUSEADDR = 1;
	}

	if (!login.autoLoginCheckState)
		return;

	const int ser = login.server;
	const int subser = login.subserver;
	const int pos = login.pos;

	if (ser == -1 || subser == -1 || pos == -1)
		return;

	const int* addr[4] = {
		(int*)MAKEADDR(0x9, 0x2, 0x7, 0x1, 0xB, 0x8),
		(int*)MAKEADDR(0xF, 0x6, 0x6, 0x5, 0x7, 0x2),
		(int*)MAKEADDR(0x9, 0x2, 0x7, 0x4, 0xA, 0x0),
		(int*)MAKEADDR(0xF, 0x6, 0x6, 0x5, 0x3, 0xF)
	};

	//DWORD* addr[5] = { (DWORD*)0x09271B8 , (DWORD*)0x0F66572 , (DWORD*)0x09274A0 , (DWORD*)0x0F6653F };
	if (wcslen(login.username) >= 6 && wcslen(login.password) >= 6)
	{
		const size_t size = 32;
		char username[size] = {};
		CRuntime::wchar2char(login.username, username);
		char password[size] = {};
		CRuntime::wchar2char(login.password, password);

		_snprintf_s((char*)addr[0], size, _TRUNCATE, "%s\0", username);
		_snprintf_s((char*)addr[1], size, _TRUNCATE, "%s\0", username);
		_snprintf_s((char*)addr[2], size, _TRUNCATE, "%s\0", password);
		_snprintf_s((char*)addr[3], size, _TRUNCATE, "%s\0", password);
	}
	else
	{
		return;
	}

	for (int i = 0; i < 4; i++)
	{
		if (strlen((char*)addr[i]) < 6)
			return;
	}

	if (*(unsigned int*)0x061BAE1 == 4171649211)
	{
		click_to_close_disconnect_msg(hWnd);
	}
	//跳過輸入帳號密碼
	else if ((s[0] == 769) && ((s[1] == 1) || s[1] == 2))
	{
		if ((s[2] == 1375731712) || (s[2] == 1325400064) || ((s[2] == 3388997633) || (s[2] == 3338665984)))
		{
			if ((s[3] == 1728149761) || (s[3] == 1728149760) || ((s[3] >= 2248251392) && (s[3] < 2248251400)) || ((s[3] == 3741446401) || (s[3] == 4261548033)))
			{
				if (!mutexoff)
				{
					//CKernel32& ker32 = CKernel32::get_instance();

					mutexoff = true;
				}
				if (*anime == 3)
					*anime = 4;
			}
		}
	}
	//選伺服器
	else if (((s[0] == 1) && (s[1] == 0) && (s[3] == 100352)) || ((s[0] == 2) && (s[1] == 0) && (s[3] == 131072)))
	{
		if ((s[2] >= 1124073472) || (s[2] <= -1946157056) || (s[2] == 67108865))
			click_main_server(hWnd, ser, subser);
	}
	//選分流
	else if (((s[0] == 1) && (s[1] == 1) && (s[3] == 100352)) || ((s[0] == 2) && (s[1] == 1) && (s[3] == 131072)))
	{
		if ((s[2] == 1811939328) || (s[2] == 1124073472) || (s[2] <= 0) || (s[2] >= 1157627904) || ((s[2] == 0) || (s[2] == 3825205249) || (s[2] == 3137339392)))
			click_sub_server(hWnd, subser, ser);
	}
	//選腳色
	else if ((s[0] == 591872) && (s[1] == 0) && ((s[3] == 82176) || (s[3] == 112896)))
	{
		//wchar_t a[MAX_PATH] = {};
		//_i64tow(s[2], a, 10);
		if ((s[2] == 855638017) || (s[2] == 1090519041) || (s[2] == 1308622848) || (*(short*)0x0F66BE9 == 2) || (*(short*)0x0F66BE9 == 1))
		{
			click_char(hWnd, pos);
		}
	}
}

//選擇主分流
void CAsm::click_main_server(const HWND& hWnd, const int& num, const int& sub)
{
	int col = 0;
	int setser = 0;
	int snum = 0;
	if (num == 0)//櫻之舞
	{
		col = 0;
		snum = 0;
		setser += 160 + sub;
	}
	else if (num == 1)//露比
	{
		col = 0;
		snum = 1;
		setser += 420 + sub;
	}
	else if (num == 2)//歌姬
	{
		col = 0;
		snum = 2;
		setser += 140 + sub;
	}
	else if (num == 3)//卡蓮
	{
		col = 1;
		snum = 0;
		setser += 0 + sub;
	}
	else if (num == 4)//獅子
	{
		col = 1;
		snum = 1;
		setser += 80 + sub;
	}
	else if (num == 5)//雙子
	{
		col = 1;
		snum = 2;
		setser += 40 + sub;
	}
	else if (num == 6)//天坪
	{
		col = 0;
		snum = 3;
		setser += 120 + sub;
	}
	else if (num == 7)//心美
	{
		col = 0;
		snum = 4;
		setser += 480 + sub;
	}
	*(int*)MAKEADDR(0x5, 0x7, 0x9, 0xE, 0xC, 0x4)/*0x0579EC4*/ = setser;
	const int isHd = is_hd_mode();
	int X = 155 + (isHd * 160);
	int Y = 120 + (isHd * 120);
	X += (100 * col);
	Y += (30 * snum);
	CKernel32& ker32 = CKernel32::get_instance();
	ker32.My_PostMessageW(hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(X, Y));
	*(int*)MOUSEADDR = 1;
}

//選擇子分流
void CAsm::click_sub_server(const HWND& hWnd, const int& ser, const int& num)
{
	const int isHd = is_hd_mode();
	CKernel32& ker32 = CKernel32::get_instance();
	const int temp[6] = { 8,21,7,0,4,2 };
	for (int i = 0; i < 6; i++)
	{
		if (ser == i)
		{
			if (*(int*)SUBSERVERPAGEADDR != temp[i])
			{
				ker32.My_PostMessageW(hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(198 + (isHd * 160), 348 + (isHd * 120)));
				*(int*)MOUSEADDR = 1;
			}
		}
	}

	int col = 0;
	int snum = 0;
	if (num == 5)
	{
		col = 1;
		snum = 0;
	}
	else if (num == 6)
	{
		col = 1;
		snum = 1;
	}
	else if (num == 7)
	{
		col = 1;
		snum = 2;
	}
	else
		snum = num;
	int X = 155 + (isHd * 160);
	int Y = 120 + (isHd * 120);
	X += (100 * col);
	Y += (30 * snum);

	ker32.My_PostMessageW(hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(X, Y));
	*(int*)MAKEADDR(0x9, 0x2, 0x7, 0x6, 0x4, 0x4)/*0x0927644*/ = ser;
	ker32.My_PostMessageW(hWnd, WM_LBUTTONDBLCLK, 0, MAKELPARAM(X, Y));
}

//選擇腳色欄位
void CAsm::click_char(const HWND& hWnd, const int& num)
{
	const int isHd = is_hd_mode();
	int X = 120 + (isHd * 175);
	int Y = 325 + (isHd * 120);
	if (num == 1)
	{
		X = 425 + (isHd * 175);
		Y = 325 + (isHd * 120);
	}
	CKernel32& ker32 = CKernel32::get_instance();
	ker32.My_PostMessageW(hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(X, Y));
	ker32.My_PostMessageW(hWnd, WM_LBUTTONDBLCLK, 0, MAKELPARAM(X, Y));
}

void CAsm::click_to_close_disconnect_msg(const HWND& hWnd)
{
	int* s[3] = {
		(int*)MAKEADDR(0xd, 0x1, 0x5, 0x7, 0x1, 0x8),//0x0D15718
		(int*)MAKEADDR(0x7, 0x2, 0xb, 0xa, 0x2, 0x4),//0x072BA24
		(int*)MAKEADDR(0xc, 0x0, 0xc, 0xb, 0xd, 0x4)// 0x0C0CBD4
	};
	for (int i = 0; i < 3; i++)
		*s[i] = 0;
	const int isHd = is_hd_mode();
	int X = 315 + (isHd * 175);
	int Y = 275 + (isHd * 120);
	CKernel32& ker32 = CKernel32::get_instance();
	ker32.My_PostMessageW(hWnd, WM_MOUSEMOVE, 0, MAKELPARAM(X, Y));
	*(int*)MOUSEADDR = 1;
}

int CAsm::is_hd_mode()
{
	const int* pWidth = (int*)MAKEADDR(0x5, 0x4, 0xC, 0xC, 0x8, 0xC);// 0x54cc8c;
	int width = *pWidth;
	int height = *(pWidth + 1);

	if (width >= 960 && height >= 720)
		return 1;
	return 0;
}

int CAsm::is_battle_screen_ready_for_hotkey()
{
	if (*(int*)ANIMEADDR != 4)
		return FALSE;
	if (CAsm::battle_screen_type_cache == -1)
		return FALSE;
	unsigned int data[5] = {};
	CRSA& rsa = CRSA::get_instance();
	memmove(data, CAsm::asm_ptr->global_status_caches, sizeof(data));
	if ((data[0] ^ rsa.getXorKey()) != 3)
		return FALSE;

	return TRUE;
}
#pragma endregion

#pragma region AI
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//人物普攻 H|%X
//人物技能 S|%X|%X|%X
//人物物品 I|%X|%X
//寵物技能 W|%X|%X
//人物逃跑 E
//人物防禦 G
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//攻擊
void CAsm::battle_attack(const int& target)
{
	constexpr wchar_t c[9] = { L"{}|{:x}\0" };
	const std::wstring sstr(std::format(c, L"H", target));//normal attack (H|tar)
	this->battle_send_command(sstr);
}
//防禦
void CAsm::battle_defense()
{
	constexpr wchar_t c[4] = { L"{}\0" };
	const std::wstring sstr(std::format(c, L"G"));
	this->battle_send_command(sstr);
}
//逃跑
void CAsm::battle_escape()
{
	constexpr wchar_t c[4] = { L"{}\0" };
	const std::wstring sstr(std::format(c, L"E"));
	this->battle_send_command(sstr);
}
//技能
void CAsm::battle_skill(const int& skill, const int& subskill, const int& target)
{
	constexpr wchar_t c[19] = { L"{}|{:x}|{:x}|{:x}\0" };
	const std::wstring sstr(std::format(c, L"S", skill, subskill, target));
	this->battle_send_command(sstr);
}
//道具
void CAsm::battle_item(const int& index, const int& target)
{
	constexpr wchar_t c[19] = { L"{}|{:x}|{:x}\0" };
	const std::wstring sstr(std::format(c, L"I", index + 0x8, target));
	this->battle_send_command(sstr);
}
//登出
inline void WINAPI basicLogOut()
{
	LogOut();
}

//根據腳色名稱獲取在腳色戰場上的位置
inline int WINAPI battle_get_char_real_position(const BattleInfo& bt, const char* charname)
{
	const std::string str = charname;

	for (int i = 0; i < 20; i++)
	{
		if (bt.exist[i] && (bt.maxhp[i] > 0) && (str.compare(bt.name[i]) == 0))
		{
			return i;
		}
	}
	return -1;
}

//根據玩家位置尋找寵物在戰場上的位置
inline int WINAPI battle_get_pet_real_position(const int& charposition)
{
	if (((charposition >= 0) && (charposition < 5)) || ((charposition >= 10) && (charposition < 15)))//人物在後排
	{
		return charposition + 5;
	}
	else if (((charposition >= 5) && (charposition < 10)) || ((charposition >= 15) && (charposition <= 20)))//人物在前排
	{
		return charposition - 5;
	}
	return -1;
}

//根據腳色位置獲取我方和敵方在戰場上的位置範圍
inline int WINAPI getMinMax(const int& charposition, int& a_min, int& a_max, int& e_min, int& e_max)
{
	if ((charposition >= 0) && (charposition <= 10))//人物在右側
	{
		a_min = 0;
		a_max = 10;
		e_min = 10;
		e_max = 20;
		return 0;
	}
	else if ((charposition >= 10) && (charposition <= 20))//人物在左側
	{
		a_min = 10;
		a_max = 20;
		e_min = 0;
		e_max = 10;
		return 1;
	}
	return -1;
}

bool CAsm::battle_cmp_allies(const int& value, const BattleInfo& bt, const COMPARE& type, int& target, int& healtype, int charposition, int petposition)
{
	int i = 0;
	std::vector<unsigned int> v;
	bool bret = false;
	switch (type)
	{
	case COMPARE::Self://自己和戰寵
	{
		for (i = this->allies_min; i < this->allies_max; i++)
		{
			//如果戰場目標存在 且 血量大於0 且 百分比血量 小於 value 且 value > 0 且 玩家位置 等於 i 或 寵物位置 等於 i
			if (bt.exist[i] && (bt.hp[i] > 0) && (get_percent(bt.hp[i], bt.maxhp[i]) <= value) && (value > 0) && ((charposition == i) || (petposition == i)))
			{
				//將符合低血量百分比條件 且為 自己或戰寵的血量值壓入vector
				v.push_back(bt.hp[i]);
				bret = true;//至少一個符合條件則返回真
				healtype = 1;
			}
			else
				v.push_back(0x7fff);//否則壓入一個大數字
		}
		break;
	}
	case COMPARE::Allies://隊友
	{
		for (i = this->allies_min; i < this->allies_max; i++)
		{
			if (bt.exist[i] && (bt.hp[i] > 0) && (get_percent(bt.hp[i], bt.maxhp[i]) <= value) && (value > 0))
			{
				//將符合低血量百分比條件 且為 隊友的血量值壓入vector
				v.push_back(bt.hp[i]);
				bret = true;//至少一個符合條件則返回真
				healtype = -1;
			}
			else
				v.push_back(0x7fff);//否則壓入一個大數字
		}
		break;
	}
	}

	if (bret)
	{
		//取最小值
		get_lowest(v, target);
	}
	else
		target = -1;
	return bret;
}

//隊友是否死亡
bool CAsm::is_allies_dead(const int& selected, const BattleInfo& bt, int& target, int petposition)
{
	int i = 0;

	switch (selected)
	{
	case 0:
	{
		for (i = this->allies_min; i < this->allies_max; i++)
		{
			//戰寵是否死亡
			if (bt.exist[i] && (bt.maxhp[i] > 0) && (bt.hp[i] == 0) && (i == petposition))
			{
				target = i;
				return true;
			}
		}
		break;
	}
	case 1:
	{
		for (i = this->allies_min; i < this->allies_max; i++)
		{
			//任意隊友是否死亡
			if (bt.exist[i] && (bt.maxhp[i] > 0) && (bt.hp[i] == 0))
			{
				target = i;
				return true;
			}
		}
		break;
	}
	}

	target = -1;
	return false;
}

//根據選擇目標類型 修正目標編號
void CAsm::battle_skill_target_correction(const BattleInfo& bt, const int& act, const int& selected, int& target, const int& petposition)
{
	int i = 0, j = 0;
	std::vector<unsigned int> vpos;//位置暫存
	std::vector<unsigned int> vhp;//全個體血量暫存
	std::vector<unsigned int> vmp;//全個體魔量暫存
	std::vector<unsigned int> vThp;//T型血量暫存
	std::vector<unsigned int> vTmp;//T型血量暫存
	//vpos.resize(20);
	//vhp.resize(20);
	//vmp.resize(20);
	//vThp.resize(20);
	//vTmp.resize(20);

	if (this->enemy_min == 10 && this->enemy_max == 20)//正位 敵方在左側 10-19
	{
		for (i = 0; i < this->enemy_max; i++)//i < 20
		{
			//我方
			if (i < this->allies_max)//i < 10
			{
				vhp.push_back(0x7fff);
				vmp.push_back(0x7fff);
				vThp.push_back(100);
				vTmp.push_back(100);
				continue;
			}
			//敵人
			if (!bt.exist[i] || (bt.hp[i] == 0))
			{
				vhp.push_back(0);
				vmp.push_back(0);
				vThp.push_back(0);
				vTmp.push_back(0);
				continue;
			}

			//以下為存在的敵人

			vpos.push_back(i);//存在的敵人位置
			vhp.push_back(bt.hp[i]);
			vmp.push_back(bt.mp[i]);

			int presize = 3;
			int prestart = 0;
			if (this->t_target_position[i][3] != -1)
			{
				presize = 4;
				prestart = 1;
			}
			int tmpThp = 0;
			int tmpTmp = 0;
			int k = 0;
			for (j = prestart; j < presize; j++)
			{
				if ((i >= 20) || (j >= 4))
					continue;
				int tmpTar = this->t_target_position[i][j];
				if ((tmpTar <= 0) || (tmpTar >= 20))
					continue;
				if (bt.hp[tmpTar] <= 0)
					continue;
				tmpThp += get_percent(bt.hp[tmpTar], bt.maxhp[tmpTar]);
				tmpTmp += get_percent(bt.mp[tmpTar], bt.maxmp[tmpTar]);
				++k;
			}
			if (k > 0)
			{
				vThp.push_back(tmpThp / k);
				vTmp.push_back(tmpTmp / k);
			}
		}
	}
	else//逆位 敵方在右側 1-9
	{
		for (i = 0; i < this->allies_max; i++)//i<20
		{
			//我方
			if (i >= this->allies_min)//i>= 10
			{
				vhp.push_back(0x7fff);
				vmp.push_back(0x7fff);
				vThp.push_back(100);
				vTmp.push_back(100);
				continue;
			}
			//敵人
			if (!bt.exist[i] || (bt.hp[i] == 0))
			{
				vhp.push_back(0);
				vmp.push_back(0);
				vThp.push_back(0);
				vTmp.push_back(0);
				continue;
			}

			//以下為存在的敵人
			vpos.push_back(i);//存在的敵人位置
			vhp.push_back(bt.hp[i]);
			vmp.push_back(bt.mp[i]);

			int presize = 3;
			int prestart = 0;
			if (this->t_target_position[i][3] != -1)
			{
				presize = 4;
				prestart = 1;
			}
			int tmpThp = 0;
			int tmpTmp = 0;
			int k = 0;
			for (j = prestart; j < presize; j++)
			{
				if ((i >= 20) || (j >= 4))
					continue;
				int tmpTar = this->t_target_position[i][j];
				if ((tmpTar <= 0) || (tmpTar >= 20))
					continue;
				if (bt.hp[tmpTar] <= 0)
					continue;
				tmpThp += get_percent(bt.hp[tmpTar], bt.maxhp[tmpTar]);
				tmpTmp += get_percent(bt.mp[tmpTar], bt.maxmp[tmpTar]);
				++k;
			}
			if (k > 0)
			{
				vThp.push_back(tmpThp / k);
				vTmp.push_back(tmpTmp / k);
			}
		}
	}

	//敵方順序單體目標
	if (selected == 0)
	{
		target = get_enemy_spot_num_order(bt);
	}
	//敵方隨機單體目標
	else if ((selected == 1) && (vpos.size() > 0))
	{
		std::vector<unsigned int> pos;
		for (const auto& it : vpos)
		{
			if (it >= 0 && it <= 20)
				pos.push_back(it);
		}
		target = (int)pos.at(CRuntime::rand(0, pos.size() - (unsigned int)1));
		return;
	}
	//敵方單體最低血量
	else if (selected == 2)
	{
		get_lowest(vhp, target);
		return;
	}
	//敵方單體最低魔量
	else if (selected == 3)
	{
		get_lowest(vhp, target);
		return;
	}
	//敵方T型最低血量
	else if (selected == 4)
	{
		get_lowest(vThp, target);
		if (act != 0)
			target += 20;
		return;
	}
	//敵方T型最低魔量
	else if (selected == 5)
	{
		get_lowest(vTmp, target);
		if (act != 0)
			target += 20;
		return;
	}
	//敵方優先前排
	else if (selected == 6)
	{
		// std::vector<int> vpos 查找 是否包含 std::vector<int> back_enemies 相同數值

		for (const int& it : vpos)
		{
			if (std::find(front_enemies.begin(), front_enemies.end(), it) != front_enemies.end())
			{
				target = it;
				return;
			}
		}
		target = get_enemy_spot_num_order(bt);

		return;
	}
	//敵方優先後排
	else if (selected == 7)
	{
		// std::vector<int> vpos 查找 是否包含 std::vector<int> front_enemies 相同數值
		for (const int& it : vpos)
		{
			if (std::find(back_enemies.begin(), back_enemies.end(), it) != back_enemies.end())
			{
				target = it;
				return;
			}
		}

		target = get_enemy_spot_num_order(bt);

		return;
	}
	//敵方全體
	else if (selected == 8)
	{
		if (act != 0)
			target = 41;
		return;
	}
	//戰寵
	else if ((selected == 9) && (petposition != -1))
	{
		target = petposition;
		return;
	}
	//戰寵T型
	else if ((selected == 9) && (petposition != -1))
	{
		target = petposition + 20;
		return;
	}
	//我方全體
	else if (selected == 9)
	{
		target = 40;
		return;
	}
}

//治療我方目標補正
void CAsm::battle_healing_target_correction(const BattleInfo& bt, const int& selected, const int& value, const int& healtype, int& target)
{
	if (selected == 0)//0為單體不需要補正
	{
		return;
	}
	else if ((selected > 0) && (healtype != -1))//healtype 不為-1代表指定自己或戰寵
	{
		if ((selected == 1) && (target != -1))//T型
			target += 20;
		else if ((selected == 2) && (target != -1))//全體
			target = 40;

		return;
	}

	int i, j;
	int AllHp = 0;
	int AllMaxHp = 0;
	int Allget_percent = 0;

	if (this->allies_min == 10 && this->allies_max == 20)//逆位 10~19
	{
		for (i = 0; i < this->allies_max; i++)//i<20
		{
			//敵人
			if (i < this->enemy_max)//i<10為敵人
			{
				continue;
			}
			if (!bt.exist || (bt.hp[i] <= 0))
				continue;
			//我方
			AllHp += bt.hp[i];//加總我方血量
			AllMaxHp += bt.hp[i];//加總我方血量最大值
			int presize = 3;
			int prestart = 0;
			if (t_target_position[i][3] != -1)
			{
				presize = 4;
				prestart = 1;
			}
			int tmpThp = 0;
			int k = 0;
			for (j = prestart; j < presize; j++)//加總T型血量百分比
			{
				if (i >= 20 || j >= 4)
					continue;
				int tmpTar = this->t_target_position[i][j];
				if (tmpTar <= 0 || tmpTar >= 20)
					continue;
				if (bt.hp[tmpTar] <= 0)
					continue;
				tmpThp += get_percent(bt.hp[tmpTar], bt.maxhp[tmpTar]);
				++k;
			}
			//計算T型區域平均血量是否低於value
			if (((tmpThp / k) <= value) && (value > 0))
			{
				target = i;
				break;
			}
		}
		Allget_percent = get_percent(AllHp, AllMaxHp);//我方總血量百分比
	}
	else//正位 0~10
	{
		for (i = 0; i < this->enemy_max; i++)
		{
			//敵人
			if (i >= this->enemy_min)
			{
				continue;
			}
			if (!bt.exist || (bt.hp[i] <= 0))
				continue;
			//我方
			AllHp += bt.hp[i];//加總我方血量
			AllMaxHp += bt.hp[i];
			int presize = 3;
			int prestart = 0;
			if (this->t_target_position[i][3] != -1)
			{
				presize = 4;
				prestart = 1;
			}
			int tmpThp = 0;
			int k = 0;
			for (j = prestart; j < presize; j++)//加總T型血量百分比
			{
				if (i >= 20 || j >= 4)
					continue;
				int tmpTar = this->t_target_position[i][j];
				if ((tmpTar <= 0) || (tmpTar >= 20))
					continue;
				if (bt.hp[tmpTar] <= 0)
					continue;
				tmpThp += get_percent(bt.hp[tmpTar], bt.maxhp[tmpTar]);
				++k;
			}
			if (((tmpThp / k) <= value) && (value > 0) && (selected == 1))
			{
				target = i;
				break;
			}
		}
		Allget_percent = get_percent(AllHp, AllMaxHp);
	}

	if ((selected == 1) && (target >= 0))
	{
		target += 20;
	}
	else if ((selected == 2) && (target >= 0))
	{
		if ((Allget_percent <= value) && (value > 0))
		{
			target = 40;
		}
		else
			target = -1;
	}

	target = -1;
}

//檢查技能是否不為被動或生產技能
bool CAsm::is_skill_non_passive(const std::wstring& wcstr)
{
	//從vector<std::wstring>char_passive_skill_vec中查找是否有同名稱技能 如果找到相同技能返回false
	for (auto& it : this->char_passive_skill_vec)
	{
		if (it == wcstr)
			return false;
	}
	return true;
}

int CAsm::get_enemy_spot_num_order(const BattleInfo& bt)
{
	if ((this->enemy_min == 10) && (this->enemy_max == 20))
	{
		//find index match to std::vector<int> enemies_spot_order
		int size = this->enemies_spot_order.size();
		for (auto j = 0; j < size; j++)//從正位敵人查找
		{
			if (!bt.exist[this->enemies_spot_order.at(j)] || bt.hp[this->enemies_spot_order.at(j)] <= 0)
				continue;
			send_log(L"posstive way:" + std::to_wstring(this->enemies_spot_order.at(j)));
			return this->enemies_spot_order.at(j);
		}
	}
	else if ((this->enemy_min == 0) && (this->enemy_max == 10))
	{
		//else find index match to std::vector<int> allies_spot_order
		int size = this->allies_spot_order.size();
		for (auto j = 0; j < size; j++)//從逆位敵人查找
		{
			if (!bt.exist[this->allies_spot_order.at(j)] || bt.hp[this->allies_spot_order.at(j)] <= 0)
				continue;
			//announcement();
			send_log(L"nagtive way:" + std::to_wstring(this->allies_spot_order.at(j)));
			return this->allies_spot_order.at(j);
		}
	}
	return -1;
}
int CAsm::get_allies_spot_num_order(const BattleInfo& bt)
{
	std::ignore = bt;
	//if (allies_min == 10 && allies_max == 20)
	//{
	//	for (int i = 10; i < 20; i++)//敵人範圍 0~9 10~19
	//	{
	//		if (!bt.exist || bt.hp[i] <= 0)
	//			continue;
	//		//find index match to std::vector<int> enemies_spot_order
	//		for (int j = 0; j < (int)enemies_spot_order.size(); j++)//從正位敵人查找
	//		{
	//			if (enemies_spot_order[j] == i)
	//				return j;
	//		}
	//	}
	//}
	//else
	//{
	//	for (int i = 0; i < 10; i++)//敵人範圍 0~9 10~19
	//	{
	//		if (!bt.exist || bt.hp[i] <= 0)
	//			continue;
	//		//else find index match to std::vector<int> allies_spot_order
	//		for (int j = 0; j < (int)allies_spot_order.size(); j++)//從逆位敵人查找
	//		{
	//			if (allies_spot_order[j] == i)
	//				return j;
	//		}
	//	}
	//}
	return 0;
}

//選擇動作類型
int CAsm::battle_char_select_action_type(const int& index, const int& skill, const int& subskill, const int& item, const int& target)
{
	const CharInfo Char = *CAsm::asm_ptr->g_info_ptr.charinfo;
	const BattleInfo battle = *CAsm::asm_ptr->g_info_ptr.battleinfo;
	//const int charposition = battle_get_char_real_position(battle, Char.name);//自身在戰場上的編號
	switch (index)
	{
	case 0://攻擊
	{
		int tar = target;
		if (tar == -1)
		{
			tar = this->get_enemy_spot_num_order(battle);

			send_log(L"error:target replace " + std::to_wstring(tar));
			//this->announcement(a, 4, 3);
		}
		if (tar != -1)
		{
			send_log(L"char basic attack " + std::to_wstring(tar));
			//this->announcement(a.c_str(), 4, 3);
			this->battle_attack(tar);
			return 1;
		}
		break;
	}
	case 1://防禦
	{
		send_log(L"char defense");
		//this->announcement(L"char d", 4, 3);
		this->battle_defense();
		return 1;
	}
	case 2://逃跑
	{
		send_log(L"char escape");
		//this->announcement(L"char r", 4, 3);
		this->battle_escape();
		return 1;
	}
	case 3://技能
	{
		char name[MAX_PATH] = {};
		GetSkill(skill, name, MAX_PATH);
		const std::wstring wname(CRuntime::char2wchar(name));

		if ((skill != -1 && subskill != -1 && target != -1) && this->is_skill_non_passive(wname))
		{
			send_log(L"char skill " + std::to_wstring(target) + L" main " + std::to_wstring(skill) + L" sub " + std::to_wstring(subskill));
			//this->announcement(k, 4, 3);
			this->battle_skill(skill, subskill, target);
			return 1;
		}
		else
		{
			send_log(L"error:skill failed switch to basic attack");
			//this->announcement(k, 4, 3);
			int tar = target;
			if (tar == -1)
			{
				tar = this->get_enemy_spot_num_order(battle);

				send_log(L"error:target replace " + std::to_wstring(tar));
				//this->announcement(a, 4, 3);
			}
			if (tar != -1)
			{
				this->battle_attack(tar);
				return 1;
			}
		}
		break;
	}
	case 4://道具
	{
		if ((item != -1 && target != -1) && GetItemExist(item))
		{
			send_log(L"char item " + std::to_wstring(target) + L" ipos" + std::to_wstring(item));
			//this->announcement(a, 4, 3);
			this->battle_item(item, target);
			return 1;
		}
		else
		{
			send_log(L"error:item failed switch to basic attack");
			//this->announcement(k, 4, 3);
			int tar = target;
			if (tar == -1)
			{
				tar = get_enemy_spot_num_order(battle);

				send_log(L"error:target replace to " + std::to_wstring(tar));
				//this->announcement(a, 4, 3);
			}
			if (tar != -1)
			{
				this->battle_attack(tar);
				return 1;
			}
		}
		break;
	}
	case 5://登出
	{
		send_log(L"char logout");
		basicLogOut();
		return 1;
	}
	}
	return 0;
}

int CAsm::battle_pet_select_action_type(const int& index, const int& skill, const int& target)
{
	const CharInfo Char = *CAsm::asm_ptr->g_info_ptr.charinfo;
	const BattleInfo battle = *CAsm::asm_ptr->g_info_ptr.battleinfo;
	//const int charposition = battle_get_char_real_position(battle, Char.name);//自身在戰場上的編號
	std::wstring sstr;
	switch (index)
	{
	case 0:
	{
		constexpr wchar_t format[14] = { L"{}|{:x}|{:x}\0" };
		if ((skill == -1) || (target == -1))
		{
			send_log(L"error:pet act switch target");
			//this->announcement(k.c_str(), 4, 3);
			sstr = std::format(format, 'W', 0, this->get_enemy_spot_num_order(battle));
		}
		else
		{
			send_log(L"pet " + std::to_wstring(index) + L" target " + std::to_wstring(target) + L" skill " + std::to_wstring(skill));
			//announcement(k, 4, 3);
			sstr = std::format(format, 'W', skill, target);
		}
		this->battle_send_command(sstr);
		return 1;
	}

	case 1:
	{
		send_log(L"pet escape");
		//announcement(L"pet r", 4, 3);
		constexpr wchar_t a[14] = { L"{}\0" };
		sstr = std::format(a, 'E');
		this->battle_send_command(sstr);
		return 1;
	}
	case 2:
	{
		send_log(L"pet logout");
		LogOut();
		return 1;
	}
	}
	return 0;
}

int CAsm::find_item_locate_spot(const std::wstring& wstr, const bool& en)
{
	if (wstr.size() <= 0)
		return 0;
	for (int i = 0; i < 20; i++)
	{
		if (!GetItemExist(i))
			continue;
		char item[MAX_PATH] = {};
		GetItemName(i, item, MAX_PATH);
		const std::wstring tmp(CRuntime::char2wchar(item));
		if (tmp.empty())
			continue;
		if (tmp.size() > 0)
		{
			if (!en && tmp.contains(wstr))
				return i;
			else if (en && (tmp.compare(wstr) == 0))
				return i;
		}
	}
	return -1;
}

const std::vector<int> CAsm::find_item_locate_spot(const std::wstring& wstr, int nothing, const bool& en)
{
	std::ignore = nothing;
	std::vector<int> v;
	for (int i = 0; i < 20; i++)
	{
		if (!GetItemExist(i))
			continue;
		char item[MAX_PATH] = {};
		GetItemName(i, item, MAX_PATH);
		const std::wstring tmp(CRuntime::char2wchar(item));
		if (tmp.empty())
			continue;
		if ((tmp.size() > 0) && (wstr.size() > 0))
		{
			if (!en && tmp.contains(wstr))
				v.push_back(i);
			else if (en && (tmp.compare(wstr) == 0))
				v.push_back(i);
		}
	}
	return v;
}

const std::vector<int> CAsm::find_bank_item_locate_spot(const std::wstring& wstr, int nothing, const bool& en)
{
	if (wstr.size() <= 0)
		return std::vector<int>();

	std::ignore = nothing;
	std::vector<int> v;
	for (int i = 0; i < 20; i++)
	{
		if (!GetBankItemExist(i))
			continue;
		std::wstring tmp;
		if (!GetBankItemName(i, tmp))
			continue;
		if (tmp.size() > 0)
		{
			if (!en && tmp.contains(wstr))
			{
				v.push_back(i);
			}
			else if (en && (tmp.compare(wstr) == 0))
			{
				v.push_back(i);
			}
		}
	}
	return v;
}

int CAsm::get_item_pos_by_id(const int& id)
{
	for (int i = 0; i < 20; i++)
	{
		const int ntmp = GetItemId(i);
		if (ntmp == id && id > 0)
		{
			return ntmp;
		}
	}
	return -1;
}

int CAsm::battle_get_max_level(const int& skill)
{
	int n = 0;
	for (int i = 0; i < 12; i++)
	{
		char a[MAX_PATH] = {};
		GetSubSkill(skill, i, a, MAX_PATH);
		if (strlen(a) > 1)
			n++;
	}
	return n;
}

void CAsm::battle_auto_set_skill_level(const int& skill, int& subskill)
{
	const int enemyamt = CAsm::asm_ptr->g_info_ptr.battleinfo->totlaEnemy;
	const int maxlevel = this->battle_get_max_level(skill);

	int ntmp = subskill;

	char a[MAX_PATH] = {};
	GetSkill(skill, a, MAX_PATH);
	std::wstring str(CRuntime::char2wchar(a));
	if (str.contains(L"亂射"))
	{
		ntmp = enemyamt;
	}
	else if (str.contains(L"氣功彈") || str.contains(L"刀刃亂舞"))
	{
		if (enemyamt >= 7)
			ntmp = 11;
		else if (enemyamt >= 5)
			ntmp = 8;
		else if (enemyamt >= 3)
			ntmp = 5;
		else if (enemyamt == 2)
			ntmp = 2;
		else if (enemyamt == 1)
			ntmp = 1;
		else
			ntmp = enemyamt;
	}
	else
	{
		if (enemyamt >= 7)
			ntmp = 11;
		else if (enemyamt >= 5)
			ntmp = 10;
		else if (enemyamt >= 4)
			ntmp = 8;
		else if (enemyamt >= 3)
			ntmp = 5;
		else if (enemyamt >= 2)
			ntmp = 3;
		else if (enemyamt >= 1)
			ntmp = 1;
		else
			ntmp = enemyamt;
	}

	if (enemyamt > maxlevel)
		ntmp = maxlevel;

	if (ntmp < 0)
	{
		ntmp = 0;
	}
	else
	{
		ntmp--;
	}
	if (ntmp >= 0)
	{
		subskill = ntmp;
	}
}

int CAsm::battle_last_correction(const AI& ai, const Action& acttype, const int& target)
{
	switch (acttype)
	{
	case Action::BT_NotUsed:
	{
		break;
	}
	case Action::BT_EnemyLowerThan:
	{
		send_log(L"BT_EnemyLowerThan");
		//this->announcement(L"BT_EnemyLowerThan", 4, 3);
		return this->battle_char_select_action_type(ai.lowerEnemyAct, ai.lowerEnemySkillbox, ai.lowerEnemySubSkillbox, get_item_pos_by_id(ai.lowerEnemyItembox), target);
	}
	case Action::BT_EnemyHigherThan:
	{
		send_log(L"BT_EnemyHigherThan");
		//this->announcement(L"BT_EnemyHigherThan:", 4, 3);
		int subskill = ai.higherEnemySubSkillbox;
		if (ai.autoSwitchSkillLevelCheckState)
			this->battle_auto_set_skill_level(ai.higherEnemySkillbox, subskill);
		return this->battle_char_select_action_type(ai.higherEnemyAct, ai.higherEnemySkillbox, subskill, get_item_pos_by_id(ai.higherEnemyItembox), target);
	}
	case Action::BT_CharMpLowerThan:
	{
		send_log(L"BT_CharMpLowerThan");
		//this->announcement(L"BT_CharMpLowerThan", 4, 3);
		return this->battle_char_select_action_type(ai.lowMpAct, ai.lowMpSkillbox, ai.lowMpSubSkillbox, get_item_pos_by_id(ai.lowMpItembox), target);
	}
	case Action::BT_Normal:
	{
		send_log(L"normal");
		//this->announcement(L"normal", 4, 3);
		int subskill = ai.normalSubSkillbox;
		if (ai.autoSwitchSkillLevelCheckState && !ai.lockSkillLvlToHighestCheckState)
		{
			//根據敵人數量自動切換技能等級
			this->battle_auto_set_skill_level(ai.normalSkillbox, subskill);
			if (subskill < 0 || subskill > 11)
				subskill = ai.normalSubSkillbox;
		}
		else if (!ai.autoSwitchSkillLevelCheckState && ai.lockSkillLvlToHighestCheckState)
		{
			//鎖定技能等級為最高
			subskill = this->battle_get_max_level(ai.normalSkillbox) - 1;
		}

		return this->battle_char_select_action_type(ai.normalAct, ai.normalSkillbox, subskill, this->get_item_pos_by_id(ai.normalItembox), target);
	}
	case Action::BT_SecondRound:
	{
		send_log(L"second");
		//this->announcement(L"second", 4, 3);
		return this->battle_char_select_action_type(ai.secondAct, ai.secondSkillbox, ai.secondSubSkillbox, this->get_item_pos_by_id(ai.secondSubSkillbox), target);
	}
	case Action::BT_HealBySkill_0:
	{
		send_log(L"BT_HealBySkill_0");
		//this->announcement(L"BT_HealBySkill_0", 4, 3);
		return this->battle_char_select_action_type(3, ai.skillHealSkillbox[0], ai.skillHealSubSkillbox[0], -1, target);
	}
	case Action::BT_HealBySkill_1:
	{
		send_log(L"BT_HealBySkill_1");
		//this->announcement(L"BT_HealBySkill_1", 4, 3);
		return this->battle_char_select_action_type(3, ai.skillHealSkillbox[1], ai.skillHealSubSkillbox[1], -1, target);
	}
	case Action::BT_HealBySkill_2:
	{
		send_log(L"BT_HealBySkill_2");
		//this->announcement(L"BT_HealBySkill_2", 4, 3);
		return this->battle_char_select_action_type(3, ai.skillHealSkillbox[2], ai.skillHealSubSkillbox[2], -1, target);
	}
	case Action::normal_heal_by_item:
	{
		if (wcslen(ai.itemHealItem) > 0)
		{
			send_log(L"normal_heal_by_item");
			//this->announcement(L"normal_heal_by_item", 4, 3);
			const std::wstring_view words{ ai.itemHealItem };
			const std::wstring_view delim{ L"|" };

			auto view = words
				| std::ranges::views::split(delim)
				| std::ranges::views::transform([](auto&& rng) {
				return std::wstring_view(&*rng.begin(), std::ranges::distance(rng));
					});
			for (const std::wstring_view& it : view)
			{
				std::wstring tmp((std::wstring)it);
				int index = this->find_item_locate_spot(tmp);
				if (index != -1)
				{
					return this->battle_char_select_action_type(4, -1, -1, index, target);
				}
			}
		}
		break;
	}
	case Action::BT_ResurrectionBySkill:
	{
		send_log(L"BT_ResurrectionBySkill");
		//this->announcement(L"BT_ResurrectionBySkill", 4, 3);
		return this->battle_char_select_action_type(3, ai.skillResurrectionSkillbox, ai.skillResurrectionSubSkillbox, -1, target);
	}
	}
	return 0;
}

int CAsm::battle_char_auto_logic(const int& type)
{
	AI ai = *CAsm::asm_ptr->g_info_ptr.aiinfo;

	const BattleInfo battle = *CAsm::asm_ptr->g_info_ptr.battleinfo;//獲取戰場資訊
	PetInfo Pet = {};
	CharInfo Char = *CAsm::asm_ptr->g_info_ptr.charinfo;//獲取人物資訊
	int charposition = battle_get_char_real_position(battle, Char.name);//自身在戰場上的編號
	int petposition = -1;

	//如果人物位置不等於-1且寵物編號大於等於0
	if ((charposition != -1) && (Char.petpos >= 0))
	{
		Pet = *CAsm::asm_ptr->g_info_ptr.petinfo[Char.petpos];
		petposition = battle_get_pet_real_position(charposition);//戰寵在戰場上的編號
	}
	int target = -1;

	//根據人物當前位置獲取友方和敵方位置範圍 返回 0 正常位 1異常位 -1 錯誤
	char_current_size_cache = getMinMax(charposition, allies_min, allies_max, enemy_min, enemy_max);

	int healtype = -1;

	send_log(std::format(L"char at:{} pet at:{} cur_side:{} allie_range:{}-{} enemy_range:{}-{}", charposition, petposition, char_current_size_cache, allies_min, allies_max, enemy_min, enemy_max));
	//this->announcement(tmp, 4, 3);
	//this->announcement(L"Char.petpos = " + std::to_wstring(Char.petpos), 4, 3);

	const int firstDelay = ai.fristRoundDelaySpin / 100;
	const int regularDelay = ai.regularRoundDelaySpin / 100;
	int i = 0;
	//如果是第一回合 且首回延時開啟 且 首回延時時間大於0
	if ((battle.round == 1) && ai.fristRoundDelayCheck && (firstDelay > 0))
	{
		for (i = 0; i < firstDelay; i++)
			CRuntime::delay(100);
	}
	// 如果正常回合延時開啟 且 延時時間大於0
	else if (ai.regularRoundDelayCheck && regularDelay > 0)
	{
		for (i = 0; i < regularDelay; i++)
			CRuntime::delay(100);
	}
	//二次動作
	if (type == 2)
	{
		this->battle_skill_target_correction(battle, ai.secondAct, ai.secondTarget, target, petposition);
		return this->battle_last_correction(ai, Action::BT_SecondRound, target);
	}
	//道具補血
	else if (ai.itemHealCheckState && (ai.itemHealLowHpSpin > 0) && (
		this->battle_cmp_allies(ai.itemHealLowHpSpin, battle, COMPARE::Self, target, healtype, charposition, petposition) ||//查找人寵
		this->battle_cmp_allies(ai.itemHealAllieLowHpSpin, battle, COMPARE::Allies, target, healtype)))//查找全隊
	{
		return this->battle_last_correction(ai, Action::normal_heal_by_item, target);
	}
	//敵數量小於等於X
	else if ((ai.lowerEnemyCount > 0) && (battle.totlaEnemy <= ai.lowerEnemyCount))
	{
		this->battle_skill_target_correction(battle, ai.lowerEnemyAct, ai.lowerEnemyTarget, target, petposition);
		return this->battle_last_correction(ai, Action::BT_EnemyLowerThan, target);
	}
	//敵數量大於X
	else if ((ai.higherEnemyCount > 0) && (battle.totlaEnemy > ai.higherEnemyCount))
	{
		this->battle_skill_target_correction(battle, ai.higherEnemyAct, ai.higherEnemyTarget, target, petposition);
		return this->battle_last_correction(ai, Action::BT_EnemyHigherThan, target);
	}
	//人物魔力低於
	else if ((ai.lowMpSpin > 0) && (get_percent(Char.mp, Char.maxmp) <= ai.lowMpSpin))
	{
		this->battle_skill_target_correction(battle, ai.lowMpAct, ai.lowMpTarget, target, petposition);
		return this->battle_last_correction(ai, Action::BT_CharMpLowerThan, target);
	}
	//技能復活
	else if (ai.skillResurrectionCheckState && this->is_allies_dead(ai.skillResurrectionTarget, battle, target, petposition))
	{
		return this->battle_last_correction(ai, Action::BT_ResurrectionBySkill, target);
	}
	//技能補血0
	else if (ai.skillHealCheckState[0] && (ai.skillHealLowHpSpin[0] > 0) && (
		this->battle_cmp_allies(ai.skillHealLowHpSpin[0], battle, COMPARE::Self, target, healtype, charposition, petposition) ||//查找人寵
		this->battle_cmp_allies(ai.skillHealAllieLowHpSpin[0], battle, COMPARE::Allies, target, healtype)))//查找全隊
	{
		this->battle_healing_target_correction(battle, ai.skillHealTarget[0], ai.skillHealAllieLowHpSpin[0], healtype, target);
		return this->battle_last_correction(ai, Action::BT_HealBySkill_0, target);
	}
	//技能補血1
	else if (ai.skillHealCheckState[1] && (ai.skillHealLowHpSpin[1] > 0) && (
		this->battle_cmp_allies(ai.skillHealLowHpSpin[1], battle, COMPARE::Self, target, healtype, charposition, petposition) ||//查找人寵
		this->battle_cmp_allies(ai.skillHealAllieLowHpSpin[1], battle, COMPARE::Allies, target, healtype)))//查找全隊
	{
		this->battle_healing_target_correction(battle, ai.skillHealTarget[1], ai.skillHealAllieLowHpSpin[1], healtype, target);
		return this->battle_last_correction(ai, Action::BT_HealBySkill_1, target);
	}
	//技能補血2
	else if (ai.skillHealCheckState[2] && (ai.skillHealLowHpSpin[2] > 0) && (
		this->battle_cmp_allies(ai.skillHealLowHpSpin[2], battle, COMPARE::Self, target, healtype, charposition, petposition) ||//查找人寵
		this->battle_cmp_allies(ai.skillHealAllieLowHpSpin[2], battle, COMPARE::Allies, target, healtype)))//查找全隊
	{
		this->battle_healing_target_correction(battle, ai.skillHealTarget[2], ai.skillHealAllieLowHpSpin[2], healtype, target);
		return this->battle_last_correction(ai, Action::BT_HealBySkill_2, target);
	}
	//一般動作
	else
	{
		this->battle_skill_target_correction(battle, ai.normalAct, ai.normalTarget, target, petposition);
		return this->battle_last_correction(ai, Action::BT_Normal, target);
	}
	return 0;
}

int CAsm::battle_pet_auto_logic(const int& type)
{
	//int i = 0;
	AI ai = *CAsm::asm_ptr->g_info_ptr.aiinfo;

	const BattleInfo battle = *CAsm::asm_ptr->g_info_ptr.battleinfo;
	PetInfo Pet = {};
	CharInfo Char = *CAsm::asm_ptr->g_info_ptr.charinfo;

	int target = -1;
	int charposition = battle_get_char_real_position(battle, Char.name);//自身在戰場上的編號

	int petposition = -1;
	if (charposition != -1 && Char.petpos >= 0 && Char.petpos < 5)
	{
		Pet = *CAsm::asm_ptr->g_info_ptr.petinfo[Char.petpos];
		battle_get_pet_real_position(charposition);//戰寵在戰場上的編號
	}

	if (type == 4)
		char_current_size_cache = getMinMax(charposition, allies_min, allies_max, enemy_min, enemy_max);

	//二次動作
	if (*(int*)MAKEADDR(0x5, 0x9, 0x8, 0x8, 0xf, 0x4) == 4)//0x05988F4
	{
		send_log(L"pet_second");
		//this->announcement(L"pet_second", 4, 3);
		this->battle_skill_target_correction(battle, -1, ai.pet_secondTarget, target, petposition);
		return this->battle_pet_select_action_type(0, ai.pet_secondSkillbox, target);
	}
	//敵數量小於等於X
	else if ((ai.pet_lowerEnemyCount > 0) && (battle.totlaEnemy <= ai.pet_lowerEnemyCount))
	{
		send_log(L"pet_enemylower");
		//this->announcement(L"pet_enemylower", 4, 3);
		this->battle_skill_target_correction(battle, -1, ai.pet_lowerEnemyTarget, target, petposition);
		return this->battle_pet_select_action_type(ai.pet_lowerEnemyAct, ai.pet_lowerEnemySkillbox, target);
	}
	//敵數量大於X
	else if ((ai.pet_higherEnemyCount > 0) && (battle.totlaEnemy > ai.pet_higherEnemyCount))
	{
		send_log(L"pet_enemyhigher");
		//this->announcement(L"pet_enemyhigher", 4, 3);
		this->battle_skill_target_correction(battle, -1, ai.pet_higherEnemyTarget, target, petposition);
		return this->battle_pet_select_action_type(ai.pet_higherEnemyAct, ai.pet_higherEnemySkillbox, target);
	}
	//寵物魔力低於
	else if ((ai.pet_lowMpSpin > 0) && (get_percent(Pet.mp, Pet.maxmp) <= ai.pet_lowMpSpin))
	{
		send_log(L"pet_lowmp");
		//this->announcement(L"pet_lowmp", 4, 3);
		this->battle_skill_target_correction(battle, -1, ai.pet_lowMpTarget, target, petposition);
		return this->battle_pet_select_action_type(ai.pet_lowMpAct, ai.pet_lowMpSkillbox, target);
	}
	//寵物血量低於
	else if ((ai.pet_lowHpCheckState) && (ai.pet_lowHpSpin > 0) && (get_percent(Pet.hp, Pet.maxhp) <= ai.pet_lowHpSpin))
	{
		send_log(L"pet_lowhp");
		//this->announcement(L"pet_lowhp", 4, 3);
		return this->battle_pet_select_action_type(0, ai.pet_lowHpSkillbox, 0);
	}
	//寵物一般動作
	else
	{
		send_log(L"pet_normal");
		//this->announcement(L"pet_normal", 4, 3);
		this->battle_skill_target_correction(battle, -1, ai.pet_normalTarget, target, petposition);
		return this->battle_pet_select_action_type(0, ai.pet_normalSkillbox, target);
	}
	return 0;
}

int CAsm::battle_ai_processing(const int& type)
{
	switch (type)
	{
	case 0://人物
	case 1://人物無寵第一回合
	case 2://人物無寵第二回合
	{
		return this->battle_char_auto_logic(type);
	}
	case 3://寵物
	case 4://人物騎寵
	{
		return this->battle_pet_auto_logic(type);
	}
	default:
	{
		break;
	}
	}
	return 0;
}

//std::mutex sd;
int __cdecl CAsm::hook_create_battle_screen(int type, int x, int y)
{
	CAsm::asm_ptr->battle_screen_type_cache = type;

	//const DWORD off = CAsm::asm_ptr->AIOFF;
	const DWORD on = CAsm::asm_ptr->AION;
	const DWORD is_auto_battle = CAsm::asm_ptr->is_auto_battle;
	typedef int(__cdecl* pfnCreateScreen)(IN int type, IN int x, IN int y);
	pfnCreateScreen CreateScreen = (pfnCreateScreen)MAKEADDR(0x4, 0x8, 0xE, 0xC, 0x7, 0x0); //0x048EC70;
	int nret = 0;
	if (is_auto_battle == on)
	{
		nret = CreateScreen(type, CRuntime::rand(1400, 1500), y);
		if (!CAsm::asm_ptr->battle_ai_processing(type))
		{
			CAsm::asm_ptr->announcement(L"AI出現異常，切換為手動", 4, 3);
			nret = CreateScreen(type, x, y);
		}
	}
	else
	{
		nret = CreateScreen(type, x, y);
	}
	return nret;
}

void CAsm::battle_send_command(const std::wstring& cmd)
//人物普攻 H|%X
//人物技能 S|%X|%X|%X
//寵物技能 W|%X|%X
//人物逃跑 E
//人物防禦 G
{
	typedef void(__cdecl* pfnCgBattleAction)(IN SOCKET socket, IN PCSTR str);
	const pfnCgBattleAction Action = (pfnCgBattleAction)MAKEADDR(0x5, 0x0, 0x6, 0x8, 0x6, 0x0);//0x0506860;
	typedef void(__cdecl* pfnClostBattleScreen)();
	const pfnClostBattleScreen Close = (pfnClostBattleScreen)MAKEADDR(0x4, 0x8, 0xF, 0x0, 0xA, 0x0);//0x048F0A0;

	char str[MAX_PATH] = {};
	CRuntime::wchar2char(cmd, str);
	SOCKET skt = *BLUESOCKET;
	__try
	{
		Action(skt, str);
		int* parg = nullptr;
		int* switcher = (int*)MAKEADDR(0x5, 0x9, 0x8, 0x9, 0x7, 0x4);// 0x0598974;
		int switche = 0;
		int temp = 0;
		int type = CAsm::battle_screen_type_cache;
		if (type == 0)
		{
			switche = 3;
			parg = (int*)MAKEADDR(0x5, 0x9, 0x8, 0x9, 0x2, 0xC);// 0x059892C;
			temp = *parg;
		}
		else if (type == 1)
		{
			switche = 2;
			parg = (int*)MAKEADDR(0x5, 0x9, 0x8, 0x9, 0x2, 0xC);// 0x059892C;
			temp = *parg;
		}
		else if (type == 2)
		{
			switche = 5;
			parg = (int*)MAKEADDR(0x5, 0x9, 0x8, 0x9, 0x2, 0xC);// 0x059892C;
			temp = *parg;
		}
		else if (type == 3)
		{
			switche = 5;
			parg = (int*)MAKEADDR(0x5, 0x9, 0x8, 0x9, 0x2, 0xC);// 0x05988F4;
			temp = *parg;
		}
		else if (type == 4)
		{
			switche = 5;
			parg = (int*)MAKEADDR(0x5, 0x9, 0x8, 0x8, 0xF, 0x4);//0x05988F4;
			temp = *parg;
		}
		if ((parg != nullptr) && (switche != 0))
		{
			*switcher = switche;
			*parg = ++temp;
			if (type == 3 || type == 4)
				*(int*)MAKEADDR(0x5, 0x4, 0x3, 0xD, 0xF, 0x0)/*0x0543DF0*/ = temp;
			Close();
		}
	}
	__finally
	{
		return;
	}
}
#pragma endregion

#pragma region script
#define debug(a) send_log(std::format(TEXT("{} :[ {} @ {} ] 異常: {}\0"), scmd.time, scmd.fileName, scmd.row, TEXT(a)));

int CAsm::find_skill_by_name(const std::wstring& str, const int& level)
{
	if ((level < 0) || (level > 11) || str.empty())
	{
		return -1;
	}
	int i = 0;
	char tmp[MAX_PATH] = {};
	for (i = 0; i < 14; i++)
	{
		ZeroMemory(tmp, MAX_PATH);
		GetSkill(i, tmp, MAX_PATH);
		std::wstring w(CRuntime::char2wchar(tmp));
		if (!str.contains(w))
		{
			continue;
		}
		ZeroMemory(tmp, MAX_PATH);
		GetSubSkill(i, level, tmp, MAX_PATH);
		if (strlen(tmp) > 0)
			return i;
	}
	return -1;
}

void CAsm::get_child_skill_index_by_name(const std::wstring str, int& skill, int& child, const bool& en)
{
	if (str.empty())
		return;
	int i = 0, j = 0;

	for (i = 0; i < 14; i++)
	{
		char tmp[MAX_PATH] = {};
		GetSkill(i, tmp, MAX_PATH);
		std::wstring w(CRuntime::char2wchar(tmp));
		if (w.empty())
			continue;

		ZeroMemory(tmp, MAX_PATH);
		for (j = 0; i < 50; j++)
		{
			GetChildSkill(i, j, tmp, MAX_PATH);
			std::wstring c(CRuntime::char2wchar(tmp));
			if (c.empty())
			{
				skill = -1;
				child = -1;
				return;
			}
			if (!en && c.contains(str))
			{
				skill = i;
				child = j;
				return;
			}
			else if (en && (c.compare(str) == 0))
			{
				skill = i;
				child = j;
				return;
			}
		}
	}
	skill = -1;
	child = -1;
}

inline void WINAPI return_base()
{
	typedef int(__cdecl* pfnReturnBase)(IN SOCKET skt);
	pfnReturnBase returnbase = (pfnReturnBase)MAKEADDR(0x5, 0x0, 0x8, 0xB, 0x7, 0x0);
	SOCKET skt = *BLUESOCKET;

	returnbase(skt);
	*(int*)ANIMEADDR = 7;
}

void CAsm::set_char_face_direction(const int& dir)
{
	typedef int(__cdecl* pfnDirectionChange)(IN int dir);
	typedef int(__cdecl* pfnDirectionSet)(IN int dir, IN int unknown);
	typedef int(__cdecl* pfnNpcTalk)(IN int x, IN int y, IN int unknown);
	pfnDirectionChange DirectionChange = (pfnDirectionChange)MAKEADDR(0x5, 0x0, 0xF, 0xD, 0x8, 0x0);
	pfnDirectionSet DirectionSet = (pfnDirectionSet)MAKEADDR(0x4, 0x6, 0x4, 0xA, 0xA, 0x0);
	pfnNpcTalk NpcTalk = (pfnNpcTalk)MAKEADDR(0x5, 0x0, 0x2, 0x7, 0xB, 0x0);
	int unknown = 0;
	int x, y;
	this->get_char_xy(x, y);

	DirectionChange(dir);
	DirectionSet(dir, 1);
	__asm
	{
		lea edx, [esp + 0xA]
		mov[esp + 0xA], al
		mov unknown, edx
	}
	NpcTalk(x, y, unknown);
}

inline const int WINAPI string_dir_to_int(const std::wstring& str)
{
	std::vector<std::wstring> v = { L"北", L"東北", L"東", L"東南", L"南", L"西南", L"西", L"西北" };
	//try to find str in v, if found return index.
	std::vector<std::wstring>::iterator it = find(v.begin(), v.end(), str);
	auto index = std::distance(std::begin(v), it);
	return index;
}

void CAsm::switch_pet_position(const int& pos, const int& type)
{
	typedef int(__cdecl* pfnSwitchPos)();
	pfnSwitchPos SwitchPos = (pfnSwitchPos)MAKEADDR(0x4, 0x8, 0x5, 0xB, 0x1, 0x0);

	if (type >= 0 && type <= 3)
	{
		if (type == 0)
			*(int*)MAKEADDR(0xc, 0x4, 0x6, 0x2, 0xc, 0x8) = -1;

		*(int*)((pos * 4) + MAKEADDR(0xc, 0x4, 0x3, 0x1, 0x3, 0x8)) = type;
		SwitchPos();
		//SwitchPos();
	}
	else if (type == 4)
	{
		//__asm
		//{
		//	mov esi, dword ptr ds : [0x0CAFDC8]
		//	mov edx, dword ptr ds : [0x0D0CEA0]
		//	push 0x0580590
		//	neg cl
		//	sbb ecx, ecx
		//	push - 0x1
		//	and ecx, 0x64
		//	push ebx
		//	add ecx, esi
		//	push ecx
		//	push edx
		//	mov eax, 0x0507D70
		//	call eax
		//	add esp, 0x14
		//}
	}

	this->update_pet_position();
}

void CAsm::buy_or_sell(const int& index, const int& amt, const int& type)
{
	if (type != 0 && type != 1)
		return;
	typedef int(__cdecl* pfnBuy)(IN SOCKET skt, IN int x, IN int y, IN int type, IN int id, IN int unknown, IN char* str);
	int npc_id = *(int*)MAKEADDR(0xC, 0xA, 0xF, 0x1, 0xF, 0x4);
	int* npc_screen_type = (int*)MAKEADDR(0xC, 0x4, 0x3, 0x9, 0x0, 0x0);

	std::string cmd(std::format("{}\\z{}", index, amt));

	char* cstr = (char*)cmd.c_str();
	int x, y;
	this->get_char_xy(x, y);
	pfnBuy Buy = (pfnBuy)MAKEADDR(0x5, 0x0, 0x8, 0x4, 0xA, 0x0);
	SOCKET skt = *BLUESOCKET;

	*npc_screen_type = 334 + type;

	Buy(skt, x, y, *npc_screen_type, npc_id, 0x0, cstr);
}

void CAsm::pruduct_produce(const std::vector<std::wstring> v, const bool& en)
{
	CKernel32& ker32 = CKernel32::get_instance();
	BYTE bNew[5] = { NOP,NOP,NOP,NOP,NOP };
	this->memory_move(MAKEADDR(0x4, 0x7, 0xC, 0xC, 0x5, 0x2), bNew, sizeof(bNew));
	int size = v.size();
	int i = 0;
	for (i = 0; i < size; i++)
	{
		int pos = this->find_item_locate_spot(v.at(i), en);
		if (pos == -1)
		{
			return;
		}
		//添加材料位置
		*(int*)(MAKEADDR(0xC, 0x2, 0x2, 0x5, 0x7, 0x8) + 0x4 * i) = pos;
	}

	//將物品標示設置為 true
	for (i = 0; i < 5; i++)
	{
		*(bool*)(MAKEADDR(0xC, 0x4, 0x5, 0xF, 0x8, 0xC) - 4 * i) = true;
	}

	//開始製作
	*(int*)ANIMEADDR = 2;
	int* status_ptr = (int*)MAKEADDR(0xC, 0xA, 0xE, 0xA, 0xE, 0x8);
	while (*status_ptr != 2)
	{
		ker32.My_Sleep(100);
	}
	//恢復
	BYTE bOri[5] = { CALL , 0xD9, 0x9C, 0xFF, 0xFF };
	this->memory_move(MAKEADDR(0x4, 0x7, 0xC, 0xC, 0x5, 0x2), bOri, sizeof(bOri));

	//動畫刷新 消除紅框
	*(int*)ANIMEADDR = 203;
}

inline void WINAPI appraisal_item(const int& pos)
{
	if (pos < 0 || pos > 19)
		return;
	typedef int(__cdecl* pfnAppraisal)(IN SOCKET skt, IN int unknown, IN int unknown2, IN int pos, IN char* unknown3);
	pfnAppraisal Appraisal = (pfnAppraisal)MAKEADDR(0x5, 0x0, 0x7, 0xD, 0x7, 0x0);
	char str[MAX_PATH] = {};
	SOCKET skt = *BLUESOCKET;

	Appraisal(skt, 0, 0, pos + 0x8, str);
}

inline void WINAPI cure(const int& index, const int& level, const int& sel, const int& tar)
{
	CKernel32& ker32 = CKernel32::get_instance();
	ker32.My_Sleep(100);
	useSkill(index, level);
	ker32.My_Sleep(300);
	preSkillCure(sel);
	ker32.My_Sleep(300);
	skillCure(index, tar);
	ker32.My_Sleep(300);
	close_screen();
}

std::mutex global_script_thread_mutex;
void CAsm::script_thread(char* mychar)
{
	std::jthread script_thread_ptr(&CAsm::script_run, this, mychar);
}

void CAsm::script_run(char* mychar)
{
	ScriptCommand scmd = {};
	CKernel32& ker32 = CKernel32::get_instance();
	ker32.My_NtMoveMemory(&scmd, mychar, sizeof(ScriptCommand));
	global_auto_walk_mutex.lock();
	this->script_receive(scmd);
	global_auto_walk_mutex.unlock();
}

int CAsm::script_receive(const ScriptCommand& scmd)
{
	//CSocket& skt = CSocket::get_instance();
	CKernel32& ker32 = CKernel32::get_instance();
	std::vector<std::wstring> arr;

	for (const auto& it : scmd.sz)
	{
		if (wcslen(it) > 0)
			arr.push_back(it);
	}

	const int size = arr.size();

	if (size != scmd.size)
	{
		debug("參數異常");
		return 0;
	}

	const std::vector<std::wstring> args = arr;

	switch ((StringValue)scmd.cmd)
	{
	case CAsm::說出:
	{
		talk(args.at(0));
		return 0;
	}
	case CAsm::坐標:
	{
		if (size < 2)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		if (!CRuntime::isint(args.at(0)) || !CRuntime::isint(args.at(1)))
		{
			debug("座標參數必須為整數");
			return 0;
		}
		int x = std::stoi(args.at(0), nullptr, 10);
		int y = std::stoi(args.at(1), nullptr, 10);
		if (x <= 0 || x >= 1500 || y <= 0 || y >= 1500)
		{
			debug("座標參數超出範圍");
			return 0;
		}
		this->move_char_to(x, y);
		return 0;
	}
	case CAsm::方向:
	{
		if (size < 1)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		int dir = -1;
		if (!CRuntime::isint(args.at(0)))
			dir = string_dir_to_int(args.at(0));
		else
		{
			dir = std::stoi(args.at(0), nullptr, 10) - 1;//將起始索引從1改回0
		}

		if (dir < 0 || dir > 7)
		{
			debug("方向參數錯誤");
			return 0;
		}
		this->set_char_face_direction(dir);

		return 0;
	}
	case CAsm::尋路:
		break;
	case CAsm::輸出:
		break;
	case CAsm::回點:
	{
		return_base();
		return 0;
	}
	case CAsm::登出:
	{
		LogOut();
		return 0;
	}
	case CAsm::清屏:
	{
		clear_screen();
		return 0;
	}
	case CAsm::按鈕:
		break;
	case CAsm::整理:
	{
		talk(L"/r");
		return 0;
	}
	case CAsm::使用道具:
	{
		int index = -1;
		if (size <= 0)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		if (size == 1)
			index = this->find_item_locate_spot(args.at(0));
		else
		{
			bool en = false;
			if (CRuntime::isint(args.at(1)))
				en = std::stoi(args.at(1), nullptr, 10);
			else
				debug("搜索模式參數必須為整數或布爾值");
			index = this->find_item_locate_spot(args.at(0), en);
		}
		if (index >= 0 && index <= 19)
			this->use_item(index);
		else
			debug("找不到符合參數條件的物品");
		return 0;
	}
	case CAsm::撿物:
		break;
	case CAsm::購買:
	{
		if (size < 2)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		if (!CRuntime::isint(args.at(0)))
		{
			debug("物品編號參數必須為整數");
			return 0;
		}
		int index = std::stoi(args.at(0), nullptr, 10) - 1;//將起始索引從1改回0
		if (index < 0)
		{
			debug("物品編號參數超出限制");
			return 0;
		}
		if (!CRuntime::isint(args.at(1)))
		{
			debug("物品數量參數必須為整數");
			return 0;
		}

		int amt = std::stoi(args.at(1), nullptr, 10);
		if (amt <= 0)
		{
			debug("物品數量參數不正確");
			return 0;
		}
		this->buy_or_sell(index, amt, 1);
		return 0;
	}
	case CAsm::售賣:
	{
		if (size < 1)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		if (args.at(0).empty())
		{
			debug("售賣物品名稱參數不能為空");
			return 0;
		}
		int en = false;
		if (size > 1)
		{
			if (CRuntime::isint(args.at(1)))
				en = std::stoi(args.at(1), nullptr, 10);
			else
				debug("搜索模式參數必須為整數或布爾值");
		}
		std::vector<int> v = this->find_item_locate_spot(args.at(0), NULL, en);
		if (v.size() <= 0)
		{
			debug("找不到符合參數條件的物品");
			return 0;
		}
		for (const auto& index : v)
		{
			if (GetItemExist(index))
				this->buy_or_sell(index /*1為起始索引*/ + 0x8/*扣掉裝備的8格*/, GetItemStack(index), 0);
		}
		return 0;
	}
	case CAsm::丟棄道具:
	{
		if (size < 1)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		if (args.at(0).empty())
		{
			debug("丟棄物品名稱參數不能為空");
			return 0;
		}
		int en = false;

		if (size > 1)
		{
			if (CRuntime::isint(args.at(1)))
				en = std::stoi(args.at(1), nullptr, 10);
			else
				debug("搜索模式參數必須為整數或布爾值");
		}

		if (CRuntime::isint(args.at(0)))
		{
			int index = std::stoi(args.at(0), nullptr, 10) - 1;// 將起始索引從1改回0
			if (index < 0 || index >= 20)
			{
				debug("物品位置參數超出範圍");
				return 0;
			}
			if (!GetItemExist(index))
			{
				debug("所選擇丟棄的物品不存在");
				return 0;
			}
			this->throw_items(index);
			return 0;
		}

		std::vector<int> v = this->find_item_locate_spot(args.at(0), NULL, en);
		if (v.size() <= 0)
		{
			debug("找不到符合參數條件的物品");
			return 0;
		}
		for (const auto& index : v)
		{
			if (GetItemExist(index))
			{
				this->throw_items(index);// +8去除裝備位置
			}
		}

		return 0;
	}
	case CAsm::丟棄金幣:
		break;
	case CAsm::丟棄寵物:
		break;
	case CAsm::治療:
	{
		if (size < 3)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		int index = -1;
		if (!CRuntime::isint(args.at(0)))
		{
			debug("技能等級參數必須為整數");
			return 0;
		}
		int level = std::stoi(args.at(0), nullptr, 10) - 1;// 將起始索引從1改回0
		if (level < 0 || level > 11)
		{
			debug("選擇治療等級參數超出範圍");
			return 0;
		}

		index = find_skill_by_name(L"治療", level);
		if (index < 0)
		{
			debug("找不到技能 \"治療\" ");
			return 0;
		}
		if (!CRuntime::isint(args.at(1)))
		{
			debug("選擇人物參數必須為整數");
			return 0;
		}
		int sel = std::stoi(args.at(1), nullptr, 10) - 1;// 將起始索引從1改回0
		if (sel < 0 || sel > 4)
		{
			debug("選擇人物參數超出範圍");
			return 0;
		}
		if (!CRuntime::isint(args.at(2)))
		{
			debug("選擇目標參數必須為整數");
			return 0;
		}
		int tar = std::stoi(args.at(2), nullptr, 10) - 1;// 將起始索引從1改回0
		if (tar < 0 || tar > 5)
		{
			debug("選擇施放對象參數超出範圍");
			return 0;
		}
		try
		{
			cure(index, level, sel, tar);
		}
		catch (...) {}
		return 0;
	}
	case CAsm::採集:
		break;
	case CAsm::鑑定:
	{
		if (size < 1)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		int index = -1;
		bool en = false;
		if (size > 1)
		{
			if (CRuntime::isint(args.at(1)))
				en = std::stoi(args.at(1), nullptr, 10);
			else
				debug("搜索模式參數必須為整數或布爾值");
		}
		if (CRuntime::isint(args.at(0)))
		{
			index = std::stoi(args.at(0), nullptr, 10) - 1;// 將起始索引從1改回0
			if (index < 0 || index > 20)
			{
				debug("鑑定物品位置參數超出範圍");
				return 0;
			}
		}
		else
		{
			index = find_item_locate_spot(args.at(0), en);
			if (index < 0 || index > 20)
			{
				debug("找不到要鑑定的物品");
				return 0;
			}
		}

		if (!GetItemExist(index))
		{
			debug("所選擇位置沒有物品存在");
			return 0;
		}
		char str[MAX_PATH] = {};
		GetItemName(index, str, MAX_PATH);
		std::wstring wstr(CRuntime::char2wchar(str));
		if (!wstr.contains(L"？"))
		{
			debug("所選擇的物品無法被鑑定");
			return 0;
		}
		appraisal_item(index);
		return 0;
	}
	case CAsm::製作:
	{
		if (size < 3)
		{
			debug("輸入參數數量過少");
			return 0;
		}

		std::vector<std::wstring> v;
		std::wstring tmp(L"");
		bool en = false;
		int end_of_value = -1;
		std::wstring end_of = args.at(args.size() - 1);
		if (CRuntime::isint(end_of))
			end_of_value = std::stoi(end_of, nullptr, 10);

		//查找是否有匹配模式參數
		if (size > 2 && (end_of_value == 0 || end_of_value == 1))
		{
			en = end_of_value;
		}
		for (int i = 1; i < size; i++)
		{
			tmp = args.at(i);
			if (tmp.empty())
				continue;
			if (en && i == size - 1)
				break;
			v.push_back(tmp);
		}

		if (v.size() <= 0)
		{
			debug("缺少材料參數");
			return 0;
		}

		int skill, child;
		if (!en)
			get_child_skill_index_by_name(args.at(0), skill, child);
		else
			get_child_skill_index_by_name(args.at(0), skill, child, en);

		if (skill < 0 || child < 0)
		{
			debug("找不到對應的製作技能");
			return 0;
		}

		create_produce_product_screen(skill, child);
		ker32.My_Sleep(300);
		pruduct_produce(v, en);
		return 0;
	}
	case CAsm::存入道具:
	{
		if (size < 1)
		{
			debug("輸入參數數量過少");
			return 0;
		}

		bool en = false;
		int max = -1;
		if (size > 2)
		{
			if (CRuntime::isint(args.at(1)))
				en = std::stoi(args.at(1), nullptr, 10);
			else
			{
				debug("搜索模式參數必須為整數或布爾值");
				return 0;
			}
			if (!CRuntime::isint(args.at(2)))
			{
				max = std::stoi(args.at(2), nullptr, 10) - 1;
			}
			else
			{
				debug("搜索模式參數必須為整數或布爾值");
				return 0;
			}
		}
		else if (size > 1)
		{
			if (CRuntime::isint(args.at(1)))
				en = std::stoi(args.at(1), nullptr, 10);
			else
			{
				debug("搜索模式參數必須為整數或布爾值");
				return 0;
			}
		}

		std::wstring item = args.at(0);
		if (item.empty())
		{
			debug("存入物品名稱/位置參數不能為空");
			return 0;
		}

		//先查找身上所有要存的物品位置
		std::vector<int> items = this->find_item_locate_spot(item, NULL, en);
		int items_size = items.size();
		int items_size_cache;
		if (max == -1)
		{
			items_size_cache = items_size;
		}
		else
		{
			items_size_cache = max;
		}
		if (items_size <= 0)
		{
			debug("找不到參數目標物品");
			return 0;
		}

		std::vector<int> bank_spots_page1;
		bool page1pass = false;
		int bank1_size = 0;
		std::vector<int> bank_spots_page2;
		bool page2pass = false;
		int bank2_size = 0;

		//先查找第二頁
		bank_set_page(1);
		ker32.My_Sleep(1500);
		if (find_bank_empty_spot(bank_spots_page2))
			page2pass = true;

		//查找第一頁
		bank_set_page(0);
		ker32.My_Sleep(1500);
		if (find_bank_empty_spot(bank_spots_page1))
			page1pass = true;

		if (!page1pass && !page2pass)
		{
			debug("銀行找不到任何可以存放的空位");
			return 0;
		}

		//如果第一頁有空位先換回第一頁
		if (page1pass)
		{
			bank1_size = bank_spots_page1.size();

			//開始將物品移入第一頁
			for (int i = 0; i < items_size; i++)
			{
				if (i >= bank1_size || items_size_cache == 0)
					break;
				this->bank_move_item(items.at(i), bank_spots_page1.at(i), true);

				//扣除要存放物品數量緩存
				--items_size_cache;
			}
		}
		//如果第二頁沒位置直接返回
		if (page2pass)
			bank2_size = bank_spots_page2.size();
		else
			return 0;

		//沒有剩餘要存的也返回
		if (items_size_cache <= 0)
			return 0;

		//切換到第二頁
		bank_set_page(1);
		ker32.My_Sleep(1500);

		//重新搜索身上物品
		items = this->find_item_locate_spot(item, NULL, en);
		items_size = items.size();
		if (items_size <= 0)
		{
			return 0;
		}

		//將物品移入第二頁
		for (int i = 0; i < items_size; i++)
		{
			if (i >= bank2_size || items_size_cache == 0)
				break;
			this->bank_move_item(items.at(i), bank_spots_page2.at(i), true);
		}

		return 0;
	}
	case CAsm::提出道具:
	{
		if (size < 1)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		bool en = false;
		int max = -1;
		//如果有包含匹配模式和數量
		if (size > 2)
		{
			if (CRuntime::isint(args.at(1)))
				en = std::stoi(args.at(1), nullptr, 10);
			else
			{
				debug("搜索模式參數必須為整數或布爾值");
				return 0;
			}
			if (CRuntime::isint(args.at(2)))
			{
				max = std::stoi(args.at(2), nullptr, 10);
			}
			else
			{
				debug("數量參數必須為整數");
				return 0;
			}
		}
		//如果有包含匹配模式
		else if (size > 1)
		{
			if (CRuntime::isint(args.at(1)))
				en = std::stoi(args.at(1), nullptr, 10);
			else
			{
				debug("搜索模式參數必須為整數或布爾值");
				return 0;
			}
		}

		std::vector<int> empty_spots;
		//檢查身上空位
		if (!find_empyt_spot(empty_spots))
		{
			debug("身上沒有可以存放的空位");
			return 0;
		}

		std::wstring www(L"spot:");
		for (const auto& it : empty_spots)
		{
			www += std::to_wstring(it) + L"|";
		}
		this->announcement(www);

		//如果沒有設置最大數量則默認以全部空格為最大值
		if (max == -1)
			max = empty_spots.size();

		std::wstring item = args.at(0);
		if (item.empty())
		{
			debug("提取道具名稱不能為空");
			return 0;
		}

		//先查找第一頁

		bank_set_page(0);
		ker32.My_Sleep(1500);
		std::vector<int> bank_item_page1 = find_bank_item_locate_spot(item, NULL, en);
		int bank_size1 = bank_item_page1.size();
		bool page1pass = false;
		if (bank_size1 > 0)
			page1pass = true;

		if (!page1pass)
		{
			debug("銀行第一頁找不到可以提取的物品");
		}
		//如果第一頁有空位先換回第一頁
		else if (page1pass)
		{
			int spot_size = empty_spots.size();
			//開始將第一頁物品移入背包
			for (int i = 0; i < bank_size1; i++)
			{
				if (i >= spot_size || max <= 0)
					break;
				bank_move_item(empty_spots.at(i), bank_item_page1.at(i), false);
				--max;
			}
		}

		if (max <= 0)
			return 0;

		//查找第二頁
		bank_set_page(1);
		ker32.My_Sleep(1500);
		std::vector<int> empty_spots2;
		//重新檢查身上空位
		if (!find_empyt_spot(empty_spots2))
		{
			debug("身上已經沒有可以存放的空位");
			return 0;
		}
		int spot_size2 = empty_spots2.size();

		std::vector<int> bank_item_page2 = find_bank_item_locate_spot(item, NULL, en);
		int bank_size2 = bank_item_page2.size();
		bool page2pass = false;
		if (bank_size2 > 0)
			page2pass = true;

		//如果第二頁沒有需要移出的物品直接返回
		if (!page2pass)
		{
			debug("銀行第二頁找不到可以提取的物品");
			return 0;
		}
		else
		{
			//第二頁有目標物品換回第二頁
			for (int i = 0; i < bank_size2; i++)
			{
				if (i >= spot_size2 || max <= 0)
					break;
				bank_move_item(empty_spots2.at(i), bank_item_page2.at(i), false);
				--max;
			}
		}

		return 0;
	}
	case CAsm::更換寵物:
	{
		if (size < 2)
		{
			debug("輸入參數數量過少");
			return 0;
		}
		if (!CRuntime::isint(args.at(0)))
		{
			debug("寵物位置參數必須為整數");
			return 0;
		}
		int pos = std::stoi(args.at(0), nullptr, 10) - 1;// 將起始索引從1改回0
		if (pos < 0 || pos > 4)
		{
			debug("寵物位置參數超出範圍");
			return 0;
		}
		if (!CRuntime::isint(args.at(1)))
		{
			debug("更換類型種類參數必須為整數");
			return 0;
		}
		int type = std::stoi(args.at(1), nullptr, 10) - 1;// 將起始索引從1改回0
		if (type < 0 || type > 4)
		{
			debug("更換類別參數超出範圍");
			return 0;
		}
		switch_pet_position(pos, type);
		return 0;
	}
	case CAsm::寵物郵件:
		break;
	default:
		break;
	}

	return 0;
}
#pragma endregion