#pragma once


// CSettingDlg 对话框
#include "framework.h"

class CSettingDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CSettingDlg)
	CComboBox m_wndPlaybackDevices;
	CRect m_rcReal;
	CRect m_rcFake;
	HBITMAP m_hRealBmp;
	HBITMAP m_hFakeBmp;

public:
	int m_nType;
	int m_nPlaybackDeviceId;
	CString m_strServerAddress;

public:
	CSettingDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CSettingDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SETTING_DIALOG };
#endif

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

	afx_msg void OnSelectDone();
	afx_msg void OnPaint();
public:
	afx_msg void OnClose();
};
