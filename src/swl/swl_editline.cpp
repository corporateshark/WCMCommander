/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#include "swl.h"

#include <algorithm>

namespace wal
{

/////////////////////////////////////////////////   EditBuf

	void EditBuf::SetSize( int n )
	{
		if ( size >= n ) { return; }

		n = ( ( n + STEP - 1 ) / STEP ) * STEP;

		std::vector<unicode_t> p( n );

		if ( count > 0 )
		{
			memcpy( p.data(), data.data(), count * sizeof( unicode_t ) );
		}

		size = n;
		data = p;
	}

	void EditBuf::InsertBlock( int pos, int n )
	{
		ASSERT( pos >= 0 && pos <= count && n > 0 );
		SetSize( count + n );

		if ( pos < count )
		{
			memmove( data.data() + pos + n, data.data() + pos, ( count - pos ) * sizeof( unicode_t ) );
		}

		count += n;
	}

	void EditBuf::DeleteBlock( int pos, int n )
	{
		ASSERT( pos >= 0 && pos < count && n > 0 );

		if ( pos + n > count )
		{
			n = count - pos;
		}

		if ( pos + n < count )
		{
			memmove( data.data() + pos, data.data() + pos + n, ( count - ( pos + n ) ) * sizeof( unicode_t ) );
		}

		count -= n;
	}

	bool EditBuf::DelMarked()
	{
		if ( cursor == marker ) { return false; }

		int a, b;

		if ( cursor < marker ) { a = cursor; b = marker; }
		else { a = marker; b = cursor; }

		int n = b - a;
		DeleteBlock( a, n );
		cursor = marker = a;
		return true;
	}

	void EditBuf::Insert( unicode_t t )
	{
		DelMarked();
		InsertBlock( cursor, 1 );
		data[cursor] = t;
		cursor++;
		marker = cursor;
	}

	void EditBuf::Insert( const unicode_t* txt )
	{
		DelMarked();

		if ( !txt || !*txt ) { return; }

		int n = unicode_strlen( txt );
		InsertBlock( cursor, n );

		for ( ; *txt; txt++, cursor++ )
		{
			data[cursor] = *txt;
		}

		marker = cursor;
	}

	void EditBuf::Del( bool DeleteWord )
	{
		if ( DelMarked() ) { return; }

		if ( DeleteWord )
		{
			int PrevCursor = cursor;

			int group = cursor < count ? EditBuf::GetCharGroup( data[cursor] ) : -1;

			int CharsToDelete = 0;

			while ( cursor < count && EditBuf::GetCharGroup( data[cursor] ) == group )
			{
				cursor++;
				CharsToDelete++;
			}

			cursor = PrevCursor;

			CharsToDelete = std::min( CharsToDelete, count - cursor );

			if ( CharsToDelete > 0 ) { DeleteBlock( cursor, CharsToDelete ); }
		}
		else if ( cursor < count )
		{
			DeleteBlock( cursor, 1 );
		}
	}

	void EditBuf::Backspace( bool DeleteWord )
	{
		if ( DelMarked() ) { return; }

		int CharsToDelete = 1;

		if ( DeleteWord )
		{
			int PrevCursor = cursor;

			if ( cursor > 0 ) { cursor--; }

			int group = cursor < count ? EditBuf::GetCharGroup( data[cursor] ) : -1;

			while ( cursor > 0 && EditBuf::GetCharGroup( data[cursor - 1] ) == group )
			{
				cursor--;
			}

			CharsToDelete = PrevCursor - cursor;
		}
		else if ( cursor > 0 )
		{
			cursor--;
		}

		marker = cursor;
		DeleteBlock( cursor, CharsToDelete );
	}

	void EditBuf::Set( const unicode_t* s, bool mark )
	{
		marker = cursor = count = 0;

		if ( !s || !*s ) { return; }

		int n = unicode_strlen( s );
		SetSize( n );
		memcpy( data.data(), s, n * sizeof( unicode_t ) );
		cursor = count = n;
		marker = mark ? 0 : cursor;
	}

	void EditBuf::CtrlLeft  ( bool mark )
	{
		if ( cursor > 0 ) { cursor--; }

		int group = cursor < count ? EditBuf::GetCharGroup( data[cursor] ) : -1;

		while ( cursor > 0 && EditBuf::GetCharGroup( data[cursor - 1] ) == group )
		{
			cursor--;
		}

		if ( !mark ) { marker = cursor; }
	}

	void EditBuf::CtrlRight ( bool mark )
	{
		int group = cursor < count ? EditBuf::GetCharGroup( data[cursor] ) : -1;

//	if (cursor < count) cursor++;

		while ( cursor < count )
		{
			int nextGroup = cursor + 1 < count ? EditBuf::GetCharGroup( data[cursor + 1] ) : -1;
			cursor++;

			if ( nextGroup != group ) { break; }
		}

		if ( !mark ) { marker = cursor; }
	}

	int EditBuf::GetCharGroup( unicode_t c )
	{
		if ( c <= ' ' || ( c >= 0x7F && c <= 0xA0 ) )
		{
			return 0;
		}

//should use unicode
		switch ( c )
		{
			case '!':
			case '"':
			case '#':
			case '$':
			case '%':
			case '&':
			case '\'':
			case '(':
			case ')':
			case '*':
			case '+':
			case ',':
			case '-':
			case '.':
			case ':':
			case ';':
			case '<':
			case '=':
			case '>':
			case '?':
			case '@':
			case '[':
			case '\\':
			case '/':
			case ']':
			case '^':

//	case '_':
			case '`':
			case '{':
			case '|':
			case '}':
				return 1;
		};

		return 100;
	}


/////////////////////////////////////////////////////////// EditLine

	int uiClassEditLine = GetUiID( "EditLine" );
	int EditLine::UiGetClassId() { return uiClassEditLine; }

	void EditLine::OnChangeStyles()
	{
		GC gc( this );
		gc.Set( GetFont() );

		cpoint ts = gc.GetTextExtents( ABCString );
		charW = ts.x / ABCStringLen;
		charH = ts.y;

		int w = ( ts.x / ABCStringLen ) * _chars;

		int h = ts.y + 2;

		if ( frame3d )
		{
			w += 8;
			h += 8;
		}

		LSize ls;
		ls.x.minimal = ls.x.ideal = w;
		ls.x.maximal = 16000;
		ls.y.minimal = ls.y.maximal = ls.y.ideal = h;
		SetLSize( ls );
	}

	EditLine::EditLine( int nId, Win* parent, const crect* rect, const unicode_t* txt, int chars, bool frame, unsigned flags )
		: Win( Win::WT_CHILD, Win::WH_TABFOCUS | WH_CLICKFOCUS, parent, rect, nId )
		, _use_alt_symbols( false )
		, _flags( flags )
		, text( txt )
		, _chars( chars > 0 ? chars : 10 )
		, cursorVisible( false )
		, passwordMode( false )
		, showSpaces( true )
		, doAcceptAltKeys( false )
		, first( 0 )
		, frame3d( frame )
		, charH( 0 )
		, charW( 0 )
		, m_ReplaceMode( false )
	{
		text.End();

		if ( !rect )
		{
			OnChangeStyles();
		}
	};

	static unicode_t passwordSymbol = '*';

	void EditLine::DrawCursor( GC& gc )
	{
		int cursor = text.Cursor();

		if ( cursor < first ) { return; }

		crect cr = ClientRect();

		if ( frame3d ) { cr.Dec( 4 ); }

		int x = cr.left;
		int y = cr.bottom - charH / 5;

		gc.Set( GetFont() );


		cpoint prevSize( 0, 0 );

		if ( cursor > first )
		{
			if ( passwordMode )
			{
				prevSize = gc.GetTextExtents( &passwordSymbol, 1 );
				prevSize.x *= cursor - first;
			}
			else
			{
				prevSize = gc.GetTextExtents( text.Ptr() + first, cursor - first );
			}
		}

		if ( prevSize.x >= cr.Width() ) { return; }

		x += prevSize.x;

		bool marked = text.Marked( cursor );
		unsigned bColor = /*GetColor(*/marked ? UiGetColor( uiMarkBackground, 0, 0, 0x800000 ) : UiGetColor( uiBackground, 0, 0, 0xFFFFFF ); //UiIC_EDIT_STEXT_BG : IC_EDIT_TEXT_BG);
		unsigned fColor = /*GetColor(*/marked ? UiGetColor( uiMarkColor, 0, 0, 0xFFFFFF ) : UiGetColor( uiColor, 0, 0, 0 ); //IC_EDIT_STEXT : IC_EDIT_TEXT);

		cpoint csize( 0, 0 );

		if ( cursor < text.Count() )
		{
			csize = gc.GetTextExtents( text.Ptr() + cursor, 1 );
		}

		if ( csize.x < charW )
		{
			csize.x = charW;
		}

		gc.SetFillColor( bColor );
		gc.FillRect( crect( x, y, x + csize.x, cr.bottom ) );

		if ( cursor < text.Count() )
		{
			gc.SetTextColor( fColor );
			gc.TextOutF( x, cr.top + ( cr.Height() - csize.y ) / 2 , text.Ptr() + cursor, 1 );
		}

		if ( cursorVisible )
		{
			gc.SetFillColor( fColor );
			gc.FillRect( crect( x, y, x + charW, cr.bottom ) );
		}
	}

	bool EditLine::CheckCursorPos()
	{
		int cursor = text.Cursor();

		if ( first == cursor ) { return false; }

		int oldFirst = first;

		if ( cursor < first )
		{
			first = cursor - 8;

			if ( first < 0 ) { first = 0; }
		}

		if ( first == cursor ) { return oldFirst != first; }

		crect cr = ClientRect();

		if ( frame3d ) { cr.Dec( 4 ); }

		GC gc( this );
		gc.Set( GetFont() );

		if ( passwordMode )
		{
			cpoint size = gc.GetTextExtents( &passwordSymbol, 1 );
			int n = cursor - first;
			int m = size.x != 0 ? cr.Width() / size.x : 0;

			if ( n >= m )
			{
				if ( m > 0 ) { m--; }

				first = cursor - m;
			}
		}
		else
		{
			cpoint size = gc.GetTextExtents( text.Ptr() + first, cursor - first );

//temp (not optimized)
			for ( ; size.x >= cr.Width() && first < cursor; first++ )
			{
				size = gc.GetTextExtents( text.Ptr() + first, cursor - first );
			}
		}

		return oldFirst != first;
	}

	std::vector<unicode_t> EditLine::GetText() const
	{
		int count = text.Count();
		std::vector<unicode_t> p( count + 1 );

		if ( count > 0 ) { memcpy( p.data(), text.Ptr(), sizeof( unicode_t )*count ); }

		p[count] = 0;
		return p;
	}

	std::string EditLine::GetTextStr() const
	{
		std::vector<unicode_t> V = GetText();

		return unicode_to_utf8_string( V.data() );
	}

	void EditLine::EventSize( cevent_size* pEvent )
	{
		first = 0;
		CheckCursorPos();
		Invalidate();
	}

	void EditLine::EventTimer( int tid )
	{
		cursorVisible = !cursorVisible;
		wal::GC gc( this );
		DrawCursor( gc );
	}

	bool EditLine::EventFocus( bool recv )
	{
		if ( !IsReadOnly() )
		{
			cursorVisible = recv;

			if ( recv )
			{
				SetTimer( 1, 500 );
			}
			else
			{
				DelTimer( 1 );
			}
		}

		Invalidate();
		return true;
	}

	bool EditLine::InFocus()
	{
		if ( UseParentFocus() )
		{
			return Parent()->InFocus();
		}

		return Win::InFocus();
	}

	void EditLine::SetFocus()
	{
		if ( UseParentFocus() )
		{
			Parent()->SetFocus();
			return;
		}

		Win::SetFocus();
	}

	void EditLine::Paint( GC& gc, const crect& paintRect )
	{
		crect cr = ClientRect();
		crect rect = cr;

		unsigned frameColor = UiGetColor( uiFrameColor, 0, 0, 0xFFFFFF );

		if ( frame3d )
		{
			DrawBorder( gc, rect, ColorTone( frameColor, +20 ) );
			rect.Dec();
			Draw3DButtonW2( gc, rect, frameColor, false );
			rect.Dec();
			rect.Dec();
			DrawBorder( gc, rect, ColorTone( frameColor, IsEnabled() ? -200 : -80 ) );
			rect.Dec();
		}

		unsigned colorBg = UiGetColor( uiBackground, 0, 0, 0xFFFFFF );

		int x = rect.left;

		gc.SetClipRgn( &rect );

		if ( first < text.Count() )
		{
			gc.Set( GetFont() );

			int cH = gc.GetTextExtents( text.Ptr() + first, 1 ).y;
			int y = cr.top + ( cr.Height() - cH ) / 2;

			int cnt = text.Count() - first;
			int i = first;

			std::vector<unicode_t> pwTextArray;
			unicode_t* pwText = 0;

			if ( passwordMode && cnt > 0 )
			{
				pwTextArray.resize( cnt );

				for ( int i = 0; i < cnt; i++ ) { pwTextArray[i] = passwordSymbol; }

				pwText = pwTextArray.data();
			}

			int color = UiGetColor( uiColor, 0, 0, 0 );
			int background = UiGetColor( uiBackground, 0, 0, 0xFFFFFF );

			int mark_color = UiGetColor( uiMarkColor, 0, 0, 0xFFFFFF );
			int mark_background = UiGetColor( uiMarkBackground, 0, 0, 0 );

			while ( cnt > 0 )
			{
				bool mark = text.Marked( i );

				int j;

				for ( j = i + 1; j < text.Count() && text.Marked( j ) == mark; ) { j++; }

				int n = j - i;
				cpoint size = gc.GetTextExtents( passwordMode ? pwText : ( text.Ptr() + i ), n );

				gc.SetFillColor( mark ? mark_background : background ); //GetColor(InFocus() && mark ? IC_EDIT_STEXT_BG : IC_EDIT_TEXT_BG));
				gc.FillRect( crect( x, cr.top, x + size.x, cr.bottom ) );
				gc.SetTextColor( mark ? mark_color : color ); //GetColor(InFocus() &&  mark ? IC_EDIT_STEXT : (IsEnabled() ? IC_EDIT_TEXT : IC_GRAY_TEXT)));

				std::vector<unicode_t> VisibleText = new_unicode_str( passwordMode ? pwText : ( text.Ptr() + i ) );

				// https://github.com/corporateshark/WCMCommander/issues/187
				if ( showSpaces )
				{
					ReplaceSpaces( &VisibleText );
				}

				gc.TextOutF( x, y, VisibleText.data(), n );
				cnt -= n;
				x += size.x;
				i += n;
			}
		}

		if ( x < cr.right )
		{
			gc.SetFillColor( colorBg ); //GetColor(IC_EDIT_TEXT_BG));
			cr.left = x;
			gc.FillRect( cr );
		}


		if ( InFocus() && !IsReadOnly() )
		{
			DrawCursor( gc );
		}

		return;
	}

	bool EditLine::EventMouse( cevent_mouse* pEvent )
	{
		switch ( pEvent->Type() )
		{
			case EV_MOUSE_MOVE:
				if ( IsCaptured() )
				{
					int n = GetCharPos( pEvent->Point() );
					text.SetCursor( first + n, true );
					cursorVisible = true;
					CheckCursorPos();
					Invalidate();
					return true;
				}
				
				break;

			case EV_MOUSE_DOUBLE:
			case EV_MOUSE_PRESS:
			{
				if ( pEvent->Button() != MB_L )
				{
					break;
				}

				int n = GetCharPos( pEvent->Point() );
				text.SetCursor( first + n );
				cursorVisible = true;
				CheckCursorPos();
				Invalidate();

				SetCapture( &captureSD );
				return true;
			}
			break;

			case EV_MOUSE_RELEASE:
				if ( pEvent->Button() != MB_L )
				{
					break;
				}

				ReleaseCapture( &captureSD );
				return true;
		};

		return Win::EventMouse( pEvent );
	}

	bool EditLine::EventKey( cevent_key* pEvent )
	{
		if ( !doAcceptAltKeys && ( pEvent->Mod() & KM_ALT ) != 0 )
		{
			return false;
		}

		if ( pEvent->Type() == EV_KEYDOWN )
		{

			bool shift = ( pEvent->Mod() & KM_SHIFT ) != 0;
			bool ctrl  = ( pEvent->Mod() & KM_CTRL ) != 0;
			bool alt   = ( pEvent->Mod() & KM_ALT ) != 0;

			if ( ctrl )
			{
				switch ( pEvent->Key() )
				{
					case VK_A:
						text.Begin( false );
						text.End( true );
						CheckCursorPos();
						Invalidate();
						return true;

					case VK_C:
						ClipboardCopy();
						return true;

					case VK_V:
						if ( !IsReadOnly() )
						{
							ClipboardPaste();
						}

						return true;

					case VK_X:
						if ( !IsReadOnly() )
						{
							ClipboardCut();
						}

						return true;
				}
			}

			switch ( pEvent->Key() )
			{
				case VK_BACK:
				{
					if ( IsReadOnly() || text.Cursor() == 0 )
					{
						return true;
					}

					text.Backspace( ctrl );
					SendCommand( SCMD_EDITLINE_DELETED );
					Changed();
				}
				break;

				case VK_DELETE:
				{
					if ( IsReadOnly() || text.Cursor() > text.Count() )
					{
						return true;
					}

					text.Del( ctrl );
					SendCommand( SCMD_EDITLINE_DELETED );
					Changed();
				}
				break;

				case VK_LEFT:
				{
					if ( text.Marked() == shift && text.Cursor() == 0 )
					{
						return true;
					}

					if ( ctrl )
					{
						text.CtrlLeft( shift );
					}
					else
					{
						text.Left( shift );
					}
				};

				break;

				case VK_RIGHT:
				{
					if ( text.Marked() == shift && text.Cursor() == text.Count() )
					{
						return true;
					}

					if ( ctrl )
					{
						text.CtrlRight( shift );
					}
					else
					{
						text.Right( shift );
					}
				};

				break;

				case VK_HOME:
				{
					if ( text.Marked() == shift && text.Cursor() == 0 )
					{
						return true;
					}

					text.Begin( shift );
				}
				break;

				case VK_END:
				{
					if ( text.Marked() == shift && text.Cursor() == text.Count() )
					{
						return true;
					}

					text.End( shift );
				}
				break;

				case VK_INSERT:
					if ( shift )
					{
						if ( IsReadOnly() )
						{
							return true;
						}
						
						ClipboardPaste();
					}
					else if ( ctrl && text.Marked() )
					{
						ClipboardCopy();
					}

					break;

				default:
					wchar_t c = pEvent->Char();
					std::vector<unicode_t> oldtext = GetText();

					if ( c && c >= 0x20 && ( !alt || _use_alt_symbols ) )
					{
						if ( IsReadOnly() )
						{
							return true;
						}

						if ( m_ReplaceMode )
						{
							text.Del( false );
						}

						text.Insert( c );

						if ( m_Validator && !m_Validator->IsValid( GetText() ) )
						{
							SetText( oldtext.data(), false );
						}

						SendCommand( SCMD_EDITLINE_INSERTED );
						Changed();
					}
					else
					{
						return false;
					}
					
					break;
			}

			cursorVisible = true;
			CheckCursorPos();
			Invalidate();
			return true;
		}

		return Win::EventKey( pEvent );
	}

	void EditLine::ClipboardCopy()
	{
		ClipboardText ctx;
		int a = text.Cursor();
		int b = text.Marker();

		if ( a > b ) { int t = a; a = b; b = t; }

		for ( int i = a; i < b; i++ ) { ctx.Append( text[i] ); }

		ClipboardSetText( this, ctx );
	}


	void EditLine::ClipboardPaste()
	{
		ClipboardText ctx;
		ClipboardGetText( this, &ctx );
		int count = ctx.Count();

		if ( count <= 0 ) { return; }

		for ( int i = 0; i < count; i++ )
		{
			unicode_t c = ctx[i];

			if ( c == 9 || c == 10 || c == 13 ) { c = ' '; }

			if ( c < 32 ) { break; }

			text.Insert( c );
		}

		CheckCursorPos();
		Changed();
		Invalidate();
	}

	void EditLine::ClipboardCut()
	{
		if ( text.Marked() )
		{
			ClipboardCopy();
			text.Del( false );
			CheckCursorPos();
			Invalidate();
		}
	}

	void EditLine::Clear()
	{
		first = 0;
		text.Clear();
		Invalidate();
	}

	void EditLine::SetText( const unicode_t* txt, bool mark )
	{
		first = 0;
		cursorVisible = true;
		text.Set( txt, mark );
		Invalidate();
	}

	void EditLine::SetText( const std::string& utf8txt, bool mark )
	{
		SetText( utf8str_to_unicode(utf8txt).data(), mark );
	}

	void EditLine::Insert( unicode_t t )
	{
		cursorVisible = true;
		text.Insert( t );
		CheckCursorPos();
		Invalidate();
	}

	void EditLine::Insert( const unicode_t* txt )
	{
		cursorVisible = true;
		text.Insert( txt );
		Invalidate();
	}

	int EditLine::GetCharPos( cpoint p )
	{
		if ( !text.Count() ) { return 0; }

		crect cr = ClientRect();

		if ( frame3d ) { cr.Dec( 4 ); }

		if ( p.x < cr.left ) { return -1; }

		GC gc( this );
		gc.Set( GetFont() );
		int n = text.Count() - first;

		for ( int i = 0; i < n; i++ )
		{
			cpoint size = gc.GetTextExtents( text.Ptr() + first + i, i + 1 );

			if ( cr.left + size.x > p.x ) { return i; }
		}

		return text.Count();
	}

	EditLine::~EditLine() {}


}; // namespace wal
