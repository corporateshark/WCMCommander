#if defined(_WIN32) && !defined(W32CONS_H)
#define W32CONS_H
#include "nc.h"

extern int uiClassTerminal;

template <class T> class Buf2D
{
	int size;
	int rows;
	int cols;
	std::vector<T> data;

	Buf2D( const Buf2D& ) {}
public:
	Buf2D(): size( 0 ), rows( 0 ), cols( 0 ) {}
	void SetSize( int r, int c )
	{
		if ( r <= 0 || c <= 0 ) { return; }

		if ( r * c < size ) { rows = r; cols = c; return; }

		int n = r * c;
		std::vector<T> p( n );
		size = n;
		rows = r;
		cols = c;
		data = p;
	}
	int Rows() const { return rows; }
	int Cols() const { return cols; }
	T* Ptr() { return data.data(); }
	T* Line( int n ) { ASSERT( n >= 0 && n < rows ); return data.data() + cols * n; }
	T* operator []( int n ) { ASSERT( n >= 0 && n < rows ); return Line( n ); }
	~Buf2D() {}
};

inline bool operator <( const COORD& a, const COORD& b ) { return a.Y < b.Y || a.Y == b.Y && a.X < b.X; }
inline bool operator <=( const COORD& a, const COORD& b ) { return a.Y < b.Y || a.Y == b.Y && a.X <= b.X; }
inline bool operator !=( const COORD& a, const COORD& b ) { return a.Y != b.Y ||  a.X != b.X; }

struct ConsMarker
{
	COORD a, b;
	ConsMarker() { a.X = a.Y = b.X = b.Y = 0; }
	void Set( int r, int c ) { a.X = c; a.Y = r; b = a; }
	void Set( const COORD& p ) { a = b = p;};
	void Reset() { a = b; }
	bool Empty() const  { return a.X == b.X && a.Y == b.Y; }
	void Move( int r, int c ) { b.X = c; b.Y = r; }
	void Move( const COORD& p ) { b = p;};
	bool In( const COORD& p ) const { return ( a < b ) ? ( a <= p && p < b ) : ( b < p && p <= a ); }
	//bool In(const COORD &p) const { return (a < b) ? (a <= p && p < b) : (b < p && p <= a); }
};

struct CScreen
{
	COORD cursor;
	ConsMarker marker;
	Buf2D<CHAR_INFO> buf;

	CScreen() {}
	void SetSize( int r, int c )
	{
		buf.SetSize( r, c );

		if ( r > 0 && c > 0 ) { memset( buf.Ptr(), 0, r * c * sizeof( CHAR_INFO ) ); }
	}
	int Rows() const { return buf.Rows(); }
	int Cols() const { return buf.Cols(); }
	CHAR_INFO* Get( int r ) { return buf.Line( r );}
};


class W32Cons : public Win
{
	Layout _lo;
	ScrollBar _scroll;
	CScreen screen; //это ТОЛЬКО видимая область экрана
	Buf2D<CHAR_INFO> _temp;
	HANDLE stopHandle;

	int cH, cW;
	crect _rect;

	HANDLE outHandle;
	CONSOLE_SCREEN_BUFFER_INFO consLastInfo;
	bool ConsoleOk() const { return outHandle != 0; }
	bool NewConsole();
	void DetachConsole();
	bool CheckConsole();

	int _firstRow;

	void Reread();
	void DrawRow( wal::GC& gc, int r, int first, int last );
	bool DrawChanges();

	void CalcScroll();
	bool SetFirst( int n );

	COORD lastMousePoint;
public:
	W32Cons( int nId, Win* parent );
	virtual void Paint( wal::GC& gc, const crect& paintRect );
	virtual void ThreadSignal( int id, int data );
	virtual void EventSize( cevent_size* pEvent );
	virtual bool EventMouse( cevent_mouse* pEvent );
	virtual void EventTimer( int tid );
	virtual bool EventShow( bool show );
	virtual void OnChangeStyles();
	virtual int GetClassId();
	virtual ~W32Cons();
	bool Execute( Win* w, int tId, const unicode_t* cmd, const unicode_t* params, const unicode_t* path ); //const sys_char_t *path);

	void Key( cevent_key* pEvent );
	void TerminalReset( bool clearScreen = false ) { }
	void TerminalPrint( const unicode_t* s, unsigned fg, unsigned bg ) { }

	void DropConsole()
	{
		MarkerClear();
		SetEvent( stopHandle );
		DetachConsole();
		NewConsole();
		SetFirst( 0 );
	}

	void Paste();
	void PageUp();
	void PageDown();
	bool Marked() const { return !screen.marker.Empty(); }
	bool GetMarked( ClipboardText& ct );
	void MarkerClear();
	virtual bool Command( int id, int subId, Win* win, void* data );
};



#endif
