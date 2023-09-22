#pragma once

#include <vector>
#include <string>
#include <boost/utility/string_ref.hpp>
#include "bbx_Requirements.h"
#include "../helpful/linux_defs.h"

namespace Bbx
{
    class Stamp;
    class FileChain;

    class Location
    {
    public:
        Location()
            : directory(), prefix(), suffix(), mask_only()
        { };
        Location(const std::wstring& folder, const std::wstring& pref, const std::wstring& suff);
        static std::wstring defaultFolder();
        std::wstring fileName(const Stamp& stamp, unsigned attempt) const;
        std::wstring filePath(const Stamp& stamp, unsigned attempt) const;
        std::wstring verificationFilePath() const;
        bool empty() const;

        // доступ к набору файлов
        std::shared_ptr<const FileChain> getCPtrChain() const; // для просмотра

        bool operator ==(const Location& other) const;
        const std::wstring& getFolder() const { return directory; }
		const std::wstring& getMask() const { return mask_only; }
        static void clearFolderCache();

        struct PathComparator // компаратор имен файлов черного ящика
        {
            bool operator() ( boost::wstring_ref one, boost::wstring_ref two ) const;
        };

    private:
        std::wstring directory;
        std::wstring prefix;
        std::wstring suffix;
		std::wstring mask_only; // предпостроенная маска поиска (только имя файла)
        static wchar_t markTZ() { return L'Z'; }
        static bool markTZ( wchar_t sym ) { return L'Z' == sym || L'z' == sym; }
    };
}
