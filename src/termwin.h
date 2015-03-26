/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#pragma once

#include "nc.h"

#include "terminal.h"

extern int uiClassTerminal;

struct ScreenMarker
{
	EmulatorScreenPoint a, b;
	ScreenMarker() {}
	void Set( int r, int c ) { a.Set( r, c ); b = a; }
	void Set( const EmulatorScreenPoint& p ) { a = b = p;};
	void Reset() { a = b; }
	bool Empty() const  { return a == b; }
	void Move( int r, int c ) { b.Set( r, c ); }
	void Move( const EmulatorScreenPoint& p ) { b = p;};

	bool In( const EmulatorScreenPoint& p ) const { return ( a < b ) ? ( a <= p && p < b ) : ( b < p && p <= a ); }
};

struct TScreen
{
	int rows, cols;
	EmulatorScreenPoint cursor;
	ScreenMarker marker;

	std::vector<TermChar> buf;
	int bufSize;

	TScreen(): rows( 1 ), cols( 1 ), buf( 1 ), bufSize( 1 ) {}

	void SetSize( int r, int c )
	{
		int newSize = r * c;

		if ( bufSize < newSize )
		{
			std::vector<TermChar> p( newSize );
			buf = p;
			bufSize = newSize;
		}

		rows = r;
		cols = c;
		memset( buf.data(), 0, bufSize * sizeof( TermChar ) );
	}

	TermChar* Get( int r ) { return buf.data() + r * cols; }
	TermChar GetChar( int r, int c ) { return ( r >= 0 && r < rows && c >= 0 && c < cols ) ? Get( r )[c] : ' '; }

};

class TerminalWin : public Win
{
	friend void* RunProcessThreadFunc( void* data );

#ifdef _WIN32

#else
	Terminal _terminal;
#endif

	Layout _lo;
	ScrollBar _scroll;
	TScreen screen; //this is ONLY visible area of the screen
	int cH;
	int cW;
	crect _rect;
	int _firstRow;
	int _currentRows;

	void Reread();
	void DrawRow( wal::GC& gc, int r, int first, int last );
	void DrawChar( wal::GC& gc, int r, int c, TermChar ch );
	void CalcScroll();
	bool SetFirst( int n );

	EmulatorScreenPoint lastMousePoint;
public:
	TerminalWin( int nId, Win* parent );
	virtual void Paint( wal::GC& gc, const crect& paintRect );
	virtual void ThreadSignal( int id, int data );
	virtual void EventSize( cevent_size* pEvent );
	virtual bool EventMouse( cevent_mouse* pEvent );
	virtual void EventTimer( int tid );
	virtual ~TerminalWin();
	void Execute( Win* w, int tId, const unicode_t* cmd, const sys_char_t* path );
#ifdef _WIN32
	void Key( unsigned key, unsigned ch ) {}
	void TerminalReset( bool clearScreen = false ) { }
	void TerminalPrint( const unicode_t* s, unsigned fg, unsigned bg ) { }
#else
	void Key( unsigned key, unsigned ch ) { _terminal.Key( key, ch ); }
	void TerminalReset( bool clearScreen = false ) { _terminal.TerminalReset( clearScreen ); if ( clearScreen ) { Reread(); _firstRow = 0; CalcScroll(); MarkerClear(); } }
	void TerminalPrint( const unicode_t* s, unsigned fg, unsigned bg ) { _terminal.TerminalPrint( s, fg, bg ); }
#endif
	void Paste();
	void PageUp();
	void PageDown();
	// + up, - down
	void Scroll( int Lines );
	bool Marked() const { return !screen.marker.Empty(); }
	bool GetMarked( ClipboardText& ct );
	void MarkerClear();
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual int UiGetClassId();
	virtual void OnChangeStyles();
};
