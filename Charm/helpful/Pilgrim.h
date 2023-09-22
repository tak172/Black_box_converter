#pragma once

#include <boost/utility/string_ref.hpp>
#include "../helpful/RT_Singleton.h"

class Pilgrim
{
    DECLARE_SINGLETON(Pilgrim);
public:
    std::wstring AddHomePath(const std::wstring& wstr) const;
    std::wstring AddEmfPath(const std::wstring& wstr) const;
    std::wstring AddSoundPath(const std::wstring& wstr) const;
    std::wstring AddExePath(const std::wstring& wstr) const;
    std::wstring ExePath() const;
    std::wstring StartupPath() const;
	std::wstring LastOpenPath() const;
	std::wstring BvsFilePath() const;
	std::wstring BvsPath() const;
    std::wstring ExeFileName() const;
    void Set_HomeFile( const std::wstring& wstr );
    void Set_EmfPath( const std::wstring& wstr );
    void Set_EmfFile( const std::wstring& wstr );
    void Set_SoundPath( boost::wstring_ref wstr );
	void Set_LastOpenPath( const std::wstring & wstr );
	void Set_BvsFilePath( const std::wstring & wstr );
    bool haveHomePath() const;
    bool haveEmfPath() const;
#ifdef _ATL
	bool LoadRegistry( LPCTSTR app_name ); //опрос реестра на наличие предустановленных путей
	void SaveRegistry( LPCTSTR app_name ); //сохранение в реестр настроенных путей
#endif
    static std::wstring FileToPath( const std::wstring& wfile );

private:
#ifdef _ATL
	std::wstring LoadRegistryPath( LPCTSTR app_name, const std::wstring& pathname );
#endif
    std::wstring fullizePath( const wchar_t* somepath, const std::wstring& rootpath ) const;

    std::wstring Path_2_Home; // путь к проекту или к конфигурационному файлу
    std::wstring Path_2_Emf;  // Путь к основным картинкам
	std::wstring Path_2_BvsFile; // Путь к файлу bvs
	std::wstring Path_2_Bvs;	// Путь к файлам *.emf для bvs
    std::wstring Path_2_Sound;    // Путь к звуковым файлам
    std::wstring Current_Exe;     // Полное имя файла самой программы
	std::wstring Path_2_LastOpen; //Путь к последнему открытому файлу
    std::wstring Startup_CurrentDir; // текущая папка при запуске программы
};


inline std::wstring Pilgrim::ExePath() const
{
    return FileToPath( Current_Exe );
}

inline std::wstring Pilgrim::StartupPath() const
{
    return Startup_CurrentDir;
}

inline std::wstring Pilgrim::LastOpenPath() const
{
	if ( Path_2_LastOpen.empty() )
		return ExePath();
	else
		return Path_2_LastOpen;
}

inline std::wstring Pilgrim::BvsFilePath() const
{
	return Path_2_BvsFile;
}

inline std::wstring Pilgrim::BvsPath() const
{
	return Path_2_Bvs;
}
inline std::wstring Pilgrim::ExeFileName() const
{
    return Current_Exe;
}
inline void Pilgrim::Set_EmfFile( const std::wstring& wstr )
{
    Set_EmfPath(FileToPath(wstr));
}

inline bool Pilgrim::haveHomePath() const
{
    return !Path_2_Home.empty();
}

inline bool Pilgrim::haveEmfPath() const
{
    return !Path_2_Emf.empty();
}

inline Pilgrim::~Pilgrim()
{}

inline std::wstring Pilgrim::AddHomePath(const std::wstring& wpath) const
{
    return fullizePath(wpath.c_str(),Path_2_Home);
}

inline std::wstring Pilgrim::AddEmfPath(const std::wstring& wpath) const
{
    return fullizePath(wpath.c_str(),Path_2_Emf);
}

inline std::wstring Pilgrim::AddSoundPath(const std::wstring& wpath) const
{
    return fullizePath(wpath.c_str(),Path_2_Sound);
}

inline std::wstring Pilgrim::AddExePath(const std::wstring& wpath) const
{
    return fullizePath(wpath.c_str(),ExePath());
}
