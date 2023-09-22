#include "stdafx.h"

#include <numeric>

#include "bbx_Record.h"
#include "bbx_File.h"

using namespace Bbx::Impl;

WriterTask::WriterTask(Bbx::Identifier id, Bbx::Stamp stamp, Bbx::RecordType type, const Bbx::Buffer& caption, const Bbx::Buffer& data)
    : id(id), stamp(stamp), type(type), sources()
{
    sources.reserve(2u);
    addCopyOfBuffer(caption);
    addCopyOfBuffer(data);
}

WriterTask::WriterTask(Bbx::Identifier id, Bbx::Stamp stamp, Bbx::RecordType type, const Bbx::Buffer& caption, const Bbx::Buffer& data1, const Bbx::Buffer& data2)
    : id(id), stamp(stamp), type(type), sources()
{
    sources.reserve(3u);
    addCopyOfBuffer(caption);
    addCopyOfBuffer(data1);
    addCopyOfBuffer(data2);
}

void WriterTask::addCopyOfBuffer(const Bbx::Buffer& data)
{
    sources.emplace_back(std::make_pair(data.size, Bbx::char_vec()));
    data.copyTo(sources.back().second);
}

unsigned WriterTask::getWeight() const
{
    return std::accumulate(sources.begin(), sources.end(), 0u, [](unsigned sum, const Source& data) {
        return sum + data.first;
    });
}

RecordOut::RecordOut(const WriterTask& task)
    : buffers(), size(0u), completedSize(0u), time(task.stamp), id(task.id), type(task.type)
{
    for (const WriterTask::Source& source : task.sources)
    {
        addBuffer(Buffer::createConst(source.first));
        if (source.first > 0u)
            addBuffer(source.second);
    }
}

void RecordOut::write(const Bbx::Buffer& outBuffer)
{
    if (!outBuffer.data_ptr || !outBuffer.size)
        return;

    ASSERT(outBuffer.size <= getRemainingSize());

    unsigned prevBuffersSize = 0;
    unsigned bytesWritten = 0;
    for (auto it = buffers.begin(); bytesWritten <= outBuffer.size && it != buffers.end(); ++it)
    {
        if (completedSize <= prevBuffersSize + it->size)
        {
            unsigned take = std::min(outBuffer.size - bytesWritten, prevBuffersSize + it->size - completedSize);
            memcpy(outBuffer.data_ptr + bytesWritten, it->data_ptr + completedSize - prevBuffersSize, take);
            bytesWritten += take;
            completedSize += take;
        }

        prevBuffersSize += it->size;
    }
}

bool RecordIn::readPart(const FileId& file, const FileAddress& address)
{
    unsigned readed = 0;

    // Итеративное заполнение буферов с предварительным чтением их размеров
    // из указанного участка файла
    while (!buffers.empty() && readed < address.size)
    {
        ContainerIn& container = buffers.front();

        // Чтение размера контейнера по частям
        if (container.sizeBytesRead < sizeof(container.size))
        {
            // Формируем буфер из еще не считанной части переменной размера контейнера
            Bbx::Buffer sizeBuf(reinterpret_cast<char *>(&container.size) + container.sizeBytesRead,
                    std::min(static_cast<unsigned>(sizeof(container.size)) - container.sizeBytesRead,
                             static_cast<unsigned>(address.size) - readed));

            SharedSection sizeLock(file, address.offset + readed, sizeBuf.size);
            if (sizeLock.read(sizeBuf))
            {
                readed += sizeBuf.size;
                container.sizeBytesRead += sizeBuf.size;

                ASSERT(container.sizeBytesRead <= sizeof(container.size));

                // Как только размер данных считан, в контейнере резервируется место
                if (container.sizeBytesRead == sizeof(container.size))
                {
                    if (container.size)
                        container.buffer->reserve(container.size);
                    else
                        buffers.pop();
                }
            }
            else
            {
                // Ошибка чтения из указанного места с указанным максимальным размером данных 
                // Возможно, следует бросать исключения в таких ситуациях
                return false;
            }
        }
        else
        {
            ASSERT(container.size);

            /* Считываем либо оставшийся размер массива, либо оставшееся число байт для чтения из куска */
            unsigned requestBytes = std::min(container.size - size32(*container.buffer), static_cast<unsigned>(address.size) - readed);
            container.buffer->resize(container.buffer->size() + requestBytes);
            Bbx::Buffer dataBuf(&container.buffer->back() - requestBytes + 1, requestBytes);

            SharedSection dataLock(file, address.offset + readed, dataBuf.size);                
            if (dataLock.read(dataBuf))
            {
                readed += dataBuf.size;

                // Если размер массива составил нужную величину, мы считаем, 
                // что считали его полностью и удаляем его из очереди на чтение
                if (container.buffer->size() == container.size)
                    buffers.pop();
            }
            else
            {
                // Ошибка чтения из указанного места с указанным максимальным размером данных 
                // Возможно, следует бросать исключения в таких ситуациях
                return false;
            }

        }
    }

    ASSERT(address.size == readed);
    return true;
}

Bbx::Impl::RecordIn::RecordIn( Stamp& recordStamp, char_vec& caption, char_vec& before, char_vec& after )
     : stamp(recordStamp), buffers()
{
    addContainer(caption);
    addContainer(before);
    addContainer(after);
}

Bbx::Impl::RecordIn::RecordIn( Stamp& recordStamp, char_vec& caption, char_vec& data )
    : stamp(recordStamp), buffers()
{
    addContainer(caption);
    addContainer(data);
}

void Bbx::Impl::RecordIn::addContainer(char_vec& container)
{
    container.clear();
    buffers.push(container);
}
