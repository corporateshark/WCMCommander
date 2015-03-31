/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "dircalc.h"
#include "string-util.h"
#include "ltext.h"

class OperDirCalcData: public OperData
{
public:
	//after creation these params are touched by search thread only
	clPtr<FS> dirFs;
	FSPath _path;
	clPtr<FSList> dirList;

	/////////////////////////
	Mutex resMutex; // {
	int badDirs;
	int64_t fileCount;
	int64_t folderCount;
	int64_t sumSize;
	FSPath currentPath;
	// } (resMutex)

	//search thread may update, main thread may read after search thread ends only
	FSString errorString;

	OperDirCalcData( NCDialogParent* p, clPtr<FS>& fs, FSPath& path, clPtr<FSList> list ):
		OperData( p ), dirFs( fs ), _path( path ), dirList( list ), badDirs( 0 ),
		fileCount( 0 ), folderCount( 0 ), sumSize( 0 )
	{}

	virtual ~OperDirCalcData();
};

OperDirCalcData::~OperDirCalcData() {}

class OperDirCalcThread: public OperFileThread
{
public:
	OperDirCalcThread( const char* opName, NCDialogParent* par, OperThreadNode* n )
		: OperFileThread( opName, par, n ) {}
	void Calc();
	virtual ~OperDirCalcThread();
private:
	int64_t CalcDir( FS* fs, FSPath path );
};


int64_t OperDirCalcThread::CalcDir( FS* fs, FSPath path )
{
	if ( Info()->Stopped() ) { return -1; }

	{
		//lock
		MutexLock lock( Node().GetMutex() );

		if ( !Node().Data() ) { return -1; }

		OperDirCalcData* data = ( OperDirCalcData* )Node().Data();
		MutexLock l1( &data->resMutex );
		data->currentPath = path;
	}

	Node().SendSignal( 10 );

	FSList list;
	int err;

	if ( fs->ReadDir( &list, path, &err, Info() ) )
	{
		MutexLock lock( Node().GetMutex() );

		if ( !Node().Data() ) { return -1; }

		MutexLock l1( &( ( OperDirCalcData* )Node().Data() )->resMutex );
		( ( OperDirCalcData* )Node().Data() )->badDirs++;
		return -1;
	};

	int count = list.Count();

	std::vector<FSNode*> p = list.GetArray();

	//list.SortByName(p.ptr(), count, true, false);

	int lastPathPos = path.Count();

	FSPath filePath = path;

	int64_t fileCount = 0;

	int64_t folderCount = 0;

	int64_t sumSize = 0;

	int i;

	for ( i = 0; i < count; i++ )
	{
		if ( p[i]->IsDir() )
		{
			folderCount++;
			continue;
		};

		fileCount++;

		if ( p[i]->IsReg() && !p[i]->IsLnk() )
		{
			sumSize += p[i]->Size();
		}
	}

	{
		//lock
		MutexLock lock( Node().GetMutex() );

		if ( !Node().Data() ) { return -1; }

		OperDirCalcData* data = ( OperDirCalcData* )Node().Data();
		MutexLock l1( &data->resMutex );

		data->fileCount += fileCount;
		data->folderCount += folderCount;
		data->sumSize += sumSize;
	}

	Node().SendSignal( 20 );

	for ( i = 0; i < count; i++ )
	{
		if ( p[i]->IsDir() && !p[i]->extType && p[i]->st.link.IsEmpty() )
		{
			if ( Info()->Stopped() )
			{
				return -1;
			}

			path.SetItemStr( lastPathPos, p[i]->Name() );
			sumSize += CalcDir( fs, path );
		}
	}

	return sumSize;
}

void OperDirCalcThread::Calc()
{
	MutexLock lock( Node().GetMutex() );

	if ( !Node().Data() ) { return; }

	OperDirCalcData* CalcData = ( OperDirCalcData* )Node().Data();

	clPtr<FS> fs = CalcData->dirFs;
	FSPath path =  CalcData->_path;
	clPtr<FSList> list = CalcData->dirList;

	lock.Unlock(); //!!!


	//dbg_printf("OperDirCalcThread::Calc list data:");
	//path.dbg_printf("FSPath:");
	//for (FSNode* node = list->First(); node; node = node->next)
	//	dbg_printf("%s\n", node->name.GetUtf8());
	if (list->Count() == 0)
	{ // then calculate current dir size
		CalcDir(fs.Ptr(), path);
	}
	else
	{ // list is not empty: calculate size of objects in the list
		int cnt = path.Count();

		for (FSNode* node = list->First(); node; node = node->next)
		{
			path.SetItemStr(cnt, node->Name()); //-V595

			bool IsDir = node->IsDir() && !node->st.IsLnk();

			if ( IsDir )
			{
				int64_t Size = CalcDir( fs.Ptr(), path);

				if ( Size >= 0 && node && node->originNode ) { node->originNode->st.size = Size; }
				CalcData->folderCount++;
			}
			else
			{
				CalcData->fileCount++;
				CalcData->sumSize += node->st.size;
			}
		}
	}
}

OperDirCalcThread::~OperDirCalcThread() {}

static std::vector<unicode_t> ScanedDirString( const unicode_t* dirName )
{
	ccollect<unicode_t, 100> list;

	if ( dirName )
	{
		for ( int i = 0; i < 100 && *dirName; dirName++, i++ )
		{
			list.append( *dirName );
		}

		if ( *dirName )
		{
			list.append( ' ' );
			list.append( '.' );
			list.append( '.' );
			list.append( '.' );
		}
	}

	list.append( 0 );
	return carray_cat<unicode_t>( utf8_to_unicode( _LT( "Folder:" ) ).data(), utf8_to_unicode( " \"" ).data(), list.ptr(), utf8_to_unicode( "\"" ).data() );
}

class DirCalcThreadWin: public NCDialog
{
	OperDirCalcData* pData;
	Layout lo;

	OperFileNameWin cPathWin;

	int64_t curFileCount;
	int64_t curFolderCount;
	int64_t curSumSize;
	int64_t curBadDirs;

	StaticLine dirString;

	StaticLine fileCountName;
	StaticLine fileCountNum;
	StaticLine folderCountName;
	StaticLine folderCountNum;
	StaticLine sumSizeName;
	StaticLine sumSizeNum;
	StaticLine badDirsName;
	StaticLine badDirsNum;

	void RefreshCounters();
public:
	DirCalcThreadWin( NCDialogParent* parent, const char* name, OperDirCalcData* pD, const unicode_t* dirName )
		:  NCDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( name ).data(), bListOk ),
		   pData( pD ),
		   lo( 12, 10 ),
		   cPathWin( this ),
		   curFileCount( -1 ),
		   curFolderCount( -1 ),
		   curSumSize( -1 ),
		   curBadDirs( -1 ),
		   dirString( 0, this, ScanedDirString( dirName ).data() ),
		   fileCountName( uiVariable, this, utf8_to_unicode( _LT( "Files:" ) ).data() ),
		   fileCountNum( uiValue, this, utf8_to_unicode( "AAAAAAAAAA" ).data() ),
		   folderCountName( uiVariable, this, utf8_to_unicode( _LT( "Folders:" ) ).data() ),
		   folderCountNum( uiValue, this, utf8_to_unicode( "AAAAAAAAAA" ).data() ),
		   sumSizeName( uiVariable, this, utf8_to_unicode( _LT( "Files size:" ) ).data() ),
		   sumSizeNum( uiValue, this, utf8_to_unicode( "AAAAAAAAAAAAAAAAAAAA" ).data() ),
		   badDirsName( uiVariable, this, utf8_to_unicode( _LT( "Not readable folders:" ) ).data() ),
		   badDirsNum( uiValue, this, utf8_to_unicode( "AAAAAAAAAA" ).data() )
	{
		lo.AddWin( &dirString, 0, 0, 0, 3 );
		lo.AddWin( &cPathWin, 9, 0, 9, 3 );
		lo.LineSet( 1, 10 );
		lo.AddWin( &fileCountName, 3, 1 );
		lo.AddWin( &folderCountName, 4, 1 );
		lo.AddWin( &sumSizeName, 5, 1 );
		lo.AddWin( &fileCountNum, 3, 2 );
		lo.AddWin( &folderCountNum, 4, 2 );
		lo.AddWin( &sumSizeNum, 5, 2 );
		lo.LineSet( 6, 7 );
		lo.AddWin( &badDirsName, 7, 1 );
		lo.AddWin( &badDirsNum, 7, 2 );
		lo.LineSet( 8, 10 );

		dirString.Show();
		dirString.Enable();
		cPathWin.Show();
		cPathWin.Enable();
		fileCountName.Show();
		fileCountName.Enable();
		fileCountNum.Show();
		fileCountNum.Enable();

		folderCountName.Show();
		folderCountName.Enable();
		folderCountNum.Show();
		folderCountNum.Enable();

		sumSizeName.Show();
		sumSizeName.Enable();
		sumSizeNum.Show();
		sumSizeNum.Enable();
		badDirsName.Show();
		badDirsName.Enable();
		badDirsNum.Show();
		badDirsNum.Enable();

		lo.SetColGrowth( 3 );
		lo.SetColGrowth( 0 );

		this->AddLayout( &lo );
		this->SetEnterCmd( CMD_OK );
		SetPosition();

		RefreshCounters();
	}

	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual bool EventKey( cevent_key* pEvent );
	virtual bool EventChildKey( Win* child, cevent_key* pEvent );
	virtual void OperThreadStopped();
	virtual void OperThreadSignal( int info );
	virtual ~DirCalcThreadWin();
};

static void SetStaticLineInt64( StaticLine& a, int64_t n )
{
	a.SetTextUtf8( ToString( n ) );
}

void DirCalcThreadWin::RefreshCounters()
{
	int64_t count = 0;
	int64_t folderCount = 0;
	int64_t size = 0;
	int64_t bad = 0;

	{
		MutexLock lock( &pData->resMutex );
		count = pData->fileCount;
		folderCount = pData->folderCount;
		size  = pData->sumSize;
		bad   = pData->badDirs;
	}

	if ( curFileCount != count )
	{
		SetStaticLineInt64( fileCountNum, count );
		curFileCount = count;
	}

	if ( curFolderCount != folderCount )
	{
		SetStaticLineInt64( folderCountNum, folderCount );
		curFolderCount = folderCount;
	}

	if ( curSumSize != size )
	{
		sumSizeNum.SetTextUtf8( ToStringGrouped( size ) );
		curSumSize = size;
	}

	if ( curBadDirs != bad ) { SetStaticLineInt64( badDirsNum, bad ); curBadDirs = bad; }
}

void DirCalcThreadWin::OperThreadSignal( int info )
{
	RefreshCounters();

	MutexLock lock( &pData->resMutex );

	if ( pData->currentPath.Count() > 0 )
	{
		cPathWin.SetText( pData->currentPath.GetUnicode() );
		pData->currentPath.Clear();
	}
}

bool DirCalcThreadWin::Command( int id, int subId, Win* win, void* data )
{
	return NCDialog::Command( id, subId, win, data );
}

void DirCalcThreadWin::OperThreadStopped()
{
	cPathWin.SetText( utf8_to_unicode( _LT( "Scan done" ) ).data() );
	RefreshCounters();
}

bool DirCalcThreadWin::EventKey( cevent_key* pEvent )
{
	if ( pEvent->Type() == EV_KEYDOWN )
	{
		switch ( pEvent->Key() )
		{
			case VK_F3:
				EndModal( CMD_CANCEL );
				return true;
		}
	};

	return NCDialog::EventKey( pEvent );
}

bool DirCalcThreadWin::EventChildKey( Win* child, cevent_key* pEvent )
{
	if ( pEvent->Type() == EV_KEYDOWN )
	{
		switch ( pEvent->Key() )
		{
			case VK_F3:
				EndModal( CMD_CANCEL );
				return true;
		}
	};

	return NCDialog::EventChildKey( child, pEvent );
}

DirCalcThreadWin::~DirCalcThreadWin() {}

void DirCalcThreadFunc( OperThreadNode* node )
{
	try
	{

		MutexLock lock( node->GetMutex() );

		if ( !node->Data() ) { return; }

		OperDirCalcData* data = ( ( OperDirCalcData* )node->Data() );
		OperDirCalcThread thread( "calc dir", data->Parent(), node );

		lock.Unlock();//!!!

		try
		{
			thread.Calc();
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
		fprintf( stderr, "ERR!!! Error exception in SearchFileThreadFunc - '%s'\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "ERR!!! Unhandled exception in SearchFileThreadFunc\n" );
	}
}

bool DirCalc( clPtr<FS> f, FSPath& path, clPtr<FSList> list, NCDialogParent* parent )
{
	bool doCurrentDir = list->Count() == 0;

	OperDirCalcData data(parent, f, path, list);
	DirCalcThreadWin dlg(parent, doCurrentDir ? _LT("Current folder metrics") : _LT("Selected folder(s) metrics"), &data, f->Uri(path).GetUnicode());

	dlg.RunNewThread( "Folder calc", DirCalcThreadFunc, &data ); //may throw exception

	dlg.Enable();
	dlg.Show();

	int cmd = dlg.DoModal();
	dlg.StopThread();

	if ( cmd != CMD_OK )
	{
		return false;
	}

	return true;
}
