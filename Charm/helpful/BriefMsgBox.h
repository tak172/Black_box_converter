#pragma once
#ifndef LINUX
#else
#define MB_OK                       0x00000000L
#define MB_OKCANCEL                 0x00000001L
#define MB_ABORTRETRYIGNORE         0x00000002L
#define MB_YESNOCANCEL              0x00000003L
#define MB_YESNO                    0x00000004L
#define MB_RETRYCANCEL              0x00000005L

#define MB_ICONHAND                 0x00000010L
#define MB_ICONQUESTION             0x00000020L
#define MB_ICONEXCLAMATION          0x00000030L
#define MB_ICONASTERISK             0x00000040L

#define MB_DEFBUTTON1               0x00000000L
#define MB_DEFBUTTON2               0x00000100L
#define MB_DEFBUTTON3               0x00000200L
#define MB_DEFBUTTON4               0x00000300L

#define IDOK                1
#define IDCANCEL            2
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define IDYES               6
#define IDNO                7

inline int MulDiv( int nNumber, int nNumerator, int nDenominator )
{
    // отсечение ошибки
    if( 0 == nDenominator )
        return -1;
    // убираем знак из знаменателя
    if( nDenominator < 0 )
    {
        nDenominator = -nDenominator;
        nNumerator = -nNumerator;
    }
    // 
    long long ret = nNumber;
    ret *= nNumerator;
    // знак минуса отделим
    bool neg = ret < 0;
    if( neg )
        ret = -ret;
    // деление с округлением
    ret += nDenominator / 2;
    ret /= nDenominator;
    // возвращаем знак
    if( neg )
        ret = -ret;
    return (int) ret;
}

#endif

#ifndef _BRIEFMSGBOX_H_ // нужно для Charm\UT\stdafx.h
#define _BRIEFMSGBOX_H_

#ifdef __AFXWIN_H__

// BriefMsgBox_impl dialog
class BriefMsgBox_impl : public CDialog
{
	DECLARE_DYNAMIC(BriefMsgBox_impl)

public:
	BriefMsgBox_impl( LPCTSTR lpText, LPCTSTR lpCaption,
                      UINT uType, unsigned timeout =0, int bTimer =0 );

    virtual ~BriefMsgBox_impl();
    virtual BOOL OnInitDialog();
    void OnTimer(UINT_PTR nIDEvent);
    void setRemember(bool* remember );

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void OnOK();
    virtual void OnCancel();
    void OnYes();
    void OnNo();
    void OnAbort();
    void OnRetry();
    void OnIgnore();

	DECLARE_MESSAGE_MAP()

private:
    HGLOBAL getGlobalHandle();
    void createHeader( int cx, int cy, const std::wstring& title, int estyle );
    void addButton( int id, const std::wstring& text );
    void addChkbox( int id, wchar_t* text );
    void addStatic( int id, const wchar_t* text );
    void addIcon  ( int x, int y, int cx, int cy, int id );
    void setIcon( UINT uType );
    int calcExStyle( UINT uType );
    void placeNeedButton( UINT uType );
    size_t addItem( const RECT& r, int id, const wchar_t* text, WORD control_class, DWORD style );
    void buttonAdjust();
    void Memo();
    void initMemoBox();

    int getDefButID();
    int getTimerButID();

    void SetDefaultButton();

    CStatic stText;
    CStatic stIcon;
    CButton stMemo;
    LPCTSTR lpText;
    UINT uType;
    std::vector<char> vTplData;
    std::vector<size_t> btTplPosition;
    time_t wait;
    time_t finish;
    unsigned bTimer;
    unsigned bDefault;
    UINT_PTR timerId;
    std::wstring timeStr;
    HGLOBAL m_hgbl;
    bool * pRemember;

    int pix2diaX(int pixX);
    int pix2diaY(int pixY);
    int dia2pixX(int diaX);
    int dia2pixY(int diaY);
    int baseunit_X; // базовые единицы измерения диалога 
    int baseunit_Y;
    static const int BUTTON_W=61;  // ширина любой кнопки
    static const int BUTTON_H=15;  // высота любой кнопки 
    static const int MEMBOX_W=85;  // ширина любой кнопки
    static const int MEMBOX_H=15;  // высота любой кнопки 
    static const int SPACE_XY=5;   // отступ от края диалога 
    static const int TOTAL_W=250;  // размеры диалога в целом
    static const int TOTAL_H=100;
};

// простой вызов 
int BriefMsgBox(const CWnd* /*pParent*/,
                 LPCTSTR lpText,
                 LPCTSTR lpCaption,
                 UINT uType,
                 unsigned timeout =0,
                 int bTimer =0
                 );
#else

class CWnd;
#ifdef LINUX
#ifndef UNREFERENCED_PARAMETER 
#define UNREFERENCED_PARAMETER(x) (void)x; 
#endif
#endif

inline int BriefMsgBox( const CWnd* /*pParent*/,
                 const wchar_t * lpText,
                 const wchar_t * lpCaption,
                 unsigned uType,
                 unsigned timeout =0,
                 int bTimer =0 )
{
    UNREFERENCED_PARAMETER(lpText);
    UNREFERENCED_PARAMETER(lpCaption);
    UNREFERENCED_PARAMETER(uType);
    UNREFERENCED_PARAMETER(timeout);
    UNREFERENCED_PARAMETER(bTimer);
	return 0;
}
#endif //__AFXWIN_H__
#endif // _BRIEFMSGBOX_H_
