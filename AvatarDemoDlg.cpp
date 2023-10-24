
// AvatarDemoDlg.cpp: 实现文件
//

#include "framework.h"
#include "AvatarDemo.h"
#include "AvatarDemoDlg.h"
#include "afxdialogex.h"
#include "Misc.h"
#include "StringUtil.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_GENERATE_IMAGE		(WM_USER + 101)
#define PIPE_BUFFER_SIZE        10485760
#define VIDEO_PIPE_BUFFER_SIZE  (10 * 1920 * 1080 * 3)

// CAvatarDemoDlg 对话框

static time_t sVideoPTS = 0;
static time_t sAudioPTS = 0;

CAvatarDemoDlg::CAvatarDemoDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_AVATARDEMO_DIALOG, pParent)
	, audio_queue_(256)
	, video_queue_(256)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	param_ = NULL;
	audio_out_pipe_ = NULL;
	audio_in_pipe_ = NULL;
	video_pipe_ = NULL;
	keep_running_ = FALSE;
	net_thread_ = NULL;
	net_thread_id_ = 0;
	audio_thread_ = NULL;
	audio_thread_id_ = 0;
	video_thread_ = NULL;
	video_thread_id_ = 0;
	play_audio_thread_ = NULL;
	play_audio_thread_id_ = 0;
	play_video_thread_ = NULL;
	play_video_thread_id_ = 0;
	exit_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
	can_play_audio_ = false;
	audio_pts_ = 0;
}

CAvatarDemoDlg::~CAvatarDemoDlg()
{
	CloseHandle(exit_event_);
}

void CAvatarDemoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	
}

BEGIN_MESSAGE_MAP(CAvatarDemoDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CAvatarDemoDlg 消息处理程序

BOOL CAvatarDemoDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);		// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	//MoveWindow(0, 0, 566, 1031);
	::SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 566, 1031, SWP_SHOWWINDOW);
	//CenterWindow();

	audio_out_pipe_ = CreateNamedPipe(
		_T("\\\\.\\pipe\\avatar_audio_to_python"),    // pipe name 
		PIPE_ACCESS_OUTBOUND,				// write access 
		PIPE_TYPE_BYTE |					// message type pipe 
		PIPE_WAIT,							// blocking mode 
		PIPE_UNLIMITED_INSTANCES,			// max. instances  
		PIPE_BUFFER_SIZE,					// output buffer size 
		0,									// input buffer size 
		0,									// client time-out 
		NULL);								// default security attribute 

	if (audio_out_pipe_ == INVALID_HANDLE_VALUE)
	{
		logger::Log(L"Create audio output named pipe failed, error: %d.\n", GetLastError());
		return FALSE;
	}

	audio_in_pipe_ = CreateNamedPipe(
		_T("\\\\.\\pipe\\avatar_audio_to_demo"),    // pipe name 
		PIPE_ACCESS_INBOUND,				// write access 
		PIPE_TYPE_BYTE |					// message type pipe 
		PIPE_WAIT,							// blocking mode 
		PIPE_UNLIMITED_INSTANCES,			// max. instances  
		0,									// output buffer size 
		PIPE_BUFFER_SIZE,					// input buffer size 
		0,									// client time-out 
		NULL);								// default security attribute 

	if (audio_in_pipe_ == INVALID_HANDLE_VALUE)
	{
		logger::Log(L"Create audio input named pipe failed, error: %d.\n", GetLastError());
		return FALSE;
	}

	video_pipe_ = CreateNamedPipe(
		_T("\\\\.\\pipe\\avatar_video"),    // pipe name 
		PIPE_ACCESS_INBOUND,				// write access 
		PIPE_TYPE_BYTE |					// message type pipe 
		PIPE_WAIT,							// blocking mode 
		PIPE_UNLIMITED_INSTANCES,			// max. instances  
		0,									// output buffer size 
		VIDEO_PIPE_BUFFER_SIZE,				// input buffer size 
		0,									// client time-out 
		NULL);								// default security attribute 

	if (video_pipe_ == INVALID_HANDLE_VALUE)
	{
		logger::Log(L"Create video named pipe failed, error: %d.\n", GetLastError());
		return FALSE;
	}

	display_window_.Create(m_hWnd, NULL, WS_CHILDWINDOW | WS_VISIBLE, 0, 0, 0, 540, 960);

	if (!player_.Open(param_->nPlaybackDeviceId, 16000, 1, 16))
	{
		AfxMessageBox(L"打开音频播放设备失败!", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	keep_running_ = TRUE;
	net_thread_ = CreateThread(NULL, 0, CAvatarDemoDlg::NetThreadProc, this, 0, &net_thread_id_);
	audio_thread_ = CreateThread(NULL, 0, CAvatarDemoDlg::AudioThreadProc, this, 0, &audio_thread_id_);
	video_thread_ = CreateThread(NULL, 0, CAvatarDemoDlg::VideoThreadProc, this, 0, &video_thread_id_);
	play_audio_thread_ = CreateThread(NULL, 0, CAvatarDemoDlg::PlayAudioThreadProc, this, 0, &play_audio_thread_id_);
	play_video_thread_ = CreateThread(NULL, 0, CAvatarDemoDlg::PlayVideoThreadProc, this, 0, &play_video_thread_id_);

	if (!CreateAvatarEngineProcess(param_->nType + 1))
	{
		logger::Log(L"Create AvatarEngine process failed!\n");
	}

	//SetTimer(1, 40, NULL);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CAvatarDemoDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CAvatarDemoDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

DWORD CAvatarDemoDlg::NetThreadProc(LPVOID param)
{
	CAvatarDemoDlg* self = (CAvatarDemoDlg*)param;
	self->NetThreadMain();
	return 0;
}

void CAvatarDemoDlg::NetThreadMain()
{
	int ret = 0;
	SOCKET s = INVALID_SOCKET;
	HANDLE hEvent = WSACreateEvent();
	HANDLE handles[2] = { exit_event_, hEvent };
	sockaddr_in addr = {0};
	BOOL connected = FALSE;
	char buf[4096] = { 0 };
	AudioFrame* frame = NULL;
	bool first_audio_frame_sent = false;

	while (keep_running_)
	{
		connected = ConnectNamedPipe(audio_out_pipe_, NULL);
		if (connected || ERROR_PIPE_CONNECTED == GetLastError())
		{
			break;
		}
	}

	do
	{
		s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		ret = WSAEventSelect(s, hEvent, FD_CONNECT | FD_READ);

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(param_->strServerAddress.c_str());
		addr.sin_port = htons(8888);
		ret = connect(s, (const sockaddr*)&addr, sizeof(addr));
		if (ret == SOCKET_ERROR && WSAEWOULDBLOCK != WSAEWOULDBLOCK)
			break;

		while (keep_running_)
		{
			ret = WSAWaitForMultipleEvents(2, handles, FALSE, 5000, FALSE);
			if (ret == WAIT_OBJECT_0)
			{
				break;
			}
			else if (ret == WAIT_TIMEOUT)
			{
				if (!connected)
				{
					ret = connect(s, (const sockaddr*)&addr, sizeof(addr));
					if (ret != WSAEWOULDBLOCK)
						break;
				}
				else
				{
					continue;
				}
			}

			WSANETWORKEVENTS networkEvents;
			if (WSAEnumNetworkEvents(s, hEvent, &networkEvents) == SOCKET_ERROR)
			{
				break;
			}
			
			if (networkEvents.lNetworkEvents & FD_CONNECT)
			{
				connected = TRUE;
			}
			
			if (networkEvents.lNetworkEvents & FD_READ)
			{
				int len = recv(s, buf, 4096, 0);
				if (len == SOCKET_ERROR)
				{
					if (WSAEWOULDBLOCK != WSAGetLastError())
						break;
				}
				else
				{
					// 接收到音频数据
					int n = 0;
					while (n < len)
					{
						if (!frame)
						{
							frame = new AudioFrame;
						}
						else
						{
							if (frame->IsFull())
							{
								//frame->pts = sAudioPTS++;
								if (!first_audio_frame_sent)
								{
									first_audio_frame_sent = true;
									logger::Log(L"First audio frame sent!!! %I64d\n", GetUnixTime());
								}
								WriteFile(audio_out_pipe_, frame->data, frame->len, NULL, NULL);
								//audio_queue_.EnQueue(frame);
								//frame = new AudioFrame;
								frame->Reset();
							}
						}

						int free_size = frame->GetFreeSize();
						if ((n + free_size) < len)
						{
							frame->SetData((const unsigned char*)buf + n, free_size);
							n += free_size;
						}
						else
						{
							frame->SetData((const unsigned char*)buf + n, len - n);
							n = len;
						}
					}
				}
			}
		}

	} while (0);
	
	//if (frame && !frame->IsFull())
	//	delete frame;
	if (frame)
		delete frame;

	WSACloseEvent(hEvent);
	closesocket(s);
	logger::Log(L"Net thread exit!\n");
}

DWORD CAvatarDemoDlg::AudioThreadProc(LPVOID param)
{
	CAvatarDemoDlg* self = (CAvatarDemoDlg*)param;
	self->AudioThreadMain();
	return 0;
}

void CAvatarDemoDlg::AudioThreadMain()
{
	BOOL connected = FALSE;
	DWORD bytes_read = 0;
	DWORD bytes_to_read = 1280;
	unsigned char* buf = new unsigned char[bytes_to_read];

	while (keep_running_)
	{
		connected = ConnectNamedPipe(audio_in_pipe_, NULL);
		if (connected || ERROR_PIPE_CONNECTED == GetLastError())
		{
			break;
		}
	}

	bool received_first_audio_frame = false;

	while (keep_running_)
	{
		if (ReadFile(audio_in_pipe_, buf, bytes_to_read, &bytes_read, NULL))
		{
			if (!received_first_audio_frame)
			{
				received_first_audio_frame = true;
				logger::Log(L"Received first audio frame!!! %I64d\n", GetUnixTime());
			}

			AudioFrame* af = new AudioFrame;
			af->pts = sAudioPTS++;
			af->SetData(buf, bytes_read);

			while (keep_running_ && audio_queue_.IsFull())
			{
				logger::Log(L"AudioQueue is full!!!\n");
				Sleep(10);
			}

			if (keep_running_)
			{
				audio_queue_.EnQueue(af);
			}
			else
			{
				delete af;
				af = nullptr;
			}
		}
	}

	delete[] buf;
	logger::Log(L"Audio thread exit!\n");
}

DWORD CAvatarDemoDlg::VideoThreadProc(LPVOID param)
{
	CAvatarDemoDlg* self = (CAvatarDemoDlg*)param;
	self->VideoThreadMain();
	return 0;
}

void CAvatarDemoDlg::VideoThreadMain()
{
	BOOL connected = FALSE;
	DWORD bytes_read = 0;
	DWORD bytes_to_read = 1920 * 1080 * 3;
	unsigned char* buf = new unsigned char[bytes_to_read];

	while (keep_running_)
	{
		connected = ConnectNamedPipe(video_pipe_, NULL);
		if (connected || ERROR_PIPE_CONNECTED == GetLastError())
		{
			break;
		}
	}

	bool received_first_video_frame = false;

	while (keep_running_)
	{
		if (ReadFile(video_pipe_, buf, bytes_to_read, &bytes_read, NULL))
		{
			if (!received_first_video_frame)
			{
				received_first_video_frame = true;
				logger::Log(L"Received first video frame!!! %I64d\n", GetUnixTime());
			}
			
			VideoFrame* vf = new VideoFrame;
			vf->pts = sVideoPTS++;
			vf->SetData(buf, bytes_to_read);

			while (keep_running_ && video_queue_.IsFull())
			{
				logger::Log(L"VideoQueue is full!!!\n");
				Sleep(10);
			}

			if (keep_running_)
			{
				video_queue_.EnQueue(vf);
				logger::Log(L"---> Enqueue NO.%I64d video frame!!! %I64d\n", vf->pts, GetUnixTime());
			}
			else
			{
				delete vf;
				vf = nullptr;
			}
		}
	}

	delete[] buf;
	logger::Log(L"Video thread exit!\n");
}

DWORD CAvatarDemoDlg::PlayAudioThreadProc(LPVOID param)
{
	CAvatarDemoDlg* self = (CAvatarDemoDlg*)param;
	self->PlayAudioThreadMain();
	return 0;
}

void CAvatarDemoDlg::PlayAudioThreadMain()
{
	bool play_audio_started = false;

	while (keep_running_)
	{
		if (can_play_audio_ && !play_audio_started)
		{
			play_audio_started = true;
			logger::Log(L"Start playing audio!!! %I64d\n", GetUnixTime());
		}

		if (can_play_audio_)
		{
			if (!audio_queue_.IsEmpty())
			{
				AudioFrame* af = audio_queue_.DeQueue();
				player_.Write(af->data, af->len);
				audio_pts_ = af->pts;
				delete af;
			}
			else
			{
				logger::Log(L"AudioQueue is empty!!! %I64d\n", GetUnixTime());
			}
		}
		else
		{
			Sleep(1);
		}
	}
	logger::Log(L"PlayAudio thread exit!\n");
}

DWORD CAvatarDemoDlg::PlayVideoThreadProc(LPVOID param)
{
	CAvatarDemoDlg* self = (CAvatarDemoDlg*)param;
	self->PlayVideoThreadMain();
	return 0;
}

void CAvatarDemoDlg::PlayVideoThreadMain()
{
	__int64 last_play_ts = 0;
	bool play_audio_started = false;

	while (keep_running_)
	{
		if (!video_queue_.IsEmpty())
		{
			__int64 now = GetUnixTime();
			__int64 interval = now - last_play_ts;
			if (interval >= 39500)
			{
				VideoFrame* vf = nullptr;
				while (!video_queue_.IsEmpty())
				{
					vf = video_queue_.Peek();
					__int64 sync_pts = audio_pts_;
					if (abs(vf->pts - sync_pts) <= 1)
					{
						logger::Log(L"<--- Dequeue NO.%I64d video frame!!! %I64d, SyncPTS = %I64d\n", vf->pts, interval, sync_pts);
						video_queue_.DeQueue();
						break;
					}
					else 
					{
						if (vf->pts < sync_pts)
						{
							// 视频滞后于音频，丢弃视频
							logger::Log(L"<--- Dequeue NO.%I64d video frame!!! %I64d, SyncPTS = %I64d, Drop\n", vf->pts, interval, sync_pts);
							video_queue_.DeQueue();
							delete vf;
							vf = nullptr;
						}
						else
						{
							// 音频滞后于视频，等待音频
							logger::Log(L"<--- Peek NO.%I64d video frame!!! %I64d, SyncPTS = %I64d, Wait\n", vf->pts, interval, sync_pts);
							vf = nullptr;
							break;
						}
					}
				}

				if (vf)
				{
					display_window_.Display(vf->data, vf->len, 1080, 1920);
					delete vf;
					last_play_ts = now;

					if (!can_play_audio_)
					{
						can_play_audio_ = true;
						logger::Log(L"Can play audio now!!! %I64d\n", GetUnixTime());
					}
				}
			}
		}
		else
		{
			if (can_play_audio_)
				logger::Log(L"VideoQueue is empty!!! %I64d\n", GetUnixTime());
			Sleep(1);
		}
	}
	logger::Log(L"PlayVideo thread exit!\n");
}

void CAvatarDemoDlg::OnClose()
{
	keep_running_ = FALSE;
	SetEvent(exit_event_);
	if (WAIT_TIMEOUT == WaitForSingleObject(net_thread_, 5000))
		TerminateThread(net_thread_, 0);
	net_thread_ = NULL;
	CloseHandle(audio_out_pipe_);

	if (WAIT_TIMEOUT == WaitForSingleObject(play_audio_thread_, 5000))
		TerminateThread(play_audio_thread_, 0);
	play_audio_thread_ = NULL;

	if (WAIT_TIMEOUT == WaitForSingleObject(play_video_thread_, 5000))
		TerminateThread(play_video_thread_, 0);
	play_video_thread_ = NULL;

	CloseHandle(audio_in_pipe_);
	if (WAIT_TIMEOUT == WaitForSingleObject(audio_thread_, 5000))
		TerminateThread(audio_thread_, 0);
	audio_thread_ = NULL;

	CloseHandle(video_pipe_);
	if (WAIT_TIMEOUT == WaitForSingleObject(video_thread_, 5000))
		TerminateThread(video_thread_, 0);
	video_thread_ = NULL;

	player_.Close();

	while (!audio_queue_.IsEmpty())
	{
		AudioFrame* af = audio_queue_.DeQueue();
		delete af;
	}

	while (!video_queue_.IsEmpty())
	{
		VideoFrame* vf = video_queue_.DeQueue();
		delete vf;
	}

	CDialogEx::OnClose();
}


void CAvatarDemoDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1)
	{
		if (!video_queue_.IsEmpty())
		{
			VideoFrame* vf = video_queue_.DeQueue();
			logger::Log(L"<--- Dequeue NO.%I64d video frame!!! %I64d\n", vf->pts, GetUnixTime());

			display_window_.Display(vf->data, vf->len, 1080, 1920);
			delete vf;

			if (!can_play_audio_)
			{
				can_play_audio_ = true;
				logger::Log(L"Can play audio now!!! %I64d\n", GetUnixTime());
			}
		}
		else
		{
			if (can_play_audio_)
				logger::Log(L"VideoQueue is empty!!! %I64d\n", GetUnixTime());
		}
		
	}
	CDialogEx::OnTimer(nIDEvent);
}

bool CAvatarDemoDlg::CreateAvatarEngineProcess(int video_template)
{
	wchar_t app[MAX_PATH] = { 0 };
	::GetModuleFileNameW(NULL, app, MAX_PATH);
	wchar_t* filename = ::PathFindFileNameW(app);
	wcscpy(filename, L"avatar.cmd");

	wchar_t cmdline[MAX_PATH] = { 0 };
	wnsprintfW(cmdline, _countof(cmdline), L"\"%s\" %d", app, video_template);

	wchar_t current_dir[MAX_PATH] = { 0 };
	::GetModuleFileNameW(NULL, current_dir, MAX_PATH);
	filename = ::PathFindFileNameW(current_dir);
	*filename = L'\0';

	STARTUPINFOW si = { sizeof(STARTUPINFOW), 0 };
	PROCESS_INFORMATION pi = { 0 };

	BOOL success = ::CreateProcessW(NULL,
		cmdline,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		current_dir,
		&si,
		&pi);
	if (success)
	{
		::CloseHandle(pi.hProcess);
		::CloseHandle(pi.hThread);
	}

	return success == TRUE;
}