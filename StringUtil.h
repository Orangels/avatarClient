#ifndef BASE_STRING_UTIL_H
#define BASE_STRING_UTIL_H

#include <string>

namespace util {

// Reserves enough memory in |str| to accommodate |length_with_null| characters,
// sets the size of |str| to |length_with_null - 1| characters, and returns a
// pointer to the underlying contiguous array of characters.  This is typically
// used when calling a function that writes results into a character array, but
// the caller wants the data to be managed by a string-like object.  It is
// convenient in that is can be used inline in the call, and fast in that it
// avoids copying the results of the call from a char* into a string.
//
// |length_with_null| must be at least 2, since otherwise the underlying string
// would have size 0, and trying to access &((*str)[0]) in that case can result
// in a number of problems.
//
// Internally, this takes linear time because the resize() call 0-fills the
// underlying array for potentially all
// (|length_with_null - 1| * sizeof(string_type::value_type)) bytes.  Ideally we
// could avoid this aspect of the resize() call, as we expect the caller to
// immediately write over this memory, but there is no other way to set the size
// of the string, and not doing that will mean people who access |str| rather
// than str.c_str() will get back a string of whatever size |str| had on entry
// to this function (probably 0).
template <class string_type>
inline typename string_type::value_type* WriteInto(string_type* str,
												   size_t length_with_null)
{
	str->reserve(length_with_null);
	str->resize(length_with_null - 1);
	return &((*str)[0]);
}

std::string UnicodeToUtf8(const wchar_t* str);
std::string UnicodeToUtf8(const std::wstring &str);
std::string UnicodeToAnsi(const wchar_t* str);
std::string UnicodeToAnsi(const std::wstring &str);
std::wstring Utf8ToUnicode(const char* str);
std::wstring Utf8ToUnicode(const std::string &str);
std::string Utf8ToAnsi(const char* str);
std::string Utf8ToAnsi(const std::string &str);
std::wstring AnsiToUnicode(const char* str);
std::wstring AnsiToUnicode(const std::string &str);
std::string AnsiToUtf8(const char* str);
std::string AnsiToUtf8(const std::string &str);

std::string StringPrintf(const char* format, ...);
std::wstring StringPrintf(const wchar_t* format, ...);
void StringReplace(std::string& str, const std::string& src, const std::string& dst);
void StringReplace(std::wstring& str, const std::wstring& src, const std::wstring& dst);
void StringTrimLeft(std::string& str, size_t count);
void StringTrimLeft(std::wstring& str, size_t count);
void StringTrimLeft(std::string& str, const char* trim);
void StringTrimLeft(std::string& str, const std::string& trim);
void StringTrimLeft(std::wstring& str, const wchar_t* trim);
void StringTrimLeft(std::wstring& str, const std::wstring& trim);
void StringTrimRight(std::string& str, size_t count);
void StringTrimRight(std::wstring& str, size_t count);
void StringTrimRight(std::string& str, const char* trim);
void StringTrimRight(std::string& str, const std::string& trim);
void StringTrimRight(std::wstring& str, const wchar_t* trim);
void StringTrimRight(std::wstring& str, const std::wstring& trim);
void StringTrim(std::string& str, const char* trim);
void StringTrim(std::string& str, const std::string& trim);
void StringTrim(std::wstring& str, const wchar_t* trim);
void StringTrim(std::wstring& str, const std::wstring& trim);

void StringMakeUpper(std::string& str);
void StringMakeUpper(std::wstring& str);
void StringMakeLower(std::string& str);
void StringMakeLower(std::wstring& str);

std::string UrlEncode(const char* url);
std::string UrlEncode(const std::string& url);

std::string UrlDecode(const char* url);
std::string UrlDecode(const std::string& url);

std::wstring UrlEncode(const wchar_t* url);
std::wstring UrlEncode(const std::wstring& url);

std::wstring UrlDecode(const wchar_t* url);
std::wstring UrlDecode(const std::wstring& url);
}

#endif
