#pragma once

#ifndef LINUX
#else
struct _EXCEPTION_POINTERS;
#endif
//Класс для непосредственного создания дампа процесса.
//Предназначен для мгновенного использования (нельзя сохранять, да и не имеет смысла).
class CProcessDump
{
public:
    CProcessDump( _EXCEPTION_POINTERS * pExPointers, bool detailed );

    static std::pair<size_t,bool> RestrictSize( unsigned totalSize_Mb = 10 * 1024 ); // ограничить общий размер дампов (в мегабайтах)
    static std::pair<size_t,bool> RestrictCount( unsigned down_count, unsigned up_count ); // ограничить количество дампов
    static std::wstring ModeAndVersion();
    static std::wstring DumpExtension() // общее расширение файлов дампа
    {
        return L".dmp";
    }
    static void SetDetailizedOnly( bool det ); // создавать только детализированные, недетализированные игнорировать

private:
    bool ExistPhantom() const;
    void MakeLocalDump( bool detailed, _EXCEPTION_POINTERS * pExPointers );
    bool MakeRemoteDump( bool detailed, _EXCEPTION_POINTERS * pExPointers );

    int WriteLocal( HANDLE hFile, bool detailed, _EXCEPTION_POINTERS * pExPointers ) const;
};
