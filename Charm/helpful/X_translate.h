#pragma once
#include <string>
#include <vector>
#include <map>

#ifdef _MFC_VER
#include <afxcmn.h>
#endif



#ifndef _X_TRANSLATE_H_
#define _X_TRANSLATE_H_

// макрос для пометки строк, подлежащих переводу
// используется тогда, когда вызов функции невозможен
#define trx_noop(context,x)     (x)

//язык по умолчанию
const std::string default_lng = "ru";

void trx_add_domain(const std::string& domain, bool default_domain = false);
void trx_add_poligon_domain(const std::string& domain);
void trx_add_path(const std::wstring& directories_path);
void trx_select(const std::string& lang = "" );
//ищет доступные доступные языки перевода
std::vector<std::wstring> trx_get_language();
//возвращает текущий язык в короткой записи
std::string trx_language();
//преобразует короткую запись языка в длинную
std::wstring short_to_long(std::string lng);
//добавляет язык в список доступных языков для выбора из меню
void trx_add_available(std::string lng);

void trx_clear_domains();
void trx_clear_path();

//поиск перевода строки в текущий язык интерфейса
std::wstring trx(const std::wstring& wstr);

//поиск перевода строки согласно контексту в текущий язык
std::wstring trx(const std::wstring& context, const std::wstring& wstr);

//поиск перевода множественной строки согласно значению n. Возвращается только перевод
std::wstring trx(const std::wstring& s, const std::wstring& p, int n);

//поиск перевода множественной строки согласно контексту и значению n. Возвращается только перевод
std::wstring trx(const std::wstring& context, const std::wstring& s, const std::wstring& p, int n);

//поиск перевода строки из указанного словаря 
std::wstring dtrx(const std::string& domain, const std::wstring& id);

//поиск перевода строки из указанного словаря с учетом контекста
std::wstring dtrx(const std::string& domain, const std::wstring& context, const std::wstring& id);

//поиск перевода множественной строки из указанного словаря множественного
std::wstring dtrx(const std::string& domain, const std::wstring& singleId, const std::wstring& pluralId, int n);

//поиск перевода множественной строки из указанного словаря с учетом контекста
std::wstring dtrx(const std::string& domain, const std::wstring& context, const std::wstring& singleId, const std::wstring& pluralId, int n);

//поиск перевода строки в текущий язык интерфейса
std::wstring trx_p(const std::wstring& wstr);

//поиск перевода строки полигона согласно контексту в текущий язык
std::wstring trx_p(const std::wstring& context, const std::wstring& wstr);

//поиск перевода множественной строки полигона согласно значению n. Возвращается только перевод
std::wstring trx_p(const std::wstring& s, const std::wstring& p, int n);

//поиск перевода множественной строки полигона согласно контексту и значению n. Возвращается только перевод
std::wstring trx_p(const std::wstring& context, const std::wstring& s, const std::wstring& p, int n);

class CLocalize {
public:

	CLocalize();
	~CLocalize();

	virtual void OnLocalize() = 0;

#ifdef _MFC_VER
protected:
	void trx(CWnd* pWnd);
	void SetHeaderText(CListCtrl* lctrl, const int index, std::wstring& text);

private: 
	std::map<const HWND, const std::wstring> translateCache;
	void EnumChildWnd(const std::wstring& context, const CWnd* parent);
#endif
};

#ifdef _MFC_VER 

// Перевод сразу всего диалога (заголовок и контролы)
//
// Например:
//  CWnd* pDlg=CWnd::FromHandlePermanent(pMsg->hwnd);
//  trx(pDlg);
// 
void trx(CWnd* pDlg);

// Перевод всех пунктов меню
//
// Например:
//  CMenu* pMenu=CMenu::FromHandlePermanent((HMENU)pMsg->wParam);
//  trx(pMenu);
//
void trx(CMenu* pMenu);

// Перевод окна страницы свойств (заголовки вкладок и кнопки)
//
void trx(CPropertySheet* pPropertySheet);
#endif

#endif