/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "w32cons.h"
#include "string-util.h"

#include <array>

/*
   need to make it possible to detect if process is running

   cursor is not stable
	programs should be searched first, and then given to cmd if not found

	need to fix reading of blocks >64k into clipboard (ReadConsoleOutputW doesn't read big blocks)
*/

int uiClassTerminal = GetUiID( "Terminal" );

BOOL WINAPI ConsoleHandlerRoutine( DWORD dwCtrlType )
{
	return TRUE;
}

bool W32Cons::NewConsole()
{
	FreeConsole();
	outHandle = 0;

	if ( !AllocConsole() ) { return false; }

	SetConsoleCtrlHandler( ConsoleHandlerRoutine, TRUE );

	outHandle = GetStdHandle( STD_OUTPUT_HANDLE );

	COORD coord;
	coord.X = 80;
	coord.Y = 1024;

//	SetConsoleTextAttribute(outHandle, 0x8);
	SetConsoleScreenBufferSize( outHandle, coord );

	if ( !GetConsoleScreenBufferInfo( outHandle, &consLastInfo ) )
	{
		outHandle = 0;
		FreeConsole();
		return false;
	}

	::ShowWindow( ::GetConsoleWindow(), SW_HIDE );

	return 0;
}

void W32Cons::DetachConsole()
{
	if ( !outHandle ) { return; }

	HWND h = ::GetConsoleWindow();

	if ( !h ) { return; }

	::ShowWindow( h, SW_SHOWNORMAL ); //SW_SHOW);
	::SetActiveWindow( h );
	FreeConsole();
	outHandle = 0;
}

bool W32Cons::CheckConsole()
{
	if ( !ConsoleOk() ) { return false; }

	CONSOLE_SCREEN_BUFFER_INFO inf;

	if ( !GetConsoleScreenBufferInfo( outHandle, &inf ) )
	{
		return false;
	}

	if (  consLastInfo.dwSize.X != inf.dwSize.X ||
	      consLastInfo.dwSize.Y != inf.dwSize.Y )
	{
		MarkerClear();
		SetEvent( stopHandle );
		DetachConsole();
		NewConsole();
		SetFirst( 0 );

		return true;
	}

	if ( consLastInfo.dwCursorPosition.X != inf.dwCursorPosition.X ||
	     consLastInfo.dwCursorPosition.Y != inf.dwCursorPosition.Y )
	{
		COORD prev = consLastInfo.dwCursorPosition;
		COORD cur = inf.dwCursorPosition;
		consLastInfo = inf;
		MarkerClear();

		if ( cur.Y >= _firstRow + screen.Rows() )
		{
			SetFirst( cur.Y - screen.Rows() + 1 );
		}
		else if ( cur.Y < _firstRow )
		{
			SetFirst( cur.Y );
		}

		prev.Y -= _firstRow;

		if ( prev.Y >= 0 && prev.Y < screen.Rows() && prev.X >= 0 && prev.X < screen.Cols() )
		{
			screen.Get( prev.Y )[prev.X].Char.UnicodeChar = 0;
			screen.Get( prev.Y )[prev.X].Attributes = 0;
		}


		cur.Y -= _firstRow;

		if ( cur.Y >= 0 && cur.Y < screen.Rows() && cur.X >= 0 && cur.X < screen.Cols() )
		{
			screen.Get( cur.Y )[cur.X].Char.UnicodeChar = 0;
			screen.Get( cur.Y )[cur.X].Attributes = 0;
		}

		return true;
	}

	return false;
}

template <class T, int BLOCKLSIZE = 1024> class SimpleQueue
{
	struct Node
	{
		int pos, size;
		T buf[BLOCKLSIZE];
		Node* next;
		Node() : pos( 0 ), size( 0 ), next( 0 ) {}
	};
	Node* first, *last;
	int count;
public:
	SimpleQueue() : first( 0 ), last( 0 ), count( 0 ) {}
	bool Empty() const { return first == 0; }

	void Clear()
	{
		while ( first )
		{
			Node* p = first;
			first = first->next;
			delete p;
		};

		last = 0;
	}

	bool Get( T* res )
	{
		if ( !first ) { return false; }

		T c = first->buf[first->pos++];

		if ( first->pos >= first->size )
		{
			Node* p = first;
			first = first->next;

			if ( !first ) { last = 0; }

			delete p;
		}

		count--;
		*res = c;
		return true;
	}

	void Put( T c )
	{
		if ( !last )
		{
			first = last = new Node;
		}
		else if ( last->size >= BLOCKLSIZE )
		{
			last->next = new Node;
			last = last->next;
		}

		last->buf[last->size++] = c;
		count++;
	}

	void Put( const T* s, int size ) { for ( ; size > 0; size--, s++ ) { Put( *s ); } }
	~SimpleQueue() {}
};

struct ConsInput
{
	Mutex mutex;
	SimpleQueue<unicode_t> queue;
	Cond cond;
};

static ConsInput consInput;

void* ConsInputThread( void* data )
{
#define IBS 0x100
	INPUT_RECORD buf[IBS];
	int i;

	for ( i = 0; i < IBS; i++ ) { buf[i].EventType = KEY_EVENT; }

	while ( true )
	{
		{
			//lock block
			MutexLock lock( &consInput.mutex );

			for ( i = 0; i < IBS; i++ )
			{
				unicode_t c;

				if ( !consInput.queue.Get( &c ) )
				{
					break;
				}

				buf[i].Event.KeyEvent.bKeyDown = TRUE;
				buf[i].Event.KeyEvent.wRepeatCount = 1;
				buf[i].Event.KeyEvent.uChar.UnicodeChar = c;
				buf[i].Event.KeyEvent.wVirtualKeyCode = 0;
				buf[i].Event.KeyEvent.wVirtualScanCode = 0;
				buf[i].Event.KeyEvent.dwControlKeyState = 0;
			}

			if ( i <= 0 )
			{
				consInput.cond.Wait( &consInput.mutex );

				if ( consInput.queue.Empty() ) { continue; }
			}
		}
		DWORD ret;
		WriteConsoleInputW( GetStdHandle( STD_INPUT_HANDLE ), buf, i, &ret );
	}

#undef IBS
}

void W32Cons::OnChangeStyles()
{
	wal::GC gc( this );
	gc.Set( GetFont() );
	cpoint p = gc.GetTextExtents( ABCString );
	cW = p.x / ABCStringLen;
	cH = p.y;

	if ( !cW ) { cW = 10; }

	if ( !cH ) { cH = 10; }

	crect rect = ClientRect();
	screen.SetSize( rect.Height() / cH, rect.Width() / cW );

	CalcScroll();
}


W32Cons::W32Cons( int nId, Win* parent )
	: Win( WT_CHILD, 0, parent, 0, nId ),
	  _lo( 1, 2 ),
	  _scroll( 0, this, true ),
	  cH( 1 ), cW( 1 ),
	  outHandle( 0 ),
	  _firstRow( 0 )
{

	static int classObjCount = 0;

	if ( classObjCount > 0 ) { exit( 1 ); }

	classObjCount++;

	NewConsole();
	stopHandle = CreateEvent( 0, TRUE, FALSE, 0 );
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

	OnChangeStyles();
	this->ThreadCreate( 100, ConsInputThread, 0 );
}

int W32Cons::GetClassId() { return CI_TERMINAL; }

void W32Cons::Paste()
{
	ClipboardText text;
	ClipboardGetText( this, &text );

	if ( text.Count() <= 0 ) { return; }

	{
		//lock
		MutexLock lock( &consInput.mutex );
		int count = text.Count();

		for ( int i = 0; i < count; i++ )
		{
			unicode_t c = text.Get( i );

			if ( c == '\n' ) { consInput.queue.Put( '\r' ); }

			consInput.queue.Put( c );
		}
	}

	consInput.cond.Signal();
}


void W32Cons::PageUp()
{
	Scroll( screen.Rows() );
}


void W32Cons::PageDown()
{
	Scroll( -screen.Rows() );
}

void W32Cons::Scroll( int Lines )
{
	if ( SetFirst( _firstRow - Lines ) )
	{
		if ( Lines > 0 ) { Reread(); }

		CalcScroll();
		Invalidate();
	}
}

bool W32Cons::SetFirst( int n )
{
	if ( !ConsoleOk() ) { return false; }

	if ( n + screen.Rows() > consLastInfo.dwSize.Y )
	{
		n = consLastInfo.dwSize.Y - screen.Rows();
	}

	if ( n < 0 ) { n = 0; }

	if ( n == _firstRow ) { return false; }

	_firstRow = n;
	return true;
}


void W32Cons::Reread()
{
}


void W32Cons::CalcScroll()
{
	bool visible = _scroll.IsVisible();

	ScrollInfo si;

	if ( !ConsoleOk() )
	{
		if ( !visible ) { return; }

		si.m_PageSize = 1;
		si.m_Size = 0;
		si.m_Pos = 0;
		_scroll.Command( CMD_SCROLL_INFO, SCMD_SCROLL_VCHANGE, this, &si );
		this->RecalcLayouts();
		return;
	}

	si.m_PageSize = screen.Rows();
	si.m_Size = consLastInfo.dwSize.Y;
	si.m_Pos = _firstRow;

	_scroll.Command( CMD_SCROLL_INFO, SCMD_SCROLL_VCHANGE, this, &si );

	if ( visible != _scroll.IsVisible() )
	{
		this->RecalcLayouts();
	}
}


bool W32Cons::Command( int id, int subId, Win* win, void* data )
{
	if ( id != CMD_SCROLL_INFO )
	{
		return false;
	}

	int n = _firstRow;

	switch ( subId )
	{
		case SCMD_SCROLL_LINE_UP:
			n--;
			break;

		case SCMD_SCROLL_LINE_DOWN:
			n++;
			break;

		case SCMD_SCROLL_PAGE_UP:
			n -= screen.Rows();
			break;

		case SCMD_SCROLL_PAGE_DOWN:
			n += screen.Rows();
			break;

		case SCMD_SCROLL_TRACK:
			n = ( ( int* )data )[0];
			break;
	}

	if ( n + screen.Rows() > consLastInfo.dwSize.Y )
	{
		n = consLastInfo.dwSize.Y - screen.Rows();
	}

	if ( n < 0 ) { n = 0; }

	if ( n != _firstRow )
	{
		_firstRow = n;
		CalcScroll();
		Invalidate();
	}

	return true;

}

void W32Cons::Key( cevent_key* pEvent )
{
	if ( !ConsoleOk() ) { return; }

	if ( pEvent->Key() == VK_C && ( pEvent->Mod() & KM_CTRL ) != 0 )
	{
		if ( pEvent->Type() == EV_KEYDOWN )
		{
			GenerateConsoleCtrlEvent( CTRL_C_EVENT, 0 );
			return;
		}
	}

	INPUT_RECORD rec;
	rec.EventType = KEY_EVENT;
	rec.Event.KeyEvent.bKeyDown = pEvent->Type() == EV_KEYDOWN ? TRUE : FALSE;
	rec.Event.KeyEvent.uChar.UnicodeChar = pEvent->Char();
	rec.Event.KeyEvent.dwControlKeyState = 0; //!!! переделать

	BYTE keyState[0x100];
	::GetKeyboardState( keyState );

	if ( keyState[VK_LSHIFT] || keyState[VK_RSHIFT] ) { rec.Event.KeyEvent.dwControlKeyState |= SHIFT_PRESSED; }

	if ( keyState[VK_LCONTROL] ) { rec.Event.KeyEvent.dwControlKeyState |= LEFT_CTRL_PRESSED; }

	if ( keyState[VK_RCONTROL] ) { rec.Event.KeyEvent.dwControlKeyState |= RIGHT_CTRL_PRESSED; }

	if ( keyState[VK_LMENU] ) { rec.Event.KeyEvent.dwControlKeyState |= LEFT_ALT_PRESSED; }

	if ( keyState[VK_RMENU] ) { rec.Event.KeyEvent.dwControlKeyState |= RIGHT_ALT_PRESSED; }

	if ( keyState[VK_NUMLOCK] ) { rec.Event.KeyEvent.dwControlKeyState |= NUMLOCK_ON; }

	if ( keyState[VK_SCROLL] ) { rec.Event.KeyEvent.dwControlKeyState |= SCROLLLOCK_ON; }

//??? if (keyState[]) rec.Event.KeyEvent.dwControlKeyState |= ;
	/*
	CAPSLOCK_ON
	*/

	rec.Event.KeyEvent.wRepeatCount = 1;
	rec.Event.KeyEvent.wVirtualScanCode = 0;
	rec.Event.KeyEvent.wVirtualKeyCode = pEvent->Key();
	DWORD wr;
	WriteConsoleInputW( GetStdHandle( STD_INPUT_HANDLE ), &rec, 1, &wr );
	ResetTimer( 10, 20 );
}

struct ExecData
{
	HANDLE handles[2]; //0 - process handle, 1 - stop event
	ExecData() { handles[0] = handles[1] = 0; }
};


void* RunProcessThreadFunc( void* data )
{
	ExecData* p = ( ExecData* )data;

	if ( p )
	{
		int msec = 100;

		while ( true )
		{
			DWORD ret = WaitForInputIdle( p->handles[0], 50 );

			if ( !ret ) { break; }

			ret = WaitForMultipleObjects( 2, p->handles, FALSE, msec );

			if ( ret != WAIT_TIMEOUT ) { break; }

			if ( msec < 2000 ) { msec += 100; }
		}

		CloseHandle( p->handles[0] );
		ResetEvent( p->handles[1] );
		delete p;
	}

	return 0;
}


bool W32Cons::Execute( Win* w, int tId, const unicode_t* _cmd, const unicode_t* params, const unicode_t* path ) //const sys_char_t *path)
{
	std::vector<wchar_t> wPath = UnicodeToUtf16( path );

//MessageBoxW(0, UnicodeToUtf16(cmd).ptr(), L"", MB_OK);
	if ( !SetCurrentDirectoryW( wPath.data() ) )
	{
		return false;
	}

	{
		//lock
		MutexLock lock( &consInput.mutex );
		consInput.queue.Clear();
	}


	ccollect<wchar_t, 0x100> cmd;

	for ( const unicode_t* p = _cmd; *p; )
	{
		if ( *p == '%' )
		{
			ccollect<wchar_t, 0x100> e;
			const unicode_t* t = p + 1;

			while ( *t != '%' && *t ) { e.append( *t ); t++; }

			e.append( 0 );

			if ( *t )
			{
				wchar_t buf[1050];
				int n = GetEnvironmentVariableW( e.ptr(), buf, sizeof( buf ) );

				if ( n > 0 && n < 1050 )
				{
					for ( int i = 0; i < n; i++ ) { cmd.append( buf[i] ); }

					p = t + 1;
				}
				else
				{
					cmd.append( *p );
					p++;
				}
			}
			else
			{
				cmd.append( *p );
				p++;
			}
		}
		else
		{
			cmd.append( *p );
			p++;
		}
	}

	cmd.append( 0 );

	{
		//
		CheckConsole();
		//reset mode
		SetConsoleMode( outHandle,
		                ENABLE_ECHO_INPUT |
		                ENABLE_INSERT_MODE |
		                ENABLE_LINE_INPUT |
		                ENABLE_MOUSE_INPUT |
		                ENABLE_PROCESSED_INPUT |
		                ENABLE_QUICK_EDIT_MODE );

		WORD lastAttr = consLastInfo.wAttributes;

		SetConsoleTextAttribute( outHandle, ( lastAttr & 0xF0 ) | 0xB );

		int l = wcslen( cmd.ptr() );
		wchar_t line1[] = L"\r\n>";
		wchar_t newline[] = L"\r\n";
		WriteConsoleW( outHandle, line1, 3, 0, 0 );
		WriteConsoleW( outHandle, cmd.ptr(), l, 0, 0 );
		WriteConsoleW( outHandle, newline, 2, 0, 0 );
		SetConsoleTextAttribute( outHandle, lastAttr );
	}

	{
		STARTUPINFOW startup;
		memset( &startup, 0, sizeof( startup ) );
		startup.cb = sizeof( startup );
		PROCESS_INFORMATION processInfo;

		ccollect<wchar_t, 0x100> wCmd;
		const wchar_t* s = cmd.ptr();
		bool x = false;

		if ( *s == '"' ) { s++; x = true; }

		for ( ; *s && !( x && *s == '"' ) && !( !x && *s <= 32 ); s++ ) { wCmd.append( *s ); }

		wCmd.append( 0 );

		if ( x && *s == '"' ) { s++; }

		while ( *s && *s <= 32 ) { s++; }

//MessageBoxW(0, s, wCmd.ptr(), MB_OK);

		if ( CreateProcessW( wCmd.ptr(),
		                     cmd.ptr(), 0, 0, TRUE, NORMAL_PRIORITY_CLASS, 0, 0, &startup, &processInfo ) )
		{
			CloseHandle( processInfo.hThread ); //!!!

			ExecData* pData = new ExecData;
			pData->handles[0] = processInfo.hProcess;
			pData->handles[1] = this->stopHandle;
			w->ThreadCreate( tId, RunProcessThreadFunc, pData );

			SetTimer( 10, 50 );
			return true;
		}
	}

	static wchar_t comspec[] = L"COMSPEC";
	std::array<wchar_t, 1050> buf;
	//wchar_t buf[1050];
	int n = GetEnvironmentVariableW( comspec, buf.data(), sizeof( buf ) );

	if ( n < 0 )
	{
		n = GetSystemWindowsDirectoryW( buf.data(), 1024 );

		if ( n <= 0 || n > 1000 ) { return false; }

#if _MSC_VER > 1700
		Lwcsncpy( buf.data() + n, buf.size() - n, L"\\system32\\cmd.exe", _TRUNCATE );
#else
		Lwcsncpy( buf.data() + n, L"\\system32\\cmd.exe", buf.size() - n );
#endif
	}

	std::vector<wchar_t> arg;
	arg = carray_cat<wchar_t>( L"/C ", cmd.ptr(), L" ", UnicodeToUtf16( params ).data(), L"" );

	STARTUPINFOW startup;
	memset( &startup, 0, sizeof( startup ) );
	startup.cb = sizeof( startup );
	PROCESS_INFORMATION processInfo;

//MessageBoxW(0, arg.ptr(), L"Hi", MB_OK);

	if ( CreateProcessW( buf.data(), arg.data(), 0, 0, TRUE, NORMAL_PRIORITY_CLASS, 0, 0, &startup, &processInfo ) )
	{
		CloseHandle( processInfo.hThread ); //!!!

		ExecData* pData = new ExecData;
		pData->handles[0] = processInfo.hProcess;
		pData->handles[1] = this->stopHandle;
		w->ThreadCreate( tId, RunProcessThreadFunc, pData );

		SetTimer( 10, 50 );


		return true;
	}

	return false;
}

void W32Cons::EventSize( cevent_size* pEvent )
{
//	cpoint size = pEvent->Size();

	int W = _rect.Width();
	int H = _rect.Height();

	screen.SetSize( H / cH, W / cW );
	screen.marker.Reset();

	int charW = W / cW;

	if ( charW > 0 && ConsoleOk() )
	{
		if ( charW < 80 ) { charW = 80; }

		COORD coord;
		coord.X = charW;
		coord.Y = 1024;

		SetConsoleScreenBufferSize( outHandle, coord );
		GetConsoleScreenBufferInfo( outHandle, &consLastInfo );

	}

	if ( consLastInfo.dwCursorPosition.Y < _firstRow )
	{
		_firstRow = consLastInfo.dwCursorPosition.Y;
	}

	if ( consLastInfo.dwCursorPosition.Y >= _firstRow + screen.Rows() )
	{
		_firstRow = consLastInfo.dwCursorPosition.Y - screen.Rows() + 1;
	}

	if ( _firstRow + screen.Rows() > consLastInfo.dwSize.Y )
	{
		_firstRow = consLastInfo.dwSize.Y - screen.Rows();
	}

	if ( _firstRow < 0 ) { _firstRow = 0; }

	Invalidate();

	RecalcLayouts();
	Reread();
	CalcScroll();

	Parent()->RecalcLayouts(); //!!!
}

inline bool EqCInfo( CHAR_INFO& a, CHAR_INFO& b )
{
	return a.Attributes == b.Attributes && a.Char.UnicodeChar == b.Char.UnicodeChar;
}

bool W32Cons::DrawChanges()
{
	GC gc( this );
//	crect clientRect = ClientRect();
	gc.Set( GetFont() );

	int R = screen.Rows();
	int C = screen.Cols();

	if ( R > 0 && C > 0 )
	{
//см Paint
		const int MAX_BLOCK = 1024;

		int step = MAX_BLOCK / C;

		if ( step <= 0 ) { step = 1; }

		_temp.SetSize( step, C );
		COORD size;
		size.X = C;
		size.Y = step;

		COORD pos;
		pos.X = 0;
		pos.Y = 0;

		bool changed = false;

		int row = 0;

		while ( row < R )
		{
			//int n = step;
			//if (row + n > R) n = R - row;

			int n = R - row;

			if ( n > step ) { n = step; }

			SMALL_RECT srect;
			srect.Left = 0 ;
			srect.Top = _firstRow + row;
			srect.Right = C - 1;
			srect.Bottom = _firstRow + row + n - 1;

			//not clear, if not doing this, then ReadConsoleOutputW corrupts memory, but should return out of bounds
			if ( srect.Bottom >= consLastInfo.dwSize.Y )
			{
				srect.Bottom = consLastInfo.dwSize.Y - 1;
			}

			if ( ReadConsoleOutputW( outHandle, _temp.Ptr(), size, pos, &srect ) )
			{

				for ( int i = 0; i < n; i++ )
				{
					CHAR_INFO* dest = screen.Get( row + i );
					CHAR_INFO* s = _temp.Line( i );

					int col = 0;

					while ( col < C )
					{
						while ( col < C && EqCInfo( s[col], dest[col] ) ) { col++; }

						if ( col < C )
						{
							int t = col;

							while ( t < C && !EqCInfo( s[t], dest[t] ) )
							{
								dest[t] = s[t];
								t++;
							}

							DrawRow( gc, row + i , col, t - 1 );
							changed = true;
							col = t;
						}
					}
				}
			}

			row += n;
		}

		return changed;
	}

	return false;
}

bool W32Cons::EventShow( bool show )
{
	ResetTimer( 10, show ? 100 : ( 100000 ) );
	return true;
}

void W32Cons::EventTimer( int tid )
{
	if ( tid == 10 )
	{
		CheckConsole();

		if ( IsVisible() )
		{
			if ( DrawChanges() )
			{
				ResetTimer( 10, 100 );
			}
			else
			{
				ResetTimer( 10, 500 );
			}
		}

		return;
	}

	if ( IsCaptured() )
	{
		COORD pt = lastMousePoint;

		int delta  =  ( pt.Y < 0 ) ? pt.Y : ( pt.Y >= screen.Rows() ? pt.Y - screen.Rows() + 1 : 0 );

		if ( delta != 0 && SetFirst( _firstRow + delta * 2 ) )
		{
			pt.X += _firstRow;
			screen.marker.Move( pt );

			Reread();
			CalcScroll();
			Invalidate();
		}
	}
}

static int FindLastNoSpace( CHAR_INFO* s, int count ) //returns space number standing after the last non-space char or count
{
	int n = -1;

	for ( int i = 0; i < count; i++ )
		if ( s[i].Char.UnicodeChar > 32 ) { n = i; }

	return n + 1;
}

//need to fix reading of blocks >64k into clipboard (ReadConsoleOutputW doesn't read big blocks)

bool W32Cons::GetMarked( ClipboardText& ct )
{
	if ( screen.marker.Empty() )
	{
		return false;
	}

	int a = screen.marker.a.Y;
	int b = screen.marker.b.Y;

	if ( a > b ) { int t = a; a = b; b = t; }

	int C = screen.Cols();

	if ( C <= 0 ) { return false; }

	int rowCount = b - a + 1;
	_temp.SetSize( rowCount, C );
	COORD size;
	size.X = C;
	size.Y = rowCount;
	COORD pos;
	pos.X = 0;
	pos.Y = 0;
	SMALL_RECT srect;
	srect.Left = 0 ;
	srect.Top = a;
	srect.Right = C;
	srect.Bottom = b + 1;

	if ( !ReadConsoleOutputW( outHandle, _temp.Ptr(), size, pos, &srect ) )
	{
		return false;
	}

	C = srect.Right - srect.Left + 1;
	int R = srect.Bottom - srect.Top + 1;

	if ( R <= 0 ) { return false; }

	if ( R == 1 )
	{
		int x1 = screen.marker.a.X;
		int x2 = screen.marker.b.X;

		if ( x1 > x2 ) { int t = x1; x1 = x2; x2 = t; }

		CHAR_INFO* s = _temp.Line( 0 );

		if ( !s ) { return false; }

		for ( ; x1 < x2; x1++ ) { ct.Append( s[x1].Char.UnicodeChar ); }

		return true;
	}
	else
	{
		int x1 = screen.marker.a.X;
		int x2 = screen.marker.b.X;
		CHAR_INFO* s = _temp.Line( 0 );

		if ( !s ) { return false; }

		int i;
		int n = FindLastNoSpace( s, C );

		for ( i = x1; i < C && i < n; i++ )
		{
			ct.Append( s[i].Char.UnicodeChar );
		}

		ct.Append( '\n' );

		for ( i = 1; i < R - 1; i++ )
		{
			CHAR_INFO* s = _temp.Line( i );

			if ( !s ) { return false; }

			n = FindLastNoSpace( s, C );

			for ( int j = 0; j < n; j++ ) { ct.Append( s[j].Char.UnicodeChar ); }

			ct.Append( '\n' );
		}

		s = _temp.Line( R - 1 );

		if ( !s ) { return false; }

		n = FindLastNoSpace( s, x2 );

		for ( i = 0; i < n; i++ ) { ct.Append( s[i].Char.UnicodeChar ); }

		return true;
	}

//...

	return false;
}


void W32Cons::MarkerClear()
{
	if ( screen.marker.Empty() )
	{
		return;
	}

	screen.marker.Reset();
	Invalidate();
}

void W32Cons::ThreadSignal( int id, int data )
{
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


bool W32Cons::EventMouse( cevent_mouse* pEvent )
{

	cpoint coord = pEvent->Point();
	COORD pt;
	pt.X = coord.x / cW;
	pt.Y = coord.y / cH;

	bool pointChanged = lastMousePoint != pt;
	lastMousePoint = pt;

//	bool shift = ( pEvent->Mod() & KM_SHIFT ) != 0;
//	bool ctrl = ( pEvent->Mod() & KM_CTRL ) != 0;

	switch ( pEvent->Type() )
	{
		case EV_MOUSE_MOVE:
			if ( IsCaptured() && pointChanged )
			{
				pt.Y += _firstRow;
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
					pt.Y += _firstRow;
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
					pt.Y += _firstRow;
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

			pt.Y += _firstRow;
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


void W32Cons::DrawRow( wal::GC& gc, int r, int first, int last )
{
	int rows = screen.Rows();

	if ( r < 0 || r  >= rows ) { return; }

	int cols = screen.Cols();

	if ( first >= cols ) { first = cols - 1; }

	if ( first < 0 ) { first = 0; }

	if ( last >= cols ) { last = cols - 1; }

	if ( last < 0 ) { last = 0; }

	if ( first > last ) { return; }

	int cursor = this->consLastInfo.dwCursorPosition.Y == r + _firstRow ? consLastInfo.dwCursorPosition.X : -1;

	if ( cursor < first || cursor > last ) { cursor = -1; }

	int y = r * cH;
	int x = first * cW;

	CHAR_INFO* p =  screen.Get( r );

	ConsMarker m = screen.marker;
	int absRow = _firstRow + r;

	while ( first <= last )
	{
		int n = 1;
		COORD coord;
		coord.Y = absRow;
		coord.X = first + 1;

		bool marked = m.In( coord );

		coord.X = first + 1;

		for ( int i = first + 1; i <= last; i++, n++, coord.X++ )
		{
			if ( p[i].Attributes != p[i - 1].Attributes ||
			     m.In( coord ) != marked )
			{
				break;
			}
		}

		unsigned c = p[first].Attributes & 0xFF;

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
				buf[i] = p[first + i].Char.UnicodeChar;
			}

			gc.TextOutF( x, y, buf, count );
			n -= count;
			first += count;
			x += count * cW;
		}
	}

	if ( cursor >= 0 )
	{
		unsigned c = p[cursor].Attributes & 0xFF;

		gc.SetFillColor( default_color_table[c & 0xF] );
		gc.SetTextColor( default_color_table[( c >> 4 ) & 0xF] );
		unicode_t sym = p[cursor].Char.UnicodeChar;
		gc.TextOutF( cursor * cW, y, &sym, 1 );
	}

}


void W32Cons::Paint( wal::GC& gc, const crect& paintRect )
{
	crect clientRect = ClientRect();
	gc.Set( GetFont() );

	int R = screen.Rows();
	int C = screen.Cols();

//for some reason when reading big blocks ReadConsoleOutputW returns ERROR_NOT_ENOUGH_MEMORY
//that's why we read by chunks

	if ( R > 0 && C > 0 )
	{
		const int MAX_BLOCK = 1024;

		int step = MAX_BLOCK / C;

		if ( step <= 0 ) { step = 1; }

		_temp.SetSize( step, C );
		COORD size;
		size.X = C;
		size.Y = step;

		COORD pos;
		pos.X = 0;
		pos.Y = 0;

		int row = 0;

		while ( row < R )
		{
			//int n = step;
			//if (row + n > R) n = R - row;

			int n = R - row;

			if ( n > step ) { n = step; }

			SMALL_RECT srect;
			srect.Left = 0 ;
			srect.Top = _firstRow + row;
			srect.Right = C - 1;
			srect.Bottom = _firstRow + row + n - 1;

			//not clear, if not doing this, then ReadConsoleOutputW corrupts memory, but should return out of bounds
			if ( srect.Bottom >= consLastInfo.dwSize.Y )
			{
				srect.Bottom = consLastInfo.dwSize.Y - 1;
			}

			if ( ReadConsoleOutputW( outHandle, _temp.Ptr(), size, pos, &srect ) )
			{
				int y = 0;

				for ( int i = 0; i < n; i++, y += cH )
				{
					CHAR_INFO* dest = screen.Get( i + row );
					CHAR_INFO* s = _temp.Line( i );
					memcpy( dest, s, sizeof( CHAR_INFO ) *C );
					DrawRow( gc, i + row, 0, C - 1 );
				}
			}

			row += n;
		}

		gc.SetFillColor( default_color_table[( ( consLastInfo.wAttributes ) >> 4 ) & 0xF] );

		crect r1 = clientRect;
		r1.left += C * cW;
		r1.bottom = R * cH;
		gc.FillRect( r1 );

		r1 = clientRect;
		r1.top = R * cH;
		gc.FillRect( r1 );
		return;
	}

	gc.SetFillColor( 0 );
	gc.FillRect( clientRect );

}

W32Cons::~W32Cons()
{
	SetConsoleCtrlHandler( ConsoleHandlerRoutine, FALSE );
	//try to close console and if not done, then show it at least
	HWND h = ::GetConsoleWindow();

	if ( h )
	{
		::SendMessage( h, WM_CLOSE, 0, 0 );
		::ShowWindow( h, SW_SHOW );
	}
}
