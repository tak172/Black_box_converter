//
// Версия проекта Charm 
//

#ifndef CHARM_VERSION_NUM

/*
строка для генерации командного файла
SET CHARM_VERSION=1.5 build 105
!define INSTALLER "1.5 build 105"
!define INSTALL_VER "1.5.100"
*/

#define CHARM_VERSION_NUM      1,5,0,105
#define CHARM_VERSION_TEXT     1.5 build 105
#define CHARM_COPYRIGHT       L"(c) ЖелДорАвтоматизация 2010"
#define CHARM_COMPANY         L"НПО ЖелДорАвтоматизация" 
#define CHARM_PRODUCT         "Charm"

#ifdef  _WIN64
#   ifdef      _DEBUG
#       define CHARM_BUILD_TEXT      (x64 debug)
#       define CHARM_BUILD_MODE     "(x64 debug)"
#   else
#       define CHARM_BUILD_TEXT      (x64)
#       define CHARM_BUILD_MODE     "(x64)"
#   endif
#   define GUESS_DLL             "Guess64.dll"
#   define HEM_DLL                 "Hem64.dll"
#	define AUGUR_EXE             "Augur64.exe"
#   define ACTOR_EXE             "Actor64.exe"
#   define FUND_EXE               "Fund64.exe"
#   define HARON_EXE             "Haron64.exe"
#   define STATBUILDER_EXE "StatBuilder64.exe"
#   define UT_EXE                   "UT64.exe"
#   define INSIDER_EXE         "Insider64.exe"
#	define ROKIT_EXE			 "Rokit64.exe"
#	define VARGRAPH_EXE         "Client64.exe"
#	define PLUK_EXE               "Pluk64.exe"
#	define PULSAR_EXE           "Pulsar64.exe"
#	define NSIPROVIDER_EXE "NsiProvider64.exe"
#	define PHANTOM_EXE         "Phantom64.exe"
#   define ZODIAC_EXE           "Zodiac64.exe"
#   define TOWER_EXE             "Tower64.exe"
#else
#   ifdef _DEBUG
#       define CHARM_BUILD_TEXT      (debug)
#       define CHARM_BUILD_MODE     "(debug)"
#   else
#       define CHARM_BUILD_TEXT     
#       define CHARM_BUILD_MODE     ""
#   endif
#   define GUESS_DLL             "Guess.dll"
#   define HEM_DLL                 "Hem.dll"
#	define AUGUR_EXE             "Augur.exe"
#   define ACTOR_EXE             "Actor.exe"
#   define FUND_EXE               "Fund.exe"
#   define HARON_EXE             "Haron.exe"
#   define STATBUILDER_EXE "StatBuilder.exe"
#   define UT_EXE                   "UT.exe"
#   define INSIDER_EXE         "Insider.exe"
#	define ROKIT_EXE			 "Rokit.exe"
#	define VARGRAPH_EXE         "Client.exe"
#	define PLUK_EXE               "Pluk.exe"
#	define PULSAR_EXE           "Pulsar.exe"
#	define NSIPROVIDER_EXE "NsiProvider.exe"
#	define PHANTOM_EXE         "Phantom.exe"
#   define ZODIAC_EXE           "Zodiac.exe"
#   define TOWER_EXE             "Tower.exe"
#endif

#define QUOTE1(y)    #y
// cppcheck-suppress unhandledCharacters
#define CONCAT3(a,v,b)    QUOTE1(a v b)
#define STATBUILDER_VERSION   CONCAT3(StatBuilder, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)
#define HARON_______VERSION   CONCAT3(      Haron, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)
#define INSIDER_____VERSION   CONCAT3(    Insider, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)
#define   AUGUR_____VERSION   CONCAT3(      Augur, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)
#define VARGRAPH____VERSION   CONCAT3(   VarGraph, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)
#define     PLUK____VERSION   CONCAT3(       Pluk, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)
#define   PULSAR____VERSION   CONCAT3(     Pulsar, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)
#define  PHANTOM____VERSION   CONCAT3(    Phantom, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)
#define      Zodiac_VERSION   CONCAT3(     Zodiac, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)
#define    TOWER____VERSION   CONCAT3(      Tower, CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)

#define CONCAT2(v,b)        QUOTE1( v b )
#define CHARM_VERSION_STR   CONCAT2(CHARM_VERSION_TEXT, CHARM_BUILD_TEXT)

#endif //CHARM_VERSION_NUM
