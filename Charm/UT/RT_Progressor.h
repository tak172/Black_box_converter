#ifndef _RT_PROGRESSOR_H_
#define _RT_PROGRESSOR_H_

#include <cppunit/TestListener.h>


//CPPUNIT_NS_BEGIN
class WatchDog;

/*! 
 * \brief TestListener that show the status of each TestCase test result.
 * \ingroup TrackingTestExecution
 */
class  RT_Progressor : public CPPUNIT_NS::TestListener
{
public:
  /*! Constructs a RT_Progressor object.
   */
  RT_Progressor(size_t normal_delay, bool verb, WatchDog& main_watchDog );

  /// Destructor.
  virtual ~RT_Progressor(){};

  void startTest( CPPUNIT_NS::Test *test );
  void addFailure( const CPPUNIT_NS::TestFailure &failure );
  void endTest( CPPUNIT_NS::Test *test );
  void endTestRun( CPPUNIT_NS::Test *test, 
                   CPPUNIT_NS::TestResult *eventManager );

private:
  size_t norm_delay;
  size_t tick_start;
  bool   verb_names;
  std::string test_name;
  WatchDog& watchDog;
  /// Prevents the use of the copy constructor.
  RT_Progressor( const RT_Progressor &copy );
  /// Prevents the use of the copy operator.
  void operator =( const RT_Progressor &copy );
};


// CPPUNIT_NS_END

#endif
