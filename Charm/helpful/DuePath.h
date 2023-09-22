#pragma once
#ifndef _DUEPATH_H_
#define _DUEPATH_H_
#include <vector>
#include <string>
// построение "соответствующего" полного имени файла
// myExe - полный путь к текущему исполняемому модулю
// x64 - текущий модуль 64-разрядный
// request - искомый модуль
// 
// result:
//      возможные варианты полного пути искомого модуля в порядке предпочтения
std::vector<std::wstring> DuePath(const std::wstring& myExe, bool x64, const std::wstring& request);

// вернуть первый существующий файл
std::wstring DuePath_firstExisted( const std::vector<std::wstring>& dirt );

#endif
