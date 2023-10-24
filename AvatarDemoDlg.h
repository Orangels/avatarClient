
// AvatarDemoDlg.h: 头文件
//

#pragma once

#include "DisplayWindow.h"
#include "player.h"
#include "Param.h"
#include "Common.h"

// CAvatarDemoDlg 对话框
class CAvatarDemoDlg : public CDialogEx
{
	HANDLE audio_out_pipe_;
	HANDLE audio_in_pipe_;
	HANDLE video_pipe_;

	BOOL keep_running_;
	HANDLE net_thread_;
	DWORD net_thread_id_;
	HANDLE audio_thread_;
	DWORD audio_thread_id_;
	HANDLE video_thread_;
	DWORD video_thread_id_;
	HANDLE play_audio_thread_;
	DWORD play_audio_thread_id_;
	HANDLE play_video_thread_;
	DWORD play_video_thread_id_;
	HANDLE exit_event_;

	Player player_;
	Param* param_;
	DisplayWindow display_window_;
	FrameQueue<AudioFrame*> audio_queue_;
	FrameQueue<VideoFrame*> video_queue_;

	std::mutex play_audio_mutex_;
	std::condition_variable play_audio_cv_;
	std::atomic_bool can_play_audio_;
	std::atomic_llong audio_pts_;

// 构造
public:
	CAvatarDemoDlg(CWnd* pParent = nullptr);	// 标准构造函数
	~CAvatarDemoDlg();

	void SetParam(Param* param) { param_ = param; }

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AVATARDEMO_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	DECLARE_MESSAGE_MAP()

	static DWORD CALLBACK NetThreadProc(LPVOID param);
	void NetThreadMain();

	static DWORD CALLBACK AudioThreadProc(LPVOID param);
	void AudioThreadMain();

	static DWORD CALLBACK VideoThreadProc(LPVOID param);
	void VideoThreadMain();

	static DWORD CALLBACK PlayAudioThreadProc(LPVOID param);
	void PlayAudioThreadMain();

	static DWORD CALLBACK PlayVideoThreadProc(LPVOID param);
	void PlayVideoThreadMain();

	bool CreateAvatarEngineProcess(int video_template);

};
