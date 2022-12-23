// EmulatorDlg.h : header file
//

#pragma once

#include "afxwin.h"

extern "C" {
#include "moas.h"
}

// CEmulatorDlg dialog
class CEmulatorDlg : public CDialog
{
// Construction
public:
	CEmulatorDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_EMULATOR_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

	LRESULT OnSwitchRead(WPARAM wParam, LPARAM lParam);
	void DisplayError(CString s, DWORD e);

private:
	CStatic relay_leds[MOAS_ANTENNAS];

	CStatic labels[MOAS_STATIONS];
	CStatic inhibit_leds[MOAS_STATIONS];
	CStatic tx_antennas[MOAS_STATIONS];
	CStatic rx_antennas[MOAS_STATIONS];
	CButton tx[MOAS_STATIONS];
	CFont Font;

public:
	afx_msg void OnBnClickedStart();
	CComboBox port;
	CButton start;
	afx_msg void OnBnClickedTx1();
	afx_msg void OnBnClickedTx2();
	afx_msg void OnBnClickedTx3();
	afx_msg void OnBnClickedTx4();
	afx_msg void OnBnClickedTx5();
	afx_msg void OnBnClickedTx6();

	void Write(const char *string);
	void Update(const int *relays, const int *inbibits);
	void Antennas(const int *tx, const int *rx);
};
