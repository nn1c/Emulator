// EmulatorDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Emulator.h"
#include "EmulatorDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define READ_SWITCH		(WM_USER+88)

static UINT SwitchCommThread(LPVOID pParam);
static HANDLE hSwitch;
static HANDLE hWakeup;
static CEmulatorDlg *dlg;

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CEmulatorDlg dialog




CEmulatorDlg::CEmulatorDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CEmulatorDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CEmulatorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PORT, port);
	DDX_Control(pDX, IDC_START, start);
	DDX_Control(pDX, IDC_TX1, tx[0]);
	DDX_Control(pDX, IDC_TX2, tx[1]);
	DDX_Control(pDX, IDC_TX3, tx[2]);
	DDX_Control(pDX, IDC_TX4, tx[3]);
	DDX_Control(pDX, IDC_TX5, tx[4]);
	DDX_Control(pDX, IDC_TX6, tx[5]);
}

BEGIN_MESSAGE_MAP(CEmulatorDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_START, &CEmulatorDlg::OnBnClickedStart)
	ON_BN_CLICKED(IDC_TX1, &CEmulatorDlg::OnBnClickedTx1)
	ON_BN_CLICKED(IDC_TX2, &CEmulatorDlg::OnBnClickedTx2)
	ON_BN_CLICKED(IDC_TX3, &CEmulatorDlg::OnBnClickedTx3)
	ON_BN_CLICKED(IDC_TX4, &CEmulatorDlg::OnBnClickedTx4)
	ON_BN_CLICKED(IDC_TX5, &CEmulatorDlg::OnBnClickedTx5)
	ON_BN_CLICKED(IDC_TX6, &CEmulatorDlg::OnBnClickedTx6)
	ON_MESSAGE(READ_SWITCH, OnSwitchRead)
END_MESSAGE_MAP()


// CEmulatorDlg message handlers

BOOL CEmulatorDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		(void)strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}
	
	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

#define X_RELAY_START 30 // 12
#define Y_RELAY_START 90 // 40

#define X_STATION_START 120 // 70
#define Y_LABEL_START 230 //130

#define XDELTA 45 // 27
#define YDELTA 32 // 18
#define YSIZE 20 // 12
#define XSIZE 30 // 20

	int i;
	CString label;
	RECT rc;

	Font.CreatePointFont(80, _T("Arial"));        // creates an 8-point-Courier-font

	// Build the array of relay LEDs
	for (i=0; i<MOAS_ANTENNAS; i++) {
		rc.left = ((i % 16)* XDELTA) + X_RELAY_START;
		rc.right = rc.left + XSIZE;
		rc.top = Y_RELAY_START + ((i / 16) * YDELTA);
		rc.bottom = rc.top + YSIZE;
		label.Format("%d", i);
		relay_leds[i].Create(label, SS_RIGHT | WS_CHILD | WS_VISIBLE, rc, this);
		relay_leds[i].SetFont(&Font);
		relay_leds[i].ShowWindow(/*SW_HIDE*/SW_SHOW);
	}

	RECT base;
	GetDlgItem(IDC_STATIC_INHIBIT)->GetWindowRect(&base);

	// Build the arrays of inhibit LEDs and antennas
	for (i=0; i<MOAS_STATIONS; i++) {
		rc.left = (i* XDELTA) + X_STATION_START;
		rc.right = rc.left + XSIZE;
		rc.top = Y_LABEL_START;
		rc.bottom = rc.top + YSIZE;
		label.Format("%d", i+1);
		labels[i].Create(label, SS_RIGHT | WS_CHILD | WS_VISIBLE, rc, this);
		labels[i].SetFont(&Font);

		rc.top = Y_LABEL_START + YDELTA;
		rc.bottom = rc.top + YSIZE;
		tx_antennas[i].Create("63", SS_RIGHT | WS_CHILD | WS_VISIBLE, rc, this);
		tx_antennas[i].SetFont(&Font);

		rc.top = Y_LABEL_START + (YDELTA*2);
		rc.bottom = rc.top + YSIZE;
		rx_antennas[i].Create("63", SS_RIGHT | WS_CHILD | WS_VISIBLE, rc, this);
		rx_antennas[i].SetFont(&Font);

		rc.top = Y_LABEL_START + (YDELTA*3);
		rc.bottom = rc.top + YSIZE;
		inhibit_leds[i].Create("X", SS_RIGHT | WS_CHILD | WS_VISIBLE, rc, this);
		inhibit_leds[i].SetFont(&Font);

		tx[i].EnableWindow(FALSE);

	}

	for (i=1; i<=32; i++) {
		label.Format("COM%d", i);
		port.AddString(label);
		port.SetCurSel(24);
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CEmulatorDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CEmulatorDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CEmulatorDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CEmulatorDlg::OnBnClickedStart()
{
	int i;

	start.ShowWindow(SW_HIDE);
	port.EnableWindow(FALSE);

	dlg = this;

	for (i=0; i<MOAS_STATIONS; i++) {
		tx[i].EnableWindow(TRUE);
	}

	DCB dcb;
	CString com;

	com.Format("\\\\.\\COM%d", port.GetCurSel()+1);

	hSwitch = CreateFile(com,
						 GENERIC_READ | GENERIC_WRITE, 
						 0,
						 0,
						 OPEN_EXISTING,
						 FILE_FLAG_OVERLAPPED,
						 0);

	if (hSwitch == INVALID_HANDLE_VALUE) {
		DisplayError("Could not open COM port: ", GetLastError());
		exit(0);
	}

	memset (&dcb, 0, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	if (!BuildCommDCB("baud=9600 parity=N data=8 stop=1", &dcb)) {
		DisplayError("Could not build COM port DCB: ", GetLastError());
		CloseHandle(hSwitch);
		exit(0);
	}

	if (!SetCommState(hSwitch, &dcb)) {
		DisplayError("Could not set MOAS COM port state: ", GetLastError());
		CloseHandle(hSwitch);
		exit(0);
	}

	hWakeup = CreateEvent(NULL, TRUE, TRUE, "");
	if (hWakeup == NULL) {
		DisplayError("Could not create MOAS wakeup event: ", GetLastError());
		CloseHandle(hSwitch);
		exit(0);
	}

	// Kick off the read thread
	AfxBeginThread(SwitchCommThread, m_hWnd);

	moas_initialize();
}

void CEmulatorDlg::OnBnClickedTx1()
{
	moas_txrx(1, tx[0].GetCheck());
}

void CEmulatorDlg::OnBnClickedTx2()
{
	moas_txrx(2, tx[1].GetCheck());
}

void CEmulatorDlg::OnBnClickedTx3()
{
	moas_txrx(3, tx[2].GetCheck());
}

void CEmulatorDlg::OnBnClickedTx4()
{
	moas_txrx(4, tx[3].GetCheck());
}

void CEmulatorDlg::OnBnClickedTx5()
{
	moas_txrx(5, tx[4].GetCheck());
}

void CEmulatorDlg::OnBnClickedTx6()
{
	moas_txrx(6, tx[5].GetCheck());
}

LRESULT CEmulatorDlg::OnSwitchRead(WPARAM wParam, LPARAM lParam)
{
	if (lParam == 0xffff) {
		DisplayError("Read Error: ", (DWORD)wParam);
		return TRUE;
	}

	moas_character((char)lParam);

	return TRUE;
}

void CEmulatorDlg::Write(const char *string)
{
	OVERLAPPED osWrite = {0};
	DWORD dwWritten;
	DWORD len = (DWORD)strlen(string);

	// Create this writes OVERLAPPED structure hEvent.
	osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osWrite.hEvent == NULL) {
		DisplayError("Could not create write event: ", GetLastError());
		return;
	}

	// Issue write.
	if (!WriteFile(hSwitch, string, len, &dwWritten, &osWrite)) {
		if (GetLastError() != ERROR_IO_PENDING) { 
			// WriteFile failed, but it isn't delayed. Report error and abort.
			DisplayError("Could not create write to switch: ", GetLastError());
		}
		else {
			// Write is pending.
			if (!GetOverlappedResult(hSwitch, &osWrite, &dwWritten, TRUE)) {
				DisplayError("Write failure: ", GetLastError());
			}
		}
	}
	CloseHandle(osWrite.hEvent);
	return;
}

void 
CEmulatorDlg::Update(const int *relays, const int *inhibits)
{
	int i;

	for (i=0; i<MOAS_RELAYS; i++) {
		relay_leds[i].ShowWindow(relays[i]);
	}

	for (i=0; i<MOAS_STATIONS; i++) {
		inhibit_leds[i].ShowWindow(inhibits[i]);
		tx[i].EnableWindow(!inhibits[i]);
	}
}

void
CEmulatorDlg::Antennas(const int *tx, const int *rx)
{
	CString t;
	int i;

	for (i=0; i<MOAS_STATIONS; i++) {
		t.Format("%d", tx[i]);
		tx_antennas[i].SetWindowText(t);
		t.Format("%d", rx[i]);
		rx_antennas[i].SetWindowText(t);
	}
}

void
CEmulatorDlg::DisplayError(CString s, DWORD e)
//----------------------------------------------------------------------
//Display a message and error string in the log List Box.
//----------------------------------------------------------------------
{
	LPVOID lpMsgBuf;
	USHORT Index = 0;
	CString	strError = "";
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		e,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);

	strError = s + (LPCTSTR)lpMsgBuf;;

	//Trim CR/LF from the error message.

	strError.TrimRight(); 
	LocalFree(lpMsgBuf); 
	AfxMessageBox(strError);
}

static UINT
SwitchCommThread(LPVOID pParam)
//----------------------------------------------------------------------
// Read a character from the switch - Separate thread, not in class
//----------------------------------------------------------------------
{
	DWORD	Result;
	DWORD	bytes;
	DWORD   commEvent = 0;
	DWORD   errors;
	char    data;
	HWND    hWnd = (HWND)pParam;
	OVERLAPPED Overlapped;
	DWORD   NumberOfBytesRead;
	COMSTAT comStat;
	BOOL    error_reported = false;

	Overlapped.hEvent = hWakeup;

	SetCommMask(hSwitch, EV_RXCHAR);
	while (1) {
 
		Overlapped.Offset = 0;
		Overlapped.OffsetHigh = 0;

		WaitCommEvent(hSwitch, &commEvent, (LPOVERLAPPED)&Overlapped);

		// IO pending is expected
		Result = GetLastError();
		if (Result == ERROR_IO_PENDING) {

			// Wait for a comm event
			WaitForSingleObject(hWakeup, INFINITE);

			// A comm event or error has occurred
			if (GetOverlappedResult(hSwitch,
					(LPOVERLAPPED) &Overlapped, &bytes, TRUE)) {
				// A comm event.
				if (!(commEvent & EV_RXCHAR)) {
					continue;
				}
			}
			else {
				if (!error_reported) {
					// An error occurred
					::PostMessage(hWnd, READ_SWITCH, GetLastError(), 0xffff);
					error_reported = true;
				}
			}
		}
		else {
			// There may have already been an event, no wait needed
			if (Result == 0) {
				if (!(commEvent & EV_RXCHAR)) {
					continue;
				}
			}
			else {
				// An immediate error
				if (!error_reported) {
					::PostMessage(hWnd, READ_SWITCH, GetLastError(), 0xffff);
					error_reported = true;
				}
			}
		}

		ResetEvent(hWakeup);
		Overlapped.Offset = 0;
		Overlapped.OffsetHigh = 0;

		// The comm event showed something in the buffer
		if (ClearCommError(hSwitch, &errors, &comStat)) {

			while (comStat.cbInQue--) {
				(void)ReadFile (hSwitch, &data, 1,
							&NumberOfBytesRead, (LPOVERLAPPED)&Overlapped);

				// IO pending is expected
				Result = GetLastError();
				if (Result == ERROR_IO_PENDING) {

					// Wait for the IO to complete or the main loop do wake us because
					// it disconnected
					WaitForSingleObject(hWakeup, INFINITE);

					// A character arrived or an error occurred
					if (GetOverlappedResult(hSwitch,
							(LPOVERLAPPED) &Overlapped, &bytes, TRUE)) {
						// A character was read or a zero-length read occurred
						if (bytes) {
							::PostMessage(hWnd, READ_SWITCH, 0, data);
						}
					}
					else {
						if (!error_reported) {
							// An error occurred
							::PostMessage(hWnd, READ_SWITCH, GetLastError(), 0xffff);
							error_reported = true;
						}
					}
				}
				else {
					// There may have already been characters, no wait needed
					if (Result == 0) {
						if (NumberOfBytesRead) {
							::PostMessage(hWnd, READ_SWITCH, 0, data);
						}
					}
					else {
						// An immediate error
						if (!error_reported) {
							::PostMessage(hWnd, READ_SWITCH, GetLastError(), 0xffff);
							error_reported = true;
						}
					}
				}

				ResetEvent(hWakeup);
			}
		}
		else {
			// ClearComError() failed.
			if (!error_reported) {
				::PostMessage(hWnd, READ_SWITCH, GetLastError(), 0xffff);
				error_reported = true;
			}
		}
	}
	return 0;
}

// These are the trampoline routines to get from the C code MOAS II
// emulator to the C++ code in this file.

void moas_callback_write(const char *buffer)
{
	dlg->Write(buffer);
}

void moas_callback_update(const int *relays, const int *inhibits)
{
	dlg->Update(relays, inhibits);
}

void moas_callback_antennas(const int *tx, const int *rx)
{
	dlg->Antennas(tx, rx);
}