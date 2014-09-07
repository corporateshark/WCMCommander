/*
   Copyright (c) by Valery Goryachev (Wal)
*/


#ifndef WINCORE_H
#define WINCORE_H

#define USE_3D_BUTTONS 0

#ifdef _WIN32
#include <windows.h>
typedef HWND WinID;
extern HINSTANCE appInstance;
#else       //#elif __linux__
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <wchar.h>
typedef Window WinID;
#endif

#include <stdarg.h>

namespace wal
{

	enum CoreCommands
	{
	   CMD_CHECK = 0, //команда проверки (поддерживается команда или нет) номер команды в этом случае передается в подкоманде
	   CMD_OK = 1,
	   CMD_CANCEL = 2,
	   CMD_YES = 3,
	   CMD_NO = 4
	};

	struct cpoint;
	struct crect;
	class cpen;
	class cbrush;
	class cfont;
	class cevent;
	//...
	class crgn;
	class cicon;
	class Win;
	class GC;
	class Layout;

	struct cpoint
	{
		int x, y;
		cpoint() {}
		cpoint( int _x, int _y ): x( _x ), y( _y ) {}
		cpoint( const cpoint& a ): x( a.x ), y( a.y ) {}
		cpoint& operator = ( const cpoint& a ) {x = a.x; y = a.y; return *this;}
		cpoint& Set( int _x, int _y ) {x = _x; y = _y; return *this;}
		cpoint& operator + ( const cpoint& a ) { x += a.x; y += a.y; return *this; }
		cpoint& operator - ( const cpoint& a ) { x -= a.x; y -= a.y; return *this; }
		bool operator == ( const cpoint& a ) const { return x == a.x && y == a.y; }
	};

	struct crect
	{
		int left, top;
		int right, bottom;
		crect() {}
		crect( const cpoint& lt, const cpoint& rb ): left( lt.x ), top( lt.y ), right( rb.x ), bottom( rb.y ) {}
		crect( int _left, int _top, int _right, int _bottom ): left( _left ), top( _top ), right( _right ), bottom( _bottom ) {}
		void Set( int _left, int _top, int _right, int _bottom ) {left = _left; right = _right; top = _top; bottom = _bottom;}

		void Inc() { left--; right++; top--; bottom++; }
		void Dec() { left++; right--; top++; bottom--; }

		void Inc( int n ) { left -= n; right += n; top -= n; bottom += n; }
		void Dec( int n ) { left += n; right -= n; top += n; bottom -= n; }

		int Height() const { return bottom - top; }
		int Width()const { return right - left; }
		bool In( cpoint p )const { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
		bool operator == ( const crect& a ) const { return left == a.left && right == a.right && top == a.top && bottom == a.bottom; }
		bool operator != ( const crect& a ) const { return !( *this == a ); }
		bool IsEmpty() const { return bottom <= top || right <= left; }
		void Zero() { left = top = right = bottom = 0; }

		bool Cross( const crect& a ) const
		{
			crect b( left > a.left ? left : a.left, top > a.top ? top : a.top,
			         right < a.right ? right : a.right, bottom < a.bottom ? bottom : a.bottom );
			return !b.IsEmpty();
		}
	};

}; //namespace wal

#include "swl_layout.h"

namespace wal
{

	void AppInit();
	int AppRun();
	void AppExit();

#ifdef _WIN32
	enum Keyboard
	{
	   VK_NUMPAD_CENTER = 0x0C,
	   VK_NUMPAD_RETURN = 0xFFFF,

		VK_LMETA      = VK_LWIN,
		VK_RMETA      = VK_RWIN,

#ifndef VK_SLASH
	   VK_SLASH      =   0xBF,
#endif

#ifndef VK_BACKSLASH
	   VK_BACKSLASH  =   0xDC,
#endif

#ifndef VK_GRAVE
	   VK_GRAVE = 0xC0,
#endif
	   VK_0 = 0x30,
	   VK_1 = 0x31,
	   VK_2 = 0x32,
	   VK_3 = 0x33,
	   VK_4 = 0x34,
	   VK_5 = 0x35,
	   VK_6 = 0x36,
	   VK_7 = 0x37,
	   VK_8 = 0x38,
	   VK_9 = 0x39,

	   VK_A = 0x41,
	   VK_B = 0x42,
	   VK_C = 0x43,
	   VK_D = 0x44,
	   VK_E = 0x45,
	   VK_F = 0x46,
	   VK_G = 0x47,
	   VK_H = 0x48,
	   VK_I = 0x49,
	   VK_J = 0x4A,
	   VK_K = 0x4B,
	   VK_L = 0x4C,
	   VK_M = 0x4D,
	   VK_N = 0x4E,
	   VK_O = 0x4F,
	   VK_P = 0x50,
	   VK_Q = 0x51,
	   VK_R = 0x52,
	   VK_S = 0x53,
	   VK_T = 0x54,
	   VK_U = 0x55,
	   VK_V = 0x56,
	   VK_W = 0x57,
	   VK_X = 0x58,
	   VK_Y = 0x59,
	   VK_Z = 0x5A,

	   VK_BRACKETLEFT = 219,
	   VK_BRACKETRIGHT = 221,
	};


#endif


#ifndef _WIN32
	enum Keyboard
	{
	   VK_NUMPAD_CENTER = XK_KP_Begin,

	   VK_ESCAPE = XK_Escape,
	   VK_TAB = XK_Tab,
	   VK_RETURN = XK_Return,
	   VK_NUMPAD_RETURN = XK_KP_Enter,
	   VK_BACK = XK_BackSpace,
	   VK_LEFT = XK_Left,
	   VK_RIGHT = XK_Right,
	   VK_HOME = XK_Home,
	   VK_END = XK_End,
	   VK_UP = XK_Up,
	   VK_DOWN = XK_Down,
	   VK_SPACE = XK_space, //XK_KP_Space,
	   VK_DELETE = XK_Delete,
	   VK_NEXT = XK_Next,
	   VK_PRIOR = XK_Prior,
	   VK_OEM_PLUS = XK_equal,

	   VK_ADD = XK_KP_Add,
	   VK_SUBTRACT = XK_KP_Subtract,
	   VK_MULTIPLY = XK_KP_Multiply,

	   VK_DIVIDE = XK_KP_Divide,
	   VK_SLASH      =   XK_slash,
	   VK_BACKSLASH  =   XK_backslash,

	   VK_GRAVE = XK_grave,

	   VK_INSERT = XK_Insert,

		VK_LMETA = XK_Meta_L,
		VK_RMETA = XK_Meta_R,

	   VK_LCONTROL = XK_Control_L,
	   VK_RCONTROL = XK_Control_R,
	   VK_LSHIFT = XK_Shift_L,
	   VK_RSHIFT = XK_Shift_R,

	   VK_LMENU = XK_Alt_L,
	   VK_RMENU = XK_Alt_R,

	   VK_F1 = XK_F1,
	   VK_F2 = XK_F2,
	   VK_F3 = XK_F3,
	   VK_F4 = XK_F4,
	   VK_F5 = XK_F5,
	   VK_F6 = XK_F6,
	   VK_F7 = XK_F7,
	   VK_F8 = XK_F8,
	   VK_F9 = XK_F9,
	   VK_F10 = XK_F10,
	   VK_F11 = XK_F11,
	   VK_F12 = XK_F12,

	   VK_1 = XK_1,
	   VK_2 = XK_2,
	   VK_3 = XK_3,
	   VK_4 = XK_4,
	   VK_5 = XK_5,
	   VK_6 = XK_6,
	   VK_7 = XK_7,
	   VK_8 = XK_8,
	   VK_9 = XK_9,
	   VK_0 = XK_0,

	   VK_BRACKETLEFT = XK_bracketleft,
	   VK_BRACKETRIGHT = XK_bracketright,

#define S(a) VK_##a = XK_##a
	   S( A ),
	   S( B ),
	   S( C ),
	   S( D ),
	   S( E ),
	   S( F ),
	   S( G ),
	   S( H ),
	   S( I ),
	   S( J ),
	   S( K ),
	   S( L ),
	   S( M ),
	   S( N ),
	   S( O ),
	   S( P ),
	   S( Q ),
	   S( R ),
	   S( S ),
	   S( T ),
	   S( U ),
	   S( V ),
	   S( W ),
	   S( X ),
	   S( Y ),
	   S( Z )
#undef s
	};
#endif

	enum events
	{
	   EV_NONE = 0,
	   EV_CLOSE,
	   EV_SHOW,
	   EV_ACTIVATE,

	   EV_KEYDOWN = 10,
	   EV_KEYUP,

	   EV_MOUSE_MOVE = 20,
	   EV_MOUSE_PRESS,
	   EV_MOUSE_RELEASE,
	   EV_MOUSE_DOUBLE,
	   EV_MOUSE_WHELL,

	   EV_ENTER,
	   EV_LEAVE,
	   EV_SETFOCUS = 30,
	   EV_KILLFOCUS,
	   EV_SIZE = 50,
	   EV_MOVE,
	   EV_TIMER = 70,
	};


	enum KEYMODFLAG
	{
	   KM_SHIFT = 0x0001,
	   KM_CTRL = 0x0002,
	   KM_ALT  = 0x0004
	};

	class cevent: public iIntrusiveCounter
	{
		int type;
	public:
		cevent(): type( EV_NONE ) {}
		cevent( int t ): type( t ) {}
		int Type() const {return type; }
		virtual ~cevent();
	};

	class cevent_show: public cevent
	{
		bool show;
	public:
		cevent_show( bool sh ): cevent( EV_SHOW ), show( sh ) {}
		bool Show() { return show; }
		virtual ~cevent_show();
	};

	class cevent_activate: public cevent
	{
		bool activated;
		Win* who;
	public:
		cevent_activate( bool act, Win* w ): cevent( EV_ACTIVATE ), activated( act ), who( w ) {}
		bool Activated() { return activated; }
		Win* Who() { return who; }
		virtual ~cevent_activate();
	};

	class cevent_input: public cevent
	{
		unsigned keyMods;
	public:
		cevent_input( int t, unsigned km ): cevent( t ), keyMods( km ) {}
		unsigned Mod() { return keyMods; }
		virtual ~cevent_input();
	};

	enum MOUSEBUTTON
	{
	   MB_L  = 0x01,
	   MB_M  = 0x02,
	   MB_R  = 0x04,
	   MB_X1 = 0x08,
	   MB_X2 = 0x10
	};

	class cevent_mouse: public cevent_input
	{
		cpoint point;
		unsigned buttonFlag;
		int button;
		//int whellDistance;
	public:
		cevent_mouse( int type, cpoint p, int b, unsigned bf, unsigned km )
			: cevent_input( type, km ), point( p ), buttonFlag( bf ), button( b ) {};

		cpoint& Point() { return point; }
		unsigned ButtonFlag() { return buttonFlag; }
		bool ButtonL() { return ( buttonFlag & MB_L ) != 0; }
		bool ButtonR() { return ( buttonFlag & MB_R ) != 0; }
		int Button() { return button; }
		virtual ~cevent_mouse();
	};


// применяются виндовозные Virtual keys
	class cevent_key: public cevent_input
	{
		int key; //virtual key
		int count;
		unicode_t chr;
	public:
		cevent_key( int type, int k, unsigned km, int cnt, unicode_t ch )
			: cevent_input( type, km ), key( k ), count( cnt ), chr( ch ) {};

		int Key() { return key; }
		int Count() { return count; }
		unicode_t Char() { return chr; };
		virtual ~cevent_key();
	};

	class cevent_size: public cevent
	{
		cpoint size;
	public:
		cevent_size( cpoint s ): cevent( EV_SIZE ), size( s ) {};
		cpoint& Size() { return size; }
		virtual ~cevent_size();
	};

	class cevent_move: public cevent
	{
		cpoint pos;
	public:
		cevent_move( cpoint s ): cevent( EV_MOVE ), pos( s ) {};
		cpoint& Pos() { return pos; }
		virtual ~cevent_move();
	};




#ifdef _WIN32
#else
	unsigned CreateColorFromRGB( unsigned );
#endif


	class cfont: public iIntrusiveCounter
	{
		cfont() {}
		cfont( const cfont& ) {}
		void operator = ( const cfont& ) {}
		friend class GC;

#ifdef _WIN32
		HFONT handle;
		bool external; //true если внешний хэндл (чтоб не удалять в деструкторе)
		void drop() { if ( handle && !external ) { ::DeleteObject( handle ); } handle = 0;  }
#else
		enum { TYPE_X11 = 0, TYPE_FT = 1 };
		int type;
		void* data;

		//XFontStruct *xfs;

		void drop();
#endif

		std::vector<char> _uri;
		std::vector<char> _name;
	public:
#ifdef _WIN32
		static void SetWin32Charset( unsigned );
		cfont( HFONT hf ): handle( hf ), external( true ) {};
		bool Ok() { return handle != NULL; }

		static std::vector<char> LogFontToUru( LOGFONT& lf );
		static void UriToLogFont( LOGFONT* plf, const char* uri );
#else
		static void SetWin32Charset( unsigned ) {};
		bool Ok() { return data != 0; }
#endif

		enum Weight { X3 = 0, Normal, Light, Bold };
		enum Flags { Italic = 1, Fixed = 2 };

		cfont( GC& gc, const char* name, int pointSize, cfont::Weight weight = Normal, unsigned flags = 0 );
		cfont( const char* fileName, int pointSize ); //FreeType
		cfont( const char* uri );
		const char* uri();
		const char* printable_name();

		~cfont() { drop(); }

		static clPtr<cfont> New( GC& gc, const char* name, int pointSize, cfont::Weight weight = Normal, unsigned flags = 0 )
		{
			clPtr<cfont> p = new cfont( gc, name, pointSize, weight, flags );

			if ( !p->Ok() ) return clPtr<cfont>();

			return p;
		}

		static clPtr<cfont> New( const char* fileName, int pointSize )
		{
			clPtr<cfont> p = new cfont( fileName, pointSize );

			if ( !p->Ok( ) ) return clPtr<cfont>( );

			return p;
		}

		static clPtr<cfont> New( const char* x11string )
		{
			clPtr<cfont> p = new cfont( x11string );

			if ( !p->Ok( ) ) return clPtr<cfont>( );

			return p;
		}

#ifdef USEFREETYPE
		struct FTInfo: public iIntrusiveCounter
		{
			enum {FIXED_WIDTH = 1};
			unsigned flags;
			std::vector<char> name;
			std::vector<char> styleName;
		};
		static clPtr<FTInfo> GetFTFileInfo( const char* path );
#endif

	};

	class XPMImage
	{
		int colorCount;
		std::vector<unsigned> colors;
		int none;
		int width, height;
		std::vector<int> data;
	public:
		void Clear();
		XPMImage(): colorCount( 0 ), none( -1 ), width( 0 ), height( 0 ) {}

		int Width() const { return width; }
		int Height() const { return height; }

		const unsigned* Colors() const { return colors.data(); }
		const int* Data() const { return data.data(); }

		bool Load( const char** list, int count );
		//void Load(InStream &stream);
		~XPMImage() {}
	};

	class Image32
	{
		int _width;
		int _height;
		std::vector<unsigned32> _data;
//		std::vector<unsigned32*> _lines;

		Image32( const Image32& ) {}
		void operator=( const Image32& ) {}

	public:
		Image32(): _width( 0 ), _height( 0 ) {}

		void alloc( int w, int c );

		void fill( int left, int top, int right, int bottom, unsigned c );
		void fill( const crect& r, unsigned c ) { fill( r.left, r.top, r.right, r.bottom, c ); }

		void copy( const Image32& a );
		void copy( const Image32& a, int w, int h );

		void copy( const XPMImage& xpm );

		void clear();

		unsigned32 get( int x, int y )
		{
			//return ( x >= 0 && x < _width && y >= 0 && y < _height ) ? _lines[y][x] : 0xFF;
			return ( x >= 0 && x < _width && y >= 0 && y < _height ) ? _data[ y * _width + x ] : 0xFF;
		}

		unsigned32* line( int y ) { return &_data[ y * _width ]; }

		Image32( int w, int h ) { alloc( w, h ); }

		int width() const { return _width; }
		int height() const { return _height; }

		~Image32() {}
	};


#ifdef _WIN32

	class Win32CompatibleBitmap: public iIntrusiveCounter
	{
		HBITMAP handle;
		int _w, _h;
		std::vector<char> mask;
		void clear();
		void init( int w, int h );
	public:
		Win32CompatibleBitmap(): handle( 0 ), _w( 0 ), _h( 0 ) {}
		Win32CompatibleBitmap( int w, int h ): handle( 0 ) { init( w, h ); }
		Win32CompatibleBitmap( Image32& image ) { Set( image ); }
		void Set( Image32& image );
		void Put( wal::GC& gc, int src_x, int src_y, int dest_x, int dest_y, int w, int h );
		~Win32CompatibleBitmap();
	};

#else
//for X
	class IntXImage
	{
		XImage im;
		std::vector<char> data;
		std::vector<char> mask;

		void init( int w, int h );
		void clear() { data.clear(); mask.clear(); }
	public:
		IntXImage( int w, int h ) { init( w, h ); }
		IntXImage( Image32& image );
		void Set( Image32& image );
		void Set( Image32& image, unsigned bgColor );
		void Put( wal::GC& gc, int src_x, int src_y, int dest_x, int dest_y, int w, int h );
		XImage* GetXImage() {return data.data() ? &im : 0; };
		~IntXImage();
	};
#endif


////////  Screen compatible image
	class SCImage
	{
		int width;
		int height;

#ifdef _WIN32
		HBITMAP handle;
#else
		Pixmap handle;
#endif

		SCImage( const SCImage& ) {};
		void operator = ( const SCImage& ) {};
	public:
		SCImage();
		void Destroy();
		void Create( int w, int h );
		int Width() const { return width; };
		int Height()const { return height; };

#ifdef _WIN32
		HBITMAP GetX11Handle() { return handle; };
#else
		Pixmap GetXDrawable() { return handle; };
#endif

		~SCImage();
	};


	struct IconData
	{
		int counter;
		Image32 image;

#ifdef _WIN32
		clPtr<Win32CompatibleBitmap> normal;
		clPtr<Win32CompatibleBitmap> disabled;

#else
		//x11 cache
		struct Node: public iIntrusiveCounter
		{
			SCImage image;
			std::vector<char> mask;
			unsigned bgColor;
		};

		clPtr<Node> normal;
		clPtr<Node> disabled;
#endif
	};


//СЃРѕР·РґР°РІР°С‚СЊ Рё РєРѕРїРёСЂРѕРІР°С‚СЊ РјРѕР¶РЅРѕ РІ Р»СЋР±РѕРј РїРѕС‚РѕРєРµ, Р° СЂРёСЃРѕРІР°С‚СЊ С‚РѕР»СЊРєРѕ РІ РѕСЃРЅРѕРІРЅРѕРј
	class cicon: public iIntrusiveCounter
	{
		IconData* data;
	public:
		cicon(): data( 0 ) {}

		void Copy( const cicon& );

		cicon( const cicon& a ): data( 0 ) { Copy( a ); }
		cicon& operator = ( const cicon& a ) { Copy( a ); return *this; }

		int Width() const { return data ? data->image.width() : 0; }
		int Height() const { return data ? data->image.height() : 0; }
		bool Valid() const { return data != 0; }

		void Draw( wal::GC& gc, int x, int y, bool enabled = true );
		void DrawF( wal::GC& gc, int x, int y, bool enabled = true );
		void Clear();

		void Load( int id, int w /*= 16*/, int h /*= 16*/ );
		void Load( const Image32&, int w, int h );
		void Load( const char** list, int w, int h );

		cicon( int id, int w /*= 16*/, int h /*= 16*/ ): data( 0 ) { Load( id, w, h ); }
		cicon( const char** xpm, int w /*= 16*/, int h /*= 16*/ ): data( 0 ) { Load( xpm, w, h ); }

		static void SetCmdIcon( int cmd, const Image32&, int w = 16, int h = 16 );
		static void SetCmdIcon( int cmd, const char** s, int w = 16, int h = 16 );

		static void ClearCmdIcons( int cmd );

		~cicon() { Clear(); }
	};





	class GC: public iIntrusiveCounter
	{
	public:
		enum LineStyle
		{
		   SOLID = 0,
		   DOT
		};
	private:
		GC() {};
		GC( const GC& ) { exit( 1 ); }
#ifdef _WIN32
		friend LRESULT CALLBACK WProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
		friend class cfont;

		struct crgn
		{
			HRGN handle;
			crgn(): handle( 0 ) {}
			void drop() { if ( handle ) { ::DeleteObject( handle ); handle = 0; } }
			void Set( crect* rect = 0 ) { drop(); if ( rect ) { handle = ::CreateRectRgn( rect->left, rect->top, rect->right, rect->bottom ); } }
			~crgn() { drop(); }
		};

		HDC handle;
		bool needDelete;
		HPEN savedPen;
		HBRUSH savedBrush;
		HFONT savedFont;
		crgn rgn;

		HPEN linePen;
		HBRUSH fillBrush;
		unsigned fillRgb;
		unsigned lineRgb;
		unsigned lineWidth;
		int lineStyle;
		unsigned textRgb;
		//unsigned textColor;
		bool textColorSet;
		bool bkColorSet;

		int bkMode; // -1 - неизвестно 0 - transparent 1 - OPAQUE
		GC( HDC h, bool needDel );
		void Restore();

#else
		::GC gc;
		XGCValues gcValues;
		unsigned long valueMask;
		static XFontStruct* defaultFontStruct;
		int lineX, lineY;
		Window winId;
		int fontAscent;
		cfont* curFont;

		unsigned lineRgb, lineColor;
		unsigned textRgb, textColor;
		unsigned fillRgb, fillColor;

		void CheckValues();
		void SetFg( unsigned );
		void SetBg( unsigned );

		void _Init( Drawable id );
	public:
		Drawable GetXDrawable() { return winId; }

#endif

	public:

		GC( Win* win );
		GC( SCImage* win );
#ifndef _WIN32
		GC( Drawable id );
#endif

		void SetFillColor( unsigned rgb );
		void SetLine( unsigned rgb, int width = 1, int style = SOLID );
		void SetTextColor( unsigned rgb );

		unsigned TextRgb() const { return textRgb; }
		unsigned FillRgb() const { return fillRgb; }


		void Set( cfont* font );

		void FillRect( crect r );
		void FillRectXor( crect r );
		void SetClipRgn( crect* r = 0 );

		void DrawIcon( int x, int y, cicon* ico );
		void DrawIconF( int x, int y, cicon* ico );

		void TextOut( int x, int y, const unicode_t* s, int charCount = -1 );
		void TextOutF( int x, int y, const unicode_t* s, int charCount = -1 );
		void MoveTo( int x, int y );
		void LineTo( int x, int y );
		void SetPixel( int x, int y, unsigned rgb );
		cpoint GetTextExtents( const unicode_t* s, int charCount = -1 );
		void Ellipce( crect r );

#ifdef _WIN32
		HDC W32Handle() { return handle; };
#else
		::GC XHandle() { return gc; }
#endif
		virtual ~GC();
	};

#define CI_WIN 0  //class ID

//extern unsigned* (*SysGetColors)(Win *w);
//extern unsigned (*SysGetColor)(Win *w, int colorId);
	extern cfont* ( *SysGetFont )( Win* w, int fontId );

///////////////////////////////// Ui

	extern int GetUiID( const char* name );

	struct UiValueNode: public iIntrusiveCounter
	{
		enum {INT = 1, STR = 2};
		int flags;
		int64 i;
		std::vector<char> s;

		UiValueNode( int64 n ): i( n ), flags( INT ) {};
		UiValueNode( const char* a ): s( new_char_str( a ) ), flags( STR ) {}

		int64 Int();
		const char* Str();
	};

	class UiParzer;

	class UiValue
	{
		friend class UiRules;
		ccollect<clPtr<UiValueNode> > list;
		UiValue* next;
		UiValue( UiValue* nx ): next( nx ) {};
		void Append( int64 n ) { clPtr<UiValueNode> v = new UiValueNode( n ); list.append( v ); }
		void Append( const char* s ) { clPtr<UiValueNode> v = new UiValueNode( s ); list.append( v ); }
		bool ParzeNode( UiParzer& parzer );
		void Parze( UiParzer& parzer );
	public:
		UiValue();
		int64 Int( int n ) { return n >= 0 && n < list.count() ? list[n]->Int() : 0 ; }
		int64 Int() { return Int( 0 ); }
		const char* Str( int n ) { return n >= 0 && n < list.count() ? list[n]->Str() : "" ; }
		const char* Str() { return Str( 0 ); }
		int Count() { return list.count(); }
		~UiValue();
	};

	struct UiSelector;
	class UiRules;

	class UiCache
	{
		bool updated;
		struct Node
		{
			UiSelector* s;
			UiValue* v;
			Node(): s( 0 ), v( 0 ) {}
			Node( UiSelector* _s, UiValue* _v ): s( _s ), v( _v ) {}
		};

		cinthash<int, ccollect<Node> > hash;
	public:
		struct ObjNode
		{
			int classId;
			int nameId;
			ObjNode(): classId( 0 ), nameId( 0 ) {}
			ObjNode( int c, int n ): classId( c ), nameId( n ) {}
		};

		UiCache();

		bool Updated() const {return updated;}
		void Clear() { hash.clear(); updated = false; }
		void Update( UiRules& rules, ObjNode* list, int listCount );
		UiValue* Get( int id, int item, int* condList );
		~UiCache();
	};

	struct UiCondList
	{
		enum {N = 16};
		int buf[N];

		void Clear() { for ( int i = 0; i < N; i++ ) { buf[i] = 0; } }
		UiCondList() { Clear(); }
		void Set( int id, bool yes );
	};

	extern int uiEnabled;
	extern int uiFocus;
	extern int uiItem;
	extern int uiClassWin;
	extern int uiColor;
	extern int uiHotkeyColor;
	extern int uiBackground;
	extern int uiFrameColor;
	extern int uiCurrentItem;
	extern int uiFocusFrameColor;
	extern int uiButtonColor;//for scrollbar ...
	extern int uiMarkColor; //marked text color
	extern int uiMarkBackground;
	extern int uiCurrentItemFrame;
	extern int uiLineColor;
	extern int uiPointerColor;
	extern int uiOdd;


	class Win: public iIntrusiveCounter
	{

#ifdef _WIN32
		friend LRESULT CALLBACK WProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
#else
		friend int DoEvents( XEvent* event );
		friend void DrawExposeRect( Win* w );
#endif
		friend class Layout;
		friend struct LItemWin;
	public:
		enum WTYPE
		{
		   WT_MAIN = 1,
		   WT_POPUP = 2,
		   WT_CHILD = 4
		};

		enum WHINT
		{
		   WH_MINBOX = 8,
		   WH_MAXBOX = 0x10,
		   WH_SYSMENU = 0x20,
		   WH_RESIZE = 0x40,
		   WH_TABFOCUS = 0x80,
		   WH_CLICKFOCUS = 0x100,
		   WH_USEDEFPOS = 0x200 //for WT_MAIN (in win32) use default width and height
		};

		enum STATE
		{
		   S_VISIBLE = 1,
		   S_ENABLED = 4
		};

		enum SHOW_TYPE
		{
		   SHOW_ACTIVE = 0,
		   SHOW_INACTIVE = 1
		};

	private:
		static WinID focusWinId;

		WinID handle;
		Win* parent;
		wal::ccollect<Win*> childList;
		WTYPE type;

		WinID blockedBy; //Каким модальным окном заблокировано
		void* modal; //если не 0 то окно в модальном состоянии

		unsigned whint;
		unsigned state;
		bool captured;
		WinID lastFocusChild;

		LSize lSize;
		Layout* upLayout, *layout;
		int uiNameId;
		UiCache uiCache;

		void SetState( unsigned s ) { state |= s; }
		void ClearState( unsigned s ) { state &= ~s; }

		bool IsOneParentWith( WinID h );
		void PopupTreeList( ccollect<WinID>& list ); //добавляет в список текущее окно и его попапы

		Win* FocusNPChild( bool next );

#ifdef _WIN32

#else
		WinID reparent; //last reparent id


		crect position; // in parent coordinates
		crect exposeRect;
		void AddExposeRect( crect r );
		SHOW_TYPE showType;

		friend void KeyEvent( int type, XKeyEvent* event );
#endif
	protected:
		void UiSetNameId( int id ) { uiNameId = id; }
	public:
		Win( WTYPE t, unsigned hints = 0, Win* _parent = 0, const crect* rect = 0, int uiNId = 0 );

		WinID GetID() { return handle; }
		Win* Parent() { return parent; }
		WTYPE Type() { return type; }
		WinID Blocked() { return blockedBy; }
		int ChildCount() { return childList.count(); }
		Win* GetChild( int n ) { return ( n >= 0 && n < childList.count() ) ? childList[n] : 0; }

		Win* FocusNextChild() { return FocusNPChild( true ); }
		Win* FocusPrevChild() { return FocusNPChild( false ); }

		crect ClientRect();
		crect ScreenRect();
		crect Rect();

		void ClientToScreen( int* x, int* y );

		void SetTabFocusFlag( bool enable ) { if ( enable ) { whint |= WH_TABFOCUS; } else { whint &= ~WH_TABFOCUS; } }
		void SetClickFocusFlag( bool enable ) { if ( enable ) { whint |= WH_CLICKFOCUS; } else { whint &= ~WH_CLICKFOCUS; } }

		virtual void Repaint()
		{
			wal::GC gc( this );
			this->Paint( gc, this->ClientRect() );
		}

		void Show( SHOW_TYPE type = SHOW_ACTIVE );
		void Hide();

		void GetLSize( LSize* ls ) { ( state & S_VISIBLE ) ? *ls = lSize : LSize(); }
		LSize GetLSize() { return ( state & S_VISIBLE ) ? lSize : LSize(); }
		void SetLSize( const LSize& ls ) { lSize = ls; if ( parent && parent->layout ) { parent->layout->valid = false; } } //main and popup ???
		void RecalcLayouts();
		void SetLayout( Layout* pl ) { layout = pl; if ( layout ) { SetLSize( layout->GetLSize() ); } RecalcLayouts(); }

		void SetIdealSize();
		void SetTimer( int id, unsigned period );
		void ResetTimer( int id, unsigned period );
		void DelTimer( int id );
		void DelAllTimers();

		void ThreadCreate( int id, void * ( *f )( void* ), void* d );
		virtual void ThreadSignal( int id, int data );
		virtual void ThreadStopped( int id, void* data );

		int DoModal();
		bool IsModal() { return modal != 0; }
		void EndModal( int id );

		void Move( crect rect, bool repaint = true );

		bool IsEnabled();
		bool InFocus() { return handle == focusWinId; }
		bool IsVisible();
		void Enable( bool en = true );
		void Activate();
		void SetCapture();
		void ReleaseCapture();
		void OnTop();
		bool IsCaptured() { return captured; }
		void Invalidate();
		void SetFocus();

		void SetName( const unicode_t* name );
		void SetName( const char* utf8Name );

		void UiCacheClear() { uiCache.Clear(); }
		virtual int UiGetClassId();
		int UiGetNameId() { return uiNameId; }
		unsigned UiGetColor( int id, int nodeId, UiCondList* cl, unsigned def );

		virtual void Paint( GC& gc, const crect& paintRect );

		virtual bool Event( cevent* pEvent );
		virtual bool EventMouse( cevent_mouse* pEvent );
		virtual bool EventChildKey( Win* child, cevent_key* pEvent );
		virtual bool EventKey( cevent_key* pEvent );
		virtual bool EventFocus( bool recv );
		virtual bool EventActivate( bool activated, Win* w );
		virtual bool EventShow( bool show );
		virtual bool EventClose();
		virtual void EventEnterLeave( cevent* pEvent );
		virtual void EventTimer( int tid );
		virtual void EventSize( cevent_size* pEvent );
		virtual void EventMove( cevent_move* pEvent );

//	virtual void EventMouseLeave();

		virtual bool Command( int id, int subId, Win* win, void* data ); //control command or info
		virtual bool Broadcast( int id, int subId, Win* win, void* data );

		int SendBroadcast( int id, int subId, Win* win, void* data, int level = 1 );

		virtual ~Win();

		//Not for each
		void Block( WinID id ) { if ( !blockedBy ) { blockedBy = id; } }
		void Unblock( WinID id ) { if ( blockedBy == id ) { blockedBy = 0; } }
		bool UnblockTree( WinID id );


		//Fonts ...
		virtual cfont* GetChildFont( Win* w, int fontId );

		cfont* GetFont( int fontId = 0 ) { return parent ? parent->GetChildFont( this, fontId ) : SysGetFont( this, fontId ); }

		//РІС‹Р·С‹РІР°РµС‚СЃСЏ РїСЂРё РёР·РјРµРЅРµРЅРёРё С„РѕРЅС‚РѕРІ РёР»Рё РєР°РєРёС… С‚Рѕ РіР»РѕР±Р°Р»СЊРЅС‹С… РїР°СЂР°РјРµС‚СЂРѕРІ, РєРѕС‚РѕСЂС‹Рµ РјРѕРіСѓС‚ РїРѕРјРµРЅСЏС‚СЊ РїСЂРѕРїРѕСЂС†РёРё РѕРєРѕРЅ
		//РІ С„СѓРЅРєС†РёРё РЅСѓР¶РЅРѕ РїРµСЂРµСЃС‡РёС‚Р°С‚СЊ LSize
		virtual void OnChangeStyles();

		//РІС‹Р·С‹РІР°РµС‚ Сѓ РІСЃРµС… РґРѕС‡РµСЂРЅРёС… РѕРєРѕРЅ OnChangeStyles Рё RecalcLayouts
		static void StylesChanged( Win* w );

#ifndef _WIN32
		static void SetIcon( const char** xpm );
#endif
	};

	bool WinThreadSignal( int data ); //signalize window from thread


	class ClipboardText
	{
		enum { BUF_SIZE = 1024 };
		ccollect< std::vector<unicode_t>, 0x100 > list;
		int count;
		void CopyFrom( const ClipboardText& a );
	public:
		ClipboardText();
		int Count() const  { return count; }
		void Clear();
		void Append( unicode_t c );
		void AppendUnicodeStr( const unicode_t* c );
		ClipboardText( const ClipboardText& a );
		ClipboardText& operator = ( const ClipboardText& a );
		unicode_t Get( int n ) const
		{
			ASSERT( n >= 0 && n < count );
			return n >= 0 && n < count ? list.const_item( n / BUF_SIZE ).at( n % BUF_SIZE ) : 0;
		}
		unicode_t operator[]( int n ) const { return Get( n ); }
	};


	void ClipboardSetText( Win* w, ClipboardText& text );
	void ClipboardGetText( Win* w, ClipboardText* text );
	void ClipboardClear();

}; //namespace wal

namespace wal
{

	cpoint StaticTextSize( GC& gc, const unicode_t* s, cfont* font = 0 );
	void DrawStaticText( GC& gc, int x, int y, const unicode_t* s, cfont* font = 0, bool transparent = true );
	unsigned ColorTone( unsigned color, int tone /*0-255*/ );
	void DrawPixelList( GC& gc, unsigned short* s, int x, int y, unsigned color );
	void Draw3DButtonW2( GC& gc, crect r, unsigned bg, bool up );

	inline void DrawBorder( GC& gc, crect r, unsigned color )
	{
		gc.SetLine( color );
		gc.MoveTo( r.right - 1, r.top );
		gc.LineTo( r.left, r.top );
		gc.LineTo( r.left, r.bottom - 1 );
		gc.LineTo( r.right - 1, r.bottom - 1 );
		gc.LineTo( r.right - 1, r.top );
	}

	extern void UiReadFile( const sys_char_t* fileName ); //can throw
	extern void UiReadMem( const char* s ); //can throw

}; //namespace wal



#endif
