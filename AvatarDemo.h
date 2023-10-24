
// AvatarDemo.h: PROJECT_NAME 应用程序的主头文件
//

#pragma once

#include "framework.h"
#include "resource.h"		// 主符号
#include <WinSock2.h>

// CAvatarDemoApp:
// 有关此类的实现，请参阅 AvatarDemo.cpp
//

class CAvatarDemoApp : public CWinApp
{
	WSADATA m_wsaData;

public:
	CAvatarDemoApp();

// 重写
public:
	virtual BOOL InitInstance();

// 实现

	DECLARE_MESSAGE_MAP()
};

extern CAvatarDemoApp theApp;
