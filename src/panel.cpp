/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#define __STDC_FORMAT_MACROS
#include <stdint.h>
#if !defined(_MSC_VER) || _MSC_VER >= 1700
#  include <inttypes.h>
#endif
#include "globals.h"
#include "wcm-config.h"
#include "ncfonts.h"
#include "ncwin.h"
#include "panel.h"
#include "ext-app.h"
#include "color-style.h"
#include "string-util.h"
#include "unicode_lc.h"
#include "strmasks.h"

#ifndef _WIN32
#  include "ux_util.h"
#else
#	include "w32util.h"
#endif

#include "icons/folder3.xpm"
#include "icons/folder.xpm"
//#include "icons/executable.xpm"
#include "icons/executable1.xpm"
#include "icons/monitor.xpm"
#include "icons/workgroup.xpm"
#include "icons/run.xpm"
#include "icons/link.xpm"

#include "vfs-smb.h"
#include "ltext.h"
#include "folder-history.h"

static const unicode_t g_LongNameMark[] = { '}', 0 };

int uiPanelSearchWin = GetUiID( "PanelSearchWin" );

PanelSearchWin::PanelSearchWin( PanelWin* parent, cevent_key* key )
	:  Win( Win::WT_CHILD, 0, parent, nullptr, uiPanelSearchWin ),
	   _parent( parent ),
	   _edit( 0, this, 0, 0, 16, true ),
	   _static( 0, this, utf8_to_unicode( _LT( "Search:" ) ).data() ),
	   _lo( 3, 4 ),
	   ret_key( 0, 0, 0, 0, 0, false )
{
	_edit.SetAcceptAltKeys();
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

	_edit.EnableAltSymbols( true );

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
	return g_DialogFont.ptr();
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
		ret_key = *pEvent;
	}
	else
	{
		ret_key = cevent_key( 0, 0, 0, 0, 0, false );
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
		case VK_LMETA:
		case VK_RMETA:
		case VK_LCONTROL:
		case VK_RCONTROL:
		case VK_LSHIFT:
		case VK_RSHIFT:
		case VK_LMENU:
		case VK_RMENU:
		case VK_BACK:
		case VK_DELETE:

		// this comes from 0xfe08 XK_ISO_Next_Group, workaround for https://github.com/corporateshark/WCMCommander/issues/22
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

cevent_key PanelWin::QuickSearch( cevent_key* key )
{
	_search = new PanelSearchWin( this, key );
	crect r = _footRect;
	LSize ls = _search->GetLSize();
	r.right = r.left + ls.x.minimal;
	r.bottom = r.top + ls.y.minimal;
	_search->Move( r );

	_search->DoModal();

	cevent_key ret = _search->ret_key;

	_search = nullptr;

	return ret;
}

NCWin* PanelWin::GetNCWin()
{
	return ( NCWin* )Parent();
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

	bool RootDir = HideDotsInDir();

	for ( i = cur + ofs; i < cnt; i++ )
	{
		const unicode_t* name = _list.GetFileName( i, RootDir );

		if ( name && accmask_nocase( name, mask ) )
		{
			SetCurrent( i );
			return true;
		}
	}

	for ( i = 0; i < cnt - 1 + ofs; i++ )
	{
		const unicode_t* name = _list.GetFileName( i, HideDotsInDir() );

		if ( name && accmask_nocase( name, mask ) )
		{
			SetCurrent( i );
			return true;
		}
	}

	return false;
}


void PanelWin::SortByName()
{
	FSNode* p = _list.Get( _current, HideDotsInDir() );

	if ( _list.SortMode() == SORT_NAME )
	{
		_list.Sort( SORT_NAME, !_list.AscSort() );
	}
	else
	{
		_list.Sort( SORT_NAME, true );
	}

	if ( p ) { SetCurrent( p->Name() ); }

	Invalidate();
}

void PanelWin::SortByExt()
{
	FSNode* p = _list.Get( _current, HideDotsInDir() );

	if ( _list.SortMode() == SORT_EXT )
	{
		_list.Sort( SORT_EXT, !_list.AscSort() );
	}
	else
	{
		_list.Sort( SORT_EXT, true );
	}

	if ( p ) { SetCurrent( p->Name() ); }

	Invalidate();
}

void PanelWin::SortBySize()
{
	FSNode* p = _list.Get( _current, HideDotsInDir() );

	if ( _list.SortMode() == SORT_SIZE )
	{
		_list.Sort( SORT_SIZE, !_list.AscSort() );
	}
	else
	{
		_list.Sort( SORT_SIZE, false );
	}

	if ( p ) { SetCurrent( p->Name() ); }

	Invalidate();
}

void PanelWin::SortByMTime()
{
	FSNode* p = _list.Get( _current, HideDotsInDir() );

	if ( _list.SortMode() == SORT_MTIME )
	{
		_list.Sort( SORT_MTIME, !_list.AscSort() );
	}
	else
	{
		_list.Sort( SORT_MTIME, false );
	}

	if ( p ) { SetCurrent( p->Name() ); }

	Invalidate();
}


void PanelWin::DisableSort()
{
	FSNode* p = _list.Get( _current, HideDotsInDir() );
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
unicode_t PanelWin::linkPrefix[] = {'^', 0};

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

	{
		UiCondList ucl;
		_longNameMarkColor = UiGetColor( uiHotkeyColor, uiItem, &ucl, 0x0 );
		_longNameMarkExtentsValid = false;
	}

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


	if ( _itemHeight <= 16 ) //for icons to fit
	{
		_itemHeight = 16;
	}

	RecalcLayouts(); //may be overcalculated, but needed

//as in EventSize (think about it)
	Check();
	bool v = _scroll.IsVisible();
	SetCurrent( _current );

	if ( v != _scroll.IsVisible() )
	{
		Check();
	}
}

static int CheckMode( int m )
{
	switch ( m )
	{
		case PanelWin::BRIEF:
		case PanelWin::MEDIUM:
		case PanelWin::FULL:
		case PanelWin::FULL_ST:
		case PanelWin::FULL_ACCESS:
		case PanelWin::TWO_COLUMNS:
			break;

		default:
			m = PanelWin::MEDIUM;
	}

	return m;
}

PanelWin::PanelWin( Win* parent, int mode )
	:
	NCDialogParent( WT_CHILD, 0, 0, parent ),
	_lo( 7, 4 ),
	_scroll( 0, this, true ), //, false), //bug with autohide and layouts
	_list( g_WcmConfig.panelShowHiddenFiles, g_WcmConfig.panelCaseSensitive ),
	_itemHeight( 1 ),
	_rows( 0 ),
	_cols( 0 ),
	_first( 0 ),
	_current( 0 ),
	_viewMode( CheckMode( mode ) ), //MEDIUM),
	_inOperState( false ),
	_operData( ( NCDialogParent* )parent ),
	_operCursorLoc( 0 )
{
	_longNameMarkExtentsValid = false;

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
	NCWin* p = GetNCWin();
	return p && p->_panel == this;
}

void PanelWin::SelectPanel()
{
	NCWin* p = GetNCWin();

	if ( p->_panel == this ) { return; }

	PanelWin* old = p->_panel;
	p->_panel = this;
	old->Invalidate();
	Invalidate();
}


void PanelWin::SetScroll()
{
	ScrollInfo si;
	si.m_PageSize = _rectList.count();
	si.m_Size = _list.Count( HideDotsInDir() );
	si.m_Pos = _first;
	si.m_AlwaysHidden = !g_WcmConfig.panelShowScrollbar;
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

	if ( n + pageSize > _list.Count( HideDotsInDir() ) )
	{
		n = _list.Count( HideDotsInDir() ) - pageSize;
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

		_list.SetShowHidden( g_WcmConfig.panelShowHiddenFiles );
		_list.SetCase( g_WcmConfig.panelCaseSensitive );

		SetCurrent( _list.Find( s, HideDotsInDir() ) );
		SetScroll();
		Invalidate();

		GetNCWin()->NotifyCurrentPathInfo();

		return true;
	}

	return false;
}

static const int VLINE_WIDTH = 3;
static const int MIN_BRIEF_CHARS = 12;
static const int MIN_MEDIUM_CHARS = 18;

void PanelWin::Check()
{
	int width = _centerRect.Width();
	int height = _centerRect.Height();

	int cols;

	_viewMode = CheckMode( _viewMode );

	switch ( _viewMode )
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
			int minChars = ( _viewMode == BRIEF ) ? MIN_BRIEF_CHARS : MIN_MEDIUM_CHARS;

			if ( width < cols * minChars * _letterSize[0].x + ( cols - 1 )*VLINE_WIDTH )
			{
				cols = ( width + VLINE_WIDTH ) / ( minChars * _letterSize[0].x + VLINE_WIDTH );

				if ( cols < 1 ) { cols = 1; }
			}

			if ( _viewMode != BRIEF && cols > 3 ) { cols = 3; }
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

static cicon linkIcon( xpm16x16_Link, PANEL_ICON_SIZE, PANEL_ICON_SIZE );
static cicon executableIcon( xpm16x16_Executable, PANEL_ICON_SIZE, PANEL_ICON_SIZE );
static cicon serverIcon( xpm16x16_Monitor, PANEL_ICON_SIZE, PANEL_ICON_SIZE );
static cicon workgroupIcon( xpm16x16_Workgroup, PANEL_ICON_SIZE, PANEL_ICON_SIZE );
static cicon runIcon( xpm16x16_Run, PANEL_ICON_SIZE, PANEL_ICON_SIZE );

bool panelIconsEnabled = true;

static int uiDir = GetUiID( "dir" );
static int uiExe = GetUiID( "exe" );
static int uiBad = GetUiID( "bad" );
static int uiLink = GetUiID( "link" );
static int uiSelectedPanel = GetUiID( "selected-panel" );
static int uiOperState = GetUiID( "oper-state" );
static int uiHidden = GetUiID( "hidden" );
static int uiSelected = GetUiID( "selected" );
//static int uiLineColor = GetUiID("line-color");

void PanelWin::DrawVerticalSplitters( wal::GC& gc, const crect& rect )
{
	UiCondList ucl;

	if ( _viewMode == FULL_ST )
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

	if ( _viewMode == FULL_ACCESS )
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

	if ( g_WcmConfig.panelShowFolderIcons || g_WcmConfig.panelShowExecutableIcons || g_WcmConfig.panelShowLinkIcons )
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

	FSNode* p = ( n < 0 || n > _list.Count( HideDotsInDir() ) ) ? 0 : _list.Get( n, HideDotsInDir() );

	bool isDir = p && p->IsDir();
	bool isExe = !isDir && p && p->IsExe();
	bool isBad = p && p->IsBad();
	bool isSelected = p && p->IsSelected();
	bool isHidden = p && p->IsHidden();
	bool isLink = p && p->IsLnk();

	/*
	PanelItemColors color;
	panelColors->itemF(&color, _inOperState, active, isSelected, isBad, isHidden, isDir, isExe);
	*/

	UiCondList ucl;

	if ( isDir ) { ucl.Set( uiDir, true ); }

	if ( isExe ) { ucl.Set( uiExe, true ); }

	if ( isBad ) { ucl.Set( uiBad, true ); }

	if ( isLink ) { ucl.Set( uiLink, true ); }

	if ( IsSelectedPanel() ) { ucl.Set( uiSelectedPanel, true ); }

	if ( n == _current ) { ucl.Set( uiCurrentItem, true ); }

	if ( isHidden ) { ucl.Set( uiHidden, true ); }

	if ( isSelected ) { ucl.Set( uiSelected, true ); }

	if ( _inOperState ) { ucl.Set( uiOperState, true ); }

	if ( n < _list.Count( HideDotsInDir() ) && ( n % 2 ) == 0 ) { ucl.Set( uiOdd, true ); }

	int color_text = UiGetColor( uiColor, uiItem, &ucl, 0x0 );
	int color_bg = UiGetColor( uiBackground, uiItem, &ucl, 0xFFFFFF );
	int color_shadow = UiGetColor( uiBackground, uiItem, &ucl, 0 );

	const auto& Rules = g_Env.GetFileHighlightingRules();

	if ( p )
	{
		for ( const auto& i : Rules )
		{
			if ( i.IsRulePassed( p->GetUnicodeName(), p->Size(), 0 ) )
			{
				if ( isSelected )
				{
					color_bg = active ? i.GetColorUnderCursorSelectedBackground() : i.GetColorSelectedBackground();
					color_text = active ? i.GetColorUnderCursorSelected() : i.GetColorSelected();
				}
				else
				{
					color_bg = active ? i.GetColorUnderCursorNormalBackground() : i.GetColorNormalBackground();
					color_text = active ? i.GetColorUnderCursorNormal() : i.GetColorNormal();
				}

				break;
			}
		}
	}

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

	if ( n < 0 || n >= _list.Count( HideDotsInDir() ) ) { return; }

	if ( g_WcmConfig.styleShow3DUI && active )
	{
		Draw3DButtonW2( gc, rect, color_bg, true );
	}

	int x = rect.left + 7; //5;
	int y = rect.top;
	int IconY = y + ( rect.Height() - folderIcon.Height() ) / 2;

	gc.SetTextColor( color_text );

	if ( isDir )
	{
		if ( g_WcmConfig.panelShowFolderIcons )
		{
			switch ( p->extType )
			{
				case FSNode::SERVER:
					serverIcon.DrawF( gc, x, IconY );
					break;

				case FSNode::WORKGROUP:
					workgroupIcon.DrawF( gc, x, IconY );
					break;

				default:
					if ( ( ( ( color_bg >> 16 ) & 0xFF ) + ( ( color_bg >> 8 ) & 0xFF ) + ( color_bg & 0xFF ) ) < 0x80 * 3 )
					{
						folderIcon.DrawF( gc, x, IconY );
					}
					else
					{
						folderIconHi.DrawF( gc, x, IconY );
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
		if ( g_WcmConfig.panelShowExecutableIcons )
		{
			executableIcon.DrawF( gc, x, IconY );
		}
		else
		{
			gc.TextOutF( x, y, exePrefix );
		}
	}
	else if ( isLink )
	{
		if ( g_WcmConfig.panelShowLinkIcons )
		{
			linkIcon.DrawF( gc, x, IconY );
		}
		else
		{
			gc.TextOutF( x, y, linkPrefix );
		}
	}
	else
	{
	}

	x += GetXMargin();

	if ( isSelected )
	{
		gc.SetTextColor( color_shadow );
		gc.TextOutF( x + 1, y + 1, _list.GetFileName( n, HideDotsInDir() ) );
		gc.SetTextColor( color_text );
	}


	if ( _viewMode == FULL_ST )
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

	if ( _viewMode == FULL_ACCESS )
	{
		FSNode* p = _list.Get( n, HideDotsInDir() );

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

	std::vector<unicode_t> Name = new_unicode_str( _list.GetFileName( n, HideDotsInDir() ) );

	if ( g_WcmConfig.panelShowSpacesMode == ePanelSpacesMode_All )
	{
		ReplaceSpaces( &Name );
	}
	else if ( g_WcmConfig.panelShowSpacesMode == ePanelSpacesMode_Trailing )
	{
		ReplaceTrailingSpaces( &Name );
	}

	gc.TextOutF( x, y, Name.data() );

	cpoint Size = gc.GetTextExtents( Name.data() );

	// show special mark for long file names: https://github.com/corporateshark/WCMCommander/issues/272
	if ( x + Size.x > rect.right )
	{
		gc.SetTextColor( _longNameMarkColor );
		if ( !_longNameMarkExtentsValid )
		{
			_longNameMarkExtents = gc.GetTextExtents( g_LongNameMark );
			_longNameMarkExtentsValid = true;
		}
		gc.TextOutF( rect.right - _longNameMarkExtents.x + 2, y, g_LongNameMark );
	}
}

void PanelWin::SetCurrent( int n )
{
	SetCurrent( n, false, 0, false );
}

void PanelWin::SetCurrent( int n, bool Shift, LPanelSelectionType* SelectType, bool SelectLastItem )
{
	if ( !this ) { return; }

	bool SelectLast = SelectLastItem;

	if ( n >= _list.Count( HideDotsInDir() ) )
	{
		n = _list.Count( HideDotsInDir() ) - 1;
		// this is similar to Far Manager
		SelectLast = true;
	}

	if ( n < 0 )
	{
		n = 0;
		// this is similar to Far Manager
		SelectLast = true;
	}

	int old = _current;
	_current = n;

	bool fullRedraw = false;

	if ( Shift && SelectType )
	{
		if ( old == _current )
		{
			_list.ShiftSelection( _current, SelectType, HideDotsInDir() );
		}
		else
		{
			int count, delta;

			if ( old < _current )
			{
				count = _current - old;
				delta = 1;
			}
			else
			{
				count = old - _current;
				delta = -1;
			}

			for ( int i = old; count >= 0; count--, i += delta )
			{
				LPanelSelectionType* SType = SelectType;
				LPanelSelectionType  LastSelection = LPanelSelectionType_Disable;

				// the last line should be selected only in specific cases
				if ( count == 0 )
				{
					SType = SelectLast ? SelectType : &LastSelection;
				}

				_list.ShiftSelection( i, SType, HideDotsInDir() );
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
	{
		if ( _list.Count( HideDotsInDir() ) - _first < _rectList.count() && _first > 0 )
		{
			_first = _list.Count( HideDotsInDir() ) - _rectList.count();

			if ( _first < 0 ) { _first = 0; }

			fullRedraw = true;
		}
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
	int n = _list.Find( name, HideDotsInDir() );

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
		int64_t FreeSpace = pFs->GetFileSystemFreeSpace( GetPath(), &Err );

		if ( FreeSpace >= 0 )
		{
			char Num[128];
			Lsnprintf( Num, sizeof( Num ), _LT( "%" PRId64 ), FreeSpace );

			char SplitNum[128];
			SplitNumber_3( Num, SplitNum );

			char b[128];
			Lsnprintf( b, sizeof( b ), _LT( "%s" ), SplitNum );

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
		PanelCounter filesCn = _list.FilesCounter( g_WcmConfig.panelSelectFolders );
		int hiddenCount = _list.HiddenCounter().count;

		char b1[64];
		unsigned_to_char<long long>( selectedCn.count > 0 ? selectedCn.size : filesCn.size , b1 );
		char b11[100];
		SplitNumber_3( b1, b11 );

		char b3[0x100] = "";

		if ( hiddenCount ) { Lsnprintf( b3, sizeof( b3 ), _LT( "(%i hidden)" ), hiddenCount ); }

		char b2[128];

		if ( selectedCn.count )
		{
			Lsnprintf( b2, sizeof( b2 ), _LT( ( selectedCn.count == 1 ) ? "%s bytes in %i file" : "%s bytes in %i files" ), b11, selectedCn.count );
		}
		else
		{
			Lsnprintf( b2, sizeof( b2 ), _LT( "%s (%i) %s" ), b11, filesCn.count, b3 );
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
				case SORT_NONE:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "u" : "U" ) ).data() );
					break;

				case SORT_NAME:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "n" : "N" ) ).data() );
					break;

				case SORT_EXT:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "x" : "X" ) ).data() );
					break;

				case SORT_SIZE:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "S" : "s" ) ).data() );
					break;

				case SORT_MTIME:
					gc.TextOutF( x, y, utf8_to_unicode( _LT( asc ? "W" : "w" ) ).data() );
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

	//dbg_printf( "PanelWin::LoadPathStringSafe path=%s\n", path );
	FSPath fspath;

	auto UTF8Path = utf8_to_unicode( path );

	clPtr<FS> fs = ParzeURI( UTF8Path.data(), fspath, {} );

	this->LoadPath( fs, fspath, 0, 0, PanelWin::SET );
}

void PanelWin::LoadPath( clPtr<FS> fs, FSPath& paramPath, FSString* current, clPtr<cstrhash<bool, unicode_t> > selected, LOAD_TYPE lType )
{
	FSStat stat;
	FSCInfo info;

	try
	{
		StopThread();
		_inOperState = true;
		_operType = lType;

		bool foundCurrent = false;
		if (current)
		{
			_operCurrentStr = *current;

			int currentIdx = _list.Find(*current, HideDotsInDir());
			if (currentIdx >= 0)
			{
				_operCursorLoc = currentIdx;
				FSNode* currentNode = _list.Get(currentIdx, HideDotsInDir());
				if (currentNode)
				{
					_operCurrent = *currentNode;
					foundCurrent = true;
				}
			}
			else
				_operCursorLoc = -1;
		}
		else
		{
			_operCurrentStr.Clear();
		}

		if (!foundCurrent)
		{
			_operCurrent.name.Clear();
		}

		_operSelected = selected;
		_operData.SetNewParams( fs, paramPath );
		RunNewThread( "Read directory", ReadDirThreadFunc, &_operData ); //may throw exception
		Invalidate();
	}
	catch ( cexception* ex )
	{
		_inOperState = false;
		NCMessageBox( ( NCDialogParent* )Parent(), _LT( "Read dialog list" ), ex->message(), true );
		ex->destroy();
		GetNCWin()->NotifyCurrentPathInfo();
	}

    AddFolderToHistory(&fs, &paramPath);
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
			NCWin* ncWin = GetNCWin();
			ncWin->NotifyCurrentPathInfo();
			ncWin->SelectDrive( this, ncWin->GetOtherPanel( this ) );
			return;
		}

		if ( !_operData.nonFatalErrorString.IsEmpty() )
		{
			NCMessageBox( ( NCDialogParent* )Parent(), _LT( "Read dialog list" ), _operData.nonFatalErrorString.GetUtf8(), true );
		}

		clPtr<FSList> list = _operData.list;
		clPtr<cstrhash<bool, unicode_t> > selected = _operSelected;

//??? why sometimes list.ptr() == 0 ???
//!!! found and fixed 20120202 in NewThreadID was & instead of % !!!

		switch ( _operType )
		{
			case RESET:
				_place.Reset( _operData.fs, _operData.path );
				break;

			case PUSH:
				_place.Set( _operData.fs, _operData.path, true );
				break;

			default:
				//_operData.path.dbg_printf("PanelWin::OperThreadStopped _operData.path=");
				_place.Set( _operData.fs, _operData.path, false );
				break;
		};

		_vst = _operData.vst;

		_list.SetData( list );

		if ( selected.ptr() )
		{
			_list.SetSelectedByHash( selected.ptr() );
		}

		bool foundCurrent = false;

		// restore cursor location
		// try to find exact match on name 
		if (!_operCurrentStr.IsEmpty())
		{
			int n = _list.Find(_operCurrentStr, HideDotsInDir());
			if (n > 0)
			{
				SetCurrent(n);
				foundCurrent = true;
			}
		}
		// in no-sort mode the best is to stay at the same cursor location
		if (!foundCurrent && _list.SortMode() == SORT_NONE)
		{
			SetCurrent(_operCursorLoc);
			foundCurrent = true;
		}

		// in sorted mode try closest node using current sort mode
		if (!foundCurrent && !_operCurrent.name.IsEmpty())
		{
			int n = _list.FindExactOrClosestSucceeding( _operCurrent, HideDotsInDir() );
			if (n > 0)
			{
				SetCurrent(n);
				foundCurrent = true;
			}
		}

		// un unsorted mode - no way to find closest match. Try last cursor location
		if (!foundCurrent && _operCursorLoc >= 0)
		{
			SetCurrent(_operCursorLoc);
		}
		
		if (!foundCurrent)
		{
			SetCurrent( 0 );
		}

	}
	catch ( cexception* ex )
	{
		NCMessageBox( ( NCDialogParent* )Parent(), _LT( "Read dialog list" ), ex->message(), true );
		ex->destroy();
	}

	GetNCWin()->NotifyCurrentPathInfo();

	Invalidate();
}

void PanelWin::Reread( bool resetCurrent, const FSString& Name )
{
	clPtr<cstrhash<bool, unicode_t> > sHash = _list.GetSelectedHash();
//	bool Root = HideDotsInDir();
	FSNode* node = resetCurrent ? 0 : GetCurrent();
	FSString s;

	FSString* StrPtr = node ? &s : 0;

	if ( !Name.IsEmpty() )
	{
		s = Name;
		StrPtr = &s;
	}
	else if ( node ) { s.Copy( node->Name() ); }

	LoadPath( GetFSPtr(), GetPath(), StrPtr, sHash, RESET );
}

bool PanelWin::EventMouse( cevent_mouse* pEvent )
{
//	bool shift = ( pEvent->Mod() & KM_SHIFT ) != 0;
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
						_list.InvertSelection( _first + i, HideDotsInDir() );
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
								GetNCWin()->RightButtonPressed( p );
							}
						}
						else if ( pEvent->Type() == EV_MOUSE_DOUBLE )
						{
							GetNCWin()->PanelEnter();
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

std::vector<std::string> PanelWin::GetMatchedFileNames( const std::string& Prefix, size_t MaxItems ) const
{
	std::vector<std::string> Result;

	if ( Prefix.empty() ) return Result;

	int Count = this->Count();

	for ( int i = 0; i != Count; i++ )
	{
		const FSNode* Node = this->Get(i);

		if ( !Node ) continue;

		const char* Name = Node->GetUtf8Name();

		if ( utf8_starts_with_and_not_equal( Name, Prefix.c_str() ) )
		{
			Result.push_back( Name );
			if ( Result.size() >= MaxItems ) break;
		}
	}

	return Result;
}

bool PanelWin::HideDotsInDir() const
{
	bool HideDots = !g_WcmConfig.panelShowDotsInRoot;

	if ( _place.IsEmpty() ) { return HideDots; }

#ifdef _WIN32
	clPtr<FS> fs = GetFSPtr();

	if ( fs->Type() == FS::SYSTEM )
	{
		FSSys* f = ( FSSys* )fs.Ptr();

		if ( f->Drive() < 0 && GetPath().Count() == 3 )
		{
			return ( _place.Count() > 2 ) ? HideDots : false;
		}
	}

#endif

	return ( _place.GetPath().Count() <= 1 ) ? HideDots : false;
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

void PanelWin::DirEnter(bool OpenInExplorer)
{
	if ( _place.IsEmpty() ) { return; }
	
	if ( !HideDotsInDir() && !Current() )
	{
		if ( OpenInExplorer )
		{
			FS* fs = this->GetFS();
			FSString URI = fs ? fs->Uri( this->GetPath() ) : FSString();
			const unicode_t* Path = URI.GetUnicode();
			ExecuteDefaultApplication( Path );
		}
		else if ( !DirUp() )
		{
			GetNCWin()->SelectDrive( this, this );
		}

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
			auto UnicodeRemoteName = Utf16ToUnicode( nr->lpRemoteName );
			clPtr<FS> newFs = ParzeURI( UnicodeRemoteName.data(), path, {} );
			LoadPath( newFs, path, nullptr, nullptr, PUSH );
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
		FSSmbParam param = ( ( FSSmb* )fs.Ptr() )->GetParamValue();
		param.SetServer( node->Name().GetUtf8() );
		clPtr<FS> smbFs = new FSSmb( &param );
		FSPath path( CS_UTF8, "/" );
		LoadPath( smbFs, path, 0, 0, PUSH );
		return;
	}

#endif

	if ( OpenInExplorer )
	{
		FSString URI = fs->Uri(p);
		const unicode_t* Path = URI.GetUnicode();
		ExecuteDefaultApplication( Path );
	}
	else
	{
		LoadPath( GetFSPtr(), p, 0, 0, RESET );
	}
}

void PanelWin::DirRoot()
{
	FSPath p;
	p.PushStr( FSString() ); //in absolute path empty sting is always at the beginning
	LoadPath( GetFSPtr(), p, 0, 0, RESET );
}

