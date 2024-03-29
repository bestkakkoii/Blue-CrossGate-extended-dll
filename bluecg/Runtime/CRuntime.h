#pragma once
#ifndef CRUNTIME_H
#define CRUNTIME_H
#include <Windows.h>
#include <vector>
#include <ranges>
#include <cctype>
#include <random>
#include <string>
#include <sstream>
#include <iterator>
#include <regex>

#include <locale>
#include <codecvt>

class CRuntime
{
public:
	static HWND WINAPI get_window_handle();
	static int WINAPI rand(const int& MIN, const int& MAX);
	static long long WINAPI rand(const long long& MIN, const long long& MAX);
	static std::vector<std::string> WINAPI char_split(const char* in, const char* delimit);
	static long WINAPI isint(const std::string& str)
	{
		for (char const& c : str) {
			if (std::isdigit(c) == 0) return false;
		}
		return true;
	}
	static long WINAPI isint(const std::wstring& str)
	{
		for (wchar_t const& c : str) {
			if (std::isdigit(c) == 0) return false;
		}
		return true;
	}
	static void WINAPI wchar2char(const wchar_t* wchar, char* retstr);
	static void WINAPI wchar2char(const std::wstring& wstr, char* retstr);
	static void WINAPI char2wchar(const char* cchar, wchar_t* retwstr);
	static const std::wstring WINAPI char2wchar(const char* cchar);

	static void WINAPI delay(long nTime);

private:
	static void WINAPI process_event(MSG* msg);
};

class QString : public std::wstring
{
public:

	inline constexpr QString() noexcept : d() {}
	QString(std::wstring w) noexcept : d(w) {}
	QString(const wchar_t* w) noexcept : d(w) {}
	QString(const char* c) noexcept :d(CRuntime::char2wchar(c)) {}
	inline ~QString() {};

	inline const QString& operator+=(const std::wstring& s1) noexcept
	{
		d += s1;
		return *this;
	}
	inline const QString& operator+=(const char* s1) noexcept
	{
		d += CRuntime::char2wchar(s1);

		return *this;
	}

	inline const QString operator+(const std::wstring& s2) // 版本 A2
	{
		return QString(d) += s2; // 利用版本 B2
	}

	inline const QString operator+(const char* s2) // 版本 A2
	{
		return QString(d) += CRuntime::char2wchar(s2); // 利用版本 B2
	}

	inline int size() const noexcept { return d.size(); }
	inline int count() const noexcept { return d.size(); }
	inline int length() const noexcept { return d.size(); }
	inline bool isEmpty() const noexcept { return d.empty(); }

	inline const std::vector<QString> split(const QString& delim)
	{
		std::wregex re{ delim };
		return std::vector<QString> {
			std::wsregex_token_iterator(d.begin(), d.end(), re, -1),
				std::wsregex_token_iterator()
		};
	}

	// 支持wchar_t宽字符集的版本
	inline const std::vector<QString> split(const wchar_t* delim) {
		const wchar_t* in = d.c_str();
		std::wregex re{ delim };
		return std::vector<QString> {
			std::wcregex_token_iterator(in, in + wcslen(in), re, -1),
				std::wcregex_token_iterator()
		};
	}

	inline const bool contains(const std::wstring f) const noexcept
	{
		return std::wstring::npos != d.find(f);
	}

	inline const int indexOf(const std::wstring& str) const noexcept
	{
		return d.find(str);
	}

	inline const bool endsWith(const std::wstring& str) const noexcept
	{
		return d.rfind(str) == d.size() - str.size();
	}

	inline const bool startWith(const std::wstring& str) const noexcept
	{
		return d.find(str) == 0;
	}

	inline const int indexOf(const std::wstring& str, const int n) const noexcept
	{
		return d.find(str, n);
	}

	inline QString trim()
	{
		return QString(d.substr(d.find_first_not_of(L" "), d.find_last_not_of(L" ") + 1));
	}

	inline QString trim(const std::wstring& delim)
	{
		return QString(d.substr(d.find_first_not_of(delim), d.find_last_not_of(delim) + 1));
	}

	inline std::wstring toStdWString() const noexcept
	{
		return d;
	}
	// encoding big5 std::string
	inline std::string toStdString()
	{
		std::locale sys_loc("zh_TW.big5");

		const wchar_t* src_wstr = d.c_str();
		const size_t MAX_UNICODE_BYTES = 4;
		const size_t BUFFER_SIZE =
			d.size() * MAX_UNICODE_BYTES + 1;

		char* extern_buffer = new char[BUFFER_SIZE];
		memset(extern_buffer, 0, BUFFER_SIZE);

		const wchar_t* intern_from = src_wstr;
		const wchar_t* intern_from_end = intern_from + d.size();
		const wchar_t* intern_from_next = 0;
		char* extern_to = extern_buffer;
		char* extern_to_end = extern_to + BUFFER_SIZE;
		char* extern_to_next = 0;

		typedef std::codecvt<wchar_t, char, mbstate_t> CodecvtFacet;

		/*CodecvtFacet::result cvt_rst */
		std::ignore =
			std::use_facet<CodecvtFacet>(sys_loc).out(out_cvt_state,
				intern_from, intern_from_end, intern_from_next,
				extern_to, extern_to_end, extern_to_next);
		//if (cvt_rst != CodecvtFacet::ok) {
		//}

		std::string result = extern_buffer;

		delete[]extern_buffer;

		return result;
	}

private:
	template<typename E,
		typename TR = std::char_traits<E>,
		typename AL = std::allocator<E>,
		typename _str_type = std::basic_string<E, TR, AL>>
		std::vector<_str_type> bs_split(const std::basic_string<E, TR, AL>& in, const std::basic_string<E, TR, AL>& delim) {
		std::basic_regex<E> re{ delim };
		return std::vector<_str_type> {
			std::regex_token_iterator<typename _str_type::const_iterator>(in.begin(), in.end(), re, -1),
				std::regex_token_iterator<typename _str_type::const_iterator>()
		};
	}

	std::wstring d;
	mbstate_t out_cvt_state = {};
};

class CRSA {
public:
	~CRSA() {
		//std::cout << "destructor called!" << std::endl;
	}
	CRSA(const CRSA&) = delete;
	CRSA& operator=(const CRSA&) = delete;
	static CRSA& get_instance() {
		static CRSA instance;
		return instance;
	}
private:
	explicit CRSA() {
		while (!e)
			RSA_Initialize();
	}

private:
	// int Plaintext[100]; //明文
	// unsigned int Ciphertext[100]; //密文
	unsigned int n = 0, d = 0;
	unsigned int e = 0;
	//二进制转换
	unsigned int WINAPI BianaryTransform(unsigned int num, unsigned int bin_num[])
	{
		unsigned int i = 0, mod = 0;

		//转换为二进制，逆向暂存temp[]数组中
		while (num != 0) {
			mod = num % 2;
			bin_num[i] = mod;
			num = num / 2;
			i++;
		}

		//返回二进制数的位数
		return i;
	}

	//反复平方求幂
	unsigned int WINAPI Modular_Exonentiation(unsigned int a, unsigned int b, unsigned int g)
	{
		unsigned int c = 0, bin_num[1000];
		unsigned int j = 1;
		unsigned int k = BianaryTransform(b, bin_num) - 1;

		for (int i = k; i >= 0; i--) {
			c = 2 * c;
			j = (j * j) % g;
			if (bin_num[i] == 1) {
				c = c + 1;
				j = (j * a) % g;
			}
		}
		return j;
	}

	//生成1000以内素数
	unsigned int WINAPI ProducePrimeNumber(unsigned int prime[])
	{
		unsigned int c = 0, vis[1001];
		memset(vis, 0, sizeof(vis));
		for (int i = 2; i <= 1000; i++)
			if (!vis[i]) {
				prime[c++] = (unsigned int)i;
				for (unsigned int j = i * i; j <= 1000; j += i)
					vis[j] = 1;
			}

		return c;
	}

	//欧几里得扩展算法
	unsigned int WINAPI Exgcd(unsigned int m, unsigned int g, unsigned int& x)
	{
		unsigned int x1, y1, x0, y0, y;
		x0 = 1;
		y0 = 0;
		x1 = 0;
		y1 = 1;
		x = 0;
		y = 1;
		unsigned int r = m % g;
		unsigned int q = (m - r) / g;
		while (r) {
			x = x0 - q * x1;
			y = y0 - q * y1;
			x0 = x1;
			y0 = y1;
			x1 = x;
			y1 = y;
			m = g;
			g = r;
			r = m % g;
			q = (m - r) / g;
		}
		return g;
	}

	unsigned int WINAPI rand(const unsigned int& MIN, const unsigned int& MAX)
	{
		std::random_device rd;
		std::default_random_engine eng(rd());
		std::uniform_int_distribution<unsigned int> distr(MIN, MAX);
		return distr(eng);
	}

	// RSA初始化
	void WINAPI RSA_Initialize()
	{
		setXorKey();
		//取出1000内素数保存在prime[]数组中
		unsigned int* prime = new unsigned int[5000];
		unsigned int count_Prime = ProducePrimeNumber(prime);

		//随机取两个素数p,q
		unsigned int ranNum1 = rand(1, 0x7fff) % count_Prime;
		unsigned int ranNum2 = rand(1, 0x7fff) % count_Prime;
		unsigned int p = prime[ranNum1], q = prime[ranNum2];

		n = p * q;

		unsigned int On = (p - 1) * (q - 1);

		//用欧几里德扩展算法求e,d
		for (unsigned int j = 3; j < On; j += 1331) {
			unsigned int gcd = Exgcd(j, On, d);
			if (gcd == 1 && d > 0) {
				e = j;
				break;
			}
		}
		delete[] prime;
		prime = nullptr;
	}

	void setXorKey()
	{
		xorkey = CRuntime::rand(1, 0x7fff);
		XORKEY_private = CRuntime::rand(1, 0x7fff) ^ xorkey;
	}
	unsigned int xorkey = 0;
	unsigned int XORKEY_private = 0;
public:
	unsigned int getXorKey()
	{
		return XORKEY_private ^ xorkey;
	}
	// RSA加密
	void WINAPI RSA_Encrypt(unsigned int* pretext, unsigned int* Ciphertext, const size_t& size)
	{
		size_t i = 0;
		for (i = 0; i < size; i++)
		{
			if (pretext[i] > 0)
				Ciphertext[i] = Modular_Exonentiation(pretext[i], e, n);
			else
				Ciphertext[i] = 0;
		}
	}

	// RSA解密
	void WINAPI RSA_Decrypt(unsigned int* Ciphertext, unsigned int* pretext, const size_t& size)
	{
		size_t i = 0;
		for (i = 0; i < size; i++)
		{
			if (Ciphertext[i] > 0)
				pretext[i] = Modular_Exonentiation(Ciphertext[i], d, n);
			else
				pretext[i] = 0;
		}
	}
};

#endif