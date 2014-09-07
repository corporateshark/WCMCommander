/*
   Copyright (c) by Valery Goryachev (Wal)
*/


#include <X11/Xutil.h>

#include "swl.h"
#include "swl_wincore_internal.h"
#include <sys/times.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>

#ifdef USEFREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#if defined( __APPLE__ )
const unsigned int MetaMask = 0x0010;
#endif

namespace wal
{

	Win* GetWinByID( WinID hWnd );
	int DelWinFromHash( WinID w );
	unsigned RunTimers();
	void AddWinToHash( WinID handle, Win* w );
	void AppBlock( WinID w );
	void AppUnblock( WinID w );

//static
	Display*     display = 0;
//static
	XVisualInfo visualInfo;
	static int     screen = 0;
	static Colormap      colorMap;
	static Visual*     visual;
	static XIM     inputMethod = 0;
	static XIC     inputContext = 0; //???

	static Atom       atom_WM_PROTOCOLS;
	static Atom       atom_WM_DELETE_WINDOW = 0; //Атом, чтоб window manager не закрывал окна самостоятельно

//WinID     Win::focusWinId =0;
	static WinID      activeWinId = 0;

//static SCImage winIcon;
	static Pixmap winIconPixmap = None;
	static Pixmap winIconMask = None;

	static unsigned toNone( unsigned n );
//static
	unsigned ( *CreateColor )( unsigned ) = toNone; //toRGB24;//toNone;
	static int connectionId = -1;
	static unsigned whiteColor = 0xFFFFFF, blackColor = 0;

	inline unsigned CreateColorFromRGB( unsigned c ) { return CreateColor( c ); }

///////////////////////
	clock_t CPS = 1;
	static void TimeInit()
	{
		CPS = sysconf( _SC_CLK_TCK );

		if ( CPS <= 0 ) { CPS = 1; }
	}

// ????????????????????????????
	unsigned GetTickMiliseconds()
	{
		struct tms t;
		clock_t ct = times( &t );
		return ( ct * 1000 ) / CPS;
	}



////////////  FreeType inc /////////////////////
#include "swl_wincore_freetype_inc.h"


///// { Clipboard data

	static ClipboardText clipboardText;
	static Atom atom_PRIMARY = 0;
	static Atom atom_CLIPBOARD = 0;
	static Atom atom_TARGETS = 0;
	static Atom atom_STRING = 0;
	static Atom atom_UTF8 = 0;
	static Atom atom_COMPOUND = 0;
	static Atom atom_CDEST = 0;
	static Atom atom_INCR = 0;

	static Window clipboardWinId = 0;

	static void X11_Clipbopard_Init()
	{
		atom_PRIMARY = XInternAtom( display, "PRIMARY", False );
		atom_CLIPBOARD = XInternAtom( display, "CLIPBOARD", False );

		atom_STRING = XInternAtom( display, "STRING", True );
		atom_UTF8 = XInternAtom( display, "UTF8_STRING", False );
		atom_TARGETS = XInternAtom( display, "TARGETS", False );
		atom_COMPOUND = XInternAtom( display, "COMPOUND_TEXT", False );
		atom_CDEST = XInternAtom( display, "SWL_CLIPDEST", False );
		atom_INCR = XInternAtom( display, "INCR", False );

		XSetWindowAttributes attrs;

		attrs.event_mask =  PropertyChangeMask;

		unsigned long valueMask = CWEventMask;

		valueMask |= CWOverrideRedirect;
		attrs.override_redirect = True;

		clipboardWinId = XCreateWindow(
		                    display,
		                    DefaultRootWindow( display ),
		                    0, 0, 10, 10,
		                    0,
		                    CopyFromParent,
		                    InputOutput,
		                    visual,
		                    valueMask, &attrs );
	}

	void ClipboardSetText( Win* w, ClipboardText& text )
	{
		clipboardText = text;
		XSetSelectionOwner( display, atom_PRIMARY, clipboardWinId, CurrentTime );
		XSetSelectionOwner( display, atom_CLIPBOARD, clipboardWinId, CurrentTime );
	}


	void ClipboardClear()
	{
		clipboardText.Clear();
		XSetSelectionOwner( display, atom_PRIMARY, None, CurrentTime );
		XSetSelectionOwner( display, atom_CLIPBOARD, None, CurrentTime );
	}



///// } (Clipboard data)



///////////////////////////////////   Init


//////////////////////////////// PseudoColor

#define CBCOUNT (16*16*16)
	static int stColors[CBCOUNT];

	inline int AbsInt( int a ) { return a < 0 ? -a : a; }

	struct RgbColorStruct
	{
		int r, g, b;
		RgbColorStruct(): r( 0 ), g( 0 ), b( 0 ) {}
		RgbColorStruct( unsigned bgr ): r( bgr & 0xFF ), g( ( bgr >> 8 ) & 0xFF ), b( ( bgr >> 16 ) & 0xFF ) {}
		void operator = ( unsigned bgr ) { r = ( bgr & 0xFF ); g = ( bgr >> 8 ) & 0xFF; b = ( bgr >> 16 ) & 0xFF; }
		unsigned long Dist2( RgbColorStruct& a )
		{
			unsigned long dr = AbsInt( r - a.r );
			unsigned long dg = AbsInt( g - a.g );
			unsigned long db = AbsInt( b - a.b );
			return dr * dr + dg * dg + db * db;
		}
	};

	struct TempStruct
	{
		RgbColorStruct rgb;
		int lastCNum;
		unsigned long lastDist2;
		TempStruct(): lastCNum( 0 ), lastDist2( 0xFFFFFFFF ) {}
	};

	static void MakeStColors( unsigned* col, int count )
	{
		TempStruct ts[CBCOUNT];

		int i;

		for ( int r = 0; r < 16; r++ )
			for ( int g = 0; g < 16; g++ )
				for ( int b = 0; b < 16; b++ )
				{
					int n = b * 256 + g * 16 + r;
					ts[n].rgb.r = ( r << 4 );
					ts[n].rgb.g = ( g << 4 );
					ts[n].rgb.b = ( b << 4 );
				}


		for ( i = 0; i < count; i++ )
		{
			RgbColorStruct rgb( col[i] );

			for ( int j = 0; j < CBCOUNT; j++ )
			{
				unsigned long d2 = rgb.Dist2( ts[j].rgb );

				if ( ts[j].lastDist2 > d2 )
				{
					ts[j].lastDist2 = d2;
					ts[j].lastCNum = i;
				}
			}
		}

		for ( i = 0; i < CBCOUNT; i++ ) { stColors[i] = ts[i].lastCNum; }
	}

	static unsigned CreatePseudoColor( unsigned bgr )
	{
		return stColors[ ( ( bgr >> 4 ) & 0xF ) + ( ( bgr >> 8 ) & 0xF0 ) + ( ( bgr >> 12 ) & 0xF00 ) ];
	}

	static void InitPseudoColor()
	{
		int n = visualInfo.colormap_size;

		if ( n > 4096 ) { n = 4096; }

		XColor xc[4096];
		unsigned bgr[4096];
		int i;

		for ( i = 0; i < n; i++ ) { xc[i].pixel = i; }

		XQueryColors( display, colorMap, xc, n );

		if ( visualInfo.c_class == PseudoColor || visualInfo.c_class == GrayScale )
		{
			//зафиксировать бля все цвета в палитре по умолчанию
			for ( i = 0; i < n; i++ )
			{
				int ret = XAllocColor( display, colorMap, &( xc[i] ) );
			}

		}

		int count = 0;

		for ( i = 0; i < n; i++ )
			if ( xc[i].flags & ( DoRed | DoBlue | DoGreen ) )
			{
				unsigned c = 0;

				if ( xc[i].flags & DoRed )
				{
					c |= ( ( xc[i].red >> 8 ) & 0xFF );
				}

				if ( xc[i].flags & DoGreen )
				{
					c |= ( ( xc[i].green >> 8 ) & 0xFF ) << 8;
				}

				if ( xc[i].flags & DoBlue )
				{
					c |= ( ( xc[i].blue >> 8 ) & 0xFF ) << 16;
				}

				bgr[count++] = c;
			}


		MakeStColors( bgr, count );
		CreateColor = CreatePseudoColor;
	}


//////////////////////////////// TrueColor

	static unsigned toRGB24( unsigned n ) { return ( ( n << 16 ) & 0xFF0000 ) | ( n & 0xFF00 ) | ( ( n >> 16 ) & 0xFF ); }
	static unsigned toNone( unsigned n ) { return n; }
	static int red_rs, red_ls;
	static int blue_rs, blue_ls;
	static int green_rs, green_ls;

	static unsigned toTrueColor( unsigned n )
	{
		return   ( ( ( n & 0x0000FF ) >> red_rs  ) << red_ls  ) |
		         ( ( ( n & 0x00FF00 ) >> green_rs ) << green_ls ) |
		         ( ( ( n & 0xFF0000 ) >> blue_rs ) << blue_ls );
	}

	static int CalcColorShifts( unsigned long mask, int* sh )
	{
		int i;

		for ( i = 0; i < int( sizeof( mask ) * 8 ) && ( mask & 1 ) == 0; i++, mask >>= 1 ) { EMPTY_OPER; }

		*sh = ( ( mask & 1 ) ? i : 0 );

		for ( i = 0; i < int( sizeof( mask ) * 8 ) && ( mask & 1 ); i++, mask >>= 1 ) { EMPTY_OPER; }

		return ( ( mask & 1 ) == 0 ) ? i : 0;
	}

	void InitTrueColor() //called if visual class is TrueColor
	{
		if (  visualInfo.red_mask == 0xFF0000 &&
		      visualInfo.green_mask == 0x00FF00 &&
		      visualInfo.blue_mask == 0x0000FF )
		{
			CreateColor = toRGB24;
		}
		else if (  visualInfo.red_mask == 0x0000FF &&
		           visualInfo.green_mask == 0x00FF00 &&
		           visualInfo.blue_mask == 0xFF0000 )
		{
			CreateColor = toNone;
		}
		else
		{
			int sh, d;
			d = CalcColorShifts( visualInfo.red_mask, &sh );
			red_rs = ( 8 - d ) + 0;
			red_ls = sh;
			d = CalcColorShifts( visualInfo.blue_mask, &sh );
			blue_rs = ( 8 - d ) + 16;
			blue_ls = sh;
			d = CalcColorShifts( visualInfo.green_mask, &sh );
			green_rs = ( 8 - d ) + 8;
			green_ls = sh;

			if ( red_rs < 0 ) { red_ls += -red_rs; red_rs = 0; }

			CreateColor = toTrueColor;
			//dbg_printf("(%i,%i) (%i,%i) (%i,%i) \n", red_rs, red_ls, blue_rs, blue_ls, green_rs, green_ls);

		}
	}


	static int xErrorCount = 0;
	static char xErrorLastText[0x100];

	static int ErrorHandler( Display* d, XErrorEvent* e )
	{

		XGetErrorText( d, e->error_code, xErrorLastText, sizeof( xErrorLastText ) );

		printf( "XError: %s\n", xErrorLastText );
		xErrorCount++;
		return 0;
	}


	void AppInit()
	{
		TimeInit();
		display = XOpenDisplay( 0 );

		if ( !display )
		{
			throw_msg( "can`t open X display (XOpenDisplay)" );
		}

		screen  = DefaultScreen( display );
		visual  = DefaultVisual( display, screen );
		colorMap   = DefaultColormap( display, screen );

		int nv = 0;
		XVisualInfo* vi = XGetVisualInfo( display, 0, &visualInfo, &nv );

		if ( !nv || !vi )
		{
			throw_msg( "no VisualInfo list" );
		}

		int i;

		for ( i = 0; i < nv; i++ )
			if ( vi[i].visual == visual )
			{
				break;
			}

		if ( i >= nv )
		{
			throw_msg( "default VisualInfo not found" );
		}

		visualInfo = vi[i];
		XFree( vi );

		connectionId = XConnectionNumber( display );
		whiteColor = WhitePixel( display, screen );
		blackColor = BlackPixel( display, screen );

		switch ( visualInfo.c_class )
		{
			case TrueColor:
				InitTrueColor();
				break;

			case GrayScale:      //!!!
			case StaticGray: ;      //!!!

			case StaticColor:
			case PseudoColor:
				InitPseudoColor();
				break;


			case DirectColor:
				throw_msg( "DirectColor visual class is not supported" );

			default:
				throw_msg( "Unknown visual class" );
		}

		setlocale( LC_ALL, "" );
		inputMethod = XOpenIM( display, 0, 0, 0 );

		if ( inputMethod )
		{
			XSetLocaleModifiers( "@im=local" );
			inputContext = XCreateIC( inputMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, DefaultRootWindow( display ),
			                          NULL ); //для борьбы с "missing sentinel in function call" надо написать NULL а не 0 (вот уроды)

			//0 );
			if ( !inputContext )
			{
				fprintf( stderr, "Can`t create input context (X)\n" );
			}

			if ( !XSupportsLocale() )
			{
				fprintf( stderr, "Locale not supported by X\n" );
			}
		}

		XSetErrorHandler( ErrorHandler );
		//Atoms
		atom_WM_DELETE_WINDOW = XInternAtom( display, "WM_DELETE_WINDOW", True );
		atom_WM_PROTOCOLS = XInternAtom( display, "WM_PROTOCOLS", True );


		X11_Clipbopard_Init();

		BaseInit();
	}

	struct RepaintHashNode
	{
		WINID id;
		RepaintHashNode ( WinID h ): id( h ) {}
		const WINID& key() const { return id; };
	private:
		RepaintHashNode (): id( 0 ) {}
	};

	void DrawExposeRect( Win* w )
	{
		if ( !w ) { return; }

		if ( !w->exposeRect.IsEmpty() )
		{
			GC gc( w );
			w->Paint( gc, w->exposeRect );
			w->exposeRect.Zero();
		}
	}


	static chash<RepaintHashNode, WINID> repaintHash;
	static void ForeachDrawExpose( RepaintHashNode* t, void* data )
	{
		DrawExposeRect( GetWinByID( t->id ) );
	}

	static void PostRepaint()
	{
		repaintHash.foreach( ForeachDrawExpose, 0 );
		repaintHash.clear();
	}

	static void AddRepaint( Win* w )
	{
		if ( !w ) { return; }

		RepaintHashNode node( w->GetID() );
		repaintHash.put( node );
	}

	static void MovePopups( Win* w, int xDelta, int yDelta )
	{
		if ( !w ) { return; }

		int n = w->ChildCount();

		for ( int i = 0; i < n; i++ )
		{
			Win* c = w->GetChild( i );

			if ( c->Type() == Win::WT_POPUP )
			{
				crect r = c->ScreenRect();
				r.left  += xDelta;
				r.right += xDelta;
				r.top += yDelta;
				r.bottom += yDelta;
				c->Move( r );
			}
			else if ( c->Type() != Win::WT_MAIN )
			{
				MovePopups( c, xDelta, yDelta );
			}
		}
	}


	static bool ChildKeyRecursive( Win* parent, Win* child, cevent_key* ev )
	{
		if ( !parent ) { return false; }

		if ( parent->Type() != Win::WT_MAIN && ChildKeyRecursive( parent->Parent(), child, ev ) ) { return true; }

		return parent->IsEnabled() && parent->EventChildKey( child, ev );
	}



	static void DoKeyEvent( int type, Win* w, KeySym ks, unsigned km,  unicode_t ch )
	{
//	printf( "type = %i ks = %x km = %u, ch = %u\n", type, (unsigned int)ks, km, ch );

		switch ( ks )
		{
			case XK_a:
				ks = XK_A;
				break;

			case XK_b:
				ks = XK_B;
				break;

			case XK_c:
				ks = XK_C;
				break;

			case XK_d:
				ks = XK_D;
				break;

			case XK_e:
				ks = XK_E;
				break;

			case XK_f:
				ks = XK_F;
				break;

			case XK_g:
				ks = XK_G;
				break;

			case XK_h:
				ks = XK_H;
				break;

			case XK_i:
				ks = XK_I;
				break;

			case XK_j:
				ks = XK_J;
				break;

			case XK_k:
				ks = XK_K;
				break;

			case XK_l:
				ks = XK_L;
				break;

			case XK_m:
				ks = XK_M;
				break;

			case XK_n:
				ks = XK_N;
				break;

			case XK_o:
				ks = XK_O;
				break;

			case XK_p:
				ks = XK_P;
				break;

			case XK_q:
				ks = XK_Q;
				break;

			case XK_r:
				ks = XK_R;
				break;

			case XK_s:
				ks = XK_S;
				break;

			case XK_t:
				ks = XK_T;
				break;

			case XK_u:
				ks = XK_U;
				break;

			case XK_v:
				ks = XK_V;
				break;

			case XK_w:
				ks = XK_W;
				break;

			case XK_x:
				ks = XK_X;
				break;

			case XK_y:
				ks = XK_Y;
				break;

			case XK_z:
				ks = XK_Z;
				break;
		}


		cevent_key ev( type, ks , km, 1, ch );

		if ( w->Type() != Win::WT_MAIN && ChildKeyRecursive( w->Parent(), w, &ev ) )
		{
			return;
		}

		if ( !w->IsEnabled() ) { return; }

		w->EventKey( &ev );
	}

	void KeyEvent( int type, XKeyEvent* event )
	{
		Win* w = GetWinByID( Win::focusWinId );

		if ( !w ) { return; }

		if ( w->Blocked() ) { return; }

		unsigned state = event->state;

		unsigned km = 0;
#if defined( __APPLE__ )
		if ( state & MetaMask ) { km |= KM_CTRL; }
#endif
		if ( state & ShiftMask ) { km |= KM_SHIFT; }

		if ( state & ControlMask ) { km |= KM_CTRL; }

		if ( state & Mod1Mask ) { km |= KM_ALT; }

		KeySym ks = 0;

		if ( inputMethod )
		{

			static wchar_t* buf = 0;
			static int bufLen = 0;

			if ( bufLen == 0 )
			{
				buf = ( wchar_t* ) malloc( 0x100 * sizeof( wchar_t ) );

				if ( buf ) { bufLen = 0x100; }
				else
				{
					fprintf( stderr, "out of memory\n" );
					exit( 1 );
				}
			}

			Status ret;
			int n = 0;

			while ( true )
			{
				n = XwcLookupString( inputContext, event, buf, 0x100, &ks, &ret );

				if ( ret != XBufferOverflow ) { break; }

				wchar_t* p = ( wchar_t* )malloc( bufLen * 2 * sizeof( wchar_t ) );

				if ( !p ) { return; }

				free( buf );
				buf = p;
				bufLen *= 2;
			}

			if ( ret !=  XLookupNone )
			{
				if ( n > 0 )
				{
					for ( int i = 0; i < n; i++ )
					{
						DoKeyEvent( type, w, ks, km, buf[i] );
					}
				}
				else
				{
					DoKeyEvent( type, w, ks, km, 0 );
				}

				return;
			}
		}


		char buf[256] = "";
		int n = XLookupString( event, buf, sizeof( buf ), &ks, NULL );

		if ( n > 0 )
		{
			for ( int i = 0; i < n; i++ )
			{
				DoKeyEvent( type, w, ks, km, buf[i] );
			}
		}
		else
		{
			DoKeyEvent( type, w, ks, km, 0 );
		}
	}

	inline int Distance( int a, int b )
	{
		return a > b ? a - b : b - a;
	}


	static void _ClipboardRequest( XEvent* event );

	int DoEvents( XEvent* event ) //return count of windows
	{
		dbg_printf( "e(%i) ", event->type );
		fflush( stdout );

		switch ( event->type )
		{

			case KeyPress:
				KeyEvent( EV_KEYDOWN, ( XKeyEvent* )event );
				break; //   2

			case KeyRelease:
				KeyEvent( EV_KEYUP, ( XKeyEvent* ) event );
				break; //   3

			case ButtonPress: //4
			case ButtonRelease:
				// 5
			{
				Win* w = GetWinByID( event->xany.window );

				if ( !w ) { break; }

				XButtonEvent* e = &event->xbutton;

				int type = ( ( event->type == ButtonPress ) ? EV_MOUSE_PRESS : EV_MOUSE_RELEASE );

				if ( !w->IsEnabled() ) { break; }

				if ( w->Blocked() ) { break; }

				int xPos = e->x;
				int yPos = e->y;

				unsigned km = 0;
				unsigned state = event->xbutton.state;
#if defined( __APPLE__ )
				if ( state & MetaMask ) { km |= KM_CTRL; }
#endif
				if ( state & ShiftMask ) { km |= KM_SHIFT; }

				if ( state & ControlMask ) { km |= KM_CTRL; }

				if ( state & Mod1Mask ) { km |= KM_ALT; }

				unsigned bf = 0;

//			printf( "Button = %i\n", e->button );

				unsigned button = 0;

				switch ( e->button )
				{
					case Button1:
						button = MB_L;
						break;

					case Button2:
						button = MB_M;
						break;

					case Button3:
						button = MB_R;
						break;

					case Button4:
						button = MB_X1;
						break;

					case Button5:
						button = MB_X2;
						break;
				}

				// mouse wheel up
				if ( e->button == Button4 && event->type == ButtonPress )
				{
					DoKeyEvent( EV_KEYDOWN, w, VK_UP, 0, 0 );
					DoKeyEvent( EV_KEYUP,   w, VK_UP, 0, 0 );
					break;
				}

				// mouse wheel down
				if ( e->button == Button5 && event->type == ButtonPress )
				{

					DoKeyEvent( EV_KEYDOWN, w, VK_DOWN, 0, 0 );
					DoKeyEvent( EV_KEYUP, w, VK_DOWN, 0, 0 );
					break;
				}

				if ( button == MB_L && event->type == ButtonPress )
				{
					static Time lastLeftClick = 0;
					static int lastX = 0, lastY = 0;
					static const Time doubleTime = 500;
					static const int maxXDistance = 5, maxYDistance = 5; //максимальное изменение координат при двойном нажатии

					if ( event->xbutton.time - lastLeftClick <= doubleTime &&
					     Distance( lastX, event->xbutton.x_root ) <= maxXDistance &&
					     Distance( lastY, event->xbutton.y_root ) <= maxYDistance )
					{
						type = EV_MOUSE_DOUBLE;
						lastLeftClick = 0;
					}
					else
					{
						lastLeftClick = event->xbutton.time;
						lastX = event->xbutton.x_root;
						lastY = event->xbutton.y_root;
					}
				}


				cevent_mouse evm( type, cpoint( xPos, yPos ), button, bf, km );


				w->Event( &evm );
			}
			break;

			case MotionNotify:
			{

				Win* w = GetWinByID( event->xany.window );

				if ( !w ) { break; }

				XMotionEvent* e = &event->xmotion;

				if ( !w->IsEnabled() ) { break; }

				if ( w->Blocked() ) { break; }

				cevent_mouse ev( EV_MOUSE_MOVE, cpoint( e->x, e->y ), 0, 0, 0 );
				w->Event( &ev );

			}

			break; //   6

			case EnterNotify:
			{
				Win* w = GetWinByID( event->xany.window );

				if ( !w ) { break; }

				cevent ev( EV_ENTER );
				w->Event( &ev );
				//dbg_printf("EnterNotify\n");
			}
			break; //   7

			case LeaveNotify:
			{
				Win* w = GetWinByID( event->xany.window );

				if ( !w ) { break; }

				cevent evl( EV_LEAVE );
				w->Event( &evl );
			}
			break; //   8


			case FocusIn:
			{
				if (  event->xfocus.detail == NotifyVirtual ||
				      event->xfocus.detail == NotifyNonlinearVirtual ||
				      event->xfocus.detail == NotifyPointer )
				{
					break;
				}

				Window id = event->xany.window;
				Win* w = GetWinByID( id );

				for ( ; w && w->type != Win::WT_MAIN && w->type != Win::WT_POPUP; w = w->parent )
				{
					EMPTY_OPER;
				}

				if ( w && w->handle != id )
				{
					id = w->handle;
				}

				if ( activeWinId == id )
				{
					break;
				}


				if ( Win::focusWinId )
				{
					Win* prevFocus = GetWinByID( Win::focusWinId );
					Win::focusWinId = 0;

					if ( prevFocus )
					{
						Win* w = prevFocus;

						for ( ; w && w->parent && w->type != Win::WT_MAIN && w->type != Win::WT_POPUP; w = w->parent )
						{
							w->parent->lastFocusChild = w->handle;
						}

						cevent ekf( EV_KILLFOCUS );
						prevFocus->Event( &ekf );

					}
				}

				Win* prevActive = GetWinByID( activeWinId );

				if ( activeWinId )
				{
					activeWinId = 0;

					if ( prevActive )
					{
						cevent_activate ev( false, w );
						activeWinId = 0;
						prevActive->Event( &ev );
					}
				}

				if ( !w ) { break; }

				if ( w->blockedBy )
				{
					Win* t = GetWinByID( w->blockedBy );

					if ( t )
					{
						if ( t->Type() == Win::WT_CHILD )
						{
							t->SetFocus();
						}
						else
						{
							t->Activate();
						}
					}

					break;
				}

				cevent_activate ev( true, prevActive );
				activeWinId = id;
				w->Event( &ev );
				Win::focusWinId = id;
				cevent ecf( EV_SETFOCUS );
				w->Event( &ecf );
			}

			break; //   9

			case FocusOut:
			{
				if (  event->xfocus.detail == NotifyVirtual ||
				      event->xfocus.detail == NotifyNonlinearVirtual ||
				      event->xfocus.detail == NotifyPointer )
				{
					break;
				}


				if ( activeWinId == event->xany.window )
				{
					Win* w = GetWinByID( activeWinId );
					activeWinId = 0;

					if ( !w ) { break; }

					if ( Win::focusWinId )
					{
						Win* t = GetWinByID( Win::focusWinId );
						Win::focusWinId = 0;

						if ( t )
						{
							Win* w = t;

							for ( ; w && w->parent && w->type != Win::WT_MAIN && w->type != Win::WT_POPUP; w = w->parent )
							{
								w->parent->lastFocusChild = w->handle;
							}

							cevent eKF( EV_KILLFOCUS );
							t->Event( &eKF );
						}
					}

					Window focus_return;
					int revert_to_return;
					XGetInputFocus( display, &focus_return, &revert_to_return );
					cevent_activate ev0( false, GetWinByID( focus_return ) );
					w->Event( &ev0 );
				}

			}

			break; //10

			case KeymapNotify:
				break; //   11

			case Expose:
			{
				Win* w = GetWinByID( event->xexpose.window );

				if ( !w ) { break; }

				crect r( event->xexpose.x, event->xexpose.y,
				         event->xexpose.x + event->xexpose.width, event->xexpose.y + event->xexpose.height );
				w->AddExposeRect( r );

				if ( !event->xexpose.count )
				{
					AddRepaint( w );
				}
			}
			break; //12

			case GraphicsExpose:
				break; //13

			case NoExpose:
				break; //14

			case VisibilityNotify:
				break; //15

			case CreateNotify:
				break; //16

			case DestroyNotify:
				break; //17

			case UnmapNotify:
			{
				Win* w = GetWinByID( event->xmap.window );

				if ( w )
				{
					cevent_show evShowFalse( false );
					w->Event( &evShowFalse );
				}

			}
			break; //18

			case MapNotify:
			{
				Win* w = GetWinByID( event->xmap.window );

				if ( w && ( w->Type() == Win::WT_MAIN || w->Type() == Win::WT_POPUP ) &&  w->showType == Win::SHOW_ACTIVE )
				{
					w->Activate();
				}

				if ( w )
				{
					cevent_show evShowTrue( true );
					w->Event( &evShowTrue );
				}

			}
			break; //19

			case MapRequest:
				dbg_printf( "MapRequest\n" );
				break; //20

			case ReparentNotify:
			{
				Win* w = GetWinByID( event->xreparent.window );

				if ( w ) { w->reparent = event->xreparent.parent; }
			}

			break; //   21

			case ConfigureNotify:
			{
				Win* w = GetWinByID( event->xconfigure.window );

				if ( !w ) { break; }

				bool resized = //true;
				   ( w->position.Width() != event->xconfigure.width ||
				     w->position.Height() != event->xconfigure.height );

				bool moved =
				   ( w->position.left != event->xconfigure.x ||
				     w->position.top != event->xconfigure.y );


				int xDelta = event->xconfigure.x - w->position.left;
				int yDelta = event->xconfigure.y - w->position.top;

				w->position.Set(
				   event->xconfigure.x,
				   event->xconfigure.y,
				   event->xconfigure.x + event->xconfigure.width,
				   event->xconfigure.y + event->xconfigure.height
				);

				if ( resized )
				{
					cevent_size evSize( cpoint ( event->xconfigure.width, event->xconfigure.height ) );
					w->Event( &evSize );
				}

				if ( moved )
				{
					cevent_move evMove( cpoint ( event->xconfigure.x, event->xconfigure.y ) );
					w->Event( &evMove );
				}

				if ( w->Type() != Win::WT_CHILD )
				{
					MovePopups( w, xDelta, yDelta );
				}

				dbg_printf( "ConfigureNotify above(%x)\n", event->xconfigure.above );
			}
			break; //   22

			case ConfigureRequest:
				break; //23

			case GravityNotify:
				break; //   24

			case ResizeRequest:
				break; //   25

			case CirculateNotify:
				break; //   26

			case CirculateRequest:
				break; //27

			case PropertyNotify:
				break; //   28

			case SelectionClear:
				break; //   29

			case SelectionRequest:

				_ClipboardRequest( event );

				break; //30

			case SelectionNotify:
				break; //   31

			case ColormapNotify:
				break; //   32

			case ClientMessage:
			{
				Win* w = GetWinByID( event->xclient.window );

				if ( w && event->xclient.message_type == atom_WM_PROTOCOLS && event->xclient.data.l[0] == atom_WM_DELETE_WINDOW )
				{
					cevent ev( EV_CLOSE );
					w->Event( &ev );
				}
			}
			break; //   33

			case MappingNotify:
				XRefreshKeyboardMapping( &( event->xmapping ) );
				dbg_printf( "MappingNotify\n" );
				break; //   34

				/*
				   case GenericEvent:
				      dbg_printf("GenericEvent\n");
				      break; //   35
				*/
			case LASTEvent:
				dbg_printf( "LASTEvent\n" );
				break; //36

			default:
				dbg_printf( "Event being thrown away\n" );
				break;
		}

		return 1; ///!!! need correction
	}

	static bool appExit = false;

	void AppExit() { appExit = true; }

	int AppRun()
	{
		appExit = false;

		while ( true )
		{
			wth_DoEvents();

			if ( appExit ) { return 0; }

			while ( XEventsQueued( display, QueuedAfterFlush ) > 0 )
			{
				XEvent event;
				XNextEvent( display, &event );
				wth_DoEvents();

				if ( !DoEvents( &event ) ) { return 0; }

				if ( appExit ) { return 0; }
			}

			unsigned w = RunTimers();
			PostRepaint();

			if ( XEventsQueued( display, QueuedAfterFlush ) > 0 )
			{
				continue;
			}

			fd_set fds;
			FD_ZERO( &fds );
			FD_SET( connectionId, &fds );
			int tSignalFd = wthInternalEvent.SignalFD();
			FD_SET( tSignalFd, &fds );

			struct timeval tv;
			tv.tv_sec = w / 1000;
			tv.tv_usec = ( w % 1000 ) * 1000;

			int n = connectionId > tSignalFd ? connectionId : tSignalFd;

			select( n + 1, &fds,  0, 0, &tv );
		}
	}


///////////////////////////////////////////////    ф-ции clipboard-а {

	/*
	   циклы обработки сообщений _Clipboard... игноригуют все сообщения от кнопок и мыши
	   в этих циклах не приходят сообщения таймеров и потоков !!!

	   это для clipboard-а
	   криво, но хоть так
	*/

	static int _ClipboardWaitSelectionNotify( XEvent* pEvent, bool stopOnExcape, int maxWaitSeconds = 10 ) // 0 - ok
	{
		time_t startTime = time( 0 );

		xErrorCount = 0;

		while ( true )
		{
			while ( XEventsQueued( display, QueuedAfterFlush ) > 0 )
			{
				XNextEvent( display, pEvent );

				if ( pEvent->type == SelectionNotify ) { return 0; }

				if ( pEvent->type == KeyPress || pEvent->type == KeyRelease )
				{
					if ( stopOnExcape && pEvent->xkey.keycode == 27 ) { return -3; }

					continue;
				}

				if ( pEvent->type == ButtonPress || pEvent->type == ButtonRelease )
				{
					continue;
				}

				if ( !DoEvents( pEvent ) ) { return -1; }

			}

			if ( xErrorCount > 0 ) { return -1; }

			PostRepaint();

			if ( XEventsQueued( display, QueuedAfterFlush ) > 0 )
			{
				continue;
			}

			fd_set fds;
			FD_ZERO( &fds );
			FD_SET( connectionId, &fds );

			struct timeval tv;
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			int n = connectionId;

			if ( time( 0 ) - startTime >= maxWaitSeconds ) { return -1; }

			select( n + 1, &fds,  0, 0, &tv );
		}
	}

	static int _ClipboardWaitPropertyNotify( XEvent* pEvent, Window id, Atom prop, bool stopOnExcape, int maxWaitSeconds = 10 ) // 0 - ok
	{
		time_t startTime = time( 0 );

		while ( true )
		{
			while ( XEventsQueued( display, QueuedAfterFlush ) > 0 )
			{
				XNextEvent( display, pEvent );

				if ( pEvent->type == PropertyNotify )
				{
					if ( pEvent->xproperty.window == id && pEvent->xproperty.atom == prop && pEvent->xproperty.state == PropertyNewValue ) { return 0; }
				}

				if ( pEvent->type == KeyPress || pEvent->type == KeyRelease )
				{
					if ( stopOnExcape && pEvent->xkey.keycode == 27 ) { return -3; }

					continue;
				}

				if ( pEvent->type == ButtonPress || pEvent->type == ButtonRelease )
				{
					continue;
				}

				if ( !DoEvents( pEvent ) ) { return -1; }

			}

			PostRepaint();

			if ( XEventsQueued( display, QueuedAfterFlush ) > 0 )
			{
				continue;
			}

			fd_set fds;
			FD_ZERO( &fds );
			FD_SET( connectionId, &fds );

			struct timeval tv;
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			int n = connectionId;

			if ( time( 0 ) - startTime >= maxWaitSeconds ) { return -1; }

			select( n + 1, &fds,  0, 0, &tv );
		}
	}


	class CharQueue
	{
		enum { BS = 1024 };

		struct Node
		{
			char data[BS];
			Node* next;
			Node(): next( 0 ) {}
		};

		int firstPos;
		int lastCount;
		int count;
		Node* first, *last;

		int PreparePut() { if ( !last ) { first = last = new Node; lastCount = 0; } else if ( lastCount >= BS ) { last->next = new Node; last = last->next; lastCount = 0; } return BS - lastCount; }
		void CheckFirst() { if ( firstPos >= BS ) { Node* p = first; first = first->next; delete p; firstPos = 0; if ( !first ) { last = 0; } }; }
	public:
		CharQueue(): firstPos( 0 ), lastCount( 0 ), count( 0 ), first( 0 ), last( 0 ) {}
		void Clear() { while ( first ) { Node* p = first; first = first->next; delete p; } last = 0; count = 0; }
		int Count() const { return count; }
		void Put( int c ) { PreparePut(); last->data[lastCount++] = c; count++; }
		void Put( const char* s, int size ) { while ( size > 0 ) { int n = PreparePut(); if ( n > size ) { n = size; } memcpy( last->data + lastCount, s, n * sizeof( char ) ); lastCount += n; size -= n; s += n; count += n; } }
		int Get() { if ( count <= 0 ) { return 0; } int c = first->data[firstPos++]; count--; CheckFirst(); return c; }

		int Get( char* s, int size )
		{
			if ( size > count ) { size = count; }

			if ( size <= 0 ) { return 0; }

			int ret = size;

			while ( size > 0 )
			{
				int n = BS - firstPos;

				if ( n > size ) { n = size; }

				memcpy( s, first->data + firstPos, n );
				size -= n;
				firstPos += n;
				count -= n;
				s += n;
				CheckFirst();
			}

			return ret;
		}

		std::vector<char> GetArray() { std::vector<char> p; if ( count > 0 ) { p.resize( count ); Get( p.data(), count ); } return p; }
		std::vector<char> GetArray( int* pCount ) { int n = count; std::vector<char> p; if ( count > 0 ) { p.resize( count ); Get( p.data(), count ); } if ( pCount ) { *pCount = n; } return p; }

		~CharQueue() { Clear(); }
	};


	static   void CharQueueToClipboartAsUtf8( ClipboardText* text, CharQueue* q )
	{
		int size;
		std::vector<char> p = q->GetArray( &size );

		char* s = p.data();

		while ( size > 0 )
		{
			if ( *s & 0x80 )
			{
				unicode_t t;
				int n;

				if ( ( *s & 0xE0 ) == 0xC0 )
				{
					t = ( *s & 0x3F );
					n = 1;
				}
				else if ( ( *s & 0xF0 ) == 0xE0 )
				{
					t = ( *s & 0x1F );
					n = 2;
				}
				else if ( ( *s &  0xF8 ) == 0xF0 )
				{
					t = ( *s & 0x07 );
					n = 3;
				}
				else
				{
					text->Append( ( *( s++ ) & 0x7F ) );
					size--;
					continue;
				}

				s++;
				size--;

				for ( ; size > 0 && ( *s & 0xC0 ) == 0x80; size--, n--, s++ )
				{
					t = ( t << 6 ) + ( *s & 0x3F );
				}

				text->Append( t );
			}
			else
			{
				text->Append( *( s++ ) );
				size--;
			}
		}
	}

	static   void ClipboardToCharQueueAsUtf8( CharQueue* q, ClipboardText* text )
	{
		int n = text->Count();

		for ( int i = 0; i < n; i++ )
		{
			unsigned c = text->Get( i );

			if ( c < 0x800 )
			{
				if ( c < 0x80 )
				{
					q->Put( c );
				}
				else
				{
					q->Put( ( 0xC0 | ( c >> 6 ) ) );
					q->Put( 0x80 | ( c & 0x3F ) );
				}
			}
			else
			{
				char s[4];

				if ( c < 0x10000 ) //1110xxxx 10xxxxxx 10xxxxxx
				{
					s[2] = 0x80 | c & 0x3F;
					c >>= 6;
					s[1] = 0x80 | c & 0x3F;
					c >>= 6;
					s[0] = ( c & 0x0F ) | 0xE0;
					q->Put( s, 3 );
				}
				else     //11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
				{
					s[3] = 0x80 | c & 0x3F;
					c >>= 6;
					s[2] = 0x80 | c & 0x3F;
					c >>= 6;
					s[1] = 0x80 | c & 0x3F;
					c >>= 6;
					s[0] = ( c & 0x7 ) | 0xF0;
					q->Put( s, 4 );
				}
			}

		}
	}


	static   void ClipboardToCharQueueAsLatin1( CharQueue* q, ClipboardText* text )
	{
		int n = text->Count();

		for ( int i = 0; i < n; i++ )
		{
			unsigned c = text->Get( i );
			q->Put( c >= 0x100 ? '?' : char( c ) );
		}
	}


// return >=0 - ok
// return <0 - error

	static int GetWinPropSize( Window winId, Atom prop, Atom* pRetType ) //в байтах
	{
		int retFormat;
		unsigned long retNItems;
		unsigned long retBytesAfter;
		unsigned char* propDataPtr = 0;

		int ret = XGetWindowProperty( display, winId, prop,
		                              0,
		                              0,
		                              False, AnyPropertyType,
		                              pRetType, &retFormat, &retNItems, &retBytesAfter, &propDataPtr );

		if ( ret != Success ) { return -1; }

		if ( propDataPtr ) { XFree( propDataPtr ); }

		return retBytesAfter;
	}

	static bool ReadCharProp( Window winId, Atom prop, CharQueue* buf )
	{
		Atom retType;
		int ret;

		int size = GetWinPropSize( winId, prop, &retType );

		if ( size < 0 ) { return false; }

		int retFormat;
		unsigned long retNItems;
		unsigned long retBytesAfter;
		unsigned char* propDataPtr = 0;

		if ( retType != atom_INCR )
		{
			ret = XGetWindowProperty( display, winId, prop,
			                          0,
			                          size, //???
			                          False, AnyPropertyType,
			                          &retType, &retFormat, &retNItems, &retBytesAfter, &propDataPtr );

			if ( ret != Success ) { return false; }

			switch ( retFormat )
			{
				case 8:
					size = retNItems;
					break;

				case 16:
					size = retNItems * 2;
					break;

				case 32:
					size = retNItems * 4;
					break;

				default:
					size = -1;
			}

			if ( !propDataPtr ) { size = -1; }

			if ( size > 0 )
			{
				buf->Put( ( char* )propDataPtr, size );
			}

			if ( propDataPtr ) { XFree( propDataPtr ); propDataPtr = 0; }

			XDeleteProperty( display, winId, prop );

			return size >= 0;
		}

		/// INCR mode
		//


		XDeleteProperty( display, winId, prop );

		size = 0;

		while ( true )
		{
			XEvent event;

			if ( _ClipboardWaitPropertyNotify( &event, winId, prop, true ) ) { return false; }

			size = GetWinPropSize( winId, prop, &retType );

			if ( size <= 0 ) { break; }

			ret = XGetWindowProperty( display, winId, prop,
			                          0,
			                          size, //???
			                          False, AnyPropertyType,
			                          &retType, &retFormat, &retNItems, &retBytesAfter, &propDataPtr );

			if ( ret != Success ) { return false; }

			switch ( retFormat )
			{
				case 8:
					size = retNItems;
					break;

				case 16:
					size = retNItems * 2;
					break;

				case 32:
					size = retNItems * 4;
					break;

				default:
					size = -1;
			}

			if ( !propDataPtr ) { size = -1; }

			if ( size > 0 )
			{
				buf->Put( ( char* )propDataPtr, size );
			}

			if ( propDataPtr ) { XFree( propDataPtr ); propDataPtr = 0; }

			XDeleteProperty( display, winId, prop );
		}

		return size == 0;
	}


	static void RunGetClipboard( Win* w, ClipboardText* text )
	{
		Atom cbAtom = atom_CLIPBOARD;
		Window selOwner = XGetSelectionOwner( display, cbAtom );


		if ( selOwner == None )
		{
			cbAtom = atom_PRIMARY;
			selOwner = XGetSelectionOwner( display, cbAtom );

			if ( selOwner == None ) { return; }
		}


		if ( selOwner == clipboardWinId )
		{
//printf("selOwner == clipboardWinId\n");
			*text = clipboardText;
			return;
		}

		XConvertSelection( display, cbAtom, atom_UTF8,  atom_CDEST, clipboardWinId, CurrentTime );

		XEvent event;

		if ( _ClipboardWaitSelectionNotify( &event, true ) ) { return; }

		if ( event.xselection.requestor != clipboardWinId ||
		     event.xselection.selection !=  cbAtom )
		{
			return;
		}

		if ( event.xselection.target != atom_UTF8 ) { return; }

		if ( event.xselection.property == None ) { return; }

		if ( event.xselection.property != atom_CDEST ) { return; }

		CharQueue cb;

		if ( !ReadCharProp( clipboardWinId, atom_CDEST, &cb ) ) { return; }

		CharQueueToClipboartAsUtf8( text, &cb );
	}

	void ClipboardGetText( Win* w, ClipboardText* text )
	{
		static bool isRun = false; //чтоб рекурсивно не вызывалось

		if ( isRun || !text || !w )
		{
			return;
		}

		isRun = true;

		try
		{
			RunGetClipboard( w, text );
		}
		catch ( ... )
		{
			isRun = false;
			throw;
		}

		isRun = false;
	}


	static void _ClipboardRequest( XEvent* event )
	{
		CharQueue q;

		XEvent reply;
		reply.type = SelectionNotify;
		reply.xselection.display = display;
		reply.xselection.selection = event->xselectionrequest.selection;
		reply.xselection.target = event->xselectionrequest.target;
		reply.xselection.requestor = event->xselectionrequest.requestor;
		reply.xselection.property = event->xselectionrequest.property;
		reply.xselection.time = event->xselectionrequest.time;

		/*
		   !!!протестировать нормально и доделать

		   непонятно надо ли делать INCR mode

		   обязательные targets ???
		   TARGETS
		   MULTIPLE
		   TIMESTAMP
		*/

		if ( event->type != SelectionRequest ) { return; }

		if ( event->xselectionrequest.selection != atom_PRIMARY && event->xselectionrequest.selection != atom_CLIPBOARD )
		{
			goto Nah;
		}


		if ( event->xselectionrequest.target == atom_TARGETS )
		{
			Atom possibleTargets[] =
			{
				atom_STRING,
				atom_UTF8,
				atom_COMPOUND
			};

			XChangeProperty( display,
			                 event->xselectionrequest.requestor,
			                 event->xselectionrequest.property,
			                 event->xselectionrequest.target,
			                 32, PropModeReplace,
			                 ( unsigned char* ) possibleTargets,
			                 2 ); //!!!

			XSendEvent( display, event->xselectionrequest.requestor, True, 0, &reply );
			return;
		}


		if ( event->xselectionrequest.target == atom_UTF8 ) { ClipboardToCharQueueAsUtf8( &q, &clipboardText ); }
		else if ( event->xselectionrequest.target == atom_STRING ) { ClipboardToCharQueueAsLatin1( &q, &clipboardText ); }
		else
		{
//printf("!!!CLIPBOARD UNKNOWN TARGET (%i), (%s)\n", int(event->xselectionrequest.target), XGetAtomName(display, event->xselectionrequest.target));
			goto Nah;
		}

		if ( q.Count() <= 0 ) { goto Nah; }

		{
			int size;
			std::vector<char> p = q.GetArray( &size );

			XChangeProperty( display,
			                 event->xselectionrequest.requestor,
			                 event->xselectionrequest.property,
			                 event->xselectionrequest.target,
			                 8,
			                 PropModeReplace,
			                 ( unsigned char* )p.data(),
			                 size );

			XSendEvent( display, event->xselectionrequest.requestor, True, 0, &reply );
			return;
		}

Nah:
		reply.xselection.property = None;
		XSendEvent( display, event->xselectionrequest.requestor, True, 0, &reply );
	}


/////////////////////////////////////////// } (clipboard)




//////////////////////////////////////   GC

	XFontStruct* GC::defaultFontStruct = 0;

	void GC::_Init( Drawable id )
	{
		curFont = 0;
		valueMask = 0;
		lineX = 0;
		lineY = 0;
		winId = id;
		lineRgb = 0;
		lineColor = blackColor;
		textRgb = 0;
		textColor = blackColor;
		fillRgb = 0xFFFFFFFF;
		fillColor = whiteColor;

		//defaults
		gcValues.foreground = 0;
		gcValues.background = 1;
		gcValues.line_width = 0;
		gcValues.line_style = LineSolid;
		gcValues.cap_style =  CapProjecting;  //CapButt;
		gcValues.join_style = JoinMiter;
		gcValues.fill_style = FillSolid;
		gcValues.fill_rule = EvenOddRule;
		gcValues.arc_mode = ArcPieSlice;
		gcValues.ts_x_origin = 0;
		gcValues.ts_y_origin = 0;
		gcValues.font = 0;
		gcValues.subwindow_mode = ClipByChildren;
		gcValues.graphics_exposures = True;
		gcValues.clip_x_origin = 0;
		gcValues.clip_y_origin = 0;
		gcValues.clip_mask = None;
		gcValues.dash_offset =  0;

		gc = XCreateGC( display, winId, GCCapStyle , &gcValues );

		if ( !defaultFontStruct )
		{
			defaultFontStruct = XQueryFont( display, XGContextFromGC( gc ) );
		}

		fontAscent = ( defaultFontStruct ) ? defaultFontStruct->ascent : 0;
	}


	GC::GC( Win* win )
		: gc( None )
	{
		_Init( win ? win->GetID() : DefaultRootWindow( display ) );
	}

	GC::GC( SCImage* im )
		: gc( None )
	{
		_Init( im->GetXDrawable() );
	}

	GC::GC( Drawable id )
		: gc( None )
	{
		_Init( id );
	}


	/*
	GC::GC(Win *win)
	:  valueMask(0),
	   lineX(0), lineY(0),
	   winId(win?win->GetID():DefaultRootWindow(display)),
	   lineRgb(0), lineColor(blackColor),
	   textRgb(0), textColor(blackColor),
	   fillRgb(0xFFFFFFFF), fillColor(whiteColor)
	{
	   //defaults
	   gcValues.foreground=0;
	   gcValues.background=1;
	   gcValues.line_width=0;
	   gcValues.line_style=LineSolid;
	   gcValues.cap_style =  CapProjecting;  //CapButt;
	   gcValues.join_style=JoinMiter;
	   gcValues.fill_style=FillSolid;
	   gcValues.fill_rule=EvenOddRule;
	   gcValues.arc_mode=ArcPieSlice;
	   gcValues.ts_x_origin=0;
	   gcValues.ts_y_origin=0;
	   gcValues.font=0;
	   gcValues.subwindow_mode = ClipByChildren;
	   gcValues.graphics_exposures = True;
	   gcValues.clip_x_origin= 0;
	   gcValues.clip_y_origin= 0;
	   gcValues.clip_mask=None;
	   gcValues.dash_offset =  0;

	   gc = XCreateGC(display, winId, GCCapStyle , &gcValues);

	   if (!defaultFontStruct)
	      defaultFontStruct = XQueryFont(display, XGContextFromGC(gc));

	   fontAscent = (defaultFontStruct)? defaultFontStruct->ascent : 0;
	}
	*/

	inline void GC::SetFg( unsigned c )
	{
		if ( gcValues.foreground != c )
		{
			valueMask |= GCForeground;
			gcValues.foreground = c;
		}
	}

	inline void GC::SetBg( unsigned c )
	{
		if ( gcValues.background != c )
		{
			valueMask |= GCBackground;
			gcValues.background = c;
		}
	}

	void GC::SetFillColor( unsigned rgb )
	{
		if ( fillRgb == rgb ) { return; }

		fillRgb = rgb;
		fillColor = CreateColorFromRGB( rgb );
	}

	void GC::SetLine( unsigned rgb, int width, int style )
	{
		if ( width == 1 ) { width = 0; }

		int s = ( style == DOT ) ? LineOnOffDash : LineSolid;

		if ( lineRgb == rgb && width  == gcValues.line_width && s == gcValues.line_style ) { return; }

		gcValues.line_width = width;
		gcValues.line_style = s;
		valueMask |= GCLineWidth | GCLineStyle;
		lineRgb = rgb;
		lineColor = CreateColorFromRGB( rgb );
	}


	void GC::Set( cfont* font )
	{
		curFont = 0;

		if ( !font ) { return; }

		if ( font->type == cfont::TYPE_X11 )
		{
			XFontStruct* fs  = ( XFontStruct* )font->data;

			if ( fs )
			{
				XSetFont( display, gc, fs->fid );
				fontAscent = fs->ascent;
			}

			return;
		}

		if ( font->type == cfont::TYPE_FT )
		{
			curFont = font;
			return;
		}
	}

	inline void GC::CheckValues()
	{
		if ( valueMask )
		{
			XChangeGC( display, gc, valueMask, &gcValues );
			valueMask = 0;
		}
	}

	void GC::FillRect( crect r )
	{
		if ( r.Width() <= 0 || r.Height() <= 0 ) { return; }

		SetFg( fillColor );
		CheckValues();
		XFillRectangle( display, winId, gc, r.left, r.top, r.Width(), r.Height() );
	}

	void GC::FillRectXor( crect r )
	{
		if ( r.Width() <= 0 || r.Height() <= 0 ) { return; }

		SetFg( fillColor );
		CheckValues();
		XSetFunction( display, gc, GXxor );
		XFillRectangle( display, winId, gc, r.left, r.top, r.Width(), r.Height() );
		XSetFunction( display, gc, GXcopy );
	}

	void GC::SetTextColor( unsigned rgb )
	{
		if ( textRgb == rgb ) { return; }

		textRgb = rgb;
		textColor = CreateColorFromRGB( rgb );

	}

	void GC::SetClipRgn( crect* r )
	{
		if ( r )
		{
			XRectangle xr;
			xr.x = r->left;
			xr.y = r->top;
			xr.width = r->Width();
			xr.height = r->Height();

			XSetClipRectangles( display, gc, 0, 0, &xr, 1, Unsorted );
		}
		else
		{
			XSetClipMask( display, gc, None );
		}
	}

	void GC::DrawIcon( int x, int y, cicon* ico )
	{
		if ( ico ) { ico->Draw( *this, x, y ); }
	}

	void GC::DrawIconF( int x, int y, cicon* ico )
	{
		if ( ico ) { ico->DrawF( *this, x, y ); }
	}


	void GC::TextOut( int x, int y, const unicode_t* s, int charCount )
	{
		if ( charCount < 0 )
		{
			charCount = unicode_strlen( s );
		}

		if ( charCount <= 0 ) { return; }

#ifdef USEFREETYPE

		if ( curFont && curFont->type == cfont::TYPE_FT )
		{
			( ( FTU::FFace* )curFont->data )->OutText( *this, x, y, s, charCount );
			return;
		}

#endif

		SetFg( textColor );
		CheckValues();
		std::vector<XChar2b> sc( charCount );

		for ( int i = 0; i < charCount; i++ )
		{
			unicode_t c = s[i];
			sc[i].byte2 = ( c & 0xFF );
			sc[i].byte1 = ( ( c >> 8 ) & 0xFF );
		}

		XDrawString16( display, winId, gc, x, y + fontAscent, sc.data(), charCount );
	}

	void GC::TextOutF( int x, int y, const unicode_t* s, int charCount )
	{
		if ( charCount < 0 )
		{
			charCount = unicode_strlen( s );
		}

		if ( charCount <= 0 ) { return; }

#ifdef USEFREETYPE

		if ( curFont && curFont->type == cfont::TYPE_FT )
		{
			( ( FTU::FFace* )curFont->data )->OutTextF( *this, x, y, s, charCount );
			return;
		}

#endif
		SetFg( textColor );
		SetBg( fillColor );
		CheckValues();
		std::vector<XChar2b> sc( charCount );

		for ( int i = 0; i < charCount; i++ )
		{
			unicode_t c = s[i];
			sc[i].byte2 = ( c & 0xFF );
			sc[i].byte1 = ( ( c >> 8 ) & 0xFF );
		}

		XDrawImageString16( display, winId, gc, x, y + fontAscent, sc.data(), charCount );
	}


	void GC::MoveTo( int x, int y )
	{
		lineX = x;
		lineY = y;
	}

	void GC::LineTo( int x, int y )
	{
		SetFg( lineColor );
		CheckValues();
		XDrawLine( display, winId, gc, lineX, lineY, x, y );
		lineX = x;
		lineY = y;
	}

	void GC::SetPixel( int x, int y, unsigned rgb )
	{
		valueMask |= GCForeground;
		gcValues.foreground = CreateColorFromRGB( rgb );
		CheckValues();
		XDrawPoint( display, winId, gc, x, y );
	}

	cpoint GC::GetTextExtents( const unicode_t* s, int charCount )
	{
		if ( !s ) { return cpoint( 0, 0 ); }

		if ( charCount < 0 )
		{
			charCount = unicode_strlen( s );
		}

#ifdef USEFREETYPE

		if ( curFont && curFont->type == cfont::TYPE_FT )
		{
			return ( ( FTU::FFace* )curFont->data )->GetTextExtents( s, charCount );
		}

#endif

		int direction;
		int ascent;
		int descent;
		XCharStruct ret;

		if ( charCount == 0 )
		{
			XChar2b xc;
			xc.byte2 = ' ';
			xc.byte1 = 0;
			XQueryTextExtents16( display, XGContextFromGC( gc ), &xc, 1, &direction, &ascent, &descent, &ret );
			ret.width = 0;
		}
		else
		{
			std::vector<XChar2b> sc( charCount );

			for ( int i = 0; i < charCount; i++ )
			{
				unicode_t c = s[i];
				sc[i].byte2 = ( c & 0xFF );
				sc[i].byte1 = ( ( c >> 8 ) & 0xFF );
			}

			XQueryTextExtents16( display, XGContextFromGC( gc ), sc.data(), charCount, &direction, &ascent, &descent, &ret );
		}

		return cpoint( ret.width, ascent + descent );
	}

	void GC::Ellipce( crect r )
	{
		SetFg( lineColor );
		CheckValues();
		int w = r.Width() - 1;
		int h = r.Height() - 1;

		if ( w < 0 ) { w = 1; }

		if ( h < 0 ) { h = 1; }

		XDrawArc( display, winId, gc, r.left, r.top, w, h, 0, 360 * 64 );
	}

	GC::~GC() { XFreeGC( display, gc ); }


//////////////////////////// Win



	Win::Win( WTYPE t, unsigned hints, Win* _parent, const crect* rect, int uiNId )
		:
		parent( _parent ),
		type( t ),
		blockedBy( 0 ),
		modal( 0 ),
		whint( hints ),
		state( 0 ),

		captured( false ),
		lastFocusChild( 0 ),
		upLayout( 0 ),
		layout( 0 ),
		uiNameId( uiNId ),

		exposeRect( 0, 0, 0, 0 ),
		reparent( 0 )
	{
		crect r = ( rect ? *rect : crect( 0, 0, 1, 1 ) );

		if ( r.top >= r.bottom ) { r.bottom = r.top + 1; }

		if ( r.left >= r.right ) { r.right = r.left + 1; }

		position = r;


		XSetWindowAttributes attrs;

		attrs.event_mask =
		   KeyPressMask      |  //Keyboard down events
		   KeyReleaseMask       |  //Keyboard up events
		   ButtonPressMask   |  //Pointer button down events
		   ButtonReleaseMask    |  //Pointer button up events
		   EnterWindowMask   |  //Pointer window entry events
		   LeaveWindowMask   |  //Pointer window leave events
		   PointerMotionMask    |  //All pointer motion events
//		PointerMotionHintMask  |  //Fewer pointer motion events
		   Button1MotionMask    |  //Pointer motion while button 1 down
		   Button2MotionMask    |  //Pointer motion while button 2 down
		   Button3MotionMask    |  //Pointer motion while button 3 down
		   Button4MotionMask    |  //Pointer motion while button 4 down
		   Button5MotionMask    |  //Pointer motion while button 5 down
		   ButtonMotionMask  |  //Pointer motion while any button down
		   KeymapStateMask   |  //Any keyboard state change on EnterNotify , LeaveNotify , FocusIn or FocusOut
		   ExposureMask      |  //Any exposure (except GraphicsExpose and NoExpose )
		   VisibilityChangeMask    |  //Any change in visibility
		   StructureNotifyMask  |  //Any change in window configuration.
//		ResizeRedirectMask  |  //Redirect resize of this window
//		SubstructureNotifyMask    |  //Notify about reconfiguration of children
//		SubstructureRedirectMask| //Redirect reconfiguration of children
		   FocusChangeMask   |  //Any change in keyboard focus
		   PropertyChangeMask   |  //Any change in property
		   ColormapChangeMask   |  //Any change in colormap
		   OwnerGrabButtonMask;       //Modifies handling of pointer events


		Window xparent =
		   ( !parent || type == WT_MAIN || type == WT_POPUP ) ?
		   DefaultRootWindow( display ) : parent->GetID();


		unsigned long valueMask = CWEventMask;

		valueMask |= CWBorderPixmap;
		attrs.border_pixmap = None;

		valueMask |= CWBackPixmap;
		attrs.background_pixmap = None;

		if ( type == WT_POPUP )
		{
			valueMask |= CWOverrideRedirect;
			attrs.override_redirect = True;
		}

		handle = XCreateWindow(
		            display,
		            xparent,
		            r.left, r.top, r.Width(), r.Height(),
		            0,
		            CopyFromParent,
		            InputOutput,
		            visual,
		            valueMask, &attrs );

		AddWinToHash( handle, this );

		if ( parent )
		{
			parent->childList.append( this );

			if ( type == WT_MAIN || type == WT_POPUP )
			{
				XSetTransientForHint( display, handle, parent->GetID() );
			}

		}

		if ( rect && !rect->IsEmpty() )
		{
			LSize ls;
			ls.Set( *rect );
			SetLSize( ls );
		}


		if ( type == WT_MAIN && atom_WM_DELETE_WINDOW != None )
		{
			XSetWMProtocols( display, handle, &atom_WM_DELETE_WINDOW, 1 );
		}

		if ( type == WT_MAIN )
		{
			XSizeHints hints;
			hints.flags = PMinSize;
			hints.min_width = 10;
			hints.min_height = 10;
			XSetWMNormalHints( display, handle, &hints );

			/*
			      if (winIcon.GetXDrawable() != None)
			      {
			         XWMHints wm_hints;
			         wm_hints.icon_pixmap = winIcon.GetXDrawable();
			         wm_hints.flags = IconPixmapHint ;
			         XSetWMProperties(display, handle, 0, 0, 0, 0, 0, &wm_hints, 0);
			      }
			*/
			if ( winIconPixmap != None && winIconMask != None )
			{
				XWMHints wm_hints;
				wm_hints.icon_pixmap = winIconPixmap;
				wm_hints.icon_mask = winIconMask;
				wm_hints.flags = IconPixmapHint | IconMaskHint;
				XSetWMHints( display, handle, &wm_hints );
			}
		};
	}



	int Win::DoModal()
	{
		bool visibled = IsVisible();
		bool enabled = IsEnabled();
		WinID lastParentFC = parent ? parent->lastFocusChild : 0;

		try
		{
			WinID oldActive = activeWinId;

			if ( !visibled ) { Show(); }

			if ( !enabled ) { Enable( true ); }

			AppBlock( GetID() );
			UnblockTree( GetID() );

			ModalStruct modalStruct;
			modal = &modalStruct;

			Activate();
			SetFocus();

			while ( !modalStruct.end )
			{
				wth_DoEvents();

				if ( modalStruct.end ) { break; }

				while ( XEventsQueued( display, QueuedAfterFlush ) > 0 )
				{
					XEvent event;
					XNextEvent( display, &event );
					wth_DoEvents();

					if ( !DoEvents( &event ) ) { return 0; }

					if ( modalStruct.end )
					{
						goto stopped;
					}
				}

				unsigned w = RunTimers();
				PostRepaint();

				if ( XEventsQueued( display, QueuedAfterFlush ) > 0 )
				{
					continue;
				}

				fd_set fds;
				FD_ZERO( &fds );
				FD_SET( connectionId, &fds );
				int tSignalFd = wthInternalEvent.SignalFD();
				FD_SET( tSignalFd, &fds );

				struct timeval tv;
				tv.tv_sec = w / 1000;
				tv.tv_usec = ( w % 1000 ) * 1000;

				int n = connectionId > tSignalFd ? connectionId : tSignalFd;

				select( n + 1, &fds,  0, 0, &tv );
			}

stopped:
			modal = 0;
			AppUnblock( GetID() );

			if ( !visibled ) { Hide(); }

			if ( type == WT_CHILD && parent )
			{
				Win* w = GetWinByID( lastParentFC );

				if ( w ) { w->SetFocus(); }
			}

			if ( oldActive )
			{
				Win* w = GetWinByID( oldActive );

				if ( w ) { w->Activate(); } //w->SetFocus();
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
		int w = rect.Width();
		int h = rect.Height();

		if ( w <= 0 ) { w = 1; }

		if ( h <= 0 ) { h = 1; }

		XMoveResizeWindow( display, GetID() , rect.left, rect.top, w, h );

		if ( repaint ) { Invalidate(); }
	}

	void Win::SetCapture()
	{
		if ( !captured )
		{
			if ( ::XGrabPointer( display, handle, False,
			                     ButtonPressMask   |
			                     ButtonReleaseMask    |
			                     PointerMotionMask,
			                     GrabModeAsync, GrabModeAsync,
			                     None, None, CurrentTime ) == GrabSuccess )
			{
				//dbg_printf("Captured %p\n", this);
				captured = true;
			}
		}
	}

	void Win::ReleaseCapture()
	{
		if ( captured )
		{
			::XUngrabPointer( display, CurrentTime );
			//dbg_printf("UnCaptured %p\n", this);
			captured = false;
		}
	}

	void Win::Activate()
	{
		if ( ( Type() != WT_MAIN && Type() != WT_POPUP ) ||
		     !IsVisible() ||
		     //blockedBy ||
		     activeWinId == handle
		   )
		{
			return;
		}

		::XSetInputFocus( display, handle, RevertToNone, CurrentTime );


		if ( focusWinId )
		{
			Win* prevFocus = GetWinByID( focusWinId );
			focusWinId = 0;

			if ( prevFocus )
			{
				Win* w = prevFocus;

				for ( ; w && w->parent && w->type != Win::WT_MAIN && w->type != Win::WT_POPUP; w = w->parent )
				{
					w->parent->lastFocusChild = w->handle;
				}

				cevent evKF( EV_KILLFOCUS );
				prevFocus->Event( &evKF );
			}
		}

		Win* prevActive = GetWinByID( activeWinId );

		if ( activeWinId )
		{

			activeWinId = 0;

			if ( prevActive )
			{
				cevent_activate ev( false, this );
				prevActive->Event( &ev );
			}
		}

		cevent_activate evActivate( true, prevActive );
		activeWinId = handle;
		Event( &evActivate );
		focusWinId = handle;

		cevent evSF( EV_SETFOCUS );
		Event( &evSF );
	}

	void Win::AddExposeRect( crect r )
	{
		if ( exposeRect.IsEmpty() )
		{
			exposeRect = r;
		}
		else
		{
			if ( r.left < exposeRect.left ) { exposeRect.left = r.left; }

			if ( r.top < exposeRect.top ) { exposeRect.top = r.top; }

			if ( r.right > exposeRect.right ) { exposeRect.right = r.right; }

			if ( r.bottom > exposeRect.bottom ) { exposeRect.bottom = r.bottom; }
		}
	}

	void Win::Invalidate()
	{
		AddExposeRect( ClientRect() );
		AddRepaint( this );
	}

	void Win::Show( SHOW_TYPE type )
	{
		::XMapWindow( display, GetID() );
		showType = type; //used in mapping event
		SetState( S_VISIBLE );
	}

	void Win::Hide()
	{
		::XUnmapWindow( display, GetID() );
		ClearState( S_VISIBLE );
	};


	void Win::RecalcLayouts()
	{
		if ( !layout ) { return; }

		wal::ccollect<WSS> list;
		layout->SetPos( ClientRect(), list );

		for ( int i = 0; i < list.count(); i++ )
		{
			list[i].w->Move( list[i].rect, true );
			list[i].w->Invalidate();
		}

		Invalidate();
	}

	void Win::SetName( const unicode_t* name )
	{
		if ( type == WT_MAIN )
		{
			unicode_t u[100];
			unicode_strncpy0(u, name, sizeof(u)/sizeof(u[0]));

			char buf[400];
			unicode_to_utf8( buf, u );
			XStoreName( display, handle, buf );
		}
	}

	void Win::SetName( const char* utf8Name )
	{
		if ( type == WT_MAIN )
		{
			XStoreName( display, handle, utf8Name );
		}

	}

	void Win::SetFocus()
	{
		if ( InFocus() ) { return; }

		if ( blockedBy )
		{
			Win* w = GetWinByID( blockedBy );

			if ( w ) { w->SetFocus(); }

			return;
		}

		Win* p = this;

		for ( ; p && p->type != WT_MAIN && p->type != WT_POPUP; ) { p = p->parent; }

		if ( p )
		{
			p->Activate();

			if ( focusWinId != handle )
			{
				Win* f = GetWinByID( focusWinId );

				if ( f )
				{
					cevent evKF( EV_KILLFOCUS );
					f->Event( &evKF );
				}

				focusWinId = handle;

				if ( type != WT_MAIN && type != WT_POPUP && parent )
				{
					parent->lastFocusChild = handle;
				}

				cevent evSF( EV_SETFOCUS );
				Event( &evSF );
			}
		}
	}

	void Win::OnTop()
	{
		::XRaiseWindow( display, handle );
	}


	crect Win::ClientRect()
	{
		return crect( 0, 0, position.Width(), position.Height() );
	}

	crect Win::ScreenRect()
	{
		crect r = position;
		int x = 0, y = 0;
		ClientToScreen( &x, &y );
		return crect( x, y, x + r.Width(), y + r.Height() );
	}

	crect Win::Rect()
	{
		return position;
	}

	void Win::ClientToScreen( int* x, int* y )
	{
		Window cw;
		::XTranslateCoordinates( display, GetID(), XDefaultRootWindow( display ), *x, *y, x, y, &cw );
	}

	Win::~Win()
	{
		wth_DropWindow( this );

		if ( modal ) // ???? ����� � �� ����
		{
			( ( ModalStruct* )modal )->EndModal( 0 );
		}

		if ( handle )
		{
			DelAllTimers();
			XDestroyWindow( display, handle );
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

	}

	void Win::SetIcon( const char** ps )
	{
		XPMImage xpm;
		xpm.Load( ps, 16000 );

		if ( xpm.Width() <= 0 || xpm.Height() <= 0 ) { return; }

		int w = xpm.Width();
		int h = xpm.Height();

		Image32 i32;
		i32.copy( xpm );

		if ( winIconPixmap != None )
		{
			XFreePixmap( display, winIconPixmap );
			winIconPixmap = None;
		}

		if ( winIconMask != None )
		{
			XFreePixmap( display, winIconMask );
			winIconMask = None;
		}

		winIconPixmap = XCreatePixmap( display, DefaultRootWindow( display ), w, h, visualInfo.depth );
		winIconMask = XCreatePixmap( display, DefaultRootWindow( display ), w, h, 1 );

		{
			IntXImage intXImage( i32 );
			GC gc( winIconPixmap );
			intXImage.Put( gc, 0, 0, 0, 0, w, h );
		}

		{
			::GC gc;
			XGCValues v;
			gc = XCreateGC( display, winIconMask, 0, &v );

			v.foreground = 0;
			XChangeGC( display, gc, GCForeground, &v );
			XFillRectangle( display, winIconMask, gc, 0, 0, w, h );

			v.foreground = 1;
			XChangeGC( display, gc, GCForeground, &v );

			unsigned32* p = i32.line( 0 );

			for ( int y = 0; y < h; y++ )
			{
				for ( int x = 0; x < w; x++, p++ )
					if ( *p < 0x80000000 )
					{
						XDrawPoint( display, winIconMask, gc, x, y );
					}
			}

			XFreeGC( display, gc );
		}
	}

/////////////////////////////////////////////  cfont

	cfont::cfont( GC& gc, const char* name, int pointSize, cfont::Weight weight, unsigned flags )
		: type( -1 ), data( 0 )
	{
		char buf[1024];
		const char* w;

		switch ( weight )
		{
			case Normal:
				w = "medium";
				break;

			case Light:
				w = "light";
				break;

			case Bold:
				w = "bold";
				break;

			default:
				w = "*";
		}

		const char* slant = "r";

		if ( flags & Italic )
		{
			slant = "i";
		}

		sprintf( buf, "-*-%s-%s-%s-*-*-*-%i-*-*-*-*-iso10646-*", name, w, slant,  pointSize * 10 );

		XFontStruct* xfs = XLoadQueryFont( display, buf );

		if ( !xfs )
		{
			dbg_printf( "font not found %s\n", buf );
		}
		else
		{
			type = TYPE_X11;
			data = xfs;
			_uri = new_char_str( buf );
			dbg_printf( "font %s created Ok!!!\n", buf );
		}
	}

	const char* cfont::uri()
	{
		return _uri.data() ? _uri.data() : "";
	}


	const char* cfont::printable_name()
	{
		if ( _name.data() ) { return _name.data(); }

		if ( type == TYPE_X11 )
		{
			return uri();
		}

#ifdef   USEFREETYPE

		if ( type == TYPE_FT )
		{
			if ( !data ) { return ""; }

			const char* name = ( ( FTU::FFace* )data )->Name();
			const char* style = ( ( FTU::FFace* )data )->StyleName();

			if ( !name || !style ) { return ""; }

			char buf[1024];
			snprintf( buf, sizeof( buf ) - 1, "%s-%s %i", name, style, ( ( FTU::FFace* )data )->Size() / 64 );
			_name = new_char_str( buf );
			return _name.data();
		}

#endif
		return "";
	}


	cfont::cfont( const char* uriStr )
		: type( -1 ), data( 0 )
	{
		const char* s = uriStr;

		while ( *s && *s <= 32 ) { s++; }

		if ( *s == '-' ) //x11 variant
		{
			XFontStruct* xfs = XLoadQueryFont( display, s );

			if ( xfs )
			{
				type = TYPE_X11;
				data = xfs;
				_uri = new_char_str( s );
			}

			return;
		}

#ifdef USEFREETYPE

		if ( *s == '+' ) //FreeType +[ptsize*10]:<path>
		{
			const char* t = s + 1;
			int size = 0;

			for ( ; *t >= '0' && *t <= '9'; t++ ) { size = size * 10 + ( *t - '0' ); }

			if ( *t == ':' )
			{
				t++;
			}
			else
			{
				t = s + 1;
				size = 100;
			}

			while ( *t && *t <= 32 ) { t++; }

			if ( size < 10 ) { size = 10; }

			if ( size > 1000 ) { size = 1000; }

			FTU::FFace* p = new  FTU::FFace();

			if ( p->Load( t, 0, ( size * 64 ) / 10 ) )
			{
				delete p;
			}
			else
			{
				type = TYPE_FT;
				data = p;
				_uri = new_char_str( s );
			}

			return;
		}

#endif
	}


	cfont::cfont( const char* fileName, int pointSize )
		: type( -1 ), data( 0 )
	{
#ifdef USEFREETYPE
		FTU::FFace* p = new  FTU::FFace();

		if ( p->Load( fileName, 0, ( pointSize * 64 ) / 10 ) )
		{
			delete p;
		}
		else
		{
			type = TYPE_FT;
			data = p;
			char buf[1024];
			snprintf( buf, sizeof( buf ), "+%i:%s", pointSize, fileName );
			_uri = new_char_str( buf );
		}

#endif
	}


	void cfont::drop()
	{
		if ( type == TYPE_X11 )
		{
			XFreeFont( display, ( XFontStruct* )data );
		}

#ifdef USEFREETYPE
		else if ( type == TYPE_FT )
		{
			delete ( FTU::FFace* )data;
		}

#endif
		type = -1;
		data = 0;
	}




//////////////////////////////////////// cicon

	extern void MakeDisabledImage32( Image32* dest, const Image32& src );

	/*
	struct Node {
	   SCImage image;
	   std::vector<char> mash;
	   unigned bgColor;
	};
	*/

	static clPtr<IconData::Node> CreateIconDataNode( Image32& _image, unsigned bgColor, bool enabled )
	{
		clPtr<IconData::Node> node = new IconData::Node;
		node->bgColor = bgColor;

		int w = _image.width();
		int h = _image.height();

		if ( w <= 0 || h <= 0 ) { return node; }

		Image32 im;

		if ( enabled )
		{
			im.copy( _image );
		}
		else
		{
			MakeDisabledImage32( &im, _image );
		}

		unsigned32* p = im.line( 0 );
		int count = w * h;

		bool masked = false;

		for ( ; count > 0; count--, p++ )
		{
			if ( *p & 0xFF000000 )
			{
				masked = masked || *p >= 0x80000000;

				unsigned fgColor = *p;
				unsigned c = ( fgColor >> 24 ) & 0xFF;

				if ( c == 0xFF ) { *p = bgColor; }
				else
				{
					unsigned c1 = 255 - c;
					*p =
					   ( ( ( ( bgColor & 0x0000FF ) * c + ( fgColor & 0x0000FF ) * c1 ) >> 8 ) & 0x0000FF ) +
					   ( ( ( ( bgColor & 0x00FF00 ) * c + ( fgColor & 0x00FF00 ) * c1 ) >> 8 ) & 0x00FF00 ) +
					   ( ( ( ( bgColor & 0xFF0000 ) * c + ( fgColor & 0xFF0000 ) * c1 ) >> 8 ) & 0xFF0000 );
				}

			}
		}

		if ( masked )
		{
			unsigned32* p = _image.line( 0 );
			int count = im.width() * im.height();

			node->mask.resize( count );
			char* m = node->mask.data();

			for ( ; count > 0; count--, p++, m++ )
			{
				*m = ( *p >= 0x80000000 ) ? 0 : 1;
			}

		}


		IntXImage xim( im );
		node->image.Create( w, h );
		wal::GC gc( &node->image );
		xim.Put( gc, 0, 0, 0, 0, w, h );
		return node;
	}

	static void PutIconDataNode( wal::GC& gc, SCImage* im, int x, int y )
	{
		if ( !im ) { return; }

		XCopyArea( display, im->GetXDrawable(), gc.GetXDrawable(), gc.XHandle(), 0, 0, im->Width(), im->Height(),  x, y );
	}

	static void PutIconDataNode( wal::GC& gc, SCImage* im, char* mask, int dest_x, int dest_y )
	{
		if ( !im ) { return; }

		if ( !mask )
		{
			PutIconDataNode( gc, im, dest_x, dest_y );
			return;
		}

		int src_x = 0;
		int src_y = 0;
		int w = im->Width();
		int h = im->Height();

		if ( w <= 0 || h <= 0 ) { return; }

		int right =  w;
		int bottom = h;

		crect r( 0, -1, 0, -1 );
		char* m = mask;

		for ( int y = src_y; y < bottom; y++, m += im->Width() )
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
//printf("copy %i\n", y);
						if ( !r.IsEmpty() )
						{
							XCopyArea( display, im->GetXDrawable(), gc.GetXDrawable(), gc.XHandle(), r.left, r.top, r.Width(), r.Height(),  dest_x + r.left, dest_y + r.top );
						}

						r.Set( x1, y, x, y + 1 );
					}
				}
			}
		}

		if ( !r.IsEmpty() )
		{
			XCopyArea( display, im->GetXDrawable(), gc.GetXDrawable(), gc.XHandle(), r.left, r.top, r.Width(), r.Height(),  dest_x + r.left, dest_y + r.top );
		}
	}


	void cicon::Draw( wal::GC& gc, int x, int y, bool enabled )
	{
		if ( !data ) { return; }

		unsigned bgColor = gc.FillRgb();
		clPtr<IconData::Node>* pNode = enabled ? &data->normal : &data->disabled;

		if ( !pNode->ptr() )
		{
			*pNode = CreateIconDataNode( data->image, bgColor, enabled );
		}

		PutIconDataNode( gc, &( pNode[0]->image ), pNode[0]->mask.data(), x, y );
	}

	void cicon::DrawF( wal::GC& gc, int x, int y, bool enabled )
	{
		if ( !data ) { return; }

		unsigned bgColor = gc.FillRgb();
		clPtr<IconData::Node>* pNode = enabled ? &data->normal : &data->disabled;

		if ( !pNode->ptr() || pNode[0]->bgColor != bgColor )
		{
			*pNode = CreateIconDataNode( data->image, bgColor, enabled );
		}

		PutIconDataNode( gc, &( pNode[0]->image ), x, y );
	}


////////////////////////////////////// IntXImage


	void IntXImage::init( int w, int h )
	{
		static unsigned byte_order_test = 0x77;
		memset( &im, 0, sizeof( im ) );
		im.width = w;
		im.height = h;
		im.xoffset = 0;
		im.depth = visualInfo.depth;

		if ( visualInfo.c_class == TrueColor )
		{
			im.format =  ZPixmap ;
			data.resize( w * h * 4 ); //32 bit
			im.data = data.data();
			im.byte_order = ( ( ( char* )&byte_order_test )[0] == 0x77 ) ? LSBFirst : MSBFirst;
			im.bitmap_unit = 32;
			im.bitmap_bit_order = MSBFirst;
			im.bitmap_pad = 32;

			im.bytes_per_line = w * 4; //
			im.bits_per_pixel = 32;
			im.red_mask = visualInfo.red_mask;
			im.green_mask = visualInfo.green_mask;
			im.blue_mask = visualInfo.blue_mask;
		}
		else
		{
			im.format = XYPixmap;
			im.bytes_per_line = ( w + 31 ) / 32 * 4;
			data.resize( im.bytes_per_line * visualInfo.depth * h ); //32 bit
			im.data = data.data();
			im.byte_order = ( ( ( char* )&byte_order_test )[0] == 0x77 ) ? LSBFirst : MSBFirst;
			im.bitmap_unit = 32;
			im.bitmap_bit_order = MSBFirst;
			im.bitmap_pad = 32;
		}

		XInitImage( &im );
	}


	void IntXImage::Set( Image32& image )
	{
		clear();

		int w = image.width();
		int h = image.height();

		if ( w <= 0 || h <= 0 ) { return; }

		init( w, h );

		if ( visualInfo.c_class != TrueColor )
		{
			int i;
			bool masked = false;

			for ( int y = 0; y < h; y++ )
			{
				unsigned32* t = image.line( y );
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

					XPutPixel( &im, i, y, CreateColor( t[i] & 0xFFFFFF ) );
				}
			}

			return;
		}



		unsigned32* p = ( unsigned32* )data.data();
		unsigned32* t = image.line( 0 );

		unsigned32 lastPixVal = 0;
		unsigned32 lastColor = CreateColor(lastPixVal);
		int n;
		for ( n = w * h; n > 0; n--, p++, t++ )
		{
			unsigned32 pixVal = *t;

			if ( lastPixVal >= 0x80000000 )
			{
				goto haveMask;
			}

			if ( pixVal == lastPixVal )
			{
				*p = lastColor;
			}
			else
			{
				lastPixVal = pixVal;
				*p = lastColor =  CreateColor( pixVal );
			}
		}

		return;

haveMask:
		mask.resize( w * h );
		memset( mask.data(), 1, w * h );

		char* m = mask.data() + w * h - n;

		for ( ; n > 0; n--, p++, t++, m++ )
		{
			unsigned32 pixVal = *t;

			if ( pixVal >= 0x80000000 )
			{
				*m = 0;
			}

			if ( pixVal == lastPixVal )
			{
				*p = lastColor;
			}
			else
			{
				lastPixVal = pixVal;
				*p = lastColor =  CreateColor( pixVal );
			}
		}
	}

	void IntXImage::Put( wal::GC& gc, int src_x, int src_y, int dest_x, int dest_y, int w, int h )
	{
		if ( !data.data() ) { return; }

		int right = src_x + w;
		int bottom = src_y + h;

		if ( src_x < 0 ) { src_x = 0; }

		if ( src_y < 0 ) { src_y = 0; }

		if ( right > im.width ) { right = im.width; }

		if ( bottom > im.height ) { bottom = im.height; }

		w = right - src_x;
		h = bottom - src_y;

		if ( w <= 0 || h <= 0 ) { return; }

		if ( mask.data() )
		{

			crect r( 0, -1, 0, -1 );
			char* m = mask.data();

			for ( int y = src_y; y < bottom; y++, m += im.width )
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
								XPutImage( display, gc.GetXDrawable(), gc.XHandle(), &im, r.left, r.top,
								           dest_x + r.left, dest_y + r.top, r.Width(), r.Height() );

							r.Set( x1, y, x, y + 1 );
						}
					}
				}
			}

			if ( !r.IsEmpty() )
				XPutImage( display, gc.GetXDrawable(), gc.XHandle(), &im, r.left, r.top,
				           dest_x + r.left, dest_y + r.top, r.Width(), r.Height() );


		}
		else
		{
			XPutImage( display, gc.GetXDrawable(), gc.XHandle(), &im, src_x, src_y, dest_x, dest_y, w, h );
		}
	}

	IntXImage::IntXImage( Image32& image )
	{
		Set( image );
	}

	IntXImage::~IntXImage() {}




////////  SCImage (Screen compatible image) /////////////////////////////////////////////////

	void SCImage::Destroy()
	{
		if ( handle != None )
		{
			XFreePixmap( display, handle );
			handle = None;
		}

		width = height = 0;
	}

	SCImage::SCImage(): width( 0 ), height( 0 ), handle( None ) {}

	void SCImage::Create( int w, int h )
	{
		Destroy();

//printf("Pixmap w=%i h = %i\n", w, h);

		if ( w <= 0 || h <= 0 )
		{
			width = height = 0;
			handle = None;
			return;
		}

		handle = XCreatePixmap( display, DefaultRootWindow( display ), w, h, visualInfo.depth );
//printf("Pixmap created\n");
		width = w;
		height = h;
	}

	SCImage::~SCImage() { Destroy(); }





} // namespace wal
