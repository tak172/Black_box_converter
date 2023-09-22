#include "stdafx.h"

#include <cppunit/TestFailure.h>
#include <cppunit/TestListener.h>
#include <cppunit/Test.h>
#include "RT_Progressor.h"
#include "../helpful/WatchDog.h"

#include <cppunit/portability/Stream.h>


//CPPUNIT_NS_BEGIN

RT_Progressor::RT_Progressor( size_t normal_delay, bool verb, WatchDog& main_watchDog )
    : norm_delay(normal_delay), tick_start(0), verb_names(verb), test_name(), watchDog(main_watchDog)
{
}

void RT_Progressor::startTest( CPPUNIT_NS::Test *t )
{
    watchDog.alive();
    if ( verb_names )
    {
        size_t oldSz = test_name.size();
        test_name = t->getName();
        std::string spacer;
        if ( test_name.size() < oldSz )
            spacer.resize( oldSz - test_name.size(), ' ' );
        CPPUNIT_NS::stdCOut() << "\r" << test_name << spacer;
        CPPUNIT_NS::stdCOut().flush();
    }
    tick_start = GetTickCount();
}


void RT_Progressor::addFailure( const CPPUNIT_NS::TestFailure &failure )
{
    CPPUNIT_NS::stdCOut() << "\r" << ( failure.isError() ? "Exception" : "     Fail" )
                          << " : " << failure.failedTest()->getName() << "\n";
    CPPUNIT_NS::stdCOut().flush();
}

void RT_Progressor::endTest( CPPUNIT_NS::Test * /*test*/ )
{
    watchDog.alive();
    size_t tick_end = GetTickCount();
    if ( !verb_names )
    {
        if ( tick_end-tick_start > norm_delay )
            CPPUNIT_NS::stdCOut() << "w";
        else
            CPPUNIT_NS::stdCOut() << ".";
    }
}

void 
RT_Progressor::endTestRun( CPPUNIT_NS::Test *, 
                                      CPPUNIT_NS::TestResult * )
{
    CPPUNIT_NS::stdCOut() <<  "\r" << std::string(test_name.size(),' ') << "\r";
    CPPUNIT_NS::stdCOut().flush();
}


//CPPUNIT_NS_END

