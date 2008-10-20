/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/detail/PoolImpl.cc
 *
*/
#include <iostream>
#include <boost/mpl/int.hpp>

#include "zypp/base/Easy.h"
#include "zypp/base/LogTools.h"
#include "zypp/base/Gettext.h"
#include "zypp/base/Exception.h"
#include "zypp/base/Measure.h"
#include "zypp/base/WatchFile.h"
#include "zypp/base/Sysconfig.h"

#include "zypp/ZConfig.h"

#include "zypp/sat/detail/PoolImpl.h"
#include "zypp/sat/Pool.h"
#include "zypp/Capability.h"
#include "zypp/Locale.h"

#include "zypp/target/modalias/Modalias.h"

using std::endl;

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::satpool"

// ///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace sat
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    namespace detail
    { /////////////////////////////////////////////////////////////////

      // MPL checks for satlib constants we redefine to avoid
      // includes and defines.
      BOOST_MPL_ASSERT_RELATION( noId,                 ==, STRID_NULL );
      BOOST_MPL_ASSERT_RELATION( emptyId,              ==, STRID_EMPTY );

      BOOST_MPL_ASSERT_RELATION( noSolvableId,         ==, ID_NULL );
      BOOST_MPL_ASSERT_RELATION( systemSolvableId,     ==, SYSTEMSOLVABLE );

      BOOST_MPL_ASSERT_RELATION( solvablePrereqMarker, ==, SOLVABLE_PREREQMARKER );
      BOOST_MPL_ASSERT_RELATION( solvableFileMarker,   ==, SOLVABLE_FILEMARKER );

      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_AND,       ==, REL_AND );
      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_OR,        ==, REL_OR );
      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_WITH,      ==, REL_WITH );
      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_NAMESPACE, ==, REL_NAMESPACE );
      BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_ARCH,      ==, REL_ARCH );

     /////////////////////////////////////////////////////////////////

      static void logSat( struct _Pool *, void *data, int type, const char *logString )
      {
	  if ((type & (SAT_FATAL|SAT_ERROR))) {
	      _ERR("satsolver") << logString;
	  } else {
	      _MIL("satsolver") << logString;
	  }
      }

      detail::IdType PoolImpl::nsCallback( struct _Pool *, void * data, detail::IdType lhs, detail::IdType rhs )
      {
        // lhs:    the namespace identifier, e.g. NAMESPACE:MODALIAS
        // rhs:    the value, e.g. pci:v0000104Cd0000840[01]sv*sd*bc*sc*i*
        // return: 0 if not supportded
        //         1 if supported by the system
        //        -1  AFAIK it's also possible to return a list of solvables that support it, but don't know how.

        static const detail::IdType RET_unsupported    = 0;
        static const detail::IdType RET_systemProperty = 1;
        switch ( lhs )
        {
          case NAMESPACE_LANGUAGE:
            {
              static IdString en( "en" );
              const std::tr1::unordered_set<IdString> & locale2Solver( reinterpret_cast<PoolImpl*>(data)->_locale2Solver );
              if ( locale2Solver.empty() )
              {
                return rhs == en.id() ? RET_systemProperty : RET_unsupported;
              }
              return locale2Solver.find( IdString(rhs) ) != locale2Solver.end() ? RET_systemProperty : RET_unsupported;
            }
            break;

          case NAMESPACE_MODALIAS:
            {
              return target::Modalias::instance().query( IdString(rhs) ) ? RET_systemProperty : RET_unsupported;
            }
            break;

          case NAMESPACE_FILESYSTEM:
            {
              static const Pathname sysconfigStoragePath( "/etc/sysconfig/storage" );
              static WatchFile      sysconfigFile( sysconfigStoragePath, WatchFile::NO_INIT );
              static std::set<std::string> requiredFilesystems;
              if ( sysconfigFile.hasChanged() )
              {
                requiredFilesystems.clear();
                str::split( base::sysconfig::read( sysconfigStoragePath )["USED_FS_LIST"],
                            std::inserter( requiredFilesystems, requiredFilesystems.end() ) );
              }
              return requiredFilesystems.find( IdString(rhs).asString() ) != requiredFilesystems.end() ? RET_systemProperty : RET_unsupported;
            }
            break;
        }

        INT << "Unhandled " << Capability( lhs ) << " vs. " << Capability( rhs ) << endl;
        return RET_unsupported;
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : PoolMember::myPool
      //	METHOD TYPE : PoolImpl
      //
      PoolImpl & PoolMember::myPool()
      {
        static PoolImpl _global;
        return _global;
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : PoolImpl::PoolImpl
      //	METHOD TYPE : Ctor
      //
      PoolImpl::PoolImpl()
      : _pool( ::pool_create() )
      {
        MIL << "Creating sat-pool." << endl;
        if ( ! _pool )
        {
          ZYPP_THROW( Exception( _("Can not create sat-pool.") ) );
        }
        // initialialize logging
        bool verbose = ( getenv("ZYPP_FULLLOG") || getenv("ZYPP_LIBSAT_FULLLOG") );
	if (verbose)
	    ::pool_setdebuglevel( _pool, 2 );
	else
	    ::pool_setdebugmask(_pool, SAT_DEBUG_STATS | SAT_DEBUG_JOB);
	
        ::pool_setdebugcallback( _pool, logSat, NULL );

        // set namespace callback
        _pool->nscallback = &nsCallback;
        _pool->nscallbackdata = (void*)this;
      }

      ///////////////////////////////////////////////////////////////////
      //
      //	METHOD NAME : PoolImpl::~PoolImpl
      //	METHOD TYPE : Dtor
      //
      PoolImpl::~PoolImpl()
      {
        ::pool_free( _pool );
      }

     ///////////////////////////////////////////////////////////////////

      void PoolImpl::setDirty( const char * a1, const char * a2, const char * a3 )
      {
        if ( a1 )
        {
          if      ( a3 ) MIL << a1 << " " << a2 << " " << a3 << endl;
          else if ( a2 ) MIL << a1 << " " << a2 << endl;
          else           MIL << a1 << endl;
        }
        _serial.setDirty();           // pool content change
        _availableLocalesPtr.reset(); // available locales may change

        // invaldate dependency/namespace related indices:
        depSetDirty();
      }

      void PoolImpl::depSetDirty( const char * a1, const char * a2, const char * a3 )
      {
        if ( a1 )
        {
          if      ( a3 ) MIL << a1 << " " << a2 << " " << a3 << endl;
          else if ( a2 ) MIL << a1 << " " << a2 << endl;
          else           MIL << a1 << endl;
        }
        ::pool_freewhatprovides( _pool );
      }

      void PoolImpl::prepare() const
      {
        if ( _watcher.remember( _serial ) )
        {
          // After repo/solvable add/remove:
          // set pool architecture
          ::pool_setarch( _pool,  ZConfig::instance().systemArchitecture().asString().c_str() );
        }
        if ( ! _pool->whatprovides )
        {
          MIL << "pool_createwhatprovides..." << endl;

          // NOTE: Thake care not to ctreate a nonexisting systemRepo
          Repository sysrepo( sat::Pool::instance().reposFind( sat::Pool::instance().systemRepoAlias() ) );
          if ( sysrepo )
          {
            ::pool_addfileprovides( _pool, sysrepo.get() );
          }
          ::pool_createwhatprovides( _pool );
        }
        if ( ! _pool->languages )
        {
          std::vector<std::string> fallbacklist;
          for ( Locale l( ZConfig::instance().textLocale() ); l != Locale::noCode; l = l.fallback() )
          {
            fallbacklist.push_back( l.code() );
          }
          dumpRangeLine( MIL << "pool_set_languages: ", fallbacklist.begin(), fallbacklist.end() ) << endl;

          std::vector<const char *> fallbacklist_cstr;
          for_( it, fallbacklist.begin(), fallbacklist.end() )
          {
            fallbacklist_cstr.push_back( it->c_str() );
          }
          ::pool_set_languages( _pool, &fallbacklist_cstr.front(), fallbacklist_cstr.size() );
        }
      }

      ///////////////////////////////////////////////////////////////////

      int PoolImpl::_addSolv( ::_Repo * repo_r, FILE * file_r, bool isSystemRepo_r )
      {
        setDirty(__FUNCTION__, repo_r->name );
        int ret = ::repo_add_solv( repo_r , file_r  );
        if ( ret == 0 && ! isSystemRepo_r )
        {
          // Filter out unwanted archs
          std::set<detail::IdType> sysids;
          {
            Arch::CompatSet sysarchs( Arch::compatSet( ZConfig::instance().systemArchitecture() ) );
            for_( it, sysarchs.begin(), sysarchs.end() )
	       sysids.insert( it->id() );

            // unfortunately satsolver treats src/nosrc as architecture:
            sysids.insert( ARCH_SRC );
            sysids.insert( ARCH_NOSRC );
          }

          detail::IdType blockBegin = 0;
          unsigned       blockSize  = 0;
          for ( detail::IdType i = repo_r->start; i < repo_r->end; ++i )
          {
            ::_Solvable * s( _pool->solvables + i );
            if ( s->repo == repo_r && sysids.find( s->arch ) == sysids.end() )
            {
              // Remember an unwanted arch entry:
              if ( ! blockBegin )
                blockBegin = i;
              ++blockSize;
            }
            else if ( blockSize )
            {
              // Free remembered entries
              ::repo_free_solvable_block( repo_r, blockBegin, blockSize, /*reuseids*/false );
              blockBegin = blockSize = 0;
            }
          }
          if ( blockSize )
          {
            // Free remembered entries
            ::repo_free_solvable_block( repo_r, blockBegin, blockSize, /*reuseids*/false );
            blockBegin = blockSize = 0;
          }
        }
        return ret;
      }

      ///////////////////////////////////////////////////////////////////

      // need on demand and id based Locale
      void _locale_hack( const LocaleSet & locales_r,
                         std::tr1::unordered_set<IdString> & locale2Solver )
      {
        std::tr1::unordered_set<IdString>( 2*locales_r.size() ).swap( locale2Solver );
        for_( it, locales_r.begin(),locales_r.end() )
        {
          for ( Locale l( *it ); l != Locale::noCode; l = l.fallback() )
            locale2Solver.insert( IdString( l.code() ) );
        }
        MIL << "New Solver Locales: " << locale2Solver << endl;
      }

      void PoolImpl::setRequestedLocales( const LocaleSet & locales_r )
      {
        depSetDirty( "setRequestedLocales" );
        _requestedLocales = locales_r;
        MIL << "New RequestedLocales: " << locales_r << endl;
        _locale_hack( _requestedLocales, _locale2Solver );
      }

      bool PoolImpl::addRequestedLocale( const Locale & locale_r )
      {
        if ( _requestedLocales.insert( locale_r ).second )
        {
          depSetDirty( "addRequestedLocale", locale_r.code().c_str() );
          _locale_hack( _requestedLocales, _locale2Solver );
          return true;
        }
        return false;
      }

      bool PoolImpl::eraseRequestedLocale( const Locale & locale_r )
      {
        if ( _requestedLocales.erase( locale_r ) )
        {
          depSetDirty( "addRequestedLocale", locale_r.code().c_str() );
          _locale_hack( _requestedLocales, _locale2Solver );
          return true;
        }
        return false;
      }

      static void _getLocaleDeps( Capability cap_r, std::tr1::unordered_set<sat::detail::IdType> & store_r )
      {
        // Collect locales from any 'namespace:language(lang)' dependency
        CapDetail detail( cap_r );
        if ( detail.kind() == CapDetail::EXPRESSION )
        {
          switch ( detail.capRel() )
          {
            case CapDetail::CAP_AND:
            case CapDetail::CAP_OR:
              // expand
              _getLocaleDeps( detail.lhs(), store_r );
              _getLocaleDeps( detail.rhs(), store_r );
              break;

            case CapDetail::CAP_NAMESPACE:
              if ( detail.lhs().id() == NAMESPACE_LANGUAGE )
              {
                store_r.insert( detail.rhs().id() );
              }
              break;

            case CapDetail::REL_NONE:
            case CapDetail::CAP_WITH:
            case CapDetail::CAP_ARCH:
              break; // unwanted
          }
        }
      }

      const LocaleSet & PoolImpl::getAvailableLocales() const
      {
        if ( !_availableLocalesPtr )
        {
          // Collect any 'namespace:language(ja)' dependencies
          std::tr1::unordered_set<sat::detail::IdType> tmp;
          Pool pool( Pool::instance() );
          for_( it, pool.solvablesBegin(), pool.solvablesEnd() )
          {
            Capabilities cap( it->supplements() );
            for_( cit, cap.begin(), cap.end() )
            {
              _getLocaleDeps( *cit, tmp );
            }
          }
#warning immediately build LocaleSet as soon as Loale is an Id based type
          _availableLocalesPtr.reset( new LocaleSet(tmp.size()) );
          for_( it, tmp.begin(), tmp.end() )
          {
            _availableLocalesPtr->insert( Locale( IdString(*it) ) );
          }
        }
        return *_availableLocalesPtr;
      }

      /////////////////////////////////////////////////////////////////
    } // namespace detail
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////
  } // namespace sat
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////