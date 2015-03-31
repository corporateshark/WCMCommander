/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#ifdef _WIN32
#  include <winsock2.h>
#endif

#include <time.h>
#include "fileopers.h"
#include "smblogon.h"
#include "ftplogon.h"
#include "sftpdlg.h"
#include "ltext.h"
#include "vfs-tmp.h"

enum
{
	CMD_ALL = 200,
	CMD_SKIP,
	CMD_SKIPALL,
	CMD_RETRY
};

#define MkDirMode  (S_IRWXU | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH)

/*
   //for ltext searcher
   _LT("DB>Delete")
   _LT("DB>All")
   _LT("DB>Skip")
   _LT("DB>Retry")
   _LT("DB>Skip All")
   _LT("DB>All")
   _LT("DB>Yes")
   _LT("DB>No")
*/

static ButtonDataNode bDeleteAllSkipCancel[] = { {"&Delete", CMD_OK}, { "&All", CMD_ALL}, { "&Skip", CMD_SKIP}, {"&Cancel", CMD_CANCEL}, {0, 0}};
static ButtonDataNode bRetrySkipCancel[] = { { "&Retry", CMD_RETRY}, { "&Skip", CMD_SKIP}, {"&Cancel", CMD_CANCEL}, {0, 0}};
static ButtonDataNode bRetrySkipSkipallCancel[] = { { "&Retry", CMD_RETRY}, { "&Skip", CMD_SKIP}, { "Skip &All", CMD_SKIPALL}, {"&Cancel", CMD_CANCEL}, {0, 0}};
static ButtonDataNode bOkSkipCancel[] = { { "O&k", CMD_OK}, { "&Skip", CMD_SKIP}, {"&Cancel", CMD_CANCEL}, {0, 0}};
static ButtonDataNode bSkipCancel[] = { { "&Skip" , CMD_SKIP}, {"&Cancel", CMD_CANCEL}, {0, 0}};
static ButtonDataNode bSkipSkipallCancel[] = { { "&Skip", CMD_SKIP}, { "Skip &All", CMD_SKIPALL}, {"&Cancel", CMD_CANCEL}, {0, 0}};
//static ButtonDataNode bOk[] = { { " O&k ", CMD_OK},  {0,0}};
static ButtonDataNode bOkAllNoCancel[] = { { "O&k", CMD_OK}, { "&All", CMD_ALL}, { "&No", CMD_NO}, {"&Cancel", CMD_CANCEL}, {0, 0}};


///////////////////////// smb callback objs

struct SmbLogonCBData
{
	FSSmbParam* volatile param;
	NCDialogParent* volatile parent;
};

#ifdef LIBSMBCLIENT_EXIST
int SmbLogonOperCallback( void* cbData )
{
	SmbLogonCBData* data = ( SmbLogonCBData* ) cbData;
	bool ret = GetSmbLogon( data->parent, *( data->param ) );
	return ret ? 1 : 0;
}
#endif

bool OF_FSCInfo::SmbLogon( FSSmbParam* a )
{
#ifdef LIBSMBCLIENT_EXIST
	MutexLock lock( _node->GetMutex() );

	if ( _node->NBStopped() ) { return false; }

	OperData* p = ( OperData* ) _node->Data();

	if ( !p ) { return false; }

	SmbLogonCBData data;
	data.param = a;
	data.parent = p->Parent();
	lock.Unlock();
	return _node->CallBack( SmbLogonOperCallback, &data ) > 0;
#else
	return false;
#endif
}


/////////////////////////ftp callback objs

struct FtpLogonCBData
{
	FSFtpParam* volatile param;
	NCDialogParent* volatile parent;
};

int FtpLogonOperCallback( void* cbData )
{
	FtpLogonCBData* data = ( FtpLogonCBData* ) cbData;
	bool ret = GetFtpLogon( data->parent, *( data->param ) );
	return ret ? 1 : 0;
}

bool OF_FSCInfo::FtpLogon( FSFtpParam* a )
{

	MutexLock lock( _node->GetMutex() );

	if ( _node->NBStopped() ) { return false; }

	OperData* p = ( OperData* ) _node->Data();

	if ( !p ) { return false; }

	FtpLogonCBData data;
	data.param = a;
	data.parent = p->Parent();
	lock.Unlock();
	return _node->CallBack( FtpLogonOperCallback, &data ) > 0;
}

///////////////////////////// prompt callback


int PromptOperCallback( void* cbData )
{
	PromptCBData* data = ( PromptCBData* ) cbData;
	bool ret = GetPromptAnswer( data );
	return ret ? 1 : 0;
}


bool OF_FSCInfo::Prompt( const unicode_t* header, const unicode_t* message, FSPromptData* prompts, int count )
{
	MutexLock lock( _node->GetMutex() );

	if ( _node->NBStopped() ) { return false; }

	OperData* p = ( OperData* ) _node->Data();

	if ( !p ) { return false; }

	PromptCBData data;
	data.header = header;
	data.message = message;
	data.prompts = prompts;
	data.count = count;
	data.parent = p->Parent();
	lock.Unlock();
	return _node->CallBack( PromptOperCallback, &data ) > 0;
}


bool OF_FSCInfo::Stopped()
{
	MutexLock lock( _node->GetMutex() );
	return _node->NBStopped();
};

OF_FSCInfo::~OF_FSCInfo() {};

OperFileThread::~OperFileThread() {}

OperRDData::~OperRDData() {}

class OperRDThread: public OperFileThread
{
	clPtr<FS> fs;
	FSPath path;
public:
	OperRDThread( const char* opName, NCDialogParent* par, OperThreadNode* n, clPtr<FS> f, FSPath& p )
		: OperFileThread( opName, par, n ), fs( f ), path( p ) {}
	virtual void Run();
	virtual ~OperRDThread();
};

void OperRDThread::Run()
{
	if ( !fs.Ptr() ) { return; }

	int n = 8;
	int ret_err;

	int havePostponedStatError = 0;
	FSString postponedStrError;

	while ( true )
	{
		if ( !( fs->Flags() & FS::HAVE_SYMLINK ) )
		{
			break;
		}

		FSStat st;

		// if path is inaccessible, try .. path. Throw the exception later
		// This makes panel at least having some valid folder
		while ( fs->Stat( path, &st, &ret_err, Info() ) )
		{
			havePostponedStatError = 1;
			postponedStrError = fs->StrError( ret_err );

			if ( !path.IsAbsolute() || path.Count() <= 1 || !path.Pop() )
			{
				throw_msg( "%s", postponedStrError.GetUtf8() );
			}
		}

		// yell immediately if the path is inaccessible (orig behavior)
		//if (fs->Stat(path, &st, &ret_err, Info()))
		// throw_msg("%s", fs->StrError(ret_err).GetUtf8());

		if ( !st.IsLnk() )
		{
			break;
		}

		n--;

		if ( n < 0 )
		{
			throw_msg( "too many symbolic links '%s'", path.GetUtf8() );
		}

		path.Pop();

		if ( !ParzeLink( path, st.link ) )
		{
			throw_msg( "invalid symbolic link '%s'", path.GetUtf8() );
		}
	}

	clPtr<FSList> list = new FSList;

	int havePostponedReadError = 0;

	// if directory is not readable, try .. path. Throw the exception later
	// "Stat" call above does not catch this: it checks only folder existence, but not accessibilly
	while ( fs->ReadDir( list.ptr(), path, &ret_err, Info() ) )
	{
		havePostponedReadError = 1;
		postponedStrError = fs->StrError( ret_err );

		if ( !path.IsAbsolute() || path.Count() <= 1 || !path.Pop() )
		{
			throw_msg( "%s", postponedStrError.GetUtf8() );
		}
	}

	// yell immediately if the dir is unreadable (orig behavior)
	//int ret = fs->ReadDir(list.ptr(), path, &ret_err, Info());
	//if (ret)
	// throw_msg("%s", fs->StrError(ret_err).GetUtf8());

	FSStatVfs vst;
	fs->StatVfs( path, &vst, &ret_err, Info() );

	MutexLock lock( Node().GetMutex() ); //!!!

	if ( Node().NBStopped() ) { return; }

	OperRDData* data = ( ( OperRDData* )Node().Data() );
	data->list = list;
	data->path = path;
	data->executed = true;
	data->vst = vst;

	if ( havePostponedReadError || havePostponedStatError )
	{
		data->nonFatalErrorString = postponedStrError;
	}
}

OperRDThread::~OperRDThread() {}

void ReadDirThreadFunc( OperThreadNode* node )
{
	try
	{
		MutexLock lock( node->GetMutex() );

		if ( !node->Data() ) { return; }

		OperRDData* data = ( ( OperRDData* )node->Data() );
		//dbg_printf("ReadDirThreadFunc path=%s",data->path.GetUtf8());
		OperRDThread thread( "panel::chdir", data->Parent(), node, data->fs, data->path );
		lock.Unlock();//!!!

		try
		{
			thread.Run();
		}
		catch ( cexception* ex )
		{
			lock.Lock(); //!!!

			if ( !node->NBStopped() ) //обязательно надо проверить, иначе 'data' может быть неактуальной
			{
				data->errorString = ex->message();
			}

			ex->destroy();
		}
	}
	catch ( cexception* ex )
	{
		fprintf( stderr, "ERR!!! Error exception in ReadDirThreadFunc - '%s'\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "ERR!!! Unhandled exception in ReadDirThreadFunc\n" );
	}
}


//////////////////////////////////////////  Common file operations

enum InfoSignal
{
	INFO_NEXTFILE = 2
};


class OperCFData: public OperData
{
public:
	volatile bool executed;

	clPtr<FS> srcFs;  //??volatile
	FSPath srcPath; //??volatile
	clPtr<FSList> srcList; //??volatile

	clPtr<FS> destFs; //??volatile
	FSPath destPath; //??volatile

	FSString errorString; //??volatile
	clPtr<cstrhash<bool, unicode_t> > resList; //??volatile

	Mutex infoMutex;
	volatile bool pathChanged;
	volatile uint64_t infoCount;

	FSString infoSrcUri; //??volatile
	FSString infoDstUri; //??volatile

	volatile bool progressChanged;
	volatile int64_t infoSize;
	volatile int64_t infoProgress;
	volatile unsigned infoMs; //for calculating copy speed
	volatile int64_t infoBytes; //

	OperCFData( NCDialogParent* p )
		:  OperData( p ), executed( false ),
		   pathChanged( false ), infoCount( 0 ), progressChanged( false ),
		   infoSize( 0 ), infoProgress( 0 ), infoMs( 0 ), infoBytes( 0 ) {}

	void Clear()
	{
		executed = false;
		errorString.Clear();
		srcList.clear();

		pathChanged = false;
//		uint64_t infoCount = 0;
		infoSrcUri.Clear();
		infoDstUri.Clear();
		progressChanged = false;
		infoSize = 0;
		infoProgress = 0;
	}
	virtual ~OperCFData();
};

OperCFData::~OperCFData() {}

class OperCFThread: public OperFileThread
{
	volatile bool commitAll;
	volatile bool skipNonRegular;
	enum { BSIZE = 1024 * 512, STARTSIZE = 1024 * 64 };
	char* _buffer;
public:
	OperCFThread( const char* opName, NCDialogParent* par, OperThreadNode* n )
		:  OperFileThread( opName, par, n ),
		   commitAll( false ), skipNonRegular( false ), _buffer( 0 )
	{
		_buffer = new char[BSIZE];
	}

	void CreateDirectory( FS* fs, FSPath& path ); //throws

	bool Unlink ( FS* fs, FSPath& path, bool* skipAll = 0 );
	bool RmDir  ( FS* fs, FSPath& path, bool* skipAll = 0 );
	bool DeleteFile   ( FS* fs, FSPath& path );
	bool DeleteDir ( FS* fs, FSPath& path );
	bool DeleteList   ( FS* fs, FSPath& _path, FSList& list );

	//from и to - эффект cptr !!!
	bool SendCopyNextFileInfo( FSString from, FSString to );

	bool SendProgressInfo( int64_t size, int64_t progress, int64_t bytes );

	bool CopyLink( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs, FSPath& path, bool move );
	// XXX CopyFile/MoveFile are #define'd in winbase.h
	bool CopyFile( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs, FSPath& destPath, bool move );
	bool CopyDir( FS* srcFs, FSPath& __srcPath, FSNode* srcNode, FS* destFs, FSPath& __destPath, bool move );
	bool CopyNode( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs, FSPath& destPath, bool move );
	bool Copy( FS* srcFs, FSPath& __srcPath, FSList* list, FS* destFs, FSPath& __destPath, cstrhash<bool, unicode_t>& resList );

	int MoveFile( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs, FSPath& destPath );
	int MoveDir( FS* srcFs, FSPath& __srcPath, FSNode* srcNode, FS* destFs, FSPath& __destPath );
	bool MoveNode( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs, FSPath& destPath );
	bool Move( FS* srcFs, FSPath& __srcPath, FSList* list, FS* destFs, FSPath& __destPath );

	virtual ~OperCFThread();
};

OperCFThread::~OperCFThread()
{
	if ( _buffer )
	{
		delete [] _buffer;
	}
}

void OperCFThread::CreateDirectory( FS* fs, FSPath& path )
{
	int ret_err;

	if ( fs->MkDir( path, 0777, &ret_err, Info() ) )
	{
		throw_msg( "%s", fs->StrError( ret_err ).GetUtf8() );
	}
}

class SimpleCFThreadWin: public NCDialog
{
public:
	OperCFData threadData;

	SimpleCFThreadWin( NCDialogParent* parent, const char* name )
		:  NCDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( name ).data(), bListCancel ), threadData( parent ) {}
	virtual void OperThreadStopped();
	virtual ~SimpleCFThreadWin();
};

void SimpleCFThreadWin::OperThreadStopped()
{
	if ( !threadData.errorString.IsEmpty() )
	{
		NCMessageBox( ( NCDialogParent* )Parent(), _LT( "Create directory" ), threadData.errorString.GetUtf8(), true );
		EndModal( 0 );
		return;
	}

	EndModal( 100 );
}

SimpleCFThreadWin::~SimpleCFThreadWin() {}


///////////////////////////////////////////////////////   MkDir

void MkDirThreadFunc( OperThreadNode* node )
{
	try
	{
		MutexLock lock( node->GetMutex() );

		if ( !node->Data() ) { return; }

		OperCFData* data = ( ( OperCFData* )node->Data() );
		OperCFThread thread( "create directory", data->Parent(), node );

		clPtr<FS> fs = data->srcFs;
		FSPath path = data->srcPath;

		lock.Unlock();//!!!

		try
		{
			thread.CreateDirectory( fs.Ptr(), path );
		}
		catch ( cexception* ex )
		{
			lock.Lock(); //!!!

			if ( !node->NBStopped() ) //this check is needed because 'data' may be not valid
			{
				data->errorString = ex->message();
			}

			ex->destroy();
		}
	}
	catch ( cexception* ex )
	{
		fprintf( stderr, "ERR!!! Error exception in MkDirThreadFunc - '%s'\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "ERR!!! Unhandled exception in MkDirThreadFunc\n" );
	}
}

bool MkDir( clPtr<FS> f, FSPath& p, NCDialogParent* parent )
{
	SimpleCFThreadWin dlg( parent, _LT( "Create directory" ) );
	dlg.threadData.Clear();
	dlg.threadData.srcFs = f;
	dlg.threadData.srcPath = p;
	dlg.RunNewThread( "Create directory", MkDirThreadFunc, &dlg.threadData ); //may throw exception
	dlg.Enable();
	dlg.Show();
	dlg.DoModal();
	dlg.StopThread();
	return dlg.threadData.errorString.IsEmpty();
}


///////////////////////////////////////////////////// DELETE

//returns true if the process can be procceeded

bool OperCFThread::Unlink( FS* fs, FSPath& path, bool* skipAll )
{
	int ret_err;

	while ( fs->Delete( path, &ret_err, Info() ) )
	{
		if ( skipAll && *skipAll ) { return true; }

		switch ( RedMessage( _LT( "Can't delete file:\n" ), fs->Uri( path ).GetUtf8(), skipAll ? bRetrySkipSkipallCancel : bRetrySkipCancel,
		                     fs->StrError( ret_err ).GetUtf8() ) )
		{
			case CMD_SKIPALL:
				if ( skipAll ) { *skipAll = true; } //no break

			case CMD_SKIP:
				return true;

			case CMD_RETRY:
				continue;

			default:
				return false;
		}
	}

	return true;
}

bool OperCFThread::RmDir( FS* fs, FSPath& path, bool* skipAll )
{
	int ret_err;

	while ( fs->RmDir( path, &ret_err, Info() ) )
	{
		if ( skipAll && *skipAll ) { return true; }

		switch ( RedMessage( _LT( "Can`t delete directory:\n" ) , fs->Uri( path ).GetUtf8(), skipAll ? bRetrySkipSkipallCancel : bRetrySkipCancel,
		                     fs->StrError( ret_err ).GetUtf8() ) )
		{
			case CMD_SKIPALL:
				if ( skipAll ) { *skipAll = true; } //no break

			case CMD_SKIP:
				return true;

			case CMD_RETRY:
				continue;

			default:
				return false;
		}
	}

	return true;
}

bool OperCFThread::DeleteFile( FS* fs, FSPath& path ) //return true if not concelled
{
	if ( Info()->Stopped() ) { return false; }

	if ( !commitAll )
	{
		switch ( RedMessage( _LT( "Do you want to delete file?\n" ), fs->Uri( path ).GetUtf8(), bDeleteAllSkipCancel ) )
		{
			case CMD_SKIP:
				return true;

			case CMD_ALL:
				commitAll = true;
				break;

			case CMD_OK:
				break;

			default:
				return false;
		}
	}

	return Unlink( fs, path ); //skip all???
}

bool OperCFThread::DeleteDir( FS* fs, FSPath& path )
{
	if ( Info()->Stopped() ) { return false; }

	FSList list;

	while ( true )
	{
		int ret_err;
		int ret = fs->ReadDir( &list, path, &ret_err, Info() );

		if ( ret == -2 ) { return false; }

		if ( !ret ) { break; }

		switch ( RedMessage( _LT( "Can`t open directory:\n" ), fs->Uri( path ).GetUtf8(), bRetrySkipCancel, fs->StrError( ret_err ).GetUtf8() ) )
		{
			case CMD_SKIP:
				return true;

			case CMD_RETRY:
				continue;

			default:
				return false;
		}
	}

	return DeleteList( fs, path, list );
}

bool OperCFThread::DeleteList( FS* fs, FSPath& _path, FSList& list )
{
	if ( Info()->Stopped() ) { return false; }

	FSPath path = _path;
	int cnt = path.Count();

	for ( FSNode* node = list.First(); node; node = node->next )
	{
		if ( node->extType ) { continue; }

		path.SetItemStr( cnt, node->Name() );

		if ( node->IsDir() && !node->st.IsLnk() )
		{
			if ( !DeleteDir( fs, path ) ) { return false; }

			if ( !RmDir( fs, path ) ) { return false; }

			continue;
		}

		if ( !DeleteFile( fs, path ) ) { return false; }
	}

	return true;
}


void DeleteThreadFunc( OperThreadNode* node )
{
	try
	{
		MutexLock lock( node->GetMutex() );

		if ( !node->Data() ) { return; }

		OperCFData* data = ( ( OperCFData* )node->Data() );
		OperCFThread thread( "Delete", data->Parent(), node );

		clPtr<FS> fs = data->srcFs;
		FSPath path = data->srcPath;
		clPtr<FSList> list = data->srcList;

		lock.Unlock();//!!!

		try
		{
			if ( list.ptr() )
			{
				thread.DeleteList( fs.Ptr(), path, *( list.ptr() ) );
			}
		}
		catch ( cexception* ex )
		{
			lock.Lock(); //!!!

			if ( !node->NBStopped() ) //this check is needed because 'data' may be not valid
			{
				data->errorString = ex->message();
			}

			ex->destroy();
		}
	}
	catch ( cexception* ex )
	{
		fprintf( stderr, "ERR!!! Error exception in DeleteThreadFunc - '%s'\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "ERR!!! Unhandled exception in DeleteThreadFunc\n" );
	}
}

bool DeleteList( clPtr<FS> f, FSPath& p, clPtr<FSList> list, NCDialogParent* parent )
{
	SimpleCFThreadWin dlg( parent, _LT( "Delete" ) );
	dlg.threadData.Clear();
	dlg.threadData.srcFs = f;
	dlg.threadData.srcPath = p;
	dlg.threadData.srcList = list;
	dlg.RunNewThread( "Delete", DeleteThreadFunc, &dlg.threadData ); //may throw exception
	dlg.Enable();
	dlg.Show();
	dlg.DoModal();
	dlg.StopThread();
	return dlg.threadData.errorString.IsEmpty();
}

/////////////////////////////////////////////////////////////// Copy

bool OperCFThread::SendCopyNextFileInfo( FSString from, FSString to )
{
	MutexLock lock( Node().GetMutex() );

	if ( Node().NBStopped() ) { return false; }

	OperCFData* data = ( ( OperCFData* )Node().Data() );

	if ( !data ) { return false; }

	data->infoCount++;
	data->infoSrcUri = from;
	data->infoDstUri = to;
	data->pathChanged = true;

	if ( !WinThreadSignal( INFO_NEXTFILE ) ) { return false; }

	data->infoProgress = 0;

	return true;
}

namespace wal
{
	extern unsigned GetTickMiliseconds();
};

bool OperCFThread::SendProgressInfo( int64_t size, int64_t progress, int64_t bytes )
{
	MutexLock lock( Node().GetMutex() );

	if ( Node().NBStopped() ) { return false; }

	OperCFData* data = ( ( OperCFData* )Node().Data() );

	if ( !data ) { return false; }

	data->infoSize = size;
	data->infoProgress = progress;
	data->infoBytes += bytes;
	data->infoMs = GetTickMiliseconds();
	data->progressChanged = true;

	if ( !WinThreadSignal( INFO_NEXTFILE ) ) { return false; }

	return true;
}

OperFileNameWin::OperFileNameWin( Win* parent, int ccount )
	: Win( Win::WT_CHILD, 0, parent, 0 )
	, _ccount( ccount )
{
	wal::GC gc( this );
	cpoint size = GetStaticTextExtent( gc, ABCString, GetFont() );
	size.x = size.x / ABCStringLen * ccount;

	if ( size.x > 500 ) { size.x = 550; }

	SetLSize( LSize( size ) );
}

void OperFileNameWin::SetText( const unicode_t* s )
{
	text = new_unicode_str( s );
	Invalidate();
}

void OperFileNameWin::Paint( wal::GC& gc, const crect& paintRect )
{
	crect rect = ClientRect();
	gc.SetFillColor( UiGetColor( uiBackground, 0, 0, 0xFFFFFF ) );
	gc.FillRect( rect );
	gc.SetTextColor( UiGetColor( uiColor, 0, 0, 0 ) );
	gc.Set( GetFont() );

	unicode_t* p = text.data();

	if ( p )
	{
		int l = unicode_strlen( p );

		if ( l > _ccount )
		{
			p += l - _ccount;
		}

		DrawStaticText( gc, 0, 0, p );
	}
}

OperFileNameWin::~OperFileNameWin() {};


class NCNumberWin: public Win
{
	int64_t _num;
public:
	NCNumberWin( Win* parent, int width = 10 )
		:  Win( Win::WT_CHILD, 0, parent, 0 ), _num( 0 )
	{
		wal::GC gc( this );
		cpoint size = GetStaticTextExtent( gc, ABCString, GetFont() );
		size.x = size.x / ABCStringLen * width;
		SetLSize( LSize( size ) );
	}

	void SetNumber( int64_t n )
	{
		if ( _num == n ) { return; }

		_num = n;
		Invalidate();
	}

	void Paint( wal::GC& gc, const crect& paintRect );
	virtual ~NCNumberWin();
};

void NCNumberWin::Paint( wal::GC& gc, const crect& paintRect )
{
	crect rect = ClientRect();
	gc.SetFillColor( UiGetColor( uiBackground, 0, 0, 0xFFFFFF ) );
	gc.FillRect( rect );
	gc.SetTextColor( UiGetColor( uiColor, 0, 0, 0 ) );
	gc.Set( GetFont() );

	unicode_t c[32];
	unicode_t buf[32];
	unicode_t* s = buf;

	int64_t n = _num;
	bool minus = n < 0;

	if ( minus ) { n = -n; }

	int i = 0;

	for ( ; n > 0; i++, n /= 10 ) { c[i] = char( n % 10 ) + '0'; }

	if ( minus ) { *s = '-'; s++; }

	if ( i == 0 ) {*s = '0'; s++; };

	for ( i--; i >= 0; i-- ) { *( s++ ) = c[i]; }

	*s = 0;

	gc.TextOut( 0, 0, buf );
}

NCNumberWin::~NCNumberWin() {}


class NCProgressWin: public Win
{
	int64_t _from, _to;
	int64_t _num;
	int64_t _lastWidth, _lastPos;
public:
	NCProgressWin( Win* parent )
		:  Win( Win::WT_CHILD, 0, parent, 0 ), _from( 0 ), _to( 0 ), _num( 0 ), _lastWidth( 0 ), _lastPos( 0 )

	{
		cpoint size;
		size.x = 10;
		size.y = 16;
		LSize ls( size );
		ls.x.maximal = 1000;
		SetLSize( ls );
	}

	void SetData( int64_t from, int64_t to, int64_t num )
	{
		if ( _from == from && _to == to && _num == num ) { return; }

		bool diapazonChanged = _from != from || _to != to;

		_from = from;
		_to = to;
		_num = num;

		if ( _num < _from || _to <= _from ) { return; }

		int64_t size = _to - _from;
		int64_t n = ( _lastWidth * _num ) / size;

		if ( diapazonChanged || _lastPos != n )
		{
			Invalidate();
		}
	}

	virtual void Paint( wal::GC& gc, const crect& paintRect );
	virtual ~NCProgressWin();
};


inline unsigned MidAB( int a, int b, int i, int n )
{
	return n > 1 ? a + ( ( b - a ) * i ) / ( n - 1 ) : a;
}

static void FillHorisont( wal::GC& gc, crect rect, unsigned a, unsigned b )
{
	if ( rect.IsEmpty() ) { return; }

	unsigned ar = a & 0xFF,    ag = ( a >> 8 ) & 0xFF, ab = ( a >> 16 ) & 0xFF,
	         br = b & 0xFF, bg = ( b >> 8 ) & 0xFF, bb = ( b >> 16 ) & 0xFF;

	int h = rect.Height();
	int x1 = rect.left, x2 = rect.right;

	if ( h <= 0 || x1 >= x2 ) { return; }

	for ( int i = 0; i < h; i++ )
	{
		unsigned color = ( MidAB( ar, br, i, h ) & 0xFF ) + ( ( MidAB( ag, bg, i, h ) & 0xFF ) << 8 ) + ( ( MidAB( ab, bb, i, h ) & 0xFF ) << 16 );
		gc.SetLine( color );
		gc.MoveTo( x1, rect.top + i );
		gc.LineTo( x2, rect.top + i );
	}

}

void NCProgressWin::Paint( wal::GC& gc, const crect& paintRect )
{
	crect rect = ClientRect();
	int w = rect.Width();

	Draw3DButtonW2( gc, rect, 0x808080, false );
	rect.Dec();
	rect.Dec();
	w -= 2;

	if ( !( _num < _from || _to <= _from || w <= 0 ) )
	{
		int64_t size = _to - _from;
		int n = int( ( w * _num ) / size );

		crect r = rect;
		r.right = n;

		unsigned color = 0xA00000;
		unsigned bColor = ColorTone( color, -80 ), aColor = ColorTone( color, +80 );
		FillHorisont( gc, r, aColor, bColor );

		_lastWidth = w;
		_lastPos = n;
		rect.left += n;
	}

	unsigned color = 0xB0B0B0;
	unsigned bColor = ColorTone( color, +50 ), aColor = ColorTone( color, -50 );
	FillHorisont( gc, rect, aColor, bColor );

}

NCProgressWin::~NCProgressWin() {}


class CopyDialog: public SimpleCFThreadWin
{
	Layout _layout;
	StaticLine _text1;
	StaticLine _text2;
	StaticLine _text3;
	OperFileNameWin _from, _to;
	NCNumberWin _countWin;
	NCProgressWin _progressWin;
	StaticLine _speedStr;
	enum
	{
		SPEED_NODE_COUNT = 10
	};
	struct SpeedNode
	{
		unsigned deltaMs;
		int64_t bytes;
		SpeedNode(): deltaMs( 0 ), bytes( 0 ) {}
	} _speedList[SPEED_NODE_COUNT];

	unsigned _lastMs;

public:
	CopyDialog( NCDialogParent* parent, bool move = false )
		:  SimpleCFThreadWin( parent, move ? _LT( "Move" ) : _LT( "Copy" ) ),
		   _layout( 7, 2 ),
		   _text1( 0, this, utf8_to_unicode( move ? _LT( "Moving the file" ) : _LT( "Copying the file" ) ).data() ),
		   _text2( 0, this, utf8_to_unicode( _LT( "to" ) ).data() ),
		   _text3( 0, this, utf8_to_unicode( _LT( "Files processed" ) ).data() ),
		   _from( this ),
		   _to( this ),
		   _countWin( this ),
		   _progressWin( this ),
		   _speedStr( uiValue, this, 0, 0, StaticLine::LEFT, 10 ),
		   _lastMs( GetTickMiliseconds() )
	{
		_layout.AddWin( &_text1, 0, 0, 0, 1 );
		_layout.AddWin( &_from, 1, 0, 1, 1 );
		_layout.AddWin( &_text2, 2, 0, 2, 1 );
		_layout.AddWin( &_to, 3, 0, 3, 1 );
		_layout.AddWin( &_progressWin, 4, 0, 4, 1 );
		_layout.AddWin( &_text3, 5, 0 );
		_layout.AddWin( &_countWin, 5, 1 );
		_layout.AddWin( &_speedStr, 6, 0 );
		_text1.Show();
		_text1.Enable();
		_text2.Show();
		_text2.Enable();
		_text3.Show();
		_text3.Enable();
		_from.Show();
		_from.Enable();
		_to.Show();
		_to.Enable();
		_countWin.Show();
		_countWin.Enable();
		_progressWin.Show();
		_progressWin.Enable();
		_speedStr.Show();
		_speedStr.Enable();
		AddLayout( &_layout );
		SetTimer( 1, 1000 );
		SetPosition();
	}

	virtual void OperThreadSignal( int info );
	virtual void EventTimer( int tid );
	virtual ~CopyDialog();
};

#define GIG (int64_t(1024)*1024*1024)
#define MEG (int64_t(1024)*1024)
#define KIL (int64_t(1024))

static char* GetSmallPrintableSpeedStr( char buf[64], int64_t size )
{
	char str[16];
	str[0] = 0;

	int64_t num = size;
	int mod = 0;

	if ( num >= GIG ) //:)
	{
		mod = ( num % GIG ) / ( GIG / 10 );
		num /= GIG;
		str[0] = ' ';
		str[1] = 'G';
		str[2] = 'b';
		str[3] = '/';
		str[4] = 's';
		str[5] = 0;
	}
	else if ( num >= MEG )
	{
		mod = ( num % MEG ) / ( MEG / 10 );
		num /= MEG;
		str[0] = ' ';
		str[1] = 'M';
		str[2] = 'b';
		str[3] = '/';
		str[4] = 's';
		str[5] = 0;
	}
	else if ( num >= KIL )
	{
		mod = ( num % KIL ) / ( KIL / 10 );
		num /= KIL;
		str[0] = ' ';
		str[1] = 'K';
		str[2] = 'b';
		str[3] = '/';
		str[4] = 's';
		str[5] = 0;
	}
	else { mod = -1; }


	char dig[64];
	char* t = unsigned_to_char<int64_t>( num, dig );

	if ( mod >= 0 ) { t--; t[0] = '.'; t[1] = mod + '0'; t[2] = 0; }

	char* us = buf;

	for ( char* s = dig; *s; s++ )
	{
		*( us++ ) = *s;
	}

	for ( char* t = str; *t; t++ )
	{
		*( us++ ) = *t;
	}

	*us = 0;

	return buf;
}

void CopyDialog::EventTimer( int tid )
{
	if ( tid == 1 )
	{
		unsigned ms = 0;
		int64_t bytes = 0;

		{
			MutexLock lock( &threadData.infoMutex );
			ms = threadData.infoMs;
			bytes = threadData.infoBytes;
			threadData.infoBytes = 0;
			lock.Unlock();
		}

		//shift
		int i;

		for ( i = SPEED_NODE_COUNT - 1; i > 0; i-- )
		{
			_speedList[i] = _speedList[i - 1];
		}

		_speedList[0].bytes = bytes;

		if ( bytes > 0 )
		{
			unsigned n = ms - _lastMs;
			_speedList[0].deltaMs = n > 1000000 ? 0 : n;
			_lastMs = ms;
		}
		else
		{
			_speedList[0].deltaMs = 0;
		}

		int64_t sumBytes = 0;
		int64_t sumMs = 0;

		for ( i = 0; i < SPEED_NODE_COUNT; i++ )
		{
			sumMs += _speedList[i].deltaMs;
			sumBytes += _speedList[i].bytes;
		}

		int64_t speed = 0;

		if ( sumMs > 0 && sumBytes > 0 )
		{
			speed = ( sumBytes * 1000 ) / sumMs;
		}

		char buf[64];
		_speedStr.SetText( utf8_to_unicode( GetSmallPrintableSpeedStr( buf, speed ) ).data() );

		return;
	}
}

void CopyDialog::OperThreadSignal( int info )
{
	if ( info == INFO_NEXTFILE )
	{
		MutexLock lock( &threadData.infoMutex );

		if ( threadData.pathChanged )
		{
			_from.SetText( threadData.infoSrcUri.GetUnicode() );
			_to.SetText( threadData.infoDstUri.GetUnicode() );
			_countWin.SetNumber( threadData.infoCount );
			threadData.pathChanged = false;
		}

		if ( threadData.progressChanged )
		{
			_progressWin.SetData( 0, threadData.infoSize, threadData.infoProgress );
			threadData.progressChanged = false;
		}
	}
}

CopyDialog::~CopyDialog() {}

static bool IsSameFile( FS* srcFs, FSPath& srcPath, FSStat* srcStat, FS* destFs, FSPath& destPath )
{
	if ( srcFs->Type() != destFs->Type() ) { return false; }

#ifndef _WIN32

	if ( destFs->Type() == FS::SYSTEM )
	{
		FSStat st;
		return ( !destFs->Stat( destPath, &st, 0, 0 ) && srcStat->dev == st.dev && srcStat->ino == st.ino );
	}

#endif

	FSString s = destFs->Uri( destPath );
	return !srcFs->Uri( srcPath ).Cmp( s );
}

bool OperCFThread::CopyLink( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs, FSPath& destPath, bool move )
{
	ASSERT( srcNode->st.IsLnk() );

	if ( IsSameFile( srcFs, srcPath, &( srcNode->st ), destFs, destPath ) )
	{
		RedMessage( _LT( "Can't copy link to itself:\n" ) , srcFs->Uri( srcPath ).GetUtf8() );
		return false;
	}

	int ret_err;

	while ( destFs->Symlink( destPath, srcNode->st.link, &ret_err, Info() ) && !skipNonRegular )
		switch ( RedMessage( _LT( "Can't create symbolic link:\n" ), destFs->Uri( destPath ).GetUtf8(), "to\n",
		                     srcNode->st.link.GetUtf8(),  bSkipSkipallCancel, destFs->StrError( ret_err ).GetUtf8() ) )
		{
			case CMD_CANCEL:
				return false;

			case CMD_SKIPALL:
				skipNonRegular = true;

			case CMD_SKIP:
				return true;
		}

	return !move || Unlink( srcFs, srcPath );
}


//#define BUFSIZE (1024*512) //(1024*64)

//inline FSString Err(FS *fs, int err){ return fs->StrError(err); }

bool OperCFThread::CopyFile( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs, FSPath& destPath, bool move )
{
	if ( !srcNode->st.IsReg() && !skipNonRegular )
		switch ( RedMessage( _LT( "Can't copy the links or special file:\n" ), srcFs->Uri( srcPath ).GetUtf8(), bSkipSkipallCancel ) )
		{
			case CMD_SKIPALL:
				skipNonRegular = true; // no break

			case CMD_SKIP:
				return true;

			default:
				return false;
		}

	if ( IsSameFile( srcFs, srcPath, &( srcNode->st ), destFs, destPath ) )
	{
		RedMessage( _LT( "Can't copy file to itself:\n" ) , srcFs->Uri( srcPath ).GetUtf8() );
		return false;
	}

	SendCopyNextFileInfo( srcFs->Uri( srcPath ), destFs->Uri( destPath ) );
	SendProgressInfo( srcNode->st.size, 0, 0 );

	bool stopped = false;

	int ret_err;

	int in = -1;

	if (destFs->Type() == FS::TYPES::TMP)
	{
		// copy and move to tmpfs work the same way. Should we yell on move op instead?
		FSTmp* destTmpFS = static_cast<FSTmp*>(destFs);
		if (!destTmpFS->BaseIsEqual(srcFs))
		{
			RedMessage(_LT("Temporary panel can store only files from the same file system:\n"));
			return false;
		}
		if (srcFs->Type() == FS::TMP && srcNode->IsReg())
		{
			FSString srcFullFSString(srcNode->name.GetUnicode());
			FSPath srcFullFSPath(srcFullFSString);
			//srcFullFSPath.dbg_printf("OperCFThread::CopyFile srcFullFSPath=");
			//destPath.dbg_printf("OperCFThread::CopyFile destPath=");
			FSPath destDir(destPath);
			destDir.Pop();
			return destTmpFS->AddNode(srcFullFSPath, destDir);
		}
		else
			return destTmpFS->AddNode(srcPath, srcNode, destPath);
	}

	while ( true )
	{
		in = srcFs->OpenRead( srcPath, FS::SHARE_READ, &ret_err, Info() );

		if ( in == -2 ) { return false; }

		if ( in >= 0 ) { break; }

		switch ( RedMessage( _LT( "Can't open file:\n" ) , srcFs->Uri( srcPath ).GetUtf8(), bRetrySkipCancel, srcFs->StrError( ret_err ).GetUtf8() ) )
		{
			case CMD_CANCEL:
				return false;

			case CMD_SKIP:
				return true;
		}
	}

	int out =  -1;

	out = destFs->OpenCreate( destPath, false | commitAll, srcNode->st.mode, 0, &ret_err, Info() );

	if ( out < 0 && destFs->IsEEXIST( ret_err ) )
		switch ( RedMessage( _LT( "Owerwrite  file?\n" ) , destFs->Uri( destPath ).GetUtf8(), bOkAllNoCancel ) )
		{
			case CMD_ALL:
				commitAll = true; //no break

			case CMD_OK:
				out = destFs->OpenCreate( destPath, true, srcNode->st.mode, 0, &ret_err, Info() );
				break;

			case CMD_NO:
				srcFs->Close( in, 0, Info() );
				return true;

			default:
				srcFs->Close( in, 0, Info() );
				return false;
		}

	if ( out < 0 )
	{
		srcFs->Close( in, 0, Info() );
		return RedMessage( _LT( "Can't create file:\n" ), destFs->Uri( destPath ).GetUtf8(), bSkipCancel, destFs->StrError( ret_err ).GetUtf8() ) == CMD_SKIP;
	}

	int  bytes;
	//char    buf[BUFSIZE];
	int64_t doneBytes = 0;

	int blockSize = STARTSIZE;

	while ( true )
	{
		if ( Info()->Stopped() )
		{
			stopped = true;
			goto err;
		}

		time_t timeStart = time( 0 );

		if ( ( bytes = srcFs->Read( in, _buffer, blockSize, &ret_err, Info() ) ) < 0 )
		{
			if ( bytes == -2 ||
			     RedMessage( _LT( "Can't read the file:\n" ), srcFs->Uri( srcPath ).GetUtf8(), bSkipCancel, srcFs->StrError( ret_err ).GetUtf8() ) != CMD_SKIP )
			{
				stopped = true;
			}

			goto err;
		}

		if ( !bytes ) { break; }

		int b;

		if ( ( b = destFs->Write( out, _buffer, bytes, &ret_err, Info() ) ) < 0 )
		{
			if ( b == -2 || RedMessage( _LT( "Can't write the file:\n" ), destFs->Uri( destPath ).GetUtf8(), bSkipCancel, destFs->StrError( ret_err ).GetUtf8() ) != CMD_SKIP )
			{
				stopped = true;
			}

			goto err;
		}


		if ( b != bytes )
		{
			if ( RedMessage( "May be disk full \n(writed bytes != readed bytes)\nwhen write:\n", destFs->Uri( destPath ).GetUtf8(), bSkipCancel ) != CMD_SKIP )
			{
				stopped = true;
			}

			goto err;
		}

		time_t timeStop = time( 0 );

		if ( timeStart == timeStop && blockSize < BSIZE )
		{
			blockSize = blockSize * 2;

			if ( blockSize > BSIZE ) { blockSize = BSIZE; }
		}

		doneBytes += bytes;
		SendProgressInfo( srcNode->st.size, doneBytes, bytes );
	}

	srcFs->Close( in, 0, Info() );
	in = -1;

	SendProgressInfo( srcNode->st.size, srcNode->st.size, 0 );

	{
		int r = destFs->Close( out, &ret_err, Info() );

		if ( r )
		{
			if ( r == -2 || RedMessage( "Can't close the file:\n", destFs->Uri( destPath ).GetUtf8(), bSkipCancel, destFs->StrError( ret_err ).GetUtf8() ) != CMD_SKIP )
			{
				stopped = true;
			}

			goto err;
		}
		else
		{
			out = -1;
		}
	}

	destFs->SetFileTime( destPath, srcNode->st.m_CreationTime, srcNode->st.m_LastWriteTime, srcNode->st.m_LastWriteTime, 0, Info() );

	return !move || Unlink( srcFs, srcPath );

err:

	if ( in >= 0 ) { srcFs->Close( in, 0, Info() ); }

	if ( out >= 0 ) { destFs->Close( out, 0, Info() ); }

	Unlink( destFs, destPath );

	return !stopped;
}

bool OperCFThread::CopyDir( FS* srcFs, FSPath& __srcPath, FSNode* srcNode, FS* destFs, FSPath& __destPath, bool move )
{
	if ( Info()->Stopped() ) { return false; }

	FSList list;

	int ret_error;

	while ( true )
	{
		int ret = srcFs->ReadDir( &list, __srcPath, &ret_error, Info() );

		if ( ret == -2 ) { return false; }

		if ( !ret ) { break; }

		switch ( RedMessage( _LT( "Can't open directory:\n" ) , srcFs->Uri( __srcPath ).GetUtf8(), bRetrySkipCancel, srcFs->StrError( ret_error ).GetUtf8() ) )
		{
			case CMD_SKIP:
				return true;

			case CMD_RETRY:
				continue;

			default:
				return false;
		}
	}

	while ( destFs->MkDir( __destPath, MkDirMode, &ret_error, Info() ) && !destFs->IsEEXIST( ret_error ) )
	{
		switch ( RedMessage( _LT( "Can't create the directory:\n" ), destFs->Uri( __destPath ).GetUtf8(), bRetrySkipCancel, destFs->StrError( ret_error ).GetUtf8() ) )
		{
			case CMD_CANCEL:
				return false;

			case CMD_SKIP:
				return true;
		}
	}


	FSPath srcPath = __srcPath;
	int srcPos = srcPath.Count();
	FSPath destPath = __destPath;
	int destPos = destPath.Count();


	for ( FSNode* node = list.First(); node; node = node->next )
	{
		if ( Info()->Stopped() ) { return false; }

		srcPath.SetItemStr( srcPos, node->Name() );
		destPath.SetItemStr( destPos, node->Name() );

		if ( !CopyNode( srcFs, srcPath, node, destFs, destPath, move ) ) { return false; }
	}

	destFs->SetFileTime( destPath, srcNode->st.m_CreationTime, srcNode->st.m_LastWriteTime, srcNode->st.m_LastWriteTime, 0, Info() );

	return !move || RmDir( srcFs, __srcPath );
}

static void stripPathFromLastItem(FSPath& path)
{
	FSString* lastItem = path.GetItem(path.Count() - 1);
	if (lastItem)
	{
		const unicode_t* lastU = lastItem->GetUnicode();
		const unicode_t* lastDelim = unicode_strrchr(lastU, DIR_SPLITTER);
		if (lastDelim != 0)
		{
			path.SetItemStr(path.Count() - 1,FSString(lastDelim + 1));
		}
	}
}

bool OperCFThread::CopyNode( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs, FSPath& destPath, bool move )
{
	// XXX blame, blame. In tmp panel name has full path. Strip the path from the last item
	stripPathFromLastItem(destPath);

	if ( srcNode->st.IsLnk() )
	{
		if ( !CopyLink( srcFs, srcPath, srcNode, destFs, destPath, move ) ) { return false; }
	}
	else if ( srcNode->st.IsDir() )
	{
		if ( !CopyDir( srcFs, srcPath, srcNode, destFs, destPath, move ) ) { return false; }
	}
	else
	{
		if ( !CopyFile( srcFs, srcPath, srcNode, destFs, destPath, move ) ) { return false; }
	}

	return true;
}

bool OperCFThread::Copy( FS* srcFs, FSPath& __srcPath, FSList* list, FS* destFs, FSPath& __destPath, cstrhash<bool, unicode_t>& resList )
{
	if ( list->Count() <= 0 ) { return true; }

	FSPath srcPath = __srcPath;
	int srcPos = srcPath.Count();
	FSPath destPath = __destPath;
	int destPos = destPath.Count();

	FSStat st;
	int ret_error;
	int res = destFs->Stat( __destPath, &st, &ret_error, Info() );

	if ( res == -2 ) { return false; }

	if ( res && !destFs->IsENOENT( ret_error ) )
	{
		RedMessage( _LT( "Can't copy to:\n" ), destFs->Uri( destPath ).GetUtf8(), bOk, destFs->StrError( ret_error ).GetUtf8() );
		return false;
	}

	bool exist = ( res == 0 );


	if ( list->Count() > 1 )
	{
		//если файлов >1 то копировать можно только в каталог
		if ( !exist )
		{
			RedMessage( _LT( "Can't copy files, destination is not found:\n" ), destFs->Uri( __destPath ).GetUtf8(), bOk );
			return false;
		}

		if ( !st.IsDir() )
		{
			RedMessage( _LT( "Destination is not directory:\n" ), destFs->Uri( __destPath ).GetUtf8(), bOk );
			return false;
		}

		for ( FSNode* node = list->First(); node; node = node->next )
		{
			if ( Info()->Stopped() ) { return false; }
			srcPath.SetItemStr( srcPos, node->Name() );
			destPath.SetItemStr(destPos, node->Name() );

			if ( !CopyNode( srcFs, srcPath, node, destFs, destPath, false ) ) { return false; }

			resList[node->Name().GetUnicode()] = true;
		}
	}
	else
	{
		// 1 element

		if ( exist && st.IsDir() )
		{
			destPath.SetItemStr(destPos, list->First()->Name());
		}

		srcPath.SetItemStr( srcPos, list->First()->Name() );

		if ( !CopyNode( srcFs, srcPath, list->First(), destFs, destPath, false ) ) { return false; }

		resList[list->First()->Name().GetUnicode()] = true;
	};

	return true;
}

void CopyThreadFunc( OperThreadNode* node )
{
	try
	{
		MutexLock lock( node->GetMutex() );

		if ( !node->Data() ) { return; }

		OperCFData* data = ( ( OperCFData* )node->Data() );
		OperCFThread thread( "Copy", data->Parent(), node );

		clPtr<FS> srcFs = data->srcFs;
		FSPath srcPath = data->srcPath;
		clPtr<FS> destFs = data->destFs;
		FSPath destPath = data->destPath;
		clPtr<FSList> list = data->srcList;

		lock.Unlock();//!!!

		try
		{
			clPtr<cstrhash<bool, unicode_t> > resList = new cstrhash<bool, unicode_t>;
			thread.Copy( srcFs.Ptr(), srcPath, list.ptr(), destFs.Ptr(), destPath, *( resList.ptr() ) );
			lock.Lock(); //!!!

			if ( !node->NBStopped() )
			{
				data->resList = resList;
				data->executed = true;
			}
		}
		catch ( cexception* ex )
		{
			lock.Lock(); //!!!

			if ( !node->NBStopped() ) //this check is needed because 'data' may be not valid
			{
				data->errorString = ex->message();
			}

			ex->destroy();
		}
	}
	catch ( cexception* ex )
	{
		fprintf( stderr, "ERR!!! Error exception in CopyThreadFunc - '%s'\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "ERR!!! Unhandled exception in CopyThreadFunc\n" );
	}
}

clPtr<cstrhash<bool, unicode_t> > CopyFiles( clPtr<FS> srcFs, FSPath& srcPath, clPtr<FSList> list, clPtr<FS> destFs, FSPath& destPath, NCDialogParent* parent )
{
	CopyDialog dlg( parent );
	dlg.threadData.Clear();
	dlg.threadData.srcFs = srcFs;
	dlg.threadData.srcPath = srcPath;
	dlg.threadData.srcList = list;

	dlg.threadData.destFs = destFs;
	dlg.threadData.destPath = destPath;

	dlg.RunNewThread( "Copy", CopyThreadFunc, &dlg.threadData ); //may throw exception
	dlg.Enable();
	dlg.Show();
	dlg.DoModal();
	dlg.StopThread();
	return dlg.threadData.resList;
}





/////////////////////////////////////////////////  MOVE files  /////////////////////////////////

/*
   0 - ok
   1 - need copy
   -1 - stop
*/

int OperCFThread::MoveFile( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs,  FSPath& destPath )
{
	if ( srcFs != destFs && !srcFs->Equal( destFs ) ) { return 1; }

	if ( IsSameFile( srcFs, srcPath, &( srcNode->st ), destFs, destPath ) )
	{
		RedMessage( _LT( "Can't move file to itself:\n" ), srcFs->Uri( srcPath ).GetUtf8() );
		return -1;
	}

	int ret_error;

	FSStat st;

	if ( !destFs->Stat( destPath, &st, &ret_error, Info() ) )
	{
		if ( !commitAll )
		{
			switch ( RedMessage( _LT( "Owerwrite  file?\n" ), destFs->Uri( destPath ).GetUtf8(),
			                     bOkAllNoCancel,
			                     destFs->StrError( ret_error ).GetUtf8() ) )
			{
				case CMD_ALL:
					commitAll = true; //no break

				case CMD_OK:
					break;

				case CMD_NO:
					return 0;

				default:
					return -1;
			}
		}

		if ( destFs->Delete( destPath, &ret_error, Info() ) )
			return RedMessage( _LT( "Can't delete the file:\n" ), destFs->Uri( destPath ).GetUtf8(),
			                   bSkipCancel, destFs->StrError( ret_error ).GetUtf8() ) == CMD_SKIP ? 0 : -1 ;
	}

	if ( srcFs->Rename( srcPath, destPath, &ret_error, Info() ) )
	{
		if ( srcFs->IsEXDEV( ret_error ) ) { return 1; }

		return RedMessage( _LT( "Can't rename the file:\n" ), srcFs->Uri( srcPath ).GetUtf8(), "\nto\n", destFs->Uri( destPath ).GetUtf8(),
		                   bSkipCancel, srcFs->StrError( ret_error ).GetUtf8() ) == CMD_SKIP ? 0 : -1;

	}

	return 0;
}

/*
   0 - ok
   1 - need copy
   -1 - stop
*/

int OperCFThread::MoveDir( FS* srcFs, FSPath& __srcPath, FSNode* srcNode, FS* destFs, FSPath& __destPath )
{
	if ( Info()->Stopped() ) { return -1; }

	if ( srcFs != destFs ) { return 1; }

	FSPath srcPath = __srcPath;
	int srcPos = srcPath.Count();
	FSPath destPath = __destPath;
	int destPos = destPath.Count();

	if ( IsSameFile( srcFs, srcPath, &( srcNode->st ), destFs, destPath ) )
	{
		RedMessage( _LT( "Can't move directory to itself:\n" ), srcFs->Uri( __srcPath ).GetUtf8() );
		return -1;
	}

	FSStat st;
	int ret_error;

	if ( !destFs->Stat( destPath, &st, &ret_error, Info() ) )
	{
		if ( !st.IsDir() )
		{
			switch ( RedMessage( _LT( "Can't copy directory\n" ), srcFs->Uri( srcPath ).GetUtf8(), _LT( "to file" ), "\n", _LT( "Delete the file?" ), destFs->Uri( destPath ).GetUtf8(), bOkSkipCancel ) )
			{
				case CMD_CANCEL:
					return -1;

				case CMD_SKIP:
					return 0;
			}

			if ( !Unlink( destFs, destPath ) ) { return -1; }
		}
		else
		{

			FSList list;

			while ( true )
			{
				int ret = srcFs->ReadDir( &list, srcPath, &ret_error, Info() );

				if ( ret == -2 ) { return -1; }

				if ( !ret ) { break; }

				switch ( RedMessage( _LT( "Can`t open directory:\n" ), srcFs->Uri( __srcPath ).GetUtf8(), bRetrySkipCancel, srcFs->StrError( ret_error ).GetUtf8() ) )
				{
					case CMD_SKIP:
						return 0;

					case CMD_RETRY:
						continue;

					default:
						return -1;
				}
			}

			for ( FSNode* node = list.First(); node; node = node->next )
			{
				if ( Info()->Stopped() ) { return -1; }

				srcPath.SetItemStr( srcPos, node->Name() );
				destPath.SetItemStr( destPos, node->Name() );

				if ( !MoveFile( srcFs, srcPath, node, destFs, destPath ) ) { return -1; }

			}

			destFs->SetFileTime( destPath, srcNode->st.m_CreationTime, srcNode->st.m_LastWriteTime, srcNode->st.m_LastWriteTime, 0, Info() );

			return RmDir( srcFs, srcPath ) ? 0 : -1;
		}
	}

	if ( srcFs->Rename( srcPath, destPath, &ret_error, Info() ) )
	{
		if ( srcFs->IsEXDEV( ret_error ) ) { return 1; }

		return RedMessage( _LT( "Can't rename the directory:\n" ), srcFs->Uri( srcPath ).GetUtf8(), "\nto\n", destFs->Uri( destPath ).GetUtf8(),
		                   bSkipCancel, srcFs->StrError( ret_error ).GetUtf8() ) == CMD_SKIP ? 0 : -1;

	}

	return 0;
}

bool OperCFThread::MoveNode( FS* srcFs, FSPath& srcPath, FSNode* srcNode, FS* destFs, FSPath& destPath )
{

	if ( srcNode->st.IsLnk() )
	{
		int r = MoveFile( srcFs, srcPath, srcNode, destFs, destPath );

		if ( r < 0 ) { return false; }

		if ( r > 0 && !CopyLink( srcFs, srcPath, srcNode, destFs, destPath, true ) ) { return false; }
	}
	else if ( srcNode->st.IsDir() )
	{
		int r = MoveDir( srcFs, srcPath, srcNode, destFs, destPath );

		if ( r < 0 ) { return false; }

		if ( r > 0 && !CopyDir( srcFs, srcPath, srcNode, destFs, destPath, true ) ) { return false; }

	}
	else
	{
		int r = MoveFile( srcFs, srcPath, srcNode, destFs, destPath );

		if ( r < 0 ) { return false; }

		if ( r > 0 && !CopyFile( srcFs, srcPath, srcNode, destFs, destPath, true ) ) { return false; }
	}

	return true;
}


bool OperCFThread::Move( FS* srcFs, FSPath& __srcPath, FSList* list, FS* destFs, FSPath& __destPath )
{
	if ( list->Count() <= 0 ) { return true; }

	FSPath srcPath = __srcPath;
	int srcPos = srcPath.Count();
	FSPath destPath = __destPath;
	int destPos = destPath.Count();

	FSStat st;
	int ret_error;
	int r = destFs->Stat( __destPath, &st, &ret_error, Info() );

	if ( r == -2 ) { return false; }


	if ( list->Count() > 1 )
	{

		// if more than one file, then can only copy in to a catalog
		if ( r )
		{
			RedMessage( _LT( "Can't move files, bad destination directory:\n" ), destFs->Uri( __destPath ).GetUtf8(), bOk, destFs->StrError( ret_error ).GetUtf8() );
			return false;
		}

		if ( !st.IsDir() )
		{
			RedMessage( _LT( "Destination is not directory:\n" ), destFs->Uri( __destPath ).GetUtf8(), bOk );
			return false;
		}

		for ( FSNode* node = list->First(); node; node = node->next )
		{
			srcPath.SetItemStr( srcPos, node->Name() );
			destPath.SetItemStr( destPos, node->Name() );

//printf("MOVE '%s'\n", srcPath.GetUtf8());
			if ( !MoveNode( srcFs, srcPath, node, destFs, destPath ) ) { return false; }
		}

	}
	else
	{
		// 1 element

		if ( r && !destFs->IsENOENT( ret_error ) )
		{
			RedMessage( _LT( "Can't move to:\n" ), destFs->Uri( destPath ).GetUtf8(), bOk, destFs->StrError( ret_error ).GetUtf8() );
			return false;
		}

		if ( !r && st.IsDir() )
		{
			destPath.SetItemStr( destPos, list->First()->Name() );
		}

//		FSNode* node = list->First();

		srcPath.SetItemStr( srcPos, list->First()->Name() );

		if ( !MoveNode( srcFs, srcPath, list->First(), destFs, destPath ) ) { return false; }

	}

	return true;
}

void MoveThreadFunc( OperThreadNode* node )
{
	try
	{
		MutexLock lock( node->GetMutex() );

		if ( !node->Data() ) { return; }

		OperCFData* data = ( ( OperCFData* )node->Data() );
		OperCFThread thread( "Copy", data->Parent(), node );

		clPtr<FS> srcFs = data->srcFs;
		FSPath srcPath = data->srcPath;
		clPtr<FS> destFs = data->destFs;
		FSPath destPath = data->destPath;
		clPtr<FSList> list = data->srcList;

		lock.Unlock();//!!!

		try
		{
			thread.Move( srcFs.Ptr(), srcPath, list.ptr(), destFs.Ptr(), destPath );
		}
		catch ( cexception* ex )
		{
			lock.Lock(); //!!!

			if ( !node->NBStopped() ) //this check is needed because 'data' may be not valid
			{
				data->errorString = ex->message();
			}

			ex->destroy();
		}
	}
	catch ( cexception* ex )
	{
		fprintf( stderr, "ERR!!! Error exception in MoveThreadFunc - '%s'\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "ERR!!! Unhandled exception in MoveThreadFunc\n" );
	}
}

bool MoveFiles( clPtr<FS> srcFs, FSPath& srcPath, clPtr<FSList> list, clPtr<FS> destFs, FSPath& destPath, NCDialogParent* parent )
{
	CopyDialog dlg( parent );
	dlg.threadData.Clear();
	dlg.threadData.srcFs = srcFs;
	dlg.threadData.srcPath = srcPath;
	dlg.threadData.srcList = list;

	dlg.threadData.destFs = destFs;
	dlg.threadData.destPath = destPath;


	dlg.RunNewThread( "Move", MoveThreadFunc, &dlg.threadData ); //may throw exception
	dlg.Enable();
	dlg.Show();
	dlg.DoModal();
	dlg.StopThread();
	return dlg.threadData.errorString.IsEmpty();
}

