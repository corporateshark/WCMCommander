/*
   Copyright (c) by Valery Goryachev (Wal)
*/


#include "swl.h"
#include "swl_wincore_internal.h"
#ifdef _WIN32

namespace wal
{
	cpoint GetScreenSize()
	{
		cpoint s;

		s.x = GetSystemMetrics( SM_CXFULLSCREEN );
		s.y = GetSystemMetrics( SM_CYFULLSCREEN );

		return s;
	}

	HINSTANCE appInstance = 0;

	static char wClass[64] = "";
	static BYTE keyStates[0x100];

	Win* mouseWindow = 0;

	Win* GetWinByID( WinID hWnd );
	int DelWinFromHash( WinID w );
	unsigned RunTimers();
	void AddWinToHash( WinID handle, Win* w );
	void AppBlock( WinID w );
	void AppUnblock( WinID w );

	inline WinID GetIDByWin( Win* w )
	{
		return w ? w->GetID() : 0;
	}

	static bool MouseEvent( int type, int button, Win*  w, WPARAM wParam, LPARAM lParam )
	{
//	if (!node) return false;
		if ( w != mouseWindow )
		{
			WinID wId = GetIDByWin( w );
			WinID mId = GetIDByWin( mouseWindow );

			cevent ev_enter( EV_ENTER );
			w->Event( &ev_enter );

			mouseWindow = GetWinByID( mId );

			if ( mouseWindow )
			{
				cevent ev_leave( EV_LEAVE );
				mouseWindow->Event( &ev_leave );
			}

			mouseWindow = GetWinByID( wId );

			if ( !GetWinByID( wId ) ) { return true; }
		}

		if ( !w->IsEnabled() ) { return false; }

		if ( w->Blocked() ) { return false; }

		int xPos = lParam & 0xFFFF;
		int yPos = ( lParam >> 16 ) & 0xFFFF;

		if ( xPos & 0x8000 ) { xPos |= ~0xFFFF; }

		if ( yPos & 0x8000 ) { yPos |= ~0xFFFF; }

		unsigned km = 0;

		if ( wParam & MK_CONTROL ) { km |= KM_CTRL; }

		if ( wParam & MK_SHIFT ) { km |= KM_SHIFT; }

		unsigned bf = 0;

		if ( wParam & MK_LBUTTON ) { bf |= MB_L; }

		if ( wParam & MK_MBUTTON ) { bf |= MB_M; }

		if ( wParam & MK_RBUTTON ) { bf |= MB_R; }

		if ( wParam & MK_XBUTTON1 ) { bf |= MB_X1; }

		if ( wParam & MK_XBUTTON2 ) { bf |= MB_X2; }

		cevent_mouse ev( type, cpoint( xPos, yPos ), button, bf, km );

		return w->Event( &ev );
	}

	void CheckMousePosition()  //needs to be run periodically (2 times a second)
	{
		if ( mouseWindow )
		{
			POINT p;
			::GetCursorPos( &p );
			HWND w = WindowFromPoint( p );

			if ( w != mouseWindow->GetID() && w != ::GetCapture() )
			{
				cevent ev_leave( EV_LEAVE );
				mouseWindow->Event( &ev_leave );
			}

			mouseWindow = 0;
		}
	}

	static bool ChildKeyRecursive( Win* parent, Win* child, cevent_key* ev )
	{
		if ( !parent ) { return false; }

		if ( parent->Type() != Win::WT_MAIN && ChildKeyRecursive( parent->Parent(), child, ev ) ) { return true; }

		return parent->IsEnabled() && parent->EventChildKey( child, ev );
	}

	static bool KeyEvent( int type, Win* win, WPARAM wParam, LPARAM lParam, bool FromMouseWheel )
	{
		if ( win->Blocked() ) { return false; }

		GetKeyboardState( keyStates );
		wchar_t ch;

		if ( ToUnicode( wParam, lParam, keyStates, &ch, 1, 0 ) < 0 )
		{
			ch = 0;
		}

		unsigned int km = 0;

		if ( ( keyStates[VK_LSHIFT] & 0x80 ) || ( keyStates[VK_RSHIFT] & 0x80 ) )
		{
			km |= KM_SHIFT;
			km |= (( keyStates[VK_LSHIFT] & 0x80 ) ? EXT_KM_LSHIFT : EXT_KM_RSHIFT);
		}

		if ( ( keyStates[VK_LCONTROL] & 0x80 ) || ( keyStates[VK_RCONTROL] & 0x80 ) )
		{
			km |= KM_CTRL;
			km |= (( keyStates[VK_LCONTROL] & 0x80 ) ? EXT_KM_LCTRL : EXT_KM_RCTRL);
		}

		if ( ( keyStates[VK_LMENU] & 0x80 ) || ( keyStates[VK_RMENU] & 0x80 ) )
		{
			km |= KM_ALT;
			km |= (( keyStates[VK_LMENU] & 0x80 ) ? EXT_KM_LALT : EXT_KM_RALT);
		}

		cevent_key ev( type, wParam, km, lParam & 0xFFFF, ch, FromMouseWheel );

		if ( win->Type() != Win::WT_MAIN && ChildKeyRecursive( win->Parent(), win, &ev ) )
		{
			return true;
		}

		if ( !win->IsEnabled() ) { return false; }

		return win->EventKey( &ev ); //Event() ???
	}


// needs to revise exceptions
	LRESULT CALLBACK WProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		try
		{

			Win* win = GetWinByID( hWnd );

			if ( !win )
			{
				return DefWindowProc( hWnd, message, wParam, lParam );
			}

			switch ( message )
			{
				case WM_CREATE:
					return 0; //return 1;

				case WM_NCACTIVATE:
//	break;
				{
					// this is done for activation and deactivation of popups tree
					// borders of all tree should be active or inactive
					if ( !win ) { break; }

					if ( win->IsOneParentWith( ( HWND )lParam ) ) { return TRUE; }

					if ( lParam == -1 ) { break; }

					lParam = -1;
					ccollect<HWND> wl;
					Win* p = win;

					while ( p->Parent() ) { p = p->Parent(); }

					p->PopupTreeList( wl );

					for ( int i = 0; i < wl.count(); i++ )
						if ( wl[i] != hWnd )
						{
							::SendMessage( wl[i], message, wParam, lParam );
						}

					break;
				}


				case WM_ERASEBKGND:
					return 0;

				case WM_GETMINMAXINFO:
					return 1;

				case WM_DESTROY:
				{
					bool post = !DelWinFromHash( hWnd );
					LRESULT res = DefWindowProc( hWnd, message, wParam, lParam );

					if ( post ) { ::PostQuitMessage( 0 ); }

					return res;
				}

				case WM_SETFOCUS:
				{
					if ( win->blockedBy )
					{
						::SetFocus( win->blockedBy );
						return 0;
					}

					Win::focusWinId = hWnd;

					if ( win->parent ) { win->parent->lastFocusChild = hWnd; }

					cevent cev( EV_SETFOCUS );
					win->Event( &cev );
					return 0;
				}

				case WM_KILLFOCUS:
				{
					Win::focusWinId = 0;
					// win->ClearState(Win::S_FOCUS);
					cevent cev( EV_KILLFOCUS );
					win->Event( &cev );
					return 0;
				}

				case WM_SHOWWINDOW:
				{
					cevent_show cevs( wParam != FALSE );
					win->Event( &cevs );
					return 0;
				}

				case WM_CLOSE:
				{
					cevent cev( EV_CLOSE );
					return ( win->Event( &cev ) ) ? DefWindowProc( hWnd, message, wParam, lParam ) : 0;
				}

				case WM_ACTIVATE:
					if ( wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE )
					{
						if ( win->blockedBy )
						{
							::SetFocus( win->blockedBy );
							//::SetActiveWindow(win->blockedBy);
							return 0;
						}
						else
						{
							cevent_activate ev( true, GetWinByID( ( HWND )lParam ) );
							win->Event( &ev );
							return 0;
						}
					}

					// deactivated
					// (WA_INACTIVE)
					{
						cevent_activate ev( false, GetWinByID( ( HWND )lParam ) );
						win->Event( &ev );
						return 0;
					}
					break;


				case WM_PAINT:
				{
					{
						PAINTSTRUCT ps;
						HDC hdc = ::BeginPaint( hWnd, &ps );

						try
						{
							crect rect( ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom );
							GC gc( hdc, false );
							win->Paint( gc, rect );
						}
						catch ( ... )
						{
							::EndPaint( hWnd, &ps );
							throw;
						}

						::EndPaint( hWnd, &ps );
					}
					return 0;
				}
				break;

				case WM_MOUSEMOVE:
					return MouseEvent( EV_MOUSE_MOVE, 0, win, wParam, lParam ) ? 0 : 1;

				case WM_LBUTTONDOWN:
					return MouseEvent( EV_MOUSE_PRESS,   MB_L, win, wParam, lParam ) ? 0 : 1;

				case WM_LBUTTONUP:
					return MouseEvent( EV_MOUSE_RELEASE, MB_L, win, wParam, lParam );

				case WM_LBUTTONDBLCLK :
					return MouseEvent( EV_MOUSE_DOUBLE,  MB_L, win, wParam, lParam );

				case WM_MBUTTONDOWN:
					return MouseEvent( EV_MOUSE_PRESS,   MB_M, win, wParam, lParam );

				case WM_MBUTTONUP:
					return MouseEvent( EV_MOUSE_RELEASE, MB_M, win, wParam, lParam );

				case WM_MBUTTONDBLCLK :
					return MouseEvent( EV_MOUSE_DOUBLE,  MB_M, win, wParam, lParam );

				case WM_RBUTTONDOWN:
					return MouseEvent( EV_MOUSE_PRESS,   MB_R, win, wParam, lParam );

				case WM_RBUTTONUP:
					return MouseEvent( EV_MOUSE_RELEASE, MB_R, win, wParam, lParam );

				case WM_RBUTTONDBLCLK :
					return MouseEvent( EV_MOUSE_DOUBLE,  MB_R, win, wParam, lParam );

				case WM_XBUTTONDOWN:
					return MouseEvent( EV_MOUSE_PRESS,   ( wParam & XBUTTON1 ) ? MB_X1 : MB_X2, win, wParam, lParam );

				case WM_XBUTTONUP:
					return MouseEvent( EV_MOUSE_RELEASE, ( wParam & XBUTTON1 ) ? MB_X1 : MB_X2, win, wParam, lParam );

				case WM_XBUTTONDBLCLK :
					return MouseEvent( EV_MOUSE_DOUBLE,  ( wParam & XBUTTON1 ) ? MB_X1 : MB_X2, win, wParam, lParam );

				case WM_MOUSEWHEEL:
				{
					if ( static_cast< int >( wParam ) > 0 )
					{
						// wheel up
						KeyEvent( EV_KEYDOWN, win, VK_UP, 0, true );
						KeyEvent( EV_KEYUP,   win, VK_UP, 0, true );
					}
					else
					{
						// wheel down
						KeyEvent( EV_KEYDOWN, win, VK_DOWN, 0, true );
						KeyEvent( EV_KEYUP, win, VK_DOWN, 0, true );
					}

					return true;
				}

				case WM_SYSKEYDOWN:
				case WM_KEYDOWN:
					if ( KeyEvent( EV_KEYDOWN, win, wParam, lParam, false ) )
					{
						return 0;
					}

					break;

				case WM_SYSKEYUP:
				case WM_KEYUP:
					if ( KeyEvent( EV_KEYUP, win, wParam, lParam, false ) )
					{
						return 0;
					}

					break;

				case WM_CHAR:
					break;

				case WM_SIZE:
				{
					cevent_size cevs( cpoint ( lParam & 0xFFFF, ( lParam >> 16 ) & 0xFFFF ) );
					return win->Event( &cevs );
				}

				case WM_MOVE:
				{
					cevent_move cevm( cpoint ( lParam & 0xFFFF, ( lParam >> 16 ) & 0xFFFF ) );
					return win->Event( &cevm );
				}
				case WM_SYSCOMMAND:
				{
					// without this pressing Alt-key freezes Ctrl|Shift switchings of the bottombar until Alt is pressed for the second time
					if (wParam == SC_KEYMENU && (lParam >> 16) <= 0) 
						return 0;
				}

			};

		}
		catch ( cexception* ex )
		{
			MessageBoxA( 0, ex->message(), "Error", MB_OK | MB_ICONERROR );
			ex->destroy();
		}

		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	size_t Win32HIconId = 0;
	HINSTANCE Win32HInstance = 0;

	static ATOM RegisterWClass()
	{
		if ( wClass[0] ) { return 0; }

		wClass[0] = 'A';
		wClass[1] = 'B';
		wClass[2] = 'C';
		wClass[4] = 0;

		WNDCLASSEX wcex;
		wcex.cbSize = sizeof( WNDCLASSEX );
		wcex.style     = CS_DBLCLKS |
		                 CS_HREDRAW | CS_VREDRAW; //
		wcex.lpfnWndProc  = WProc;
		wcex.cbClsExtra      = 0;
		wcex.cbWndExtra      = 0;
		wcex.hInstance    = appInstance;
		wcex.hIcon     = LoadIcon( Win32HInstance, ( LPCTSTR )Win32HIconId );
		wcex.hCursor      = LoadCursor( 0, IDC_ARROW );
		wcex.hbrBackground   = ( HBRUSH )( COLOR_WINDOW + 1 );
		wcex.lpszMenuName = 0;
		wcex.lpszClassName   = wClass;
		wcex.hIconSm      = 0;

		ATOM a = RegisterClassEx( &wcex );

		if ( !a )
		{
			throw_syserr();
		}

		return a;
	}

	unsigned GetTickMiliseconds()
	{
		return ::GetTickCount();
	}

	void AppInit()
	{
		if ( appInstance ) { return; }

		BaseInit();
		appInstance = GetModuleHandle( 0 );
		RegisterWClass();
	}

	static bool appExit = false;

	void AppExit() { appExit = true; }


	int AppRun()
	{
		MSG msg;

		while ( true )
		{
			wth_DoEvents();

			if ( appExit ) { return 0; }

			if ( !PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
			{
				unsigned waitTime = RunTimers();

				if ( waitTime > 5000 ) { waitTime = 5000; }

				HANDLE thEvent = wthInternalEvent.SignalFD();
				DWORD res = MsgWaitForMultipleObjects( 1, &thEvent, FALSE, waitTime, QS_ALLINPUT );

				if ( res == WAIT_TIMEOUT ) { CheckMousePosition(); }

				continue;
			}

			if ( !GetMessage( &msg, NULL, 0, 0 ) ) { break; }

			DispatchMessage( &msg );
		}

		return 0;
	}


	static std::vector<wchar_t> GetWCT( const unicode_t* s, int charCount, int* retCount )
	{
		if ( charCount < 0 )
		{
			charCount = 0;

			for ( const unicode_t* t = s; *t; t++ ) { charCount++; }
		}

		std::vector<wchar_t> p( charCount + 1 );

		for ( int i = 0; i < charCount; i++ ) { p[i] = s[i]; }

		p[charCount] = 0;

		if ( retCount ) { *retCount = charCount; }

		return p;
	}



////////////////////////////// Win

	Win::Win( WTYPE t, unsigned hints, Win* _parent, const crect* rect, int uiNId )
		:
		type( t ),
		whint( hints ),
		parent( _parent ),
		captured( false ),
		state( 0 ),
		lastFocusChild( 0 ),
		blockedBy( 0 ),
		modal( 0 ),

		upLayout( 0 ),
		layout( 0 ),
		uiNameId( uiNId )
	{

		DWORD st = WS_CLIPSIBLINGS | WS_CLIPCHILDREN  ;

		switch ( type )
		{
			case WT_CHILD:
				st |= WS_CHILD;
				break;

			case WT_POPUP:
				st |= WS_POPUP;
				break;

			case WT_MAIN:
			default:
				if ( ( hints & WH_MINBOX ) != 0 ) { st |= WS_MINIMIZEBOX; }

				if ( ( hints & WH_MAXBOX ) != 0 ) { st |= WS_MAXIMIZEBOX; }

				if ( ( hints & WH_SYSMENU ) != 0 ) { st |= WS_SYSMENU; }

				if ( ( hints & WH_RESIZE ) != 0 ) { st |= WS_SIZEBOX ; }

//		if ((hints & WH_OVERLAPPED) !=0 ) st |= WS_OVERLAPPED;
		}

		crect r;

		if ( rect ) { r = *rect; }
		else
		{
			r.left = CW_USEDEFAULT;
			r.top = CW_USEDEFAULT;
			r.right = CW_USEDEFAULT;
			r.bottom = CW_USEDEFAULT;
		}

		{
			int X, Y, W, H;

			if ( !rect || ( type == WT_MAIN && ( hints & WH_USEDEFPOS ) != 0 ) )
			{
				X = CW_USEDEFAULT;
				Y = 0;
				W = CW_USEDEFAULT;
				H = 0;
			}
			else
			{
				X = rect->left;
				Y = rect->top;
				W = rect->Width();
				H = rect->Height();
			}

			handle = CreateWindow( wClass, "Hi", st,
			                       X, Y, W, H,
			                       ( parent && parent->handle ) ? parent->handle : 0, NULL, appInstance, NULL );
			DWORD e = GetLastError();
		}

		if ( !handle )
		{
			throw_syserr();
		}

		AddWinToHash( handle, this );

		if ( parent )
		{
			parent->childList.append( this );
		}

		if ( rect && !rect->IsEmpty() )
		{
			LSize ls;
			ls.Set( *rect );
			SetLSize( ls );
		}
	}


	int Win::DoModal()
	{
		WinID lastParentFC = parent ? parent->lastFocusChild : 0;
		bool visibled = IsVisible();
		bool enabled = IsEnabled();

		try
		{
			if ( !visibled ) { Show(); }

			if ( !enabled ) { Enable( true ); }

			if ( parent && type != WT_POPUP ) { parent->RecalcLayouts(); }

			AppBlock( GetID() );
			UnblockTree( GetID() );

			ModalStruct modalStruct;
			modal = &modalStruct;

			MSG msg;

			while ( !modalStruct.end )
			{
				wth_DoEvents();

				if ( modalStruct.end ) { break; }

				if ( !PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
				{
					unsigned waitTime = RunTimers();

					if ( waitTime > 5000 ) { waitTime = 5000; }

					if ( modalStruct.end ) { break; }

					HANDLE thEvent = wthInternalEvent.SignalFD();
					DWORD res = MsgWaitForMultipleObjects( 1, &thEvent, FALSE, waitTime, QS_ALLINPUT );

					if ( res == WAIT_TIMEOUT ) { CheckMousePosition(); }

					continue;
				}

				if ( !GetMessage( &msg, NULL, 0, 0 ) )
				{
					break;
				}

				DispatchMessage( &msg );
			}

			modal = nullptr;
			AppUnblock( GetID() );

			if ( !visibled ) { Hide(); }

///
			if ( type == WT_POPUP || ( type == WT_CHILD && parent ) )
			{
				Win* w = GetWinByID( lastParentFC );

				if ( w ) { w->SetFocus(); }
			}

			return modalStruct.id;
		}
		catch ( ... )
		{
			modal = 0;
			AppUnblock( GetID() );

			if ( !visibled ) { Hide(); }

			throw;
		}
	}

	void Win::Move( crect rect, bool repaint )
	{
		::MoveWindow( handle, rect.left, rect.top, rect.Width(), rect.Height(), repaint ? TRUE : FALSE );
	}

	/*
	void Win::SetCapture()
	{
	   if (!captured)
	   {
	      ::SetCapture(handle);
	      captured = true;
	   }
	}

	void Win::ReleaseCapture()
	{
	   if (captured && GetCapture() == handle)
	   {
	      ::ReleaseCapture();
	      captured = false;
	   }
	}
	*/

	bool Win::SetCapture( CaptureSD* sd )
	{
		if ( captured ) { return false; }

		HWND h = ::SetCapture( handle );
		captured = true;

		if ( sd ) { sd->h = h; }

		return true;
	}

	void Win::ReleaseCapture( CaptureSD* sd )
	{
		if ( !captured || GetCapture() != handle ) { return; }

		captured = false;

		if ( sd && sd->h )
		{
			::SetCapture( sd->h );
		}
		else
		{
			::ReleaseCapture();
		}
	}

	void Win::Activate()
	{
		::SetActiveWindow( handle );
	}

	void Win::Invalidate()
	{
		RECT r;
		::GetClientRect( handle, &r );
		::InvalidateRect( handle, &r, FALSE );
	}

	void Win::Maximize()
	{
		
	}

	void Win::Show( SHOW_TYPE type )
	{
		switch ( type )
		{
			case SHOW_ACTIVE: ::ShowWindow( handle, SW_SHOW );
				break;

			case SHOW_INACTIVE: ::ShowWindow( handle, SW_SHOWNA );
				break;

			case SHOW_MAXIMIZE: ::ShowWindow( handle, SW_MAXIMIZE );
				break;
		};

		SetState( S_VISIBLE );
	}


	void Win::SetName( const unicode_t* name )
	{
		SetWindowTextW( this->handle, GetWCT( name, -1, 0 ).data() );
	}

	void Win::SetName( const char* utf8Name )
	{
		SetName( utf8_to_unicode( utf8Name ).data() );
	}

	void Win::Hide()
	{
		::ShowWindow( handle, SW_HIDE );
		ClearState( S_VISIBLE );
	};


	void Win::RecalcLayouts()
	{
		if ( !layout ) { return; }

		wal::ccollect<WSS> list;
		layout->SetPos( ClientRect(), list );

		if ( list.count() > 0 )
		{
			HDWP hdwp = ::BeginDeferWindowPos( list.count() );

			if ( hdwp == NULL ) { return; }

			for ( int i = 0; i < list.count(); i++ )
			{
				crect r = list[i].rect;
				::DeferWindowPos( hdwp, list[i].w->GetID(), NULL, r.left, r.top, r.Width(), r.Height(), SWP_NOZORDER );
			}

			::EndDeferWindowPos( hdwp );
		}

		Invalidate();
	}

	void Win::SetFocus()
	{
		if ( InFocus() ) { return; }

		if ( !IsVisible() ) { return; }

//	if (type != WT_POPUP && parent) parent->SetFocus();
		::SetFocus( handle );
	}

	crect Win::ClientRect()
	{
		RECT rect;
		::GetClientRect( handle, &rect );
		return crect( rect.left, rect.top, rect.right, rect.bottom );
	}

	crect Win::Rect()
	{
		POINT p;
		p.x = 0;
		p.y = 0;
		::ClientToScreen( ( HWND )this->handle, &p );

		if ( type != WT_POPUP && this->parent )
		{
			ScreenToClient( parent->handle, &p );
		}

		RECT rect;
		::GetClientRect( ( HWND )handle, &rect );
		rect.left = p.x;
		rect.top = p.y;
		rect.right += p.x;
		rect.bottom += p.y;
		return crect( rect.left, rect.top, rect.right, rect.bottom );
	}

	crect Win::ScreenRect()
	{
		RECT rect;
		::GetWindowRect( handle, &rect );
		return crect( rect.left, rect.top, rect.right, rect.bottom );
	}

	void Win::ClientToScreen( int* x, int* y )
	{
		POINT point;
		point.x = *x;
		point.y = *y;
		::ClientToScreen( handle, &point );
		*x = point.x;
		*y = point.y;
	}

	void Win::OnTop()
	{
		::SetWindowPos( handle, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
	}


	Win::~Win()
	{
		wth_DropWindow( this );

		if ( modal ) // ???? does this needed?
		{
			( ( ModalStruct* )modal )->EndModal( 0 );
		}

		if ( handle )
		{
			DelAllTimers();
			DestroyWindow( handle );
			DelWinFromHash( handle );
		}

		for ( int i = 0; i < childList.count(); i++ )
		{
			childList[i]->parent = 0;
		}

		if ( parent )
		{
			for ( int i = 0; i < parent->childList.count(); i++ )
				if ( parent->childList[i] == this )
				{
					parent->childList.del( i );
					break;
				}
		}

		if ( upLayout )
		{
			upLayout->DelObj( this );
		}

		if ( mouseWindow == this )
		{
			mouseWindow = 0;
		}
	}




////////////////////////////////////////// GC
	GC::GC( HDC h, bool needDel )
		:  handle( h ),
		   needDelete( needDel ),
		   savedPen( 0 ),
		   savedBrush( 0 ),
		   savedFont( 0 ),
		   bkMode( -1 ),
		   linePen( 0 ),
		   fillBrush( 0 ),
		   fillRgb( 0 ),
		   lineRgb( 0 ),
		   lineWidth( 1 ),
		   lineStyle( SOLID ),
		   textRgb( 0 ), //textColor(0),
		   textColorSet( false ),
		   bkColorSet( false )
	{
	}


	GC::GC( Win* win )
		:  handle( ::GetDC( win ? win->GetID() : 0 ) ),
		   needDelete( true ),
		   savedPen( 0 ),
		   savedBrush( 0 ),
		   savedFont( 0 ),
		   bkMode( -1 ),
		   linePen( 0 ),
		   fillBrush( 0 ),
		   fillRgb( 0 ),
		   lineRgb( 0 ),
		   lineWidth( 1 ),
		   lineStyle( SOLID ),
		   textRgb( 0 ), //textColor(0),
		   textColorSet( false ),
		   bkColorSet( false )
	{
	}


	void GC::Restore()
	{
		if ( savedPen ) { ::SelectObject( handle, savedPen ); savedPen = 0; };

		if ( savedBrush ) { ::SelectObject( handle, savedBrush ); savedBrush = 0; }

		if ( savedFont ) { ::SelectObject( handle, savedFont ); savedFont = 0; }

		if ( linePen ) {::DeleteObject( linePen ); linePen = 0; }

		if ( fillBrush ) {::DeleteObject( fillBrush ); fillBrush = 0; }

		///::SelectClipRgn(handle, 0); ???
	}

	void GC::SetFillColor( unsigned rgb )
	{
		::SetBkColor( handle, rgb );

		if ( fillBrush && fillRgb == rgb ) { return; }

		fillBrush = ::CreateSolidBrush( rgb );
		fillRgb = rgb;
		HBRUSH b = ( HBRUSH ) ::SelectObject( handle, fillBrush );

		if ( b )
		{
			if ( savedBrush )
			{
				::DeleteObject( b );
			}
			else
			{
				savedBrush = b;
			}
		}

		bkColorSet = false;
	}

	void GC::SetLine( unsigned rgb, int width, int style )
	{
		if ( linePen && lineRgb == rgb && lineWidth == width && lineStyle == style ) { return; }

		int s;

		switch ( style )
		{
			case DOT:
				s = PS_DOT;
				break;

			case SOLID:
			default:
				s = PS_SOLID;
		}

		lineRgb = rgb;
		lineWidth = width;
		lineStyle = style;
		linePen = ::CreatePen( s, width, rgb );
		HPEN p = ( HPEN ) ::SelectObject( handle, linePen );

		if ( p )
		{
			if ( savedPen )
			{
				::DeleteObject( p );
			}
			else
			{
				savedPen = p;
			}
		}
	}

	void GC::SetTextColor( unsigned rgb )
	{
		if ( textColorSet && textRgb == rgb ) { return; }

		::SetTextColor( handle, rgb );
		textRgb = rgb;
		textColorSet = true;
	}


	void GC::Set( cfont* font )
	{
		if ( !font ) { return; }

		HFONT f = ( HFONT ) ::SelectObject( handle, font->handle );

		if ( f && !savedFont ) { savedFont = f; }
	}

	void GC::FillRect( crect r )
	{
		RECT rect = {r.left, r.top, r.right, r.bottom};
		::FillRect( handle, &rect, fillBrush );
	};

	void GC::FillRectXor( crect r )
	{
		//RECT rect = {r.left, r.top, r.right, r.bottom};
		::PatBlt( handle, r.left, r.top, r.Width(), r.Height(), PATINVERT );
	};

	void GC::SetClipRgn( crect* r )
	{
		rgn.Set( r );
		::SelectClipRgn( handle, rgn.handle );
	}

	void GC::DrawIcon( int x, int y, cicon* ico )
	{
		if ( ico ) { ico->Draw( *this, x, y ); }
	}

	void GC::MoveTo( int x, int y )
	{
		::MoveToEx( handle, x, y, 0 );
	};

	void GC::LineTo( int x, int y )
	{
		::LineTo( handle, x, y );
	}

	void GC::SetPixel( int x, int y, unsigned rgb )
	{
		::SetPixel( handle, x, y, rgb );
	}

	void GC::Ellipce( crect r )
	{
		::Ellipse( handle, r.left, r.top, r.right, r.bottom );
	}

	void GC::TextOut( int x, int y, const unicode_t* s, int charCount )
	{
		if ( bkMode )
		{
			::SetBkMode( handle, TRANSPARENT );
			bkMode = 0;
		}

		std::vector<wchar_t> p = GetWCT( s, charCount, &charCount );
		//::TextOutW(handle, x, y, p.ptr(), charCount);
		::ExtTextOutW( handle, x, y, ETO_IGNORELANGUAGE, 0, p.data(), charCount, 0 );
	}

	void GC::TextOutF( int x, int y, const unicode_t* s, int charCount )
	{
		if ( bkMode != 1 )
		{
			::SetBkMode( handle, OPAQUE );
			bkMode = 1;
		}

		std::vector<wchar_t> p = GetWCT( s, charCount, &charCount );
		//::TextOutW(handle, x, y, p.ptr(), charCount);
		::ExtTextOutW( handle, x, y, ETO_IGNORELANGUAGE, 0, p.data(), charCount, 0 );
	}


	cpoint GC::GetTextExtents( const unicode_t* s, int charCount )
	{
		if ( !s ) { return cpoint( 0, 0 ); }

		SIZE size;
		std::vector<wchar_t> p = GetWCT( s, charCount, &charCount );
		::GetTextExtentPoint32W( handle, p.data(), charCount, &size );
		return cpoint( size.cx, size.cy );
	}


	GC::~GC()
	{
		Restore();

		if ( handle && needDelete )
		{
			DeleteDC( handle );
		}
	}

///////////////////////////////////////// cfont
	static unsigned win32FontCharset = RUSSIAN_CHARSET;

	void cfont::SetWin32Charset( unsigned n )
	{
		win32FontCharset = n;
	}

	cfont::cfont( GC& gc, const char* name, int pointSize, cfont::Weight weight, unsigned flags )
		: external( false )
	{
		LOGFONT lf;
		memset( &lf, 0, sizeof( lf ) );
		lf.lfCharSet = win32FontCharset;
		lf.lfHeight = -MulDiv( pointSize, GetDeviceCaps( gc.handle, LOGPIXELSY ), 72 );

		//lf.lfEscapement;
		//lf.lfOrientation
		switch ( weight )
		{
			case Bold:
				lf.lfWeight = FW_BOLD;
				break;

			case Normal:
				lf.lfWeight = FW_NORMAL;
				break;

			case Light:
				lf.lfWeight = FW_LIGHT;
				break;

			default:
				lf.lfWeight = FW_DONTCARE;
				break;
		};

		lf.lfItalic = ( flags & Italic ) ? TRUE : FALSE;

		//lf.lfUnderline;
		//lf.lfStrikeOut;
		lf.lfOutPrecision = OUT_DEFAULT_PRECIS;

		lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;

		lf.lfQuality = DEFAULT_QUALITY;

		lf.lfPitchAndFamily = ( flags & Fixed ) ? FIXED_PITCH : DEFAULT_PITCH;

		strncpy( lf.lfFaceName, name, sizeof( lf.lfFaceName ) - 1 );

		handle = CreateFontIndirect( &lf );
	}

	std::vector<char> cfont::LogFontToUru( LOGFONT& lf )
	{
		std::vector<char> uri;
		uri.push_back( '-' );

		GC gc( ( Win* )0 );

		int py = GetDeviceCaps( gc.handle, LOGPIXELSY );
		int size = 100;

		if ( lf.lfHeight < 0 )
		{
			size = MulDiv( -lf.lfHeight, 72, py ) * 10;
		};

		char buf[0x100];

		sprintf( buf, "%i", size );

		char* s;

		for ( s = buf; *s; s++ ) { uri.push_back( *s ); }

		uri.push_back( ':' );
		int i;

		for ( i = 0, s = lf.lfFaceName; *s && i < sizeof( lf.lfFaceName ); i++, s++ )
		{
			uri.push_back( *s );
		}

		uri.push_back( ':' );

		if ( lf.lfPitchAndFamily & FIXED_PITCH ) { uri.push_back( 'F' ); }

		if ( lf.lfItalic == TRUE ) { uri.push_back( 'I' ); }

		switch ( lf.lfWeight )
		{
			case FW_BOLD:
				uri.push_back( 'B' );
				break;

			case FW_NORMAL:
				uri.push_back( 'N' );
				break;

			case FW_LIGHT:
				uri.push_back( 'L' );
				break;
		};

		uri.push_back( 0 );

		return uri;
	}

	void cfont::UriToLogFont( LOGFONT* plf, const char* uri )
	{
		memset( plf, 0, sizeof( LOGFONT ) );
		const char* s = uri;

		if ( *s == '-' )
		{
			s++;
			int size = 0;

			for ( ; *s >= '0' && *s <= '9'; s++ ) { size = size * 10 + ( *s - '0' ); }

			if ( *s == ':' )
			{
				s++;
			}
			else
			{
				size = 100;
			}

			while ( *s && *s <= 32 ) { s++; }

			if ( size < 10 ) { size = 10; }

			if ( size > 1000 ) { size = 1000; }

			ccollect<char> name;

			for ( ; *s && *s != ':'; s++ )
			{
				name.append( *s );
			}

			name.append( 0 );

			if ( *s == ':' ) { s++; }

			char weight = 0;
			bool italic = false;
			char pitch = 0;

			for ( ; *s; s++ )
				switch ( *s )
				{
					case 'B':
					case 'N':
					case 'L':
						weight = *s;
						break;

					case 'F':
						pitch = *s;
						break;

					case 'I':
						italic = true;
				};

			plf->lfCharSet = win32FontCharset;

			GC gc( ( Win* )0 );

			plf->lfHeight = -MulDiv( size, GetDeviceCaps( gc.handle, LOGPIXELSY ), 72 ) / 10;

			switch ( weight )
			{
				case 'B':
					plf->lfWeight = FW_BOLD;
					break;

				case 'N':
					plf->lfWeight = FW_NORMAL;
					break;

				case 'L':
					plf->lfWeight = FW_LIGHT;
					break;

				default:
					plf->lfWeight = FW_DONTCARE;
					break;
			};

			plf->lfItalic = ( italic ) ? TRUE : FALSE;

			plf->lfOutPrecision = OUT_DEFAULT_PRECIS;

			plf->lfClipPrecision = CLIP_DEFAULT_PRECIS;

			plf->lfQuality = DEFAULT_QUALITY;

			plf->lfPitchAndFamily = ( pitch == 'F' ) ? FIXED_PITCH : DEFAULT_PITCH;

			strncpy( plf->lfFaceName, name.ptr(), sizeof( plf->lfFaceName ) - 1 );
		}
	}


	cfont::cfont( const char* uriStr )
		: external( false )
	{
		if ( uriStr && *uriStr == '-' ) //win32 font
		{
			LOGFONT lf;
			UriToLogFont( &lf, uriStr );
			handle = CreateFontIndirect( &lf );
		}

		//...
		_uri = uriStr;
		_name = uriStr;
	}


	const char* cfont::uri()
	{
		return _uri.data() ? _uri.data() : "";
	}


	const char* cfont::printable_name()
	{
		if ( _name.data() ) { return _name.data(); }

		return "";
	}


///////////////////////////////////////////// Clipboard

	void ClipboardSetText( Win* w, ClipboardText& text )
	{
		int maxSize = text.Count();

		int rCount = 0;
		int i;

		for ( i = 0; i < maxSize; i++ ) if ( text[i] == '\n' ) { rCount++; }

		HGLOBAL h = GlobalAlloc( GMEM_MOVEABLE | GMEM_DDESHARE, ( maxSize + rCount + 1 ) * sizeof( wchar_t ) );

		if ( h == NULL ) { throw_oom(); }

		wchar_t* p = ( wchar_t* )GlobalLock( h );

		if ( !p )
		{
			GlobalFree( h );
			return;
		}

		wchar_t* s = p;

		for ( int i = 0; i < maxSize; i++ )
		{
			unicode_t c = text[i];

			if ( c == '\n' ) { *( s++ ) = '\r'; }

			*( s++ ) = c;
		}

		*s = 0;
		GlobalUnlock( h );

		if ( OpenClipboard( NULL ) )
		{
			if ( !EmptyClipboard() || SetClipboardData( CF_UNICODETEXT, h ) == NULL )
			{
				( void )GetLastError();
				GlobalFree( h );
			}

			CloseClipboard();
		}
		else { GlobalFree( h ); }
	}

	void ClipboardGetText( Win* w, ClipboardText* text )
	{
		if ( !text || !OpenClipboard( w->GetID() ) ) { return; }

		try
		{
			HANDLE h = GetClipboardData( CF_UNICODETEXT );

			if ( !h ) { return; }

			wchar_t* s =  ( wchar_t* )GlobalLock( h );

			if ( s != NULL )
			{
				try
				{
					for ( ; *s; s++ )
					{
						if ( !( *s == '\r' && s[1] == '\n' ) )
						{
							text->Append( *s );
						}
					}
				}
				catch ( ... )
				{
					GlobalUnlock( h );
					throw;
				}

				GlobalUnlock( h );
			}
		}
		catch ( ... )
		{
			CloseClipboard();
			throw;
		}

		CloseClipboard();
	}

	void ClipboardClear()
	{
		EmptyClipboard();
	}

////////////////////////////////////////// Win32CompatibleBitmap



	Win32CompatibleBitmap::~Win32CompatibleBitmap()
	{
		clear();
	}

	void Win32CompatibleBitmap::clear()
	{
		if ( handle ) { ::DeleteObject( handle ); handle = 0; }

		mask.clear();
		_w = 0;
		_h = 0;
	}

	void Win32CompatibleBitmap::init( int w, int h )
	{
		clear();
		HDC dc = GetDC( 0 );

		if ( !dc ) { return; }

		handle = ::CreateCompatibleBitmap( dc, w, h );
		ReleaseDC( 0, dc );
		_w = w;
		_h = h;
	}

	void Win32CompatibleBitmap::Set( Image32& image )
	{
///!!! should be done right, not by using SetPixel !!!
		clear();
		int w = image.width();
		int h = image.height();

		if ( w <= 0 && h <= 0 ) { return; }

		init( w, h );

		if ( !handle ) { return; }

		HDC dc = CreateCompatibleDC( 0 );

		if ( !dc ) { return; }

		_w = w;
		_h = h;

		HGDIOBJ old = ::SelectObject( dc, handle );

		int i;
		bool masked = false;

		for ( int y = 0; y < h; y++ )
		{
			uint32_t* t = image.line( y );
			char* m = ( masked ) ? mask.data() + y * w : 0;

			for ( i = 0; i < w; i++ )
			{
				if ( t[i] > 0x80000000 )
				{
					if ( !masked )
					{
						mask.resize( w * h );
						memset( mask.data(), 1, w * h );
						masked = true;
						m = mask.data() + y * w;
					}

					m[i] = 0;
				}

				::SetPixel( dc, i, y, t[i] & 0xFFFFFF );
			}
		}

		::SelectObject( dc, old );
		DeleteDC( dc );
	}

	void Win32CompatibleBitmap::Put( wal::GC& gc, int src_x, int src_y, int dest_x, int dest_y, int w, int h )
	{
		if ( !handle ) { return; }

		int right = src_x + w;
		int bottom = src_y + h;

		if ( src_x < 0 ) { src_x = 0; }

		if ( src_y < 0 ) { src_y = 0; }

		if ( right > _w ) { right = _w; }

		if ( bottom > _h ) { bottom = _h; }

		w = right - src_x;
		h = bottom - src_y;

		if ( w <= 0 || h <= 0 ) { return; }

		HDC dc = CreateCompatibleDC( gc.W32Handle() );

		if ( !dc ) { return; }

		HGDIOBJ old = SelectObject( dc, handle );

		if ( mask.data() )
		{
			crect r( 0, -1, 0, -1 );
			char* m = mask.data();

			for ( int y = src_y; y < bottom; y++, m += _w )
			{
				int x = src_x;

				while ( x < right )
				{
					while ( x < right && !m[x] ) { x++; }

					if ( x < right )
					{
						int x1 = x;

						while ( x < right && m[x] ) { x++; }

						if ( r.bottom == y && r.left == x1 && r.right == x )
						{
							r.bottom++;
						}
						else
						{
							if ( !r.IsEmpty() )
							{
								BitBlt( gc.W32Handle(), dest_x + r.left, dest_y + r.top, r.Width(), r.Height(), dc, r.left, r.top,
								        SRCCOPY );
								// XPutImage(display, gc.GetXDrawable(), gc.XHandle(), &im, r.left, r.top,
								// dest_x + r.left, dest_y + r.top, r.Width(), r.Height());
							}

							r.Set( x1, y, x, y + 1 );
						}
					}
				}
			}

			if ( !r.IsEmpty() )
			{
				BitBlt( gc.W32Handle(), dest_x + r.left, dest_y + r.top, r.Width(), r.Height(), dc, r.left, r.top,
				        SRCCOPY );
				//XPutImage(display, gc.GetXDrawable(), gc.XHandle(), &im, r.left, r.top,
				// dest_x + r.left, dest_y + r.top, r.Width(), r.Height());
			}


		}
		else
		{
			BitBlt( gc.W32Handle(), dest_x, dest_y, w, h, dc, src_x, src_y,
			        SRCCOPY );
			//XPutImage(display, gc.GetXDrawable(), gc.XHandle(), &im, src_x, src_y, dest_x, dest_y, w, h);
		}

		::SelectObject( dc, old );
		DeleteDC( dc );
	}


//////////////////////////////////////// cicon
	extern void MakeDisabledImage32( Image32* dest, const Image32& src );

	void cicon::Draw( wal::GC& gc, int x, int y, bool enabled )
	{
		if ( !data ) { return; }

		if ( enabled )
		{
			if ( !data->normal.ptr() )
			{
				data->normal = new Win32CompatibleBitmap( data->image );
			}

			data->normal->Put( gc, 0, 0, x, y, data->image.width(), data->image.height() );
		}
		else
		{
			if ( !data->disabled.ptr() )
			{
				Image32 dis;
				MakeDisabledImage32( &dis, data->image );
				data->disabled = new Win32CompatibleBitmap( dis );
			}

			data->disabled->Put( gc, 0, 0, x, y, data->image.width(), data->image.height() );
		}
	}


	void cicon::DrawF( wal::GC& gc, int x, int y, bool enabled )
	{
		//...
		Draw( gc, x, y, enabled );
	}


}; //namespace wal
#endif
