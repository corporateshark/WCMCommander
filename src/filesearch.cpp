/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "filesearch.h"
#include "string-util.h"
#include "search-dlg.h"
#include "search-tools.h"
#include "ltext.h"
#include "unicode_lc.h"
#include "strmasks.h"
#include "ncwin.h"
#include "globals.h"

#include <unordered_map>

SearchAndReplaceParams searchParams;

struct SearchItemNode
{
	bool m_Added;
	int dirId;
	charset_struct* cs;
	clPtr<FSNode> fsNode; //if empty, then it's just a directory which contains files down the list after it

	SearchItemNode( )
		: m_Added( false ), dirId( -1 ), cs( 0 )
	{}

	SearchItemNode( int di, FSNode* pNode, charset_struct* _c )
		: m_Added( false ), dirId( di ), cs( _c ), fsNode( pNode ? new FSNode( *pNode ) : ( ( FSNode* )0 ) )
	{}
};

struct SearchDirNode: public iIntrusiveCounter
{
	int id;
	FSPath path;
	SearchDirNode(): id( -1 ) {}
	SearchDirNode( int _id, FSPath& p ): id( _id ), path( p ) {}
};


/* this blocks contains info about found items from search thread
   IDs of directories set by search thread and unique per thread

   SearchItemNode comes to the list in the same order by adding to the end
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
	//after creation these params maybe touched by search thread only
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

	//search thread may change, main thread may use only after search thread ends
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
		int bufSize = 16000;
		std::vector<char> buf( bufSize + maxLen );

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

//				int n = bufSize - count;
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

	FSNodeVectorSorter::Sort(p, true, false, SORT_NAME);

	//check by mask
	const unicode_t* mask = this->searchParams.m_SearchMask.data();

	int lastPathPos = path.Count();

	FSPath filePath = path;

	int i;

	std::vector<unicode_t> Mask = new_unicode_str( mask );

	for ( i = 0; i < count; i++ )
	{
		clMultimaskSplitter Splitter( Mask );

		if ( Splitter.CheckAndFetchAllMasks_NoCase( p[i]->Name().GetUnicode() ) )
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
	std::unordered_map<int, clPtr<SearchDirNode> > m_DirHash;
	std::vector<SearchItemNode> m_ItemList;
	int fontW;
	int fontH;
	clPtr<FS> m_FileSystem;
	NCWin* m_NCWin UNUSED;

public:
	SearchListWin( Win* parent, const clPtr<FS>& FileSystem, NCWin* ncwin )
		: VListWin( Win::WT_CHILD, WH_TABFOCUS | WH_CLICKFOCUS, 0, parent, VListWin::SINGLE_SELECT, VListWin::BORDER_3D, 0 )
		, m_FileSystem( FileSystem )
		, m_NCWin( ncwin )
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

		for ( size_t i = 0; i < ( int )p->m_DirList.size(); i++ )
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

		if ( this->GetPageFirstItem() + this->GetPageItemCount() + 1 < ( int )m_ItemList.size() )
		{
			return;
		}

		Invalidate();
	}

	bool GetCurrentURI( std::vector<unicode_t>* uri )
	{
		if ( GetCurrent() < 0 || GetCurrent() > GetCount() ) { return false; }

		SearchItemNode* t = &( m_ItemList[GetCurrent()] );

		auto i = m_DirHash.find( t->dirId );

		if ( i == m_DirHash.end() ) { return false; }

		clPtr<SearchDirNode> d = i->second;

		if ( !d ) { return false; }

		FSPath path = d->path;

		if ( t->fsNode.ptr() )
		{
			path.SetItemStr( path.Count(), t->fsNode->Name() );
		}

		FSString CurrentURI = m_FileSystem->Uri( path );

		if ( uri ) { *uri = new_unicode_str( CurrentURI.GetUnicode() ); }

		return true;
	}

	bool GetCurrentPath( FSPath* p )
	{
		if ( GetCurrent() < 0 || GetCurrent() > GetCount() ) { return false; }

		SearchItemNode* t = &( m_ItemList[GetCurrent()] );

		auto i = m_DirHash.find( t->dirId );

		if ( i == m_DirHash.end() ) { return false; }

		clPtr<SearchDirNode> d = i->second;

		if ( !d ) { return false; }

		FSPath path = d->path;

		if ( t->fsNode.ptr() )
		{
			path.SetItemStr( path.Count(), t->fsNode->Name() );
		}

		if ( p ) { *p = path; }

		return true;
	}

	bool EventKey( cevent_key* pEvent ) override
	{
		/*
		      bool Pressed = pEvent->Type() == EV_KEYDOWN;

		      if ( Pressed && m_NCWin )
		      {
		         std::vector<unicode_t> URI;

		         switch ( pEvent->Key() )
		         {
		         case VK_F3:
		         {
		            if ( this->GetCurrentURI( &URI ) )m_NCWin->StartViewer( URI, 0 );
		            break;
		         }
		         case VK_F4:
		         {
		            if ( this->GetCurrentURI( &URI ) ) m_NCWin->StartEditor( URI, 0, 0 );
		            break;
		         }
		         default:
		            break;
		         }
		      }
		*/
		return VListWin::EventKey( pEvent );
	}

	virtual void DrawItem( wal::GC& gc, int n, crect rect );
	
	void FillFoundItemsList(std::list<FSPath>& foundItemsList, bool fromTmpFS) const
	{
		if (fromTmpFS) // put path from names
		{
			for (std::vector<SearchItemNode>::const_iterator it = m_ItemList.begin(); it != m_ItemList.end(); ++it)
			{
				if (it->fsNode) // m_ItemList has dummy 'dir' entries, where fsNode is null. Skip them
				{
					FSPath fullPath(it->fsNode.Ptr()->name);
					foundItemsList.push_back(fullPath);
				}
			}
		}
		else
		{
			for (std::vector<SearchItemNode>::const_iterator it = m_ItemList.begin(); it != m_ItemList.end(); ++it)
			{
				if (it->fsNode) // m_ItemList has dummy 'dir' entries, where fsNode is null. Skip them
				{
					FSPath fullPath(m_DirHash.at(it->dirId)->path);
					fullPath.PushStr(it->fsNode.Ptr()->name);
					//fullPath.dbg_printf("SearchListWin::GetFoundItemsList adding file:");
					foundItemsList.push_back(fullPath);
				}
			}
		}
	}

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
	if ( n >= 0 && n < ( int )this->m_ItemList.size() )
	{
//		bool frame = false;
		UiCondList ucl;

		if ( ( n % 2 ) == 0 ) { ucl.Set( uiOdd, true ); }

		if ( n == this->GetCurrent() )
		{
			ucl.Set( uiCurrentItem, true );
		}

		unsigned bg = UiGetColor( uiBackground, uiItem, &ucl, 0xFFFFFF );
		unsigned textColor = UiGetColor( uiColor, uiItem, &ucl, 0 );
//		unsigned frameColor = UiGetColor( uiFrameColor, uiItem, &ucl, 0 );;
		/*
		      if ( n == this->GetCurrent() )
		      {
		         frame = true;
		      }
		*/
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

			auto i = m_DirHash.find( m_ItemList[n].dirId );

			if ( i != m_DirHash.end() )
			{
				clPtr<SearchDirNode> d = i->second;

				if ( d ) { txt = d->path.GetUnicode( ); }
			}

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
		: NCDialog(::createDialogAsChild, 0, parent, utf8_to_unicode(name).data(), bListOkCancelPanel),
		   pData( pD ),
		   lo( 10, 10 ),
		   listWin( this, pD->searchFs, dynamic_cast<NCWin*>( parent ) ),
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

		// set the size of the path text line
		wal::LSize s = cPathWin.GetLSize();
		s.x = parent->GetLSize().x;
		cPathWin.SetLSize( s );
	}

	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual void OperThreadStopped();
	virtual void OperThreadSignal( int info );
	virtual ~SearchFileThreadWin();
	bool GetCurrentPath( FSPath* p ) { return listWin.GetCurrentPath( p ); }
	void FillFoundItemsList(std::list<FSPath>& foundItemsList, bool fromTmpFS) const { listWin.FillFoundItemsList(foundItemsList, fromTmpFS); }
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

CoreCommands SearchFile(clPtr<FS> f, FSPath p, NCDialogParent* parent, FSPath* retPath, std::list<FSPath>& foundItemsList)
{
	if ( !DoFileSearchDialog( parent, &searchParams ) )
	{
		return CMD_CANCEL;
	}

	if ( !searchParams.m_SearchMask.data() || !searchParams.m_SearchMask[0] ) { return CMD_CANCEL; }

	std::string utf8Mask = unicode_to_utf8( searchParams.m_SearchMask.data() );

	clPtr<MegaSearcher> megaSearcher;

	if ( searchParams.m_SearchText.data() && searchParams.m_SearchText[0] )
	{
		megaSearcher = new MegaSearcher();

		if ( !megaSearcher->Set( searchParams.m_SearchText.data(), searchParams.m_CaseSensitive, 0 ) )
		{
			NCMessageBox( parent,  _LT( "File search" ) ,  _LT( "can't search this text" ) ,  true );
			return CMD_CANCEL;
		}
	}


	OperSearchData data( parent, searchParams, f, p, megaSearcher );
	SearchFileThreadWin dlg( parent, carray_cat<char>( _LT( "Search:" ), utf8Mask.data() ).data(), &data );
	dlg.RunNewThread( "Search file", SearchFileThreadFunc, &data ); //may throw exception
	dlg.Enable();
	dlg.Show();
	int cmd = dlg.DoModal();
	dlg.StopThread();

	if (cmd == CMD_PUT_RESULTS_TO_TEMP_PANEL)
	{
		dlg.FillFoundItemsList(foundItemsList, f->Type() == FS::TMP);
		//dbg_printf("filesearch.cpp.SearchFile list for panel:\n");
		//for (std::list<FSPath>::iterator it = foundItemsList.begin(); it != foundItemsList.end(); ++it)
		//	it->dbg_printf("");
		return CMD_PUT_RESULTS_TO_TEMP_PANEL;
	}
	else if ( cmd == CMD_OK )
	{
		return dlg.GetCurrentPath(retPath) ? CMD_OK : CMD_CANCEL;
	}
	else
		return CMD_CANCEL;
}
