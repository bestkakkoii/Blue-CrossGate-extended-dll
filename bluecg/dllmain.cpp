#include "dllmain.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AheadLib 命名空間
namespace AheadLib
{
	static DWORD m_dwReturn[2] = {};
	static FARPROC XEDParseAssemble = NULL;
	static FARPROC start = NULL;

	class CDllMain
	{
	public:

		BOOL m_bThreadFlag = false;
		HMODULE m_hModule = NULL;	// 原始模塊句柄
		CMain* thread = nullptr;

		static CDllMain* dllmain_ptr;

		explicit CDllMain()
		{
			dllmain_ptr = this;
		}

		virtual ~CDllMain()
		{
		}

		// 加載原始模塊
		BOOL WINAPI Load()
		{
			CKernel32& ker32 = CKernel32::get_instance();
			m_hModule = ker32.My_LoadLibraryW(L"sfc32.dll");
			if (m_hModule != NULL)
			{
				XEDParseAssemble = GetAddress("XEDParseAssemble");
				start = GetAddress("start");
			}
			return (m_hModule != NULL);
		}

		// 釋放原始模塊
		VOID WINAPI Free()
		{
			if (m_hModule)
			{
				CKernel32& ker32 = CKernel32::get_instance();
				ker32.My_FreeLibrary(m_hModule);
			}
		}

		// 獲取原始函數地址
		FARPROC WINAPI GetAddress(PCSTR pszProcName)
		{
			CKernel32& ker32 = CKernel32::get_instance();
			FARPROC fpAddress = (FARPROC)ker32.GetFunAddr((DWORD*)m_hModule, pszProcName);
			if (fpAddress == NULL)
			{
				ker32.My_ExitProcess(0);
				return NULL;
			}
			return fpAddress;
		}
	};

	CDllMain dllmainObj;
	CDllMain* CDllMain::dllmain_ptr = nullptr;
}

using namespace AheadLib;

DWORD CThread::dwTlsIndex;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 导出函数
ALCDECL AheadLib_XEDParseAssemble(void)
{
	// 保存返回地址到 TLS
	__asm PUSH m_dwReturn[0 * TYPE long];
	__asm CALL DWORD PTR[TlsSetValue];

	// 调用原始函数
	XEDParseAssemble();

	// 获取返回地址并返回
	__asm PUSH EAX;
	__asm PUSH m_dwReturn[0 * TYPE long];
	__asm CALL DWORD PTR[TlsGetValue];
	__asm XCHG EAX, [ESP];
	__asm RET;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 导出函数
ALCDECL AheadLib_start(void)
{
	// 保存返回地址到 TLS
	__asm PUSH m_dwReturn[1 * TYPE long];
	__asm CALL DWORD PTR[TlsSetValue];

	// 调用原始函数
	start();

	// 获取返回地址并返回
	__asm PUSH EAX;
	__asm PUSH m_dwReturn[1 * TYPE long];
	__asm CALL DWORD PTR[TlsGetValue];
	__asm XCHG EAX, [ESP];
	__asm RET;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 入口函數

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, PVOID pvReserved)
{
	CDllMain* m_ptr = CDllMain::dllmain_ptr;
	std::ignore = pvReserved;
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		if (!m_ptr->m_bThreadFlag)
		{
			m_ptr->m_bThreadFlag = true;
			// Allocate a TLS index.
			CThread::dwTlsIndex = TlsAlloc();
			CKernel32& ker32 = CKernel32::get_instance();
			TDebug& dbg = TDebug::get_instance();
			CRSA::get_instance();
			CFormat::get_instance();
			CSocket::get_instance();

			dbg.TAntiVEH();
			//dbg.TInitMinDump();
			ker32.My_DisableThreadLibraryCalls(hModule);
			m_ptr->thread = new CMain;
			m_ptr->thread->start();
		}

		for (DWORD i = 0; i < sizeof(m_dwReturn) / sizeof(DWORD); i++)
		{
			m_dwReturn[i] = TlsAlloc();
		}

		return m_ptr->Load();
	}
	case DLL_PROCESS_DETACH:
	{
		if (m_ptr->m_bThreadFlag)
		{
			m_ptr->m_bThreadFlag = false;
			m_ptr->thread->requestInterruption();
			m_ptr->thread->wait();
			delete m_ptr->thread;
			m_ptr->thread = nullptr;
			TlsFree(CThread::dwTlsIndex);
		}

		for (DWORD i = 0; i < sizeof(m_dwReturn) / sizeof(DWORD); i++)
		{
			TlsFree(m_dwReturn[i]);
		}
		m_ptr->Free();
		break;
	}
	default:
		break;
	}

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////