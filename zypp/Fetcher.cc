/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/Fetcher.cc
 *
*/
#include <iostream>
#include <fstream>
#include <list>
#include <map>

#include "zypp/base/Easy.h"
#include "zypp/base/Logger.h"
#include "zypp/base/PtrTypes.h"
#include "zypp/base/DefaultIntegral.h"
#include "zypp/base/String.h"
#include "zypp/Fetcher.h"
#include "zypp/CheckSum.h"
#include "zypp/base/UserRequestException.h"

using namespace std;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  /**
   * Class to encapsulate the \ref OnMediaLocation object
   * and the \ref FileChecker together
   */
  struct FetcherJob
  {
      
    FetcherJob( const OnMediaLocation &loc )
      : location(loc)
      , directory(false)
      , recursive(false)
    {
      //MIL << location << endl;
    }

    ~FetcherJob()
    {
      //MIL << location << " | * " << checkers.size() << endl;
    }

    OnMediaLocation location;
    //CompositeFileChecker checkers;
    list<FileChecker> checkers;
    bool directory;
    bool recursive;      
  };

  typedef shared_ptr<FetcherJob> FetcherJob_Ptr;

  std::ostream & operator<<( std::ostream & str, const FetcherJob_Ptr & obj )
  {
    return str << obj->location;
  }


  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Fetcher::Impl
  //
  /** Fetcher implementation. */
  class Fetcher::Impl
  {
    friend std::ostream & operator<<( std::ostream & str, const Fetcher::Impl & obj );
  public:
    Impl() {}
    ~Impl() {
      MIL << endl;
     }
    
      void enqueue( const OnMediaLocation &resource, const FileChecker &checker = FileChecker()  );
      void enqueueDir( const OnMediaLocation &resource, bool recursive, const FileChecker &checker = FileChecker() );
      void enqueueDigested( const OnMediaLocation &resource, const FileChecker &checker = FileChecker() );
    void addCachePath( const Pathname &cache_dir );
    void reset();
    void start( const Pathname &dest_dir,
                MediaSetAccess &media,
                const ProgressData::ReceiverFnc & progress_receiver );

    /** Offer default Impl. */
    static shared_ptr<Impl> nullimpl()
    {
      static shared_ptr<Impl> _nullimpl( new Impl );
      return _nullimpl;
    }
  private:
      /**
       * tries to provide the file represented by job into dest_dir by
       * looking at the cache. If success, returns true, and the desired
       * file should be available on dest_dir
       */
      bool provideFromCache( const OnMediaLocation &resource, const Pathname &dest_dir );
      /**
       * Validates the job against is checkers, by using the file instance
       * on dest_dir
       * \throws Exception
       */
      void validate( const OnMediaLocation &resource, const Pathname &dest_dir, const list<FileChecker> &checkers );
      
      /**
       * scan the directory and adds the individual jobs
       */
          void addDirJobs( MediaSetAccess &media, const OnMediaLocation &resource,
                       const Pathname &dest_dir, bool recursive );
      /**
       * Provide the resource to \ref dest_dir
       */
      void provideToDest( MediaSetAccess &media, const OnMediaLocation &resource, const Pathname &dest_dir );

  private:
    friend Impl * rwcowClone<Impl>( const Impl * rhs );
    /** clone for RWCOW_pointer */
    Impl * clone() const
    { return new Impl( *this ); }

    std::list<FetcherJob_Ptr> _resources;
    std::list<Pathname> _caches;
  };
  ///////////////////////////////////////////////////////////////////

  void Fetcher::Impl::enqueueDigested( const OnMediaLocation &resource, const FileChecker &checker )
  {
    FetcherJob_Ptr job;
    job.reset(new FetcherJob(resource));
    ChecksumFileChecker digest_check(resource.checksum());
    job->checkers.push_back(digest_check);
    if ( checker )
      job->checkers.push_back(checker);
    _resources.push_back(job);
  }
    
  void Fetcher::Impl::enqueueDir( const OnMediaLocation &resource, 
                                  bool recursive,
                                  const FileChecker &checker )
  {
    FetcherJob_Ptr job;
    job.reset(new FetcherJob(resource));
    if ( checker )
        job->checkers.push_back(checker);
    job->directory = true;
    job->recursive = recursive;
    _resources.push_back(job);
  }  

  void Fetcher::Impl::enqueue( const OnMediaLocation &resource, const FileChecker &checker )
  {
    FetcherJob_Ptr job;
    job.reset(new FetcherJob(resource));
    if ( checker )
      job->checkers.push_back(checker);
    _resources.push_back(job);
  }

  void Fetcher::Impl::reset()
  {
    _resources.clear();
  }

  void Fetcher::Impl::addCachePath( const Pathname &cache_dir )
  {
    PathInfo info(cache_dir);
    if ( info.isExist() )
    {
      if ( info.isDir() )
      {
        DBG << "Adding fetcher cache: '" << cache_dir << "'." << endl;
        _caches.push_back(cache_dir);
      }
      else
      {
        // don't add bad cache directory, just log the error
        ERR << "Not adding cache: '" << cache_dir << "'. Not a directory." << endl;
      }
    }
    else
    {
        ERR << "Not adding cache '" << cache_dir << "'. Path does not exists." << endl;
    }
    
  }

  bool Fetcher::Impl::provideFromCache( const OnMediaLocation &resource, const Pathname &dest_dir )
  {
    Pathname dest_full_path = dest_dir + resource.filename();
          
    // first check in the destination directory
    if ( PathInfo(dest_full_path).isExist() )
    {
      if ( is_checksum( dest_full_path, resource.checksum() )
           && (! resource.checksum().empty() ) )
          return true;
    }
    
    MIL << "start fetcher with " << _caches.size() << " cache directories." << endl;
    for_ ( it_cache, _caches.begin(), _caches.end() )
    {
      // does the current file exists in the current cache?
      Pathname cached_file = *it_cache + resource.filename();
      if ( PathInfo( cached_file ).isExist() )
      {
        DBG << "File '" << cached_file << "' exist, testing checksum " << resource.checksum() << endl;
         // check the checksum
        if ( is_checksum( cached_file, resource.checksum() ) && (! resource.checksum().empty() ) )
        {
          // cached
          MIL << "file " << resource.filename() << " found in previous cache. Using cached copy." << endl;
          // checksum is already checked.
          // we could later implement double failover and try to download if file copy fails.
           // replicate the complete path in the target directory
          if( dest_full_path != cached_file )
          {
            if ( assert_dir( dest_full_path.dirname() ) != 0 )
              ZYPP_THROW( Exception("Can't create " + dest_full_path.dirname().asString()));

            if ( filesystem::hardlink(cached_file, dest_full_path ) != 0 )
            {
              WAR << "Can't hardlink '" << cached_file << "' to '" << dest_dir << "'. Trying copying." << endl;
              if ( filesystem::copy(cached_file, dest_full_path ) != 0 )
              {
                ERR << "Can't copy " << cached_file + " to " + dest_dir << endl;
                // try next cache
                continue;
              }
            }
          }
          // found in cache
          return true;
        }
      }
    } // iterate over caches
    return false;
  }
    
    void Fetcher::Impl::validate( const OnMediaLocation &resource, const Pathname &dest_dir, const list<FileChecker> &checkers )
  {
    // no matter where did we got the file, try to validate it:
    Pathname localfile = dest_dir + resource.filename();
    // call the checker function
    try {
      MIL << "Checking job [" << localfile << "] (" << checkers.size() << " checkers )" << endl;
      for ( list<FileChecker>::const_iterator it = checkers.begin();
            it != checkers.end();
            ++it )
      {
        if (*it)
        {
          (*it)(localfile);
        }
        else
        {
          ERR << "Invalid checker for '" << localfile << "'" << endl;
        }
      }
       
    }
    catch ( const FileCheckException &e )
    {
      ZYPP_RETHROW(e);
    }
    catch ( const Exception &e )
    {
      ZYPP_RETHROW(e);
    }
    catch (...)
    {
      ZYPP_THROW(Exception("Unknown error while validating " + resource.filename().asString()));
    }
  }

  void Fetcher::Impl::addDirJobs( MediaSetAccess &media,
                                  const OnMediaLocation &resource, 
                                  const Pathname &dest_dir, bool recursive  )
  {
      // first get the content of the directory so we can add
      // individual transfer jobs
      MIL << "Adding directory " << resource.filename() << endl;
      filesystem::DirContent content;
      media.dirInfo( content, resource.filename(), false /* dots */, resource.medianr());
      
      filesystem::DirEntry shafile, shasig, shakey;
      shafile.name = "SHA1SUMS"; shafile.type = filesystem::FT_FILE;
      shasig.name = "SHA1SUMS.asc"; shasig.type = filesystem::FT_FILE;
      shakey.name = "SHA1SUMS.key"; shakey.type = filesystem::FT_FILE;
      
      // create a new fetcher with a different state to transfer the
      // file containing checksums and its signature
      Fetcher fetcher;
      // signature checker for SHA1SUMS. We havent got the signature from
      // the nextwork yet.
      SignatureFileChecker sigchecker;

      // now try to find the SHA1SUMS signature
      if ( find(content.begin(), content.end(), shasig) 
           != content.end() )
      {
          MIL << "found checksums signature file: " << shasig.name << endl;
          // TODO refactor with OnMediaLocation::extend or something
          fetcher.enqueue( OnMediaLocation(resource.filename() + shasig.name, resource.medianr()).setOptional(true) );
          assert_dir(dest_dir + resource.filename());
          fetcher.start( dest_dir, media );

          // if we get the signature, update the checker
          sigchecker = SignatureFileChecker(dest_dir + resource.filename() + shasig.name);
          fetcher.reset();
      }
      else
          MIL << "no signature for " << shafile.name << endl;

      // look for the SHA1SUMS.key file
      if ( find(content.begin(), content.end(), shakey) 
           != content.end() )
      {
          MIL << "found public key file: " << shakey.name << endl;
          fetcher.enqueue( OnMediaLocation(resource.filename() + shakey.name, resource.medianr()).setOptional(true) );
          assert_dir(dest_dir + resource.filename());
          fetcher.start( dest_dir, media );
          fetcher.reset();
          KeyContext context;
          sigchecker.addPublicKey(dest_dir + resource.filename() + shakey.name, context);
      }

      // look for the SHA1SUMS public key file
      if ( find(content.begin(), content.end(), shafile) 
           != content.end() )
      {
          MIL << "found checksums file: " << shafile.name << endl;
          fetcher.enqueue( OnMediaLocation(resource.filename() + shafile.name, resource.medianr()).setOptional(true) );
          assert_dir(dest_dir + resource.filename()); 
          fetcher.start( dest_dir, media );
          fetcher.reset();
      }

      // hash table to store checksums
      map<string, CheckSum> checksums;
      
      // look for the SHA1SUMS file
      if ( find(content.begin(), content.end(), shafile) != content.end() )
      {
          MIL << "found checksums file: " << shafile.name << endl;
          fetcher.enqueue( OnMediaLocation(
                               resource.filename() + shafile.name,
                               resource.medianr()).setOptional(true),
                           FileChecker(sigchecker) );
          assert_dir(dest_dir + resource.filename());
          fetcher.start( dest_dir + resource.filename(), media );
          fetcher.reset();

          // now read the SHA1SUMS file
          Pathname pShafile = dest_dir + resource.filename() + shafile.name;
          std::ifstream in( pShafile.c_str() );
          string buffer;
          if ( ! in.fail() )
          {          
              while ( getline(in, buffer) )
              {
                  vector<string> words;
                  str::split( buffer, back_inserter(words) );
                  if ( words.size() != 2 )
                      ZYPP_THROW(Exception("Wrong format for SHA1SUMS file"));
                  //MIL << "check: '" << words[0] << "' | '" << words[1] << "'" << endl;
                  if ( ! words[1].empty() )
                      checksums[words[1]] = CheckSum::sha1(words[0]);
              }
          }
          else
              ZYPP_THROW(Exception("Can't open SHA1SUMS file: " + pShafile.asString()));
      }

      for ( filesystem::DirContent::const_iterator it = content.begin();
            it != content.end();
            ++it )
      {
          // skip SHA1SUMS* as they were already retrieved
          if ( str::hasPrefix(it->name, "SHA1SUMS") )
              continue;
          
          Pathname filename = resource.filename() + it->name;
          
          switch ( it->type )
          {
          case filesystem::FT_NOT_AVAIL: // old directory.yast contains no typeinfo at all
          case filesystem::FT_FILE:
          {
              CheckSum checksum;
              if ( checksums.find(it->name) != checksums.end() )
                  checksum = checksums[it->name];
                  
              // create a resource from the file
              // if checksum was not available we will have a empty
              // checksum, which will end in a validation anyway
              // warning the user that there is no checksum
              enqueueDigested(OnMediaLocation()
                              .setFilename(filename)
                              .setChecksum(checksum));
                  
              break;
          }
          case filesystem::FT_DIR: // newer directory.yast contain at least directory info
              if ( recursive )
                  addDirJobs(media, filename, dest_dir, recursive);
              break;
          default:
              // don't provide devices, sockets, etc.
              break;
          }
      }
  }

  void Fetcher::Impl::provideToDest( MediaSetAccess &media, const OnMediaLocation &resource, const Pathname &dest_dir )
  {
    bool got_from_cache = false;
      
    // start look in cache
    got_from_cache = provideFromCache(resource, dest_dir);
      
    if ( ! got_from_cache )
    {
      MIL << "Not found in cache, downloading" << endl;
	
      // try to get the file from the net
      try
      {
        Pathname tmp_file = media.provideFile(resource);
        Pathname dest_full_path = dest_dir + resource.filename();
        if ( assert_dir( dest_full_path.dirname() ) != 0 )
              ZYPP_THROW( Exception("Can't create " + dest_full_path.dirname().asString()));
        if ( filesystem::copy(tmp_file, dest_full_path ) != 0 )
        {
          ZYPP_THROW( Exception("Can't copy " + tmp_file.asString() + " to " + dest_dir.asString()));
        }

        media.releaseFile(resource); //not needed anymore, only eat space
      }
      catch (Exception & excpt_r)
      {
        ZYPP_CAUGHT(excpt_r);
        excpt_r.remember("Can't provide " + resource.filename().asString() + " : " + excpt_r.msg());
        ZYPP_RETHROW(excpt_r);
      }
    }
    else
    {
      // We got the file from cache
      // continue with next file
        return;
    }
  }  

  void Fetcher::Impl::start( const Pathname &dest_dir,
                             MediaSetAccess &media,
                             const ProgressData::ReceiverFnc & progress_receiver )
  {
    ProgressData progress(_resources.size());
    progress.sendTo(progress_receiver);

    for ( list<FetcherJob_Ptr>::const_iterator it_res = _resources.begin(); it_res != _resources.end(); ++it_res )
    { 

      if ( (*it_res)->directory )
      {
          const OnMediaLocation location((*it_res)->location);
          addDirJobs(media, location, dest_dir, true);
          continue;
      }

      provideToDest(media, (*it_res)->location, dest_dir);

      // validate job, this throws if not valid
      validate((*it_res)->location, dest_dir, (*it_res)->checkers);
      
      if ( ! progress.incr() )
        ZYPP_THROW(AbortRequestException());
    } // for each job
  }

  /** \relates Fetcher::Impl Stream output */
  inline std::ostream & operator<<( std::ostream & str, const Fetcher::Impl & obj )
  {
      for ( list<FetcherJob_Ptr>::const_iterator it_res = obj._resources.begin(); it_res != obj._resources.end(); ++it_res )
      { 
          str << *it_res;
      }
      return str;
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Fetcher
  //
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Fetcher::Fetcher
  //	METHOD TYPE : Ctor
  //
  Fetcher::Fetcher()
  : _pimpl( new Impl() )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Fetcher::~Fetcher
  //	METHOD TYPE : Dtor
  //
  Fetcher::~Fetcher()
  {}

  void Fetcher::enqueueDigested( const OnMediaLocation &resource, const FileChecker &checker )
  {
    _pimpl->enqueueDigested(resource, checker);
  }

  void Fetcher::enqueueDir( const OnMediaLocation &resource,
                            bool recursive,
                            const FileChecker &checker )
  {
      _pimpl->enqueueDir(resource, recursive, checker);
  }

  void Fetcher::enqueue( const OnMediaLocation &resource, const FileChecker &checker  )
  {
    _pimpl->enqueue(resource, checker);
  }

  void Fetcher::addCachePath( const Pathname &cache_dir )
  {
    _pimpl->addCachePath(cache_dir);
  }

  void Fetcher::reset()
  {
    _pimpl->reset();
  }

  void Fetcher::start( const Pathname &dest_dir,
                       MediaSetAccess &media,
                       const ProgressData::ReceiverFnc & progress_receiver )
  {
    _pimpl->start(dest_dir, media, progress_receiver);
  }


  /******************************************************************
  **
  **	FUNCTION NAME : operator<<
  **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const Fetcher & obj )
  {
    return str << *obj._pimpl;
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
