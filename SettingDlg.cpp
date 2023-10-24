// CSettingDlg.cpp: 实现文件
//

#include "AvatarDemo.h"
#include "SettingDlg.h"
#include "Misc.h"
#include "StringUtil.h"
#include "afxdialogex.h"
#include <portaudio.h>


// CSettingDlg 对话框

IMPLEMENT_DYNAMIC(CSettingDlg, CDialogEx)

CSettingDlg::CSettingDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PAGE1, pParent)
	, m_nType(0)
	, m_strServerAddress(_T("127.0.0.1"))
	, m_hRealBmp(NULL)
	, m_hFakeBmp(NULL)
{

}

CSettingDlg::~CSettingDlg()
{
	if (m_hRealBmp)
		DeleteObject(m_hRealBmp);

	if (m_hFakeBmp)
		DeleteObject(m_hFakeBmp);
}

void CSettingDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_TYPE_REAL, m_nType);
	DDX_Control(pDX, IDC_PLAYBACK_DEVICES, m_wndPlaybackDevices);
	DDX_Text(pDX, IDC_SERVER_ADDRESS, m_strServerAddress);
}


BEGIN_MESSAGE_MAP(CSettingDlg, CDialogEx)
	ON_BN_CLICKED(IDC_SELECT_DONE, &CSettingDlg::OnSelectDone)
	ON_WM_PAINT()
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CSettingDlg 消息处理程序

BOOL CSettingDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	int defaultId = Pa_GetDefaultOutputDevice();

	int nCount = Pa_GetDeviceCount();
	for (int i = 0; i < nCount; i++)
	{
		const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
		if (info->maxOutputChannels)
		{
			std::wstring text;
			if (i == defaultId)
				text = util::Utf8ToUnicode(info->name) + L" (默认)";
			else
				text = util::Utf8ToUnicode(info->name);
			int id = m_wndPlaybackDevices.AddString(text.c_str());
			m_wndPlaybackDevices.SetItemData(id, i);

			if (i == defaultId)
				m_wndPlaybackDevices.SetCurSel(id);
		}
	}

	GetDlgItem(IDC_PIC_REAL)->GetWindowRect(m_rcReal);
	GetDlgItem(IDC_PIC_REAL)->DestroyWindow();
	ScreenToClient(m_rcReal);

	GetDlgItem(IDC_PIC_FAKE)->GetWindowRect(m_rcFake);
	GetDlgItem(IDC_PIC_FAKE)->DestroyWindow();
	ScreenToClient(m_rcFake);

	m_hRealBmp = DecodeImage("face_01.jpg");
	m_hFakeBmp = DecodeImage("face_02.jpg");

	return TRUE;
}

void CSettingDlg::OnSelectDone()
{
	UpdateData(TRUE);
	if (m_wndPlaybackDevices.GetCurSel() == -1)
	{
		AfxMessageBox(_T("请选择音频播放设备"), MB_OK | MB_ICONERROR);
		return;
	}

	if (m_strServerAddress.IsEmpty())
	{
		AfxMessageBox(_T("请指定服务器地址"), MB_OK | MB_ICONERROR);
		return;
	}

	m_nPlaybackDeviceId = m_wndPlaybackDevices.GetItemData(m_wndPlaybackDevices.GetCurSel());
	
	CDialogEx::OnOK();
}

void CSettingDlg::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	BITMAP bm;
	GetObject(m_hRealBmp, sizeof(BITMAP), &bm);

	if (m_hRealBmp && m_hFakeBmp)
	{
		CDC bmpDC;
		bmpDC.CreateCompatibleDC(&dc);
		HGDIOBJ hOldBmp = bmpDC.SelectObject(m_hRealBmp);
		dc.SetStretchBltMode(HALFTONE);
		dc.StretchBlt(m_rcReal.left, m_rcReal.top, m_rcReal.Width(), m_rcReal.Height(), &bmpDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		bmpDC.SelectObject(hOldBmp);

		GetObject(m_hFakeBmp, sizeof(BITMAP), &bm);
		hOldBmp = bmpDC.SelectObject(m_hFakeBmp);
		dc.SetStretchBltMode(HALFTONE);
		dc.StretchBlt(m_rcFake.left, m_rcFake.top, m_rcFake.Width(), m_rcFake.Height(), &bmpDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		bmpDC.SelectObject(hOldBmp);

		bmpDC.DeleteDC();
	}
}

void CSettingDlg::OnClose()
{
	CDialogEx::OnCancel();
}
