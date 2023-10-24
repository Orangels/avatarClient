#include "StringUtil.h"
#include "BasicTypes.h"
#include <Windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <vector>
#include <algorithm>

namespace util {

std::string UnicodeToUtf8(const wchar_t* str)
{
	if (!str || !*str)
		return "";

	int str_len = lstrlenW(str);
	int buf_len = ::WideCharToMultiByte(CP_UTF8, 0, str, str_len, NULL, 0, NULL, NULL);
	std::string buf(buf_len, 0);
	::WideCharToMultiByte(CP_UTF8, 0, str, str_len, (LPSTR)buf.data(), buf_len, NULL, NULL);
	return buf;
}

std::string UnicodeToUtf8(const std::wstring &str)
{
	return UnicodeToUtf8(str.c_str());
}

std::string UnicodeToAnsi(const wchar_t* str)
{
	if (!str || !*str)
		return "";

	int str_len = lstrlenW(str);
	int buf_len = ::WideCharToMultiByte(936, 0, str, str_len, NULL, 0, NULL, NULL);
	std::string buf(buf_len, 0);
	::WideCharToMultiByte(936, 0, str, str_len, (LPSTR)buf.data(), buf_len, NULL, NULL);
	return buf;
}

std::string UnicodeToAnsi(const std::wstring &str)
{
	return UnicodeToAnsi(str.c_str());
}

std::wstring Utf8ToUnicode(const char* str)
{
	if (!str || !*str)
		return L"";

	int str_len = lstrlenA(str);
	int buf_len = ::MultiByteToWideChar(CP_UTF8, 0, str, str_len, NULL, 0);
	std::wstring buf(buf_len, 0);
	::MultiByteToWideChar(CP_UTF8, 0, str, str_len, (LPWSTR)buf.data(), buf_len);
	return buf;
}

std::wstring Utf8ToUnicode(const std::string &str)
{
	return Utf8ToUnicode(str.c_str());
}

std::string Utf8ToAnsi(const char* str)
{
	if (!str || !*str)
		return "";

	std::wstring unicode = Utf8ToUnicode(str);
	return UnicodeToAnsi(unicode);
}

std::string Utf8ToAnsi(const std::string &str)
{
	return Utf8ToAnsi(str.c_str());
}

std::wstring AnsiToUnicode(const char* str)
{
	if (!str || !*str)
		return L"";

	int str_len = lstrlenA(str);
	int buf_len = ::MultiByteToWideChar(936, 0, str, str_len, NULL, 0);
	std::wstring buf(buf_len, 0);
	::MultiByteToWideChar(936, 0, str, str_len, (LPWSTR)buf.data(), buf_len);
	return buf;
}

std::wstring AnsiToUnicode(const std::string &str)
{
	return AnsiToUnicode(str.c_str());
}

std::string AnsiToUtf8(const char* str)
{
	if (!str || !*str)
		return "";

	std::wstring unicode = AnsiToUnicode(str);
	return UnicodeToUtf8(unicode);
}

std::string AnsiToUtf8(const std::string &str)
{
	return AnsiToUtf8(str.c_str());
}

// Overloaded wrappers around vsnprintf and vswprintf. The buf_size parameter
// is the size of the buffer. These return the number of characters in the
// formatted string excluding the NUL terminator. If the buffer is not
// large enough to accommodate the formatted string without truncation, they
// return the number of characters that would be in the fully-formatted string
// (vsnprintf, and vswprintf on Windows), or -1 (vswprintf on POSIX platforms).
inline int vsnprintfT(char* buffer,
                      size_t buf_size,
                      const char* format,
                      va_list argptr)
{
	return vsnprintf(buffer, buf_size, format, argptr);
}

inline int vsnprintfT(wchar_t* buffer,
                      size_t buf_size,
                      const wchar_t* format,
                      va_list argptr)
{
	return vswprintf(buffer, buf_size, format, argptr);
}

// Templatized backend for StringPrintF/StringAppendF. This does not finalize
// the va_list, the caller is expected to do that.
template <class StringType>
static void StringAppendVT(StringType* dst,
                           const typename StringType::value_type* format,
                           va_list ap)
{
	// First try with a small fixed size buffer.
	// This buffer size should be kept in sync with StringUtilTest.GrowBoundary
	// and StringUtilTest.StringPrintfBounds.
	typename StringType::value_type stack_buf[1024];

	va_list ap_copy = ap;

	int result = vsnprintfT(stack_buf, arraysize(stack_buf), format, ap_copy);
	va_end(ap_copy);

	if (result >= 0 && result < static_cast<int>(arraysize(stack_buf)))
	{
		// It fit.
		dst->append(stack_buf, result);
		return;
	}

	// Repeatedly increase buffer size until it fits.
	int mem_length = arraysize(stack_buf);
	while (true)
	{
		if (result < 0)
		{
			// Try doubling the buffer size.
			mem_length *= 2;
		}
		else
		{
			// We need exactly "result + 1" characters.
			mem_length = result + 1;
		}

		if (mem_length > 32 * 1024 * 1024)
		{
			// That should be plenty, don't try anything larger.  This protects
			// against huge allocations when using vsnprintfT implementations that
			// return -1 for reasons other than overflow without setting errno.
			return;
		}

		std::vector<typename StringType::value_type> mem_buf(mem_length);

		// NOTE: You can only use a va_list once.  Since we're in a while loop, we
		// need to make a new copy each time so we don't use up the original.
		ap_copy = ap;
		result = vsnprintfT(&mem_buf[0], mem_length, format, ap_copy);
		va_end(ap_copy);

		if ((result >= 0) && (result < mem_length))
		{
			// It fit.
			dst->append(&mem_buf[0], result);
			return;
		}
	}
}

void StringAppendV(std::string* dst, const char* format, va_list ap)
{
	StringAppendVT(dst, format, ap);
}

void StringAppendV(std::wstring* dst, const wchar_t* format, va_list ap)
{
	StringAppendVT(dst, format, ap);
}

std::string StringPrintf(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	std::string result;
	StringAppendV(&result, format, ap);
	va_end(ap);
	return result;
}

std::wstring StringPrintf(const wchar_t* format, ...)
{
	va_list ap;
	va_start(ap, format);
	std::wstring result;
	StringAppendV(&result, format, ap);
	va_end(ap);
	return result;
}

void StringReplace(std::string& str, const std::string& src, const std::string& dst)
{
	size_t pos = 0;
	while((pos = str.find(src, pos)) != std::string::npos)
	{
		str.replace(pos, src.length(), dst);
		pos += dst.length();
	}
}

void StringReplace(std::wstring& str, const std::wstring& src, const std::wstring& dst)
{
	size_t pos = 0;
	while((pos = str.find(src, pos)) != std::wstring::npos)
	{
		str.replace(pos, src.length(), dst);
		pos += dst.length();
	}
}

void StringTrimLeft(std::string& str, size_t count)
{
	if (!str.empty())
	{
		if (str.length() < count)
			str.clear();
		else
			str.erase(0, count);
	}
}

void StringTrimLeft(std::wstring& str, size_t count)
{
	if (!str.empty())
	{
		if (str.length() < count)
			str.clear();
		else
			str.erase(0, count);
	}
}

void StringTrimLeft(std::string& str, const char* trim)
{
	if (!str.empty() && trim && *trim)
	{
		size_t pos = str.find_first_not_of(trim);
		if (pos != 0)
		{
			str.erase(0, pos);
		}
	}
}

void StringTrimLeft(std::string& str, const std::string& trim)
{
	StringTrimLeft(str, trim.c_str());
}

void StringTrimLeft(std::wstring& str, const wchar_t* trim)
{
	if (!str.empty() && trim && *trim)
	{
		size_t pos = str.find_first_not_of(trim);
		if (pos != 0)
		{
			str.erase(0, pos);
		}
	}
}

void StringTrimLeft(std::wstring& str, const std::wstring& trim)
{
	StringTrimLeft(str, trim.c_str());
}

void StringTrimRight(std::string& str, size_t count)
{
	if (!str.empty())
	{
		if (str.length() < count)
			str.clear();
		else
			str.erase(str.length() - count, count);
	}
}

void StringTrimRight(std::wstring& str, size_t count)
{
	if (!str.empty())
	{
		if (str.length() < count)
			str.clear();
		else
			str.erase(str.length() - count, count);
	}
}

void StringTrimRight(std::string& str, const char* trim)
{
	if (!str.empty() && trim && *trim)
	{
		size_t pos = str.find_last_not_of(trim);
		if (pos != (str.length() - 1))
		{
			str.erase(pos + 1);
		}
	}
}

void StringTrimRight(std::string& str, const std::string& trim)
{
	StringTrimRight(str, trim.c_str());
}

void StringTrimRight(std::wstring& str, const wchar_t* trim)
{
	if (!str.empty() && trim && *trim)
	{
		size_t pos = str.find_last_not_of(trim);
		if (pos != (str.length() - 1))
		{
			str.erase(pos + 1);
		}
	}
}

void StringTrimRight(std::wstring& str, const std::wstring& trim)
{
	StringTrimRight(str, trim.c_str());
}

void StringTrim(std::string& str, const char* trim)
{
	StringTrimLeft(str, trim);
	StringTrimRight(str, trim);
}

void StringTrim(std::string& str, const std::string& trim)
{
	StringTrimLeft(str, trim);
	StringTrimRight(str, trim);
}

void StringTrim(std::wstring& str, const wchar_t* trim)
{
	StringTrimLeft(str, trim);
	StringTrimRight(str, trim);
}

void StringTrim(std::wstring& str, const std::wstring& trim)
{
	StringTrimLeft(str, trim);
	StringTrimRight(str, trim);
}

void StringMakeUpper(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), toupper);
}

void StringMakeUpper(std::wstring& str)
{
	std::transform(str.begin(), str.end(), str.begin(), toupper);
}

void StringMakeLower(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), tolower);
}

void StringMakeLower(std::wstring& str)
{
	std::transform(str.begin(), str.end(), str.begin(), tolower);
}

std::string UrlEncode(const char* url)
{
	static char kHex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

	std::string ret;

	if (!url || !*url)
		return ret;

	for (const char* p = url; *p; p++)
	{
		char c = *p;

		if (c == '.' || c == '/' || c == '?' || c == '&' || c == '=' || c == ':' || 
			(c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122))
		{
			ret += c;
		}
		else
		{
			ret += '%';
			ret += kHex[(c >> 4) & 0x0F];
			ret += kHex[c & 0x0F];
		}
	}

	return ret;
}

std::string UrlEncode(const std::string& url)
{
	return UrlEncode(url.c_str());
}

std::string UrlDecode(const char* url)
{
	std::string str;

	const char* p = url;

	while (*p)
	{
		if (*p == '%')
		{
			char h = 0;
			char l = 0;
			char c = *(++p);
			if (c >= '0' && c <= '9')
			{
				h = c - '0';
			}
			else if (c >= 'A' && c <= 'F')
			{
				h = c - 'A' + 10;
			}
			else if (c >= 'a' && c <= 'f')
			{
				h = c - 'a' + 10;
			}

			c = *(++p);
			if (c >= '0' && c <= '9')
			{
				l = c - '0';
			}
			else if (c >= 'A' && c <= 'F')
			{
				l = c - 'A' + 10;
			}
			else if (c >= 'a' && c <= 'f')
			{
				l = c - 'a' + 10;
			}

			str += ((h << 4) | l);

			p++;
		}
		else
		{
			str += *p++;
		}
	}

	return str;
}

std::string UrlDecode(const std::string& url)
{
	return UrlDecode(url.c_str());
}

std::wstring UrlEncode(const wchar_t* url)
{
	std::string str = UrlEncode(UnicodeToUtf8(url));
	return Utf8ToUnicode(str);
}

std::wstring UrlEncode(const std::wstring& url)
{
	return UrlEncode(url.c_str());
}

std::wstring UrlDecode(const wchar_t* url)
{
	std::string str = UrlDecode(UnicodeToUtf8(url));
	return Utf8ToUnicode(str);
}

std::wstring UrlDecode(const std::wstring& url)
{
	return UrlDecode(url.c_str());
}

}