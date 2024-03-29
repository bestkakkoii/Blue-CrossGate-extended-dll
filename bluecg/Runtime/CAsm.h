#pragma once
#ifndef CASM_H
#define CASM_H
#include "CSocket.h"
#include <Windows.h>
#include <mutex>
#include <map>

#include "CKernel32.h"
#include "CMap.h"
#define BLUECDECL __cdecl
inline const int getconst(unsigned int i, unsigned int base = 0x10000)
{
	return (int)(i * base) + 0x4;
}

#define ACT_EXIT getconst(0x1, 0xA00000)
#define ACT_ANTIMUTEX getconst(0x2, 0xA00000)
#define ACT_ANNOUNCE getconst(0x4, 0xA00000)
#define ACT_MOVE getconst(0x8, 0xA00000)
#define ACT_AUTOLOGIN getconst(0x10, 0xA00000)
#define ACT_SCRIPT getconst(0x24, 0xA00000)

#define DA_MULTIPLEDATA getconst(0x1, 0xF0000)
#define DA_CLIENTINFO getconst(0x2)
#define DA_HWND getconst(0x4)
#define DA_INDEX getconst(0x8)
#define DA_USERTABLE getconst(0x10)
#define DA_USERLOGIN getconst(0x20)
#define DA_SKILLINFO getconst(0x40)
#define DA_ITEMINFO getconst(0x80)
#define DA_BATTLEINFO getconst(0x100)
#define DA_DATETIME getconst(0x200)
#define DA_PETINFO getconst(0x400)
#define DA_CHARINFO getconst(0x800)
#define DA_AIINFO getconst(0x1000)
#define DA_ClientAllInfo getconst(0x2000)

#define REMOTE_AION (unsigned int)0x800
#define REMOTE_AIOFF (unsigned int)0x400

//hiding class for hide value, DWORD get(); void set(DWORD value);
class HideValue
{
public:
	explicit HideValue(const DWORD& dw) : v(dw << 24)
	{
	}
	const DWORD BLUECDECL get()
	{
		const DWORD dwret = (DWORD)((v >> 24) & 0xFF);
		return dwret;
	}

private:
	DWORD v = 0;
};

const DWORD BLUECDECL makeaddress(HideValue a, HideValue b, HideValue c, HideValue d, HideValue e, HideValue f);
#define MAKEADDR(a, b, c, d, e, f ) makeaddress(HideValue(a), HideValue(b), HideValue(c), HideValue(d), HideValue(e), HideValue(f))

template <class ToType, class FromType>
void GetMemberFuncAddr_VC6(ToType& addr, FromType f)
{
	union
	{
		FromType _f;
		ToType   _t;
	}ut;

	ut._f = f;

	addr = ut._t;
}
#define BLUESOCKET        (SOCKET*)MAKEADDR(0xD, 0x0, 0xC, 0xE, 0xA, 0x0)//0D0CEA0

#define USERNAMEADDR      (DWORD)MAKEADDR(0xD, 0x1, 0x5, 0x6, 0x4, 0x4)//0x0D15644
#define CHARNAMEADDR      (DWORD)MAKEADDR(0xF, 0x4, 0xC, 0x3, 0xF, 0x8)//0x0F4C3F8
#define ANIMEADDR         (DWORD)MAKEADDR(0xF, 0x6, 0x2, 0x9, 0x5, 0x4)//0x0F62954
#define MAPADDR           (DWORD)MAKEADDR(0x9, 0x5, 0xC, 0x8, 0x7, 0x0)//0x095C870
#define GOLDADDR          (DWORD)MAKEADDR(0xF, 0x4, 0xC, 0x3, 0xE, 0xC)//0x0F4C3EC
#define LEVELADDR         (DWORD)MAKEADDR(0xF, 0x4, 0xC, 0x2, 0xF, 0x4)//0x0F4C2F4
#define TIMEADDR          (DWORD)MAKEADDR(0xF, 0xC, 0x2, 0x4, 0x2, 0x0)//0x0FC2420
#define MOUSEADDR		  (DWORD)MAKEADDR(0xC, 0xD, 0xA, 0x9, 0x8, 0x4)//0xCDA984
#define MOUSEDOWNADDR     (DWORD)MAKEADDR(0xC, 0x0, 0xC, 0x2, 0xD, 0xC)//0xC0C2DC
#define SUBSERVERPAGEADDR (DWORD)MAKEADDR(0x5, 0x4, 0x9, 0xE, 0x6, 0xC)//0x0549E6C
#define SUPERSPEED        (DWORD)MAKEADDR(0x5, 0x4, 0x8, 0xF, 0x1, 0x0)//0x548F10
//#define SUPERSPEEDFAKE    (DWORD)0x548F08
#define SUPERSPEEDFAKE2   (DWORD)MAKEADDR(0x5, 0x3, 0xE, 0x3, 0xF, 0x8)//0x53E3F8

#pragma region Struct

typedef struct tagMoveXY
{
	int x;
	int y;
}MoveXY;

/*與客戶端共用*/
typedef struct tagSocketCheck {
	int cbsize;
	int msg;
	int gnum;
} SocketCheck;

typedef struct tagAnnounce {
	SocketCheck identify;
	int color;
	int size;
	wchar_t sz[260];
} Ann;

typedef struct tagScriptCommand {
	SocketCheck identify;
	int size;
	int row;
	wchar_t time[12];
	wchar_t fileName[260];
	int cmd;
	wchar_t sz[20][32];
} ScriptCommand;

typedef struct tagMultiDATA {
	SocketCheck identify;
	int additional;

	int llvalue;
	DWORD hWnd;
	wchar_t sz[260];
} DATA;

typedef struct tagLoginInformation {
	SocketCheck identify;
	int autoLoginCheckState;
	int server;
	int subserver;
	int pos;
	wchar_t username[32];
	wchar_t password[32];
} LoginInformation;

typedef struct tagDateTime {
	SocketCheck identify;

	unsigned int yyyy;
	unsigned int MM;
	unsigned int dd;
	unsigned int hh;
	unsigned int mm;
	unsigned int ss;
	unsigned int mmm;
} DateTime;

//從客戶端接收的

typedef struct tagTableInfo {
	SocketCheck identify;

	char account[32];
	int level;
	char name[32];
	int status;
	int hp;
	int maxhp;
	int mp;
	int maxmp;
	int gold;
	int time;
	int east;
	int south;
	int mapid;
	char map[32];
} TableInfo;

typedef struct tagBattleInfo {
	SocketCheck identify;

	int total;
	int round;
	long long tickcount;
	long long totalduration;
	unsigned char exist[20];
	long long unknown0[20];
	long long unknown1[20];
	int pos[20];
	char name[20][32];
	int model[20];
	long long unknown2[20];
	int level[20];
	int hp[20];
	int maxhp[20];
	int mp[20];
	int maxmp[20];
	long long status[20];

	int totlaEnemy;
} BattleInfo;

typedef struct tagItem {
	SocketCheck identify;

	unsigned char exist[28];
	int id[28];
	int stack[28];
	char name[28][24];
} ItemInfo;

typedef struct tagSkill {
	SocketCheck identify;

	char name[14][24];
	char subname[14][11][24];
	int exp[14];
	int maxexp[14];
} SkillInfo;

typedef struct tagCharInfo {
	int vit;
	int str;
	int vgh;
	int dex;
	int inte;
	int manu_endurance;
	int manu_skillful;
	int manu_intelligence;
	int atk;
	int def;
	int agi;
	int spirit;
	int recovery;
	int charisma;
	unsigned char unknown0[12];
	int resist_poison; //+4
	int resist_sleep; //+4
	int resist_medusa; //+4
	int resist_drunk;
	int resist_chaos; //+4
	int resist_forget; //+4
	int critical; //+4
	int strikeback; //+4
	int accurancy; //+4
	int dodge; //+4
	int earth; //+4
	int water; //+4
	int fire; //+4
	int wind; //+4
	int level;
	int health; //健康度，0为绿色，100为红色
	int souls; //掉魂数，0~5
	int gold;
	int unitid;
	int petpos;
	int direction;
	int punchclock; //卡時
	int isusingpunchclock; //是否已打卡
	char name[17];
	char nickname[17];
	int hp;
	int maxhp;
	int mp;
	int maxmp;
	int point;
} CharInfo;

typedef struct tagCharInfos
{
	SocketCheck identify;
	CharInfo charinfo;
}CharInfos;

typedef struct tagPetSkill {
	char name[25]; //+14
	char detail[115]; //+
} PetSkill;

typedef struct tagPetInfo {
	int unknown0; //+0
	int unknown1; //+4
	int level; //+4
	char unknown2[96]; //+4
	int point; //+96
	int vit; //+4
	int str; //+4
	int vgh; //+4
	int dex; //+4
	int inte; //+4
	int atk; //+4
	int def; //+4
	int agi; //+4
	int spirit; //+4
	int recovery; //+4
	int loyalty; //+4
	int resist_poison; //+4
	int resist_sleep; //+4
	int resist_medusa; //+4
	int resist_drunk;
	int resist_chaos; //+4
	int resist_forget; //+4
	int critical; //+4
	int strikeback; //+4
	int accurancy; //+4
	int dodge; //+4
	int earth; //+4
	int water; //+4
	int fire; //+4
	int wind; //+4
	char unknown3[10]; //+4

	PetSkill skill[10];

	unsigned char unknown4[86]; //+4
	char name[17];
	char nickname[17];

	int hp;
	int maxhp;
	int mp;
	int maxmp;
} PetInfo;

typedef struct tagPetInfos
{
	SocketCheck identify;
	PetInfo petinfo[5];
}PetInfos;

typedef struct tagAi {
	SocketCheck identify;

	int autoBattleCheckState;
	int battleSpeed;
	int normalSpeed;

	int lowerEnemyCount;
	int lowerEnemyAct;
	int lowerEnemySkillbox;
	int lowerEnemySubSkillbox;
	int lowerEnemyItembox;
	int lowerEnemyTarget;

	int higherEnemyCount;
	int higherEnemyAct;
	int higherEnemySkillbox;
	int higherEnemySubSkillbox;
	int higherEnemyItembox;
	int higherEnemyTarget;

	int lowMpSpin;
	int lowMpAct;
	int lowMpSkillbox;
	int lowMpSubSkillbox;
	int lowMpItembox;
	int lowMpTarget;

	int normalAct;
	int normalSkillbox;
	int normalSubSkillbox;
	int normalItembox;
	int normalTarget;
	int autoSwitchSkillLevelCheckState;
	int lockSkillLvlToHighestCheckState;

	int secondAct;
	int secondSkillbox;
	int secondSubSkillbox;
	int secondItembox;
	int secondTarget;

	int skillHealCheckState[3];
	int skillHealLowHpSpin[3];
	int skillHealAllieLowHpSpin[3];
	int skillHealSkillbox[3];
	int skillHealSubSkillbox[3];
	int skillHealTarget[3];

	int itemHealCheckState;
	int itemHealLowHpSpin;
	int itemHealAllieLowHpSpin;
	wchar_t itemHealItem[260];

	int skillResurrectionCheckState;
	int skillResurrectionSkillbox;
	int skillResurrectionSubSkillbox;
	int skillResurrectionTarget;

	int outBattleItemHealHpCheckState;
	int outBattleCharItemHealHpSpin;
	int outBattlePetItemHealHpSpin;
	wchar_t outBattlePetItemHealHpEdit[260];

	int outBattleItemHealMpCheckState;
	int outBattleCharItemHealMpSpin;
	int outBattlePetItemHealMpSpin;
	wchar_t outBattlePetItemHealMpEdit[260];

	int pet_lowerEnemyCount;
	int pet_lowerEnemyAct;
	int pet_lowerEnemySkillbox;
	int pet_lowerEnemyTarget;

	int pet_higherEnemyCount;
	int pet_higherEnemyAct;
	int pet_higherEnemySkillbox;
	int pet_higherEnemyTarget;

	int pet_lowMpSpin;
	int pet_lowMpAct;
	int pet_lowMpSkillbox;
	int pet_lowMpTarget;

	int pet_normalSkillbox;
	int pet_normalTarget;

	int pet_secondSkillbox;
	int pet_secondTarget;

	int pet_lowHpCheckState;
	int pet_lowHpSpin;
	int pet_lowHpSkillbox;

	int fristRoundDelayCheck;
	int fristRoundDelaySpin;

	int regularRoundDelayCheck;
	int regularRoundDelaySpin;

	int disableBattleEffect;

	int auto_walk;
	int auto_walkDirection;
	int auto_walkLen;
	int auto_walkFreq;

	int disableDrawEffect;

	int outBattleAutoThrowItemCheckState;
	wchar_t outBattleAutoThrowItemEdit[260];
} AI;

typedef struct tagClientInfo {
	SocketCheck identify;
	TableInfo tableinfo;
	BattleInfo battleinfo;
	ItemInfo iteminfo;
	SkillInfo skillinfo;
	CharInfo charinfo;
	PetInfo petinfo[5];
} ClientInfo;

typedef struct tagInfo {
	BattleInfo* battleinfo;
	ItemInfo* iteminfo;
	SkillInfo* skillinfo;
	CharInfo* charinfo;
	PetInfo* petinfo[5];
	TableInfo* tableinfo;
	AI* aiinfo;
} PINFO;

enum class FORMATION
{
	THREAD_HOTKETFORMAT,
	THREAD_TITLEFORMAT,
	THREAD_WELCOMEFORMAT,
	THREAD_LOSTFORMAT,
	ASM_AUTOONFORMAT,
	ASM_AUTOOFFFORMAT,
	ASM_SPEEDCHANGED,
	ASM_AUTOLOGINONFORMAT,
	ASM_AUTOLOGINOFFFORMAT,
	ASM_CLEARDATAFORMAT,
	ASM_ANTICODEPATTERN,
	ASM_auto_walkSWITCHER_ON,
	ASM_auto_walkSWITCHER_OFF,
};

__forceinline const int BLUECDECL send_log(const std::wstring& wstr)
{
	CSocket& skt = CSocket::get_instance();
	Ann an = {};
	an.identify.cbsize = sizeof(Ann);
	an.identify.msg = ACT_ANNOUNCE;
	an.identify.gnum = skt.gnum;
	an.size = (int)wstr.size();
	_snwprintf_s(an.sz, an.size * 2, _TRUNCATE, L"%s\0", wstr.c_str());
	return 	skt.Write((char*)&an, sizeof(Ann));
}

#pragma endregion

//typedef void(__cdecl* pfnCreateScreen)(IN int type, IN int x, IN int y);
//typedef int(__cdecl* pfnAnnounce)(IN PCSTR type, IN int color, IN int size);
//typedef void(__cdecl* pfnDropGold)(IN SOCKET* socket, IN int x, IN int y, IN int amt);
//typedef void(__cdecl* pfnDropPet)(IN SOCKET* socket, IN int x, IN int y, IN int pos);
//typedef void(__cdecl* pfnDropItem)(IN SOCKET* socket, IN int x, IN int y, IN int dir, IN int pos);
//typedef void(__cdecl* pfnCollectItem)(IN SOCKET* socket, IN int x, IN int y, IN int dir);
//typedef void(__cdecl* pfnCloseScreen)(IN int type);
//typedef void(__cdecl* pfnAddPetAbility)(IN SOCKET* socket, IN int pos, IN int sec);
//typedef void(__cdecl* pfnAddCharAbility)(IN SOCKET* socket, IN int sec);
//typedef void(__cdecl* pfnCollecting)(IN SOCKET* socket, IN int type, IN int sec);
//typedef void(__cdecl* pfnMakerScreen)(IN SOCKET* socket, IN int type, IN int sec, IN int unknown);
//typedef void(__cdecl* pfnPetMailing)(IN SOCKET* socket, IN int tar, IN int petpos, IN int ipos, IN char* str, IN int unknown);
//typedef void(__cdecl* pfnMailing)(IN SOCKET* socket, IN int tar, IN char* str, IN int unknown);
//typedef void(__cdecl* pfnTalk)(IN SOCKET* socket, IN char* str, IN int color);
//typedef void(__cdecl* pfnSystemControl)(IN int type);
//typedef void(__cdecl* pfnReturnBase)(IN SOCKET* socket);
//typedef void(__cdecl* pfnPress)(IN int type, IN int row);
//typedef void(__cdecl* pfnSetBankPage)(IN SOCKET* socket, IN int page);
//typedef void(__cdecl* pfnuse_item)(IN SOCKET* socket, IN int x, IN int y, IN int pos, IN int unknown);

#pragma region CFormat

class CFormat
{
public:
	~CFormat() {
		//std::cout << "destructor called!" << std::endl;
	}
	CFormat(const CFormat&) = delete;
	CFormat& operator=(const CFormat&) = delete;
	static CFormat& get_instance() {
		static CFormat instance;
		return instance;
	}
private:
	explicit CFormat() {
		//std::cout << "constructor called!" << std::endl;
	}
public:
	void set(const FORMATION& e, std::wstring& wstr)
	{
		switch (e)
		{
		case FORMATION::THREAD_HOTKETFORMAT:
		{
			VMPBEGIN
				specialset(0x7fff, 0xff, 0xff0ff);
			VMPEND
				break;
		}
		case FORMATION::THREAD_TITLEFORMAT:
		{
			VMPBEGIN
				title(wstr);
			VMPEND
				break;
		}
		case FORMATION::THREAD_WELCOMEFORMAT:
		{
			VMPBEGIN
				welcome(wstr);
			VMPEND
				break;
		}
		case FORMATION::THREAD_LOSTFORMAT:
		{
			VMPBEGIN
				lost(wstr);
			VMPEND
				break;
		}
		case FORMATION::ASM_AUTOONFORMAT:
		{
			VMPBEGIN
				autoon(wstr);
			VMPEND
				break;
		}
		case FORMATION::ASM_AUTOOFFFORMAT:
		{
			VMPBEGIN
				autooff(wstr);
			VMPEND
				break;
		}
		case FORMATION::ASM_SPEEDCHANGED:
		{
			VMPBEGIN
				speedchanged(wstr);
			VMPEND
				break;
		}
		case FORMATION::ASM_AUTOLOGINONFORMAT:
		{
			VMPBEGIN
				autologinon(wstr);
			VMPEND
				break;
		}
		case FORMATION::ASM_AUTOLOGINOFFFORMAT:
		{
			VMPBEGIN
				autologinoff(wstr);
			VMPEND
				break;
		}
		case FORMATION::ASM_CLEARDATAFORMAT:
		{
			VMPBEGIN
				cleardata(wstr);
			VMPEND
				break;
		}
		case FORMATION::ASM_ANTICODEPATTERN:
		{
			VMPBEGIN
				anticodepattern(wstr);
			VMPEND
				break;
		}
		case FORMATION::ASM_auto_walkSWITCHER_ON:
		{
			VMPBEGIN
				auto_walkswitcheron(wstr);
			VMPEND
				break;
		}
		case FORMATION::ASM_auto_walkSWITCHER_OFF:
		{
			VMPBEGIN
				auto_walkswitcheroff(wstr);
			VMPEND
				break;
		}
		}
	}
private:
	void __cdecl title(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("{} [{}] - {}:{} {} {}:{}/{} {}:{}/{} - {}:{}:{}\0") };
		wstr = tl;
	}

	void __cdecl welcome(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>歡迎使用水藍小幫手 到期日期:{}-{}-{} {}:{}:{} 剩餘天數:{}\0") };
		wstr = tl;
	}

	void __cdecl lost(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>失去與小幫手的連線。\0") };
		wstr = tl;
	}

	void __cdecl specialset(const int& ia, const int& ib, const int& ic)
	{
		std::ignore = ia;
		std::ignore = ib;
		std::ignore = ic;
		const int pointer = 0x15C;
		const int base = MAKEADDR(0x5, 0xe, 0x8, 0x3, 0xd, 0x4);//0x05E83D4

		constexpr WCHAR w7[24] = { TEXT("走路遇敵開關\0") };
		char a7[MAX_PATH] = {};
		CRuntime::wchar2char(w7, a7);
		_snprintf_s((char*)(base - pointer * 7), 24, _TRUNCATE, "%s\0", a7);

		constexpr WCHAR w6[24] = { TEXT("\0") };
		char a6[MAX_PATH] = {};
		CRuntime::wchar2char(w6, a6);
		_snprintf_s((char*)(base - pointer * 6), 24, _TRUNCATE, "%s\0", a6);

		constexpr WCHAR w5[24] = { TEXT("\0") };
		char a5[MAX_PATH] = {};
		CRuntime::wchar2char(w5, a5);
		_snprintf_s((char*)(base - pointer * 5), 24, _TRUNCATE, "%s\0", a5);

		constexpr WCHAR w4[24] = { TEXT("\0") };
		char a4[MAX_PATH] = {};
		CRuntime::wchar2char(w4, a4);
		_snprintf_s((char*)(base - pointer * 4), 24, _TRUNCATE, "%s\0", a4);

		constexpr WCHAR w3[24] = { TEXT("\0") };
		char a3[MAX_PATH] = {};
		CRuntime::wchar2char(w3, a3);
		_snprintf_s((char*)(base - pointer * 3), 24, _TRUNCATE, "%s\0", a3);

		constexpr WCHAR w2[24] = { TEXT("戰鬥減速\0") };
		char a2[MAX_PATH] = {};
		CRuntime::wchar2char(w2, a2);
		_snprintf_s((char*)(base - pointer * 2), 24, _TRUNCATE, "%s\0", a2);

		constexpr WCHAR w1[24] = { TEXT("戰鬥加速\0") };
		char a1[MAX_PATH] = {};
		CRuntime::wchar2char(w1, a1);
		_snprintf_s((char*)(base - pointer * 1), 24, _TRUNCATE, "%s\0", a1);

		constexpr WCHAR w0[24] = { TEXT("AI自動戰鬥開關\0") };
		char a0[MAX_PATH] = {};
		CRuntime::wchar2char(w0, a0);
		_snprintf_s((char*)(base - pointer * 0), 24, _TRUNCATE, "%s\0", a0);
	}

	void autoon(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>AI自動戰鬥:開啟\0") };
		wstr = tl;
	}

	void autooff(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>AI自動戰鬥:關閉\0") };
		wstr = tl;
	}

	void speedchanged(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>戰鬥加速:\0") };
		wstr = tl;
	}

	void autologinon(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>自動登入:開啟") };
		wstr = tl;
	}

	void autologinoff(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>自動登入:關閉") };
		wstr = tl;
	}

	void cleardata(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>清空數據，自動驗證冷卻 60 秒") };
		wstr = tl;
	}

	void anticodepattern(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("[\\/][A-Z][\\s][\\d]{1,4}") };
		wstr = tl;
	}

	void auto_walkswitcheron(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>走路遇敵:開啟") };
		wstr = tl;
	}

	void auto_walkswitcheroff(std::wstring& wstr)
	{
		constexpr WCHAR tl[MAX_PATH] = { TEXT("<系統>走路遇敵:關閉") };
		wstr = tl;
	}
};
#pragma endregion

class CAsm
{
public:
	explicit CAsm();
	virtual ~CAsm();
	static CAsm* asm_ptr;

	DWORD is_auto_battle;
	DWORD is_auto_login = 0;
	DWORD is_auto_walk = 0;
	DWORD is_auto_walk_cache = 0;

	int battle_screen_type_cache;

	long is_hook_installed = false;

	DateTime dateline = {};
	PINFO g_info_ptr;
	DWORD AION = 0x000800;
	DWORD AIOFF = 0x000400;

private:
	enum statusbar
	{
		s_status,
		s_hp,
		s_maxhp,
		s_mp,
		s_maxmp,
	};

	DWORD m_pfnOrigWalkY = NULL;
	DWORD m_pfnOrigMove = NULL;
	DWORD m_pfnOrigHpIN = NULL;
	DWORD m_pfnOrigHpOUT = NULL;
	DWORD m_pfnOrigMessageBox = NULL;
	DWORD m_pfnOrigCreateCharScreen = NULL;
	DWORD m_pfnOrigCreatePetScreen = NULL;
	DWORD m_pfnOrigCreateRideScreen = NULL;
	DWORD m_pfnOrigHotKey = NULL;

	static DWORD m_pfnNewXcodeDis;

	std::vector<char*> anticode_cache_vec;

	unsigned int global_status_caches[5] = {};

	bool mutexoff = false;

public:
	LoginInformation login;
	//static DWORD xcodeaddr;
	static double global_static_battle_speed_cache;
	static double global_static_normal_speed_cache;

private:
	TableInfo table = {};

	typedef int(__cdecl CAsm::* ptrasm)(void);
	typedef int(__cdecl* ptra)(void);
	typedef void(__cdecl CAsm::* ptrasmnoreturn)(void);
	typedef int(__cdecl* ptrasmwitharg)(IN int a, IN int b, IN int c);
	typedef int(WINAPI CAsm::* fackmessagebox)(IN HWND hWnd, IN LPCSTR str, IN LPCSTR title, IN UINT flag);

	BYTE  m_bOldWalkBytesX[5];
	BYTE  m_bOldWalkBytesY[5];
	BYTE  m_bOldHpBytesIN[5];
	BYTE  m_bOldHpBytesOUT[5];
	BYTE  m_bOldMessageBoxBytes[17];
	BYTE  m_bOldCreateCharScreenBytes[5];
	BYTE  m_bOldCreatePetScreenBytes[5];
	BYTE  m_bOldCreateRideScreenBytes[5];
	BYTE  m_bOldHotKeyBytes[7];
	BYTE  m_bOldXcodeBytes[5];

	inline void BLUECDECL install_hook(ptrasm pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest = 0);
	inline void BLUECDECL install_hook(ptrasmnoreturn pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest = 0);
	inline void BLUECDECL install_hook(ptrasmwitharg pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest = 0);
	inline void BLUECDECL install_hook(fackmessagebox pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest = 0);
	inline void BLUECDECL install_hook(ptra pfnHookFunc, const DWORD& bOri, BYTE* bOld, BYTE* bNew, const int& nsize, const DWORD offest = 0);

	inline void BLUECDECL unhook(const DWORD& ori, const BYTE* oldBytes, const SIZE_T& size);
public:

	void BLUECDECL HOOK_global_install_hook();
	void BLUECDECL HOOK_global_unhook();
	inline void BLUECDECL memory_move(const DWORD& dis, const void* source, const size_t& size);
	void BLUECDECL disable_draw_effect(const bool& en);
	void BLUECDECL disable_battle_anime_effect(const bool& en);

	inline int BLUECDECL is_hd_mode();

	const TableInfo BLUECDECL get_table_info(const int& num, const int& prestatus);
	void BLUECDECL get_char_skill_info(SkillInfo& charskill);
	void BLUECDECL get_all_item_info(ItemInfo& iinfo);
	bool BLUECDECL get_battle_info(BattleInfo& btinfo);
	void BLUECDECL get_char_info(CharInfo& charinfo);
	void BLUECDECL get_pet_info(const int& index, PetInfo& petinfo);

	void BLUECDECL auto_login(const HWND& hWnd);
	void BLUECDECL auto_walk(CMap Map, const int& dir, const int& len, const int& x, const int& y, int& predir);
	int BLUECDECL anti_test_code(int& anti_code_counter_cache, std::wstring& lastoutput);
	void BLUECDECL auto_throw_item(int* status);
	void BLUECDECL auto_heal(int* status);

	static int BLUECDECL update_pet_position();
	inline void BLUECDECL get_char_xy(int& east, int& south);
	inline int BLUECDECL announcement(const std::wstring& wstr, const int& color = 4, const int& size = 3);

	int BLUECDECL hook_hotkey_extend(const BYTE& nret);

	void BLUECDECL script_thread(char* mychar);

private:
	int __cdecl hook_movement();
	int __cdecl hook_hp_mp_in_battle();
	int __cdecl hook_hp_mp_in_normal();
	//void __cdecl on_HookMessageBox();
	//void __cdecl on_HookXcode();
	static int __cdecl hook_create_battle_screen(int type, int x, int y);
	int __cdecl hook_hotkey();

	void BLUECDECL set_battle_memory_data(const bool& en);

	inline int BLUECDECL is_battle_screen_ready_for_hotkey();
	//

	int BLUECDECL script_receive(const ScriptCommand& scmd);
	void BLUECDECL script_run(char* mychar);

	inline void BLUECDECL click_main_server(const HWND& hWnd, const int& num, const int& sub);
	inline void BLUECDECL click_sub_server(const HWND& hWnd, const int& ser, const int& num);
	inline void BLUECDECL click_char(const HWND& hWnd, const int& num);
	inline void BLUECDECL click_to_close_disconnect_msg(const HWND& hWnd);

	inline void BLUECDECL normal_heal_by_item(const int& pos, const int& target);
	inline void BLUECDECL use_item(const int& pos);
	inline void BLUECDECL throw_items(const int index);
	inline void BLUECDECL switch_pet_position(const int& pos, const int& type);
	inline void BLUECDECL set_char_face_direction(const int& dir);
	void BLUECDECL pruduct_produce(const std::vector<std::wstring> v, const bool& en);
	inline void BLUECDECL buy_or_sell(const int& index, const int& amt, const int& type);
	void BLUECDECL bank_move_item(const int& src, const int& dst, const bool& isdepost = true);
	inline void BLUECDECL move_char_to(const int& x, const int& y);

	int BLUECDECL find_item_locate_spot(const std::wstring& wstr, const bool& en = false);
	const std::vector<int> BLUECDECL find_item_locate_spot(const std::wstring& wstr, int nothing, const bool& en = false);
	const std::vector<int> BLUECDECL find_bank_item_locate_spot(const std::wstring& wstr, int nothing, const bool& en);

	int BLUECDECL get_item_pos_by_id(const int& id);
	int BLUECDECL find_skill_by_name(const std::wstring& str, const int& level);
	int BLUECDECL find_item_by_divided_string(const std::wstring& wstr);

	void BLUECDECL get_child_skill_index_by_name(const std::wstring str, int& skill, int& child, const bool& en = false);

	void BLUECDECL get_private_sfc32dll(wchar_t* wstr)
	{
		VMPBEGIN
			const wchar_t a[MAX_PATH] = { L"sfc32.dll\0" };
		memory_move((DWORD)wstr, a, MAX_PATH);
		VMPEND
	}

private:////////////////////////////////*                 AI                 *//////////////////////////////////

	const std::vector<int> back_enemies = { 14, 12, 10, 11, 13 };
	const std::vector<int> front_enemies = { 19, 17, 15, 16, 18 };
	const std::vector<int> front_allies = { 9, 7, 5, 6, 8 };
	const std::vector<int> back_allies = { 4, 2, 0, 1, 3 };

	const std::vector<int> enemies_spot_order = { 18, 16 , 15, 17, 19 , 13, 11, 10, 12, 14 };
	const std::vector<int> allies_spot_order = { 9, 7, 5, 6, 8, 4, 2, 0, 1, 3 };

	//T型範圍目標編號
	const int t_target_position[20][4] = {
	{ 0, 1, 2, 5 },
	{ 1, 0, 6, 3 },
	{ 2, 0, 4, 7 },
	{ 3, 1, 8, -1 },
	{ 4, 2, 9, -1 },
	{ 5, 0, 6, 7 },
	{ 6, 1, 5, 8 },
	{ 7, 2, 5, 9 },
	{ 8, 3, 6, -1 },
	{ 9, 4, 7, -1 },
	{ 10, 11, 12, 15 },
	{ 11, 10, 13, 16 },
	{ 12, 10, 14, 17 },
	{ 13, 11, 18, -1 },
	{ 14, 12, 19, -1 },
	{ 15, 10, 16, 17 },
	{ 16, 11, 15, 18 },
	{ 17, 12, 15, 19 },
	{ 18, 13, 16, -1 },
	{ 19, 12, 17, -1 },
	};

	//被動技能
	const std::vector<std::wstring> char_passive_skill_vec = {
		L"抗毒",L"抗昏睡",L"抗石化",L"抗酒醉",L"抗混亂",L"抗遺忘",
		L"精靈的盟約",L"寵物強化",L"調教",L"防具修理",L"防具修理",
		L"急救",L"刻印",L"鑑定",L"變身",L"變裝",
		L"武器修理",L"治療",L"裝飾",L"挖掘",L"狩獵",
		L"伐木",L"製藥",L"料理",L"造盾",L"製鞋",
		L"製靴",L"製長袍",L"造衣服",L"防具修理",L"剪取",
		L"造鎧",L"製帽子",L"製頭盔",L"造小刀",L"造投擲武器",
		L"造杖",L"造弓",L"造槍",L"造斧",L"鑄劍",
		L"炸彈轉化",L"外傷藥製作",L"療傷藥製作",L"仙術鍊成",L"飾品鑄造",
		L"寵物項圈製造",L"寵物晶石製造",L"寵物輕裝製造",L"寵物重裝製造",L"寵物飾品製造",
	};

	enum class COMPARE
	{
		Self,
		Allies,
	};

	//動作類型
	enum class Action
	{
		BT_NotUsed,
		BT_EnemyLowerThan,
		BT_EnemyHigherThan,
		BT_CharMpLowerThan,
		BT_Normal,
		BT_SecondRound,
		BT_HealBySkill_0,
		BT_HealBySkill_1,
		BT_HealBySkill_2,
		normal_heal_by_item,
		BT_ResurrectionBySkill,
	};

	int enemy_min = 10;
	int enemy_max = 20;
	int allies_min = 0;
	int allies_max = 10;
	int char_current_size_cache = 0;

	//送出戰鬥指令
	inline void BLUECDECL battle_send_command(const std::wstring& cmd);

	//人物AI邏輯
	int BLUECDECL battle_char_auto_logic(const int& type);

	//寵物AI邏輯
	int BLUECDECL battle_pet_auto_logic(const int& type);

	//攻擊
	void BLUECDECL battle_attack(const int& target = 0xF);
	//防禦
	void BLUECDECL battle_defense();
	//逃跑
	void BLUECDECL battle_escape();
	//技能
	void BLUECDECL battle_skill(const int& skill, const int& subskill, const int& target = 0xF);
	//道具
	void BLUECDECL battle_item(const int& index, const int& target = 0xF);

	//輔助功能
	inline bool BLUECDECL is_skill_non_passive(const std::wstring& wcstr);

	//隊友是否死亡
	bool BLUECDECL is_allies_dead(const int& selected, const BattleInfo& bt, int& target, int petposition);

	bool BLUECDECL battle_cmp_allies(const int& value, const BattleInfo& bt, const COMPARE& type, int& target, int& healtype, int charposition = -1, int petposition = -1);

	//最終動作修正
	int BLUECDECL battle_last_correction(const AI& ai, const Action& acttype, const int& target);

	//人物選擇動作種類
	int BLUECDECL battle_char_select_action_type(const int& index, const int& skill, const int& sub, const int& item, const int& target);

	//寵物選擇動作種類
	int BLUECDECL battle_pet_select_action_type(const int& index, const int& skill, const int& target);

	//人物自動調整技能等級
	void BLUECDECL battle_auto_set_skill_level(const int& skill, int& subskill);

	//人物取技能最大等級
	int BLUECDECL battle_get_max_level(const int& skill);

	//修正攻擊目標
	void BLUECDECL battle_skill_target_correction(const BattleInfo& bt, const int& act, const int& selected, int& target, const int& petposition);

	//修正補血目標
	void BLUECDECL battle_healing_target_correction(const BattleInfo& bt, const int& selected, const int& value, const int& healtype, int& target);

	//戰鬥AI執行
	int BLUECDECL battle_ai_processing(const int& type);

	//取敵方順序位置
	int BLUECDECL get_enemy_spot_num_order(const BattleInfo& bt);

	//取友方順序位置
	int BLUECDECL get_allies_spot_num_order(const BattleInfo& bt);

private:
	enum StringValue {
		Not_Define,

		提示,
		查坐標,
		延時, //msleep
		消息, //messagebox
		輸入, //inputdialog
		允許開關,
		執行,
		暫停, //pause
		結束, //stop
		地圖,
		戰鬥中,
		取道具資訊,
		取寵物資訊,
		取人物資訊,
		取技能資訊,
		取地圖資訊,
		標記, //function, label
		跳轉, //jmp
		調用,
		返回, //ret
		比較, //cmp
		等於則跳轉, //je
		不等於則跳轉, //jne
		大於則跳轉, //jg
		大於等於則跳轉, //jge
		小於則跳轉, //jl
		小於等於則跳轉, //jle
		變數, //var
		自增, //++
		自減, //--
		格式化, //format

		設置,
		取消,
		聽見,

		說出,
		坐標,
		方向,
		尋路,

		輸出,

		回點,
		登出,
		清屏,
		按鈕,
		整理,
		使用道具,
		撿物,
		購買,
		售賣,
		丟棄道具,
		丟棄金幣,
		丟棄寵物,
		治療,
		採集,
		鑑定,
		製作,
		存入道具,
		提出道具,
		更換寵物,
		寵物郵件,

		Enum_Count,
	};
};

#endif
