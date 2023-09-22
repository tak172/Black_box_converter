// BriefMsgBox_impl.cpp : implementation file
//
#include "stdafx.h"
#include <string>
#include <vector>
#include <boost/foreach.hpp>
#include <stdlib.h>
#include "../helpful/RT_Macros.h"
#include "../helpful/X_translate.h"
#include "../helpful/BriefMsgBox.h"


using namespace std;

const int IDC_BRIEF_ICON = 500;
const int IDC_BRIEF_TEXT = 555;
const int IDC_BRIEF_CHECKBOX = 577;

static void append( vector<char>& vTplData, WORD w )
{
    const char * p=reinterpret_cast<const char*>(&w);
    vTplData.insert(vTplData.end(),p,p+sizeof(*&w) );
}
static void append( vector<char>& vTplData, const DLGTEMPLATE& w )
{
    const char * p=reinterpret_cast<const char*>(&w);
    vTplData.insert(vTplData.end(),p,p+sizeof(*&w) );
}
static void append( vector<char>& vTplData, const DLGITEMTEMPLATE& w )
{
    const char * p=reinterpret_cast<const char*>(&w);
    vTplData.insert(vTplData.end(),p,p+sizeof(*&w) );
}
static void append( vector<char>& vTplData, const std::wstring &name )
{
    for( const wchar_t* pw = name.c_str(); *pw; pw++ )
        append( vTplData, *pw );
    append( vTplData, wchar_t(0) );
}

int BriefMsgBox(const CWnd* /*pParent*/, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType, unsigned timeout /*=0*/, int bTimer /*=0 */)
{
    BriefMsgBox_impl box( lpText, lpCaption, uType, timeout, bTimer );
    return static_cast<int> ( box.DoModal() );
}

// BriefMsgBox_impl dialog

IMPLEMENT_DYNAMIC(BriefMsgBox_impl, CDialog)
BriefMsgBox_impl::BriefMsgBox_impl(
                         LPCTSTR _lpText,
                         LPCTSTR lpCaption,
                         UINT _uType,
                         unsigned _timeout /*=0*/,
                         int _bTimer/*=0*/
                         )
	: CDialog(), vTplData(), btTplPosition(), 
    uType(_uType), lpText(_lpText),
    wait(_timeout), finish(0),
    bTimer(_bTimer), bDefault(0),
    baseunit_X(LOWORD(GetDialogBaseUnits())),
    baseunit_Y(HIWORD(GetDialogBaseUnits())),
    pRemember(NULL)
{
    createHeader( TOTAL_W, TOTAL_H, lpCaption, calcExStyle(uType) );
    addIcon( 4, 9,  21, 20, IDC_BRIEF_ICON );
    addStatic( IDC_BRIEF_TEXT, lpText );
    addChkbox( IDC_BRIEF_CHECKBOX, L"всегда отвечать также" );
    placeNeedButton(uType);
    m_hgbl = getGlobalHandle();
    InitModalIndirect(m_hgbl);
}

BriefMsgBox_impl::~BriefMsgBox_impl()
{
    GlobalFree(m_hgbl);
}

void BriefMsgBox_impl::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
    DDX_Control( pDX, IDC_BRIEF_TEXT, stText );
    DDX_Control( pDX, IDC_BRIEF_ICON, stIcon );
    DDX_Control( pDX, IDC_BRIEF_CHECKBOX, stMemo );
}



BEGIN_MESSAGE_MAP(BriefMsgBox_impl, CDialog)
    ON_WM_TIMER()
    ON_BN_CLICKED( IDYES, OnYes )
    ON_BN_CLICKED( IDNO, OnNo )
    ON_BN_CLICKED( IDABORT, OnAbort )
    ON_BN_CLICKED( IDRETRY, OnRetry )
    ON_BN_CLICKED( IDIGNORE, OnIgnore )
END_MESSAGE_MAP()


// BriefMsgBox_impl message handlers

void BriefMsgBox_impl::OnOK()
{
    Memo();
    CDialog::OnOK();
}

void BriefMsgBox_impl::OnCancel()
{
    Memo();
    CDialog::OnCancel();
}

void BriefMsgBox_impl::OnYes()
{
    Memo();
    EndDialog(IDYES);
}

void BriefMsgBox_impl::OnNo()
{
    Memo();
    EndDialog(IDNO);
}

void BriefMsgBox_impl::OnAbort()
{
    Memo();
    EndDialog(IDABORT);
}

void BriefMsgBox_impl::OnRetry()
{
    Memo();
    EndDialog(IDRETRY);
}

void BriefMsgBox_impl::OnIgnore()
{
    Memo();
    EndDialog(IDIGNORE);
}

void BriefMsgBox_impl::Memo()
{
    if ( 0!=pRemember && BST_CHECKED == stMemo.GetCheck() )
        *pRemember = true;
}

void BriefMsgBox_impl::createHeader( int cx, int cy, const std::wstring& title, int estyle )
{
    DLGTEMPLATE dt;
    ZeroMemory(&dt,sizeof(dt));
    dt.style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION | WS_VISIBLE | DS_CENTER | WS_CLIPSIBLINGS;
    dt.dwExtendedStyle = estyle;
    dt.cdit = 0;         // Number of controls
    dt.x  = 0;  
    dt.y  = 0;
    dt.cx = static_cast<short>(cx); 
    dt.cy = static_cast<short>(cy);
    append( vTplData, dt );
    append( vTplData, WORD(0) ); // No menu
    append( vTplData, WORD(0) ); // Predefined dialog box class (by default)
    append( vTplData, title );
}

void BriefMsgBox_impl::addButton( int id, const std::wstring& wtext )
{
    RECT r;
    r.left = 1; // позже будет пересчитана
    r.top = TOTAL_H -BUTTON_H
            -SPACE_XY
            -pix2diaY( GetSystemMetrics(SM_CYEDGE) )
            ;
    r.right = r.left + BUTTON_W;
    r.bottom = r.top + BUTTON_H;
    size_t pos = addItem(r, id, wtext.c_str(), WORD(0x0080)/*Button*/, BS_PUSHBUTTON | BS_CENTER );
    btTplPosition.push_back(pos);
}

void BriefMsgBox_impl::addChkbox( int id, wchar_t* text )
{
    RECT r;
    r.left = SPACE_XY;
    r.top = TOTAL_H -BUTTON_H
        -SPACE_XY
        -BUTTON_H
        -SPACE_XY
        -pix2diaY( GetSystemMetrics(SM_CYEDGE) )
        ;
    r.right = r.left + MEMBOX_W;
    r.bottom = r.top + MEMBOX_H;
    addItem(r, id, text, WORD(0x0080)/*Button*/, BS_AUTOCHECKBOX );
}

void BriefMsgBox_impl::addStatic( int id, const wchar_t* text )
{
    RECT r;
    r.left = SPACE_XY;
    r.right = r.left + TOTAL_W - pix2diaX( 2*GetSystemMetrics(SM_CXEDGE) ) - 2*SPACE_XY;
    r.top = SPACE_XY;
    r.bottom = TOTAL_H - pix2diaY( GetSystemMetrics(SM_CYEDGE) )
                 - SPACE_XY
                 - BUTTON_H
                 - SPACE_XY
                 ;
    // отцентровать надпись по вертикали и горизонтали
    {
        HDC screen = ::GetDC(NULL);
        RECT around;
        around.left = 0;
        around.top  = 0;
        around.right   = dia2pixX( r.right - r.left );
        around.bottom  = dia2pixY( r.bottom - r.top );
        DrawTextEx( screen, (LPWSTR)text, -1, &around, DT_CALCRECT|DT_WORDBREAK, NULL );
        around.right   = pix2diaX( around.right );
        around.bottom  = pix2diaY( around.bottom );
        if ( around.right  <= r.right  - r.left &&
             around.bottom <= r.bottom - r.top
            )
        {
//             // центровка по горизонтали не нужна
//             int dx = (r.right  - r.left - around.right  ) / 2;
//             r.left += dx;
//             r.right -= dx;
            int dy = (r.bottom - r.top  - around.bottom ) / 2;
            r.top  += dy;
            r.bottom -= dy;
        }
		::ReleaseDC(NULL, screen);
    }
    addItem(r, id, text, WORD(0x0082)/*static*/, SS_CENTER );
}

int BriefMsgBox_impl::pix2diaX(int pixX) { return MulDiv( pixX, 4, baseunit_X ); }
int BriefMsgBox_impl::pix2diaY(int pixY) { return MulDiv( pixY, 8, baseunit_Y ); }
int BriefMsgBox_impl::dia2pixX(int diaX) { return MulDiv( diaX, baseunit_X, 4 ); }
int BriefMsgBox_impl::dia2pixY(int diaY) { return MulDiv( diaY, baseunit_Y, 8 ); }

void BriefMsgBox_impl::addIcon( int x, int y, int cx, int cy, int id )
{
    RECT r;
    r.left = x;
    r.top = y;
    r.right = r.left + cx;
    r.bottom = r.top + cy;
    addItem(r, id, L"", WORD(0x0082)/*static*/, SS_ICON );
}

size_t BriefMsgBox_impl::addItem( const RECT& r, int id, const wchar_t* text, WORD control_class, DWORD style )
{
#define ALIGNER(vec,to)           while( vec.size()%sizeof(to) ) vec.push_back( 0 );

    ALIGNER( vTplData, DWORD ); // Align DLGITEMTEMPLATE on DWORD boundary
    size_t res=vTplData.size();
    DLGITEMTEMPLATE dit;
    ZeroMemory(&dit,sizeof(dit));
    dit.x  = static_cast<short>(r.left);
    dit.y  = static_cast<short>(r.top);
    dit.cx = static_cast<short>(r.right - r.left);
    dit.cy = static_cast<short>(r.bottom - r.top);
    dit.id = static_cast<WORD>(id);       // control identifier
    dit.style = (style | WS_VISIBLE | WS_CHILD);
    append( vTplData, dit );
    append( vTplData, WORD(0xFFFF) ); // стандартный контрол
    append( vTplData, control_class );
    append( vTplData, text );
    ALIGNER( vTplData, WORD );  // Align on WORD boundary
    append( vTplData, WORD(0) );// No creation data

    reinterpret_cast<DLGTEMPLATE*>(&vTplData.front())->cdit ++;

#undef ALIGNER
    return res;
}

HGLOBAL BriefMsgBox_impl::getGlobalHandle()
{
    HGLOBAL hgbl = GlobalAlloc(GMEM_ZEROINIT, vTplData.size());
    if (!hgbl)
        throw std::bad_alloc();
    if ( char * pgbl = (char*)GlobalLock(hgbl) )
    {
        memcpy( pgbl, &vTplData.front(), vTplData.size() );
        GlobalUnlock(hgbl); 
    }
    else
    {
        GlobalFree(hgbl);
        throw std::bad_alloc();
    }
    return hgbl;
}

void BriefMsgBox_impl::placeNeedButton( UINT _uType )
{
    switch(MB_TYPEMASK & _uType)
    {
    case MB_OKCANCEL:
        addButton( IDOK,     trx(L"OK"));
        addButton( IDCANCEL, trx(L"Отмена"));
        break;
    case MB_YESNOCANCEL:
        addButton( IDYES,    trx(L"Да"));
        addButton( IDNO,     trx(L"Нет"));
        addButton( IDCANCEL, trx(L"Отмена"));
        break;
    case MB_YESNO:
        addButton( IDYES,    trx(L"Да"));
        addButton( IDNO,     trx(L"Нет"));
        break;
    case MB_ABORTRETRYIGNORE:
        addButton( IDABORT,  trx(L"Стоп"));
        addButton( IDRETRY,  trx(L"Повтор"));
        addButton( IDIGNORE, trx(L"Пропуск"));
        break;
    case MB_RETRYCANCEL:
        addButton( IDRETRY,  trx(L"Повтор"));
        addButton( IDCANCEL, trx(L"Отмена"));
        break;
    default:
    case MB_OK:
        addButton( IDOK, trx(L"OK"));
        break;
    }
    buttonAdjust();
}

int BriefMsgBox_impl::calcExStyle( UINT _uType )
{
    int exstyle=0;
    if ( _uType & MB_SYSTEMMODAL || (_uType & MB_TOPMOST) )
        exstyle |= WS_EX_TOPMOST;
    if ( _uType & MB_HELP )
        exstyle |= WS_EX_CONTEXTHELP;
    return exstyle;
}
BOOL BriefMsgBox_impl::OnInitDialog()
{
    CDialog::OnInitDialog();

    if ( uType & MB_SETFOREGROUND )
        SetForegroundWindow();
    setIcon(uType);
    if ( wait != 0 )
    {
        time( &finish );
        finish += wait;
        timerId = SetTimer( 123, 100, NULL );
        wchar_t buff[100];
        GetDlgItemText(bTimer,buff,size_array(buff));
        timeStr = buff;
    }
    SetDefaultButton();
    initMemoBox();
    NearToMouse(this);
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void BriefMsgBox_impl::SetDefaultButton( )
{
    unsigned currdef = GetDefID();
    if ( HIWORD( currdef ) == DC_HASDEFID )
    {
        UINT oldDef = LOWORD( currdef );
        if ( oldDef == bDefault )
        {
            CWnd* hnew = GetDlgItem( bDefault );
            SendMessage( WM_NEXTDLGCTL, bDefault, TRUE );
            hnew->SetFocus();
            return ;
        }
        else
        {
            CWnd* hOld = GetDlgItem( oldDef );
            if( NULL!=hOld )
                hOld->SendMessage( BM_SETSTYLE, BS_PUSHBUTTON, ( LONG ) TRUE );

            CWnd* hnew = GetDlgItem( bDefault );
            if ( NULL!=hnew )
            {
                hnew->SendMessage( BM_SETSTYLE, BS_DEFPUSHBUTTON, ( LONG ) TRUE );
                SetDefID( bDefault );
                hnew->SetFocus();
            }
            else
                SetDefID( bDefault );
        }
    }
    else
    {
        CWnd* hnew = GetDlgItem( bDefault );
        SetDefID( bDefault );
        SendDlgItemMessage( bDefault, BM_SETSTYLE, BS_DEFPUSHBUTTON, ( LONG ) TRUE );
        SendMessage( WM_NEXTDLGCTL, bDefault, true );
        hnew->SetFocus();
    }
}


void BriefMsgBox_impl::setIcon( UINT _uType )
{
    LPWSTR id = NULL;
    if ( MB_ICONMASK & _uType )
    {
        switch ( _uType & MB_ICONMASK )
        {
        case MB_ICONHAND:
            id = IDI_HAND;
            break;
        case MB_ICONQUESTION:
            id = IDI_QUESTION;
            break;
        case MB_ICONEXCLAMATION:
            id = IDI_EXCLAMATION;
            break;
        case MB_ICONASTERISK:
            id = IDI_ASTERISK;
            break;
        default:
            return ;
        }
        MessageBeep( _uType & MB_ICONMASK );
    }

    if ( id )
        stIcon.SetIcon((HICON)id);
}

void BriefMsgBox_impl::buttonAdjust()
{
    bDefault = getDefButID();
    if ( wait )
    {
        if ( bTimer )
            bTimer = getTimerButID();
        if ( 0==bTimer)
            bTimer = bDefault;
    }
    else
        bTimer = 0;

    int between = static_cast<int>((TOTAL_W - btTplPosition.size() * BUTTON_W)/(btTplPosition.size()+1));
    int back_end=0;
    BOOST_FOREACH(size_t& pos, btTplPosition)
    {
        DLGITEMTEMPLATE& dit = *reinterpret_cast<DLGITEMTEMPLATE*>(&(vTplData[pos]));
        dit.x  = static_cast<short>(back_end+between); 
        dit.cx = BUTTON_W;
        back_end = dit.x + dit.cx;
    }
}

void BriefMsgBox_impl::OnTimer(UINT_PTR nIDEvent)
{
    CDialog::OnTimer(nIDEvent);

    time_t ct;
    time( &ct );
    if ( ct >= finish )
    {
        pRemember = 0;
        EndDialog( bTimer );
    }
    else if ( bTimer )
    {
        CWnd* wTimer = GetDlgItem( bTimer );
        if ( NULL!=wTimer )
        {
            wstring ws= timeStr+L" ("+to_wstring(int(finish - ct))+L")";
            wTimer->SetWindowText( ws.c_str() );
        }
    }
}

int BriefMsgBox_impl::getDefButID()
{
    size_t defOrd = 0;
    switch(MB_DEFMASK & uType)
    {
    default:
    case MB_DEFBUTTON1:
        defOrd = 0;
        break;
    case MB_DEFBUTTON2:
        defOrd = 1;
        break;
    case MB_DEFBUTTON3:
        defOrd = 2;
        break;
    case MB_DEFBUTTON4:
        defOrd = 3;
        break;
    }
    if ( btTplPosition.size()<=defOrd )
        defOrd = 0;

    DLGITEMTEMPLATE& dit = *reinterpret_cast<DLGITEMTEMPLATE*>( &( vTplData[ btTplPosition[defOrd] ] ) );
    return dit.id;
}

int BriefMsgBox_impl::getTimerButID()
{
    // return founded
    BOOST_FOREACH(size_t& pos, btTplPosition)
    {
        DLGITEMTEMPLATE& dit = *reinterpret_cast<DLGITEMTEMPLATE*>(&(vTplData[pos]));
        if ( bTimer==dit.id )
            return bTimer;
    }
    // if not found
    return 0;
}

void BriefMsgBox_impl::initMemoBox()
{
    stMemo.SetCheck(BST_UNCHECKED);
    if ( pRemember )
    {
        CRect c;
        CRect m;
        GetClientRect(&c);

        stMemo.GetWindowRect(m);
        ScreenToClient(&m);
        m.MoveToX(( c.Width() - m.Width() )/2 );
        stMemo.MoveWindow(&m);
        stMemo.ShowWindow(SW_SHOW);
    }
    else
        stMemo.ShowWindow(SW_HIDE);
}

void BriefMsgBox_impl::setRemember(bool* remember)
{
    pRemember = remember;
    *pRemember = false;
}
