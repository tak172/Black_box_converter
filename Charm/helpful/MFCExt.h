#pragma once

#include <WinDef.h>
#include <string>
#include <afxcmn.h>

#ifndef _MFCEXT_H_
#define _MFCEXT_H_

namespace mfcext {
	std::wstring read_reg(const HKEY parent, const std::wstring& path, const std::wstring& key, const std::wstring& default_v);
	bool write_reg(const HKEY parent, const std::wstring& path, const std::wstring& key, std::wstring value);
	void set_header_text(CListCtrl* lctrl, const int index, std::wstring& text);
	std::wstring get_tab_text(const CTabCtrl* ptbctrl, const int index);
	void set_tab_text(CTabCtrl* ptbctrl, const int index, std::wstring text);
}
#endif