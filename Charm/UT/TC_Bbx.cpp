#include "stdafx.h"

#include <numeric>
#include <boost/filesystem.hpp>
#include "TC_Bbx.h"
#include "../BlackBox/bbx_FileChain.h"
#include "../helpful/RT_ThreadName.h"
#include "../helpful/Log.h"
#include "../helpful/Time_Iso.h"

using namespace Bbx;
namespace bfs = boost::filesystem;
namespace bt = boost::posix_time;

CPPUNIT_TEST_SUITE_REGISTRATION( TC_Bbx );

static const int KB=1000; //1024;
static const int MB=KB*KB;
static const bt::time_duration quick_delay = bt::milliseconds(40);


/* Обязательно корректное добавление записи в ЧЯ (rec_param == true означает опорную запись) */
Bbx::Writer& operator <<(Bbx::Writer& writer, std::pair<time_t, bool> rec_param)
{
    Bbx::Identifier id(Bbx::Identifier::Source::FundOutput);
    if (rec_param.second)
    {
        Bbx::char_vec caption(10, 'R'), data(128, 'r');
        CPPUNIT_ASSERT(writer.pushReference(caption, data, rec_param.first, id));
    }
    else
    {
        Bbx::char_vec caption(10, 'I'), before(128, 'i'), after(196, 'a');
        CPPUNIT_ASSERT(writer.pushIncrement(caption, before, after, rec_param.first, id));
    }
    return writer;
}

class AsyncFunction
{
public:
    virtual ~AsyncFunction();
    void async_shutdown();
    bool shutdown();
    void start();
    bool running() const;
protected:
    AsyncFunction();
    virtual void run() =0;
private:
    void run_wrapper();
    boost::thread sub_thread;
};

class AsyncBlackBoxWriter : public AsyncFunction
{
public:
    AsyncBlackBoxWriter(const Bbx::Location& _locator, time_t _start, size_t _needWrites );
    ~AsyncBlackBoxWriter();
    size_t getWrittenBytes() const;
    bool ready() const;
private:
    void run() override;

    std::shared_ptr<Bbx::Writer> writer;
    time_t fixMoment;
    const size_t needWrites;
    std::atomic<size_t> bytesWrote;
    std::atomic<bool> endOfRun;
};

class AsyncBlackBoxReader : public AsyncFunction
{
public:
    AsyncBlackBoxReader( std::wstring seqNum, const Bbx::Location& locator, time_t start);
    ~AsyncBlackBoxReader();
    size_t getReadedBytes() const;
private:
    void run() override;
    void yield();

    const std::wstring sequenceNum;
    Bbx::Reader reader;
    const time_t fixMoment;
    std::atomic<size_t> bytesReaded;
};


AsyncFunction::AsyncFunction()
{}

AsyncFunction::~AsyncFunction()
{
    shutdown();
}

void AsyncFunction::start()
{
    sub_thread = boost::thread( boost::bind( &AsyncFunction::run_wrapper, this ) );
}

bool AsyncFunction::running() const
{
    return sub_thread.joinable();
}

void AsyncFunction::run_wrapper()
{
    run();
}

void AsyncFunction::async_shutdown()
{
    if ( sub_thread.joinable() )
        sub_thread.interrupt();
}

bool AsyncFunction::shutdown()
{
    async_shutdown();
    if ( sub_thread.joinable() )
        return sub_thread.try_join_for( boost::chrono::milliseconds(100) );
    else
        return false;
}

AsyncBlackBoxWriter::AsyncBlackBoxWriter(const Bbx::Location& _locator, time_t _start, size_t _needWrites )
    : writer(Bbx::Writer::create(_locator)), fixMoment(_start), needWrites(_needWrites),
    bytesWrote(0), endOfRun(false)
{
    writer->setRecomendedFileSize( 10*MB );
}

AsyncBlackBoxWriter::~AsyncBlackBoxWriter()
{}

void AsyncBlackBoxWriter::run()
{
    try
    {
        RT_SetThreadName( "AsyncBlackBoxWriter::run" );
        Bbx::Identifier id( Bbx::Identifier::Source::HaronInput );
        const size_t capSize = 128;
        const size_t dataSize = 185007;
        std::string caption( capSize, 'c' ), data( dataSize, 'd' );

        size_t writingOperations = 0;
        while( !boost::this_thread::interruption_requested() && writingOperations < needWrites )
        {
            ++writingOperations;
            CPPUNIT_ASSERT( writer->pushReference( caption, data, fixMoment + writingOperations, id ) );
            {
                bytesWrote += capSize + dataSize;
                ++id;
            }
        }
        if( writingOperations == needWrites )
            writer->flush();
    }
    catch( boost::thread_interrupted& /*e*/ )
    {
        //normal exit
    }
    catch( ... )
    {
    }
        endOfRun = true;
}

bool AsyncBlackBoxWriter::ready() const
{
    return endOfRun;
}

size_t AsyncBlackBoxWriter::getWrittenBytes() const
{
    return bytesWrote;
}

AsyncBlackBoxReader::AsyncBlackBoxReader(std::wstring seqNum, const Bbx::Location& locator, time_t start)
:sequenceNum(seqNum), reader(locator), fixMoment(start), bytesReaded(0)
{}

AsyncBlackBoxReader::~AsyncBlackBoxReader()
{}

size_t AsyncBlackBoxReader::getReadedBytes() const
{
    return bytesReaded;
}

void AsyncBlackBoxReader::run()
{
    RT_SetThreadName(("AsyncBlackBoxReader"+ToUtf8(sequenceNum)).c_str());

    try
    {
        Bbx::Stamp stamp;
        Bbx::char_vec caption, data;

        reader.setDirection( true );
        while( !boost::this_thread::interruption_requested() && !reader.rewind( fixMoment ) )
            yield();

        time_t readOperations = 0;
        while( !boost::this_thread::interruption_requested() ) {
            if( reader.readReference( stamp, caption, data ) ) {
                ++readOperations;
                if( stamp.getTime() == fixMoment + readOperations - 1 ) {
                    //bytesReaded += caption.size() + data.size();
                    // опорные записи при смене файла читаются повторно.
                    // \TODO: исправить повторное чтение референсных записей при переходе файла
                    --readOperations;
                    //OutputDebugString( (sequenceNum + L" error row " + std::to_wstring(readOperations) + L"\n").c_str() );
                } else if( stamp.getTime() == fixMoment + readOperations ) {
                    bytesReaded += caption.size() + data.size();
                } else {
                    CPPUNIT_ASSERT( stamp.getTime() == fixMoment + readOperations );
                }

                while( !boost::this_thread::interruption_requested() && !reader.next() )
                    yield();
            } else {
                yield();
            }
        }
    } catch( boost::thread_interrupted& /*e*/ ) {
        //normal exit
    } catch( ... ) {
    }
}

void AsyncBlackBoxReader::yield()
{
    boost::this_thread::sleep( bt::milliseconds( 30 ) );
}

/*
проверить классы BbxIn и BbxOut
*/
void TC_Bbx::setUp()
{
    restore_delay = Bbx::Writer::getCacheDeviateDelay();
    Bbx::Writer::setCacheDeviateDelay(quick_delay);
    fix_moment = fixTm();

    for( size_t i=0; i<BBX_COUNT; ++i )
    {
        std::wstring pf = L"Pfx-"+std::to_wstring(i)+L"-";
        std::wstring sf = L".sfx"+std::to_wstring(i);
        BbxLocation[i] = Bbx::Location( bfs::temp_directory_path().wstring(), pf, sf );
    }

    defaultId = Bbx::Identifier(Bbx::Identifier::Source::HaronOutput);

    for( auto& bl : BbxLocation )
        DeleteBlackBoxFiles(bl);
}

void TC_Bbx::tearDown()
{
    for( auto& bl : BbxLocation )
        DeleteBlackBoxFiles(bl);
    Bbx::Writer::setCacheDeviateDelay(restore_delay);
}

size_t TC_Bbx::DeleteBlackBoxFiles(const Bbx::Location& location)
{
    Bbx::FileChain fc = *location.getCPtrChain();
    size_t processed = 0;
    while( !fc.empty() )
    {
        auto fPath = fc.takeEarliestFile();
        boost::system::error_code ec;
        if ( 0 == bfs::remove( fPath, ec ) )
        {
            CPPUNIT_FAIL("Can't delete bbx file");
        }
        processed++;
    }
    return processed;
}

time_t TC_Bbx::fixTm()
{
    // начальное стандартное время выбрано произвольно
    struct tm work;
    work.tm_sec = 5;     /* seconds after the minute - [0,59] */
    work.tm_min = 4;     /* minutes after the hour - [0,59] */
    work.tm_hour = 3;    /* hours since midnight - [0,23] */
    work.tm_mday = 2;    /* day of the month - [1,31] */
    work.tm_mon = 1;     /* months since January - [0,11] */
    work.tm_year = 2011 - 1900;   /* years since 1900 */
    work.tm_wday = 0;    /* days since Sunday - [0,6] */
    work.tm_yday = 0;    /* days since January 1 - [0,365] */
    work.tm_isdst = 0;   /* daylight savings time flag */
    time_t t = mktime( &work );
    // только для удобства отладки выставим последние 6 цифр нулями - погрешность около 11 суток
    t -= t % 1000000;
    return t;
}

void TC_Bbx::Create()
{
    // некие данные
    const int count = 5;
    const std::string captions[count] = {"caption0", "",        "caption2", "", ""};
    const std::string before[count] =   {"before0",  "before1", "before2",  "before3", ""};
    const std::string after[count] =    {"after0",   "after1",  "after2", "", ""};
    Bbx::Identifier id(Bbx::Identifier::Source::HaronOutput);

    {
        // Создать новый черный ящик
        auto out_bbx = Bbx::Writer::create(BbxLocation[0]);
        CPPUNIT_ASSERT(out_bbx);
        for (int i = 0; i < count; ++i)
        {
            if (!i || i == count - 1)
                CPPUNIT_ASSERT(out_bbx->pushReference(captions[i], before[i], fix_moment + i, id++));
            else
                CPPUNIT_ASSERT(out_bbx->pushIncrement(captions[i], before[i], after[i], fix_moment + i, id++));
        }

        /* Нельзя создать два черных ящика для записи в одном наборе папка/префикс/суффикс */
        auto double_out_bbx = Bbx::Writer::create(BbxLocation[0]);
        CPPUNIT_ASSERT_MESSAGE("Overwrite bbx",!double_out_bbx);
    }

    {
        // Прочитать черный ящик
        Bbx::Reader in_bbx(BbxLocation[0]);

        // убедиться что в ящике есть записи
        Bbx::Stamp beg,end;
        std::tie(beg,end) = in_bbx.getBoundStamp();
        CPPUNIT_ASSERT_EQUAL( fix_moment, beg.getTime() );
        CPPUNIT_ASSERT_EQUAL( fix_moment + count - 1, end.getTime() );

        CPPUNIT_ASSERT(in_bbx.rewind(fix_moment));

        Bbx::char_vec caption, data;
        Bbx::Stamp stamp;

        for (size_t i = 0; i < count; ++i)
        {
            in_bbx.readAnyRecord(stamp, caption, data);
            CPPUNIT_ASSERT(in_bbx.lastResult());
            CPPUNIT_ASSERT(stamp.getTime() == time_t(fix_moment + i));

            Bbx::Identifier in_id = in_bbx.getCurrentIdentifier();
            CPPUNIT_ASSERT_EQUAL(i, (size_t)in_id.getId());

            ReadResult rr = in_bbx.next();
            if (i < count - 1)
                // Не вынесено в тело цикла для обеспечения корректности перехода
                CPPUNIT_ASSERT(rr);
            else 
                CPPUNIT_ASSERT(!rr);
        }
    }
}

void TC_Bbx::Search()
{
    using namespace Bbx;
    /* Проверка поиска по несозданному ЧЯ */
    {
        Reader reader(BbxLocation[0]);
        CPPUNIT_ASSERT(ReadResult::NoDataAvailable == reader.rewind(fix_moment));
        CPPUNIT_ASSERT(ReadResult::NoDataAvailable == reader.rewind(fix_moment + 5000));
    }

    /* Запись данных для теста */
    {
        auto writer = Bbx::Writer::create(BbxLocation[0]);
        CPPUNIT_ASSERT(writer);
        *writer
            << std::make_pair(fix_moment + 0000, true)
            << std::make_pair(fix_moment + 1000, false)
            << std::make_pair(fix_moment + 2000, false)
            << std::make_pair(fix_moment + 3000, true)
            << std::make_pair(fix_moment + 4000, false)
            << std::make_pair(fix_moment + 5000, false)
            << std::make_pair(fix_moment + 5001, true);
    }

    /* Проход по ключевым точкам и проверка */
    {
        Reader reader(BbxLocation[0]);
        CPPUNIT_ASSERT(ReadResult::Success == reader.rewind(fix_moment));
        CPPUNIT_ASSERT(ReadResult::FoundApproximateValue == reader.rewind(fix_moment + 1));
        CPPUNIT_ASSERT(ReadResult::FoundApproximateValue == reader.rewind(fix_moment + 1000));
        CPPUNIT_ASSERT(ReadResult::FoundApproximateValue == reader.rewind(fix_moment + 2999));
        CPPUNIT_ASSERT(ReadResult::Success == reader.rewind(fix_moment + 3000));
        CPPUNIT_ASSERT(ReadResult::FoundApproximateValue == reader.rewind(fix_moment + 3001));
        CPPUNIT_ASSERT(ReadResult::FoundApproximateValue == reader.rewind(fix_moment + 4999));
        CPPUNIT_ASSERT(ReadResult::Success == reader.rewind(fix_moment + 5001));
        CPPUNIT_ASSERT(ReadResult::FoundApproximateValue == reader.rewind(fix_moment + 10000));
    }
    
}

void TC_Bbx::Records()
{
    using namespace Bbx;
    const size_t _big = 10000000u;
    const size_t _medium = 1000000u;
    const size_t _small = 100u;
    const std::string bodyBig(_big, 'b'), bodyMedium(_medium, 'm'), bodySmall(_small, 's');
    const std::string capBig(_big, 'B'), capMedium(_medium, 'M'), capSmall(_small, 'S');
    const Bbx::Identifier id(Bbx::Identifier::Source::HaronOutput);
    {
        auto out = Bbx::Writer::create(BbxLocation[0]);
        CPPUNIT_ASSERT(out->pushReference(capBig, bodyBig, fix_moment + 1, id));
        CPPUNIT_ASSERT(out->pushReference(capMedium, bodyMedium, fix_moment + 2, id));
        CPPUNIT_ASSERT(out->pushReference(capSmall, bodySmall, fix_moment + 3, id));
    }
    {
        Reader in(BbxLocation[0]);
        CPPUNIT_ASSERT(in.rewind(fix_moment));
        CPPUNIT_ASSERT(Bbx::RecordType::Reference == in.getCurrentType());
        CPPUNIT_ASSERT(fix_moment + 1 == in.getBoundStamp().first && fix_moment + 3 == in.getBoundStamp().second);
        CPPUNIT_ASSERT(in.getCurrentStamp() - 1 == fix_moment);

        Stamp stamp;
        std::vector<char> caption, data;
        CPPUNIT_ASSERT(in.readReference(stamp, caption, data));
        CPPUNIT_ASSERT(stamp - 1 == fix_moment);
        CPPUNIT_ASSERT(caption.size() == _big);
        CPPUNIT_ASSERT(data.size() == _big);

        CPPUNIT_ASSERT(in.next());
        CPPUNIT_ASSERT(in.readReference(stamp, caption, data));
        CPPUNIT_ASSERT(stamp - 2 == fix_moment);
        CPPUNIT_ASSERT(caption.size() == _medium);
        CPPUNIT_ASSERT(data.size() == _medium);

        CPPUNIT_ASSERT(in.next());
        CPPUNIT_ASSERT(in.readReference(stamp, caption, data));
        CPPUNIT_ASSERT(stamp - 3 == fix_moment);
        CPPUNIT_ASSERT(caption.size() == _small);
        CPPUNIT_ASSERT(data.size() == _small);

        CPPUNIT_ASSERT(ReadResult::NoDataAvailable == in.next());

        in.setDirection(false);
        CPPUNIT_ASSERT(in.readReference(stamp, caption, data));
        CPPUNIT_ASSERT(stamp - 3 == fix_moment);
        CPPUNIT_ASSERT(caption.size() == _small);
        CPPUNIT_ASSERT(data.size() == _small);

        CPPUNIT_ASSERT(in.next());
        CPPUNIT_ASSERT(in.readReference(stamp, caption, data));
        CPPUNIT_ASSERT(stamp - 2 == fix_moment);
        CPPUNIT_ASSERT(caption.size() == _medium);
        CPPUNIT_ASSERT(data.size() == _medium);

        CPPUNIT_ASSERT(in.next());
        CPPUNIT_ASSERT(in.readReference(stamp, caption, data));
        CPPUNIT_ASSERT(stamp - 1 == fix_moment);
        CPPUNIT_ASSERT(caption.size() == _big);
        CPPUNIT_ASSERT(data.size() == _big);

        CPPUNIT_ASSERT(ReadResult::NoDataAvailable == in.next());
    }
}

void TC_Bbx::Silence()
{
    using namespace Bbx;
    const std::string zag = "шапка";
    const std::string body = "тельце";
    const time_t TOTAL = 24 * 60 * 60;
    const size_t EVERY = 10 * 60;
    // Создать новый черный ящик
    {
        auto out_bbx = Bbx::Writer::create(BbxLocation[0]);
        CPPUNIT_ASSERT(out_bbx);
        /* Заставить ЧЯ создавать множество файлов */
        out_bbx->setPageSize(256);
        out_bbx->setRecomendedFileSize(512);
        
        for( time_t curr = fix_moment; curr <= fix_moment + TOTAL; curr += EVERY )
        {
            *out_bbx << std::make_pair(curr, true);
            CPPUNIT_ASSERT(!out_bbx->isDead());
        }
    }
    // Прочитать черный ящик
    {
        Reader in_bbx(BbxLocation[0]);
        // убедиться что в ящике есть записи
        auto bounds = in_bbx.getBoundStamp();
        CPPUNIT_ASSERT( bounds.first.getTime() == fix_moment );
        CPPUNIT_ASSERT( fix_moment + TOTAL == bounds.second.getTime() );
    }
}

void TC_Bbx::AsyncStressTest()
{
    const size_t WRITES_REF = 500; // количество записей в ящик
    unsigned READER_ALL = 50*2;     // общее количество читателей
    unsigned READER_PRE = READER_ALL/5; // столько читателей запускается до запуска писателя

    typedef std::shared_ptr<AsyncBlackBoxReader> AsyncBlackBoxReaderPtr;
    std::vector<AsyncBlackBoxReaderPtr> readers;
    
    // Запуск читателей до запуска писателя
    for (size_t i = 0; i < READER_PRE; ++i)
    {
        auto& BbxLoc = BbxLocation[ i%BBX_COUNT ];
        AsyncBlackBoxReaderPtr r = std::make_shared<AsyncBlackBoxReader>( L"pre."+std::to_wstring(i), BbxLoc, fix_moment );
        readers.push_back( r );
        r->start();
        CPPUNIT_ASSERT( r->running() );
    }
    boost::this_thread::sleep( quick_delay + bt::milliseconds(100) );

    bt::ptime startPTime = bt::microsec_clock::universal_time();
    
    std::unique_ptr<AsyncBlackBoxWriter> asyncWriter[BBX_COUNT];
    for( size_t i=0; i<BBX_COUNT; ++i )
        asyncWriter[i].reset( new AsyncBlackBoxWriter( BbxLocation[i], fix_moment,WRITES_REF) );
    for( auto& aw : asyncWriter )
        aw->start();

    // Запуск читеталей после запуска писателя
    for (size_t j = READER_PRE; j < READER_ALL; ++j)
    {
        auto& BbxLoc = BbxLocation[ j%BBX_COUNT ];
        AsyncBlackBoxReaderPtr r = std::make_shared<AsyncBlackBoxReader>( L"aft."+std::to_wstring(j), BbxLoc, fix_moment );
        readers.push_back( r );
        r->start();
        CPPUNIT_ASSERT( r->running() );
    }

    auto no_rdy = []( std::unique_ptr<AsyncBlackBoxWriter>& pw ){
        return !pw->ready();
    };

    while ( std::any_of( std::begin(asyncWriter), std::end(asyncWriter), no_rdy ) )
    {
        boost::this_thread::sleep( quick_delay );
    }

    // Измерение результирующей скорости записи в файл
    for( auto& aw : asyncWriter )
        aw->shutdown();
    auto written_0 = asyncWriter[0]->getWrittenBytes();
    const bt::ptime finishWrite = bt::microsec_clock::universal_time();
    for( size_t i=0; i<BBX_COUNT; ++i )
    {
        auto written = asyncWriter[i]->getWrittenBytes();
        auto bytesWPS = double(written * 1000.) / double((finishWrite - startPTime).total_milliseconds());
		auto wspeed = bytesWPS / MB;
		boost::wformat fmt_(L"Writer%d     speed %6.2f MBps (total %d MB)\n");
        fmt_% i% wspeed% int( written / MB );
		DebugLog( fmt_.str() );
        CPPUNIT_ASSERT_MESSAGE( "Write"+std::to_string(i)+" speed only " + std::to_string(bytesWPS)+" bps", bytesWPS > 500*KB );
    }

    // дождаться когда все читатели дочитают требуемый объем
    std::vector<AsyncBlackBoxReaderPtr> outReaders;
    outReaders.reserve( readers.size() );
    while( outReaders.size() < readers.size() ) {
        for( auto& r : readers ) {
            if ( r ) {
                size_t rr = r->getReadedBytes();
                if ( rr >= written_0 ) {
                    r->async_shutdown();
                    outReaders.push_back( r );
                    r.reset();
                }
            }
        }
    }

    bt::ptime cleanReader = bt::microsec_clock::universal_time() + bt::seconds(3);
    while( !outReaders.empty() && bt::microsec_clock::universal_time() < cleanReader )
    {
        for( auto itOut = outReaders.begin(); outReaders.end() != itOut; /*none*/ )
        {
            if ( (*itOut)->shutdown() )
                itOut = outReaders.erase( itOut );
            else
                ++itOut;
        }
    }
    CPPUNIT_ASSERT( outReaders.empty() );
}

// несколько сеансов записи
void TC_Bbx::FileNames_recreate()
{
    const size_t c_PageSize = 512;
    const size_t c_FileSize = c_PageSize * 8;
    const size_t c_FilesCount = 16;
    std::string body(c_FileSize / 2, '@');
    Bbx::Identifier id(Bbx::Identifier::Source::HaronInput);
    for (size_t it = 0; it < c_FilesCount; ++it)
    {
        auto writer = Bbx::Writer::create(BbxLocation[0]);
        CPPUNIT_ASSERT(writer);
        writer->setPageSize(c_PageSize);
        writer->setRecomendedFileSize(c_FileSize);
        CPPUNIT_ASSERT(writer->pushReference(Buffer(), body, fix_moment, id));
        id.increment();
    }

    size_t numberFiles = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
    CPPUNIT_ASSERT_EQUAL(c_FilesCount, numberFiles);
}

// непрерывная запись в одном сеансе
void TC_Bbx::FileNames_continuous()
{
    const size_t c_PageSize = 512;
    const size_t c_FileSize = c_PageSize * 8;
    const size_t c_FilesCount = 16;
    std::string body( c_FileSize / 2, '@' );
    Bbx::Identifier id( Bbx::Identifier::Source::HaronInput );
    {
        auto writer = Bbx::Writer::create( BbxLocation[ 0 ] );

        for( size_t it = 0; it < c_FilesCount; ++it )
        {
            CPPUNIT_ASSERT( writer );
            writer->setPageSize( c_PageSize );
            writer->setRecomendedFileSize( c_FileSize );
            CPPUNIT_ASSERT( writer->pushReference( Buffer(), body, fix_moment, id ) );
            id.increment();
        }
    }
    size_t numberFiles = BbxLocation[ 0 ].getCPtrChain()->getNumberOfFiles();
    CPPUNIT_ASSERT_EQUAL( c_FilesCount, numberFiles );
}

void TC_Bbx::SizeParameters()
{
    using namespace Bbx;
    const size_t c_PageSize = 512;
    const size_t c_FileSize = c_PageSize * 8;
    const size_t c_FilesCount = 16;
    std::string body(c_FileSize * 4, '+');

    for (size_t it = 0; it < c_FilesCount; ++it)
    {
        auto writer = Bbx::Writer::create(BbxLocation[0]);
        CPPUNIT_ASSERT(writer);
        CPPUNIT_ASSERT( c_PageSize != writer->getPageSize() );
        writer->setPageSize(c_PageSize);
        CPPUNIT_ASSERT( c_PageSize == writer->getPageSize() );
        writer->setRecomendedFileSize(c_FileSize);
        CPPUNIT_ASSERT(writer->pushReference(Buffer(), body, fix_moment, defaultId));
    }

    auto fileCount = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
    CPPUNIT_ASSERT_EQUAL(c_FilesCount, fileCount);
}

/*----------------------------------------------
запись bbx из отдельной нити исполнения
интенсивная запись bbx
каждый читатель в отдельной нити исполнения
активных читателей несколько
-----------------------------------------------*/
#define VIDI(xx)   /*wcout << char(xx)*/

// очистка устаревших файлов
void TC_Bbx::RemoveOldFiles()
{
    const int SZ_PAGE = 4000;
    const int NUM_PAGE = 2;
    const size_t STEPS = 5;
    const int PERIOD = 120; // 2 мин.

    using namespace Bbx;

    for ( size_t i = 0; i < STEPS; i++ )
	{
        VIDI( '-' );
        auto bOut = Bbx::Writer::create(BbxLocation[0]);
        bOut->setPageSize( SZ_PAGE );
        bOut->setRecomendedFileSize( SZ_PAGE * NUM_PAGE );

        time_t t = fixTm() + i * PERIOD;
        std::string body( 2000, 'b' );
        fill_sample_with_time(body, t, i);

        CPPUNIT_ASSERT( bOut->pushReference( std::string(), body, t, defaultId ) );
	}
    auto numFiles = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
    CPPUNIT_ASSERT_EQUAL( STEPS, numFiles );
    for( size_t i = 0; i < STEPS; i++ )
	{
        auto bOut = Bbx::Writer::create( BbxLocation[0] );
        bOut->setLifeTime( STEPS * PERIOD - 1 );
        time_t t = fixTm() + ( STEPS + i ) * PERIOD;
        std::string body( 8000, 'c' );
        fill_sample_with_time(body, t, i);
		bOut->pushReference( std::string(), body, t, defaultId );
        auto v2count = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
        CPPUNIT_ASSERT( v2count == STEPS );
	}
}

// проверка непрерывной очистки устаревших файлов при записи
// Задаем интервал хранения и проверяем, что при добавлении
// записей количество файлов ограничено т.е. старые данные удаляются
void TC_Bbx::ContinuousTruncation()
{
    using namespace Bbx;
    //const int WAIT_WRITE = 510; unused?
    const int STEPS = 30;           // выполнить столько операций записи
    const int DELTA = 10 * 60;       // интервал времени между записями
    const int GUARD = 3;             // столько записей надо хранить (приблизительно)
    const int HISTORY = DELTA * GUARD; // сохраняемая история
    const int SZ_PAGE = 200;    // страница ящика
    const int NUM_PAGE = 7;     // страниц в файле
    auto bOut = Bbx::Writer::create( BbxLocation[0] );
    bOut->setPageSize( SZ_PAGE );
    bOut->setRecomendedFileSize( SZ_PAGE * NUM_PAGE );
    bOut->setLifeTime( HISTORY );
    std::string body( SZ_PAGE, 'x' );
    // несколько первых записей без проверок
    for( int i = 0; i < GUARD; i++ )
    {
        bOut->pushReference( std::string(), body, fixTm() + i * DELTA, defaultId );
    }

    // т.к. запись по размеру влезает в один файл, то
    // наибольшее количество файлов не превысит
    const int MAX_COUNT_OF_FILES = GUARD +  // по целому файлу на каждую запись
                                   1 +      // начальные данные даже в предыдущем файле
                                   1;       // конечные данные ещё и в последующем файле
    for( int i = GUARD; i < STEPS; i++ )
    {
        // добавляем очередную запись
        time_t curr_time = fixTm() + i * DELTA;
        bOut->pushReference( std::string(), body, curr_time, defaultId );

        bOut->flush();
		
        // проверяем количество файлов
        auto vFsize = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
        CPPUNIT_ASSERT( vFsize >= 1 );
        // что-то там должно быть
        CPPUNIT_ASSERT( vFsize <= MAX_COUNT_OF_FILES );  // хотя и немного
        // проверяем наличие в ящике требуемого интервала
		
        bOut->flush();

        Reader bIn( BbxLocation[0] );
        time_t bbb,eee;
        std::tie(bbb,eee) = bIn.getBoundStamp();
        CPPUNIT_ASSERT( curr_time <= eee );           // последняя запись есть
        CPPUNIT_ASSERT( bbb <= curr_time - HISTORY ); // хранимое начало есть
        CPPUNIT_ASSERT( curr_time - 4 * HISTORY < bbb ); // явное старье отсутствует
        VIDI( '+' );
    }
    CPPUNIT_ASSERT( !bOut->isDead() );
}

// проверка непрерывной очистки файлов по общему размеру
// Задаем допустимый размер и проверяем, что он не превышен
void TC_Bbx::ConstrainSpace()
{
    using namespace Bbx;
    // const int TICK_FLOW = 700; unused?
    const int STEPS = 30;                    // выполнить столько операций записи
    const int DELTA = 10 * 60;               // интервал времени между записями
    const int SZ_PAGE = 1024;                // страница ящика
    const int NUM = 2;
    const int SZ_FILE = NUM*SZ_PAGE + SZ_PAGE/2; // размер файла

    auto bOut = Bbx::Writer::create( BbxLocation[0] );
    bOut->setPageSize( SZ_PAGE );
    bOut->setRecomendedFileSize( SZ_FILE );
    const int DISK = 20*1024;
    bOut->setDiskLimit( "20KB" );
    std::string body( SZ_PAGE, 'x' );

    // т.к. запись по размеру влезает в один файл, то
    // наибольшее количество файлов не превысит
    const int MAX_COUNT_OF_FILES = DISK/SZ_FILE;
    for( int i = 0; i < STEPS; i++ )
    {
        // добавляем очередную запись
        time_t curr_time = fixTm() + i * DELTA;
        bOut->pushReference( std::string(), body, curr_time, defaultId );
		
        bOut->flush();

        // проверяем количество файлов
        auto vFsize = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
        CPPUNIT_ASSERT( vFsize >= 1 );
        // что-то там должно быть
        CPPUNIT_ASSERT( vFsize <= MAX_COUNT_OF_FILES );  // хотя и немного
        // проверяем наличие в ящике требуемого интервала
        
        bOut->flush();

        Reader bIn( BbxLocation[0] );
        time_t eee = bIn.getBoundStamp().second;
        CPPUNIT_ASSERT( curr_time <= eee );           // последняя запись есть
        VIDI( '+' );
    }
    auto vFsize = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
    CPPUNIT_ASSERT( vFsize >= 1 );
    CPPUNIT_ASSERT( vFsize <= MAX_COUNT_OF_FILES );
    CPPUNIT_ASSERT( !bOut->isDead() );
}

// проверка непрерывной очистки файлов по интервалу хранения
// Задаем допустимый интервал и проверяем, что он не превышен
void TC_Bbx::ConstrainTime()
{
    using namespace Bbx;
    // const int TICK_FLOW = 700; unused?
    const int STEPS = 30;                    // выполнить столько операций записи
    const time_t DAY = 24*60*60;         // между записями ровно сутки
    const int SZ_PAGE = 1024;                // страница ящика
    const int SZ_FILE = 2*SZ_PAGE + SZ_PAGE/2; // размер файла

    auto bOut = Bbx::Writer::create( BbxLocation[0] );
    bOut->setPageSize( SZ_PAGE );
    bOut->setRecomendedFileSize( SZ_FILE );
    const time_t KEEP = 5*DAY;
    bOut->setLifeTime( KEEP );
    std::string body( SZ_PAGE, 'x' );

    // т.к. интервал хранения указан в днях, то всего первый + {в интервале} + новый
    const size_t MAX_COUNT_OF_FILES = 1 + KEEP/DAY + 1;
    const size_t MIN_COUNT_OF_FILES = 2;
    for( size_t i = 0; i < STEPS; i++ )
    {
        // добавляем очередную запись
        time_t curr_time = fixTm() + i * DAY;
        bOut->pushReference( std::string(), body, curr_time, defaultId );
        bOut->flush();

        // проверяем количество файлов
        auto vFsize = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
        CPPUNIT_ASSERT( vFsize >= (std::min)(i+1,MIN_COUNT_OF_FILES) ); // что-то там должно быть
        CPPUNIT_ASSERT( vFsize <= MAX_COUNT_OF_FILES );  // хотя и немного
        // проверяем наличие в ящике требуемого интервала
        bOut->flush();
        Reader bIn( BbxLocation[0] );
        time_t eee = bIn.getBoundStamp().second;
        CPPUNIT_ASSERT( curr_time <= eee );           // последняя запись есть
        VIDI( '+' );
    }
    auto vFsize = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
    CPPUNIT_ASSERT( vFsize >= 1 );
    CPPUNIT_ASSERT( vFsize <= MAX_COUNT_OF_FILES );
    CPPUNIT_ASSERT( !bOut->isDead() );
}

// проверка ограничения по размеру
void TC_Bbx::ConstrainValue()
{
    const int K = 1024;
    const int M = 1024*K;
    const BBX_DISK_SIZE G = 1024*M;
    const BBX_DISK_SIZE T = 1024*LONGLONG(G);
    const BBX_DISK_SIZE P = 1024*LONGLONG(T);

    auto bOut = Bbx::Writer::create(BbxLocation[0]);
    bOut->setPageSize( K );
    bOut->setRecomendedFileSize( M );
    // обычные случаи
    CPPUNIT_ASSERT( bOut->setDiskLimit( "350GB" ) );
    CPPUNIT_ASSERT( 350*G == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( bOut->setDiskLimit( "1G" ) );
    CPPUNIT_ASSERT( 1*G == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( bOut->setDiskLimit( "25MB" ) );
    CPPUNIT_ASSERT( 25*M == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( bOut->setDiskLimit( "2M" ) );
    CPPUNIT_ASSERT( 2*M == bOut->getDiskLimit() );
    // слишком маленькое значение - меньше двойного размера файла нельзя
    BBX_DISK_SIZE old = 2*M;
    CPPUNIT_ASSERT( !bOut->setDiskLimit( "73KB" ) );
    CPPUNIT_ASSERT( old == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( !bOut->setDiskLimit( "73K" ) );
    CPPUNIT_ASSERT( old == bOut->getDiskLimit() );
    // слишком большое
    CPPUNIT_ASSERT( !bOut->setDiskLimit( "2PB" ) );
    CPPUNIT_ASSERT( old == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( !bOut->setDiskLimit( "2P" ) );
    CPPUNIT_ASSERT( old == bOut->getDiskLimit() );
    // некорректное 
    CPPUNIT_ASSERT( !bOut->setDiskLimit( "-4K" ) );
    CPPUNIT_ASSERT( old == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( !bOut->setDiskLimit( "0" ) );
    CPPUNIT_ASSERT( old == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( !bOut->setDiskLimit( "+25.3M" ) );
    CPPUNIT_ASSERT( old == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( !bOut->setDiskLimit( "" ) );
    CPPUNIT_ASSERT( old == bOut->getDiskLimit() );
    // большое число в разном виде
    CPPUNIT_ASSERT( bOut->setDiskLimit( "1PB" ) );
    CPPUNIT_ASSERT( P == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( bOut->setDiskLimit( "1P" ) );
    CPPUNIT_ASSERT( P == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( bOut->setDiskLimit( "1024TB" ) );
    CPPUNIT_ASSERT( P == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( bOut->setDiskLimit( "1048576GB" ) );
    CPPUNIT_ASSERT( P == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( bOut->setDiskLimit( "1073741824MB" ) );
    CPPUNIT_ASSERT( P == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( bOut->setDiskLimit( "1099511627776KB" ) );
    CPPUNIT_ASSERT( P == bOut->getDiskLimit() );
    CPPUNIT_ASSERT( bOut->setDiskLimit( "1125899906842624" ) );
    CPPUNIT_ASSERT( P == bOut->getDiskLimit() );
}


// проверка на обработку лакун
void TC_Bbx::Lacuna()
{   
    using namespace Bbx;
    // Создадим ящик [100..500] с лакуной [201..399]
    {
        {
            auto w = Bbx::Writer::create( BbxLocation[0] );
            *w   << std::make_pair(fix_moment + 100, true)
                << std::make_pair(fix_moment + 200, true);
        }
        {
            auto w = Bbx::Writer::create( BbxLocation[0] );
            *w   << std::make_pair(fix_moment + 400, true)
                << std::make_pair(fix_moment + 500, true);
        }
    }
    // проверим наличие данных только в указанных интервалах
    Reader readerBbx( BbxLocation[0] );
    //читаем вперед и видим пустоты
    {
        readerBbx.setDirection( true );
        // встаем перед пустотой
        CPPUNIT_ASSERT(readerBbx.rewind(fix_moment + 200));
        time_t currentStamp = readerBbx.getCurrentStamp().getTime();
        CPPUNIT_ASSERT( currentStamp - fix_moment == 200 );
        // шаг через пустоту
        ReadResult readResult = readerBbx.next();
        CPPUNIT_ASSERT( ReadResult::NewSession == readResult );
        // так как обнаружена новая сессия, делаем шаг с усилием
        readResult = readerBbx.forceNext();
        CPPUNIT_ASSERT( ReadResult::Success == readResult );
        // впрочем, сам переход выполнен
        time_t nextStamp = readerBbx.getCurrentStamp().getTime();
        CPPUNIT_ASSERT( nextStamp - fix_moment == 400 );
        // шагаем обычный
        readResult = readerBbx.next();
        CPPUNIT_ASSERT( ReadResult::Success == readResult );
        // шагаем за конец ящика
        readResult = readerBbx.next();
        time_t endStamp = readerBbx.getCurrentStamp().getTime();
        CPPUNIT_ASSERT( ReadResult::NoDataAvailable == readResult );
        CPPUNIT_ASSERT( endStamp - fix_moment == 500 );
    }

    //читаем назад и видим пустоты
    {
        readerBbx.setDirection( false );
        // встаем перед пустотой
        CPPUNIT_ASSERT( readerBbx.rewind( fix_moment + 400 ) );
        time_t currentStamp = readerBbx.getCurrentStamp().getTime();
        CPPUNIT_ASSERT( currentStamp - fix_moment == 400 );
        // шаг через пустоту
        ReadResult readResult = readerBbx.next();
        CPPUNIT_ASSERT( ReadResult::NewSession == readResult );
        readResult = readerBbx.forceNext();
        CPPUNIT_ASSERT( ReadResult::Success == readResult );
        // впрочем, сам переход выполнен
        time_t nextStamp = readerBbx.getCurrentStamp().getTime();
        CPPUNIT_ASSERT( nextStamp - fix_moment == 200 );
        // шагаем обычно
        readResult = readerBbx.next();
        CPPUNIT_ASSERT( ReadResult::Success == readResult );
        // шаг за начало ящика
        readResult = readerBbx.next();
        time_t endStamp = readerBbx.getCurrentStamp().getTime();
        CPPUNIT_ASSERT( ReadResult::NoDataAvailable == readResult );
        CPPUNIT_ASSERT( endStamp - fix_moment == 100 );
    }
}


// граница чтения
void TC_Bbx::ReadBorder()
{
    using namespace Bbx;
    // Добавляем в ящик опорные посылки fix+100, +200, +300
    {
        Reader readerBbx(BbxLocation[0]);
        for( int i = 0; i < 10; i++ )
        {
            {
                auto writerTmp = Bbx::Writer::create(BbxLocation[0]);
                *writerTmp 
                    << std::make_pair(fix_moment + i * 100, true)
                    << std::make_pair(fix_moment + i * 100 + 50, true);
                CPPUNIT_ASSERT( !writerTmp->isDead() );
            }

            auto bounds = readerBbx.getBoundStamp();
            CPPUNIT_ASSERT_EQUAL( bounds.first .getTime(), fix_moment );
            CPPUNIT_ASSERT_EQUAL( bounds.second.getTime(), fix_moment + i * 100 + 50 );
        }
    }
}

// Проверяем требование 3.2.1 - файл начинается опорной записью
void TC_Bbx::Req321_SupportStartFile()
{
    using namespace Bbx;
    const int STEPS = 500;      // выполнить столько операций записи
    const int SZ_PAGE = 200;    // страница ящика
    const int NUM_PAGE = 7;     // страниц в файле

    auto writerBbx = Bbx::Writer::create(BbxLocation[0]);
    CPPUNIT_ASSERT(writerBbx);
    writerBbx->setPageSize(SZ_PAGE);
    writerBbx->setRecomendedFileSize(SZ_PAGE * NUM_PAGE);
    
    // первая и ЕДИНСТВЕННАЯ опорная запись
    {
        std::string body( SZ_PAGE, 'x' );
        CPPUNIT_ASSERT(writerBbx->pushReference(Buffer(), body, fixTm(), defaultId));

        writerBbx->flush();

        auto numFiles = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
        CPPUNIT_ASSERT( 1 == numFiles );
    }
    for( int i = 0; i < STEPS; ++i )
    {
        // добавляем только инкрементные записи
        std::string bef( SZ_PAGE, 'b' );
        std::string aft( SZ_PAGE, 'a' );
        CPPUNIT_ASSERT(writerBbx->pushIncrement(Buffer(), bef, aft, fixTm(), defaultId));

        // опорная запись одна. Значит и файл может быть только один (см. ТЗ 3.2.1)
        auto numFiles = BbxLocation[0].getCPtrChain()->getNumberOfFiles();
        CPPUNIT_ASSERT( 1 == numFiles );
    }
    CPPUNIT_ASSERT( !writerBbx->isDead() );
}

// недоступность очистки устаревших файлов при чтении из них
void TC_Bbx::DisRemoveOldFiles()
{
    using namespace Bbx;
    const int SZ_PAGE = 4000;
    const int NUM_PAGE = 2;
    const size_t STEPS = 5;
    const int PERIOD = 120; // 2 мин.
    for ( size_t i = 0; i < STEPS; i++ )
    {
        VIDI( '-' );
        auto writerBbx = Bbx::Writer::create(BbxLocation[0]);
        writerBbx->setPageSize(SZ_PAGE);
        writerBbx->setRecomendedFileSize(SZ_PAGE * NUM_PAGE);
        CPPUNIT_ASSERT(!writerBbx->isDead());

        time_t t = fixTm() + i * PERIOD;
        std::string body( 2000, 'b' );
        fill_sample_with_time(body, t, i);
        
        CPPUNIT_ASSERT(writerBbx->pushReference(Buffer(), body, t, defaultId));
    }

    auto filesCount = BbxLocation[0].getCPtrChain()->getNumberOfFiles();

    // В цикле создано STEPS файлов
    CPPUNIT_ASSERT_EQUAL( STEPS, filesCount );

    Reader readerBbx( BbxLocation[0] );
    auto writerBbx = Bbx::Writer::create( BbxLocation[0] );
    writerBbx->setPageSize(SZ_PAGE);
    writerBbx->setRecomendedFileSize(SZ_PAGE * NUM_PAGE);

    // ОЧЕНЬ маленький интервал хранения
    writerBbx->setLifeTime(PERIOD);

    ReadResult readResult = readerBbx.rewind( readerBbx.getBoundStamp().first );
    CPPUNIT_ASSERT(readResult);

    for( size_t i = 0; i < STEPS; i++ )
    {
        time_t t = fixTm() + ( STEPS + i ) * PERIOD;
        std::string body( SZ_PAGE * NUM_PAGE / 2, 'c' );
        fill_sample_with_time(body, t, i);
        CPPUNIT_ASSERT(writerBbx->pushReference(Buffer(), body, t, defaultId));

        writerBbx->flush();

        auto files2Count = BbxLocation[0].getCPtrChain()->getNumberOfFiles();

        CPPUNIT_ASSERT_MESSAGE("Читатель стоит на начале ЧЯ, поэтому файлы не удаляются и копятся", STEPS + 1u + i == files2Count);
    }

    for( size_t i = 0; i < STEPS; i++ )
    {
        ReadResult nextResult = readerBbx.next();
        CPPUNIT_ASSERT( ReadResult::NewSession == nextResult );
        nextResult = readerBbx.forceNext();
        CPPUNIT_ASSERT( ReadResult::Success == nextResult );

        time_t t = fixTm() + ( STEPS * 2 + i ) * PERIOD;
        std::string body( SZ_PAGE * NUM_PAGE / 2, 'c' );
        fill_sample_with_time(body, t, i);

        CPPUNIT_ASSERT(writerBbx->pushReference(Buffer(), body, t, defaultId));

        writerBbx->flush();

        auto files3count = BbxLocation[0].getCPtrChain()->getNumberOfFiles();

        // Читатель читает файлы один за другим, писать пишет файлы один за другим,
        // Файлы удаляются пока параллельно и их число не изменяется
        CPPUNIT_ASSERT( STEPS * 2u == files3count );
    }
    CPPUNIT_ASSERT( !writerBbx->isDead() );
}

void TC_Bbx::ReaderSeeWriter()
{
    using namespace Bbx;
    // создаем читателя и проверяем что писателя не видно
    Reader readerBbx(BbxLocation[0]);
    CPPUNIT_ASSERT( !readerBbx.existActualWriter() );

    {
        // читатель знает о писателе
        auto writer1 = Bbx::Writer::create(BbxLocation[0]);
        CPPUNIT_ASSERT( readerBbx.existActualWriter() );
    }
    // читатель знает, что писатель исчез
    CPPUNIT_ASSERT( !readerBbx.existActualWriter() );
}

void TC_Bbx::SmallRecordsSupport()
{
	using namespace Bbx;

    auto bbx = Bbx::Writer::create(BbxLocation[0]);
	bbx->setRecomendedFileSize(1024);
	bbx->setPageSize(256); // Меньше 256 поставить размер страницы нельзя

	/*
	8 = 4 + 4 - Длины размеров буферов заголовка и данных опорной записи
	36 = 20 + 16 - Длины заголовка и дополнительной секции страницы
	14 - Длина заголовка куска сообщения
	1 - Длина заголовка
	11 - Недостаточное место для размещения сообщения в странице
	*/
	char_vec header(1, '1'), data(256 - 8 - 36 - 14 - 11 - 1, 'd');

	CPPUNIT_ASSERT(bbx->pushReference(header, data, fix_moment, defaultId));
    
	/* До ревизии 51718 добавление записи этой длины не провоцировало заполнение остатка
       страницы нулями и при последующем за записью в файл update'е ЧЯ вызывался ASSERT */
	bbx->flush();
}

void TC_Bbx::SmallRecordsReading()
{
	using namespace Bbx;

	auto bbx = Bbx::Writer::create(BbxLocation[0]);
	bbx->setRecomendedFileSize(1024);
	bbx->setPageSize(256); // Меньше 256 поставить размер страницы нельзя

	/*
	36 = 20 + 16 - Длины заголовка и дополнительной секции страницы
	8 = 4 + 4 - Длины размеров буферов заголовка и данных опорной записи
	14 - Длина заголовка куска сообщения
	*/
	char_vec header(256 - 36 - 8 - 14, 'h'), data(4, 'd');

	CPPUNIT_ASSERT(bbx->pushReference(header, data, fix_moment, defaultId));
	CPPUNIT_ASSERT(bbx->pushReference(header, data, fix_moment + 1, defaultId));
    bbx->flush();

	Reader readBbx(BbxLocation[0]);
	CPPUNIT_ASSERT(readBbx.rewind(fix_moment));

	Stamp tempStamp;
	CPPUNIT_ASSERT(readBbx.readReference(tempStamp, header, data));
	CPPUNIT_ASSERT(readBbx.next());
	CPPUNIT_ASSERT(readBbx.readReference(tempStamp, header, data));
}

void TC_Bbx::SmallIncrementRecordsSlicing()
{
    using namespace Bbx;

    auto out = Bbx::Writer::create(BbxLocation[0]);
    out->setRecomendedFileSize(1024);
    out->setPageSize(256); // Меньше 256 поставить размер страницы нельзя

    /*
    36 = 20 + 16 - Длины заголовка и дополнительной секции страницы
    8 = 4 + 4 - Длины размеров буферов заголовка и данных опорной записи
    14 - Длина заголовка куска сообщения
    */
    {
        char_vec header(10, 'h'), data(150, 'd');
        CPPUNIT_ASSERT(out->pushReference(header, data, fix_moment, defaultId));
    }

    {
        char_vec header(10, 'h'), data(30, 'd');
        CPPUNIT_ASSERT(out->pushIncrement(header, data, data, fix_moment + 1, defaultId));
    }

    {
        char_vec header(10, 'h'), data(150, 'd');
        CPPUNIT_ASSERT(out->pushReference(header, data, fix_moment + 2, defaultId));
    }

    out->flush();
    
    Reader reader(BbxLocation[0]);
    CPPUNIT_ASSERT_EQUAL(ReadResult::Success, reader.rewind(fix_moment + 2).get());
    reader.setDirection(false);

    CPPUNIT_ASSERT(Bbx::RecordType::Reference == reader.getCurrentType());
    CPPUNIT_ASSERT_EQUAL(fix_moment + 2, reader.getCurrentStamp().getTime());

    CPPUNIT_ASSERT_EQUAL(ReadResult::Success, reader.next().get());
    CPPUNIT_ASSERT(Bbx::RecordType::Reference != reader.getCurrentType());
    CPPUNIT_ASSERT_EQUAL(fix_moment + 1, reader.getCurrentStamp().getTime());

    CPPUNIT_ASSERT_EQUAL(ReadResult::Success, reader.next().get());
    CPPUNIT_ASSERT(Bbx::RecordType::Reference == reader.getCurrentType());
    CPPUNIT_ASSERT_EQUAL(fix_moment, reader.getCurrentStamp().getTime());

    CPPUNIT_ASSERT_EQUAL(ReadResult::NoDataAvailable, reader.next().get());
}

void TC_Bbx::IdentifierArifmetics()
{
    Identifier::Source source = Identifier::Source::HaronInput;
    {
        // Базовые операции
        Identifier id(source);
        CPPUNIT_ASSERT_EQUAL(id.getSource(), source);
        CPPUNIT_ASSERT_EQUAL(id.getId(), 0u);
        id.increment();
        CPPUNIT_ASSERT_EQUAL(id.getSource(), source);
        CPPUNIT_ASSERT_EQUAL(id.getId(), 1u);

        // Постфиксный и префиксный инкремент
        CPPUNIT_ASSERT_EQUAL((id++).getId(), 1u);
        CPPUNIT_ASSERT_EQUAL(id.getId(), 2u);
        CPPUNIT_ASSERT_EQUAL((++id).getId(), 3u);
    }

    source = Identifier::Source::HaronInput;
    {
        // Проверка на переполнение
        Identifier id(source);
        id.unsafeSet(id.getMaximumValue());
        CPPUNIT_ASSERT_EQUAL(id.getSource(), source);
        CPPUNIT_ASSERT_EQUAL(id.getId(), id.getMaximumValue());

        ++id;
        CPPUNIT_ASSERT_EQUAL(id.getSource(), source);
        CPPUNIT_ASSERT_EQUAL(id.getId(), 0u);
    }
}

void TC_Bbx::PushStressTest()
{
    auto writer = Bbx::Writer::create(BbxLocation[0]);
    writer->setPageSize(256u);
    Bbx::char_vec header(4, 'h'), data(8, 'd');

    for (int i = 0; i < 2048; ++i)
    {
        CPPUNIT_ASSERT(writer->pushReference(header, data, fix_moment, defaultId));
    }
}

void TC_Bbx::fill_sample_with_time( std::string &body, time_t t, size_t i )
{
    boost::format fmt("%-24s add(%d - %lld) ");
    fmt % time_to_iso( t )
        % i % t;
    std::string body2 = fmt.str();
    ASSERT( body.size() >= body2.size() );
    std::copy_n( body2.begin(), body2.size(), body.begin() );
}

void TC_Bbx::Compatible_NameLess()
{
    auto cmp = Bbx::Location::PathComparator();
    CPPUNIT_ASSERT( !cmp( L"Fund_151117-1340Z.fobbx", L"Fund_151117-125510Z.fobbx") );
    CPPUNIT_ASSERT( cmp(  L"Fund_151117-1340.fobbx",  L"Fund_151117-125510Z.fobbx") );
    CPPUNIT_ASSERT( !cmp( L"Fund_151117-1340.fobbx",  L"Fund_151117-125510.fobbx") );
    // строчные буквы слева
    CPPUNIT_ASSERT( !cmp( L"fund_151117-1340z.fobbx", L"Fund_151117-125510Z.fobbx") );
    CPPUNIT_ASSERT( cmp(  L"fund_151117-1340.fobbx",  L"Fund_151117-125510Z.fobbx") );
    CPPUNIT_ASSERT( !cmp( L"fund_151117-1340.fobbx",  L"Fund_151117-125510.fobbx") );
    // строчные буквы справа
    CPPUNIT_ASSERT( !cmp( L"Fund_151117-1340Z.fobbx", L"fund_151117-125510z.fobbx") );
    CPPUNIT_ASSERT( cmp(  L"Fund_151117-1340.fobbx",  L"fund_151117-125510z.fobbx") );
    CPPUNIT_ASSERT( !cmp( L"Fund_151117-1340.fobbx",  L"fund_151117-125510.fobbx") );
    // файлы с расширенной нумерацией из-за совпадения минут
    CPPUNIT_ASSERT(  cmp( L"Fund_151117-1340Z.fobbx",    L"Fund_151117-1340Z_01.fobbx") );
    CPPUNIT_ASSERT( !cmp( L"Fund_151117-1340Z_02.fobbx", L"Fund_151117-1340Z.fobbx") );
    CPPUNIT_ASSERT(  cmp( L"Fund_151117-1340Z_03.fobbx", L"Fund_151117-1342Z.fobbx") );
    CPPUNIT_ASSERT(  cmp( L"Fund_151117-1340Z.fobbx",    L"Fund_151117-1342Z_04.fobbx") );
}

// хранение временнОй зоны в заголовке ЧЯ
void TC_Bbx::StoreTimeZone()
{
    const std::string myTZ = "тестовая зона";
    // проверка временной зоны живого (записываемого) ящика
    {
        auto bOut = Bbx::Writer::create( BbxLocation[0] );
        bOut->setTimeZone(myTZ);
        bOut->pushReference( std::string(), std::string("ref text"), fixTm()+1, defaultId );
        Reader bIn( BbxLocation[0] );
        auto t1 = bIn.getTimeZone();
        CPPUNIT_ASSERT( t1 == myTZ );
    }
    // проверка временной зоны ранее записанного ящика
    {
        Reader bInNext( BbxLocation[0] );
        auto t2 = bInNext.getTimeZone();
        CPPUNIT_ASSERT( t2 == myTZ );
    }
    // проверка чтения отсутствующей временной зоны
    {
        auto bOut = Bbx::Writer::create( BbxLocation[0] );
        bOut->pushReference( std::string(), std::string("ref text"), fixTm()+2, defaultId );
    }
    {
        Reader bIn( BbxLocation[0] );
        auto t3 = bIn.getTimeZone();
        CPPUNIT_ASSERT( t3.empty() );
    }
}
