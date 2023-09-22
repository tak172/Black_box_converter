#pragma once

namespace DumpSupport
{
    // "C:\Retime\Charm\Augur(x64)_1.3.881_191204_230811_3.dmp" образец состоит из 
    // "C:\Retime\Charm\Augur"                                  начальная часть полного пути к дампам т.е. полный путь exe без расширения
    //                      "(x64)_1.3.881"                     режим и версия
    //                                   "_191204_230811_3.dmp" штамп времени, номер (при совпадении) и расширение файла
    std::wstring FullPathWithoutExtension( DWORD _processId );
    std::wstring EventName( DWORD _processId, const std::wstring& modVer, const std::wstring& suffix );
    void WriteExplicitDump( DWORD procId, DWORD threadId, const std::wstring& modeVers, bool detailed, bool clientFlag, _EXCEPTION_POINTERS * pExPointers );
    std::wstring SuffixRequest();
    std::wstring SuffixFinal();
    std::wstring SuffixShMem();
    HANDLE OpenDumpFile( const std::wstring& partFName );
    void WriteDumpImp( HANDLE hFile, bool detailed, DWORD processId, DWORD threadId, bool clientPtr, _EXCEPTION_POINTERS* pExPointers );
    void CloseDumpFile( HANDLE hFile );

    HANDLE makeEvent( std::wstring evName ); // создать event (ранее не существовал)
    HANDLE findEvent( std::wstring evName ); // найти event (уже создан)
    // разделяемая память
    struct TransData
    {
        short                tdSize;
        DWORD                tdThreadId;
        _EXCEPTION_POINTERS* tdPtrEP;
        bool                 tdReqDetailed;
    };
    HANDLE makeShMem( std::wstring shMemName );
    bool readShMem( std::wstring shMemName, DumpSupport::TransData* trData );
    bool writeShMem( std::wstring shMemName, const DumpSupport::TransData& trData );
};