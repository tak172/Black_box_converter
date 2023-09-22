#ifndef TC_BBX_H
#define TC_BBX_H
#include <atomic>
#include <cppunit/extensions/HelperMacros.h>
#include "../BlackBox/bbx_BlackBox.h"
/* 
 * Класс для проверки работы классов BbxIn и BbxOut
 */
namespace Bbx { class Location; };

class TC_Bbx : public CPPUNIT_NS::TestFixture
{
  CPPUNIT_TEST_SUITE( TC_Bbx );

  CPPUNIT_TEST(Create);                  /* Проверка всех условий создания и пересоздания */
  CPPUNIT_TEST(Search);                  /* Проверка результатов поиска опорной записи */
  CPPUNIT_TEST(RemoveOldFiles);          /* Проверка удаления устаревших файлов */
  CPPUNIT_TEST(ConstrainSpace);          /* Соблюдение ограничений места на диске */
  CPPUNIT_TEST(ConstrainTime);           /* Соблюдение ограничений по интервалу хранения */
  CPPUNIT_TEST(ConstrainValue);          /* Разбор ограничений */
  CPPUNIT_TEST(ContinuousTruncation);    /* В цикле 60 ожиданий по 0.5 секунды */
  CPPUNIT_TEST(Records);                 /* Проверка записи данных разного размера */
  CPPUNIT_TEST(Silence);
  CPPUNIT_TEST(AsyncStressTest);         /* Замер скорости работы при большом числе читателей */
  CPPUNIT_TEST(Lacuna);                  /* Проверка обнаружения лакун */
  CPPUNIT_TEST(ReadBorder);              /* Проверка чтения границ */
  CPPUNIT_TEST(Req321_SupportStartFile); /* Проверка начала файла только с опорной записи */
  CPPUNIT_TEST(DisRemoveOldFiles);       /* Проверка неудаления старых файлов при чтении */ 
  CPPUNIT_TEST(SizeParameters);          // Добавление записи, размер которой превышает размер файла 
  CPPUNIT_TEST( FileNames_recreate );    // Создание множества файлов с одинаковым штампом 
  CPPUNIT_TEST( FileNames_continuous );  // Создание множества файлов с одинаковым штампом 
  CPPUNIT_TEST(ReaderSeeWriter);         /* Проверка знаний читателя о наличии писателя */
  CPPUNIT_TEST(SmallRecordsSupport);     /* Проверка на поддержку экстремумных значений длин сообщений */
  CPPUNIT_TEST(SmallRecordsReading);     /* Проверка на поддержку чтения обрезанных страницей сообщений */
  CPPUNIT_TEST(SmallIncrementRecordsSlicing); /* Исправление проблем с чтение порезанных инкрементных записей */
  CPPUNIT_TEST(IdentifierArifmetics);    /* Проверка идентификатора */
  CPPUNIT_TEST(PushStressTest);
  CPPUNIT_TEST(Compatible_NameLess);
  CPPUNIT_TEST(StoreTimeZone);
  CPPUNIT_TEST_SUITE_END();

public:
    virtual void setUp();
    virtual void tearDown();

    static size_t DeleteBlackBoxFiles(const Bbx::Location& location);

protected:
	void Create();
    void Records();
    void Search();
    void Silence();
    void AsyncStressTest();
    void FileNames_recreate();   // несколько сеансов записи
    void FileNames_continuous(); // непрерывная запись в одном сеансе
    void SizeParameters();
	void RemoveOldFiles();// очистка устаревших файлов
    void ConstrainSpace(); // ограничение места на диске
    void ConstrainTime();  // ограничение времени хранения
    void ConstrainValue(); // разбор выражений

    void fill_sample_with_time( std::string &body, time_t t, size_t i );
    void ContinuousTruncation();
	void DisRemoveOldFiles();// недоступность очистки устаревших файлов при чтении из них
	void ReadBorder();// граница чтения
    void Req321_SupportStartFile();
    void Lacuna();
    void ReaderSeeWriter();
	void SmallRecordsSupport(); // Исправление проблем с записью малых сообщений
	void SmallRecordsReading(); // Исправление проблем с чтением сообщений специфичной длины
    void SmallIncrementRecordsSlicing(); // Исправление проблем с чтение порезанных инкрементных записей
    void IdentifierArifmetics();
    void PushStressTest();
    void Compatible_NameLess(); // совместимость сравнения имен файлов со старой версией
    void StoreTimeZone();   // сохранение временной зоны в заголовке файла
private:
    static time_t fixTm();

    void write_sup( Bbx::Writer& bOut, time_t t, const Bbx::char_vec& support ) const;
    void write_inc( Bbx::Writer& bOut, Bbx::char_vec*, time_t t, const Bbx::char_vec& before, const Bbx::char_vec& after_ ) const;
    static Bbx::char_vec minus( const Bbx::char_vec& total, const Bbx::char_vec &del );
    static Bbx::char_vec plus( const Bbx::char_vec& total, const Bbx::char_vec& add );
    Bbx::char_vec Req25_write_helper( const Bbx::Location& loc, const Bbx::char_vec& absolute_b );
    void Req25_read_helper( const Bbx::Location& loc, const bool direction,
                            const Bbx::char_vec& relat_b, const Bbx::char_vec& relat_e );
    void search_pause( Bbx::Writer &out_bbx );
    void search_addSupport( int shift, Bbx::Writer &out_bbx, std::vector<int> &supp );
    void addSupport( time_t moment, Bbx::Writer &out_bbx );
    std::vector<int> search_make_checkpoint( const std::vector<int>& supp );


private:
    time_t fix_moment;
    static const size_t BBX_COUNT = 3; 
    Bbx::Location BbxLocation[BBX_COUNT];
    Bbx::Identifier defaultId;
    boost::posix_time::time_duration restore_delay;
};

#endif // TC_BBX_H