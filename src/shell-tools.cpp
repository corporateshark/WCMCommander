/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "shell-tools.h"
#include "nc.h"
#include "vfs-uri.h"
#include "unicode_lc.h"
#include "strmasks.h"

struct ShellLoadDirTD
{
	clPtr<FS> fs;
	FSPath path;

	Mutex mutex;
	bool winClosed;
	bool threadStopped;
	FSCSimpleInfo info;
	clPtr<FSList> list;
	std::string err;
	ShellLoadDirTD( clPtr<FS> f, FSPath& p ): fs( f ), path( p ), winClosed( false ), threadStopped( false ) {}
};

void* ShellLoadDirThreadFunc( void* ptr )
{
	ShellLoadDirTD* data = ( ShellLoadDirTD* )ptr;

	clPtr<FSList> list;

	try
	{
		list = new FSList();

		if ( data->fs.Ptr() )
		{
			int err;

			if ( data->fs->ReadDir( list.ptr(), data->path, &err, &data->info ) )
			{
				data->err = data->fs->StrError( err ).GetUtf8();
			}
		}
	}
	catch ( cexception* ex )
	{
		MutexLock lock( &data->mutex );

		try { data->err = ex->message(); }
		catch ( cexception* x ) { x->destroy(); }

		ex->destroy();
	}
	catch ( ... )
	{
		MutexLock lock( &data->mutex );

		try { data->err = "Unhabdled exception: void *VSThreadFunc(void *ptr) "; }
		catch ( cexception* x ) { x->destroy(); }
	}

	{
		//lock
		MutexLock lock( &data->mutex );

		if ( data->winClosed )
		{
			lock.Unlock(); //!!!
			delete data;
			return 0;
		}

		data->threadStopped = true;
		data->list  = list;
		WinThreadSignal( 0 ); //???
	}

	return 0;
}

class ShellLoadDirDialog: public NCVertDialog
{
public:
	ShellLoadDirTD* data;
	StaticLine _text;
	clPtr<FS> _fs;
	FSPath& _path;

	ShellLoadDirDialog( NCDialogParent* parent, clPtr<FS> fs, FSPath& path );
	virtual void ThreadStopped( int id, void* data );
	virtual ~ShellLoadDirDialog();
};

ShellLoadDirDialog::~ShellLoadDirDialog()
{
	if ( data )
	{
		MutexLock lock( &data->mutex );

		if ( data->threadStopped )
		{
			lock.Unlock(); //!!!
			delete data;
			data = 0;
		}
		else
		{
			data->winClosed = true;
			data->info.SetStop();
			data = 0;
		}
	}
}

ShellLoadDirDialog::ShellLoadDirDialog( NCDialogParent* parent, clPtr<FS> fs, FSPath& path )
	:  NCVertDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( "TAB" ).data(), bListCancel ),
	   data( 0 ),
	   _text( 0, this, utf8_to_unicode( "Read ..." ).data() ),
	   _fs( fs ),
	   _path( path )
{
	_text.Enable();
	_text.Show();
	AddWin( &_text );
	SetEnterCmd( CMD_CANCEL );
	SetPosition();
	data = new ShellLoadDirTD( fs, path );

	try
	{
		this->ThreadCreate( 1, ShellLoadDirThreadFunc, data );
	}
	catch ( ... )
	{
		delete data;
		data = 0;
		throw;
	}
}

void ShellLoadDirDialog::ThreadStopped( int id, void* data )
{
	EndModal( CMD_OK );
}

////////////////////////////////////

class ShellFileListWin: public VListWin
{
	clPtr< ccollect<FSNode*, 0x100> > pList;
	int fontW;
	int fontH;
public:
	ShellFileListWin( Win* parent, int H )
		:  VListWin( Win::WT_CHILD, 0 /*WH_TABFOCUS | WH_CLICKFOCUS*/, 0, parent, VListWin::SINGLE_SELECT, VListWin::BORDER_3D, 0 )
	{
		wal::GC gc( this );
		gc.Set( GetFont() );
		cpoint ts = gc.GetTextExtents( ABCString );
		fontW = ( ts.x / ABCStringLen );
		fontH = ts.y + 4;

		this->SetItemSize( ( fontH > 16 ? fontH : 16 ) + 1, 100 );

		LSize ls;
		ls.x.maximal = 10000;
		ls.x.ideal = ls.x.minimal = fontW * 57;

		if ( ls.x.minimal < 100 ) { ls.x.minimal = 100; }

		ls.y.maximal = 10000;
		ls.y.ideal = ls.y.minimal = fontH * H;

		if ( ls.y.minimal < 100 ) { ls.y.minimal = 100; }

		SetLSize( ls );
	}

	void SetList( clPtr< ccollect<FSNode*, 0x100> > p ) { pList = p; SetCount( pList.ptr() ? pList->count() : 0 ); }

	virtual void DrawItem( wal::GC& gc, int n, crect rect );
	FSNode* GetCurrentNode() { int n = GetCurrent(); return n >= 0 && pList.ptr() && n < pList->count() ? pList->get( n ) : 0; }

	virtual ~ShellFileListWin();
};


static int uiDir = GetUiID( "dir" );

void ShellFileListWin::DrawItem( wal::GC& gc, int n, crect rect )
{
	if ( pList.ptr() && n >= 0 && n < pList->count() )
	{
		FSNode* p = pList->get( n );

		UiCondList ucl;

		if ( ( n % 2 ) == 0 ) { ucl.Set( uiOdd, true ); }

		if ( n == GetCurrent() ) { ucl.Set( uiCurrentItem, true ); }

		if ( p->IsDir() ) { ucl.Set( uiDir, true ); }

		unsigned bg = UiGetColor( uiBackground, uiItem, &ucl, 0xFFFFFF );
		unsigned fg = UiGetColor( uiColor, uiItem, &ucl, 0 );

		gc.Set( GetFont() );
		gc.SetFillColor( bg );
		gc.SetTextColor( fg );
		gc.FillRect( rect );

		static unicode_t dirChar = DIR_SPLITTER;

		if ( p->IsDir() )
		{
			gc.TextOutF( rect.left, rect.top, &dirChar, 1 );
		}

		gc.TextOutF( rect.left + fontW, rect.top, p->Name().GetUnicode() );
	}
	else
	{
		gc.SetFillColor( UiGetColor( uiBackground, uiItem, 0, 0xFFFFFF ) );
		gc.FillRect( rect );
	}
}

ShellFileListWin::~ShellFileListWin() {};

/////////////////////////////////////////

struct ShellFileDlgData
{
	clPtr<FSList> list;
	std::vector<FSNode*> sorted;
	std::vector<unicode_t> mask;
	clPtr< ccollect<FSNode*, 0x100> > filtered;

	void RefreshList( std::vector<unicode_t> mask );

	ShellFileDlgData( clPtr<FSList> l, const unicode_t* s ): list( l )
	{
		sorted = list->GetArray();
		FSNodeVectorSorter::Sort(sorted,true,
#ifdef _WIN32
		                    false
#else
		                    true
#endif
		                  ,SORT_NAME);
		RefreshList( new_unicode_str( s ) );
	}
};

void ShellFileDlgData::RefreshList( std::vector<unicode_t> m )
{
	filtered = 0;
	mask = m;

	if ( list->Count() )
	{
		clPtr< ccollect<FSNode*, 0x100> > p = new ccollect<FSNode*, 0x100>;
		int n = list->Count();

		for ( int i = 0; i < n; i++ )
		{
			if (
#if defined( _WIN32 ) || defined( __APPLE__ )
			   accmask_nocase
#else
			   accmask
#endif
			   ( sorted[i]->GetUnicodeName(), mask.data() ) )
			{
				p->append( sorted[i] );
			}
		}

		filtered = p;
	}
}

//#define BGCOLOR 0x505050 //0xA8B9BC //0xD8E9EC
//#define FGCOLOR 0xFFFFFF


class ShellFileDlg: public NCDialog
{
	Layout layout;
	ShellFileListWin list;
	EditLine line;
	ShellFileDlgData* data;
public:
	ShellFileDlg( NCDialogParent* parent, ShellFileDlgData* d )
		:  NCDialog( createDialogAsChild, 0, parent, utf8_to_unicode( " TAB " ).data(), bListOkCancel ), //BGCOLOR),
		   layout( 10, 10 ),
		   list( this, 15 ),
		   line( 0, this, 0, 0, 20 ),
		   data( d )
	{
		layout.AddWin( &list, 0, 0 );
		list.Enable();
		list.Show();
		layout.AddWin( &line, 1, 0 );
		line.Enable();
		line.Show();

		if ( data->mask.data() ) { line.SetText( data->mask.data() ); }

		line.SetFocus();
		AddLayout( &layout );
		SetEnterCmd( CMD_OK );
		SetPosition();

		list.SetList( data->filtered );
		list.MoveCurrent( 0 );
	};
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual bool EventChildKey( Win* child, cevent_key* pEvent );
	virtual int UiGetClassId();
	FSNode* GetCurrent() { return list.GetCurrentNode(); }
	virtual ~ShellFileDlg();
};

int ShellFileDlg::UiGetClassId() { return GetUiID( "TabDialog" ); }

bool ShellFileDlg::Command( int id, int subId, Win* win, void* d )
{
	if ( id == CMD_ITEM_CLICK && win == &list )
	{
		EndModal( CMD_OK );
	}
	else if ( id == CMD_EDITLINE_INFO && subId == SCMD_EDITLINE_CHANGED && win == &line )
	{
		data->RefreshList( line.GetText() );
		list.SetList( data->filtered );
		list.MoveCurrent( 0 );

		if ( list.IsVisible() ) { list.Invalidate(); }

		return true;
	};

	return NCDialog::Command( id, subId, win, data );
}

bool ShellFileDlg::EventChildKey( Win* child, cevent_key* pEvent )
{
	switch ( pEvent->Key() )
	{
		case VK_UP:
		case VK_DOWN:
		case VK_NEXT:
		case VK_PRIOR:
			return list.EventKey( pEvent );
	};

	return NCDialog::EventChildKey( child, pEvent );
}

ShellFileDlg::~ShellFileDlg() {}



/////////////////////////////////////

static clPtr<FSList> DoShellLoadDirDialog( NCDialogParent* parent, clPtr<FS> fs, FSPath& path )
{
	ShellLoadDirDialog dlg( parent, fs, path );

	if ( dlg.DoModal() == CMD_OK && dlg.data && dlg.data->list.ptr() && dlg.data->list->Count() )
	{
		return dlg.data->list;
	}

	return 0;
}


std::vector<unicode_t> ShellTabKey( NCDialogParent* par, clPtr<FS> fs, FSPath& path, const unicode_t* str, int* cursor )
{
	if ( fs->Type() != FS::SYSTEM ) { return std::vector<unicode_t>(); }

	int len = unicode_strlen( str );
	int n = len;

	if ( cursor && *cursor >= 0 && *cursor < n ) { n = *cursor; }

	int b = -1;
	int ds  = -1;
	int i;
	unicode_t strChar = 0;

	for ( i = 0; i < n; i++ )
	{
		unicode_t c = str[i];

		if ( c <= 32 )
		{
			if ( !strChar ) { b = -1; }
		}
		else if ( c == '"' || c == '\'' )
		{
			if ( strChar )
			{
				if ( strChar == c )
				{
					strChar = 0;
					b = -1;
				}
			}
			else
			{
				strChar = c;
				b = i + 1;
			}
		}
		else
		{
			if ( c == DIR_SPLITTER ) { ds = i; }

			if ( b < 0 ) { b = i; }
		}
	}

	int e;

	for ( e = n; e < len; e++ )
	{
		unicode_t c = str[e];

		if ( c <= 32 )
		{
			if ( !strChar ) { break; }
		}
		else if ( c == '"' || c == '\'' )
		{
			if ( strChar )
			{
				if ( strChar == c ) { break; }
			}
			else
			{
				break;
			}
		}
		else
		{
			if ( c == DIR_SPLITTER ) { break; }
		}
	}

	if ( b < 0 ) { b = n; }

	if ( b >= e ) { return std::vector<unicode_t>(); }

	FSPath searchPath = path;
	int bm = ds >= b ? ds + 1 : b;
	int maskLen = e - bm;
	std::vector<unicode_t> mask( maskLen + 1 );

	for ( i = 0; i < maskLen; i++ )
	{
		mask[i] = str[bm + i];
	}

	mask[i] = 0;

	if ( ds >= b )
	{
		int l = ds - b + 1;
		std::vector<unicode_t> buf( l + 1 );

		for ( i = 0; i < l; i++ )
		{
			buf[i] = str[b + i];
		}

		buf[i] = 0;

		clPtr<FS> f1 = ParzeURI( buf.data(), searchPath, { fs } );

		if ( !f1.Ptr() ) { return std::vector<unicode_t>(); }

		fs = f1;
	}

	clPtr<FSList> list = DoShellLoadDirDialog( par, fs, searchPath );

	if ( list.ptr() )
	{

		FSNode* p = 0;
		ShellFileDlgData data( list, mask.data() );

		if ( data.filtered.ptr() )
		{
			if ( !data.filtered->count() ) { return std::vector<unicode_t>(); }

			if ( data.filtered->count() == 1 )
			{
				p = data.filtered->get( 0 );
			}
			else
			{
				ShellFileDlg dlg( par, &data );

				if ( dlg.DoModal() == CMD_OK )
				{
					p = dlg.GetCurrent();
				}
			}
		}

		if ( p )
		{

			const unicode_t* name = p->GetUnicodeName();

			if ( !name ) { return std::vector<unicode_t>(); } //???

			int nLen = unicode_strlen( name );

			std::vector<unicode_t> buf;

			for ( i = 0; i < b; i++ )
			{
				buf.push_back( str[i] );
			}

			for ( i = b; i <= ds; i++ )
			{
				buf.push_back( str[i] );
			}

			for ( ; *name; name++ )
			{
				buf.push_back( *name );
			}

			if ( cursor ) { *cursor = i + nLen; }

			for ( i = e; i < len; i++ )
			{
				buf.push_back( str[i] );
			}

			buf.push_back( 0 );

			return buf;
		}
	}

	return std::vector<unicode_t>();
}
