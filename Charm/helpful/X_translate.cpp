#include "stdafx.h"
#include "X_translate.h"

#ifndef LINUX
#include <pshpack8.h>
#endif // LINUX
#include <boost/locale.hpp>
#ifndef LINUX
#include <poppack.h>
#endif // LINUX

#include <vector>

#include <boost/filesystem.hpp>
#include "boost/algorithm/string/replace.hpp"

#if _MFC_VER
#include "MFCExt.h"
#endif

#include "Utf8.h"

namespace bl = boost::locale;
namespace blg = bl::gnu_gettext;
namespace bfs = boost::filesystem;

//TODO: вынести в другое место чтение и запись в реестр значение текущего языка
#define HKEY_ROOT HKEY_CURRENT_USER
const std::wstring language_path = L"Software\\Charm\\Actor\\Settings";
const std::wstring language_key = L"Language";


std::string ReadLanguage() {
#if _MFC_VER
	return ToUtf8(mfcext::read_reg(HKEY_ROOT, language_path, language_key, FromUtf8(default_lng)));
#else
	return default_lng;
#endif
}

bool WriteLanguage(const std::string& 
#if _MFC_VER
	lg
#endif
) {
#if _MFC_VER
	return mfcext::write_reg(HKEY_ROOT, language_path, language_key, FromUtf8(lg));
#else
	return true;
#endif
}

static std::vector<CLocalize*>* localizeObserver;

static std::unique_ptr<std::locale> current_locale;
boost::locale::generator _generator;
boost::locale::gnu_gettext::messages_info _messageInfo;
std::string poligon_domain;
std::vector<std::wstring> available_lng;

static std::vector<char> MO_file_loader( const std::string& file_name, const std::string& /*encoding*/ ) 
{
	std::vector<char> res;
#ifndef LINUX
	std::string actual_file = boost::replace_all_copy(file_name, "//", "/");

	HANDLE h = CreateFileA(actual_file.c_str(),
		FILE_READ_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h != INVALID_HANDLE_VALUE)
	{
		unsigned long szRead = GetFileSize(h, NULL);
		res.resize(szRead + 1);
		unsigned long szReaded = 0;
		if (ReadFile(h, &res.front(), szRead, &szReaded, NULL) && szReaded == szRead)
		{
			res[szRead] = 0;
		}
		else
		{
			res.clear();
		}
		CloseHandle(h);
	}

#endif // !LINUX
	return res;
}

void trx_add_domain(const std::string& domain, bool default_domain) 
{
	blg::messages_info::domain dmn(domain);
	_messageInfo.domains.push_back(dmn);
	_generator.add_messages_domain(domain);
	if (default_domain) 
	{
		_generator.set_default_messages_domain(domain);
	}
}

void trx_add_poligon_domain(const std::string& domain)
{
	poligon_domain = domain;
	trx_add_domain(domain);
}

void trx_add_path(const std::wstring& directories_path) 
{
    boost::filesystem::path dir(directories_path);
	_messageInfo.paths.push_back( dir.string() );
}

// выбор текущего языка
void trx_select( const std::string& lang)
{
	
	std::string language = lang.empty() ? ReadLanguage() : lang;
	std::string current_lng = _messageInfo.language;

	_messageInfo.encoding = "UTF-8";
	_messageInfo.language = language;
	_messageInfo.locale_category = "";
	_messageInfo.callback = MO_file_loader;

	current_locale.reset( new std::locale( _generator(""), blg::create_messages_facet<wchar_t>(_messageInfo) ) );

	if (lang != ReadLanguage()) 
	{
		WriteLanguage(language);
	}
	if (current_lng != language && localizeObserver != nullptr) 
	{

		//TODO make safe call
		for (auto item: *localizeObserver)
		{
			item->OnLocalize();
		}
	}
}

std::string trx_language() {
	if ( !current_locale )
		return "";
	return ReadLanguage();
}

void trx_clear_domains() 
{
	_messageInfo.domains.clear();
	_generator.clear_domains();
}

void trx_clear_path() 
{
	_messageInfo.paths.clear();
}


// перевод строки
std::wstring trx(const std::wstring& id)
{
    ASSERT( !id.empty() );
	if ( !current_locale )
		return id;
	else
		return bl::gettext( id.c_str(), *current_locale );
}

// перевод строки с множественной формой
std::wstring trx(const std::wstring& s, const std::wstring& p, int n)
{
    ASSERT( !s.empty() && !p.empty() );
	if ( !current_locale )
		return bl::ngettext( s.c_str(), p.c_str(), n );
	else
		return bl::ngettext( s.c_str(), p.c_str(), n, *current_locale );
}

// контекстный перевод строки
std::wstring trx(const std::wstring& context, const std::wstring& id)
{
    ASSERT( !context.empty() );
	if ( !current_locale || id.empty() )
		return id;
	else
		return bl::pgettext( context.c_str(), id.c_str(), *current_locale );
}

// контекстный перевод строки с множественной формой
std::wstring trx(const std::wstring& context, const std::wstring& s, const std::wstring& p, int n)
{
    ASSERT( !context.empty() && !s.empty() && !p.empty() );
	if ( !current_locale )
		return bl::npgettext( context.c_str(), s.c_str(), p.c_str(), n );
	else
		return bl::npgettext( context.c_str(), s.c_str(), p.c_str(), n, *current_locale );
}

std::wstring dtrx(const std::string& domain, const std::wstring& id) {
    ASSERT( !id.empty() );
	if (!current_locale)
		return id;
	else 
		return bl::dgettext(domain.c_str(), id.c_str(), *current_locale);
}

std::wstring dtrx(const std::string& domain, const std::wstring& context, const std::wstring& id) {
    ASSERT( !domain.empty() && !context.empty() && !id.empty() );
	if (!current_locale)
		return id;
	else 
		return bl::dpgettext(domain.c_str(), context.c_str(), id.c_str(), *current_locale);
}

std::wstring dtrx(const std::string& domain, const std::wstring& singleId, const std::wstring& pluralId, int n) {
    ASSERT( !domain.empty() && !singleId.empty() && !pluralId.empty() );
	if (!current_locale)
		return singleId;
	else 
		return bl::dngettext(domain.c_str(), singleId.c_str(), pluralId.c_str(), n, *current_locale);
}

std::wstring dtrx(const std::string& domain, const std::wstring& context, const std::wstring& singleId, const std::wstring& pluralId, int n) {
    ASSERT( !domain.empty() && !context.empty() && !singleId.empty() && !pluralId.empty() );
	if (!current_locale)
		return singleId;
	else 
		return bl::dnpgettext(domain.c_str(), context.c_str(), singleId.c_str(), pluralId.c_str(), n, *current_locale);
}

std::wstring trx_p(const std::wstring& wstr)
{
	return dtrx(poligon_domain, wstr);
}

std::wstring trx_p(const std::wstring& context, const std::wstring& wstr)
{
 	return dtrx(poligon_domain, context, wstr);
}

std::wstring trx_p(const std::wstring& s, const std::wstring& p, int n)
{
	return dtrx(poligon_domain, s ,p, n);
}

std::wstring trx_p(const std::wstring& context, const std::wstring& s, const std::wstring& p, int n)
{
	return dtrx(poligon_domain, context, s, p, n);
}

static bool has_mo_file(bfs::directory_iterator itr_dir) 
{
	if (bfs::is_directory(itr_dir->status()))
	{
		bfs::directory_iterator end_itr;
		std::string dir = itr_dir->path().string();
		for (bfs::directory_iterator itr_f(dir); itr_f != end_itr; ++itr_f)
		{
			if (bfs::is_regular_file(itr_f->status()))
			{
				std::string em =  itr_f->path().leaf().string();
				for (auto domain: _messageInfo.domains)
				{
					if (em == domain.name + ".mo")
					{
						return true;
					}
				}
			}
		}
	} 
	return false;
}

std::wstring short_to_long(std::string lng)
{
	if (lng == "lv" || lng == "LV")
        return std::wstring(L"Latvie") + wchar_t(0x0161) + L"u"; // universal chararacter name
	else
        return L"Русский";
}

std::vector<std::wstring> trx_get_available_language()
{
	std::vector<std::wstring> localizations;
	localizations.push_back(short_to_long(default_lng));
	for (auto dir: _messageInfo.paths) 
	{
		if (bfs::exists(dir)) 
		{
			bfs::directory_iterator end_itr;
			for (bfs::directory_iterator itr_d(dir); itr_d != end_itr; ++itr_d)
			{
				if (has_mo_file(itr_d))
				{
					std::wstring name = short_to_long(itr_d->path().filename().string());
					if (std::find(localizations.begin(), localizations.end(), name) == localizations.end()) 
					{
						localizations.push_back(name);
					}
				}
			}
		}
	}
	return localizations;
}

std::vector<std::wstring> trx_get_language()
{
	if (!available_lng.empty()) 
	{
		return available_lng;
	}
	else 
	{
		return trx_get_available_language();
	}
}

void trx_add_available(std::string lng)
{

	if (!lng.empty() && lng != "any") 
	{
		std::wstring long_lng = short_to_long(lng);
		std::vector<std::wstring> vector_lng = trx_get_available_language();
		if (std::find(vector_lng.begin(), vector_lng.end(), long_lng) != vector_lng.end() &&
			std::find(available_lng.begin(), available_lng.end(), long_lng) == available_lng.end())
		{
			available_lng.push_back(long_lng);
		}
	}
	else if (lng == "any") 
	{
		available_lng = trx_get_available_language();
	}
}

#ifdef _MFC_VER

void WrapHotKey(const CString text, std::wstring& menu, std::wstring& hotKey) 
{
	menu = std::wstring(text);
	std::size_t pos = menu.find('\t');
	if (pos != std::wstring::npos) 
	{
		hotKey = menu.substr(pos, menu.length());
		menu = menu.substr(0, pos);
	}
}

static void EnumChildWnd(CWnd* pDlg, std::wstring context)
{
	for(CWnd* pWnd = pDlg->GetWindow(GW_CHILD); pWnd!=NULL; pWnd = pWnd->GetWindow(GW_HWNDNEXT))
	{
		CString text;
		pWnd->GetWindowText(text);
        if ( !context.empty() && !std::wstring(text).empty() )
        {
            pWnd->SetWindowText(trx(context, std::wstring(text)).c_str());
        }
        else if ( !text.IsEmpty() )
        if ( !std::wstring(text).empty() )
        {
            pWnd->SetWindowText(trx(std::wstring(text)).c_str());
        }
		EnumChildWnd(pWnd, context);
	}
}

void trx(CWnd* dlg)
{
	if (dlg) {
		CString dlgTitle;
		dlg->GetWindowText(dlgTitle);
		std::wstring dlgContext(dlgTitle);
        if ( !dlgContext.empty() )
            dlg->SetWindowText(trx(dlgContext, dlgContext).c_str());
		EnumChildWnd(dlg, dlgContext);
	}
}

void trx(std::wstring context, CMenu* pMenu) {
	if (pMenu) 
	{
		for( int i=0; i<pMenu->GetMenuItemCount(); i++ )
		{
			CString text;
			pMenu->GetMenuString(i,text,MF_BYPOSITION);
			std::wstring hotKey;
			std::wstring menu;
			WrapHotKey(text, menu, hotKey);
			if (!menu.empty()) 
			{
				MENUITEMINFO info;
				info.cbSize = sizeof(MENUITEMINFO);
				info.fMask = MIIM_ID;
				VERIFY(pMenu->GetMenuItemInfo(i, &info, TRUE));
				pMenu-> ModifyMenu(info.wID, MF_BYCOMMAND | MF_STRING, info.wID ,(trx(context, menu)+hotKey).c_str());
			}
			trx(context + L"|" + std::wstring(menu), pMenu->GetSubMenu(i));
		}
	}
}

void trx(CMenu* pMenu)
{
	trx(L"MENU", pMenu);
}

void trx(CPropertySheet* pPropertySheet) 
{ 
	CString dlgTitle;
	pPropertySheet->GetWindowText(dlgTitle);
	pPropertySheet->SetWindowText(trx(std::wstring(dlgTitle)).c_str());
	SetWindowText(*pPropertySheet->GetDlgItem(IDOK), trx(L"OK").c_str());
	SetWindowText(*pPropertySheet->GetDlgItem(IDCANCEL), trx(L"Закрыть").c_str());
	SetWindowText(*pPropertySheet->GetDlgItem(ID_APPLY_NOW), trx(L"Применить").c_str());
	CTabCtrl* tabCtrl = pPropertySheet->GetTabControl();
	if (tabCtrl != NULL) 
	{
		for(int index = 0; index < pPropertySheet->GetPageCount(); index ++)
		{
			std::wstring title = mfcext::get_tab_text(tabCtrl, index);
			mfcext::set_tab_text(tabCtrl, index, trx(title, title));
		}
	}
}

#endif

CLocalize::CLocalize() 
{
	if (localizeObserver == NULL)
		localizeObserver = new std::vector<CLocalize *>(0);
	if ( std::find(localizeObserver->begin(), localizeObserver->end(), this) == localizeObserver->end() ) 
	{
		localizeObserver->push_back(this);
	}
}

#ifdef _MFC_VER

typedef std::pair<const HWND, const std::wstring> TCache;

void CLocalize::EnumChildWnd(const std::wstring& context, const CWnd* parent) {
	for(CWnd* pWnd = parent->GetWindow(GW_CHILD); pWnd!=NULL; pWnd = pWnd->GetWindow(GW_HWNDNEXT))
	{
		HWND hWnd = pWnd->GetSafeHwnd();
		auto value = translateCache.find(hWnd);
		std::wstring text;
		if (value == translateCache.end()) {
			CString cache;
			pWnd->GetWindowText(cache);
            std::wstring wcache(cache);
            if ( !wcache.empty() ) {
                translateCache.insert(TCache(hWnd, wcache));
                text = ::trx(context, wcache);
            }
		} else {
			text = ::trx(context, value->second);
		}
		if (!text.empty()) {
			pWnd->SetWindowText(text.c_str());
		}
	}
}

void CLocalize::trx(CWnd* pWnd) {
	if (pWnd && IsWindow(pWnd->GetSafeHwnd())) {
		std::wstring context;
		HWND hWnd = pWnd->GetSafeHwnd();
		auto value = translateCache.find(hWnd);
		if (value != translateCache.end()) {
			context = value->second;
		} else {
			CString caption;
			pWnd->GetWindowText(caption);
			context = caption;
			translateCache.insert(TCache(hWnd, context));
		}
		pWnd->SetWindowText(::trx(context, context).c_str());	
		EnumChildWnd(context, pWnd);
	}
}
#endif

CLocalize::~CLocalize()
{

#ifdef _MFC_VER
	translateCache.clear();
#endif

    auto it = std::find(localizeObserver->begin(), localizeObserver->end(), this);
	if ( localizeObserver->end() != it )
        localizeObserver->erase( it );
	if (localizeObserver->size() == 0)
		delete localizeObserver;
}