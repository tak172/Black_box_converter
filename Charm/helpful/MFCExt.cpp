#include "stdafx.h"
#include "MFCExt.h"
#include <atlbase.h>

const int BUFFER_SIZE = 256;

namespace mfcext {
	std::wstring read_reg(const HKEY parent, const std::wstring& path, const std::wstring& key, const std::wstring& default_v)
	{
		std::wstring res = default_v;
		CRegKey reg_key;
		LONG result = reg_key.Open(parent, path.c_str());
		if (result == ERROR_SUCCESS) {
			wchar_t szBuffer[BUFFER_SIZE];
			ULONG cchBuffer = BUFFER_SIZE;
			result = reg_key.QueryStringValue(key.c_str(), szBuffer, &cchBuffer);
			if (result == ERROR_SUCCESS) {
				std::wstring value(szBuffer);
				if (!value.empty()) 
				{
					res = value;
				}
			}
			reg_key.Close();
		}
		return res;
	}

	bool write_reg(const HKEY parent, const std::wstring& path, const std::wstring& key, std::wstring value)
	{
		CRegKey reg_key;
		LONG result = reg_key.Open(parent, path.c_str());
		if (result != ERROR_SUCCESS) {
			result = reg_key.Create(HKEY_CURRENT_USER, path.c_str());
			if (result != ERROR_SUCCESS) {
				reg_key.Close();
				return false;
			}	
		}	
		result = reg_key.SetStringValue(key.c_str(), value.c_str());
		if (result != ERROR_SUCCESS) {
			reg_key.Close();
			return false;
		}
		reg_key.Close();
		return true;
	}

	void set_header_text(CListCtrl* plctrl,  const int index, std::wstring& text) {
		if (plctrl && IsWindow(plctrl->GetSafeHwnd())) {
			LVCOLUMN lvCol;
			::ZeroMemory((void *)&lvCol, sizeof(LVCOLUMN));
			wchar_t buffer[BUFFER_SIZE] = {0};
			wcscpy_s(buffer, BUFFER_SIZE, text.c_str());
			lvCol.pszText = buffer;
			lvCol.cchTextMax = BUFFER_SIZE;
			lvCol.mask=LVCF_TEXT;
			plctrl->SetColumn(index, &lvCol);
			plctrl->SetColumnWidth(index,LVSCW_AUTOSIZE_USEHEADER);
		}
	}

	std::wstring get_tab_text(const CTabCtrl* ptbctrl, const int index)
	{
		TCITEM tcItem;
		wchar_t buffer[BUFFER_SIZE] = {0};
		tcItem.pszText = buffer;
		tcItem.cchTextMax = BUFFER_SIZE;
		tcItem.mask = TCIF_TEXT;
		ptbctrl->GetItem(index, &tcItem);
		return std::wstring(tcItem.pszText);
	}

	void set_tab_text(CTabCtrl* ptbctrl, const int index, std::wstring text)
	{
		wchar_t buffer[BUFFER_SIZE] = {0};
		wcscpy_s(buffer, BUFFER_SIZE, text.c_str());
		TCITEM tcItem;
		tcItem.pszText = buffer;
		tcItem.cchTextMax = BUFFER_SIZE;
		tcItem.mask = TCIF_TEXT;
		ptbctrl->SetItem(index, &tcItem);
	}

}