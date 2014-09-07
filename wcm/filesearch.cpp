#include "filesearch.h"
#include "string-util.h"
#include "search-dlg.h"
#include "search-tools.h"
#include "ltext.h"
#include "unicode_lc.h"

SearchAndReplaceParams searchParams;

struct SearchItemNode
{
	bool m_Added;
	int dirId;
	charset_struct* cs;
	clPtr<FSNode> fsNode; //если пусто, то это просто директорий в котором лежат файлы следующие в списке за ним
	SearchItemNode( )
	: m_Added(false), dirId( -1 ), cs( 0 )
	{}
	SearchItemNode( int di, FSNode* pNode, charset_struct* _c )
	: m_Added(false), dirId( di ), fsNode( pNode ? new FSNode( *pNode ) : ( ( FSNode* )0 ) ), cs( _c )
	{}
};

struct SearchDirNode: public iIntrusiveCounter
{
	int id;
	FSPath path;
	SearchDirNode(): id( -1 ) {}
	SearchDirNode( int _id, FSPath& p ): id( _id ), path( p ) {}
};


/* такими блоками передается информация о найденом из потока поиска
   id каталого задается потоком поиска и уникальна для директория в одном процессе поиска

   SearchItemNode поступают в список поиска в том же порядке, добавляясь в конец
*/
struct ThreadRetStruct: public iIntrusiveCounter
{
	std::vector< clPtr<SearchDirNode> > m_DirList;
	std::vector< SearchItemNode > m_ItemList;
	void AddDir( int id, FSPath& path )
	{
		m_DirList.push_back( new SearchDirNode( id, path ) );
	}
	void AddItem( int dirId, FSNode* pNode, charset_struct* _c )
	{
		m_ItemList.push_back( SearchItemNode( dirId, pNode, _c ) );
	}
};


class OperSearchData: public OperData
{
public:
	//после создания эти параметры может трогать толькл поток поиска
	SearchAndReplaceParams searchParams;
	clPtr<FS> searchFs;
	FSPath searchPath;
	clPtr<MegaSearcher> megaSearcher;

	Mutex resMutex; // {
	int found;
	int badDirs;
	int badFiles;
	clPtr<ThreadRetStruct> res;
	FSPath currentPath;
	// } (resMutex)

	//поисковый поток может менять, основной поток может использовать только после завершения поискового потока
	FSString errorString;

	OperSearchData( NCDialogParent* p, SearchAndReplaceParams& sParams, clPtr<FS>& fs, FSPath& path, clPtr<MegaSearcher> searcher ):
		OperData( p ), searchParams( sParams ), searchFs( fs ), searchPath( path ), megaSearcher( searcher ),
		found ( 0 ), badDirs( 0 ), badFiles( 0 )
	{}

	virtual ~OperSearchData();
};

OperSearchData::~OperSearchData() {}

class OperSearchThread: public OperFileThread
{
	SearchAndReplaceParams searchParams;
	int lastDirId;
	int NewDirId() { return ++lastDirId; }
public:
	OperSearchThread( const char* opName, NCDialogParent* par, OperThreadNode* n, SearchAndReplaceParams& sp )
		: OperFileThread( opName, par, n ), searchParams( sp ), lastDirId( 0 ) {}

	int TextSearch( FS* fs, FSPath& path, MegaSearcher* pSearcher, int* err, FSCInfo* info, charset_struct** cs );

	void SearchDir( FS* fs, FSPath path, MegaSearcher* pSearcher );
	void Search();
	virtual ~OperSearchThread();
};

static bool accmask_nocase( const unicode_t* name, const unicode_t* mask )
{
	if ( !*mask )
	{
		return *name == 0;
	}

	while ( true )
	{
		switch ( *mask )
		{
			case 0:
				for ( ; *name ; name++ )
					if ( *name != '*' )
					{
						return false;
					}

				return true;

			case '?':
				break;

			case '*':
				while ( *mask == '*' ) { mask++; }

				if ( !*mask ) { return true; }

				for ( ; *name; name++ )
					if ( accmask_nocase( name, mask ) )
					{
						return true;
					}

				return false;

			default:
				if ( UnicodeLC( *name ) != UnicodeLC( *mask ) ) //(*name != *mask)
				{
					return false;
				}
		}

		name++;
		mask++;
	}
}

int OperSearchThread::TextSearch( FS* fs, FSPath& path, MegaSearcher* pSearcher, int* err, FSCInfo* info, charset_struct** rCS ) //-2 - stop, -1 - err, 1 - true, 0 - false
{
	if ( info && info->Stopped() ) { return -2; }

	int fd = fs->OpenRead( path, FS::SHARE_READ | FS::SHARE_WRITE, err, info );

	if ( fd < 0 ) { return fd; }

	char* sResult = 0;
	int bytes = 0;

	try
	{

		int maxLen = pSearcher->MaxLen();
		int minLen = pSearcher->MinLen();
		int bufSize = 16000 + maxLen;
		std::vector<char> buf( bufSize );

		bytes =  fs->Read( fd, buf.data(), bufSize, err, info );

		if ( bytes > 0 )
		{
			int count = bytes;

			while ( true )
			{
				if ( count >= maxLen )
				{
					int n = count - maxLen + 1;
					int fBytes = 0;
					sResult = pSearcher->Search( buf.data(), buf.data() + n, &fBytes, rCS );

					if ( sResult )
					{
						break;
					}

					int t = count - n;

					if ( t > 0 ) { memmove( buf.data(), buf.data() + n, t ); }

					count = t;
				}

				int n = bufSize - count;
				bytes = fs->Read( fd, buf.data(), bufSize, err, info );

				if ( bytes <= 0 ) { break; }

				count += bytes;
			}

			if ( minLen < maxLen )
			{
				int n = maxLen - minLen;

				for ( ; n > 0 && count < bufSize ; n-- )
				{
					buf[count++] = 0;
				}

				if ( count >= maxLen )
				{
					int n = count - maxLen + 1;
					int fBytes = 0;
					sResult = pSearcher->Search( buf.data(), buf.data() + n, &fBytes, rCS );
				}

			}
		}
	}
	catch ( ... )
	{
		fs->Close( fd, err, info );
		throw;
	}

	fs->Close( fd, err, info );

	if ( bytes < 0 ) { return bytes; }

	return sResult != 0 ? 1 : 0;
}

void OperSearchThread::SearchDir( FS* fs, FSPath path, MegaSearcher* pSearcher )
{
	if ( Info()->Stopped() ) { return; }

	{
		//lock
		MutexLock lock( Node().GetMutex() );

		if ( !Node().Data() ) { return; }

		OperSearchData* data = ( OperSearchData* )Node().Data();
		MutexLock l1( &data->resMutex );
		data->currentPath = path;
	}

	Node().SendSignal( 10 );

	FSList list;
	int err;

	if ( fs->ReadDir( &list, path, &err, Info() ) )
	{
		MutexLock lock( Node().GetMutex() );

		if ( !Node().Data() ) { return; }

		MutexLock l1( &( ( OperSearchData* )Node().Data() )->resMutex );
		( ( OperSearchData* )Node().Data() )->badDirs++;
		return;
	};

	int dirId = -1;

	int count = list.Count();

	std::vector<FSNode*> p = list.GetArray();

	list.SortByName( p.data(), count, true, false );

	//check by mask
	unicode_t* mask = this->searchParams.mask.data();

	int lastPathPos = path.Count();

	FSPath filePath = path;

	int i;

	for ( i = 0; i < count; i++ )
	{
		if ( accmask_nocase( p[i]->Name().GetUnicode(), mask ) )
		{
			charset_struct* charset = 0;

			if ( pSearcher )
			{
				if ( p[i]->IsDir() )
				{
					continue;
				}

				filePath.SetItemStr( lastPathPos, p[i]->Name() );

				int ret = TextSearch( fs, filePath, pSearcher, &err, Info(), &charset );

				if ( ret == -2 ) //stopped
				{
					return;
				}

				if ( ret < 0 )
				{
					MutexLock lock( Node().GetMutex() );

					if ( !Node().Data() ) { return; }

					MutexLock l1( &( ( OperSearchData* )Node().Data() )->resMutex );
					( ( OperSearchData* )Node().Data() )->badFiles++;
				}

				if ( ret <= 0 )
				{
					continue;
				}
			}

			{
				//lock
				MutexLock lock( Node().GetMutex() );

				if ( !Node().Data() ) { return; }

				OperSearchData* data = ( OperSearchData* )Node().Data();
				MutexLock l1( &data->resMutex );

				if ( !data->res.ptr() )
				{
					data->res = new ThreadRetStruct;
				}

				if ( dirId < 0 )
				{
					dirId = this->NewDirId();
					data->res->AddDir( dirId, path );
					data->res->AddItem( dirId, 0, 0 );
				}

				data->found++;
				data->res->AddItem( dirId, p[i], pSearcher && pSearcher->Count() > 1 ? charset : 0 );
			}

			Node().SendSignal( 20 );
		}
	}


	for ( i = 0; i < count; i++ )
	{
		if ( p[i]->IsDir() && !p[i]->extType && p[i]->st.link.IsEmpty() )
		{
			if ( Info()->Stopped() )
			{
				return;
			}

			path.SetItemStr( lastPathPos, p[i]->Name() );
			SearchDir( fs, path, pSearcher );
		}
	}

}

void OperSearchThread::Search()
{
	MutexLock lock( Node().GetMutex() );

	if ( !Node().Data() ) { return; }

	FSPath path = ( ( OperSearchData* )Node().Data() )->searchPath;
	clPtr<FS> fs = ( ( OperSearchData* )Node().Data() )->searchFs;
	clPtr<MegaSearcher> pSearcher = ( ( OperSearchData* )Node().Data() )->megaSearcher;
	lock.Unlock(); //!!!

	SearchDir( fs.Ptr(), path, pSearcher.ptr() );
//printf("OperSearchThread::Search() stopped\n");
}

OperSearchThread::~OperSearchThread() {}


class SearchListWin: public VListWin
{
	cinthash<int, clPtr<SearchDirNode> > m_DirHash;
	std::vector<SearchItemNode> m_ItemList;
	int fontW;
	int fontH;
public:
	SearchListWin( Win* parent )
		:  VListWin( Win::WT_CHILD, WH_TABFOCUS | WH_CLICKFOCUS, 0, parent, VListWin::SINGLE_SELECT, VListWin::BORDER_3D, 0 )
	{
		wal::GC gc( this );
		gc.Set( GetFont() );
		cpoint ts = gc.GetTextExtents( ABCString );
		fontW = ( ts.x / ABCStringLen );
		fontH = ts.y + 4;

		this->SetItemSize( ( fontH > 16 ? fontH : 16 ) + 1, 100 );

		LSize ls;
		ls.x.maximal = 10000;
		ls.x.ideal = 800;
		ls.x.minimal = 100;

		ls.y.maximal = 10000;
		ls.y.ideal = 800;
		ls.y.minimal = 100;

		SetLSize( ls );
	}

	void Add( clPtr<ThreadRetStruct> p )
	{
		if ( !p.ptr() ) { return; }

		for ( size_t i = 0; i < (int)p->m_DirList.size(); i++ )
		{
			m_DirHash[p->m_DirList[i]->id] = p->m_DirList[i];
		}

		for ( size_t i = 0; i < p->m_ItemList.size(); i++ )
		{
			if ( !p->m_ItemList[i].m_Added )
			{
				m_ItemList.push_back( p->m_ItemList[i] );
				p->m_ItemList[i].m_Added = true;
			}
		}

		this->SetCount( m_ItemList.size() );

		if ( GetCurrent() < 0 && this->GetCount() > 0 )
		{
			SetCurrent( 0 );
		}

		this->CalcScroll();

		if ( this->GetPageFirstItem() + this->GetPageItemCount() + 1 < (int)m_ItemList.size() )
		{
			return;
		}

		Invalidate();
	}

	bool GetCurrentPath( FSPath* p )
	{
		if ( GetCurrent() < 0 || GetCurrent() > GetCount() ) { return false; }

		SearchItemNode* t = &( m_ItemList[GetCurrent()] );

		clPtr<SearchDirNode>* d = m_DirHash.exist( t->dirId );

		if ( !d ) { return false; }

		FSPath path = d[0]->path;

		if ( t->fsNode.ptr() )
		{
			path.SetItemStr( path.Count(), t->fsNode->Name() );
		}

		if ( p ) { *p = path; }

		return true;
	}

	virtual void DrawItem( wal::GC& gc, int n, crect rect );
	virtual ~SearchListWin();
};

extern cicon folderIcon;
extern cicon folderIconHi;

int CenterIconHeight( const crect& rect, int IconHeight )
{
	return rect.top + ( rect.Height( ) - IconHeight ) / 2;
}

void SearchListWin::DrawItem( wal::GC& gc, int n, crect rect )
{
	if ( n >= 0 && n < (int)this->m_ItemList.size() )
	{
		bool frame = false;
		UiCondList ucl;

		if ( ( n % 2 ) == 0 ) { ucl.Set( uiOdd, true ); }

		if ( n == this->GetCurrent() )
		{
			ucl.Set( uiCurrentItem, true );
		}

		unsigned bg = UiGetColor( uiBackground, uiItem, &ucl, 0xFFFFFF );
		unsigned textColor = UiGetColor( uiColor, uiItem, &ucl, 0 );
		unsigned frameColor = UiGetColor( uiFrameColor, uiItem, &ucl, 0 );;

		if ( n == this->GetCurrent() )
		{
			frame = true;
		}

		gc.SetFillColor( bg );
		gc.FillRect( rect );

		int x = 0;
		const unicode_t* txt = 0;

		if ( m_ItemList[n].fsNode )
		{
			txt = m_ItemList[n].fsNode->GetUnicodeName();
			x = fontW * 10;

			if ( m_ItemList[n].fsNode->IsDir() )
			{
				gc.DrawIcon( x, CenterIconHeight( rect, folderIcon.Height( ) ), &folderIcon );
			}
			else
			{
				if ( m_ItemList[n].cs )
				{
					gc.Set( GetFont() );
					gc.SetTextColor( textColor );
					gc.TextOutF( rect.left + 10, rect.top + 2, utf8_to_unicode( m_ItemList[n].cs->name ).data() );
				}				
			}

			x += 20;
		}
		else
		{
			gc.SetFillColor( bg ); //!!!
			crect r( rect );
			r.bottom = r.top + 1;
			gc.FillRect( r );
			clPtr<SearchDirNode>* d = m_DirHash.exist( m_ItemList[n].dirId );

			if ( d ) { txt = d[0]->path.GetUnicode(); }

			const int FolderIconMargin = 10;

			gc.DrawIcon( x + FolderIconMargin, CenterIconHeight( rect, folderIcon.Height( ) ), &folderIcon );
			gc.SetLine( textColor );
			gc.MoveTo( 0, rect.top + 1 );
			gc.LineTo( rect.right, rect.top + 1 );
			x += folderIcon.Width( ) + FolderIconMargin;
		}

		int textWidth = x;

		if ( txt )
		{
			gc.SetTextColor( textColor );
			gc.Set( GetFont() );
			textWidth = gc.GetTextExtents( txt ).x;
			gc.SetFillColor( bg );
			gc.TextOutF( rect.left + x, rect.top + 2, txt );
		}

		if ( this->GetItemWidth() < textWidth )
		{
			this->SetItemSize( GetItemHeight(), textWidth );
		}

	}
	else
	{
		gc.SetFillColor( UiGetColor( uiBackground, uiItem, 0, 0xFFFFFF ) );
		gc.FillRect( rect );
	}
}

SearchListWin::~SearchListWin() {};

class SearchFileThreadWin: public NCDialog
{
	OperSearchData* pData;
	Layout lo;
	SearchListWin listWin;
	OperFileNameWin cPathWin;

	int curFound;
	int curBadDirs;
	int curBadFiles;

	StaticLine foundName;
	StaticLine foundCount;
	StaticLine badDirsName;
	StaticLine badDirsCount;
	StaticLine badFilesName;
	StaticLine badFilesCount;

	void RefreshCounters();
public:
	SearchFileThreadWin( NCDialogParent* parent, const char* name, OperSearchData* pD )
		:  NCDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( name ).data(), bListOkCancel ),
		   pData( pD ),
		   lo( 10, 10 ),
		   listWin( this ),
		   cPathWin( this ),
		   curFound( -1 ),
		   curBadDirs( -1 ),
		   curBadFiles( -1 ),
		   foundName( 0, this, utf8_to_unicode( _LT( "Files found:" ) ).data() ),
		   foundCount( 0, this, utf8_to_unicode( "AAAAAAAAAA" ).data() ),
		   badDirsName( 0, this, utf8_to_unicode( _LT( "Bad directories:" ) ).data() ),
		   badDirsCount( 0, this, utf8_to_unicode( "AAAAAAAAAA" ).data() ),
		   badFilesName( 0, this, utf8_to_unicode( _LT( "Not opened files:" ) ).data() ),
		   badFilesCount( 0, this, utf8_to_unicode( "AAAAAAAAAA" ).data() )
	{
		listWin.Show();
		listWin.Enable();
		cPathWin.Show();
		cPathWin.Enable();

		foundName.Show();
		foundName.Enable();
		badDirsName.Show();
		badDirsName.Enable();
		badFilesName.Show();
		badFilesName.Enable();
		foundCount.Show();
		foundCount.Enable();
		badDirsCount.Show();
		badDirsCount.Enable();
		badFilesCount.Show();
		badFilesCount.Enable();

		lo.AddWin( &listWin, 0, 0, 0, 1 );
		lo.AddWin( &cPathWin, 1, 0, 1, 1 );

		lo.LineSet( 2, 10 );

		lo.AddWin( &foundName, 3, 0 );
		lo.AddWin( &badDirsName, 4, 0 );
		lo.AddWin( &badFilesName, 5, 0 );

		lo.AddWin( &foundCount, 3, 1 );
		lo.AddWin( &badDirsCount, 4, 1 );
		lo.AddWin( &badFilesCount, 5, 1 );
		lo.SetColGrowth( 1 );


		this->AddLayout( &lo );
		MaximizeIfChild();
		listWin.SetFocus();
		this->SetEnterCmd( CMD_OK );

		RefreshCounters();

		SetPosition();
	}

	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual void OperThreadStopped();
	virtual void OperThreadSignal( int info );
	virtual ~SearchFileThreadWin();
	bool GetCurrentPath( FSPath* p ) { return listWin.GetCurrentPath( p ); }
};

static void SetStaticLineInt( StaticLine& a, int n )
{
	char buf[64];
	sprintf( buf, "%i", n );
	unicode_t ubuf[64];

	for ( int i = 0; i < 64; i++ )
	{
		ubuf[i] = buf[i];

		if ( !buf[i] ) { break; }
	}

	a.SetText( ubuf );
}

void SearchFileThreadWin::RefreshCounters()
{
	int found = 0;
	int badDirs = 0;
	int badFiles = 0;

	{
		MutexLock lock( &pData->resMutex );
		found = pData->found;
		badDirs = pData->badDirs;
		badFiles = pData->badFiles;
	}

	if ( curFound != found ) { SetStaticLineInt( foundCount, found ); curFound = found; }

	if ( curBadDirs != badDirs ) { SetStaticLineInt( badDirsCount, badDirs ); curBadDirs = badDirs; }

	if ( curBadFiles != badFiles ) { SetStaticLineInt( badFilesCount, badFiles ); curBadFiles = badFiles; }
}

void SearchFileThreadWin::OperThreadSignal( int info )
{
	RefreshCounters();

	MutexLock lock( &pData->resMutex );

	if ( pData->currentPath.Count() > 0 )
	{
		cPathWin.SetText( pData->currentPath.GetUnicode() );
		pData->currentPath.Clear();
	}

	if ( pData->res.ptr() )
	{
		this->listWin.Add( pData->res );
	}
}

bool SearchFileThreadWin::Command( int id, int subId, Win* win, void* data )
{
	if ( id == CMD_ITEM_CLICK && win == &listWin )
	{
		EndModal( CMD_OK );
	}

	return NCDialog::Command( id, subId, win, data );
}

void SearchFileThreadWin::OperThreadStopped()
{
	cPathWin.SetText( utf8_to_unicode( _LT( "Search done" ) ).data() );

	if ( pData->res.ptr() )
	{
		this->listWin.Add( pData->res );
	}
}

SearchFileThreadWin::~SearchFileThreadWin() {}

void SearchFileThreadFunc( OperThreadNode* node )
{
	try
	{
		MutexLock lock( node->GetMutex() );

		if ( !node->Data() ) { return; }

		OperSearchData* data = ( ( OperSearchData* )node->Data() );
		OperSearchThread thread( "find file", data->Parent(), node, data->searchParams );

		lock.Unlock();//!!!

		try
		{
			thread.Search();
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
		fprintf( stderr, "ERR!!! Error exception in SearchFileThreadFunc - '%s'\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "ERR!!! Unhandled exception in SearchFileThreadFunc\n" );
	}
}

bool SearchFile( clPtr<FS> f, FSPath p, NCDialogParent* parent, FSPath* retPath )
{
	if ( !DoFileSearchDialog( parent, &searchParams ) )
	{
		return false;
	}

	if ( !searchParams.mask.data() || !searchParams.mask[0] ) { return false; }

	std::vector<char> utf8Mask = unicode_to_utf8( searchParams.mask.data() );

	clPtr<MegaSearcher> megaSearcher;

	if ( searchParams.txt.data() && searchParams.txt[0] )
	{
		megaSearcher = new MegaSearcher();

		if ( !megaSearcher->Set( searchParams.txt.data(), searchParams.sens, 0 ) )
		{
			NCMessageBox( parent,  _LT( "File search" ) ,  _LT( "can't search this text" ) ,  true );
			return false;
		}
	}


	OperSearchData data( parent, searchParams, f, p, megaSearcher );
	SearchFileThreadWin dlg( parent, carray_cat<char>( _LT( "Search:" ), utf8Mask.data() ).data(), &data );
	dlg.RunNewThread( "Search file", SearchFileThreadFunc, &data ); //может быть исключение
	dlg.Enable();
	dlg.Show();
	int cmd = dlg.DoModal();
	dlg.StopThread();

	if ( cmd != CMD_OK )
	{
		return false;
	}

	return dlg.GetCurrentPath( retPath );
}
