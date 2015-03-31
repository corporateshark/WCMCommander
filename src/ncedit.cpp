/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#if !defined( NOMINMAX )
#define NOMINMAX
#endif

#include "globals.h"
#include "nc.h"
#include "ncedit.h"
#include "ncwin.h"
#include "wcm-config.h"
#include "color-style.h"
#include "string-util.h"
#include "charsetdlg.h"
#include "ltext.h"
#include "globals.h"
#include "unicode_lc.h"

#include <algorithm>

int uiClassEditor = GetUiID( "Editor" );
static int uiShlDEF  = GetUiID( "DEF" );
static int uiShlKEYWORD = GetUiID( "KEYWORD" );
static int uiShlCOMMENT = GetUiID( "COMMENT" );
static int uiShlSTRING  = GetUiID( "STRING" );
static int uiShlPRE  = GetUiID( "PRE" );
static int uiShlNUM  = GetUiID( "NUM" );
static int uiShlOPER    = GetUiID( "OPER" );
static int uiShlATTN    = GetUiID( "ATTN" );
static int uiShl  = GetUiID( "shl" );
static int uiColorCtrl  = GetUiID( "ctrl-color" );
static int uiColorCursor = GetUiID( "cursor-color" );


void EditWin::OnChangeStyles()
{
	shlDEF      = UiGetColor( uiShlDEF,     uiShl, 0, 0x000000 );
	shlKEYWORD  = UiGetColor( uiShlKEYWORD, uiShl, 0, 0x00FFFF );
	shlCOMMENT  = UiGetColor( uiShlCOMMENT, uiShl, 0, 0x808080 );
	shlSTRING   = UiGetColor( uiShlSTRING,  uiShl, 0, 0xFFFF00 );
	shlPRE      = UiGetColor( uiShlPRE,     uiShl, 0, 0xFF00FF );
	shlNUM      = UiGetColor( uiShlNUM,     uiShl, 0, 0xFFFFFF );
	shlOPER  = UiGetColor( uiShlOPER,    uiShl, 0, 0xFF80FF );
	shlATTN     = UiGetColor( uiShlATTN,    uiShl, 0, 0x0000FF );
	color_text  = UiGetColor( uiColor, 0, 0, 0 );
	color_background = UiGetColor( uiBackground, 0, 0, 0xFFFFFF );
	color_mark_text   = UiGetColor( uiMarkColor, 0, 0, 0 );
	color_mark_background = UiGetColor( uiMarkBackground, 0, 0, 0xFFFFFF );
	color_ctrl  = UiGetColor( uiColorCtrl, 0, 0, 0xFF0000 );
	color_cursor   = UiGetColor( uiColorCursor, 0, 0, 0x00FFFF );

	wal::GC gc( this );
	gc.Set( GetFont() );
	cpoint p = gc.GetTextExtents( ABCString );
	charW = p.x / ABCStringLen;
	charH = p.y;

	if ( !charW ) { charW = 10; }

	if ( !charH ) { charH = 10; }

//like in EventSize !!!
	rows = editRect.Height() / charH;
	cols = editRect.Width() / charW;

	if ( rows <= 0 ) { rows = 1; }

	if ( cols <= 0 ) { cols = 1; }

	CalcScroll();

	{
		int r = ( editRect.Height() + charH - 1 ) / charH;
		int c = ( editRect.Width() + charW - 1 ) / charW;

		if ( r < 1 ) { r = 1; }

		if ( c < 1 ) { c = 1; }

		screen.Alloc( r, c );
		__RefreshScreenData();
		Invalidate();
	}

}

EditWin::EditWin( Win* parent )
	:   Win( WT_CHILD, 0, parent ),
	    _lo( 2, 2 ),
	    charset( charset_table[GetFirstOperCharsetId()] ), //EditCSLatin1),
	    vscroll( 0, this, true, false ),
	    tabSize( g_WcmConfig.editTabSize ),
	    autoIdent( g_WcmConfig.editAutoIdent ),
	    firstLine( 0 ),
	    colOffset( 0 ),
	    rows( 0 ),
	    cols( 0 ),
	    charH( 1 ),
	    charW( 1 ),
	    recomendedCursorCol( -1 ),

	    _shl( 0 ),
	    _shlLine( -1 ),

	    _changed( false )
{
	vscroll.Enable();
	vscroll.Show();
	vscroll.SetManagedWin( this );
	_lo.AddWin( &vscroll, 0, 1 );
	_lo.AddRect( &editRect, 0, 0 );
	_lo.SetColGrowth( 0 );
	SetLayout( &_lo );
	LSRange lr( 0, 10000, 1000 );
	LSize ls;
	ls.x = ls.y = lr;
	SetLSize( ls );

	OnChangeStyles();

	SetTimer( 0, 500 );
}

int EditWin::UiGetClassId() { return uiClassEditor; }

void  EditWin::CursorToScreen()
{
	int f = firstLine;

	if ( cursor.line >= firstLine + rows )
	{
		firstLine = cursor.line - rows + 1;
	}

	if ( cursor.line < firstLine )
	{
		firstLine = cursor.line;
	}

	if ( f != firstLine )
	{
		CalcScroll();
	}

	int c = GetCursorCol();
	int n = cols > 4 ? cols / 4 : 1;

	if ( colOffset + cols <= c )
	{
		colOffset = c + n - cols;

		if ( colOffset > c ) { colOffset = c; }
	}

	if ( c < colOffset ) { colOffset = c - n; }

	if ( colOffset < 0 ) { colOffset = 0; }
}

void  EditWin::CursorToScreen_forReplace()
{
	int f = firstLine;
	firstLine = cursor.line - 2;

	if ( firstLine < 0 ) { firstLine = 0; }

	int c = GetCursorCol();
	colOffset = ( c < cols ) ? 0 : c - cols;

	if ( f != firstLine )
	{
		CalcScroll();
	}
}



void EditWin::CursorLeft( bool mark )
{
	ASSERT( cursor.line >= 0 && cursor.line < text.Count() );
	ASSERT( cursor.pos >= 0 && cursor.pos <= text.Get( cursor.line ).Len() );

	recomendedCursorCol = -1;
	char* t = charset->GetPrev( text.Get( cursor.line ).Get() + cursor.pos, text.Get( cursor.line ).Get() );

	bool refresh = false;

	if ( t )
	{
		cursor.pos = t - text.Get( cursor.line ).Get();
		refresh = true;
		SendChanges();
	}
	else
	{
		if ( cursor.line > 0 )
		{
			cursor.line--;
			cursor.pos = text.Get( cursor.line ).Len();
			refresh = true;
			SendChanges();
		}
	}

	CursorToScreen();

	if ( !mark ) { marker = cursor; }

	if ( refresh ) { Refresh(); }
}

void EditWin::SetCursorPos( EditPoint p )
{
	if ( p.line >= text.Count() ) { p.line = text.Count() - 1; p.pos = 0; }

	if ( p.line < 0 ) { p.line = 0; p.pos = 0; }

	int l = text.Get( p.line ).Len();

	if ( p.pos > l ) { p.pos = l; }

	if ( p.pos < 0 ) { p.pos = 0; }

	marker = cursor = p;

	CursorToScreen();
	SendChanges();
	Refresh();
}

void EditWin::CursorHome( bool mark )
{
	recomendedCursorCol = -1;

	if ( cursor.pos > 0 )
	{
		cursor.pos = 0;
	}

	CursorToScreen();
	SendChanges();

	if ( !mark ) { marker = cursor; }

	Refresh();
}

void EditWin::CursorEnd( bool mark )
{
	recomendedCursorCol = -1;
	ASSERT( cursor.line >= 0 && cursor.line < text.Count() );

	if ( cursor.pos < text.Get( cursor.line ).Len() )
	{
		cursor.pos = text.Get( cursor.line ).Len();
	}

	CursorToScreen();
	SendChanges();

	if ( !mark ) { marker = cursor; }

	Refresh();
}

void EditWin::CursorRight( bool mark )
{
	recomendedCursorCol = -1;
	ASSERT( cursor.line >= 0 && cursor.line < text.Count() );

	if ( cursor.pos >= text.Get( cursor.line ).Len() )
	{
		if ( cursor.line + 1 < text.Count() )
		{
			cursor.line++;
			cursor.pos = 0;
			CursorToScreen();
			SendChanges();

			if ( !mark ) { marker = cursor; }

			Refresh();
		}

		return;
	}

	//bool tab;
	char* s = text.Get( cursor.line ).Get() + cursor.pos;
	char* end = text.Get( cursor.line ).Get() + text.Get( cursor.line ).Len();
	char* t = charset->GetNext( s, end );
	cursor.pos = t ? ( t - text.Get( cursor.line ).Get() ) : text.Get( cursor.line ).Len();
	CursorToScreen();
	SendChanges();

	if ( !mark ) { marker = cursor; }

	Refresh();
}


void EditWin::CursorDown( bool mark )
{
	if ( cursor.line + 1 < text.Count() )
	{
		if ( recomendedCursorCol < 0 )
		{
			recomendedCursorCol = GetColFromPos( cursor.line, cursor.pos );
		}

		cursor.pos = GetPosFromCol( cursor.line + 1, recomendedCursorCol );
		cursor.line++;
		CursorToScreen();
		SendChanges();

		if ( !mark ) { marker = cursor; }

		Refresh();
	}
}

void EditWin::CursorUp( bool mark )
{
	if ( cursor.line > 0 )
	{
		if ( recomendedCursorCol < 0 )
		{
			recomendedCursorCol = GetColFromPos( cursor.line, cursor.pos );
		}

		cursor.pos = GetPosFromCol( cursor.line - 1, recomendedCursorCol );
		cursor.line--;
		CursorToScreen();
		SendChanges();

		if ( !mark ) { marker = cursor; }

		Refresh();
	}
}

bool EditWin::StepLeft( EditPoint* p, unicode_t* c )
{
	if ( p->line < 0 || p->line >= text.Count() ) { return false; }

	if ( p->pos < 0 || p->pos > text.Get( p->line ).Len() ) { return false; }

	EditString& str = text.Get( p->line );
	char* t = charset->GetPrev( str.Get() + p->pos, str.Get() );

	if ( t )
	{
		p->pos = t - text.Get( p->line ).Get();
		*c = charset->GetChar( t, str.Get() + str.Len() );
		return true;
	}

	if ( p->line <= 0 ) { return false; }

	p->line--;
	p->pos = text.Get( p->line ).Len();
	*c = ' ';
	return true;
}

bool EditWin::StepRight( EditPoint* p, unicode_t* c )
{
	if ( p->line < 0 || p->line >= text.Count() ) { return false; }

	if ( p->pos < 0 ) { return false; }


	if ( p->pos >= text.Get( p->line ).Len() )
	{
		if ( p->line + 1 >= text.Count() ) { return false; }

		p->line++;
		p->pos = 0;
	}
	else
	{
		char* begin = text.Get( p->line ).Get();
		int len = text.Get( p->line ).Len();
		char* s = begin + p->pos;
		char* end = begin + len;
		char* t = charset->GetNext( s, end );
		p->pos = t ? ( t - begin ) : len;
	}

	EditString& str = text.Get( p->line );
	*c = str.Len() > p->pos ? charset->GetChar( str.Get() + p->pos, str.Get() + str.Len() ) : ' ';
	return true;
}

void EditWin::CursorCtrlLeft( bool mark )
{
	EditPoint p = cursor;
	unicode_t c;

	if ( !StepLeft( &p, &c ) ) { return; }

	int group = EditBuf::GetCharGroup( c );
	cursor = p;

	while ( StepLeft( &p, &c ) && EditBuf::GetCharGroup( c ) == group )
	{
		if ( cursor.pos > 0 )
		{
			cursor = p;
		}
		else
		{
			cursor.pos = 0;
		}
	}

	recomendedCursorCol = -1;
	SendChanges();
	CursorToScreen();

	if ( !mark ) { marker = cursor; }

	Refresh();
}

void EditWin::CursorCtrlRight( bool mark )
{
	EditPoint p = cursor;

	if ( p.line < 0 || p.line >= text.Count() ) { return; }

	if ( p.pos < 0 || p.pos > text.Get( p.line ).Len() ) { return; }

	EditString& str = text.Get( p.line );
	unicode_t c = str.Len() > 0 ? charset->GetChar( str.Get() + p.pos, str.Get() + str.Len() ) : ' ';

	int group = EditBuf::GetCharGroup( c );

	while ( StepRight( &p, &c ) )
	{
		if ( p.line > cursor.line )
		{
			cursor = p;
			cursor.pos = 0;
			break;
		}
		else
		{
			cursor = p;
		}

		if ( EditBuf::GetCharGroup( c ) != group ) { break; }
	}

	recomendedCursorCol = -1;
	SendChanges();
	CursorToScreen();

	if ( !mark ) { marker = cursor; }

	Refresh();
}


void EditWin::PageDown( bool mark )
{
	CursorToScreen();

	if ( recomendedCursorCol < 0 )
	{
		recomendedCursorCol = GetColFromPos( cursor.line, cursor.pos );
	}

	int lines = text.Count();

	if ( firstLine + rows >= lines )
	{
		if ( lines > 0 && cursor.line + 1 < lines )
		{
			cursor.line = lines - 1;
			cursor.pos = GetPosFromCol( cursor.line, recomendedCursorCol );
			SendChanges();

			if ( !mark ) { marker = cursor; }

			Refresh();
			return;
		}
	}

	int addLines = rows - 1;

	if ( firstLine + addLines + rows > lines )
	{
		addLines = lines - rows - firstLine;
	}

	if ( firstLine + addLines < 0 )
	{
		addLines = -firstLine;
	}

	//ASSERT(addLines >= 0);
	firstLine += addLines;

	if ( firstLine < 0 ) { firstLine = 0; }

	if ( firstLine >= lines ) { firstLine = lines - 1; }

	cursor.line += addLines;

	if ( cursor.line < 0 ) { cursor.line = 0; }

	if ( cursor.line >= lines ) { cursor.line = lines - 1; }

	cursor.pos = GetPosFromCol( cursor.line, recomendedCursorCol );
	CalcScroll();
	SendChanges();

	if ( !mark ) { marker = cursor; }

	Refresh();
}

void EditWin::PageUp( bool mark )
{
	CursorToScreen();

	if ( recomendedCursorCol < 0 )
	{
		recomendedCursorCol = GetColFromPos( cursor.line, cursor.pos );
	}

	if ( firstLine == 0 )
	{
		if ( cursor.line > 0 )
		{
			cursor.line = 0;
			cursor.pos = GetPosFromCol( cursor.line, recomendedCursorCol );
			SendChanges();

			if ( !mark ) { marker = cursor; }

			Refresh();
			return;
		}
	}

	int minus = rows - 1;

	if ( firstLine < minus )
	{
		minus = firstLine;
	}

	firstLine -= minus;
	cursor.line -= minus;
	cursor.pos = GetPosFromCol( cursor.line, recomendedCursorCol );
	CalcScroll();
	SendChanges();

	if ( !mark ) { marker = cursor; }

	Refresh();
}

void EditWin::CtrlPageDown( bool mark )
{
	if ( !text.Count() ) { return; }

	cursor.line = text.Count() - 1;
	cursor.pos = text.Get( cursor.line ).Len();

	CalcScroll();
	SendChanges();
	CursorToScreen();

	if ( !mark ) { marker = cursor; }

	Refresh();
}

void EditWin::CtrlPageUp( bool mark )
{
	cursor.line = 0;
	cursor.pos = 0;

	CalcScroll();
	SendChanges();
	CursorToScreen();

	if ( !mark ) { marker = cursor; }

	Refresh();
}


void EditWin::SelectAll()
{
	marker.Set( 0, 0 );
	int n = text.Count() - 1;
	cursor.Set( n, text.Get( n ).len );
	CalcScroll();
	CursorToScreen();
	SendChanges();
	Refresh();
}

int EditWin::GetCursorCol()
{
	return GetColFromPos( cursor.line, cursor.pos );
}

int32_t EditWin::GetCursorSymbol()
{
	if ( cursor.line >= text.Count() ) { return -1; }

	ASSERT( cursor.line >= 0 && cursor.line < text.Count() );
	EditString& str = text.Get( cursor.line );

	if ( cursor.pos >= str.len ) { return -1; }

	char* s = str.Get();
	unicode_t c = charset->GetChar( s + cursor.pos, s + str.len );
	return c;
}

bool EditWin::Undo()
{
	UndoBlock* u = undoList.GetUndo();

	if ( !u ) { return false; }

	try
	{
		for ( UndoRec* r = u->last; r; r = r->prev )
		{
			if ( _shlLine > r->line ) { _shlLine = r->line; }

			switch ( r->type )
			{

				case UndoRec::ATTR:
				{
					EditString& str = text.Get( r->line );
					str.flags = r->prevAttr;
				}
				break;

				case UndoRec::INSTEXT:
				{
					EditString& str = text.Get( r->line );
					str.Delete( r->pos, r->dataSize );
				}
				break;

				case UndoRec::DELTEXT:
				{
					EditString& str = text.Get( r->line );
					str.Insert( r->data.data(), r->pos, r->dataSize );
				}
				break;

				case UndoRec::DELLINE:
				{
					int n = r->line;
					text.Insert( n, 1, r->attr );
					text.Get( n ).Set( r->data.data(), r->dataSize );
				}
				break;

				case UndoRec::ADDLINE:
				{
					int n = r->line;
					text.Delete( n, 1 );
				}
				break;

				default:
					NCMessageBox( ( NCDialogParent* )Parent(), "bad oper", "undo", true );
					undoList.Clear();
					return false;
			}
		}

		cursor = u->beginCursor;
		marker = u->beginMarker;
		_changed = u->editorChanged;
	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}

	CursorToScreen();
	CalcScroll();
	SendChanges();
	Refresh();
	return true;
}

bool EditWin::Redo()
{
	bool chg;
	UndoBlock* u = undoList.GetRedo( &chg );

	if ( !u ) { return false; }

	try
	{
		for ( UndoRec* r = u->first; r; r = r->next )
		{
			if ( _shlLine > r->line ) { _shlLine = r->line; }

			switch ( r->type )
			{

				case UndoRec::ATTR:
				{
					EditString& str = text.Get( r->line );
					str.flags = r->attr;
				}
				break;

				case UndoRec::INSTEXT:
				{
					EditString& str = text.Get( r->line );
					str.Insert( r->data.data(), r->pos, r->dataSize );
				}
				break;

				case UndoRec::DELTEXT:
				{
					EditString& str = text.Get( r->line );
					str.Delete( r->pos, r->dataSize );
				}
				break;

				case UndoRec::DELLINE:
				{
					int n = r->line;
					text.Delete( n, 1 );
				}
				break;

				case UndoRec::ADDLINE:
				{
					int n = r->line;
					text.Insert( n, 1, r->attr );
					text.Get( n ).Set( r->data.data(), r->dataSize );
				}
				break;

				default:
					NCMessageBox( ( NCDialogParent* )Parent(), "bad oper", "Redo", true );
					undoList.Clear();
					return false;
			}
		}

		cursor = u->endCursor;
		marker = u->endMarker;
		_changed = chg;
	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}

	CursorToScreen();
	CalcScroll();
	SendChanges();
	Refresh();


	return true;
}

void EditWin::InsChar( unicode_t ch ) //!Undo
{
	char buf[0x100];
	int n = charset->SetChar( buf, ch );

	if ( n <= 0 )
	{
		return;
	}

	clPtr<UndoBlock> undoBlock = new UndoBlock( true, _changed );
	undoBlock->SetBeginPos( cursor, marker );

	try
	{

		SetChanged( cursor.line );
		ASSERT( cursor.line >= 0 && cursor.line < text.Count() );
		EditString& str = text.Get( cursor.line );
		str.Insert( buf, cursor.pos, n );
		undoBlock->InsText( cursor.line, cursor.pos, buf, n );

		cursor.pos += n;
		SendChanges();
		marker = cursor;

		undoBlock->SetEndPos( cursor, marker );
		undoList.Append( undoBlock );

	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}

	CursorToScreen();
	CalcScroll();
	SendChanges();
	Refresh();
}

bool EditWin::DelMarked() //!Undo
{
	if ( cursor == marker ) { return false; }

	clPtr<UndoBlock> undoBlock = new UndoBlock( false, _changed );
	undoBlock->SetBeginPos( cursor, marker );

	EditPoint begin, end;

	if ( cursor < marker )
	{
		begin = cursor;
		end = marker;
	}
	else
	{
		begin = marker;
		end = cursor;
	}

	SetChanged( begin.line );

	try
	{

		if ( begin.line == end.line )
		{
			EditString& line = text.Get( begin.line );
			int count = end.pos - begin.pos;
			undoBlock->DelText( begin.line, begin.pos, line.Get() + begin.pos, count );
			line.Delete( begin.pos, count );
			cursor = begin;
		}
		else
		{
			EditString& bLine = text.Get( begin.line );
			EditString& eLine = text.Get( end.line );

			if ( begin.pos < bLine.len )
			{
				undoBlock->DelText( begin.line, begin.pos, bLine.Get() + begin.pos, bLine.len - begin.pos );
				bLine.Delete( begin.pos, bLine.len - begin.pos );
			}

			if ( end.pos < eLine.len )
			{
				int count = eLine.len - end.pos;
				bLine.Insert( eLine.Get() + end.pos, begin.pos, count );
				undoBlock->InsText( begin.line, begin.pos, bLine.Get() + begin.pos, count );
			}

			undoBlock->Attr( begin.line, bLine.flags, eLine.flags );
			bLine.flags = eLine.flags;

			int delCount = end.line - begin.line;

			for ( int i = 0; i < delCount; i++ )
			{
				EditString& line = text.Get( begin.line + i + 1 );
				undoBlock->DelLine( begin.line + 1 /*!!! without i*/, line.flags, line.Get(), line.Len() );
			}

			text.Delete( begin.line + 1, delCount );
			cursor = begin;
		}

		marker = cursor;

		undoBlock->SetEndPos( cursor, marker );
		undoList.Append( undoBlock );

		CursorToScreen();
		CalcScroll();
		SendChanges();
		Refresh();

	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}

	return true;
}

inline void _ToClipboardText( char* s, int count, charset_struct* cs, ClipboardText& ctx )
{
	if ( count <= 0 )
	{
//printf("??? count= %i\n", count);
		return;
	}

	char* endPos = s + count;

	while ( s )
	{
		//bool tab;
		unicode_t c = cs->GetChar( s, endPos );
		ctx.Append( c );
		s = cs->GetNext( s, endPos );
	}
}

void EditWin::ToClipboard()
{
	if ( cursor == marker ) { return; }

	ClipboardText ctx;

	EditPoint begin, end;

	if ( cursor < marker )
	{
		begin = cursor;
		end = marker;
	}
	else
	{
		begin = marker;
		end = cursor;
	}

	if ( begin.line == end.line )
	{
		EditString& line = text.Get( begin.line );
		_ToClipboardText( line.Get() + begin.pos, end.pos - begin.pos, charset, ctx );
		//cursor = begin;
	}
	else
	{
		EditString& bLine = text.Get( begin.line );
		_ToClipboardText( bLine.Get() + begin.pos, bLine.len - begin.pos, charset, ctx );
		ctx.Append( '\n' );

		for ( int i = begin.line + 1; i < end.line; i++ )
		{
			EditString& line = text.Get( i );
			_ToClipboardText( line.Get(), line.len, charset, ctx );
			ctx.Append( '\n' );
		}

//		EditString& eLine = text.Get( end.line );

		if ( end.pos > 0 )
		{
			EditString& eLine = text.Get( end.line );
			_ToClipboardText( eLine.Get(), end.pos, charset, ctx );
		}
	}

	ClipboardSetText( this, ctx );
}

void EditWin::FromClipboard() //!Undo
{
	ClipboardText ctx;
	ClipboardGetText( this, &ctx );
	int ctxLen = ctx.Count();

	if ( ctxLen <= 0 ) { return; }

	clPtr<UndoBlock> undoBlock = new UndoBlock( false, _changed );
	undoBlock->SetBeginPos( cursor, marker );

	try
	{

		recomendedCursorCol = -1;
		SetChanged( cursor.line );

		int ctxPos = 0;

		// insert clipboard text (ctx) into editor buffer (text) line by line
		while ( ctxPos < ctxLen )
		{
			char buf[1024];
			int bufPos = 0;
			bool newline = false;

			// find next newline char
			for ( ; ctxPos < ctxLen && bufPos < 1024 - 32; ctxPos++ )
			{
				unicode_t ch = ctx[ctxPos];

				if ( ch == '\n' )
				{
					newline = true;
					ctxPos++;
					break;
				}

				bufPos += charset->SetChar( buf + bufPos, ch );
			}

			EditString& line = text.Get( cursor.line );

			if ( bufPos > 0 ) // insert clipboard fragment up to newline to cursor pos
			{
				line.Insert( buf, cursor.pos, bufPos );
				undoBlock->InsText( cursor.line, cursor.pos, buf, bufPos );
			}

			cursor.pos += bufPos;

			if ( newline ) // then we need to add new line to edit buffer
			{
				EditString& str = text.Get( cursor.line );
				text.Insert( cursor.line + 1, 1, line.flags );

				if ( cursor.pos < str.len ) // move original text that was after cursor on current line to a new line
				{
					char* textAfterCursor = str.Get() + cursor.pos;
					int lenTextAfterCursor = str.len - cursor.pos;
					undoBlock->AddLine( cursor.line + 1, line.flags, textAfterCursor, lenTextAfterCursor );
					// copy the aftercursor text to new line
					text.Get( cursor.line + 1 ).Set( textAfterCursor, lenTextAfterCursor );
					// remove the aftercursor text from current line
					str.len = cursor.pos;
					undoBlock->DelText( cursor.line, cursor.pos, textAfterCursor, lenTextAfterCursor );
				}
				else // we have added an empty line
				{
					undoBlock->AddLine( cursor.line + 1, line.flags,  0, 0 );
				}

				cursor.line++;
				cursor.pos = 0;
			}
		}

		marker = cursor;

		undoBlock->SetEndPos( cursor, marker );
		undoList.Append( undoBlock );

		CalcScroll();
		CursorToScreen();
		SendChanges();
		Refresh();

	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}
}

void EditWin::Cut()
{
	if ( cursor == marker ) { return; }

	ToClipboard();
	DelMarked();
}

void EditWin::Del( bool DeleteWord ) //!Undo
{
	ASSERT( cursor.line >= 0 && cursor.line < text.Count() );
	ASSERT( cursor.pos >= 0 && cursor.pos <= text.Get( cursor.line ).Len() );

	if ( DelMarked() )
	{
		return;
	}

	clPtr<UndoBlock> undoBlock = new UndoBlock( true, _changed );
	undoBlock->SetBeginPos( cursor, marker );

	try
	{
		EditString& str = text.Get( cursor.line );

		if ( cursor.pos < str.len )
		{
			SetChanged( cursor.line );

			char* s = str.Get() + cursor.pos;
			char* end = str.Get() + str.Len();

			int totalDelCount = 0;

			if ( DeleteWord )
			{
				EditPoint oldcursor = cursor;
				EditPoint p = cursor;

				if ( p.line < 0 || p.line >= text.Count() ) { return; }

				if ( p.pos < 0 || p.pos > text.Get( p.line ).Len() ) { return; }

				EditString& str = text.Get( p.line );
				unicode_t c = str.Len() > 0 ? charset->GetChar( str.Get() + p.pos, str.Get() + str.Len() ) : ' ';

				int group = EditBuf::GetCharGroup( c );

				while ( StepRight( &p, &c ) )
				{
					cursor = p;

					if ( EditBuf::GetCharGroup( c ) != group ) { break; }
				}

				totalDelCount = cursor.pos - oldcursor.pos;

				cursor = oldcursor;
			}
			else
			{
				char* t = charset->GetNext( s, end );
				int delCount = t ? t - s : str.len - cursor.pos;

				totalDelCount += delCount;
			}

			undoBlock->DelText( cursor.line, cursor.pos, s, totalDelCount );

			str.Delete( cursor.pos, totalDelCount );
		}
		else
		{
			if ( cursor.line + 1 < text.Count() )
			{
				SetChanged( cursor.line );

				EditString& str_1 = text.Get( cursor.line + 1 );

				str.Insert( str_1.Get(), cursor.pos, str_1.len );
				undoBlock->InsText( cursor.line, cursor.pos, str_1.Get(), str_1.len );

				undoBlock->Attr( cursor.line, str.flags, str_1.flags );
				str.flags = str_1.flags;

				undoBlock->DelLine( cursor.line + 1, str_1.flags, str_1.Get(), str_1.len );
				text.Delete( cursor.line + 1, 1 );
			}
		}

		marker = cursor;

		undoBlock->SetEndPos( cursor, marker );
		undoList.Append( undoBlock );

		CalcScroll();
		SendChanges();
		Refresh();

	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}

}

void EditWin::Backspace( bool DeleteWord ) //!Undo
{
	ASSERT( cursor.line >= 0 && cursor.line < text.Count() );
	ASSERT( cursor.pos >= 0 && cursor.pos <= text.Get( cursor.line ).Len() );

	recomendedCursorCol = -1;

	if ( DelMarked() )
	{
		return;
	}

	clPtr<UndoBlock> undoBlock = new UndoBlock( true, _changed );
	undoBlock->SetBeginPos( cursor, marker );

	try
	{

		EditString& str = text.Get( cursor.line );

		if ( cursor.pos > 0 )
		{
			SetChanged( cursor.line );

			if ( DeleteWord )
			{
				EditPoint oldcursor = cursor;
				EditPoint p = cursor;
				unicode_t c;

				if ( !StepLeft( &p, &c ) ) { return; }

				int group = EditBuf::GetCharGroup( c );
				cursor = p;

				while ( StepLeft( &p, &c ) && EditBuf::GetCharGroup( c ) == group )
				{
					if ( cursor.line == p.line )
					{
						cursor = p;
					}
					else
					{
						// just stay at the beginning of this line
						cursor.pos = 0;
						break;
					}
				}

				int totalDelCount = oldcursor.pos - cursor.pos;

				char* s = str.Get() + cursor.pos;

				undoBlock->DelText( cursor.line, cursor.pos, s, totalDelCount );

				str.Delete( cursor.pos, totalDelCount );
			}
			else
			{
				char* s = str.Get() + cursor.pos;
				char* t = charset->GetPrev( s, str.Get() );

				if ( t )
				{
					undoBlock->DelText( cursor.line, t - str.Get(), t, s - t );
					str.Delete( t - str.Get(), s - t );
					cursor.pos -= s - t;
				}
				else
				{
					undoBlock->DelText( cursor.line, 0, str.Get(), cursor.pos );
					str.Delete( 0, cursor.pos );
					cursor.pos = 0;
				}
			}
		}
		else
		{
			if ( cursor.line > 0 )
			{
				SetChanged( cursor.line - 1 );
				EditString& str_1 = text.Get( cursor.line - 1 );
				int l = str_1.len;


				undoBlock->InsText( cursor.line - 1, str_1.len, str.Get(), str.Len() ); //while unchanged str_1.len
				str_1.Insert( str.Get(), str_1.len, str.len );

				undoBlock->Attr( cursor.line - 1, str_1.flags, str.flags );
				str_1.flags = str.flags;

				undoBlock->DelLine( cursor.line, str.flags, str.Get(), str.Len() );
				text.Delete( cursor.line, 1 );

				cursor.line--;
				cursor.pos = l;
			}
		}

		marker = cursor;

		undoBlock->SetEndPos( cursor, marker );
		undoList.Append( undoBlock );

		CursorToScreen();
		SendChanges();
		Refresh();

	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}

}

char* EditWin::Ident( EditString& str )
{
	if ( !str.len )
	{
		return 0;
	}

	char* s = str.Get();
	char* end = s + str.len;

	while ( true )
	{
		unicode_t c = charset->GetChar( s, end );

		if ( c != ' '  && c != '\t' )
		{
			break;
		}

		//bool tab;
		s = charset->GetNext( s, end );

		if ( !s )
		{
			return 0;
		}
	}

	return s;
}

void EditWin::SetCharset( charset_struct* cs )
{
	ASSERT( cursor.line >= 0 && cursor.line < text.Count() );
	ASSERT( cursor.pos >= 0 && cursor.pos <= text.Get( cursor.line ).Len() );
	charset = cs;
	CursorRight( false );
	CursorLeft( false );
	SendChanges();
	marker = cursor;
	EnableShl( g_WcmConfig.editShl );
	Refresh();
}

void EditWin::DeleteLine() //!Undo
{
	ASSERT( cursor.line >= 0 && cursor.line < text.Count() );
	ASSERT( cursor.pos >= 0 && cursor.pos <= text.Get( cursor.line ).Len() );
	recomendedCursorCol = -1;

	clPtr<UndoBlock> undoBlock = new UndoBlock( false, _changed );
	undoBlock->SetBeginPos( cursor, marker );

	EditString& str = text.Get( cursor.line );

	try
	{
		if ( cursor.line + 1 >= text.Count() )
		{
			if ( str.len == 0 )
			{
				return;
			}

			undoBlock->DelText( cursor.line, 0, str.Get(), str.len );
			str.len = 0; //!!!
			cursor.pos = 0;
		}
		else
		{
			undoBlock->DelLine( cursor.line, str.flags, str.Get(), str.len );
			text.Delete( cursor.line, 1 );
			cursor.pos = 0;
		}

		SetChanged( cursor.line - 1 );

		marker = cursor;

		undoBlock->SetEndPos( cursor, marker );
		undoList.Append( undoBlock );

		CursorToScreen();
		SendChanges();
		Refresh();
	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}
}

void EditWin::Enter() //!Undo
{
	ASSERT( cursor.line >= 0 && cursor.line < text.Count() );
	ASSERT( cursor.pos >= 0 && cursor.pos <= text.Get( cursor.line ).Len() );

	recomendedCursorCol = -1;

	clPtr<UndoBlock> undoBlock = new UndoBlock( true, _changed );
	undoBlock->SetBeginPos( cursor, marker );

	try
	{
		SetChanged( cursor.line );

		EditString& str = text.Get( cursor.line );
		text.Insert( cursor.line + 1, 1, str.flags );

		if ( cursor.pos < str.len )
		{
			text.Get( cursor.line + 1 ).Set( str.Get() + cursor.pos, str.len - cursor.pos );
			undoBlock->AddLine( cursor.line + 1, str.flags, str.Get() + cursor.pos, str.len - cursor.pos );
			undoBlock->DelText( cursor.line, cursor.pos, str.Get() + cursor.pos, str.len - cursor.pos );
			str.len = cursor.pos;
		}
		else
		{
			undoBlock->AddLine( cursor.line + 1, str.flags, 0, 0 );
		}

		cursor.line++;
		cursor.pos = 0;

		if ( autoIdent )
		{
			for ( int i = cursor.line - 1; i >= 0; i-- )
			{
				char* s = Ident( text.Get( i ) );

				if ( s )
				{
					char* p = text.Get( i ).Get();
					EditString& cstr = text.Get( cursor.line );
					char* t = Ident( cstr );

					if ( t )
					{
						undoBlock->DelText( cursor.line, 0, cstr.Get() , t - cstr.Get() );
						cstr.Delete( 0, t - cstr.Get() );
						cursor.pos = 0;
					}

					undoBlock->InsText( cursor.line, 0, p, s - p );
					cstr.Insert( p, 0, s - p );
					cursor.pos = s - p;
					break;
				}
			}
		}

		marker = cursor;

		undoBlock->SetEndPos( cursor, marker );
		undoList.Append( undoBlock );

		CursorToScreen();
		SendChanges();
		Refresh();

	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}

}

sEditorScrollCtx EditWin::GetScrollCtx() const
{
	sEditorScrollCtx Ctx;
	Ctx.m_Point = this->GetCursorPos();
	Ctx.m_FirstLine = firstLine;
	return Ctx;
}

void EditWin::SetScrollCtx( const sEditorScrollCtx& Ctx )
{
	recomendedCursorCol = -1;
	int MaxLine = std::max( 0, text.Count() - rows );
	SetCursorPos( Ctx.m_Point );
	firstLine = Ctx.m_FirstLine;
	firstLine = std::min( firstLine, MaxLine );
	colOffset = 0;
	marker = cursor;
	SendChanges();
	Refresh();
}

void EditWin::CalcScroll()
{
	ScrollInfo vsi;
	vsi.m_PageSize = rows;
	vsi.m_Size = text.Count();
	vsi.m_Pos = firstLine;
	bool vVisible = vscroll.IsVisible();
	vscroll.Command( CMD_SCROLL_INFO, SCMD_SCROLL_VCHANGE, this, &vsi );

	if ( vVisible != vscroll.IsVisible() )
	{
		this->RecalcLayouts();
	}
}


bool EditWin::Command( int id, int subId, Win* win, void* data )
{
	if ( id != CMD_SCROLL_INFO )
	{
		return false;
	}

	int n = firstLine;

	switch ( subId )
	{
		case SCMD_SCROLL_LINE_UP:
			n--;
			break;

		case SCMD_SCROLL_LINE_DOWN:
			n++;
			break;

		case SCMD_SCROLL_PAGE_UP:
			n -= rows;
			break;

		case SCMD_SCROLL_PAGE_DOWN:
			n += rows;
			break;

		case SCMD_SCROLL_TRACK:
			n = ( ( int* )data )[0];
			break;
	}

	if ( n + rows > text.Count() )
	{
		n = text.Count() - rows;
	}

	if ( n < 0 ) { n = 0; }

	if ( n != firstLine )
	{
		firstLine = n;
		CalcScroll();
		SendChanges();
		Refresh();
	}

	return true;
}

bool EditWin::Broadcast( int id, int subId, Win* win, void* data )
{
	if ( id == ID_CHANGED_CONFIG_BROADCAST )
	{
		autoIdent = g_WcmConfig.editAutoIdent;
		tabSize = g_WcmConfig.editTabSize;

		if ( IsVisible() ) { Invalidate(); }

		return true;
	}

	return false;
}


unsigned EditWin::ColorById( int id )
{
	switch ( id )
	{
		case COLOR_DEF_ID:
			return shlDEF;

		case COLOR_KEYWORD_ID:
			return shlKEYWORD;

		case COLOR_COMMENT_ID:
			return shlCOMMENT;

		case COLOR_STRING_ID:
			return shlSTRING;

		case COLOR_PRE_ID:
			return shlPRE;

		case COLOR_NUM_ID:
			return shlNUM;

		case COLOR_OPER_ID:
			return shlOPER;

		case COLOR_ATTN_ID:
			return shlATTN;
	}

//printf("color id ?(%i)\n", id);
	return shlDEF;
}

#ifdef _WIN32
#include "w32util.h"
#endif

static bool CheckAsciiSymbol( charset_struct* cs, unsigned char c )
{
	char buf[64];
	int n = cs->SetChar( buf, c );
	return n == 1 && buf[0] == c;
}

static bool CheckAscii( charset_struct* cs )
{
	for ( int i = 1; i < 128; i++ )
		if ( !CheckAsciiSymbol( cs, i ) )
		{
			return false;
		}

	return true;
}

void EditWin::EnableShl( bool on )
{
	_shl = 0;
	_shlLine = -1;
	_shlConf = 0;

	if ( on )
	{

		if ( !CheckAscii( charset ) )
		{
			return;
		}

		std::unordered_map< std::string, int > colors;
		colors["DEF"   ] = COLOR_DEF_ID;
		colors["KEYWORD"] = COLOR_KEYWORD_ID;
		colors["COMMENT"] = COLOR_COMMENT_ID;
		colors["STRING"   ] = COLOR_STRING_ID;
		colors["PRE"   ] = COLOR_PRE_ID;
		colors["NUM"   ] = COLOR_NUM_ID;
		colors["OPER"  ] = COLOR_OPER_ID;
		colors["ATTN"  ] = COLOR_ATTN_ID;

		std::vector<char> firstLine;

		if ( text.Count() > 0 )
		{
			EditString& str = text.Get( EditList::Pos() );
			int len = str.len;

			firstLine.resize( len + 1 );

			char* s = firstLine.data();

			if ( len > 0 )
			{
				memcpy( firstLine.data(), str.Get(), len );
			}

			firstLine[len] = 0;

			char* ns = 0;

			for ( ; *s; s++ )
				if ( *s <= 0 || *s > ' ' )
				{
					ns = s;
				}

			if ( ns ) { ns[1] = 0; }
		}

		_shlConf = new SHL::ShlConf();
#ifdef _WIN32
		std::vector<wchar_t> path = GetAppPath();
		_shlConf->Parze( carray_cat<wchar_t>( GetAppPath().data(), utf8_to_sys( "\\shl\\config.cfg" ).data() ).data() );
#else
		_shlConf->Parze( ( sys_char_t* ) UNIX_CONFIG_DIR_PATH "/shl/config.cfg" );
#endif
		_shlLine = -1;
		//need to make not utf8 but current cs
		_shl = _shlConf->Get( _path.GetUnicode() , utf8_to_unicode( firstLine.data() ).data(), colors );
	}

	__RefreshScreenData();
	Invalidate();
}

void EditWin::Load( clPtr<FS> fs, FSPath& path, MemFile& f )
{
	Clear();
	text.Load( f );
	_fs = fs;
	_path = path;
	CalcScroll();

	_changed = false;

	EnableShl( g_WcmConfig.editShl );
}

void EditWin::Clear()
{
	text.Clear();
	undoList.Clear();
	cursor.Set( 0, 0 );
	marker = cursor;
	firstLine = 0;
	colOffset = 0;
	_path.Clear();
	_fs = 0;
	recomendedCursorCol = -1;
}

void EditWin::Save( MemFile& f )
{
	f.Clear();
	text.Save( f );
	if ( g_WcmConfig.editClearHistoryAfterSaving ) undoList.Clear();
//	changed = false;
}

void EditWin::SetChanged( int minLine )
{
	_changed = true;

	if ( _shlLine > minLine ) { _shlLine = minLine; }
}

void EditWin::RefreshShl( int n )
{
	if ( _shl && n > _shlLine && text.Count() > 0 )
	{
		int first = _shlLine;

		int count = text.Count();

		if ( first < 0 )
		{
			text.Get( 0 ).shlId = _shl->GetStartId();
			first = 0;
			_shlLine = 0;
		}

		int last = n;

		if ( last >= count ) { last = count - 1; }

		if ( first < last )
		{
			EditList::Pos pos( first );
			int statId = text.Get( pos ).shlId;

			for ( ; pos <= last ; pos.Inc() )
			{
				EditString& str   = text.Get( pos );
				char* begin = str.Get(), *s = begin, *end = s + str.Len();

				if ( pos != first ) { str.shlId = statId; }

				if ( pos != last )
				{
					statId = _shl->ScanLine( ( unsigned char* )begin, ( unsigned char* )end, statId );
				}
			}
		}

		_shlLine = last;
	}
}

void EditWin::__RefreshScreenData()
{
	if ( screen.Rows() <= 0 || screen.Cols() <= 0 ) { return; }

	//EditorColors colors = *editorColors;

	int r;

	for ( r = 0; r < screen.Rows(); r++ )
	{
		int line = r + firstLine;

		if ( line < 0 || line >= text.Count() )
		{
			screen.Set( r, 0, screen.Cols(), ' ', 0, color_background );
			continue;
		}

		EditString& str   = text.Get( line );
		char* begin = str.Get(), *s = begin, *end = s + str.Len();
		bool tab = charset->IsTab( s, end );
		int col = 0;

		if ( str.len )
		{

//TEMP
			std::vector<char> colId( str.len + 1 );
			memset( colId.data(), -1, str.len + 1 );

			if ( _shl )
			{
				RefreshShl( line );
				_shl->ScanLine( ( unsigned char* )str.Get(), colId.data(), str.len, str.shlId );
			}

			int ce = col;

			while ( s )
			{
				ce = ( tab ) ? col + tabSize - col % tabSize : col + 1;

				if ( ce > colOffset ) { break; }

				s = charset->GetNext( s, end );
				tab = charset->IsTab( s, end );
				col = ce;
			}

			while ( s && col - colOffset < screen.Cols() )
			{
				bool marked = InMark( EditPoint( line, s - begin ) );

				if ( s >= end )
				{
					printf( "s>=end (%i)\n", int( s - end ) );
				}

				unsigned COL =    _shl ? ColorById( colId[s - begin] ) : color_text;

				if ( tab )
				{
					ce = col + tabSize - col % tabSize;


					screen.Set( r, col - colOffset, ce, ' ', 0, marked ? color_mark_background : color_background );
					col = ce;
				}
				else
				{
					unicode_t ch = charset->GetChar( s, end );
					screen.Set( r, col - colOffset, col - colOffset + 1, ( ch < 32 /*|| ch>=0x80 && ch< 0xA0*/ ) ? '.' : ch,
					            ( ch < 32 /*|| ch>=0x80 && ch< 0xA0*/ ) ? 0xFF3030 : ( marked ? color_mark_text : /*colors.fg*/ COL ),
					            marked ? color_mark_background : color_background );
					col++;
				}

				s = charset->GetNext( s, end );
				tab = charset->IsTab( s, end );
			}
		}

		if ( col - colOffset < screen.Cols() )
		{
			screen.Set( r, col - colOffset, screen.Cols(), ' ', 0, InMark( EditPoint( line, end - begin ) ) ? color_mark_background : color_background );
		}
	}

	screen.SetCursor( cursor.line - firstLine, GetCursorCol() - colOffset );
}


#define PBSIZE (0x100)

void EditWin::__DrawChanges()
{
	if ( screen.Rows() <= 0 && screen.Cols() <= 0 ) { return; }

	wal::GC gc( this );
	gc.Set( GetFont() );

	if ( screen.prevCursor != screen.cursor &&
	     screen.prevCursor.line >= 0 && screen.prevCursor.line < screen.Rows() &&
	     screen.prevCursor.pos >= 0 && screen.prevCursor.pos < screen.Cols() )
	{
		screen.Line( screen.prevCursor.line )[screen.prevCursor.pos].changed = true;
	}

	for ( int r = 0; r < screen.Rows(); r++ )
	{
		int c = 0;
		EditScreenChar* p = screen.Line( r );

		if ( !p ) { continue; }

		EditScreenChar* pe = p + screen.Cols();
		int y = editRect.top + r * charH;

		while ( p < pe )
		{
			if ( !p->changed )
			{
				while ( p < pe && !p->changed ) { p++; c++; }

				continue;
			}

			int x = editRect.left + c * charW;

			if ( p->ch == ' ' )
			{
				EditScreenChar* t = p + 1;
				p->changed = false;

				for ( ; t < pe && t->ch == ' ' && t->changed && t->bColor == p->bColor; t++ )
				{
					t->changed = false;
				}

				int n = t - p;
				gc.SetFillColor( p->bColor );
				gc.FillRect( crect( x, y, x + n * charW, y + charH ) );
				p = t;
				c += n;
				continue;
			}

			EditScreenChar* t = p + 1;
			p->changed = false;

			for ( ; t < pe && t->changed  && t->bColor == p->bColor && t->fColor == p->fColor; t++ )
			{
				t->changed = false;
			}

			int n = t - p;
			gc.SetFillColor( p->bColor );
			gc.SetTextColor( p->fColor );

			while ( n > 0 )
			{
				unicode_t buf[PBSIZE];
				int count = n > PBSIZE ? PBSIZE : n;

				for ( int i = 0; i < count; i++ ) { buf[i] = p[i].ch; }

				gc.TextOutF( x, y, buf, count );
				n -= count;
				p += count;
				x += count * charW;
				c += count;
			}
		}
	}


//redraw cursor always //  if (screen.prevCursor != screen.cursor)
	DrawCursor( gc );
}

void EditWin::DrawCursor( wal::GC& gc )
{
	int curR = screen.cursor.line;
	int curC = screen.cursor.pos;

	if ( curR >= 0 && curR < screen.Rows() && curC >= 0 && curC < screen.Cols() )
	{
		int y = editRect.top + curR * charH;
		int x = editRect.left + curC * charW;
		gc.SetFillColor( color_cursor/*0xFFFF00*/ );
		gc.FillRectXor( GetCursorRect( x, y ) );
	}

	screen.prevCursor = screen.cursor;
}

void EditWin::Paint( wal::GC& gc, const crect& paintRect )
{
	int r1 = ( paintRect.top - editRect.top ) / charH;
	int r2 = ( paintRect.bottom - editRect.top + charH - 1 ) / charH;
	int c1 = ( paintRect.left - editRect.left ) / charW;
	int c2 = ( paintRect.right - editRect.left + charW - 1 ) / charW;

	if ( r1 < 0 ) { r1 = 0; }

	if ( r2 > screen.Rows() ) { r2 = screen.Rows(); }

	if ( c1 < 0 ) { c1 = 0; }

	if ( c2 > screen.Cols() ) { c2 = screen.Cols(); }

	if ( r1 >= r2 || c1 >= c2 ) { return; }

	gc.Set( GetFont() );

	for ( int r = r1; r < r2; r++ )
	{
		int c = c1;
		EditScreenChar* p = screen.Line( r ) + c;

		if ( !p ) { continue; }

		EditScreenChar* pe = p + c2 - c1;
		int y = editRect.top + r * charH;

		while ( p < pe )
		{
			int x = editRect.left + c * charW;

			if ( p->ch == ' ' )
			{
				EditScreenChar* t = p + 1;
				p->changed = false;

				for ( ; t < pe && t->ch == ' ' && t->bColor == p->bColor; t++ )
				{
					t->changed = false;
				}

				int n = t - p;
				gc.SetFillColor( p->bColor );
				gc.FillRect( crect( x, y, x + n * charW, y + charH ) );
				p = t;
				c += n;
				continue;
			}

			EditScreenChar* t = p + 1;
			p->changed = false;

			for ( ; t < pe && t->bColor == p->bColor && t->fColor == p->fColor; t++ )
			{
				t->changed = false;
			}

			int n = t - p;
			gc.SetFillColor( p->bColor );
			gc.SetTextColor( p->fColor );

			while ( n > 0 )
			{
				unicode_t buf[PBSIZE];
				int count = n > PBSIZE ? PBSIZE : n;

				for ( int i = 0; i < count; i++ ) { buf[i] = p[i].ch; }

				gc.TextOutF( x, y, buf, count );
				n -= count;
				p += count;
				x += count * charW;
				c += count;
			}
		}
	}

	DrawCursor( gc );
}

crect EditWin::GetCursorRect( int x, int y ) const
{
	return crect( x, y + charH - charH / 5, x + charW, y + charH );
}

void EditWin::SetPath( clPtr<FS> fs, FSPath& p )
{
	_fs = fs;
	_path = p;
}

int EditWin::GetCharsetId()
{
	return charset->id;
}

void EditWin::NextCharset()
{
	SetCharset( charset_table[GetNextOperCharsetId( charset->id )] );
}

void EditWin::SetCharset( int n )
{
	SetCharset( charset_table[n] );
}

bool EditWin::EventKey( cevent_key* pEvent )
{
	if ( pEvent->Type() == EV_KEYDOWN )
	{
		bool shift = ( pEvent->Mod() & KM_SHIFT ) != 0;
		bool ctrl = ( pEvent->Mod() & KM_CTRL ) != 0;
		bool alt = ( pEvent->Mod() & KM_ALT ) != 0;

		if ( ctrl )
		{
			switch ( pEvent->Key() )
			{
				case VK_UP:
					if (pEvent->IsFromMouseWheel())
					{
						if ( g_MainWin ) g_MainWin->IncreaseFontSize(NCWin::EDIT);
						return true;
					}
					break;

				case VK_DOWN:
					if (pEvent->IsFromMouseWheel())
					{
						if ( g_MainWin ) g_MainWin->DecreaseFontSize(NCWin::EDIT);
						return true;
					}
					break;

				case VK_0:
				case VK_1:
				case VK_2:
				case VK_3:
				case VK_4:
				case VK_5:
				case VK_6:
				case VK_7:
				case VK_8:
				case VK_9:
				{
					int Slot = pEvent->Key() - VK_0;
					bool LeftCtrl = ( pEvent->ExtMod() & EXT_KM_LCTRL ) != 0;
					if ( LeftCtrl )
					{
						sEditorScrollCtx Ctx;
						if (m_ScrollCtxStorage.Get(Slot, &Ctx)) this->SetScrollCtx(Ctx);
					}
					else
					{
						sEditorScrollCtx Ctx = this->GetScrollCtx();
						m_ScrollCtxStorage.Put(Slot, &Ctx);
					}
					return true;
				}

				case VK_Y:
					DeleteLine();
					return true;

				case VK_X:
					Cut();
					return true;

				case VK_C:
					ToClipboard();
					return true;

				case VK_V:
					DelMarked();
					FromClipboard();
					return true;

				case VK_Z:
					if ( shift ) { Redo(); }
					else { Undo(); }

					return true;
			}
		}


		switch ( pEvent->Key() )
		{
			case VK_RIGHT:
				if ( ctrl )
				{
					CursorCtrlRight( shift );
				}
				else
				{
					CursorRight( shift );
				}

				break;

			case VK_LEFT:
				if ( ctrl )
				{
					CursorCtrlLeft( shift );
				}
				else
				{
					CursorLeft( shift );
				}

				break;

			case VK_DOWN:
#if defined( __APPLE__)
				if ( ctrl ) { PageDown( shift ); }
				else
#endif
					CursorDown( shift );

				break;

			case VK_UP:
#if defined( __APPLE__)
				if ( ctrl ) { PageUp( shift ); }
				else
#endif
					CursorUp( shift );

				break;

			case VK_HOME:
				if ( ctrl ) { CtrlPageUp( shift ); }
				else { CursorHome( shift ); }

				break;

			case VK_END:
				if ( ctrl ) { CtrlPageDown( shift ); }
				else { CursorEnd( shift ); }

				break;

			case VK_NEXT:
				if ( ctrl ) { CtrlPageDown( shift ); }
				else { PageDown( shift ); }

				break;

			case VK_PRIOR:
				if ( ctrl ) { CtrlPageUp( shift ); }
				else { PageUp( shift ); }

				break;

			case VK_DELETE:
				if ( ctrl ) { Del( true ); return true; }

				if ( shift ) { Cut(); return true; }

				Del( false );
				break;

			case VK_BACK:
				if ( alt )
				{
					Undo();
				}
				else
				{
					Backspace( ctrl );
				}

				break;

			case VK_NUMPAD_RETURN:
			case VK_RETURN:
				DelMarked();
				Enter();
				break;

			case VK_INSERT:
				if ( shift )
				{
					DelMarked();
					FromClipboard();
					return true;
				}

				if ( ctrl ) { ToClipboard(); return true; }

				break;

			default:
				if ( ctrl && pEvent->Key() == VK_A ) { SelectAll(); break; }

				if ( pEvent->Char() == '\t' || pEvent->Char() >= ' ' )
				{
					DelMarked();
					InsChar( pEvent->Char() );
					return true;
				}

				return false;
		};

		return true;
	}

	return false;
}

void EditWin::EventSize( cevent_size* pEvent )
{
	Win::EventSize( pEvent );
	rows = editRect.Height() / charH;
	cols = editRect.Width() / charW;

	if ( rows <= 0 ) { rows = 1; }

	if ( cols <= 0 ) { cols = 1; }

	CalcScroll();

	{
		int r = ( editRect.Height() + charH - 1 ) / charH;
		int c = ( editRect.Width() + charW - 1 ) / charW;
		screen.Alloc( r, c );
		__RefreshScreenData();
		Invalidate();
	}
}


int EditWin::GetColFromPos( int line, int pos )
{
	if ( line < 0 || line >= text.Count() ) { return 0; }

	EditString& str = text.Get( line );

	char* s = str.Get(), *end = s + str.Len();

	bool tab = charset->IsTab( s, end );
	int col = 0;

	while ( s )
	{
		if ( pos <= 0 ) { break; }

		col += tab ? tabSize - col % tabSize : 1;
		char* t = charset->GetNext( s, end );
		tab = charset->IsTab( t, end );

		if ( t ) { pos -= t - s; }

		s = t;
	}

	return col;
}


int EditWin::GetPosFromCol( int line, int nCol )
{
	if ( line < 0 || line >= text.Count() ) { return 0; }

	EditString& str = text.Get( line );

	char* s = str.Get(), *end = s + str.Len();

	bool tab = charset->IsTab( s, end );

	int pos = 0, col = 0;

	while ( s )
	{
		int step = ( tab ? tabSize - ( col % tabSize ) : 1 );

		if ( step > nCol - col )
		{
			break;
		}

		col += step;
		char* t = charset->GetNext( s, end );
		tab = charset->IsTab( t, end );

		if ( t )
		{
			pos += t - s;
		}
		else
		{
			pos = str.Len();
		}

		s = t;
	}

	return pos;
}

void EditWin::SetCursor( cpoint p, bool mark )
{
	int r = ( p.y - editRect.top ) / charH;
	int c = ( p.x - editRect.left ) / charW;
	r += firstLine;
	c += colOffset;

	if ( p.y < 0 ) { r--; }

	if ( p.x < 0 ) { c--; }

	int line = r;

	if ( line >= text.Count() ) { line = text.Count() - 1; }

	if ( line < 0 ) { line = 0; }

//	EditString& str = text.Get( line );
	int pos = GetPosFromCol( line, c );
	cursor.Set( line, pos );

	if ( !mark ) { marker = cursor; }

	CursorToScreen();
	SendChanges();
	Refresh();
}


void EditWin::EventTimer( int id )
{
	if ( id == 0 )
	{
		wal::GC gc( this );
		DrawCursor( gc );
		return;
	}

	SetCursor( lastMousePoint, true );
}


bool EditWin::EventMouse( cevent_mouse* pEvent )
{
	lastMousePoint = pEvent->Point();

	switch ( pEvent->Type() )
	{
		case EV_MOUSE_MOVE:
			if ( IsCaptured() )
			{
				SetCursor( lastMousePoint, true );
			}

			break;

		case EV_MOUSE_PRESS:
		{
			if ( pEvent->Button() == MB_X1 )
			{
				PageUp( false );
				break;
			}

			if ( pEvent->Button() == MB_X2 )
			{
				PageDown( false );
				break;
			}

			if ( pEvent->Button() != MB_L )
			{
				break;
			}

			SetCapture();
			SetCursor( lastMousePoint, false );

			SetTimer( 1, 100 );
		}
		break;


		case EV_MOUSE_RELEASE:
			if ( pEvent->Button() != MB_L )
			{
				break;
			}

			DelTimer( 1 );

			ReleaseCapture();
			break;
	};

	return false;
}

bool EditWin::Search( const unicode_t* arg, bool sens )
{
	std::vector<unicode_t> search = new_unicode_str( arg );

	if ( !sens )
		for ( unicode_t* u = search.data(); *u; u++ ) { *u = UnicodeLC( *u ); }

	int line =  cursor.line;

	EditString& str = text.Get( line );
	char* begin = str.Get(), *end = begin + str.Len();
	char* s = begin + cursor.pos;

	//bool tab;
	if ( s < end )
	{
		s = charset->GetNext( s, end );
	}

	while ( true )
	{
		while ( s )
		{
			const unicode_t* sPtr = search.data();

			for ( char* p = s; ; p = charset->GetNext( p, end ), sPtr++ )
			{
				if ( !*sPtr )
				{
					cursor.Set( line, s - begin );
					marker = cursor;
					CursorToScreen();
					SendChanges();
					Refresh();
					return true;
				}

				if ( !p ) { break; }

				unicode_t c = charset->GetChar( p, end );

				if ( sens )
				{
					if ( c != *sPtr ) { break; }
				}
				else
				{
					if ( UnicodeLC( c ) != *sPtr ) { break; }
				}
			}

			s = charset->GetNext( s, end );
		}

		line++;

		if ( line >= text.Count() ) { break; }

		EditString& str = text.Get( line );
		s = begin = str.Get();
		end = begin + str.Len();
	}

	return false;
}

#define CMD_REPLACE 1000
#define CMD_ALL 1001
#define CMD_SKIP 1002

static ButtonDataNode bReplaceAllSkipCancel[] = { {"&Replace", CMD_REPLACE}, { "&All", CMD_ALL}, { "&Skip", CMD_SKIP}, {"&Cancel", CMD_CANCEL}, {0, 0}};

bool EditWin::Replace( const unicode_t* from, const unicode_t* to, bool sens )
{
	std::vector<unicode_t> search = new_unicode_str( from );

	if ( !sens )
		for ( unicode_t* u = search.data(); *u; u++ ) { *u = UnicodeLC( *u ); }

	ccollect<char> rep;
	charset_struct* cs = charset;

	for ( ; *to; to++ )
	{
		char buf[32];
		int n = cs->SetChar( buf, *to );

		for ( int i = 0; i < n; i++ )
		{
			rep.append( buf[i] );
		}
	}

	int line =  cursor.line;

	EditString& str = text.Get( line );
	char* begin = str.Get(), *end = begin + str.Len();
	char* s = begin + cursor.pos;

	if ( s < end )
	{
		s = charset->GetNext( s, end );
	}

	bool all = false;
	int foundCount = 0;

	clPtr<UndoBlock> undoBlock = new UndoBlock( false, _changed );
	undoBlock->SetBeginPos( cursor, marker );

	try
	{
		while ( true )
		{
			while ( s )
			{

restart:

				const unicode_t* sPtr = search.data();

				for ( char* p = s; ; p = charset->GetNext( p, end ), sPtr++ )
				{
					if ( !*sPtr )
					{
						char* blockEnd = ( p ? p : end );

						foundCount++;

						if ( !all )
						{
							marker.Set( line, s - begin );
							cursor.Set( line, blockEnd - begin );
							CursorToScreen_forReplace();
							SendChanges();
							Refresh();

							int ret = NCMessageBox( ( NCDialogParent* )Parent(), _LT( "Replace" ), _LT( "Replace it?" ), false, bReplaceAllSkipCancel );

							if ( ret == CMD_ALL ) { all = true; }

							if ( ret == CMD_SKIP ) { continue; }

							if ( ret != CMD_ALL && ret != CMD_REPLACE )
							{
								marker = cursor;

								//!!
								undoBlock->SetEndPos( cursor, marker );
								undoList.Append( undoBlock );

								CursorToScreen();
								SendChanges();
								Refresh();
								return true;
							}
						}

						{
							//replace
							int pos = s - begin;
							int size = blockEnd - s;
							SetChanged( line );

							EditString& str = text.Get( line );

							undoBlock->DelText( line, pos, str.Get() + pos, size );
							str.Delete( pos, size );

							if ( rep.count() > 0 )
							{
								str.Insert( rep.ptr(), pos, rep.count() );
								undoBlock->InsText( line, pos, rep.ptr(), rep.count() );
							}

							begin = str.Get();
							end = begin + str.Len();
							s = begin + pos + rep.count();

							cursor.Set( line, s - begin );
							marker = cursor;

							if ( !p ) { break; }

							goto restart;
						}
					}

					if ( !p ) { break; }

					unicode_t c = charset->GetChar( p, end );

					if ( sens )
					{
						if ( c != *sPtr ) { break; }
					}
					else
					{
						if ( UnicodeLC( c ) != *sPtr ) { break; }
					}
				}

				s = charset->GetNext( s, end );
			}

			line++;

			if ( line >= text.Count() ) { break; }

			EditString& str = text.Get( line );
			s = begin = str.Get();
			end = begin + str.Len();
		}

		marker = cursor;

		undoBlock->SetEndPos( cursor, marker );
		undoList.Append( undoBlock );

		CursorToScreen();
		SendChanges();
		Refresh();

	}
	catch ( ... )
	{
		undoList.Clear();
		throw;
	}

	return foundCount > 0;
}


EditWin::~EditWin()
{
	DelTimer( 0 );
}


