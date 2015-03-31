/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "eloadsave.h"

//////////////////////////////// Load editor file


class OperLoadFileData: public OperData
{
public:
	volatile bool executed;
	clPtr<FS> fs;
	FSPath path;
	FSString errorString;
	clPtr<MemFile> file;
	volatile bool notExist;

	OperLoadFileData( NCDialogParent* p ): OperData( p ), executed( false ), notExist( false ) {}

	void SetNewParams( clPtr<FS> f, FSPath& p )
	{
		executed = false;
		fs = f;
		path = p;
		errorString = "";
	}

	virtual ~OperLoadFileData();
};

OperLoadFileData::~OperLoadFileData() {}


class OperLoadFileThread: public OperFileThread
{
public:
	OperLoadFileThread( const char* opName, NCDialogParent* par, OperThreadNode* n )
		: OperFileThread( opName, par, n ) {}
	virtual void Run();
	virtual ~OperLoadFileThread();
};

void OperLoadFileThread::Run()
{
	MutexLock lock( Node().GetMutex() ); //!!!

	if ( Node().NBStopped() ) { return; }

	OperLoadFileData* data = ( ( OperLoadFileData* )Node().Data() );
	clPtr<FS> fs = data->fs;
	FSPath path = data->path;
	lock.Unlock();

	clPtr<MemFile> file = new MemFile;

	int ret_error;
	int f = fs->OpenRead( path, FS::SHARE_READ, &ret_error, Info() );

	if ( f < 0 )
	{
		if ( fs->IsENOENT( ret_error ) )
		{
			lock.Lock();

			if ( Node().NBStopped() ) { return; }

			data->notExist = true;
			lock.Unlock();
		}

		throw_msg( "%s", fs->StrError( ret_error ).GetUtf8() );
	}

	try
	{
		while ( true )
		{
			char buf[16 * 1024];

			lock.Lock();

			if ( Node().NBStopped() ) { return; }

			lock.Unlock();

			int n = fs->Read( f, buf, sizeof( buf ), &ret_error, Info() );

//printf("!!!\n");
			if ( n < 0 )
			{
				throw_msg( "Can`t load file\n%s", fs->StrError( ret_error ).GetUtf8() );
			}

			if ( n == 0 ) { break; }

			file->Append( buf, n );
		};

		fs->Close( f, 0, Info() );
	}
	catch ( ... )
	{
		fs->Close( f, 0, Info() );
		throw;
	}

	lock.Unlock();

	if ( Node().NBStopped() ) { return; }

	data->file = file;
	data->executed = true;
}

OperLoadFileThread::~OperLoadFileThread() {}


void LoadFileThreadFunc( OperThreadNode* node )
{
	try
	{
		MutexLock lock( node->GetMutex() );

		if ( !node->Data() ) { return; }

		OperLoadFileData* data = ( ( OperLoadFileData* )node->Data() );
		OperLoadFileThread thread( "Load editor file", data->Parent(), node );
		lock.Unlock();//!!!

		try
		{
			thread.Run();
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
		fprintf( stderr, "ERR!!! Error exception in LoadFileThreadFunc - '%s'\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "ERR!!! Unhandled exception in LoadFileThreadFunc\n" );
	}
}


class LoadThreadWin: public NCDialog
{
	bool _ignoreENOENT;
public:
	OperLoadFileData threadData;
	clPtr<MemFile> file;

	LoadThreadWin( NCDialogParent* parent, bool ignoreENOENT )
		:  NCDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( "Loading file" ).data(), bListCancel ),
		   _ignoreENOENT( ignoreENOENT ),
		   threadData( parent )
	{}

	virtual void OperThreadStopped();
	virtual ~LoadThreadWin();
};

void LoadThreadWin::OperThreadStopped()
{
	if ( !threadData.errorString.IsEmpty() && !( _ignoreENOENT && threadData.notExist ) )
	{
		NCMessageBox( ( NCDialogParent* )Parent(), "Loading file", threadData.errorString.GetUtf8(), true );
		EndModal( 0 );
		return;
	}

	file = threadData.file;

	if ( !file.ptr() ) //may be when _ignoreENOENT, create empty
	{
		file = new MemFile;
	}

	EndModal( 100 );
}

LoadThreadWin::~LoadThreadWin() {}


clPtr<MemFile> LoadFile( clPtr<FS> f, FSPath& p, NCDialogParent* parent, bool ignoreENOENT )
{
	LoadThreadWin dlg( parent, ignoreENOENT );
	dlg.threadData.SetNewParams( f, p );
	dlg.RunNewThread( "Load editor file", LoadFileThreadFunc, &dlg.threadData ); //may throw exception
	dlg.DoModal();
	dlg.StopThread();
	return dlg.file;
}



//////////////////////////////////////// Save file

class OperSaveFileData: public OperData
{
public:
	volatile bool executed;
	clPtr<FS> fs;
	FSPath path;
	FSString errorString;
	clPtr<MemFile> file;

	OperSaveFileData( NCDialogParent* p ): OperData( p ), executed( false ) {}

	void SetNewParams( clPtr<FS> f, FSPath& p, clPtr<MemFile> __file )
	{
		executed = false;
		fs = f;
		path = p;
		errorString = "";
		file = __file;
	}

	virtual ~OperSaveFileData();
};

OperSaveFileData::~OperSaveFileData() {}


class OperSaveFileThread: public OperFileThread
{
public:
	OperSaveFileThread( const char* opName, NCDialogParent* par, OperThreadNode* n )
		: OperFileThread( opName, par, n ) {}
	virtual void Run();
	virtual ~OperSaveFileThread();
};

void OperSaveFileThread::Run()
{
	MutexLock lock( Node().GetMutex() ); //!!!

	if ( Node().NBStopped() ) { return; }

	OperLoadFileData* data = ( ( OperLoadFileData* )Node().Data() );
	clPtr<FS> fs = data->fs;
	FSPath path = data->path;
	clPtr<MemFile> file = data->file;
	lock.Unlock();

	file->BeginRead();

	int ret_error;
	int f = fs->OpenCreate( path, true, 0666, 0, &ret_error, Info() );

	if ( f < 0 )
	{
		throw_msg( "%s", fs->StrError( ret_error ).GetUtf8() );
	}

	try
	{
		while ( true )
		{
			char buf[16 * 1024];

			int count = file->NonReadedSize();

			if ( count <= 0 ) { break; }

			if ( count > sizeof( buf ) )
			{
				count = sizeof( buf );
			}

			lock.Lock();

			if ( Node().NBStopped() ) { return; }

			lock.Unlock();

			int r = file->Read( buf, count );

			if ( r != count ) { throw_msg( "Unable to read\n" ); }

			int n = fs->Write( f, buf, count, &ret_error, Info() );

			if ( n < 0 )
			{
				throw_msg( "Can't save file\n%s", fs->StrError( ret_error ).GetUtf8() );
			}

			if ( n != count )
			{
				throw_msg( "Out of disk space\n" );
			}
		};

		if ( fs->Close( f, &ret_error, Info() ) )
		{
			throw_msg( "Can't save file\n%s", fs->StrError( ret_error ).GetUtf8() );
		}
	}
	catch ( ... )
	{
		fs->Close( f, 0, Info() );
		throw;
	}

	lock.Unlock();

	if ( Node().NBStopped() ) { return; }

	data->executed = true;
}

OperSaveFileThread::~OperSaveFileThread() {}


void SaveFileThreadFunc( OperThreadNode* node )
{
	try
	{
		MutexLock lock( node->GetMutex() );

		if ( !node->Data() ) { return; }

		OperSaveFileData* data = ( ( OperSaveFileData* )node->Data() );
		OperSaveFileThread thread( "Save editor file", data->Parent(), node );
		lock.Unlock();//!!!

		try
		{
			thread.Run();
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
		fprintf( stderr, "ERR!!! Error exception in SaveFileThreadFunc - '%s'\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "ERR!!! Unhandled exception in SaveFileThreadFunc\n" );
	}
}

class SaveThreadWin: public NCDialog
{
public:
	OperSaveFileData threadData;
	clPtr<MemFile> file;

	SaveThreadWin( NCDialogParent* parent )
		:  NCDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( "Saving file" ).data(), bListCancel ), threadData( parent ) {}
	virtual void OperThreadStopped();
	virtual ~SaveThreadWin();
};

void SaveThreadWin::OperThreadStopped()
{
	if ( !threadData.errorString.IsEmpty() )
	{
		NCMessageBox( ( NCDialogParent* )Parent(), "Saving file", threadData.errorString.GetUtf8(), true );
		EndModal( 0 );
		return;
	}

	file = threadData.file;
	EndModal( 100 );
}

SaveThreadWin::~SaveThreadWin() {}

bool SaveFile( clPtr<FS> f, FSPath& p, clPtr<MemFile> file, NCDialogParent* parent )
{
	SaveThreadWin dlg( parent );
	dlg.threadData.SetNewParams( f, p, file );
	dlg.RunNewThread( "Save editor file", SaveFileThreadFunc, &dlg.threadData ); //may throw exception
	dlg.Enable();
	dlg.Show();
	dlg.DoModal();
	dlg.StopThread();
	return dlg.threadData.executed;
}

