#ifndef _M_IX86
#error "The following code only works for x86!"
#endif

#include "windows.h"
#include "TDebuger.h"
#include "../Runtime/CKernel32.h"
#include <exception>
#include <dbghelp.h>
//#pragma comment(lib,"Dbghelp.lib")

#include <string>
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <vector>
#include <assert.h>
#include <winternl.h>
#include <regex>
#include <assert.h>
#include <Tlhelp32.h>
#include <process.h>

//#pragma comment(lib,"ntdll.lib")

using namespace std;

bool TDebug::TPEB_BegingDebugged()
{
	bool BegingDebugged = false;
	__asm
	{
		mov eax, fs: [0x30] ;               //鳳PEB
		mov al, byte ptr ds : [eax + 0x2] ;//鳳Peb.BegingDebugged
		mov BegingDebugged, al;
	}
	return BegingDebugged;                //彆峈1寀佽隴淏婓掩覃彸
}

//毀覃彸2
bool TDebug::TNQIP_ProcessDebugPort()
{
	long nDebugPort = NULL;
	NtQueryInformationProcess(
		GetCurrentProcess(),//醴梓輛最曆梟
		ProcessDebugPort,   //脤戙陓洘腔濬倰
		&nDebugPort,        //怀堤脤戙腔陓洘
		sizeof(nDebugPort), //脤戙濬倰腔湮苤
		NULL);
	return nDebugPort == 0xFFFFFFFF ? true : false;
}

#pragma warning(push)
#pragma warning(disable:6387)
PVOID TDebug::TGetPEB64()
{
	PVOID pPeb = NULL;
#ifndef _WIN64
	// 1. There are two copies of PEB - PEB64 and PEB32 in WOW64 process
	// 2. PEB64 follows after PEB32
	// 3. This is true for version less then Windows 8, else __readfsdword returns address of real PEB64

	BOOL isWow64 = FALSE;
	CKernel32& ker32 = CKernel32::get_instance();
	if (ker32.My_IsWow64Process(ker32.My_GetCurrentProcess(), &isWow64))
	{
		if (isWow64)
		{
			pPeb = (PVOID)__readfsdword(0x0C * sizeof(PVOID));
			pPeb = (PVOID)((PBYTE)pPeb + 0x1000);
		}
	}

#endif

	return pPeb;
}
#pragma   warning(pop)

void TDebug::TCheckNtGlobalFlag()
{
	// PVOID pPeb = GetPEB();
	PVOID pPeb64 = TGetPEB64();
	DWORD offsetNtGlobalFlag = NULL;
#ifdef _WIN64
	offsetNtGlobalFlag = 0xBC;
#else
	offsetNtGlobalFlag = 0x68;
#endif
	//DWORD NtGlobalFlag = *(PDWORD)((PBYTE)pPeb + offsetNtGlobalFlag);
	//if (NtGlobalFlag & NT_GLOBAL_FLAG_DEBUGGED)
	//{
		//std::cout << "Stop debugging program!" << std::endl;
		//exit(-1);
	//}
	if (pPeb64)
	{
		DWORD NtGlobalFlagWow64 = *(PDWORD)((PBYTE)pPeb64 + 0xBC);
		if (NtGlobalFlagWow64 & NT_GLOBAL_FLAG_DEBUGGED)
		{
			//std::cout << "Stop debugging program!" << std::endl;
			CKernel32& ker32 = CKernel32::get_instance();
			ker32.My_TerminateProcess(ker32.My_GetCurrentProcess(), 0xBABEFACE);
		}
	}
}

PIMAGE_NT_HEADERS TDebug::TGetImageNtHeaders(PBYTE pImageBase)
{
	PIMAGE_DOS_HEADER pImageDosHeader = (PIMAGE_DOS_HEADER)pImageBase;
	return (PIMAGE_NT_HEADERS)(pImageBase + pImageDosHeader->e_lfanew);
}
PIMAGE_SECTION_HEADER TDebug::TFindRDataSection(PBYTE pImageBase)
{
	static const std::string rdata = ".rdata";
	PIMAGE_NT_HEADERS pImageNtHeaders = TGetImageNtHeaders(pImageBase);
	PIMAGE_SECTION_HEADER pImageSectionHeader = IMAGE_FIRST_SECTION(pImageNtHeaders);
	int n = 0;
	for (; n < pImageNtHeaders->FileHeader.NumberOfSections; ++n)
	{
		if (rdata == (char*)pImageSectionHeader[n].Name)
		{
			break;
		}
	}
	return &pImageSectionHeader[n];
}

void TDebug::TCheckGlobalFlagsClearInProcess()
{
	PBYTE pImageBase = (PBYTE)GetModuleHandle(NULL);
	PIMAGE_NT_HEADERS pImageNtHeaders = TGetImageNtHeaders(pImageBase);
	PIMAGE_LOAD_CONFIG_DIRECTORY pImageLoadConfigDirectory = (PIMAGE_LOAD_CONFIG_DIRECTORY)(pImageBase
		+ pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress);
	if (pImageLoadConfigDirectory->GlobalFlagsClear != 0)
	{
		//std::cout << "Stop debugging program!" << std::endl;
		CKernel32& ker32 = CKernel32::get_instance();
		ker32.My_TerminateProcess(ker32.My_GetCurrentProcess(), 0xBABEFACE);
	}
}

void TDebug::TCheckGlobalFlagsClearInFile()
{
	HANDLE hExecutable = INVALID_HANDLE_VALUE;
	HANDLE hExecutableMapping = NULL;
	PBYTE pMappedImageBase = NULL;
	CKernel32& ker32 = CKernel32::get_instance();
	__try
	{
		PBYTE pImageBase = (PBYTE)GetModuleHandle(NULL);
		PIMAGE_SECTION_HEADER pImageSectionHeader = TFindRDataSection(pImageBase);
		TCHAR pszExecutablePath[MAX_PATH];
		DWORD dwPathLength = GetModuleFileName(NULL, pszExecutablePath, MAX_PATH);
		if (0 == dwPathLength) __leave;
		hExecutable = CreateFile(pszExecutablePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (INVALID_HANDLE_VALUE == hExecutable) __leave;
		hExecutableMapping = CreateFileMapping(hExecutable, NULL, PAGE_READONLY, 0, 0, NULL);
		if (NULL == hExecutableMapping) __leave;
		pMappedImageBase = (PBYTE)ker32.My_MapViewOfFile(hExecutableMapping, FILE_MAP_READ, 0, 0,
			pImageSectionHeader->PointerToRawData + pImageSectionHeader->SizeOfRawData);
		if (NULL == pMappedImageBase) __leave;
		PIMAGE_NT_HEADERS pImageNtHeaders = TGetImageNtHeaders(pMappedImageBase);
		PIMAGE_LOAD_CONFIG_DIRECTORY pImageLoadConfigDirectory = (PIMAGE_LOAD_CONFIG_DIRECTORY)(pMappedImageBase
			+ (pImageSectionHeader->PointerToRawData
				+ (pImageNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress - pImageSectionHeader->VirtualAddress)));
		if (pImageLoadConfigDirectory->GlobalFlagsClear != 0)
		{
			//std::cout << "Stop debugging program!" << std::endl;

			ker32.My_TerminateProcess(ker32.My_GetCurrentProcess(), 0xBABEFACE);
		}
	}
	__finally
	{
		if (NULL != pMappedImageBase)
			ker32.My_UnmapViewOfFile(pMappedImageBase);
		if (NULL != hExecutableMapping)
			ker32.My_CloseHandle(hExecutableMapping);
		if (INVALID_HANDLE_VALUE != hExecutable)
			ker32.My_CloseHandle(hExecutable);
	}
}

int TDebug::TGetHeapFlagsOffset(bool x64)
{
	return x64 ?
		true ? 0x70 : 0x14 : //x64 offsets
		true ? 0x40 : 0x0C; //x86 offsets
}
int TDebug::TGetHeapForceFlagsOffset(bool x64)
{
	return x64 ?
		true ? 0x74 : 0x18 : //x64 offsets
		true ? 0x44 : 0x10; //x86 offsets
}
void TDebug::TCheckHeap()
{
	//PVOID pPeb = GetPEB();
	PVOID pPeb64 = TGetPEB64();
	PVOID heap = NULL;
	DWORD offsetProcessHeap = NULL;
	PDWORD heapFlagsPtr = NULL, heapForceFlagsPtr = NULL;
	//BOOL x64 = FALSE;
#ifdef _WIN64
	x64 = TRUE;
	offsetProcessHeap = 0x30;
#else
	offsetProcessHeap = 0x18;
#endif

	if (pPeb64)
	{
		heap = (PVOID) * (PDWORD_PTR)((PBYTE)pPeb64 + 0x30);
		heapFlagsPtr = (PDWORD)((PBYTE)heap + TGetHeapFlagsOffset(true));
		heapForceFlagsPtr = (PDWORD)((PBYTE)heap + TGetHeapForceFlagsOffset(true));
		if (*heapFlagsPtr & ~HEAP_GROWABLE || *heapForceFlagsPtr != 0)
		{
			CKernel32& ker32 = CKernel32::get_instance();
			ker32.My_TerminateProcess(ker32.My_GetCurrentProcess(), 0xBABEFACE);
		}
	}
}

int TDebug::TIsRemotePresent()
{
	BOOL isDebuggerPresent = FALSE;
	CKernel32& ker32 = CKernel32::get_instance();
	if (ker32.My_CheckRemoteDebuggerPresent(ker32.My_GetCurrentProcess(), &isDebuggerPresent))
	{
		if (isDebuggerPresent)
		{
			//std::cout << "Stop debugging program!" << std::endl;
			ker32.My_TerminateProcess(ker32.My_GetCurrentProcess(), 0xBABEFACE);
		}
	}
	return 0;
}

void TDebug::TPEBChecker()
{
	VMPBEGIN
		CKernel32& ker32 = CKernel32::get_instance();
	if (TPEB_BegingDebugged()) { ker32.My_TerminateProcess(ker32.My_GetCurrentProcess(), 0xBABEFACE); }
	if (TNQIP_ProcessDebugPort()) { ker32.My_TerminateProcess(ker32.My_GetCurrentProcess(), 0xBABEFACE); }

	TCheckNtGlobalFlag();
	//TCheckGlobalFlagsClearInProcess();
	//TCheckGlobalFlagsClearInFile();
	TCheckHeap();
	TIsRemotePresent();

	if (ker32.My_IsDebuggerPresent()) { ker32.My_TerminateProcess(ker32.My_GetCurrentProcess(), 0xBABEFACE); }
	VMPEND
}

DWORD TDebug::TCRC32(BYTE* ptr, DWORD Size)
{
	DWORD crcTable[256]{}, crcTmp1;

	// 雄怓汜傖CRC-32桶
	for (int i = 0; i < 256; i++)
	{
		crcTmp1 = (DWORD)i;
		for (int j = 8; j > 0; j--)
		{
			if (crcTmp1 & 1) crcTmp1 = (crcTmp1 >> 1) ^ 0xEDB88320L;
			else crcTmp1 >>= 1;
		}
		crcTable[i] = crcTmp1;
	}
	// 數呾CRC32硉
	DWORD crcTmp2 = 0xFFFFFFFF;
	while (Size--)
	{
		crcTmp2 = ((crcTmp2 >> 8) & 0x00FFFFFF) ^ crcTable[(crcTmp2 ^ (*ptr)) & 0xFF];
		ptr++;
	}
	return (crcTmp2 ^ 0xFFFFFFFF);
}

// 潰脤囀湔笢CRC32杻涽硉
DWORD TDebug::TMemoryChecker()
{
	PIMAGE_DOS_HEADER pDosHeader = NULL;
	PIMAGE_NT_HEADERS pNtHeader = NULL;
	PIMAGE_SECTION_HEADER pSecHeader = NULL;
	DWORD ImageBase;

	// 鳳價華硊
	ImageBase = (DWORD)GetModuleHandle(NULL);

	// 隅弇善PE芛賦凳
	pDosHeader = (PIMAGE_DOS_HEADER)ImageBase;
	pNtHeader = (PIMAGE_NT_HEADERS32)((DWORD)pDosHeader + pDosHeader->e_lfanew);

	pSecHeader = IMAGE_FIRST_SECTION(pNtHeader);
	DWORD va_base = ImageBase + pSecHeader->VirtualAddress;   // 隅弇測鎢誹va價華硊
	DWORD sec_len = pSecHeader->Misc.VirtualSize;             // 鳳測鎢誹酗僅

	DWORD CheckCRC32 = TCRC32((BYTE*)(va_base), sec_len);
	// printf(".text誹CRC32 = %x \n", CheckCRC32);
	return CheckCRC32;
}

BOOL TDebug::CheckDebug3()
{
	int result = 0;
	__asm
	{
		mov eax, fs: [30h]
		mov eax, [eax + 68h]
		and eax, 0x70
		mov result, eax
	}
	return result != 0;
}

BOOL TDebug::CheckDebug5()
{
	CONTEXT context = {};
	HANDLE hThread = GetCurrentThread();
	context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
	GetThreadContext(hThread, &context);
	if (context.Dr0 != 0 || context.Dr1 != 0 || context.Dr2 != 0 || context.Dr3 != 0)
	{
		return TRUE;
	}
	return FALSE;
}

BOOL TDebug::CheckDebug6()
{
	PIMAGE_DOS_HEADER pDosHeader;
	PIMAGE_NT_HEADERS32 pNtHeaders;
	PIMAGE_SECTION_HEADER pSectionHeader;
	DWORD dwBaseImage = (DWORD)GetModuleHandle(NULL);
	pDosHeader = (PIMAGE_DOS_HEADER)dwBaseImage;
	pNtHeaders = (PIMAGE_NT_HEADERS32)((DWORD)pDosHeader + pDosHeader->e_lfanew);
	pSectionHeader = (PIMAGE_SECTION_HEADER)((DWORD)pNtHeaders + sizeof(pNtHeaders->Signature) + sizeof(IMAGE_FILE_HEADER) +
		(WORD)pNtHeaders->FileHeader.SizeOfOptionalHeader);
	DWORD dwAddr = pSectionHeader->VirtualAddress + dwBaseImage;
	DWORD dwCodeSize = pSectionHeader->SizeOfRawData;
	DWORD checksum = NULL;
	__asm
	{
		cld
		mov     esi, dwAddr
		mov     ecx, dwCodeSize
		xor eax, eax
		checksum_loop :
		movzx    ebx, byte ptr[esi]
			add        eax, ebx
			rol eax, 1
			inc esi
			loop       checksum_loop
			mov checksum, eax
	}
	if (checksum != 0x46ea24)
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

BOOL TDebug::CheckDebug7()
{
	DWORD time1 = 0, time2 = 0;
	__asm
	{
		rdtsc
		mov time1, eax
		rdtsc
		mov time2, eax
	}
	if (time2 - time1 < 0xff)
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

static unsigned int crc32_tab[] = {
   0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
   0xe963a535, 0x9e6495a3,    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
   0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
   0xf3b97148, 0x84be41de,    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
   0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,    0x14015c4f, 0x63066cd9,
   0xfa0f3d63, 0x8d080df5,    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
   0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,    0x35b5a8fa, 0x42b2986c,
   0xdbbbc9d6, 0xacbcf940,    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
   0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
   0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
   0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,    0x76dc4190, 0x01db7106,
   0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
   0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
   0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
   0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
   0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
   0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
   0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
   0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
   0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
   0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
   0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
   0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
   0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
   0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
   0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
   0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
   0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
   0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
   0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
   0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
   0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
   0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
   0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
   0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
   0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
   0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
   0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
   0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
   0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
   0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
   0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
   0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

const unsigned int TDebug::getCrc32(unsigned int crc, const void* buf, int size) {
	const unsigned char* p;
	p = (unsigned char*)buf;
	crc = crc ^ ~0U;
	while (size--) {
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	}
	return crc ^ ~0U;
}

const wchar_t* TDebug::TExceptionDescription(DWORD code)
{
	switch (code) {
	case EXCEPTION_ACCESS_VIOLATION:         return L"EXCEPTION_ACCESS_VIOLATION";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return L"EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_BREAKPOINT:               return L"EXCEPTION_BREAKPOINT";
	case EXCEPTION_DATATYPE_MISALIGNMENT:    return L"EXCEPTION_DATATYPE_MISALIGNMENT";
	case EXCEPTION_FLT_DENORMAL_OPERAND:     return L"EXCEPTION_FLT_DENORMAL_OPERAND";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return L"EXCEPTION_FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_INEXACT_RESULT:       return L"EXCEPTION_FLT_INEXACT_RESULT";
	case EXCEPTION_FLT_INVALID_OPERATION:    return L"EXCEPTION_FLT_INVALID_OPERATION";
	case EXCEPTION_FLT_OVERFLOW:             return L"EXCEPTION_FLT_OVERFLOW";
	case EXCEPTION_FLT_STACK_CHECK:          return L"EXCEPTION_FLT_STACK_CHECK";
	case EXCEPTION_FLT_UNDERFLOW:            return L"EXCEPTION_FLT_UNDERFLOW";
	case EXCEPTION_ILLEGAL_INSTRUCTION:      return L"EXCEPTION_ILLEGAL_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR:            return L"EXCEPTION_IN_PAGE_ERROR";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:       return L"EXCEPTION_INT_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW:             return L"EXCEPTION_INT_OVERFLOW";
	case EXCEPTION_INVALID_DISPOSITION:      return L"EXCEPTION_INVALID_DISPOSITION";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return L"EXCEPTION_NONCONTINUABLE_EXCEPTION";
	case EXCEPTION_PRIV_INSTRUCTION:         return L"EXCEPTION_PRIV_INSTRUCTION";
	case EXCEPTION_SINGLE_STEP:              return L"EXCEPTION_SINGLE_STEP";
	case EXCEPTION_STACK_OVERFLOW:           return L"EXCEPTION_STACK_OVERFLOW";
	case EXCEPTION_GUARD_PAGE:                  return L"EXCEPTION_GUARD_PAGE";
	case EXCEPTION_INVALID_HANDLE:              return L"EXCEPTION_INVALID_HANDLE";
	default: return L"UNKNOWN EXCEPTION";
	}
}

void DisableSetUnhandledExceptionFilter();
long CALLBACK CrashInfocallback(_EXCEPTION_POINTERS* pexcp);

inline string FormatErrorMessage(DWORD error, const string& msg)
{
	static const int BUFFERLENGTH = 1024;
	vector<char> buf(BUFFERLENGTH);
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, error, 0, buf.data(),
		BUFFERLENGTH - 1, 0);
	return string(buf.data()) + "   (" + msg + ")";
}

class Win32Exception : public runtime_error
{
private:
	DWORD m_error = NULL;

public:

	Win32Exception(DWORD error, const string& msg) : runtime_error(FormatErrorMessage(error, msg)), m_error(error) { }

	DWORD GetErrorCode() const { return m_error; }
};

void DisableSetUnhandledExceptionFilter() {
	try {
		void* addr = (void*)SetUnhandledExceptionFilter;
		if (addr) {
			unsigned char code[16]{};
			int size = 0;

			code[size++] = 0x33;
			code[size++] = 0xC0;
			code[size++] = 0xC2;
			code[size++] = 0x04;
			code[size++] = 0x00;

			DWORD dwOldFlag, dwTempFlag;
			VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &dwOldFlag);
			WriteProcessMemory(GetCurrentProcess(), addr, code, size, NULL);
			VirtualProtect(addr, size, dwOldFlag, &dwTempFlag);
		}
	}
	catch (const Win32Exception& e) {
		// 异常处理
		SetLastError(e.GetErrorCode());
	}
}
#include <format>
long CALLBACK CrashInfocallback(_EXCEPTION_POINTERS* pexcp) {
	DWORD code = (DWORD)pexcp->ExceptionRecord->ExceptionAddress;

	std::wstring wstr(std::format(L"{:X}", code));
	MessageBox(0, wstr.c_str(), L"", 0);
	//throw pexcp;

	return EXCEPTION_CONTINUE_EXECUTION;
}

bool TDebug::TInitMinDump()
{
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)CrashInfocallback);
	int ret = GetLastError();
	DisableSetUnhandledExceptionFilter();
	return (ret == 0);
}

LONG CALLBACK TDebug::ExceptionHandlerProc(PEXCEPTION_POINTERS ExceptionInfo)
{
	PCONTEXT ctx = ExceptionInfo->ContextRecord;

	if (ctx->Dr0 != 0 || ctx->Dr1 != 0 || ctx->Dr2 != 0 || ctx->Dr3 != 0)
	{
		exit(1);
	}
	//DWORD code = (DWORD)ExceptionInfo->ExceptionRecord->ExceptionAddress;

	//std::wstring wstr(std::format(L"EXCEPTION ADDRESS:0x{:X}", code));
	//MessageBox(0, wstr.c_str(), L"ERROR", 0);

	ctx->Eip += 2;
	return EXCEPTION_CONTINUE_EXECUTION;
}

bool TDebug::TAntiVEH()
{
	AddVectoredExceptionHandler(0, TDebug::ExceptionHandlerProc);
	CKernel32& ker32 = CKernel32::get_instance();
	long ret = ker32.My_GetLastError();
	//__asm int 1h;
	return (ret == 0);
}