#pragma once

#ifndef LINUX
class FileId
{
public:
    FileId() : m_handle( INVALID_HANDLE_VALUE )
    {}
    FileId( HANDLE _handle ) : m_handle( _handle )
    {}
    operator HANDLE () const {
        return m_handle;
    }
    FileId& operator=( const FileId& _handle ) {
        m_handle = _handle;
        return ( *this );
    }
    bool empty() const {
        return INVALID_HANDLE_VALUE == m_handle;
    }
private:
    HANDLE m_handle;
};

#else

class FileId
{
public:
    FileId() : m_handle( -1 )
    {}
    FileId( int _handle ) : m_handle( _handle )
    {}
    operator int() const {
        return m_handle;
    }
    FileId& operator=( const FileId& _handle ) {
        m_handle = _handle;
        return ( *this );
    }
    bool empty() const {
        return -1 == m_handle;
    }
private:
    int m_handle;
};
#endif