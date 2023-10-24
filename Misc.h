#ifndef MISC_H
#define MISC_H

#include <windows.h>
#include <string>
#include <vector>
#include <strsafe.h>

namespace logger {

void Log(const wchar_t* fmt, ...);

}

HBITMAP DecodeImage(const char* filename);

__int64 GetUnixTime();

#endif