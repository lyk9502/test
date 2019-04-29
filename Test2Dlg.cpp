
// Test2Dlg.cpp : 实现文件
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

// UserApi对象
CThostFtdcMdApi* pMDUserApi;
CThostFtdcTraderApi* pTDUserApi;

// 配置参数
char MD_FRONT_ADDR[] = "tcp://180.168.146.187:10010";		// 前置地址
char TD_FRONT_ADDR[] = "tcp://180.168.146.187:10001";		// 前置地址
TThostFtdcBrokerIDType	BROKER_ID = "9999";				// 经纪公司代码
TThostFtdcInvestorIDType INVESTOR_ID = "002340";			// 投资者代码
TThostFtdcPasswordType  PASSWORD = "66666666";			// 用户密码
char *ppInstrumentID[] = { "IF1904", "IF1905" };			// 行情订阅列表
int iInstrumentID = 2;									// 行情订阅数量
TThostFtdcDirectionType	DIRECTION = THOST_FTDC_D_Buy;	// 买卖方向
TThostFtdcPriceType	LIMIT_PRICE = 3000;				// 价格
TThostFtdcInstrumentIDType INSTRUMENT_ID = "IF1903";	// 合约代码


// 请求编号
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

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
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


// CTest2Dlg 对话框



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


// CTest2Dlg 消息处理程序

BOOL CTest2Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
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

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO:  在此添加额外的初始化代码
	InitializeCriticalSection(&g_lockLog);
	g_pDataBuffer = new CDataBuffer();
	g_pIndicator = new CIndicator();
	// 初始化TDUserApi
	pTDUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi("TD");			// 创建UserApi
	CTraderSpi* pTDUserSpi = new CTraderSpi();
	pTDUserApi->RegisterSpi((CThostFtdcTraderSpi*)pTDUserSpi);			// 注册事件类
	pTDUserApi->SubscribePublicTopic(TERT_RESUME);					// 注册公有流
	pTDUserApi->SubscribePrivateTopic(TERT_RESUME);					// 注册私有流

	// 初始化MDUserApi
	pMDUserApi = CThostFtdcMdApi::CreateFtdcMdApi("MD");			// 创建UserApi
	CThostFtdcMdSpi* pMDUserSpi = new CMdSpi();
	pMDUserApi->RegisterSpi(pMDUserSpi);						// 注册事件类
	pMDUserApi->RegisterFront(MD_FRONT_ADDR);					// connect

	g_pDataBuffer = new CDataBuffer;    //g_pDataBuffer指向CDataBuffer
	g_pIndicator = new CIndicator;     //g_pIndicator指向CIndicator

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

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
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

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CTest2Dlg::OnPaint()
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
HCURSOR CTest2Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CTest2Dlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO:  在此添加消息处理程序代码和/或调用默认值

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
	// TODO:  在此添加控件通知处理程序代码
	AddLog("启动系统");
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