
// Test2Dlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "Test2.h"
#include "Test2Dlg.h"
#include "afxdialogex.h"
#include "MdSpi.h"
#include "TraderSpi.h"
#include <string>
#include <map>
#include <vector>
#include "DataBuffer.h"
#include "Indicator.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


using namespace std;

// UserApi����
CThostFtdcMdApi* pMDUserApi;
CThostFtdcTraderApi* pTDUserApi;

// ���ò���
char MD_FRONT_ADDR[] = "tcp://180.168.146.187:10010";		// ǰ�õ�ַ
char TD_FRONT_ADDR[] = "tcp://180.168.146.187:10001";		// ǰ�õ�ַ
TThostFtdcBrokerIDType	BROKER_ID = "9999";				// ���͹�˾����
TThostFtdcInvestorIDType INVESTOR_ID = "002340";			// Ͷ���ߴ���
TThostFtdcPasswordType  PASSWORD = "66666666";			// �û�����
char *ppInstrumentID[] = { "IF1904", "IF1905" };			// ���鶩���б�
int iInstrumentID = 2;									// ���鶩������
TThostFtdcDirectionType	DIRECTION = THOST_FTDC_D_Buy;	// ��������
TThostFtdcPriceType	LIMIT_PRICE = 3000;				// �۸�
TThostFtdcInstrumentIDType INSTRUMENT_ID = "IF1903";	// ��Լ����


// ������
int iRequestID = 0;

map<string, CThostFtdcDepthMarketDataField> g_mapQuotes;
map<string, CThostFtdcInstrumentField>g_mapInstruments;
vector<string> g_vctIFCodes;
vector<pair<string, string>> g_vctIFSpreads;



typedef struct tagSpreadStruct
{
	double dLast_Last;
	double dBid_Bid;
	double dAsk_Ask;
	double dBid_Ask;
	double dAsk_Bid;
}SpreadStruct;
map<pair<string, string>, SpreadStruct> g_mapSpreads;

SpreadStruct spread;

string strCurrentOrderRef = "";
int nCurrentOrderFrontID = 0;
int nCurrentOrderSessionID = 0;

void AddLog(CString csLog);
CString g_csLogs;
HWND g_hMemo;
HWND g_hDlg;
CRITICAL_SECTION g_lockLog;

CDataBuffer* g_pDataBuffer = NULL;
CIndicator * g_pIndicator = NULL;
vector<CThostFtdcDepthMarketDataField> vct_IF1904;

vector<Values> IF1904_SMA5;
vector<Values> IF1904_SMA30;

// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

// ʵ��
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CTest2Dlg �Ի���



CTest2Dlg::CTest2Dlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CTest2Dlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTest2Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_edIF1904);
	DDX_Control(pDX, IDC_EDIT5, m_edCurrTime);
	DDX_Control(pDX, IDC_EDIT2, m_edIF1905);
	DDX_Control(pDX, IDC_EDIT3, m_edIF1906);
	DDX_Control(pDX, IDC_EDIT4, m_edIF1909);
	DDX_Control(pDX, IDC_LIST1, m_listSpreads);
	DDX_Control(pDX, IDC_EDIT_MEMO, m_edMemo);
}

BEGIN_MESSAGE_MAP(CTest2Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON1, &CTest2Dlg::OnBnClickedButton1)
END_MESSAGE_MAP()


// CTest2Dlg ��Ϣ�������

BOOL CTest2Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ��������...���˵�����ӵ�ϵͳ�˵��С�

	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// ���ô˶Ի����ͼ�ꡣ  ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO:  �ڴ���Ӷ���ĳ�ʼ������
	InitializeCriticalSection(&g_lockLog);
	g_pDataBuffer = new CDataBuffer();
	g_pIndicator = new CIndicator();
	// ��ʼ��TDUserApi
	pTDUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi("TD");			// ����UserApi
	CTraderSpi* pTDUserSpi = new CTraderSpi();
	pTDUserApi->RegisterSpi((CThostFtdcTraderSpi*)pTDUserSpi);			// ע���¼���
	pTDUserApi->SubscribePublicTopic(TERT_RESUME);					// ע�ṫ����
	pTDUserApi->SubscribePrivateTopic(TERT_RESUME);					// ע��˽����

	// ��ʼ��MDUserApi
	pMDUserApi = CThostFtdcMdApi::CreateFtdcMdApi("MD");			// ����UserApi
	CThostFtdcMdSpi* pMDUserSpi = new CMdSpi();
	pMDUserApi->RegisterSpi(pMDUserSpi);						// ע���¼���
	pMDUserApi->RegisterFront(MD_FRONT_ADDR);					// connect

	g_pDataBuffer = new CDataBuffer;    //g_pDataBufferָ��CDataBuffer
	g_pIndicator = new CIndicator;     //g_pIndicatorָ��CIndicator

	m_listSpreads.InsertColumn(0, "",LVCFMT_RIGHT,50);
	m_listSpreads.InsertColumn(1, "1", LVCFMT_RIGHT, 50);
	m_listSpreads.InsertColumn(2, "2", LVCFMT_RIGHT, 50);
	m_listSpreads.InsertColumn(3, "3", LVCFMT_RIGHT, 50);
	m_listSpreads.InsertColumn(4, "4", LVCFMT_RIGHT, 50);
	m_listSpreads.InsertItem(0, "");
	m_listSpreads.InsertItem(1, "1");
	m_listSpreads.InsertItem(2, "2");
	m_listSpreads.InsertItem(3, "3");
	m_listSpreads.InsertItem(4, "4");

	g_hMemo = m_edMemo.GetSafeHwnd();
	g_hDlg = GetSafeHwnd();
	SetTimer(1, 500, NULL);

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
}

void CTest2Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ  ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CTest2Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CTest2Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CTest2Dlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO:  �ڴ������Ϣ�����������/�����Ĭ��ֵ

	if (nIDEvent == 1)
	{
		CTime t;
		t = CTime::GetCurrentTime();
		m_edCurrTime.SetWindowText(t.Format("%H:%M:%S"));


		if (IF1904_SMA5.size() > 0) {
			char temp_a[20];
			sprintf(temp_a, "%.2f", IF1904_SMA5.back().dValue);
			CString csTemp;
			csTemp.Format("%s", temp_a);
			m_edIF1904.SetWindowText(csTemp);
		}
		if (IF1904_SMA30.size() > 0){
			char temp_a[20];
			sprintf(temp_a, "%.2f", IF1904_SMA5.back().dValue);
			CString csTemp;
			csTemp.Format("%s", temp_a);
			m_edIF1905.SetWindowText(csTemp);
		}
		


	}
	else if (nIDEvent == 3)
	{
		KillTimer(3);
		m_edMemo.LineScroll(m_edMemo.GetLineCount(), 0);
	}

	CDialogEx::OnTimer(nIDEvent);
}


void CTest2Dlg::OnBnClickedButton1()
{
	// TODO:  �ڴ���ӿؼ�֪ͨ����������
	AddLog("����ϵͳ");
	pMDUserApi->Init();
	CString cs;
	string str;
	
	//cs=>str
	cs = "IF1904";
	cs = "m1904";
	str = cs.GetBuffer();
	cs.ReleaseBuffer();

	//str=>
	cs = str.c_str();

}

void AddLog(CString csLog)
{
	CTime t = CTime::GetCurrentTime();
	TRACE("%s\t%s\n", t.Format("%H:%M:%S"), csLog);
	CString cs;
	cs.Format("%s\t%s\r\n", t.Format("%H:%M:%S"), csLog);
	EnterCriticalSection(&g_lockLog);
	g_csLogs += cs;
	::SetWindowText(g_hMemo, g_csLogs.GetBuffer());
	g_csLogs.ReleaseBuffer();
	LeaveCriticalSection(&g_lockLog);
	::SetTimer(g_hDlg, 3, 0, NULL);
}