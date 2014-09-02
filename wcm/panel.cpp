/*
   Copyright (c) by Valery Goryachev (Wal)
*/

//#define panelColors !!!

#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>

#include "wcm-config.h"
#include "ncfonts.h"
#include "ncwin.h"
#include "panel.h"
#include "ext-app.h"
#include "color-style.h"
#include "string-util.h"

#include "icons/folder3.xpm"
#include "icons/folder.xpm"
//#include "icons/executable.xpm"
#include "icons/executable1.xpm"
#include "icons/monitor.xpm"
#include "icons/workgroup.xpm"
#include "icons/run.xpm"

#include "vfs-smb.h"
#include "ltext.h"

int uiPanelSearchWin = GetUiID( "PanelSearchWin" );

PanelSearchWin::PanelSearchWin( PanelWin* parent, cevent_key* key )
	:  Win( Win::WT_CHILD, 0, parent, 0, uiPanelSearchWin ),
	   _parent( parent ),
	   _edit( 0, this, 0, 0, 16, true ),
	   _static( 0, this, utf8_to_unicode( _LT( "Search:" ) ).data() ),
	   _lo( 3, 4 )
{
	_lo.AddWin( &_static, 1, 1 );
	_lo.AddWin( &_edit, 1, 2 );
	_lo.LineSet( 0, 2 );
	_lo.LineSet( 2, 2 );
	_lo.ColSet( 0, 2 );
	_lo.ColSet( 3, 2 );
	_edit.Show();
	_edit.Enable();
	_static.Show();
	_static.Enable();
	SetLayout( &_lo );
	RecalcLayouts();

	LSize ls = _lo.GetLSize();
	ls.x.maximal = ls.x.minimal;
	ls.y.maximal = ls.y.minimal;
	SetLSize( ls );

	if ( _parent ) { _parent->RecalcLayouts(); }

	if ( key ) { _edit.EventKey( key ); }
}

void PanelSearchWin::Repaint()
{
	Win::Repaint();

	_edit.Repaint();
	_static.Repaint();
}

void PanelSearchWin::Paint( wal::GC& gc, const crect& paintRect )
{
	int bc = UiGetColor( uiBackground, 0, 0, 0xD8E9EC );
	crect r = ClientRect();
	Draw3DButtonW2( gc, r, bc, true );
	r.Dec();
	r.Dec();
	gc.SetFillColor( bc );
	gc.FillRect( r );
}


cfont* PanelSearchWin::GetChildFont( Win* w, int fontId )
{
	return dialogFont.ptr();
}

bool PanelSearchWin::Command( int id, int subId, Win* win, void* data )
{
	if ( id == CMD_EDITLINE_INFO && subId == SCMD_EDITLINE_CHANGED )
	{
		std::vector<unicode_t> text = _edit.GetText();

		if ( !_parent->Search( text.data(), false ) )
		{
			unicode_t empty = 0;
			_edit.SetText( oldMask.data() ? oldMask.data() : &empty );
		}
		else
		{
			oldMask = text;
		}

		this->Repaint();

		return true;
	}

	return false;
}


bool PanelSearchWin::EventKey( cevent_key* pEvent )
{
	return EventChildKey( 0, pEvent );
}

void PanelSearchWin::EndSearch( cevent_key* pEvent )
{
	EndModal( 0 );

	if ( pEvent )
	{
		ret_key = new cevent_key( *pEvent );
	}
}


bool PanelSearchWin::EventMouse( cevent_mouse* pEvent )
{
	switch ( pEvent->Type() )
	{
		case EV_MOUSE_DOUBLE:
		case EV_MOUSE_PRESS:
		case EV_MOUSE_RELEASE:
			ReleaseCapture();
			EndSearch( 0 );
			break;
	};

	return true;
}


bool PanelSearchWin::EventChildKey( Win* child, cevent_key* pEvent )
{
	this->Repaint();

	if ( pEvent->Type() != EV_KEYDOWN ) { return false; }

	bool ctrl = ( pEvent->Mod() & KM_CTRL ) != 0;

	if ( ctrl && pEvent->Key() == VK_RETURN )
	{
		std::vector<unicode_t> text = _edit.GetText();
		_parent->Search( text.data(), true );
		return false;
	}

	wchar_t c = pEvent->Char();

	if ( c && c >= 0x20 ) { return false; }

//	printf( "Key = %x\n", pEvent->Key() );

	switch ( pEvent->Key() )
	{
#ifdef _WIN32

		case VK_CONTROL:
		case VK_SHIFT:
#endif
		case VK_LCONTROL:
		case VK_RCONTROL:
		case VK_LSHIFT:
		case VK_RSHIFT:
		case VK_LMENU:
		case VK_RMENU:
		case VK_BACK:
		case VK_DELETE:

			// this comes from 0xfe08 XK_ISO_Next_Group, workaround for https://github.com/corporateshark/WalCommander/issues/22
		case 0xfe08:
			return false;

		case VK_ESCAPE:
			// end search and kill the ESC key
			EndSearch( NULL );
			return false;
	}

	EndSearch( pEvent );

	return true;
}

bool PanelSearchWin::EventShow( bool show )
{
	if ( show )
	{
		LSize ls = _lo.GetLSize();
		ls.x.maximal = ls.x.minimal;
		ls.y.maximal = ls.y.minimal;
		SetLSize( ls );
		crect r = Rect();
		r.right = r.left + ls.x.minimal;
		r.bottom = r.top + ls.y.minimal;
		Move( r, true );
		_edit.SetFocus();
		SetCapture();
	}
	else if ( IsCaptured() )
	{
		ReleaseCapture();
	}

	return true;
}

PanelSearchWin::~PanelSearchWin() {}


clPtr<cevent_key> PanelWin::QuickSearch( cevent_key* key )
{
	_search = new PanelSearchWin( this, key );
	crect r = _footRect;
	LSize ls = _search->GetLSize();
	r.right = r.left + ls.x.minimal;
	r.bottom = r.top + ls.y.minimal;
	_search->Move( r );

	_search->DoModal();

	clPtr<cevent_key> ret = _search->ret_key;
	_search = 0;

	return ret;
}

static bool accmask_nocase_begin( const unicode_t* name, const unicode_t* mask )
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
				return true;

			case '?':
				break;

			case '*':
				while ( *mask == '*' ) { mask++; }

				if ( !*mask ) { return true; }

				for ( ; *name; name++ )
					if ( accmask_nocase_begin( name, mask ) )
					{
						return true;
					}

				return false;

			default:
				if ( UnicodeLC( *name ) != UnicodeLC( *mask ) )
				{
					return false;
				}
		}

		name++;
		mask++;
	}
}


bool PanelWin::Search( unicode_t* mask, bool SearchForNext )
{
//	printf( "mask = %S (%p)\n", (wchar_t*)mask, mask );

	// always match the empty mask with any file
	if ( !mask ) { return true; }

	if ( !*mask ) { return true; }

	int cur = Current();
	int cnt = Count();

	int i;

	int ofs = SearchForNext ? 1 : 0;

	for ( i = cur + ofs; i < cnt; i++ )
	{
		const unicode_t* name = _list.GetFileName( i );;

		if ( name && accmask_nocase_begin( name, mask ) )
		{
			SetCurrent( i );
			return true;
		}
	}

	for ( i = 0; i < cnt - 1 + ofs; i++ )
	{
		const unicode_t* name = _list.GetFileName( i );

		if ( name && accmask_nocase_begin( name, mask ) )
		{
			SetCurrent( i );
			return true;
		}
	}

	return false;
}


void PanelWin::SortByName()
{
	FSNode* p = _list.Get( _current );

	if ( _list.SortMode() == PanelList::SORT_NAME )
	{
		_list.Sort( PanelList::SORT_NAME, !_list.AscSort() );
	}
	else
	{
		_list.Sort( PanelList::SORT_NAME, true );
	}

	if ( p ) { SetCurrent( p->Name() ); }

	Invalidate();
}

void PanelWin::SortByExt()
{
	FSNode* p = _list.Get( _current );

	if ( _list.SortMode() == PanelList::SORT_EXT )
	{
		_list.Sort( PanelList::SORT_EXT, !_list.AscSort() );
	}
	else
	{
		_list.Sort( PanelList::SORT_EXT, true );
	}

	if ( p ) { SetCurrent( p->Name() ); }

	Invalidate();
}

void PanelWin::SortBySize()
{
	FSNode* p = _list.Get( _current );

	if ( _list.SortMode() == PanelList::SORT_SIZE )
	{
		_list.Sort( PanelList::SORT_SIZE, !_list.AscSort() );
	}
	else
	{
		_list.Sort( PanelList::SORT_SIZE, true );
	}

	if ( p ) { SetCurrent( p->Name() ); }

	Invalidate();
}

void PanelWin::SortByMTime()
{
	FSNode* p = _list.Get( _current );

	if ( _list.SortMode() == PanelList::SORT_MTIME )
	{
		_list.Sort( PanelList::SORT_MTIME, !_list.AscSort() );
	}
	else
	{
		_list.Sort( PanelList::SORT_MTIME, true );
	}

	if ( p ) { SetCurrent( p->Name() ); }

	Invalidate();
}


void PanelWin::DisableSort()
{
	FSNode* p = _list.Get( _current );
	_list.DisableSort();

	if ( p ) { SetCurrent( p->Name() ); }

	Invalidate();
}

FSString PanelWin::UriOfDir()
{
	FSString s;
	FS* fs = _place.GetFS();

	if ( !fs ) { return s; }

	return fs->Uri( GetPath() );
}

FSString PanelWin::UriOfCurrent()
{
	FSString s;
	FS* fs = _place.GetFS();

	if ( !fs ) { return s; }

	FSPath path = GetPath();
	FSNode* node = GetCurrent();

	if ( node ) { path.PushStr( node->Name() ); }

	return fs->Uri( path );
}

int uiClassPanel = GetUiID( "Panel" );
int PanelWin::UiGetClassId() { return uiClassPanel; }


unicode_t PanelWin::dirPrefix[] = {'/', 0};
unicode_t PanelWin::exePrefix[] = {'*', 0};

inline int GetTextW( wal::GC& gc, unicode_t* txt )
{
	if ( !txt || !*txt ) { return 0; }

	return gc.GetTextExtents( txt ).x;
}

void PanelWin::OnChangeStyles()
{
	wal::GC gc( ( Win* )0 );
	gc.Set( GetFont() );
	_letterSize[0] = gc.GetTextExtents( ABCString );
	_letterSize[0].x /= ABCStringLen;

	gc.Set( GetFont( 1 ) );
	_letterSize[1] = gc.GetTextExtents( ABCString );
	_letterSize[1].x /= ABCStringLen;


	dirPrefixW = GetTextW( gc, dirPrefix );
	exePrefixW = GetTextW( gc, exePrefix );

	_itemHeight = _letterSize[0].y;

	int headerH = _letterSize[1].y + 2 + 2;

	if ( headerH < 10 ) { headerH = 10; }

	_lo.LineSet( 1, headerH );
	_lo.LineSet( 2, 1 );

	int footH = _letterSize[1].y * 3 + 3 + 3;

//	if (footH<20) footH=20;

	_lo.LineSet( 5, footH );
	_lo.LineSet( 4, 1 );


	if ( _itemHeight <= 16 ) //чтоб иконки влезли
	{
		_itemHeight = 16;
	}

	RecalcLayouts(); //получается двойной пересчет, но хер ли делать

//как в EventSize (подумать)
	Check();
	bool v = _scroll.IsVisible();
	SetCurrent( _current );

	if ( v != _scroll.IsVisible() )
	{
		Check();
	}
}

static int* CheckMode( int* m )
{
	switch ( *m )
	{
		case PanelWin::BRIEF:
		case PanelWin::MEDIUM:
		case PanelWin::FULL:
		case PanelWin::FULL_ST:
		case PanelWin::FULL_ACCESS:
		case PanelWin::TWO_COLUMNS:
			break;

		default:
			*m = PanelWin::MEDIUM;
	}

	return m;
}

PanelWin::PanelWin( Win* parent, int* mode )
	:
	NCDialogParent( WT_CHILD, 0, 0, parent ),
	_lo( 7, 4 ),
	_scroll( 0, this, true ), //, false), //bug with autohide and layouts
	_itemHeight( 1 ),
	_rows( 0 ),
	_cols( 0 ),
	_first( 0 ),
	_current( 0 ),
	_viewMode( CheckMode( mode ) ), //MEDIUM),
	_inOperState( false ),
	_operData( ( NCDialogParent* )parent ),
	_list( ::wcmConfig.panelShowHiddenFiles, ::wcmConfig.panelCaseSensitive )
{
	_lo.SetLineGrowth( 3 );
	_lo.SetColGrowth( 1 );

	_lo.ColSet( 0, 4 );
	_lo.ColSet( 3, 4 );

	_lo.LineSet( 0, 4 );
	_lo.LineSet( 6, 4 );

	_lo.ColSet( 1, 32, 100000 );
	_lo.LineSet( 3, 32, 100000 );

	_lo.AddRect( &_headRect, 1, 1, 1, 2 );
	_lo.AddRect( &_centerRect, 3, 1 );
	_lo.AddRect( &_footRect, 5, 1, 5, 2 );

	_lo.AddWin( &_scroll, 3, 2 );


	OnChangeStyles();

	_scroll.SetManagedWin( this );
	_scroll.Enable();
	_scroll.Show( Win::SHOW_INACTIVE );

	NCDialogParent::AddLayout( &_lo );


	try
	{
		FSPath path;
		clPtr<FS> fs =  ParzeCurrentSystemURL( path );
		LoadPath( fs, path, 0, 0, SET );
	}
	catch ( ... )
	{
	}
}

bool PanelWin::IsSelectedPanel()
{
	NCWin* p = ( NCWin* )Parent();
	return p && p->_panel == this;
}

void PanelWin::SelectPanel()
{
	NCWin* p = ( NCWin* )Parent();

	if ( p->_panel == this ) { return; }

	PanelWin* old = p->_panel;
	p->_panel = this;
	old->Invalidate();
	Invalidate();
}


void PanelWin::SetScroll()
{
	ScrollInfo si;
	si.pageSize = _rectList.count();
	si.size = _list.Count();
	si.pos = _first;
	bool visible = _scroll.IsVisible();
	_scroll.Command( CMD_SCROLL_INFO, SCMD_SCROLL_VCHANGE, this, &si );

	if ( visible != _scroll.IsVisible() )
	{
		this->RecalcLayouts();
		Check();
	}
}


bool PanelWin::Command( int id, int subId, Win* win, void* data )
{
	if ( id != CMD_SCROLL_INFO )
	{
		return false;
	}

	int n = _first;
	int pageSize = _rectList.count();

	switch ( subId )
	{
		case SCMD_SCROLL_LINE_UP:
			n--;
			break;

		case SCMD_SCROLL_LINE_DOWN:
			n++;
			break;

		case SCMD_SCROLL_PAGE_UP:
			n -= pageSize;
			break;

		case SCMD_SCROLL_PAGE_DOWN:
			n += pageSize;
			break;

		case SCMD_SCROLL_TRACK:
			n = ( ( int* )data )[0];
			break;
	}

	if ( n + pageSize > _list.Count() )
	{
		n = _list.Count() - pageSize;
	}

	if ( n < 0 ) { n = 0; }

	if ( n != _first )
	{
		_first = n;
		SetScroll();
		Invalidate();
	}

	return true;
}

bool PanelWin::Broadcast( int id, int subId, Win* win, void* data )
{
	if ( id == ID_CHANGED_CONFIG_BROADCAST )
	{
		FSNode* node = GetCurrent();
		FSString s;

		if ( node ) { s.Copy( node->Name() ); }

		bool a = _list.SetShowHidden( wcmConfig.panelShowHiddenFiles );
		bool b = _list.SetCase( wcmConfig.panelCaseSensitive );

//		if (a || b)
//		{

		SetCurrent( _list.Find( s ) );
		Invalidate();

//		}

		return true;
	}

	return false;
}


#define VLINE_WIDTH (3)
#define MIN_BRIEF_CHARS 12
#define MIN_MEDIUM_CHARS 18

void PanelWin::Check()
{
	int width = _centerRect.Width();
	int height = _centerRect.Height();

	int cols;

	CheckMode( _viewMode );

	switch ( *_viewMode )
	{
		case FULL:
		case FULL_ST:
		case FULL_ACCESS:
			cols = 1;
			break;

		case TWO_COLUMNS:
			cols = 2;
			break;

		default:
		{
			cols = 100;
			int minChars = ( *_viewMode == BRIEF ) ? MIN_BRIEF_CHARS : MIN_MEDIUM_CHARS;

			if ( width < cols * minChars * _letterSize[0].x + ( cols - 1 )*VLINE_WIDTH )
			{
				cols = ( width + VLINE_WIDTH ) / ( minChars * _letterSize[0].x + VLINE_WIDTH );

				if ( cols < 1 ) { cols = 1; }
			}

			if ( *_viewMode != BRIEF && cols > 3 ) { cols = 3; }
		}
	};

	int rows = height / _itemHeight;

	if ( rows < 1 ) { rows = 1; }

	if ( rows > 100 ) { rows = 100; }

	if ( cols > 100 ) { cols = 100; }

	_cols = cols;
	_rows = rows;

	crect rect( 0, 0, 0, 0 );;
	_rectList.clear();
	_rectList.append_n( rect, rows * cols );
	_vLineRectList.clear();
	_vLineRectList.append_n( rect, cols - 1 );
	_emptyRectList.clear();
	_emptyRectList.append_n( rect, cols );

	int wDiv = ( width - ( cols - 1 ) * VLINE_WIDTH ) / cols;
	int wMod = ( width - ( cols - 1 ) * VLINE_WIDTH ) % cols;

	int x = _centerRect.left;
	int n = 0;

	for ( int c = 0; c < cols; c++ )
	{
		int w = wDiv;

		if ( wMod )
		{
			w++;
			wMod--;
		}

		int y = _centerRect.top;

		for ( int r = 0; r < rows; r++, y += _itemHeight )
		{
			_rectList[n++] = crect( x, y, x + w, y + _itemHeight );
		}

		_emptyRectList[c] = crect( x, y, x + w, _centerRect.bottom );

		if ( c < _vLineRectList.count() )
			_vLineRectList[c] = crect( x + w, _centerRect.top,
			                           x + w + VLINE_WIDTH, _centerRect.bottom );

		x += w + VLINE_WIDTH;
	}
}

void PanelWin::EventSize( cevent_size* pEvent )
{
	NCDialogParent::EventSize( pEvent );
	Check();
	bool v = _scroll.IsVisible();
	SetCurrent( _current );

	if ( v != _scroll.IsVisible() )
	{
		Check();
	}

	if ( _search.ptr() )
	{
		crect r = _footRect;
		LSize ls = _search->GetLSize();
		r.right = r.left + ls.x.minimal;
		r.bottom = r.top + ls.y.minimal;
		_search->Move( r );
	}
}


static const int timeWidth =  9; //in characters
static const int dateWidth = 12; //in characters
static const int sizeWidth = 10;
static const int minFileNameWidth = 7;

static const int userWidth = 16;
static const int groupWidth = 16;
static const int accessWidth = 13;


#define PANEL_ICON_SIZE 16

cicon folderIcon( xpm16x16_Folder, PANEL_ICON_SIZE, PANEL_ICON_SIZE );
cicon folderIconHi( xpm16x16_Folder_HI, PANEL_ICON_SIZE, PANEL_ICON_SIZE );

static cicon executableIcon( xpm16x16_Executable, PANEL_ICON_SIZE, PANEL_ICON_SIZE );
static cicon serverIcon( xpm16x16_Monitor, PANEL_ICON_SIZE, PANEL_ICON_SIZE );
static cicon workgroupIcon( xpm16x16_Workgroup, PANEL_ICON_SIZE, PANEL_ICON_SIZE );
static cicon runIcon( xpm16x16_Run, PANEL_ICON_SIZE, PANEL_ICON_SIZE );

bool panelIconsEnabled = true;

static int uiDir = GetUiID( "dir" );
static int uiExe = GetUiID( "exe" );
static int uiBad = GetUiID( "bad" );
static int uiSelectedPanel = GetUiID( "selected-panel" );
static int uiOperState = GetUiID( "oper-state" );
static int uiHidden = GetUiID( "hidden" );
static int uiSelected = GetUiID( "selected" );
//static int uiLineColor = GetUiID("line-color");

void PanelWin::DrawVerticalSplitters( wal::GC& gc, const crect& rect )
{
	UiCondList ucl;

	if ( *_viewMode == FULL_ST )
	{
		int width = rect.Width();

		/*
		   |        fileW       |    sizeW    |   dateW   |  timeW  |
		*/

		int fileW = minFileNameWidth * _letterSize[0].x;
		int sizeW = sizeWidth * _letterSize[0].x;
		int dateW = dateWidth * _letterSize[0].x;
		int timeW = timeWidth * _letterSize[0].x;

		int sizeX = ( width - sizeW - dateW - timeW ) < fileW ? fileW : width - sizeW - dateW -  timeW;
		sizeX += rect.left;
		int dateX = sizeX + sizeW;
		int timeX = dateX + dateW;

		gc.SetLine( UiGetColor( uiLineColor, uiItem, &ucl, 0xFF ) );

		gc.MoveTo( sizeX, rect.top );
		gc.LineTo( sizeX, rect.bottom );

		gc.MoveTo( timeX, rect.top );
		gc.LineTo( timeX, rect.bottom );

		gc.MoveTo( dateX, rect.top );
		gc.LineTo( dateX, rect.bottom );
	}

	if ( *_viewMode == FULL_ACCESS )
	{
		int width = rect.Width();

		int fileW = minFileNameWidth * _letterSize[0].x;
		int accessW = accessWidth * _letterSize[0].x;
		int userW = userWidth * _letterSize[0].x;
		int groupW = groupWidth * _letterSize[0].x;

		int accessX = ( width - accessW - userW - groupW ) < fileW ? fileW : width - accessW - userW - groupW;
		accessX += rect.left;
		int userX = accessX + accessW;
		int groupX = userX + userW;

		gc.SetLine( UiGetColor( uiLineColor, uiItem, &ucl, 0xFF ) );

		gc.MoveTo( accessX, rect.top );
		gc.LineTo( accessX, rect.bottom );

		gc.MoveTo( userX, rect.top );
		gc.LineTo( userX, rect.bottom );

		gc.MoveTo( groupX, rect.top );
		gc.LineTo( groupX, rect.bottom );
	}
}

int PanelWin::GetXMargin() const
{
	int x = 0;

	if ( wcmConfig.panelShowIcons )
	{
		x += PANEL_ICON_SIZE;
	}
	else
	{
		x += dirPrefixW;
	}

	x += 4;

	return x;
}

void PanelWin::DrawItem( wal::GC& gc,  int n )
{
	bool active = IsSelectedPanel() && n == _current;
	int pos = n - _first;

	if ( pos < 0 || pos >= _rectList.count() ) { return; }

	FSNode* p = ( n < 0 || n >= _list.Count() ) ? 0 : _list.Get( n );

	bool isDir = p && p->IsDir();
	bool isExe = !isDir && p && p->IsExe();
	bool isBad = p && p->IsBad();
	bool isSelected = p && p->IsSelected();
	bool isHidden = p && p->IsHidden();

	/*
	PanelItemColors color;
	panelColors->itemF(&color, _inOperState, active, isSelected, isBad, isHidden, isDir, isExe);
	*/

	UiCondList ucl;

	if ( isDir ) { ucl.Set( uiDir, true ); }

	if ( isExe ) { ucl.Set( uiExe, true ); }

	if ( isBad ) { ucl.Set( uiBad, true ); }

	if ( IsSelectedPanel() ) { ucl.Set( uiSelectedPanel, true ); }

	if ( n == _current ) { ucl.Set( uiCurrentItem, true ); }

	if ( isHidden ) { ucl.Set( uiHidden, true ); }

	if ( isSelected ) { ucl.Set( uiSelected, true ); }

	if ( _inOperState ) { ucl.Set( uiOperState, true ); }

	if ( n < _list.Count() && ( n % 2 ) == 0 ) { ucl.Set( uiOdd, true ); }

	int color_text = UiGetColor( uiColor, uiItem, &ucl, 0x0 );
	int color_bg = UiGetColor( uiBackground, uiItem, &ucl, 0xFFFFFF );
	int color_shadow = UiGetColor( uiBackground, uiItem, &ucl, 0 );

	gc.SetFillColor( color_bg );

	crect rect = _rectList[pos];

	gc.SetClipRgn( &rect );
	gc.FillRect( rect );
	gc.SetClipRgn( &_centerRect );
	crect frect = _centerRect;
	frect.left = rect.left;
	frect.right = rect.right;
	frect.bottom = ClientRect().bottom;
	DrawVerticalSplitters( gc, frect );
	gc.SetClipRgn( &rect );

	if ( n < 0 || n >= _list.Count() ) { return; }

#if USE_3D_BUTTONS

	if ( active )
	{
		Draw3DButtonW2( gc, rect, color_bg, true );
	}

#endif

	int x = rect.left + 7; //5;
	int y = rect.top;

	gc.SetTextColor( color_text );

	if ( isDir )
	{
		if ( wcmConfig.panelShowIcons )
		{
			switch ( p->extType )
			{
				case FSNode::SERVER:
					serverIcon.DrawF( gc, x, y );
					break;

				case FSNode::WORKGROUP:
					workgroupIcon.DrawF( gc, x, y );
					break;

				default:
					if ( ( ( ( color_bg >> 16 ) & 0xFF ) + ( ( color_bg >> 8 ) & 0xFF ) + ( color_bg & 0xFF ) ) < 0x80 * 3 )
					{
						folderIcon.DrawF( gc, x, y );
					}
					else
					{
						folderIconHi.DrawF( gc, x, y );
					}
			};
		}
		else
		{
			gc.TextOutF( x, y, dirPrefix );
			//x += dirPrefixW;
		}
	}
	else if ( isExe )
	{
		if ( wcmConfig.panelShowIcons )
		{
			executableIcon.DrawF( gc, x, y );
		}
		else
		{
			gc.TextOutF( x, y, exePrefix );
			//x += exePrefixW;
		}
	}
	else
	{
	}

	x += GetXMargin();

	if ( isSelected )
	{
		gc.SetTextColor( color_shadow );
		gc.TextOut( x + 1, y + 1, _list.GetFileName( n ) );
		gc.SetTextColor( color_text );
	}


	if ( *_viewMode == FULL_ST )
	{
//		FSNode *p = _list.Get(n);

		int width = rect.Width();

		int fileW = minFileNameWidth * _letterSize[0].x;
		int sizeW = sizeWidth * _letterSize[0].x;
		int dateW = dateWidth * _letterSize[0].x;
		int timeW = timeWidth * _letterSize[0].x;

		int sizeX = ( width - sizeW - dateW - timeW ) < fileW ? fileW : width - sizeW - dateW - timeW;
		sizeX += rect.left;
		int dateX = sizeX + sizeW;
		int timeX = dateX + dateW;

		if ( p )
		{
			unicode_t ubuf[64];
			p->st.GetPrintableSizeStr( ubuf );
			cpoint size = gc.GetTextExtents( ubuf );
			gc.TextOutF( sizeX + sizeW - size.x - _letterSize[0].x, y, ubuf );

			unicode_t buf[64] = {0};
			p->st.GetMTimeStr( buf );
			gc.TextOutF( dateX + _letterSize[0].x, y, buf );
		}

		gc.SetLine( UiGetColor( uiLineColor, uiItem, &ucl, 0xFF ) );

		gc.MoveTo( sizeX, rect.top );
		gc.LineTo( sizeX, rect.bottom );
		gc.MoveTo( dateX, rect.top );
		gc.LineTo( dateX, rect.bottom );
		gc.MoveTo( timeX, rect.top );
		gc.LineTo( timeX, rect.bottom );

		rect.right = sizeX;
		gc.SetClipRgn( &rect );

	}

	if ( *_viewMode == FULL_ACCESS )
	{
		FSNode* p = _list.Get( n );

		int width = rect.Width();

		int accessW = accessWidth * _letterSize[0].x;
		int userW = userWidth * _letterSize[0].x;
		int groupW = groupWidth * _letterSize[0].x;

		int accessX = ( width - accessW - userW - groupW ) < minFileNameWidth * _letterSize[0].x ? minFileNameWidth * _letterSize[0].x : width - accessW - userW - groupW;
		accessX += rect.left;
		int userX = accessX + accessW;
		int groupX = userX + userW;

		gc.SetLine( UiGetColor( uiLineColor, uiItem, &ucl, 0xFF ) );

		/*
		      gc.MoveTo(accessX, rect.top);
		      gc.LineTo(accessX, rect.bottom);

		      gc.MoveTo(userX, rect.top);
		      gc.LineTo(userX, rect.bottom);

		      gc.MoveTo(groupX, rect.top);
		      gc.LineTo(groupX, rect.bottom);
		*/
		if ( p )
		{
			unicode_t ubuf[64];
			gc.GetTextExtents( p->st.GetModeStr( ubuf ) );
			gc.TextOutF( accessX + _letterSize[0].x, y, ubuf );

			crect cliprect( rect );

			cliprect.right = groupX;
			gc.SetClipRgn( &cliprect );

			const unicode_t* userName = GetUserName( p->GetUID() );
			gc.TextOutF( userX + _letterSize[0].x, y, userName );

			cliprect.right = rect.right;
			gc.SetClipRgn( &cliprect );

			const unicode_t* groupName = GetGroupName( p->GetGID() );
			gc.TextOutF( groupX + _letterSize[0].x, y, groupName );

		}

		rect.right = accessX;
		gc.SetClipRgn( &rect );

	}


	if ( active )
	{
		gc.TextOut( x, y, _list.GetFileName( n ) );
	}
	else
	{
		gc.TextOutF( x, y, _list.GetFileName( n ) );
	}
}

void PanelWin::SetCurrent( int n )
{
	SetCurrent( n, false, 0 );
}

void PanelWin::SetCurrent( int n, bool shift, int* selectType )
{
	if ( !this ) { return; }

	if ( n >= _list.Count() ) { n = _list.Count() - 1; }

	if ( n < 0 ) { n = 0; }

//	if (n == _current) return;

	int old = _current;
	_current = n;

	bool fullRedraw = false;

	if ( shift && selectType )
	{
		if ( old == _current )
		{
			_list.ShiftSelection( _current, selectType );
		}
		else
		{
			int count, delta;

			if ( old < _current )
			{
				count = _current - old + 1;
				delta = 1;
			}
			else
			{
				count = old - _current + 1;
				delta = -1;
			}

			for ( int i = old; count > 0; count--, i += delta )
			{
				_list.ShiftSelection( i, selectType );
			}
		}

		fullRedraw = true;
	}

	if ( _current >= _first + _rectList.count() )
	{
		_first = _current - _rectList.count() + 1;

		if ( _first < 0 ) { _first = 0; }

		fullRedraw = true;
	}
	else if ( _current < _first )
	{
		_first = _current;
		fullRedraw = true;
	}
	else

		if ( _list.Count() - _first < _rectList.count() && _first > 0 )
		{
			_first = _list.Count() - _rectList.count();

			if ( _first < 0 ) { _first = 0; }

			fullRedraw = true;
		}

	SetScroll();

	if ( fullRedraw )
	{
		Invalidate();
		return;
	}

	wal::GC gc( this );
	gc.Set( GetFont() );
	DrawItem( gc, old );
	DrawItem( gc, _current );
	DrawFooter( gc );
}


bool PanelWin::SetCurrent( FSString& name )
{
	int n = _list.Find( name );

	if ( n < 0 ) { return false; }

	SetCurrent( n );
	return true;
}

void SplitNumber_3( char* src, char* dst )
{
	for ( int n = strlen( src ); n > 0; n-- )
	{
		if ( ( n % 3 ) == 0 ) { *( dst++ ) = ' '; }

		*( dst++ ) = *( src++ );
	}

	*dst = 0;
}

void DrawTextRightAligh( wal::GC& gc, int x, int y, const unicode_t* txt, int width )
{
	if ( width <= 0 ) { return; }

	cpoint size = gc.GetTextExtents( txt );

	if ( size.x > width )
	{
		crect r( x, y, x + width, y + size.y );
		gc.SetClipRgn( &r );
		x = x + width - size.x;
	}

	gc.TextOutF( x, y, txt );

	if ( size.x > width )
	{
		gc.SetClipRgn();
	}

}

static int uiFooter = GetUiID( "footer" );
static int uiHeader = GetUiID( "header" );
static int uiHaveSelected = GetUiID( "have-selected" );
static int uiSummary = GetUiID( "summary-color" );

void PanelWin::DrawFooter( wal::GC& gc )
{
	PanelCounter selectedCn = _list.SelectedCounter();

	UiCondList ucl;

	if ( IsSelectedPanel() ) { ucl.Set( uiSelectedPanel, true ); }

	if ( _inOperState ) { ucl.Set( uiOperState, true ); }

	int color_text = UiGetColor( uiColor, uiFooter, &ucl, 0x0 );
	int color_bg = UiGetColor( uiBackground, uiFooter, &ucl, 0xFFFFFF );

	gc.SetFillColor( color_bg );
	crect tRect = _footRect;
	gc.SetClipRgn( &tRect );

	gc.FillRect( tRect );

	FSNode* cur = GetCurrent();
	gc.Set( GetFont( 1 ) );

	if ( _inOperState )
	{
		unicode_t ub[512];
		utf8_to_unicode( ub, _LT( "...Loading..." ) );
		gc.SetTextColor( UiGetColor( uiSummary, uiFooter, &ucl, 0xFF ) );
		cpoint size = gc.GetTextExtents( ub );
		int x = ( tRect.Width() - size.x ) / 2;
		gc.TextOutF( x, tRect.top + 5, ub );
		return;
	};

	//print free space
	FS* pFs = this->GetFS();

	if ( pFs )
	{
		int Err;
		int64 FreeSpace = pFs->GetFileSystemFreeSpace( GetPath(), &Err );

		if ( FreeSpace >= 0 )
		{
			char Num[128];
			sprintf( Num, _LT( "%" PRId64 ), FreeSpace );

			char SplitNum[128];
			SplitNumber_3( Num, SplitNum );

			char b[128];
			sprintf( b, _LT( "%s" ), SplitNum );

			unicode_t ub[512];
			utf8_to_unicode( ub, b );

			gc.SetTextColor( UiGetColor( uiSummary, uiFooter, &ucl, 0xFF ) );
			cpoint size = gc.GetTextExtents( ub );
			int x = ( tRect.Width() - size.x );

			if ( x < 10 ) { x = 10; }

			gc.TextOutF( x, tRect.top + 3, ub );
		}
	}

	{
		//print files count and size
		if ( selectedCn.count > 0 ) { ucl.Set( uiHaveSelected, true ); }

		PanelCounter selectedCn = _list.SelectedCounter();
		PanelCounter filesCn = _list.FilesCounter( wcmConfig.panelSelectFolders );
		int hiddenCount = _list.HiddenCounter().count;

		char b1[64];
		unsigned_to_char<long long>( selectedCn.count > 0 ? selectedCn.size : filesCn.size , b1 );
		char b11[100];
		SplitNumber_3( b1, b11 );

		char b3[0x100] = "";

		if ( hiddenCount ) { sprintf( b3, _LT( "(%i hidden)" ), hiddenCount ); }

		char b2[128];

		if ( selectedCn.count )
		{
			sprintf( b2, _LT( ( selectedCn.count == 1 ) ? "%s bytes in %i file" : "%s bytes in %i files" ), b11, selectedCn.count );
		}
		else
		{
			sprintf( b2, _LT( "%s (%i) %s" ), b11, filesCn.count, b3 );
		}


		unicode_t ub[512];

		utf8_to_unicode( ub, b2 );

		gc.SetTextColor( UiGetColor( uiSummary, uiFooter, &ucl, 0xFF ) );
		cpoint size = gc.GetTextExtents( ub );
		int x = ( tRect.Width() - size.x ) / 2;

		if ( x < 10 ) { x = 10; }

		gc.TextOutF( x, tRect.top + 3, ub );
	}

	if ( cur )
	{
		gc.SetTextColor( color_text );

		int sizeTextW = 0;

		int y = tRect.top + 3;
		y += _letterSize[1].y;

		if ( !cur->extType )
		{
			unicode_t ubuf[64];
			cur->st.GetPrintableSizeStr( ubuf );
			cpoint size = gc.GetTextExtents( ubuf );
			gc.TextOutF( tRect.right - size.x, y, ubuf );
			sizeTextW = size.x;
		}

		const unicode_t* name = cur->GetUnicodeName();

		int x = tRect.left + 5;
		DrawTextRightAligh( gc, x, y, name, tRect.right - sizeTextW - x - 15 );

		//gc.TextOut(tRect.left + 5, y, name);

		if ( !cur->extType )
		{
			y += _letterSize[1].y;
			int x = tRect.left + 5;
			unicode_t ubuf[64];

#ifdef _WIN32
			FS* pFs = this->GetFS();

			if ( pFs && pFs->Type() == FS::SYSTEM )
			{
				int n = 0;
				ubuf[n++] = '[';

				if ( ( cur->st.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 ) { ubuf[n++] = 'D'; }

				if ( ( cur->st.dwFileAttributes & FILE_ATTRIBUTE_READONLY ) != 0 ) { ubuf[n++] = 'R'; }

				if ( ( cur->st.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM ) != 0 ) { ubuf[n++] = 'S'; }

				if ( ( cur->st.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ) != 0 ) { ubuf[n++] = 'H'; }

				if ( ( cur->st.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE ) != 0 ) { ubuf[n++] =  'A'; }

				if ( ( cur->st.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED ) != 0 ) { ubuf[n++] =  'E'; }

				if ( ( cur->st.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED ) != 0 ) { ubuf[n++] =  'C'; }

				ubuf[n++] = ']';
				ubuf[n] = 0;
			}
			else
			{
				cur->st.GetModeStr( ubuf );
			}

#else
			cur->st.GetModeStr( ubuf );
#endif
			cpoint size = gc.GetTextExtents( ubuf );

			gc.TextOutF( x, y, ubuf );
			x += size.x + 10;
#ifdef _WIN32

			if ( !( pFs && pFs->Type() == FS::SYSTEM ) )
#else
			if ( 1 )
#endif
			{
				const unicode_t* userName = GetUserName( cur->GetUID() );
				size = gc.GetTextExtents( userName );
				gc.TextOutF( x, y, userName );

				x += size.x;
				static unicode_t ch = ':';
				size = gc.GetTextExtents( &ch, 1 );
				gc.TextOutF( x, y, &ch, 1 );
				x += size.x;

				const unicode_t* groupName = GetGroupName( cur->GetGID() );
				gc.TextOutF( x, y, groupName );
			}

			cur->st.GetMTimeStr( ubuf );
			size = gc.GetTextExtents( ubuf );
			gc.TextOutF( tRect.right - size.x, y, ubuf );
		}
	}
}

static int uiBorderColor1 = GetUiID( "border-color1" );
static int uiBorderColor2 = GetUiID( "border-color2" );
static int uiBorderColor3 = GetUiID( "border-color3" );
static int uiBorderColor4 = GetUiID( "border-color4" );

static int uiVLineColor1 = GetUiID( "vline-color1" );
static int uiVLineColor2 = GetUiID( "vline-color2" );
static int uiVLineColor3 = GetUiID( "vline-color3" );

void PanelWin::Paint( wal::GC& gc, const crect& paintRect )
{
	PanelCounter selectedCn = _list.SelectedCounter();
	UiCondList ucl;

	if ( IsSelectedPanel() ) { ucl.Set( uiSelectedPanel, true ); }

	if ( _inOperState ) { ucl.Set( uiOperState, true ); }

	if ( selectedCn.count > 0 ) { ucl.Set( uiHaveSelected, true ); }

	int color_bg = UiGetColor( uiBackground, 0, &ucl, 0xFFFFFF );

	crect r1 = ClientRect();
	DrawBorder( gc, r1, UiGetColor( uiBorderColor1, 0, &ucl, 0xFF ) );
	r1.Dec();
	DrawBorder( gc, r1, UiGetColor( uiBorderColor2, 0, &ucl, 0xFF ) );
	r1.Dec();
	DrawBorder( gc, r1, UiGetColor( uiBorderColor3, 0, &ucl, 0xFF ) );
	r1.Dec();
	DrawBorder( gc, r1, UiGetColor( uiBorderColor4, 0, &ucl, 0xFF ) );

	gc.Set( GetFont() );

	int i;

	for ( i = 0; i < _rectList.count(); i++ )
	{
		if ( paintRect.Cross( _rectList[i] ) ) { DrawItem( gc, i + _first ); }
	}

	gc.SetClipRgn( &r1 );

	gc.SetFillColor( color_bg );

	for ( i = 0; i < _emptyRectList.count(); i++ )
		if ( paintRect.Cross( _emptyRectList[i] ) )
		{
			gc.FillRect( _emptyRectList[i] );
		}

	for ( i = 0; i < _vLineRectList.count(); i++ )
		if ( paintRect.Cross( _vLineRectList[i] ) )
		{
			gc.SetLine( UiGetColor( uiVLineColor1, 0, &ucl, 0xFF ) );
			gc.MoveTo( _vLineRectList[i].left, _vLineRectList[i].top );
			gc.LineTo( _vLineRectList[i].left, _vLineRectList[i].bottom );
			gc.SetLine( UiGetColor( uiVLineColor2, 0, &ucl, 0xFF ) );
			gc.MoveTo( _vLineRectList[i].left + 1, _vLineRectList[i].top );
			gc.LineTo( _vLineRectList[i].left + 1, _vLineRectList[i].bottom );
			gc.SetLine( UiGetColor( uiVLineColor3, 0, &ucl, 0xFF ) );
			gc.MoveTo( _vLineRectList[i].left + 2, _vLineRectList[i].top );
			gc.LineTo( _vLineRectList[i].left + 2, _vLineRectList[i].bottom );
		}

//header
	crect tRect( _headRect.left, _headRect.bottom, _headRect.right, _headRect.bottom + 1 );

	if ( paintRect.Cross( _headRect ) )
	{
//		PanelHeaderColors colors;
//		panelColors->headF(&colors, IsSelectedPanel());

		gc.SetFillColor( UiGetColor( uiLineColor, 0, &ucl, 0xFF ) );

		gc.FillRect( tRect );

		gc.SetFillColor( UiGetColor( uiBackground, uiHeader, &ucl, 0xFFFF ) );
		tRect = _headRect;
		gc.FillRect( tRect );

		FS* fs =  GetFS();

		if ( fs )
		{
			FSPath& path = GetPath();
			FSString uri = fs->Uri( path );
			const unicode_t* uPath = uri.GetUnicode();

			cpoint p = gc.GetTextExtents( uPath );
			int x = _headRect.left + 2;

			const int LeftXMargin  = 7;
			const int RightXMargin = LeftXMargin + 3;

			if ( p.x > _headRect.Width( ) - GetXMargin( ) - RightXMargin )
			{
				x -= p.x - _headRect.Width( ) + GetXMargin( ) + RightXMargin;
			}

			int y = _headRect.top + 2;

			gc.SetTextColor( UiGetColor( uiColor, uiHeader, &ucl, 0xFFFF ) );
			gc.Set( GetFont( 1 ) );
			// magic constant comes from DrawItem() value of 'x'
			gc.TextOutF( x + GetXMargin( ) + LeftXMargin, y, uPath );

			// draw sorting order mark
			x = _headRect.left + 2;

			bool asc = _list.AscSort();

			crect SRect = _headRect;
			SRect.right = x + GetXMargin() + LeftXMargin;
			gc.FillRect( SRect );

			switch ( _list.SortMode() )
			{
				case PanelList::SORT_NONE:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "u" : "U" ) ).data() );
					break;

				case PanelList::SORT_NAME:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "n" : "N" ) ).data() );
					break;

				case PanelList::SORT_EXT:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "x" : "X" ) ).data() );
					break;

				case PanelList::SORT_SIZE:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "S" : "s" ) ).data() );
					break;

				case PanelList::SORT_MTIME:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "w" : "W" ) ).data() );
					break;
			};
		}
	}

	tRect = _footRect;
	tRect.bottom = tRect.top;
	tRect.top -= 1;
	gc.SetFillColor( UiGetColor( uiLineColor, 0, &ucl, 0xFF ) );
	gc.FillRect( tRect );

	DrawVerticalSplitters( gc, _centerRect );

	if ( paintRect.Cross( _footRect ) )
	{
		DrawFooter( gc );
	}
}

void PanelWin::LoadPathStringSafe( const char* path )
{
	if ( !path || !*path ) { return; }

	FSPath fspath;

	clPtr<FS> fs = ParzeURI( utf8_to_unicode( path ).data(), fspath, 0, 0 );

	this->LoadPath( fs, fspath, 0, 0, PanelWin::SET );
}

void PanelWin::LoadPath( clPtr<FS> fs, FSPath& paramPath, FSString* current, clPtr<cstrhash<bool, unicode_t> > selected, LOAD_TYPE lType )
{
//printf("LoadPath '%s'\n", paramPath.GetUtf8());

	FSStat stat;
	FSCInfo info;
	int err;

//	if ( fs->Stat(paramPath, &stat, &err, &info) == -1 ) return;

	try
	{
		StopThread();
		_inOperState = true;
		_operType = lType;

		if ( current ) { _operCurrent = *current; }
		else { _operCurrent.Clear(); }

		_operSelected = selected;
		_operData.SetNewParams( fs, paramPath );
		RunNewThread( "Read directory", ReadDirThreadFunc, &_operData ); //может быть исключение
		Invalidate();
	}
	catch ( cexception* ex )
	{
		_inOperState = false;
		NCMessageBox( ( NCDialogParent* )Parent(), _LT( "Read dialog list" ), ex->message(), true );
		ex->destroy();
	}
}

void PanelWin::OperThreadSignal( int info )
{
}

void PanelWin::OperThreadStopped()
{
	if ( !_inOperState )
	{
		fprintf( stderr, "BUG: PanelWin::OperThreadStopped\n" );
		Invalidate();	
		return;
	}

	_inOperState = false;

	try
	{
		if ( !_operData.errorString.IsEmpty() )
		{
			// Do not invalidate panel. Show "select drive" dialog instead.
			//Invalidate();
			NCMessageBox( ( NCDialogParent* )Parent(), _LT( "Read dialog list" ), _operData.errorString.GetUtf8(), true );
			NCWin* ncWin = (NCWin*)Parent();
			ncWin->SelectDrive(this, ncWin->GetOtherPanel(this));
			return;
		}

		if (!_operData.nonFatalErrorString.IsEmpty())
		{
			NCMessageBox((NCDialogParent*)Parent(), _LT("Read dialog list"), _operData.nonFatalErrorString.GetUtf8(), true);
		}

		clPtr<FSList> list = _operData.list;
		clPtr<cstrhash<bool, unicode_t> > selected = _operSelected;

//??? почему-то иногда list.ptr() == 0 ???
//!!! найдено и исправлено 20120202 в NewThreadID стоял & вместо % !!!

		switch ( _operType )
		{
			case RESET:
				_place.Reset( _operData.fs, _operData.path );
				break;

			case PUSH:
				_place.Set( _operData.fs, _operData.path, true );
				break;

			default:
				_place.Set( _operData.fs, _operData.path, false );
				break;
		};

		_list.SetData( list );

		if ( selected.ptr() )
		{
			_list.SetSelectedByHash( selected.ptr() );
		}

		if ( !_operCurrent.IsEmpty() )
		{
			int n = _list.Find( _operCurrent );
			SetCurrent( n < 0 ? 0 : n );
		}
		else
		{
			SetCurrent( 0 );
		}

	}
	catch ( cexception* ex )
	{
		NCMessageBox( ( NCDialogParent* )Parent(), _LT( "Read dialog list" ), ex->message(), true );
		ex->destroy();
	}

	Invalidate();
}


void PanelWin::Reread( bool resetCurrent )
{
	clPtr<cstrhash<bool, unicode_t> > sHash = _list.GetSelectedHash();
	FSNode* node = resetCurrent ? 0 : GetCurrent();
	FSString s;

	if ( node ) { s.Copy( node->Name() ); }

	LoadPath( GetFSPtr(), GetPath(), node ? &s : 0, sHash, RESET );
}


bool PanelWin::EventMouse( cevent_mouse* pEvent )
{
	bool shift = ( pEvent->Mod() & KM_SHIFT ) != 0;
	bool ctrl = ( pEvent->Mod() & KM_CTRL ) != 0;

	switch ( pEvent->Type() )
	{
		case EV_MOUSE_MOVE:
			if ( IsCaptured() )
			{
			}

			break;


		case EV_MOUSE_DOUBLE:
		case EV_MOUSE_PRESS:
		{
			if ( IsSelectedPanel() )
			{
				if ( pEvent->Button() == MB_X1 )
				{
					KeyPrior();
					break;
				}

				if ( pEvent->Button() == MB_X2 )
				{
					KeyNext();
					break;
				}
			}

			SelectPanel();

			for ( int i = 0; i < _rectList.count(); i++ )
				if ( _rectList[i].In( pEvent->Point() ) )
				{
					if ( ctrl && pEvent->Button() == MB_L )
					{
						_list.InvertSelection( _first + i );
						SetCurrent( _first + i );
					}
					else
					{
						SetCurrent( _first + i );

						if ( pEvent->Button() == MB_R )
						{
							FSNode* fNode =  GetCurrent();

							if ( fNode && Current() == _first + i )
							{
								crect rect = ScreenRect();
								cpoint p = pEvent->Point();
								p.x += rect.left;
								p.y += rect.top;
								( ( NCWin* )Parent() )->RightButtonPressed( p );
							}
						}
						else if ( pEvent->Type() == EV_MOUSE_DOUBLE )
						{
							( ( NCWin* )Parent() )->PanelEnter();
						}
					}

					break;
				}

		}
		break;


		case EV_MOUSE_RELEASE:
			break;
	};

	return false;
}

bool PanelWin::DirUp()
{
	if ( _place.IsEmpty() ) { return false; }

#ifdef _WIN32
	{
		clPtr<FS> fs = GetFSPtr();

		if ( fs->Type() == FS::SYSTEM )
		{
			FSSys* f = ( FSSys* )fs.Ptr();

			if ( f->Drive() < 0 && GetPath().Count() == 3 )
			{
				if ( _place.Count() <= 2 )
				{
					static unicode_t aa[] = {'\\', '\\', 0};
					std::vector<wchar_t> name = UnicodeToUtf16( carray_cat<unicode_t>( aa, GetPath().GetItem( 1 )->GetUnicode() ).data() );

					NETRESOURCEW r;
					r.dwScope = RESOURCE_GLOBALNET;
					r.dwType = RESOURCETYPE_ANY;
					r.dwDisplayType = RESOURCEDISPLAYTYPE_GENERIC;
					r.dwUsage = RESOURCEUSAGE_CONTAINER;
					r.lpLocalName = 0;
					r.lpRemoteName = name.data();
					r.lpComment = 0;
					r.lpProvider = 0;
					clPtr<FS> netFs = new FSWin32Net( &r );
					FSPath path( CS_UTF8, "\\" );

					LoadPath( netFs, path, 0, 0, RESET );
					return true;
				}
				else
				{
					if ( _place.Pop() )
					{
						Reread( true );
					}

					return false;
				}
			}
		}
	}
#endif


	FSPath p = _place.GetPath();

	if ( p.Count() <= 1 )
	{
		if ( _place.Pop() )
		{
			Reread( true );
		}

		return false;
	}

	FSString* s = p.GetItem( p.Count() - 1 );
	FSString item( "" );

	if ( s ) { item = *s; }

	p.Pop();
	LoadPath( GetFSPtr(), p, s ? &item : 0, 0, RESET );

	return true;
}

void PanelWin::DirEnter()
{
	if ( _place.IsEmpty() ) { return; }

	if ( !Current() )
	{

		DirUp();
		return;
	};

	FSNode* node = GetCurrent();

	if ( !node || !node->IsDir() ) { return; }

	FSPath p = GetPath();
	int cs = node->Name().PrimaryCS();
	p.Push( cs, node->Name().Get( cs ) );


	clPtr<FS> fs = GetFSPtr();

	if ( fs.IsNull() ) { return; }

#ifdef _WIN32

	if ( fs->Type() == FS::WIN32NET )
	{
		NETRESOURCEW* nr = node->_w32NetRes.Get();

		if ( !nr || !nr->lpRemoteName ) { return; }

		if ( node->extType == FSNode::FILESHARE )
		{
			FSPath path;
			clPtr<FS> newFs = ParzeURI( Utf16ToUnicode( nr->lpRemoteName ).data(), path, 0, 0 );
			LoadPath( newFs, path, 0, 0, PUSH );
		}
		else
		{
			clPtr<FS> netFs = new FSWin32Net( nr );
			FSPath path( CS_UTF8, "\\" );
			LoadPath( netFs, path, 0, 0, PUSH );
		}

		return;
	}

#endif

	// for smb servers

#ifdef LIBSMBCLIENT_EXIST

	if ( fs->Type() == FS::SAMBA && node->extType == FSNode::SERVER )
	{
		FSSmbParam param;
		( ( FSSmb* )fs.Ptr() )->GetParam( &param );
		param.SetServer( node->Name().GetUtf8() );
		clPtr<FS> smbFs = new FSSmb( &param );
		FSPath path( CS_UTF8, "/" );
		LoadPath( smbFs, path, 0, 0, PUSH );
		return;
	}

#endif

	LoadPath( GetFSPtr(), p, 0, 0, RESET );
}

void PanelWin::DirRoot()
{
	FSPath p;
	p.PushStr( FSString() ); //у абсолютного пути всегда пустая строка в начале
	LoadPath( GetFSPtr(), p, 0, 0, RESET );
}


PanelWin::~PanelWin() {}

