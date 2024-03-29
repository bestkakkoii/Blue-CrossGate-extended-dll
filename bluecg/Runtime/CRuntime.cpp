#include "CRuntime.h"
#include "CKernel32.h"
#include <random>

HWND find_main_window(const unsigned long& process_id);
HWND CRuntime::get_window_handle()
{
	CKernel32& ker32 = CKernel32::get_instance();
	unsigned long process_id = ker32.My_GetCurrentProcessId();
	HWND hwnd = NULL;
	while (!hwnd) { hwnd = find_main_window(process_id); }
	return hwnd;
}

struct handle_data {
	unsigned long process_id;
	HWND window_handle;
};

BOOL is_main_window(const HWND& handle)
{
	if ((GetWindow(handle, GW_OWNER) == NULL) && (IsWindowVisible(handle)))
		return TRUE;
	else
		return FALSE;
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
	handle_data& data = *(handle_data*)lParam;
	unsigned long process_id = 0;
	GetWindowThreadProcessId(handle, &process_id);
	if (data.process_id != process_id || !is_main_window(handle))
		return TRUE;
	data.window_handle = handle;
	return FALSE;
}

HWND find_main_window(const unsigned long& process_id)
{
	handle_data data = {};
	data.process_id = process_id;
	data.window_handle = 0;
	EnumWindows(enum_windows_callback, (LPARAM)&data);
	return data.window_handle;
}

long long CRuntime::rand(const long long& MIN, const long long& MAX)
{
	std::random_device rd;
	std::default_random_engine eng(rd());
	std::uniform_int_distribution<long long> distr(MIN, MAX);
	return distr(eng);
}

int CRuntime::rand(const int& MIN, const int& MAX)
{
	std::random_device rd;
	std::default_random_engine eng(rd());
	std::uniform_int_distribution<int> distr(MIN, MAX);
	return distr(eng);
}

std::vector<std::string> CRuntime::char_split(const char* in, const char* delimit)
{
	std::vector<std::string> ret;
	const std::string_view words{ in };
	const std::string_view delim{ delimit };

	auto view = words
		| std::ranges::views::split(delim)
		| std::ranges::views::transform([](auto&& rng) {
		return std::string_view(&*rng.begin(), std::ranges::distance(rng));
			});
	for (std::string_view it : view)
	{
		ret.push_back((std::string)it);
	}
	return ret;
}

// wchar_t 转为 char
void CRuntime::wchar2char(const wchar_t* wchar, char* retstr)
{
	//char l[MAX_PATH] = { "big5\0" };
	//setlocale(LC_ALL, l);
	char* m_char;
	int len = WideCharToMultiByte(CP_ACP, 0, wchar, wcslen(wchar), NULL, 0, NULL, NULL);
	m_char = new char[len + 1];
	WideCharToMultiByte(CP_ACP, 0, wchar, wcslen(wchar), m_char, len, NULL, NULL);
	m_char[len] = '\0';
	_snprintf_s(retstr, len + 1, _TRUNCATE, "%s\0", m_char);
	delete[] m_char;
	m_char = nullptr;
}

void CRuntime::wchar2char(const std::wstring& wstr, char* retstr)
{
	const wchar_t* wchar = wstr.c_str();
	//char l[MAX_PATH] = { "big5\0" };
	//setlocale(LC_ALL, l);
	char* m_char;
	int len = WideCharToMultiByte(CP_ACP, 0, wchar, wcslen(wchar), NULL, 0, NULL, NULL);
	m_char = new char[len + 1];
	WideCharToMultiByte(CP_ACP, 0, wchar, wcslen(wchar), m_char, len, NULL, NULL);
	m_char[len] = '\0';
	_snprintf_s(retstr, len + 1, _TRUNCATE, "%s\0", m_char);
	delete[] m_char;
	m_char = nullptr;
}

void CRuntime::char2wchar(const char* cchar, wchar_t* retwstr)
{
	//char l[MAX_PATH] = { "big5\0" };
	//setlocale(LC_ALL, l);
	wchar_t* m_wchar;
	int len = MultiByteToWideChar(CP_ACP, 0, cchar, strlen(cchar), NULL, 0);
	m_wchar = new wchar_t[len + 1];
	MultiByteToWideChar(CP_ACP, 0, cchar, strlen(cchar), m_wchar, len);
	m_wchar[len] = '\0';
	_snwprintf_s(retwstr, len + 1, _TRUNCATE, L"%s\0", m_wchar);
	delete[] m_wchar;
	m_wchar = nullptr;
}

const std::wstring CRuntime::char2wchar(const char* cchar)
{
	wchar_t* m_wchar;
	int len = MultiByteToWideChar(CP_ACP, 0, cchar, strlen(cchar), NULL, 0);
	m_wchar = new wchar_t[len + 1];
	MultiByteToWideChar(CP_ACP, 0, cchar, strlen(cchar), m_wchar, len);
	m_wchar[len] = '\0';
	std::wstring retwstr(m_wchar);
	delete[] m_wchar;
	m_wchar = nullptr;
	return retwstr;
}

void CRuntime::delay(long nTime)
{
	//ULONGLONG tm0 = 0, tm1 = 0;
	//tm0 = GetTickCount64();//获取当前时间，单位为ms
	//MSG msg = { 0 };
	//while ((tm1 - tm0) < s)
	//{
	//	tm1 = GetTickCount64();
	//}
	//return;
	HANDLE hTimer = 0;
	MSG* msg = new MSG;
	*msg = {};
	LARGE_INTEGER lar = {};

	lar.QuadPart = -10 * static_cast<LONGLONG>(nTime) * 1000;

	if ((hTimer = CreateWaitableTimer(NULL, FALSE, NULL)) != NULL)
	{
		SetWaitableTimer(hTimer, &lar, NULL, NULL, NULL, FALSE);

		while (MsgWaitForMultipleObjects(1, &hTimer, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
		{
			process_event(msg);
		}
		CloseHandle(hTimer);
	}
	delete msg;
	msg = nullptr;
}

void CRuntime::process_event(MSG* msg)
{
	while (PeekMessage(msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(msg);//translates virtual-key messages into character messages
		DispatchMessage(msg);//dispatches a message to a window procedure
	}
}