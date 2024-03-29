#pragma once
#ifndef CKERNEL32_H
#define CKERNEL32_H
#include <windows.h>
#include <versionhelpers.h>
#include <psapi.h>
#include "../TDebug/TMessage.h"
#include <TlHelp32.h>

inline __declspec(naked) DWORD* GetKernel32()
{
	__asm
	{
		mov eax, fs: [0x30] ;
		mov eax, [eax + 0xC];
		mov eax, [eax + 0x1C];
		mov eax, [eax];
		mov eax, [eax];
		mov eax, [eax + 8];
		ret;
	}
}

class CKernel32
{
public:
	~CKernel32() {
		//std::cout<<"destructor called!"<<std::endl;
	}
	CKernel32(const CKernel32&) = delete;
	CKernel32& operator=(const CKernel32&) = delete;
	static CKernel32& get_instance() {
		static CKernel32 instance;
		return instance;
	}
private:
	explicit CKernel32() {
		VMPBEGIN
			installUndocumentApi();
		VMPEND
	}
public:
	DWORD WINAPI GetFunAddr(const DWORD* DllBase, const char* FunName)
	{
		// 遍历导出表
		PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)DllBase;
		PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + (DWORD)pDos);
		PIMAGE_OPTIONAL_HEADER pOt = (PIMAGE_OPTIONAL_HEADER)&pNt->OptionalHeader;
		PIMAGE_EXPORT_DIRECTORY pExport = (PIMAGE_EXPORT_DIRECTORY)(pOt->DataDirectory[0].VirtualAddress + (DWORD)DllBase);
		// 获取到ENT、EOT、EAT
		DWORD* pENT = (DWORD*)(pExport->AddressOfNames + (DWORD)DllBase);
		WORD* pEOT = (WORD*)(pExport->AddressOfNameOrdinals + (DWORD)DllBase);
		DWORD* pEAT = (DWORD*)(pExport->AddressOfFunctions + (DWORD)DllBase);

		for (DWORD i = 0; i < pExport->NumberOfNames; ++i)
		{
			char* Name = (char*)(pENT[i] + (DWORD)DllBase);
			if (!strcmp(Name, FunName))
				return pEAT[pEOT[i]] + (DWORD)DllBase;
		}
		return (DWORD)NULL;
	}
private:

private:
#define NT_SUCCESS(x) ((x) >= 0)
#define DefApiFun(name)\
	decltype(name)* My_##name = NULL;
#define DefineFuncPtr(base, name) My_##name = (decltype(name)*)GetFunAddr(base, (char*)#name)
	typedef HANDLE(CALLBACK* HHeapCreate)(_In_ DWORD flOptions, _In_ SIZE_T dwInitialSize, _In_ SIZE_T dwMaximumSize);
	typedef LPVOID(CALLBACK* LPHeapAlloc)(_In_ HANDLE hHeap, _In_ DWORD dwFlags, _In_ SIZE_T dwBytes);
	typedef BOOL(CALLBACK* BHeapFree)(_Inout_ HANDLE hHeap, _In_ DWORD dwFlags, __drv_freesMem(Mem) _Frees_ptr_opt_ LPVOID lpMem);

	typedef struct {
		unsigned short Length;
		unsigned short MaximumLength;
		unsigned short* Buffer;
	} UNICODE_STRING;
	typedef UNICODE_STRING* PUNICODE_STRING;

	typedef struct _CLIENT_ID {
		HANDLE UniqueProcess;
		HANDLE UniqueThread;
	} CLIENT_ID;
	typedef CLIENT_ID* PCLIENT_ID;

	typedef struct _OBJECT_ATTRIBUTES {
		ULONG Length;
		HANDLE RootDirectory;
		PUNICODE_STRING ObjectName;
		ULONG Attributes;
		PVOID SecurityDescriptor;
		PVOID SecurityQualityOfService;
	} OBJECT_ATTRIBUTES, * POBJECT_ATTRIBUTES;
	typedef CONST OBJECT_ATTRIBUTES* PCOBJECT_ATTRIBUTES;

	typedef struct {
		unsigned long AllocationSize;
		unsigned long ActualSize;
		unsigned long Flags;
		unsigned long Unknown1;
		UNICODE_STRING Unknown2;
		void* InputHandle;
		void* OutputHandle;
		void* ErrorHandle;
		UNICODE_STRING CurrentDirectory;
		void* CurrentDirectoryHandle;
		UNICODE_STRING SearchPaths;
		UNICODE_STRING ApplicationName;
		UNICODE_STRING CommandLine;
		void* EnvironmentBlock;
		unsigned long Unknown[9];
		UNICODE_STRING Unknown3;
		UNICODE_STRING Unknown4;
		UNICODE_STRING Unknown5;
		UNICODE_STRING Unknown6;
	} PROCESS_PARAMETERS;

	typedef struct {
		unsigned long AllocationSize;
		unsigned long Unknown1;
		void* ProcessHinstance;
		void* ListDlls;
		PROCESS_PARAMETERS* ProcessParameters;
		unsigned long Unknown2;
		void* Heap;
	} PEB;

	typedef struct {
		unsigned int ExitStatus;
		PEB* PebBaseAddress;
		unsigned int AffinityMask;
		unsigned int BasePriority;
		unsigned long UniqueProcessId;
		unsigned long InheritedFromUniqueProcessId;
	} PROCESS_BASIC_INFORMATION;

	typedef struct _PROCESS_BASIC_INFORMATION32 {
		NTSTATUS ExitStatus;
		UINT32 PebBaseAddress;
		UINT32 AffinityMask;
		UINT32 BasePriority;
		UINT32 UniqueProcessId;
		UINT32 InheritedFromUniqueProcessId;
	} PROCESS_BASIC_INFORMATION32;

	typedef struct _UNICODE_STRING32 {
		USHORT Length;
		USHORT MaximumLength;
		PWSTR Buffer;
	} UNICODE_STRING32, * PUNICODE_STRING32;

	typedef struct _PEB32 {
		UCHAR InheritedAddressSpace;
		UCHAR ReadImageFileExecOptions;
		UCHAR BeingDebugged;
		UCHAR BitField;
		ULONG Mutant;
		ULONG ImageBaseAddress;
		ULONG Ldr;
		ULONG ProcessParameters;
		ULONG SubSystemData;
		ULONG ProcessHeap;
		ULONG FastPebLock;
		ULONG AtlThunkSListPtr;
		ULONG IFEOKey;
		ULONG CrossProcessFlags;
		ULONG UserSharedInfoPtr;
		ULONG SystemReserved;
		ULONG AtlThunkSListPtr32;
		ULONG ApiSetMap;
	} PEB32, * PPEB32;

	typedef struct _PEB_LDR_DATA32 {
		ULONG Length;
		BOOLEAN Initialized;
		ULONG SsHandle;
		LIST_ENTRY32 InLoadOrderModuleList;
		LIST_ENTRY32 InMemoryOrderModuleList;
		LIST_ENTRY32 InInitializationOrderModuleList;
		ULONG EntryInProgress;
	} PEB_LDR_DATA32, * PPEB_LDR_DATA32;

	typedef struct _LDR_DATA_TABLE_ENTRY32 {
		LIST_ENTRY32 InLoadOrderLinks;
		LIST_ENTRY32 InMemoryOrderModuleList;
		LIST_ENTRY32 InInitializationOrderModuleList;
		ULONG DllBase;
		ULONG EntryPoint;
		ULONG SizeOfImage;
		UNICODE_STRING32 FullDllName;
		UNICODE_STRING32 BaseDllName;
		ULONG Flags;
		USHORT LoadCount;
		USHORT TlsIndex;
		union {
			LIST_ENTRY32 HashLinks;
			ULONG SectionPointer;
		};
		ULONG CheckSum;
		union {
			ULONG TimeDateStamp;
			ULONG LoadedImports;
		};
		ULONG EntryPointActivationContext;
		ULONG PatchInformation;
	} LDR_DATA_TABLE_ENTRY32, * PLDR_DATA_TABLE_ENTRY32;

	typedef struct _PROCESS_BASIC_INFORMATION64 {
		NTSTATUS ExitStatus;
		UINT32 Reserved0;
		UINT64 PebBaseAddress;
		UINT64 AffinityMask;
		UINT32 BasePriority;
		UINT32 Reserved1;
		UINT64 UniqueProcessId;
		UINT64 InheritedFromUniqueProcessId;
	} PROCESS_BASIC_INFORMATION64;
	typedef struct _PEB64 {
		UCHAR InheritedAddressSpace;
		UCHAR ReadImageFileExecOptions;
		UCHAR BeingDebugged;
		UCHAR BitField;
		ULONG64 Mutant;
		ULONG64 ImageBaseAddress;
		ULONG64 Ldr;
		ULONG64 ProcessParameters;
		ULONG64 SubSystemData;
		ULONG64 ProcessHeap;
		ULONG64 FastPebLock;
		ULONG64 AtlThunkSListPtr;
		ULONG64 IFEOKey;
		ULONG64 CrossProcessFlags;
		ULONG64 UserSharedInfoPtr;
		ULONG SystemReserved;
		ULONG AtlThunkSListPtr32;
		ULONG64 ApiSetMap;
	} PEB64, * PPEB64;

	typedef struct _PEB_LDR_DATA64 {
		ULONG Length;
		BOOLEAN Initialized;
		ULONG64 SsHandle;
		LIST_ENTRY64 InLoadOrderModuleList;
		LIST_ENTRY64 InMemoryOrderModuleList;
		LIST_ENTRY64 InInitializationOrderModuleList;
		ULONG64 EntryInProgress;
	} PEB_LDR_DATA64, * PPEB_LDR_DATA64;

	typedef struct _UNICODE_STRING64 {
		USHORT Length;
		USHORT MaximumLength;
		ULONG64 Buffer;
	} UNICODE_STRING64, * PUNICODE_STRING64;

	typedef struct _LDR_DATA_TABLE_ENTRY64 {
		LIST_ENTRY64 InLoadOrderLinks;
		LIST_ENTRY64 InMemoryOrderModuleList;
		LIST_ENTRY64 InInitializationOrderModuleList;
		ULONG64 DllBase;
		ULONG64 EntryPoint;
		ULONG SizeOfImage;
		UNICODE_STRING64 FullDllName;
		UNICODE_STRING64 BaseDllName;
		ULONG Flags;
		USHORT LoadCount;
		USHORT TlsIndex;
		union {
			LIST_ENTRY64 HashLinks;
			ULONG64 SectionPointer;
		};
		ULONG CheckSum;
		union {
			ULONG TimeDateStamp;
			ULONG64 LoadedImports;
		};
		ULONG64 EntryPointActivationContext;
		ULONG64 PatchInformation;
	} LDR_DATA_TABLE_ENTRY64, * PLDR_DATA_TABLE_ENTRY64;

	typedef struct _IO_STATUS_BLOCK {
		union {
			NTSTATUS Status;
			PVOID Pointer;
		} DUMMYUNIONNAME;

		ULONG_PTR Information;
	} IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;

	typedef struct _INITIAL_TEB {
		PVOID                StackBase;
		PVOID                StackLimit;
		PVOID                StackCommit;
		PVOID                StackCommitMax;
		PVOID                StackReserved;
	} INITIAL_TEB, * PINITIAL_TEB;

#ifndef InitializeObjectAttributes
#define InitializeObjectAttributes(p, n, a, r, s) \
    {                                             \
        (p)->Length = sizeof(OBJECT_ATTRIBUTES);  \
        (p)->RootDirectory = r;                   \
        (p)->Attributes = a;                      \
        (p)->ObjectName = n;                      \
        (p)->SecurityDescriptor = s;              \
        (p)->SecurityQualityOfService = NULL;     \
    }
#endif

	typedef enum class _OBJECT_INFORMATION_CLASS {
		ObjectBasicInformation,
		ObjectNameInformation,
		ObjectTypeInformation,
		ObjectAllInformation,
		ObjectDataInformation
	} OBJECT_INFORMATION_CLASS,
		* POBJECT_INFORMATION_CLASS;

	typedef enum PROCESS_INFORMATION_CLASSX {
		ProcessBasicInformations,
		ProcessQuotaLimits,
		ProcessIoCounters,
		ProcessVmCounters,
		ProcessTimes,
		ProcessBasePriority,
		ProcessRaisePriority,
		ProcessDebugPort,
		ProcessExceptionPort,
		ProcessAccessToken,
		ProcessLdtInformation,
		ProcessLdtSize,
		ProcessDefaultHardErrorMode,
		ProcessIoPortHandlers,
		ProcessPooledUsageAndLimits,
		ProcessWorkingSetWatch,
		ProcessUserModeIOPL,
		ProcessEnableAlignmentFaultFixup,
		ProcessPriorityClass,
		ProcessWx86Information,
		ProcessHandleCount,
		ProcessAffinityMask,
		ProcessPriorityBoost,
		MaxProcessInfoClass
	} PROCESS_INFORMATION_CLASSX, * PPROCESS_INFORMATION_CLASSX;

	typedef enum _POOL_TYPE {
		NonPagedPool,
		NonPagedPoolExecute = NonPagedPool,
		PagedPool,
		NonPagedPoolMustSucceed = NonPagedPool + 2,
		DontUseThisType,
		NonPagedPoolCacheAligned = NonPagedPool + 4,
		PagedPoolCacheAligned,
		NonPagedPoolCacheAlignedMustS = NonPagedPool + 6,
		MaxPoolType,
		NonPagedPoolBase = 0,
		NonPagedPoolBaseMustSucceed = NonPagedPoolBase + 2,
		NonPagedPoolBaseCacheAligned = NonPagedPoolBase + 4,
		NonPagedPoolBaseCacheAlignedMustS = NonPagedPoolBase + 6,
		NonPagedPoolSession = 32,
		PagedPoolSession = NonPagedPoolSession + 1,
		NonPagedPoolMustSucceedSession = PagedPoolSession + 1,
		DontUseThisTypeSession = NonPagedPoolMustSucceedSession + 1,
		NonPagedPoolCacheAlignedSession = DontUseThisTypeSession + 1,
		PagedPoolCacheAlignedSession = NonPagedPoolCacheAlignedSession + 1,
		NonPagedPoolCacheAlignedMustSSession = PagedPoolCacheAlignedSession + 1,
		NonPagedPoolNx = 512,
		NonPagedPoolNxCacheAligned = NonPagedPoolNx + 4,
		NonPagedPoolSessionNx = NonPagedPoolNx + 32,
	} POOL_TYPE;

	typedef enum class _SYSTEM_INFORMATION_CLASS {
		SystemBasicInformation, // 0        Y        N
		SystemProcessorInformation, // 1        Y        N
		SystemPerformanceInformation, // 2        Y        N
		SystemTimeOfDayInformation, // 3        Y        N
		SystemNotImplemented1, // 4        Y        N
		SystemProcessesAndThreadsInformation, // 5       Y        N
		SystemCallCounts, // 6        Y        N
		SystemConfigurationInformation, // 7        Y        N
		SystemProcessorTimes, // 8        Y        N
		SystemGlobalFlag, // 9        Y        Y
		SystemNotImplemented2, // 10       Y        N
		SystemModuleInformation, // 11       Y        N
		SystemLockInformation, // 12       Y        N
		SystemNotImplemented3, // 13       Y        N
		SystemNotImplemented4, // 14       Y        N
		SystemNotImplemented5, // 15       Y        N
		SystemHandleInformation, // 16       Y        N
		SystemObjectInformation, // 17       Y        N
		SystemPagefileInformation, // 18       Y        N
		SystemInstructionEmulationCounts, // 19       Y        N
		SystemInvalidInfoClass1, // 20
		SystemCacheInformation, // 21       Y        Y
		SystemPoolTagInformation, // 22       Y        N
		SystemProcessorStatistics, // 23       Y        N
		SystemDpcInformation, // 24       Y        Y
		SystemNotImplemented6, // 25       Y        N
		SystemLoadImage, // 26       N        Y
		SystemUnloadImage, // 27       N        Y
		SystemTimeAdjustment, // 28       Y        Y
		SystemNotImplemented7, // 29       Y        N
		SystemNotImplemented8, // 30       Y        N
		SystemNotImplemented9, // 31       Y        N
		SystemCrashDumpInformation, // 32       Y        N
		SystemExceptionInformation, // 33       Y        N
		SystemCrashDumpStateInformation, // 34       Y        Y/N
		SystemKernelDebuggerInformation, // 35       Y        N
		SystemContextSwitchInformation, // 36       Y        N
		SystemRegistryQuotaInformation, // 37       Y        Y
		SystemLoadAndCallImage, // 38       N        Y
		SystemPrioritySeparation, // 39       N        Y
		SystemNotImplemented10, // 40       Y        N
		SystemNotImplemented11, // 41       Y        N
		SystemInvalidInfoClass2, // 42
		SystemInvalidInfoClass3, // 43
		SystemTimeZoneInformation, // 44       Y        N
		SystemLookasideInformation, // 45       Y        N
		SystemSetTimeSlipEvent, // 46       N        Y
		SystemCreateSession, // 47       N        Y
		SystemDeleteSession, // 48       N        Y
		SystemInvalidInfoClass4, // 49
		SystemRangeStartInformation, // 50       Y        N
		SystemVerifierInformation, // 51       Y        Y
		SystemAddVerifier, // 52       N        Y
		SystemSessionProcessesInformation // 53       Y        N
	} SYSTEM_INFORMATION_CLASS;

	typedef struct _OBJECT_NAME_INFORMATION {
		UNICODE_STRING Name;
		WCHAR* NameBuffer;
	} OBJECT_NAME_INFORMATION, * POBJECT_NAME_INFORMATION;

	typedef struct _OBJECT_TYPE_INFORMATION {
		UNICODE_STRING TypeName;
		ULONG TotalNumberOfHandles;
		ULONG TotalNumberOfObjects;
		WCHAR Unused1[8];
		ULONG HighWaterNumberOfHandles;
		ULONG HighWaterNumberOfObjects;
		WCHAR Unused2[8];
		ACCESS_MASK InvalidAttributes;
		GENERIC_MAPPING GenericMapping;
		ACCESS_MASK ValidAttributes;
		BOOLEAN SecurityRequired;
		BOOLEAN MaintainHandleCount;
		USHORT MaintainTypeList;
		POOL_TYPE PoolType;
		ULONG DefaultPagedPoolCharge;
		ULONG DefaultNonPagedPoolCharge;
	} OBJECT_TYPE_INFORMATION, * POBJECT_TYPE_INFORMATION;

	typedef struct _SYSTEM_HANDLE_INFORMATION {
		ULONG ProcessId; //进程ID
		UCHAR ObjectTypeNumber;
		UCHAR Flags;
		USHORT Handle; //句柄
		PVOID Object; //句柄对象
		ACCESS_MASK GrantedAccess;
	} SYSTEM_HANDLE, * PSYSTEM_HANDLE;

	typedef struct SYSTEM_HANDLE_INFORMATION {
		ULONG NumberOfHandles; //数组数量
		SYSTEM_HANDLE Information[1]; //数组指针
	} SYSTEM_HANDLE_INFORMATIO, * PSYSTEM_HANDLE_INFORMATION;

	typedef struct _PEB_LDR_DATA {
		ULONG                   Length;
		BOOLEAN                 Initialized;
		PVOID                   SsHandle;
		LIST_ENTRY              InLoadOrderModuleList;          //按加載順序
		LIST_ENTRY              InMemoryOrderModuleList;        //按內存順序
		LIST_ENTRY              InInitializationOrderModuleList;//按初始化順序
		PVOID          EntryInProgress;
	} PEB_LDR_DATA, * PPEB_LDR_DATA;

	//每個模塊信息的LDR_MODULE部分
	typedef struct _LDR_MODULE {
		LIST_ENTRY              InLoadOrderModuleList;
		LIST_ENTRY              InMemoryOrderModuleList;
		LIST_ENTRY              InInitializationOrderModuleList;
		PVOID                   BaseAddress;
		PVOID                   EntryPoint;
		ULONG                   SizeOfImage;
		UNICODE_STRING          FullDllName;
		UNICODE_STRING          BaseDllName;
		ULONG                   Flags;
		SHORT                   LoadCount;
		SHORT                   TlsIndex;
		LIST_ENTRY              HashTableEntry;
		ULONG                   TimeDateStamp;
	} LDR_MODULE, * PLDR_MODULE;

	//定义函数NtQueryInformationProcess的指针类型
	typedef NTSTATUS(NTAPI* pfnNtQueryInformationProcess)(
		HANDLE ProcessHandle, ULONG ProcessInformationClass,
		PVOID ProcessInformation, UINT32 ProcessInformationLength,
		UINT32* ReturnLength);

	typedef NTSTATUS(NTAPI* pfnNtQuerySystemInformation)(
		SYSTEM_INFORMATION_CLASS SystemInformationClass,
		PVOID SystemInformation,
		ULONG SystemInformationLength,
		PULONG ReturnLength);

	typedef VOID(NTAPI* PIO_APC_ROUTINE)(IN PVOID ApcContext,
		IN PIO_STATUS_BLOCK IoStatusBlock,
		IN ULONG Reserved);

	typedef NTSTATUS(NTAPI* pfnNtWow64QueryInformationProcess64)(
		IN HANDLE ProcessHandle, IN ULONG ProcessInformationClass,
		OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
		OUT PULONG ReturnLength OPTIONAL);

	typedef NTSTATUS(NTAPI* pfnNtWow64ReadVirtualMemory64)(
		IN HANDLE ProcessHandle, IN PVOID64 BaseAddress, OUT PVOID Buffer,
		IN ULONG64 Size, OUT PULONG64 NumberOfBytesRead);

	typedef NTSTATUS(NTAPI* pfnNtWow64WriteVirtualMemory64)(
		IN HANDLE ProcessHandle, IN ULONG64 BaseAddress, OUT PVOID BufferData,
		IN ULONG64 BufferLength, OUT PULONG64 ReturnLength OPTIONAL);

	typedef NTSTATUS(NTAPI* pfnZwOpenProcess)(
		OUT PHANDLE ProcessHandle, IN ACCESS_MASK DesiredAccess,
		IN POBJECT_ATTRIBUTES ObjectAttributes, IN PCLIENT_ID ClientId OPTIONAL);

	typedef NTSTATUS(NTAPI* pfnNtReadVirtualMemory)(
		IN HANDLE hProcess, IN LPCVOID lpBaseAddress, OUT LPVOID lpBuffer,
		IN DWORD nSize, OUT LPDWORD lpNumberOfBytesRead);

	typedef NTSTATUS(NTAPI* pfnNtWriteVirtualMemory)(
		IN HANDLE hProcess, OUT LPVOID lpBaseAddress, IN LPVOID lpBuffer,
		IN DWORD nSize, OUT LPDWORD lpNumberOfBytesWritten);

	typedef NTSTATUS(NTAPI* pfnZwVirtualProtect)(LPVOID lpAddress, DWORD dwSize,
		DWORD flNewProtect,
		PDWORD lpflOldProtect);

	typedef NTSTATUS(NTAPI* pfnZwVirtualProtectEx)(HANDLE hProcess,
		LPVOID lpAddress, SIZE_T dwSize,
		DWORD flNewProtect,
		PDWORD lpflOldProtect);

	typedef NTSTATUS(NTAPI* pfnCreateRemoteThread)(
		IN HANDLE hProcess, IN LPSECURITY_ATTRIBUTES lpThreadAttributes,
		IN SIZE_T dwStackSize, IN LPTHREAD_START_ROUTINE lpStartAddress,
		IN LPVOID lpParameter, IN DWORD dwCreationFlags, OUT LPDWORD lpThreadId);

	typedef NTSTATUS(NTAPI* pfnNtWriteVirtualMemory)(HANDLE ProcessHandle,
		PVOID BaseAddress,
		PVOID Buffer,
		ULONG NumberOfBytesToWrite,
		PULONG NumberOfBytesWritten);

	typedef NTSTATUS(NTAPI* pfnNtOpenProcess)(PHANDLE ProcessHandle,
		ACCESS_MASK DesiredAccess,
		POBJECT_ATTRIBUTES ObjectAttributes,
		PCLIENT_ID ClientId);

	typedef NTSTATUS(NTAPI* pfnNtOpenThread)(PHANDLE ThreadHandle,
		ACCESS_MASK AccessMask,
		POBJECT_ATTRIBUTES ObjectAttributes,
		PCLIENT_ID);

	typedef NTSTATUS(NTAPI* pfnNtSuspendThread)(HANDLE ThreadHandle,
		PULONG SuspendCount);

	typedef NTSTATUS(NTAPI* pfnNtAlertResumeThread)(HANDLE ThreadHandle,
		PULONG SuspendCount);

	typedef NTSTATUS(NTAPI* pfnNtAllocateVirtualMemory)(
		HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits,
		PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);

	typedef NTSTATUS(NTAPI* pfnNtQueueApcThread)(
		HANDLE ThreadHandle, PIO_APC_ROUTINE ApcRoutine,
		PVOID ApcRoutineContext OPTIONAL, PIO_STATUS_BLOCK ApcStatusBlock OPTIONAL,
		ULONG ApcReserved OPTIONAL);

	typedef NTSTATUS(NTAPI* pfnNtCreateThreadEx)(
		OUT PHANDLE hThread, IN ACCESS_MASK DesiredAccess,
		IN PVOID ObjectAttributes, IN HANDLE ProcessHandle, IN PVOID lpStartAddress,
		IN PVOID lpParameter, IN ULONG Flags, IN SIZE_T StackZeroBits,
		IN SIZE_T SizeOfStackCommit, IN SIZE_T SizeOfStackReserve,
		OUT PVOID lpBytesBuffer);

	typedef NTSTATUS(NTAPI* pfnNtProtectVirtualMemory)(
		IN HANDLE ProcessHandle, IN OUT PVOID* BaseAddress,
		IN OUT PULONG NumberOfBytesToProtect, IN ULONG NewAccessProtection,
		OUT PULONG OldAccessProtection);

	typedef NTSTATUS(NTAPI* pfnNtWriteVirtualMemory)(
		IN HANDLE ProcessHandle, OUT PVOID BaseAddress, IN PVOID Buffer,
		IN ULONG BufferSize, OUT PULONG NumberOfBytesWritten OPTIONAL);

	typedef NTSTATUS(NTAPI* pfnNtTerminateProcess)(
		IN HANDLE ProcessHandle OPTIONAL,
		IN NTSTATUS ExitStatus);

	typedef NTSTATUS(NTAPI* pfnNtGetTickCount)();

	typedef NTSTATUS(NTAPI* pfnNtCreateThread)(
		OUT PHANDLE             ThreadHandle,
		IN ACCESS_MASK          DesiredAccess,
		IN POBJECT_ATTRIBUTES   ObjectAttributes OPTIONAL,
		IN HANDLE               ProcessHandle,
		OUT PCLIENT_ID          ClientId,
		IN PCONTEXT             ThreadContext,
		IN PINITIAL_TEB         InitialTeb,
		IN BOOLEAN              CreateSuspended);

	typedef NTSTATUS(NTAPI* pfnNtClose)(
		IN HANDLE Handle
		);

	typedef NTSTATUS(NTAPI* pfnNtQueryObject)(
		HANDLE Handle,
		OBJECT_INFORMATION_CLASS ObjectInformationClass,
		PVOID ObjectInformation,
		ULONG ObjectInformationLength,
		PULONG ReturnLength);

	typedef VOID(NTAPI* pfnRtlMoveMemory)(
		_Out_       VOID UNALIGNED* Destination,
		_In_  const VOID UNALIGNED* Source,
		_In_        SIZE_T         Length
		);

private:
	HMODULE hFakeSfcModule = NULL;
	pfnNtQueryInformationProcess NtQueryInformationProcess = NULL;
	pfnNtQuerySystemInformation NtQuerySystemInformation = NULL;
	pfnNtQueryObject NtQueryObject = NULL;
	pfnNtWow64QueryInformationProcess64 NtWow64QueryInformationProcess64 = NULL;
	pfnNtWow64ReadVirtualMemory64 NtWow64ReadVirtualMemory64 = NULL;
	pfnNtWow64WriteVirtualMemory64 NtWow64WriteVirtualMemory64 = NULL;
	pfnNtOpenProcess NtOpenProcess = NULL;
	pfnNtOpenThread NtOpenThread = NULL;
	pfnNtQueueApcThread NtQueueApcThread = NULL;
	pfnNtAllocateVirtualMemory NtAllocateVirtualMemory = NULL;
	pfnNtSuspendThread NtSuspendThread = NULL;
	pfnNtCreateThreadEx NtCreateThreadEx = NULL;
	pfnNtProtectVirtualMemory NtProtectVirtualMemory = NULL;
	pfnNtReadVirtualMemory NtReadVirtualMemory = NULL;
	pfnNtWriteVirtualMemory NtWriteVirtualMemory = NULL;
	pfnZwOpenProcess ZwOpenProcess = NULL;
	pfnZwVirtualProtectEx ZwProtectVirtualMemory = NULL;
	pfnNtTerminateProcess NtTerminateProcess = NULL;
	pfnNtGetTickCount NtGetTickCount = NULL;
	pfnNtCreateThread NtCreateThread = NULL;
	pfnNtClose NtClose = NULL;
	pfnRtlMoveMemory NtRtlMoveMemory = NULL;
public:
	DefApiFun(GetProcAddress);
	DefApiFun(GetCurrentProcessId);
	DefApiFun(SetWindowsHookExW);
	DefApiFun(UnhookWindowsHookEx);
	DefApiFun(VirtualProtect);
	DefApiFun(LoadLibraryW);
	DefApiFun(LoadLibraryExW);
	DefApiFun(GetMessageW);
	DefApiFun(TranslateMessage);
	DefApiFun(DispatchMessageW);
	DefApiFun(RegisterClassExW);
	DefApiFun(CreateWindowExW);
	DefApiFun(ShowWindow);
	DefApiFun(UpdateWindow);
	DefApiFun(GetModuleHandleW);
	DefApiFun(DefWindowProcW);
	DefApiFun(GetWindowTextW);
	DefApiFun(PostQuitMessage);
	DefApiFun(GetStockObject);
	DefApiFun(MessageBoxW);
	DefApiFun(VirtualAlloc);
	DefApiFun(GetCurrentProcess);
	DefApiFun(ExitProcess);
	DefApiFun(IsWindowVisible);
	DefApiFun(IsWindow);
	DefApiFun(GetWindowThreadProcessId);
	DefApiFun(EnumWindows);
	DefApiFun(CreateThread);
	DefApiFun(DisableThreadLibraryCalls);
	DefApiFun(GetExitCodeProcess);
	DefApiFun(OpenProcess);
	DefApiFun(FreeLibrary);
	DefApiFun(GetCurrentThreadId);
	DefApiFun(Sleep);
	DefApiFun(IsWow64Process);
	DefApiFun(CheckRemoteDebuggerPresent);
	DefApiFun(K32EmptyWorkingSet);
	DefApiFun(SetProcessWorkingSetSize);
	DefApiFun(FlushInstructionCache);
	DefApiFun(OpenProcessToken);
	DefApiFun(AdjustTokenPrivileges);
	DefApiFun(LookupPrivilegeValueW);
	DefApiFun(SetWindowTextA);
	DefApiFun(SetWindowTextW);
	DefApiFun(send);
	DefApiFun(listen);
	DefApiFun(recv);
	DefApiFun(accept);
	DefApiFun(inet_addr);
	DefApiFun(gethostbyname);
	DefApiFun(inet_ntoa);
	DefApiFun(htons);
	DefApiFun(htonl);
	DefApiFun(connect);
	DefApiFun(socket);
	DefApiFun(bind);
	DefApiFun(WSAStartup);
	DefApiFun(WSACleanup);
	DefApiFun(closesocket);
	DefApiFun(gethostname);
	DefApiFun(DuplicateHandle);
	DefApiFun(PostMessageW);
	DefApiFun(GetTickCount64);
	DefApiFun(IsDebuggerPresent);
	DefApiFun(MapViewOfFile);
	DefApiFun(UnmapViewOfFile);
	DefApiFun(AddVectoredExceptionHandler);
	DefApiFun(GetLastError);
	DefApiFun(MultiByteToWideChar);
	DefApiFun(lstrlenA);
	DefApiFun(SysAllocStringLen);
	DefApiFun(SysFreeString);

	DefApiFun(ioctlsocket);
	DefApiFun(CreateToolhelp32Snapshot);
	DefApiFun(GetExitCodeThread);
	DefApiFun(OpenThread);
	DefApiFun(TerminateThread);
	DefApiFun(Thread32First);
	DefApiFun(Thread32Next);

	DefApiFun(shutdown);

public:

	VOID WINAPI HideModule(HMODULE hLibrary)
	{
		PPEB_LDR_DATA	pLdr = (PPEB_LDR_DATA)0x1234;
		PLDR_MODULE		FirstModule = NULL;
		PLDR_MODULE		GurrentModule = NULL;
		__try
		{
			__asm
			{
				mov esi, fs: [0x30]
				mov esi, [esi + 0x0C]
				mov pLdr, esi
			}

			FirstModule = (PLDR_MODULE)(pLdr->InLoadOrderModuleList.Flink);
			GurrentModule = FirstModule;
			while (!(GurrentModule->BaseAddress == hLibrary))
			{
				GurrentModule = (PLDR_MODULE)(GurrentModule->InLoadOrderModuleList.Blink);
				if (GurrentModule == FirstModule)
					break;
			}
			if (GurrentModule->BaseAddress != hLibrary)
				return;

			//Dll解除鏈接
			((PLDR_MODULE)(GurrentModule->InLoadOrderModuleList.Flink))->InLoadOrderModuleList.Blink = GurrentModule->InLoadOrderModuleList.Blink;
			((PLDR_MODULE)(GurrentModule->InLoadOrderModuleList.Blink))->InLoadOrderModuleList.Flink = GurrentModule->InLoadOrderModuleList.Flink;

			memset(GurrentModule->FullDllName.Buffer, 0, GurrentModule->FullDllName.Length);
			memset(GurrentModule, 0, sizeof(PLDR_MODULE));
		}

		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return;
		}
	}

	//LdrLoadDll function prototype
	typedef NTSTATUS(WINAPI* fLdrLoadDll)(IN PWCHAR PathToFile OPTIONAL, IN ULONG Flags OPTIONAL, IN PUNICODE_STRING ModuleFileName, OUT PHANDLE ModuleHandle);

	//RtlInitUnicodeString function prototype
	typedef VOID(WINAPI* fRtlInitUnicodeString)(PUNICODE_STRING DestinationString, PCWSTR SourceString);

	HMODULE					hntdll;
	fLdrLoadDll				_LdrLoadDll;
	fRtlInitUnicodeString	_RtlInitUnicodeString;

	HMODULE WINAPI LoadDll(LPCSTR lpFileName)
	{
		if (_LdrLoadDll == NULL)
		{
			_LdrLoadDll = (fLdrLoadDll)My_GetProcAddress(hntdll, getpfn(pfn::LdrLoadDll));
		}

		if (_RtlInitUnicodeString == NULL)
		{
			_RtlInitUnicodeString = (fRtlInitUnicodeString)My_GetProcAddress(hntdll, getpfn(pfn::RtlInitUnicodeString));
		}

		int StrLen = My_lstrlenA(lpFileName);
		BSTR WideStr = My_SysAllocStringLen(NULL, StrLen);
		My_MultiByteToWideChar(CP_ACP, 0, lpFileName, StrLen, WideStr, StrLen);

		UNICODE_STRING usDllName;
		_RtlInitUnicodeString(&usDllName, WideStr);
		My_SysFreeString(WideStr);

		HANDLE DllHandle;
		_LdrLoadDll(0, 0, &usDllName, &DllHandle);

		return (HMODULE)DllHandle;
	}

	void WINAPI hidegetntdllW(wchar_t* wstr)
	{
		getntdllW(wstr);
	}

	virtual void WINAPI getntdllW(wchar_t* wstr)
	{
		VMPBEGIN
			constexpr wchar_t a[MAX_PATH] = { L"ntdll.dll\0" };
		memmove(wstr, a, MAX_PATH);
		VMPEND
	}

	VOID WINAPI installUndocumentApi()
	{
		DWORD* KernelBase = GetKernel32();
		DefineFuncPtr(KernelBase, GetCurrentThreadId);
		DefineFuncPtr(KernelBase, GetProcAddress);
		DefineFuncPtr(KernelBase, VirtualProtect);
		DefineFuncPtr(KernelBase, VirtualAlloc);
		DefineFuncPtr(KernelBase, GetModuleHandleW);
		DefineFuncPtr(KernelBase, LoadLibraryW);
		DefineFuncPtr(KernelBase, LoadLibraryExW);
		DefineFuncPtr(KernelBase, GetCurrentProcess);
		DefineFuncPtr(KernelBase, ExitProcess);
		DefineFuncPtr(KernelBase, GetCurrentProcessId);
		DefineFuncPtr(KernelBase, CreateThread);
		DefineFuncPtr(KernelBase, DisableThreadLibraryCalls);
		DefineFuncPtr(KernelBase, GetExitCodeProcess);
		DefineFuncPtr(KernelBase, OpenProcess);
		DefineFuncPtr(KernelBase, FreeLibrary);
		DefineFuncPtr(KernelBase, Sleep);
		DefineFuncPtr(KernelBase, IsWow64Process);
		DefineFuncPtr(KernelBase, CheckRemoteDebuggerPresent);
		DefineFuncPtr(KernelBase, K32EmptyWorkingSet);
		DefineFuncPtr(KernelBase, FlushInstructionCache);
		DefineFuncPtr(KernelBase, SetProcessWorkingSetSize);
		DefineFuncPtr(KernelBase, DuplicateHandle);
		DefineFuncPtr(KernelBase, GetTickCount64);
		DefineFuncPtr(KernelBase, IsDebuggerPresent);
		DefineFuncPtr(KernelBase, MapViewOfFile);
		DefineFuncPtr(KernelBase, UnmapViewOfFile);
		DefineFuncPtr(KernelBase, AddVectoredExceptionHandler);
		DefineFuncPtr(KernelBase, GetLastError);
		DefineFuncPtr(KernelBase, MultiByteToWideChar);
		DefineFuncPtr(KernelBase, lstrlenA);
		DefineFuncPtr(KernelBase, CreateToolhelp32Snapshot);
		DefineFuncPtr(KernelBase, GetExitCodeThread);
		DefineFuncPtr(KernelBase, OpenThread);
		DefineFuncPtr(KernelBase, TerminateThread);
		DefineFuncPtr(KernelBase, Thread32First);
		DefineFuncPtr(KernelBase, Thread32Next);

		wchar_t wcs_ntdll[MAX_PATH] = {};
		hidegetntdllW(wcs_ntdll);
		hntdll = My_GetModuleHandleW(wcs_ntdll);

		const HMODULE hOleAut32 = My_GetModuleHandleW(L"OleAut32.dll");
		DefineFuncPtr((DWORD*)hOleAut32, SysAllocStringLen);
		DefineFuncPtr((DWORD*)hOleAut32, SysFreeString);

		constexpr char wcs_user32[11] = { 'u','s','e','r','3','2','.','d','l','l','\0' };
		//HMODULE hUser32 = My_LoadLibraryW(wcs_user32);
		HMODULE hUser32 = LoadDll(wcs_user32);
		DefineFuncPtr((DWORD*)hUser32, CreateWindowExW);
		DefineFuncPtr((DWORD*)hUser32, GetMessageW);
		DefineFuncPtr((DWORD*)hUser32, RegisterClassExW);
		DefineFuncPtr((DWORD*)hUser32, TranslateMessage);
		DefineFuncPtr((DWORD*)hUser32, DispatchMessageW);
		DefineFuncPtr((DWORD*)hUser32, ShowWindow);
		DefineFuncPtr((DWORD*)hUser32, UpdateWindow);
		DefineFuncPtr((DWORD*)hUser32, GetWindowTextW);
		DefineFuncPtr((DWORD*)hUser32, PostQuitMessage);
		DefineFuncPtr((DWORD*)hUser32, DefWindowProcW);
		DefineFuncPtr((DWORD*)hUser32, MessageBoxW);
		DefineFuncPtr((DWORD*)hUser32, IsWindowVisible);
		DefineFuncPtr((DWORD*)hUser32, GetWindowThreadProcessId);
		DefineFuncPtr((DWORD*)hUser32, EnumWindows);
		DefineFuncPtr((DWORD*)hUser32, SetWindowsHookExW);
		DefineFuncPtr((DWORD*)hUser32, UnhookWindowsHookEx);
		DefineFuncPtr((DWORD*)hUser32, SetWindowTextA);
		DefineFuncPtr((DWORD*)hUser32, SetWindowTextW);
		DefineFuncPtr((DWORD*)hUser32, IsWindow);
		DefineFuncPtr((DWORD*)hUser32, PostMessageW);

		constexpr char wcs_advapi32[13] = { 'a','d','v','a','p','i','3','2','.','d','l','l','\0' };
		//HMODULE hAdvapi32 = My_LoadLibraryW(wcs_advapi32);
		const HMODULE hAdvapi32 = LoadDll(wcs_advapi32);
		DefineFuncPtr((DWORD*)hAdvapi32, OpenProcessToken);
		DefineFuncPtr((DWORD*)hAdvapi32, AdjustTokenPrivileges);
		DefineFuncPtr((DWORD*)hAdvapi32, LookupPrivilegeValueW);

		constexpr char wcs_ws2_32[11] = { 'w','s','2','_','3','2','.','d','l','l','\0' };
		//HMODULE hws2_32 = My_LoadLibraryW(wcs_ws2_32);
		const HMODULE hws2_32 = LoadDll(wcs_ws2_32);
		DefineFuncPtr((DWORD*)hws2_32, send);
		DefineFuncPtr((DWORD*)hws2_32, listen);
		DefineFuncPtr((DWORD*)hws2_32, recv);
		DefineFuncPtr((DWORD*)hws2_32, accept);
		DefineFuncPtr((DWORD*)hws2_32, inet_addr);
		DefineFuncPtr((DWORD*)hws2_32, htons);
		DefineFuncPtr((DWORD*)hws2_32, htonl);
		DefineFuncPtr((DWORD*)hws2_32, connect);
		DefineFuncPtr((DWORD*)hws2_32, socket);
		DefineFuncPtr((DWORD*)hws2_32, bind);
		DefineFuncPtr((DWORD*)hws2_32, WSAStartup);
		DefineFuncPtr((DWORD*)hws2_32, WSACleanup);
		DefineFuncPtr((DWORD*)hws2_32, closesocket);
		DefineFuncPtr((DWORD*)hws2_32, gethostbyname);
		DefineFuncPtr((DWORD*)hws2_32, gethostname);
		DefineFuncPtr((DWORD*)hws2_32, inet_ntoa);
		DefineFuncPtr((DWORD*)hws2_32, ioctlsocket);
		DefineFuncPtr((DWORD*)hws2_32, shutdown);

		const HMODULE NtdllModule = LoadDll(getpfn(pfn::Ntdll));
		NtQueryInformationProcess = (pfnNtQueryInformationProcess)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtQueryInformationProcess));
		NtQuerySystemInformation = (pfnNtQuerySystemInformation)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtQuerySystemInformation));
		NtQueryObject = (pfnNtQueryObject)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtQueryObject));
		NtWow64ReadVirtualMemory64 = (pfnNtWow64ReadVirtualMemory64)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtWow64ReadVirtualMemory64));
		NtWow64WriteVirtualMemory64 = (pfnNtWow64WriteVirtualMemory64)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtWow64WriteVirtualMemory64));
		NtWow64QueryInformationProcess64 = (pfnNtWow64QueryInformationProcess64)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtWow64QueryInformationProcess64));
		ZwOpenProcess = (pfnZwOpenProcess)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::ZwOpenProcess));
		ZwProtectVirtualMemory = (pfnZwVirtualProtectEx)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::ZwVirtualProtectEx));
		NtOpenProcess = (pfnNtOpenProcess)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtOpenProcess));
		NtOpenThread = (pfnNtOpenThread)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtOpenThread));
		NtQueueApcThread = (pfnNtQueueApcThread)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtQueueApcThread));
		NtAllocateVirtualMemory = (pfnNtAllocateVirtualMemory)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtAllocateVirtualMemory));
		NtSuspendThread = (pfnNtSuspendThread)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtSuspendThread));
		NtCreateThreadEx = (pfnNtCreateThreadEx)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtCreateThreadEx));
		NtReadVirtualMemory = (pfnNtReadVirtualMemory)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtReadVirtualMemory));
		NtWriteVirtualMemory = (pfnNtWriteVirtualMemory)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtWriteVirtualMemory));
		NtTerminateProcess = (pfnNtTerminateProcess)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtTerminateProcess));
		NtProtectVirtualMemory = (pfnNtProtectVirtualMemory)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtProtectVirtualMemory));
		NtGetTickCount = (pfnNtGetTickCount)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtGetTickCount));
		NtClose = (pfnNtClose)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::NtClose));
		NtRtlMoveMemory = (pfnRtlMoveMemory)(NTSTATUS)My_GetProcAddress(NtdllModule, getpfn(pfn::RtlMoveMemory));
		HideModule(NtdllModule);
	}

	enum class pfn
	{
		Ntdll,
		NtQueryInformationProcess,
		NtQuerySystemInformation,
		NtQueryObject,
		NtWow64ReadVirtualMemory64,
		NtWow64WriteVirtualMemory64,
		NtWow64QueryInformationProcess64,
		ZwOpenProcess,
		ZwVirtualProtectEx,
		NtOpenProcess,
		NtOpenThread,
		NtQueueApcThread,
		NtAllocateVirtualMemory,
		NtSuspendThread,
		NtCreateThreadEx,
		NtReadVirtualMemory,
		NtWriteVirtualMemory,
		NtTerminateProcess,
		NtProtectVirtualMemory,
		NtGetTickCount,
		NtClose,
		RtlMoveMemory,
		LdrLoadDll,
		RtlInitUnicodeString,
	};

	const char* WINAPI getpfn(pfn p)
	{
		return hidepfn(p);
	}

	virtual const char* WINAPI hidepfn(const pfn& p)
	{
		VMPBEGIN
			switch (p)
			{
			case pfn::Ntdll: return (char*)"Ntdll";
			case pfn::NtQueryInformationProcess: return (char*)"NtQueryInformationProcess";
			case pfn::NtQuerySystemInformation: return (char*)"NtQuerySystemInformation";
			case pfn::NtQueryObject: return (char*)"NtQueryObject";
			case pfn::NtWow64ReadVirtualMemory64: return (char*)"NtWow64ReadVirtualMemory64";
			case pfn::NtWow64WriteVirtualMemory64: return (char*)"NtWow64WriteVirtualMemory64";
			case pfn::NtWow64QueryInformationProcess64: return (char*)"NtWow64QueryInformationProcess64";
			case pfn::ZwOpenProcess: return (char*)"ZwOpenProcess";
			case pfn::ZwVirtualProtectEx: return (char*)"ZwVirtualProtectEx";
			case pfn::NtOpenProcess: return (char*)"NtOpenProcess";
			case pfn::NtOpenThread: return (char*)"NtOpenThread";
			case pfn::NtQueueApcThread: return (char*)"NtQueueApcThread";
			case pfn::NtAllocateVirtualMemory: return (char*)"NtAllocateVirtualMemory";
			case pfn::NtSuspendThread: return (char*)"NtSuspendThread";
			case pfn::NtCreateThreadEx: return (char*)"NtCreateThreadEx";
			case pfn::NtReadVirtualMemory: return (char*)"NtReadVirtualMemory";
			case pfn::NtWriteVirtualMemory: return (char*)"NtWriteVirtualMemory";
			case pfn::NtTerminateProcess: return (char*)"NtTerminateProcess";
			case pfn::NtProtectVirtualMemory: return (char*)"NtProtectVirtualMemory";
			case pfn::NtGetTickCount: return (char*)"NtGetTickCount";
			case pfn::NtClose: return (char*)"NtClose";
			case pfn::RtlMoveMemory: return (char*)"RtlMoveMemory";
			case pfn::LdrLoadDll: return (char*)"LdrLoadDll";
			case pfn::RtlInitUnicodeString: return (char*)"RtlInitUnicodeString";
			}
		VMPEND
	}

	void WINAPI My_NtMoveMemory(VOID UNALIGNED* Destination, const VOID UNALIGNED* Source, SIZE_T Length)
	{
		NtRtlMoveMemory(Destination, Source, Length);
	}

	LONG WINAPI My_CloseHandle(const HANDLE& handle)
	{
		return NtClose(handle);
	}

	LONG WINAPI My_ProtectVirtualMemory(HANDLE ProcessHandle, LPVOID BaseAddress, SIZE_T  NumberOfBytesToProtect, DWORD NewAccessProtection, PDWORD OldAccessProtection)
	{
		return NtProtectVirtualMemory(ProcessHandle, &BaseAddress, &NumberOfBytesToProtect, NewAccessProtection, OldAccessProtection);
	}

	BOOL WINAPI My_TerminateProcess(HANDLE ProcessHandle, LONG nExitCode)
	{
		return NtTerminateProcess(ProcessHandle, nExitCode);
	}

	HANDLE WINAPI my_CreateThread(HANDLE hProcess, LPTHREAD_START_ROUTINE pThreadProc, LPVOID pRemoteBuf)
	{
		BOOL bret = FALSE;
		HANDLE      hThread = NULL;
		//FARPROC     pFunc = NULL;
		if (IsWindowsVistaOrGreater())
		{
			NtCreateThreadEx(
				&hThread,
				0x1FFFFF,   // 内核调试的结果就是这个参数
				NULL,
				hProcess,
				pThreadProc,
				pRemoteBuf,
				FALSE,
				NULL,
				NULL,
				NULL,
				NULL);
			if (hThread != NULL)
			{
				bret = TRUE;
			}
		}
		if (!bret)
		{
			DWORD threadid;
			return My_CreateThread(0, NULL, pThreadProc, pRemoteBuf, 0, &threadid);
		}
		return hThread;
	}

	HANDLE WINAPI myOpenProcess(const DWORD& dwProcessId)
	{
		ElevatePrivileges();
		HANDLE hprocess = NULL;

		hprocess = My_NtOpenProcess(dwProcessId);
		if (hprocess == NULL) {
			My_ZwOpenProcess(dwProcessId);
			if (hprocess == NULL) {
				My_OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
				if (hprocess == NULL) {
					return 0;
				}
			}
		}

		return hprocess;
	}

	BOOL WINAPI ElevatePrivileges()
	{
		HANDLE hToken;
		TOKEN_PRIVILEGES tkp{};
		tkp.PrivilegeCount = 1;
		if (!My_OpenProcessToken(My_GetCurrentProcess(),
			TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
			return FALSE;
		constexpr WCHAR m[MAX_PATH] = { TEXT("SeDebugPrivilege\0") };
		My_LookupPrivilegeValueW(0, m, &tkp.Privileges[0].Luid);
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if (!My_AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES),
			0, 0)) {
			return FALSE;
		}

		return TRUE;
	}
private:
	HANDLE WINAPI My_ZwOpenProcess(DWORD dwProcessId)
	{
		HANDLE ProcessHandle = (HANDLE)0;
		OBJECT_ATTRIBUTES ObjectAttribute = {
			sizeof(OBJECT_ATTRIBUTES), 0, 0, 0, 0, 0
		};
		ObjectAttribute.Attributes = NULL;
		CLIENT_ID ClientIds = {};
		ClientIds.UniqueProcess = (HANDLE)dwProcessId;
		ClientIds.UniqueThread = (HANDLE)0;
		ZwOpenProcess(&ProcessHandle, PROCESS_ALL_ACCESS, &ObjectAttribute,
			&ClientIds);

		return ProcessHandle;
	}

	HANDLE WINAPI My_NtOpenProcess(DWORD dwProcessId)
	{
		HANDLE ProcessHandle = NULL;
		OBJECT_ATTRIBUTES ObjectAttribute = {
			sizeof(OBJECT_ATTRIBUTES), 0, 0, 0, 0, 0
		};
		CLIENT_ID ClientId = {};

		InitializeObjectAttributes(&ObjectAttribute, 0, 0, 0, 0);
		ClientId.UniqueProcess = (PVOID)dwProcessId;
		ClientId.UniqueThread = (PVOID)0;

		if (NT_SUCCESS(NtOpenProcess(&ProcessHandle, MAXIMUM_ALLOWED,
			&ObjectAttribute, &ClientId))) {
			return ProcessHandle;
		}

		return 0;
	}

	HANDLE WINAPI My_NtOpenThread(DWORD dwProcessId, DWORD dwThreadId)
	{
		HANDLE ThreadHandle = NULL;
		OBJECT_ATTRIBUTES ObjectAttributes = {};
		CLIENT_ID ClientId = {};
		InitializeObjectAttributes(&ObjectAttributes, 0, 0, 0, 0);
		ClientId.UniqueProcess = (PVOID)dwProcessId;
		ClientId.UniqueThread = (PVOID)dwThreadId;

		if (NT_SUCCESS(NtOpenThread(&ThreadHandle, MAXIMUM_ALLOWED, &ObjectAttributes,
			&ClientId))) {
			if (ThreadHandle) {
				return ThreadHandle;
			}
		}
		return 0;
	}
public:
	void WINAPI FreeProcessMemory()
	{
		ElevatePrivileges();
		HANDLE hProcess = My_GetCurrentProcess();
		My_SetProcessWorkingSetSize(hProcess, (SIZE_T)-1, (SIZE_T)-1);
		My_K32EmptyWorkingSet(hProcess);
	}

private:

public:

#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)

	SYSTEM_HANDLE_INFORMATION* WINAPI GetSystemProcessHandleInfo()
	{
		DWORD buffLen = 0x1000;
		NTSTATUS status;
		BYTE* buff = new BYTE[buffLen];
		do {
			status = NtQuerySystemInformation(_SYSTEM_INFORMATION_CLASS::SystemHandleInformation, buff, buffLen, &buffLen);
			if (status == STATUS_INFO_LENGTH_MISMATCH) {
				delete[] buff;
				buff = new BYTE[buffLen];
			}
			else
				break;
		} while (TRUE);
		return (SYSTEM_HANDLE_INFORMATION*)buff;
	}
	BOOL WINAPI CloseMutex()
	{
		ElevatePrivileges();

		NTSTATUS Status = NULL;
		HANDLE hSource = NULL;
		HANDLE hDuplicate = NULL;
		DWORD HandleCount = NULL;
		OBJECT_NAME_INFORMATION* ObjectName = nullptr;
		OBJECT_TYPE_INFORMATION* ObjectType = nullptr;
		char BufferForObjectName[1024] = {};
		char BufferForObjectType[1024] = {};
		int count = NULL;

		hSource = My_GetCurrentProcess();//myOpenProcess(dwProcessId);
		if (hSource == NULL) {
			return FALSE;
		}
		DWORD dwHandle = NULL;
		NtQueryInformationProcess(hSource, ProcessHandleCount, &HandleCount, sizeof(HandleCount), NULL);

		for (DWORD i = 1; i <= HandleCount; i++) //穷举句柄
		{
			dwHandle = i * 4;
			if (My_DuplicateHandle(hSource, //复制一个句柄对象 && 判断此句柄是否有效
				(HANDLE)dwHandle,
				My_GetCurrentProcess(),
				&hDuplicate,
				0, FALSE, DUPLICATE_SAME_ACCESS)) {
				ZeroMemory(BufferForObjectName, 1024);
				ZeroMemory(BufferForObjectType, 1024);

				//获取句柄类型
				Status = NtQueryObject(hDuplicate,
					_OBJECT_INFORMATION_CLASS::ObjectTypeInformation,
					BufferForObjectType,
					sizeof(BufferForObjectType),
					NULL);

				ObjectType = (OBJECT_TYPE_INFORMATION*)BufferForObjectType;
				if (Status == STATUS_INFO_LENGTH_MISMATCH || !NT_SUCCESS(Status))
					continue;

				//QString strTemp = QString::fromWCharArray((wchar_t*)ObjectType->TypeName.Buffer).toLower();
				wchar_t* wcs = (wchar_t*)ObjectType->TypeName.Buffer;
				_wcslwr_s(wcs, ObjectType->TypeName.Length);

				if (wcscmp(wcs, TEXT("mutant")) != 0)
					continue;

				//获取句柄名
				Status = NtQueryObject((HANDLE)hDuplicate,
					_OBJECT_INFORMATION_CLASS::ObjectNameInformation,
					BufferForObjectName,
					sizeof(BufferForObjectName),
					NULL);

				ObjectName = (POBJECT_NAME_INFORMATION)BufferForObjectName;
				if (Status == STATUS_INFO_LENGTH_MISMATCH || !NT_SUCCESS(Status))
					continue;

				//strTemp = QString::fromWCharArray((wchar_t*)ObjectName->Name.Buffer).toLower();
				count++;
				if (wcslen((wchar_t*)ObjectName->Name.Buffer) > 35 || wcslen((wchar_t*)ObjectName->Name.Buffer) == 0) {
					continue;
				}

				// xdebug << strTemp;

				//关闭复制的句柄
				My_CloseHandle(hDuplicate);
				hDuplicate = NULL;
				if (My_DuplicateHandle(hSource, //复制一个句柄对象 && 判断此句柄是否有效
					(HANDLE)dwHandle,
					My_GetCurrentProcess(),
					&hDuplicate,
					0, FALSE, DUPLICATE_CLOSE_SOURCE)) {
					My_CloseHandle(hDuplicate);
					//My_CloseHandle(hSource);
					//MessageBox(0, L"Mutex close finished",L"", 0);
					//qDebug() << "Mutex close finished";
					return TRUE;
				}
			}
		}
		//My_CloseHandle(hSource);

		//qDebug() << "Mutex count: " << count << Status;
		if (count == 19) {
			return TRUE;
		}
		//MessageBox(0, L"FAIL", L"", 0);
		return FALSE;
	}
};

#endif
