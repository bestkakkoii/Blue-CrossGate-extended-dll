#pragma once
#ifndef TDEBUGER_H
#define TDEBUGER_H
#include <windows.h>
#include "../Runtime/CKernel32.h"
#include "TMessage.h"

constexpr auto FLG_HEAP_ENABLE_TAIL_CHECK = 0x10;
constexpr auto FLG_HEAP_ENABLE_FREE_CHECK = 0x20;
constexpr auto FLG_HEAP_VALIDATE_PARAMETERS = 0x40;

class TDebug
{
#define NT_GLOBAL_FLAG_DEBUGGED (FLG_HEAP_ENABLE_TAIL_CHECK | FLG_HEAP_ENABLE_FREE_CHECK | FLG_HEAP_VALIDATE_PARAMETERS)

public:
	~TDebug() {
		//std::cout<<"destructor called!"<<std::endl;
	}
	TDebug(const TDebug&) = delete;
	TDebug& operator=(const TDebug&) = delete;
	static TDebug& get_instance() {
		static TDebug instance;
		return instance;
	}
private:
	explicit TDebug() {
		//std::cout<<"constructor called!"<<std::endl;
	}

public:
	DWORD OriginalCRC32 = NULL;
	bool WINAPI TAntiVEH();
	void WINAPI TPEBChecker();
	DWORD WINAPI TMemoryChecker();

	const unsigned int WINAPI getCrc32(unsigned int crc, const void* buf, int size);
private:

	DWORD TCRC32(BYTE* ptr, DWORD Size);
	bool WINAPI TPEB_BegingDebugged();
	bool WINAPI TNQIP_ProcessDebugPort();
	PVOID WINAPI TGetPEB64();
	void WINAPI TCheckNtGlobalFlag();
	PIMAGE_NT_HEADERS WINAPI TGetImageNtHeaders(PBYTE pImageBase);
	PIMAGE_SECTION_HEADER WINAPI TFindRDataSection(PBYTE pImageBase);
	void WINAPI TCheckGlobalFlagsClearInProcess();
	void WINAPI TCheckGlobalFlagsClearInFile();
	int WINAPI TGetHeapFlagsOffset(bool x64);
	int WINAPI TGetHeapForceFlagsOffset(bool x64);
	void WINAPI TCheckHeap();
	int WINAPI TIsRemotePresent();
	static LONG CALLBACK ExceptionHandlerProc(PEXCEPTION_POINTERS ExceptionInfo);

	BOOL WINAPI CheckDebug3();
	BOOL WINAPI CheckDebug5();
	BOOL WINAPI CheckDebug6();
	BOOL WINAPI CheckDebug7();

	const wchar_t* TExceptionDescription(DWORD code);
public:
	bool TInitMinDump();
};

#endif