/*
 *   File name:	kdirtree.h
 *   Summary:	Support classes for KDirStat
 *   License:	LGPL - See file COPYING.LIB for details.
 *   Author:	Stefan Hundhammer <sh@suse.de>
 *
 *   Updated:	2001-06-17
 *
 *   $Id: kdirtree.cpp,v 1.1 2001/06/29 16:37:49 hundhammer Exp $
 *
 */


#include <string.h>
#include <sys/errno.h>
#include <qtimer.h>
#include <kdebug.h>
#include <kapp.h>
#include <klocale.h>
#include "kdirtree.h"
#include "kdirtreeview.h"
#include "kdirsaver.h"
#include "kio/job.h"


using namespace KDirStat;


KFileInfo::KFileInfo(  KFileInfo *	parent,
		       const char *	name )
    : _parent( parent )
    , _next( 0 )
{
    _isLocalFile = true;
    _name	 = name ? name : "";
    _device	 = 0;
    _mode	 = 0;
    _links	 = 0;
    _size	 = 0;
    _blocks	 = 0;
    _mtime	 = 0;
}


KFileInfo::KFileInfo( const QString &	filenameWithoutPath,
		      struct stat *	statInfo,
		      KFileInfo	  *	parent )
    : _parent( parent )
    , _next( 0 )
{
    CHECK_PTR( statInfo );

    _isLocalFile = true;
    _name	 = filenameWithoutPath;

    _device	 = statInfo->st_dev;
    _mode	 = statInfo->st_mode;
    _links	 = statInfo->st_nlink;
    _size	 = statInfo->st_size;
    _blocks	 = statInfo->st_blocks;
    _mtime	 = statInfo->st_mtime;
}


KFileInfo::KFileInfo(  const KFileItem	* fileItem,
		       KFileInfo	* parent )
    : _parent( parent )
    , _next( 0 )
{
    CHECK_PTR( fileItem );

    _isLocalFile = fileItem->isLocalFile();
    _name	 = parent ? fileItem->name() : fileItem->url().url();
    _device	 = 0;
    _mode	 = fileItem->mode();
    _links	 = 1;
    _size	 = fileItem->size();
    _blocks	 = _size / blockSize();

    if ( ( _size % blockSize() ) > 0 )
	_blocks++;

    _mtime	 = fileItem->time( KIO::UDS_MODIFICATION_TIME );
}


KFileInfo::~KFileInfo()
{
    // NOP
}


QString
KFileInfo::url() const
{
    if ( _parent )
    {
	QString parentUrl = _parent->url();

	if ( isDotEntry() )	// don't append "/." for dot entries
	    return parentUrl;

	if ( parentUrl == "/" ) // avoid duplicating slashes
	    return parentUrl + _name;
	else
	    return parentUrl + "/" + _name;
    }
    else
	return _name;
}


QString
KFileInfo::debugUrl() const
{
    return url() + ( isDotEntry() ? "/<Files>" : "" );
}


QString
KFileInfo::urlPart( int targetLevel ) const
{
    int level = treeLevel();	// Cache this - it's expensive!

    if ( level < targetLevel )
    {
	kdError() << k_funcinfo << "URL level " << targetLevel
		  << " requested, this is level " << level << endl;
	return "";
    }

    const KFileInfo *item = this;

    while ( level > targetLevel )
    {
	level--;
	item = item->parent();
    }

    return item->name();
}


int
KFileInfo::treeLevel() const
{
    int		level	= 0;
    KFileInfo *	parent	= _parent;

    while ( parent )
    {
	level++;
	parent = parent->parent();
    }

    return level;


    if ( _parent )
	return _parent->treeLevel() + 1;
    else
	return 0;
}


bool
KFileInfo::hasChildren() const
{
    return firstChild() || dotEntry();
}


KFileInfo *
KFileInfo::locate( QString url )
{
    if ( ! url.startsWith( _name ) )
	return 0;
    else					// URL starts with this node's name
    {
	url.remove( 0, _name.length() );	// Remove leading name of this node

	if ( url.length() == 0 )		// Nothing left?
	    return this;			// Hey! That's us!

	if ( url.startsWith( "/" ) )		// If the next thing a path delimiter,
	    url.remove( 0, 1 );			// remove that leading delimiter.
	else					// No path delimiter at the beginning
	{
	    if ( _name.right(1) != "/" &&	// and this is not the root directory
		 ! isDotEntry() )		// or a dot entry:
		return 0;			// This can't be any of our children.
	}


	// Search all children

	KFileInfo *child = firstChild();

	while ( child )
	{
	    KFileInfo *foundChild = child->locate( url );

	    if ( foundChild )
		return foundChild;
	    else
		child = child->next();
	}


	// Search the dot entry if there is one - but only if there is no more
	// path delimiter left in the URL. The dot entry contains files only,
	// and their names may not contain the path delimiter, nor can they
	// have children. This check is not strictly necessary, but it may
	// speed up things a bit if we don't search the non-directory children
	// if the rest of the URL consists of several pathname components.

	if ( dotEntry() &&
	     url.find ( "/" ) < 0 )	// No (more) "/" in this URL
	{
	    return dotEntry()->locate( url );
	}
    }

    return 0;
}






KDirInfo::KDirInfo( KFileInfo * parent,
		    bool	asDotEntry )
    : KFileInfo( parent )
{
    init();

    if ( asDotEntry )
    {
	_isDotEntry	= true;
	_dotEntry	= 0;
	_name		= ".";
    }
    else
    {
	_isDotEntry	= false;
	_dotEntry	= new KDirInfo( this, true );
    }
}


KDirInfo::KDirInfo( const QString &	filenameWithoutPath,
		    struct stat	*	statInfo,
		    KFileInfo	*	parent )
    : KFileInfo( filenameWithoutPath,
		 statInfo,
		 parent )
{
    init();
    _dotEntry	= new KDirInfo( this, true );
}


KDirInfo::KDirInfo( const KFileItem	* fileItem,
		    KFileInfo		* parent )
    : KFileInfo( fileItem,
		 parent )
{
    init();
    _dotEntry	= new KDirInfo( this, true );
}


void
KDirInfo::init()
{
    _isDotEntry		= false;
    _pendingReadJobs	= 0;
    _dotEntry		= 0;
    _firstChild		= 0;
    _totalSize		= _size;
    _totalBlocks	= _blocks;
    _totalItems		= 0;
    _totalSubDirs	= 0;
    _totalFiles		= 0;
    _latestMtime	= _mtime;
    _summaryDirty	= false;
    _readState		= KDirQueued;
}


KDirInfo::~KDirInfo()
{
    KFileInfo	*child = _firstChild;


    // Recursively delete all children.

    while ( child )
    {
	KFileInfo * nextChild = child->next();
	delete child;
	child = nextChild;
    }


    // Delete the dot entry.

    if ( _dotEntry )
    {
	delete _dotEntry;
    }
}


void
KDirInfo::recalc()
{
    _totalSize		= _size;
    _totalBlocks	= _blocks;
    _totalItems		= 0;
    _totalSubDirs	= 0;
    _totalFiles		= 0;
    _latestMtime	= _mtime;

    if ( _dotEntry )
	( dynamic_cast<KDirInfo *>( _dotEntry ) )->recalc();

    KFileInfo * child = _firstChild;

    while ( child )
    {
	_totalSize	+= child->totalSize();
	_totalBlocks	+= child->totalBlocks();
	_totalItems	+= child->totalItems() + 1;
	_totalSubDirs	+= child->totalSubDirs();
	_totalFiles	+= child->totalFiles();

	if ( child->isDir() )
	    _totalSubDirs++;

	if ( child->isFile() )
	    _totalFiles++;

	time_t childLatestMtime = child->latestMtime();

	if ( childLatestMtime > _latestMtime )
	    _latestMtime = childLatestMtime;

	child = child->next();
    }

    _summaryDirty = false;
}


KFileSize
KDirInfo::totalSize()
{
    if ( _summaryDirty )
	recalc();

    return _totalSize;
}


KFileSize
KDirInfo::totalBlocks()
{
    if ( _summaryDirty )
	recalc();

    return _totalBlocks;
}


int
KDirInfo::totalItems()
{
    if ( _summaryDirty )
	recalc();

    return _totalItems;
}


int
KDirInfo::totalSubDirs()
{
    if ( _summaryDirty )
	recalc();

    return _totalSubDirs;
}


int
KDirInfo::totalFiles()
{
    if ( _summaryDirty )
	recalc();

    return _totalFiles;
}


time_t
KDirInfo::latestMtime()
{
    if ( _summaryDirty )
	recalc();

    return _latestMtime;
}


bool
KDirInfo::isFinished()
{
    return ! isBusy();
}


bool
KDirInfo::isBusy()
{
    if ( _pendingReadJobs > 0 )
	return true;

    if ( readState() == KDirReading ||
	 readState() == KDirQueued    )
	return true;

    return false;
}


void
KDirInfo::insertChild( KFileInfo *newChild )
{
    CHECK_PTR( newChild );

    if ( newChild->isDir() || _dotEntry == 0 || _isDotEntry )
    {
	/*
	 * Only directories are stored directly in pure directory nodes -
	 * unless something went terribly wrong, e.g. there is no dot entry to use.
	 * If this is a dot entry, store everything it gets directly within it.
	 *
	 * In any of those cases, insert the new child in the children list.
	 *
	 * We don't bother with this list's order - it's explicitly declared to
	 * be unordered, so be warned! We simply insert this new child at the
	 * list head since this operation can be performed in constant time
	 * without the need for any additional lastChild etc. pointers or -
	 * even worse - seeking the correct place for insertion first. This is
	 * none of our business; the corresponding "view" object for this tree
	 * will take care of such niceties.
	 */
	newChild->setNext( _firstChild );
	_firstChild = newChild;
	newChild->setParent( this );	// make sure the parent pointer is correct

	childAdded( newChild );		// update summaries
    }
    else
    {
	/*
	 * If the child is not a directory, don't store it directly here - use
	 * this entry's dot entry instead.
	 */
	_dotEntry->insertChild( newChild );
    }
}


void
KDirInfo::childAdded( KFileInfo *newChild )
{
    if ( ! _summaryDirty )
    {
	_totalSize	+= newChild->size();
	_totalBlocks	+= newChild->blocks();
	_totalItems++;

	if ( newChild->isDir() )
	    _totalSubDirs++;

	if ( newChild->isFile() )
	    _totalFiles++;

	if ( newChild->mtime() > _latestMtime )
	    _latestMtime = newChild->mtime();
    }
    else
    {
	// NOP

	/*
	 * Don't bother updating the summary fields if the summary is dirty
	 * (i.e. outdated) anyway: As soon as anybody wants to know some exact
	 * value a complete recalculation of the entire subtree will be
	 * triggered. On the other hand, if nobody wants to know (which is very
	 * likely) we can save this effort.
	 */
    }

    if ( _parent )
	_parent->childAdded( newChild );
}


void
KDirInfo::childDeleted( KFileInfo *deletedChild )
{
    /*
     * When children are deleted, things go downhill: Marking the summary
     * fields as dirty (i.e. outdated) is the only thing that can be done here.
     *
     * The accumulated sizes could be updated (by subtracting this deleted
     * child's values from them), but the latest mtime definitely has to be
     * recalculated: The child now being deleted might just be the one with the
     * latest mtime, and figuring out the second-latest cannot easily be
     * done. So we merely mark the summary as dirty and wait until a recalc()
     * will be triggered from outside - which might as well never happen when
     * nobody wants to know some summary field anyway.	*/

    _summaryDirty = true;

    if ( _parent )
	_parent->childDeleted( deletedChild );
}


void
KDirInfo::readJobAdded()
{
    _pendingReadJobs++;

    if ( _parent )
	_parent->readJobAdded();
}


void
KDirInfo::readJobFinished()
{
    _pendingReadJobs--;

    if ( _parent )
	_parent->readJobFinished();
}


void
KDirInfo::finalizeLocal()
{
    cleanupDotEntries();
}


KDirReadState
KDirInfo::readState() const
{
    if ( _isDotEntry && _parent )
	return _parent->readState();
    else
	return _readState;
}


void
KDirInfo::cleanupDotEntries()
{
    if ( ! _dotEntry || _isDotEntry )
	return;

    // Reparent dot entry children if there are no subdirectories on this level

    if ( ! _firstChild )
    {
	// kdDebug() << "Removing solo dot entry " << debugUrl() << endl;
	KFileInfo *child = _dotEntry->firstChild();
	_firstChild = child;		// Move the entire children chain here.
	_dotEntry->setFirstChild( 0 );	// _dotEntry will be deleted below.

	while ( child )
	{
	    child->setParent( this );
	    child = child->next();
	}
    }


    // Delete dot entries without any children

    if ( ! _dotEntry->firstChild() )
    {
	// kdDebug() << "Removing empty dot entry " << debugUrl() << endl;
	delete _dotEntry;
	_dotEntry = 0;
    }
}





KDirReadJob::KDirReadJob( KDirTree * tree,
			  KDirInfo * dir  )
    : _tree( tree )
    , _dir( dir )
{
    _dir->readJobAdded();
}


KDirReadJob::~KDirReadJob()
{
    _dir->readJobFinished();
}


void
KDirReadJob::childAdded( KFileInfo *newChild )
{
    _tree->childAddedNotify( newChild );
}


void
KDirReadJob::childDeleted( KFileInfo *deletedChild )
{
    _tree->childDeletedNotify( deletedChild );
}






KLocalDirReadJob::KLocalDirReadJob( KDirTree *	tree,
				    KDirInfo *	dir )
    : KDirReadJob( tree, dir )
    , _diskDir( 0 )
{
}


KLocalDirReadJob::~KLocalDirReadJob()
{
}


void
KLocalDirReadJob::startReading()
{
    struct dirent *	entry;
    struct stat		statInfo;
    QString		dirName	 = _dir->url();

    if ( ( _diskDir = opendir( dirName ) ) )
    {
	_tree->sendProgressInfo( dirName );
	_dir->setReadState( KDirReading );

	while ( ( entry = readdir( _diskDir ) ) )
	{
	    QString entryName = entry->d_name;

	    if ( entryName != "."  &&
		 entryName != ".."   )
	    {
		QString fullName = dirName + "/" + entryName;

		if ( lstat( fullName, &statInfo ) == 0 )	// lstat() OK
		{
		    if ( S_ISDIR( statInfo.st_mode ) )	// directory child?
		    {
			KDirInfo *subDir = new KDirInfo( entryName, &statInfo, _dir );
			_dir->insertChild( subDir );
			childAdded( subDir );

			if ( subDir->dotEntry() )
			    childAdded( subDir->dotEntry() );

			if ( _tree->crossFileSystems() ||
			     _dir->device() == subDir->device()	)
			{
			    /*
			     * Recurse into this sub directory if crossing file
			     * system boundaries is desired or if this
			     * subdirectory is on the same file system as its
			     * parent.
			     */
			    _tree->addJob( new KLocalDirReadJob( _tree, subDir ) );
			}
			else
			{
			    // TODO: mark as mount point
			}
		    }
		    else		// non-directory child
		    {
			KFileInfo *child = new KFileInfo( entryName, &statInfo, _dir );
			_dir->insertChild( child );
			childAdded( child );
		    }
		}
		else			// lstat() error
		{
		    kdWarning() << "lstat(" << fullName << ") failed: " << strerror( errno ) << endl;

		    /*
		     * Not much we can do when lstat() didn't work; let's at
		     * least create an (almost empty) entry as a placeholder.
		     */
		    KDirInfo *child = new KDirInfo( _dir, entry->d_name );
		    child->setReadState( KDirError );
		    _dir->insertChild( child );
		    childAdded( child );
		}
	    }
	}

	closedir( _diskDir );
	// kdDebug() << "Finished reading " << _dir->debugUrl() << endl;
	_dir->setReadState( KDirFinished );
	_tree->sendFinalizeLocal( _dir );
	_dir->finalizeLocal();
    }
    else
    {
	_dir->setReadState( KDirError );
	_tree->sendFinalizeLocal( _dir );
	_dir->finalizeLocal();
	kdWarning() << k_funcinfo << "opendir(" << dirName << ") failed" << endl;
	// opendir() doesn't set 'errno' according to POSIX  :-(
    }

    _tree->jobFinishedNotify( this );
    // Don't add anything after _tree->jobFinishedNotify()
    // since this deletes this job!
}






KAnyDirReadJob::KAnyDirReadJob( KDirTree *	tree,
				KDirInfo *	dir )
    : QObject()
    , KDirReadJob( tree, dir )
{
    _job = 0;
}


KAnyDirReadJob::~KAnyDirReadJob()
{
#if 0
    if ( _job )
	_job->kill( true );	// quietly
#endif
}


void
KAnyDirReadJob::startReading()
{
    KURL url( _dir->url() );

    if ( url.isMalformed() )
    {
	kdWarning() << k_funcinfo << "URL malformed: " << _dir->url() << endl;
    }
    
    _job = KIO::listDir( url,
			 false );	// showProgressInfo

    connect( _job, SIGNAL( entries( KIO::Job *, const KIO::UDSEntryList& ) ),
             this, SLOT  ( entries( KIO::Job *, const KIO::UDSEntryList& ) ) );

    connect( _job, SIGNAL( result  ( KIO::Job * ) ),
	     this, SLOT  ( finished( KIO::Job * ) ) );

    connect( _job, SIGNAL( canceled( KIO::Job * ) ),
	     this, SLOT  ( finished( KIO::Job * ) ) );
}


void
KAnyDirReadJob::entries ( KIO::Job *			job,
			  const KIO::UDSEntryList &	entryList )
{
    NOT_USED( job );
    KURL url( _dir->url() );	// Cache this - it's expensive!

    if ( url.isMalformed() )
    {
	kdWarning() << k_funcinfo << "URL malformed: " << _dir->url() << endl;
    }

    KIO::UDSEntryListConstIterator it = entryList.begin();
    
    while ( it != entryList.end() )
    {
	KFileItem entry( *it,
			 url,
			 true,		// determineMimeTypeOnDemand
			 true );	// URL is parent directory

	if ( entry.name() != "." &&
	     entry.name() != ".."  )
	{
	    // kdDebug() << "Found " << entry.url().url() << endl;

	    if ( entry.isDir()    &&	// Directory child
		 ! entry.isLink()   )	// and not a symlink?
	    {
		KDirInfo *subDir = new KDirInfo( &entry, _dir );
		_dir->insertChild( subDir );
		childAdded( subDir );

		if ( subDir->dotEntry() )
		    childAdded( subDir->dotEntry() );
		
		_tree->addJob( new KAnyDirReadJob( _tree, subDir ) );
	    }
	    else	// non-directory child
	    {
		KFileInfo *child = new KFileInfo( &entry, _dir );
		_dir->insertChild( child );
		childAdded( child );
	    }
	}
	
	++it;
    }
}


void
KAnyDirReadJob::finished( KIO::Job * job )
{
    if ( job->error() )
	_dir->setReadState( KDirError );
    else
	_dir->setReadState( KDirFinished );
    
    _dir->finalizeLocal();
    _job = 0;	// The job deletes itself after this signal!
    
    _tree->jobFinishedNotify( this );
    // Don't add anything after _tree->jobFinishedNotify()
    // since this deletes this job!
}






KDirTree::KDirTree()
    : QObject()
{
    _root			= 0;
    _crossFileSystems		= false;
    _enableLocalFileReader	= true;
    _jobQueue.setAutoDelete( true );	// Delete queued jobs automatically when destroyed
}


KDirTree::~KDirTree()
{
    // Jobs still in the job queue are automatically deleted along with the
    // queue since autoDelete is set.

    if ( _root )
	delete _root;
}


void
KDirTree::startReading( const KURL & url )
{
    // kdDebug() << k_funcinfo << " " << url.url() << endl;

#if 0
    kdDebug() << "url: "		<< url.url()		<< endl;
    kdDebug() << "path: "		<< url.path()		<< endl;
    kdDebug() << "filename: "		<< url.filename() 	<< endl;
    kdDebug() << "protocol: "		<< url.protocol() 	<< endl;
    kdDebug() << "isValid: "		<< url.isValid() 	<< endl;
    kdDebug() << "isMalformed: "	<< url.isMalformed() 	<< endl;
    kdDebug() << "isLocalFile: "	<< url.isLocalFile() 	<< endl;
#endif

    if ( url.isLocalFile() &&  _enableLocalFileReader )
    {
	// Local directory - use readdir() / lstat()

	KDirSaver dir( url );			// will restore cwd when going out of scope
	QString path = dir.currentDirPath();	// resolve relative paths
	struct stat statInfo;
	// kdDebug() << "Beginning with dir " << path << endl;

	if ( lstat( url.path(), &statInfo ) == 0 )	// lstat() OK
	{
	    if ( S_ISDIR( statInfo.st_mode ) )		// directory?
	    {
		KDirInfo * dir = new KDirInfo( path, &statInfo );
		_root = dir;
		childAddedNotify( _root );

		addJob( new KLocalDirReadJob( this, dir ) );
	    }
	    else	// no directory
	    {
		_root = new KFileInfo( path, &statInfo );
		childAddedNotify( _root );
	    }
	}
	else	// lstat() error
	{
	    kdWarning() << "lstat(" << url.path() << ") failed: " << strerror( errno ) << endl;
	    emit finished();
	    emit finalizeLocal( 0 );
	}

	if ( ! _jobQueue.isEmpty() )
	    QTimer::singleShot( 0, this, SLOT( timeSlicedRead() ) );
    }
    else	// Non-local URL - use KIO methods
    {
	_root = 0;
	KURL cleanUrl = url;
	cleanUrl.cleanPath();	// Resolve relative paths, get rid of multiple '/'
	KIO::StatJob * statJob = KIO::stat( cleanUrl,
					    false );	// showProgressInfo
	
	connect( statJob, SIGNAL( result  ( KIO::Job * ) ),
		 this,    SLOT  ( statRoot( KIO::Job * ) ) );

	// The rest is deferred to statRoot().
    }
}


void
KDirTree::statRoot( KIO::Job * job )
{
    KIO::StatJob *statJob = (KIO::StatJob *) job;
    
    if ( ! job->error() )
    {
	KFileItem entry( statJob->statResult(),
			 statJob->url(),
			 true,		// determineMimeTypeOnDemand
			 false );	// URL is parent directory

	if ( entry.isDir() )		// Directory?
	{
	    KDirInfo * dir = new KDirInfo( &entry,
					   0 );	// Parent
	    _root = dir;
	    childAddedNotify( _root );

	    addJob( new KAnyDirReadJob( this, dir ) );
	}
	else	// No directory
	{
	    _root = new KFileInfo( &entry,
				   0 );		// Parent
	    childAddedNotify( _root );
	}

	if ( ! _jobQueue.isEmpty() )
	    QTimer::singleShot( 0, this, SLOT( timeSlicedRead() ) );
    }
    else	// StatJob returned an Error
    {
	kdWarning() << "KIO::stat(" << statJob->url().url() << ") failed " << endl;
	emit finished();
	emit finalizeLocal( 0 );
    }
}


void
KDirTree::timeSlicedRead()
{
    if ( ! _jobQueue.isEmpty() )
	_jobQueue.head()->startReading();
}


void
KDirTree::jobFinishedNotify( KDirReadJob *job )
{
    // Get rid of the old (finished) job.

    _jobQueue.dequeue();
    delete job;


    // Look for a new job.

    if ( _jobQueue.isEmpty() )	// No new job available - we're done.
    {
	emit finished();
    }
    else			// There is a new job
    {
	// Set up zero-duration timer for the new job.

	QTimer::singleShot( 0, this, SLOT( timeSlicedRead() ) );
    }
}


void
KDirTree::childAddedNotify( KFileInfo *newChild )
{
    emit childAdded( newChild );

    if ( newChild->dotEntry() )
	emit childAdded( newChild->dotEntry() );
}


void
KDirTree::childDeletedNotify( KFileInfo *deletedChild )
{
    emit childDeleted( deletedChild );
}


void
KDirTree::addJob( KDirReadJob * job )
{
    CHECK_PTR( job );
    _jobQueue.enqueue( job );
}


void
KDirTree::sendProgressInfo( const QString &infoLine )
{
    emit progressInfo( infoLine );
}


void
KDirTree::sendFinalizeLocal( KDirInfo *dir )
{
    emit finalizeLocal( dir );
}




// EOF
