/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "termwin.h"
#include "terminal.h"

#ifndef _WIN32

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

void TerminalWin::OnChangeStyles()
{
	wal::GC gc( this );
	gc.Set( GetFont() );
	cpoint p = gc.GetTextExtents( ABCString );
	cW = p.x / ABCStringLen;
	cH = p.y;

	if ( !cW ) { cW = 10; }

	if ( !cH ) { cH = 10; }

//the same as in EventSize, need to think about it
	int W = _rect.Width();
	int H = _rect.Height();

	screen.SetSize( H / cH, W / cW );
	Reread();
	CalcScroll();

}

TerminalWin::TerminalWin( int nId, Win* parent )
	: Win( WT_CHILD, 0, parent, 0, nId ),
	  _lo( 1, 2 ),
	  _scroll( 0, this, true, false ), //need to find solution for autohide
	  cH( 1 ), cW( 1 ),
	  _firstRow( 0 ),
	  _currentRows( 1 )
{
	_scroll.Enable();
	_scroll.Show();
	_scroll.SetManagedWin( this );
	_lo.AddWin( &_scroll, 0, 1 );
	_lo.AddRect( &_rect, 0, 0 );
	_lo.SetColGrowth( 0 );
	SetLayout( &_lo );
	LSRange lr( 0, 10000, 1000 );
	LSize ls;
	ls.x = ls.y = lr;
	SetLSize( ls );

	ThreadCreate( 0, TerminalInputThreadFunc, &_terminal );
	OnChangeStyles();
}

int uiClassTerminal = GetUiID( "Terminal" );

int TerminalWin::UiGetClassId()
{
	return uiClassTerminal;
}

void TerminalWin::Paste()
{
	ClipboardText ctx;
	ClipboardGetText( this, &ctx );
	int count = ctx.Count();

	if ( count <= 0 ) { return; }

	for ( int i = 0; i < count; i++ )
	{
		_terminal.UnicodeOutput( ctx[i] );
	}
}


void TerminalWin::PageUp()
{
	Scroll( + ( screen.rows - 1 ) );
}


void TerminalWin::PageDown()
{
	Scroll( - ( screen.rows - 1 ) );
}

void TerminalWin::Scroll( int Lines )
{
	if ( SetFirst( _firstRow + Lines ) )
	{
		Reread();
		CalcScroll();
		Invalidate();
	}
}

bool TerminalWin::SetFirst( int n )
{
	if ( n + screen.rows > _currentRows )
	{
		n = _currentRows - screen.rows;
	}

//	if (n + screen.rows > MAX_TERM_ROWS)
//		n = MAX_TERM_ROWS - screen.rows;

	if ( n < 0 ) { n = 0; }

	int old = _firstRow;
	_firstRow = n;

	ASSERT( _firstRow >= 0 );
//	ASSERT(_firstRow < _currentRows);

	return n != old;
}


void TerminalWin::Reread()
{
	MutexLock lock( _terminal.InputMutex() );
	_terminal.SetSize( screen.rows, screen.cols );

	for ( int r = 0; r < screen.rows; r++ )
	{
		TermChar* sc = screen.Get( r );
		TermChar* tc = _terminal.Get( r + _firstRow );

		if ( tc )
		{
			memcpy( sc, tc, screen.cols * sizeof( TermChar ) );
		}
	}

	screen.cursor.Set( _terminal.CRow() - _firstRow,  _terminal.CCol() );
}


void TerminalWin::CalcScroll()
{
	ScrollInfo si;
	si.m_PageSize = screen.rows;
	si.m_Size = _currentRows;
	si.m_Pos = _currentRows - _firstRow - screen.rows;

	bool visible = _scroll.IsVisible();

	_scroll.Command( CMD_SCROLL_INFO, SCMD_SCROLL_VCHANGE, this, &si );

	if ( visible != _scroll.IsVisible() )
	{
		this->RecalcLayouts();
	}

	ASSERT( _firstRow >= 0 );
//	ASSERT(_firstRow < MAX_TERM_ROWS);
}


bool TerminalWin::Command( int id, int subId, Win* win, void* data )
{
	if ( id != CMD_SCROLL_INFO )
	{
		return false;
	}

	int n = _firstRow;

	switch ( subId )
	{
		case SCMD_SCROLL_LINE_UP:
			n++;
			break;

		case SCMD_SCROLL_LINE_DOWN:
			n--;
			break;

		case SCMD_SCROLL_PAGE_UP:
			n += screen.rows;
			break;

		case SCMD_SCROLL_PAGE_DOWN:
			n -= screen.rows;
			break;

		case SCMD_SCROLL_TRACK:
			n = _currentRows - ( ( int* )data )[0] - screen.rows;
			break;
	}

	if ( n + screen.rows > _currentRows )
	{
		n = _currentRows - screen.rows;
	}

	if ( n < 0 ) { n = 0; }

	if ( n != _firstRow )
	{
		_firstRow = n;

		Reread();

		CalcScroll();
		Invalidate();
	}

	return true;
}

struct ExecData
{
	std::vector<sys_char_t> cmd;
	std::vector<sys_char_t> path;
	Shell* shell;
};


void* RunProcessThreadFunc( void* data )
{
	ExecData* p = ( ExecData* )data;

	if ( p )
	{
		if ( !p->shell->CD( p->path.data() ) )
		{
			pid_t pid = p->shell->Exec( p->cmd.data() );

			if ( pid > 0 )
			{
				WinThreadSignal( pid );
				int ret;
//printf("waitpid %i\n", pid);
				int r = p->shell->Wait( pid, &ret );
//printf("done wait %i (ret=%i)\n", pid, r);
				WinThreadSignal( -1 );
			}
		}

		delete p;
	}

	return 0;
}


void TerminalWin::Execute( Win* w, int tId, const unicode_t* cmd, const sys_char_t* path )
{
	ExecData* p = new ExecData;
	p->cmd =  unicode_to_sys_array( cmd );
	p->path = new_sys_str( path );
	p->shell = & _terminal.GetShell();
	_terminal.TerminalReset( false );
	w->ThreadCreate( tId, RunProcessThreadFunc, p );
}

void TerminalWin::EventSize( cevent_size* pEvent )
{
	RecalcLayouts();
//	cpoint size = pEvent->Size();

	int W = _rect.Width();
	int H = _rect.Height();

	screen.SetSize( H / cH, W / cW );
	Reread();
	CalcScroll();
	Parent()->RecalcLayouts(); //!!!
}

void TerminalWin::EventTimer( int tid )
{
	if ( IsCaptured() )
	{
		EmulatorScreenPoint pt = lastMousePoint;
		int delta  =  ( pt.row < 0 ) ? pt.row : ( pt.row >= screen.rows ? pt.row - screen.rows + 1 : 0 );

		if ( delta != 0 && SetFirst( _firstRow + delta * 2 ) )
		{
			pt.row += _firstRow;
			screen.marker.Move( pt );

			Reread();
			CalcScroll();
			Invalidate();
		}
	}
}

bool TerminalWin::GetMarked( ClipboardText& ct )
{
	ct.Clear();

	if ( screen.marker.Empty() ) { return false; }

	MutexLock lock( _terminal.InputMutex() );
	int n1 = screen.marker.a.row;
	int n2 = screen.marker.b.row;

	if ( n1 > n2 ) { int t = n1; n1 = n2; n2 = t; }

	if ( n1 < 0 ) { n1 = 0; }

	if ( n2 >= _currentRows ) { n2 = _currentRows - 1; }

	for ( int i = n2; i >= n1; i-- )
	{
		TermChar* tc = _terminal.Get( i );

		if ( i == n1 || i == n2 )
		{
			for ( int j = 0; j < screen.cols; j++ )
				if ( screen.marker.In( EmulatorScreenPoint( i, j ) ) )
				{
					unicode_t c = tc[j] & 0xFFFFFF;

					if ( c < 32 ) { c = 32; }

					ct.Append( c );
				}
		}
		else
		{
			for ( int j = 0; j < screen.cols; j++ )
			{
				unicode_t c = tc[j] & 0xFFFFFF;

				if ( c < 32 ) { c = 32; }

				ct.Append( c );
			}
		}

		if ( i != n1 ) { ct.Append( '\n' ); }
	}

	return true;
}


void TerminalWin::MarkerClear()
{
	if ( screen.marker.Empty() )
	{
		return;
	}

	screen.marker.Reset();
	Invalidate();
}

void TerminalWin::ThreadSignal( int id, int data )
{
//	printf("terminal thread signal id=%i, data=%i\n", id, data);

	MutexLock lock( _terminal.InputMutex() );


	bool calcScroll = false;

	if ( _currentRows != _terminal.CurrentRows() )
	{
		_currentRows = _terminal.CurrentRows();
		calcScroll = true;
	}



	wal::GC gc( this );
	gc.Set( GetFont() );

	bool fullDraw = false;

	if ( _firstRow != 0 )
	{
		_firstRow = 0;
		CalcScroll();
		fullDraw = true;
	}

	if ( !screen.marker.Empty() )
	{
		screen.marker.Reset();
		fullDraw = true;
	}

	int cols = screen.cols;

	for ( int i = 0; i < screen.rows; i++ )
		if ( _terminal.IsChanged( i ) )
		{

			TermChar* sc = screen.Get( i );
			TermChar* tc = _terminal.Get( i );

			if ( sc && tc )
			{
				int first = 0;

				for ( ; first < cols && sc[first] == tc[first]; first++ );

				if ( first < cols )
				{
					int last = first;

					sc[first] = tc[first];

					for ( int j = first + 1; j < cols; j++ )
						if ( sc[j] != tc[j] )
						{
							sc[j] = tc[j];
							last = j;
						}

					_terminal.SetChanged( i, false );

					if ( !fullDraw )
					{
						lock.Unlock();
						DrawRow( gc, i, first, last );
						lock.Lock();
					}
				}
			}

		}

	if ( fullDraw )
	{
		Invalidate();
		return;
	}

	//cursor
	if ( screen.cursor.row >= 0 && screen.cursor.row < screen.rows )
	{
		EmulatorScreenPoint cur( _terminal.CRow(), _terminal.CCol() );
		lock.Unlock();

		if ( screen.cursor != cur ) //hide old
		{
			DrawChar( gc, screen.cursor.row, screen.cursor.col, screen.GetChar( screen.cursor.row, screen.cursor.col ) );
		}

		screen.cursor = cur;
		TermChar ch = screen.GetChar( screen.cursor.row, screen.cursor.col );
		ch = ( ( ch >> 4 ) & 0xF000000 ) + ( ( ch << 4 ) & 0xF0000000 ) + ( ch & 0xFFFFFF );
		DrawChar( gc, screen.cursor.row, screen.cursor.col, ch );
	}

	if ( calcScroll ) { CalcScroll(); }
}

static unsigned default_color_table[16] =
{
	0x000000, //black
	0x800000, //dark blue
	0x008000, //dark green
	0x808000, //dark cyan

	0x000080, //dark red
	0x800080, //
	0x008080, //
	0xC0C0C0, //gray

	0x808080, //dark gray
	0xFF0000, //blue
	0x00FF00, //green
	0xFFFF00, //cyan

	0x0000FF, //red
	0xFF00FF, //
	0x00FFFF,
	0xFFFFFF //white
};

void TerminalWin::DrawChar( wal::GC& gc, int r, int c, TermChar tc )
{
	if ( r < 0 || r >= screen.rows ) { return; }

	if ( c < 0 || c >= screen.cols ) { return; }

	gc.SetFillColor( default_color_table[( tc >> 28 ) & 0xF] );
	gc.SetTextColor( default_color_table[( tc >> 24 ) & 0xF] );

	int y = ( screen.rows - r - 1 ) * cH;
	int x = c * cW;

	unicode_t ch = tc & 0xFFFFFF;
	gc.TextOutF( x, y, &ch, 1 );
}

bool TerminalWin::EventMouse( cevent_mouse* pEvent )
{
	cpoint coord = pEvent->Point();
	EmulatorScreenPoint pt;
	pt.Set( screen.rows - ( coord.y + cH - 1 ) / cH, coord.x / cW );
	bool pointChanged = lastMousePoint != pt;
	lastMousePoint = pt;

//	bool shift = ( pEvent->Mod() & KM_SHIFT ) != 0;
//	bool ctrl = ( pEvent->Mod() & KM_CTRL ) != 0;

	switch ( pEvent->Type() )
	{
		case EV_MOUSE_MOVE:
			if ( IsCaptured() && pointChanged )
			{
				pt.row += _firstRow;
				screen.marker.Move( pt );
				Invalidate();
			}

			break;

		case EV_MOUSE_DOUBLE:
		case EV_MOUSE_PRESS:
		{
			if ( pEvent->Button() == MB_X1 )
			{
				PageUp();

				if ( IsCaptured() )
				{
					pt.row += _firstRow;
					screen.marker.Move( pt );
					Invalidate();
				}

				break;
			}

			if ( pEvent->Button() == MB_X2 )
			{
				PageDown();

				if ( IsCaptured() )
				{
					pt.row += _firstRow;
					screen.marker.Move( pt );
					Invalidate();
				}

				break;
			}

			if ( pEvent->Button() != MB_L )
			{
				break;
			}

			SetCapture();
			SetTimer( 1, 100 );

			pt.row += _firstRow;
			screen.marker.Set( pt );
			Invalidate();
		}
		break;

		case EV_MOUSE_RELEASE:

			if ( pEvent->Button() != MB_L ) { break; }

			DelTimer( 1 );
			ReleaseCapture();
			break;
	};

	return false;
}


void TerminalWin::DrawRow( wal::GC& gc, int r, int first, int last )
{
	int rows = screen.rows;

	if ( r < 0 || r  >= rows ) { return; }

	int cols = screen.cols;

	if ( first >= cols ) { first = cols - 1; }

	if ( first < 0 ) { first = 0; }

	if ( last >= cols ) { last = cols - 1; }

	if ( last < 0 ) { last = 0; }

	if ( first > last ) { return; }

	int y = ( rows - r  - 1 ) * cH;
	int x = first * cW;

	ASSERT( r >= 0 );
	TermChar* p =  screen.Get( r );

	ScreenMarker m = screen.marker;
	int absRow = _firstRow + r;

	while ( first <= last )
	{
		int n = 1;

		bool marked = m.In( EmulatorScreenPoint( absRow, first ) );

		for ( int i = first + 1; i <= last; i++, n++ )
		{
			if ( ( p[i] & 0xFF000000 ) != ( p[i - 1] & 0xFF000000 ) ||
			     m.In( EmulatorScreenPoint( absRow, i ) ) != marked )
			{
				break;
			}
		}

		unsigned c = ( ( p[first] >> 24 ) & 0xFF );

		if ( marked )
		{
			c = 0xF0;

			//c = ((c>>4)|(c<<4))&0xFF;
		}

		gc.SetFillColor( default_color_table[( c >> 4 ) & 0xF] );
		gc.SetTextColor( default_color_table[c & 0xF] );

		while ( n > 0 )
		{
			int count = n;
			unicode_t buf[0x100];

			if ( count > 0x100 ) { count = 0x100; }

			for ( int i = 0; i < count; i++ )
			{
				buf[i] = ( p[first + i] & 0xFFFFFF );

			}

			gc.TextOutF( x, y, buf, count );
			n -= count;
			first += count;
			x += count * cW;
		}
	}
}

void TerminalWin::Paint( wal::GC& gc, const crect& paintRect )
{
	crect r = ClientRect();

	gc.Set( GetFont() );

	for ( int i = 0; i < screen.rows; i++ )
	{
		DrawRow( gc, i, 0, screen.cols - 1 );
	}

//cursor
	if ( screen.cursor.row >= 0 && screen.cursor.row < screen.rows )
	{
		TermChar ch = screen.GetChar( screen.cursor.row, screen.cursor.col );
		ch = ( ( ch >> 4 ) & 0xF000000 ) + ( ( ch << 4 ) & 0xF0000000 ) + ( ch & 0xFFFFFF );
		DrawChar( gc, screen.cursor.row, screen.cursor.col, ch );
	}

	crect r1 = r;
	r1.top = screen.rows * cH;
	gc.SetFillColor( 0 ); //0x200000);
	gc.FillRect( r1 );

	r1 = r;
	r1.left = screen.cols * cW;
	gc.FillRect( r1 );
}

TerminalWin::~TerminalWin() {};
#endif
