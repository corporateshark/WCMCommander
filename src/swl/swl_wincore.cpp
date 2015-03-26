/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#include <string>
#include <unordered_map>

#include "string-util.h"
#include "swl.h"
#include "swl_wincore_internal.h"

namespace wal
{

	struct WINHASHNODE
	{
		WinID id;
		Win* win;

		WINHASHNODE( WinID h, Win* w ): id( h ), win( w ) {}
		WINHASHNODE(): id( 0 ), win ( nullptr ) {}

		const WinID& key() const { return id; };
	};

	static std::unordered_map<WinID, WINHASHNODE> winhash;
	WinID Win::focusWinId = 0;


	Win* GetWinByID( WinID hWnd )
	{
		WinID id( hWnd );
		auto i = winhash.find(hWnd);
		const WINHASHNODE* node = ( i == winhash.end() ) ? nullptr : &(i->second);
		return node ? node->win : 0;
	}

	int DelWinFromHash( WinID w )
	{
		winhash.erase(w);
		return winhash.size();
	}

	void AddWinToHash( WinID handle, Win* w )
	{
		WINHASHNODE node( handle, w );
		winhash[handle] = node;
	}


	static void ForeachBlock( const WINHASHNODE* t, void* data ) { t->win->Block( *( ( WinID* )data ) );}
	static void ForeachUnblock( const WINHASHNODE* t, void* data ) {  t->win->Unblock( *( ( WinID* )data ) );}
	void AppBlock( WinID w ) { for ( const auto& i : winhash ) ForeachBlock( &(i.second), &w ); }
	void AppUnblock( WinID w ) { for ( const auto& i : winhash ) ForeachUnblock( &(i.second), &w ); }


	struct TimerStruct
	{
		WinID winId;
		int timerId;
		unsigned period;
		unsigned last;
	};

	static ccollect<TimerStruct> timerList;
	unsigned GetTickMiliseconds();

	static void __TimerAppend( WinID wid, int tid, unsigned period )
	{
		if ( !period ) { period = 0; }

		TimerStruct ts;
		ts.winId = wid;
		ts.timerId = tid;
		ts.period = period;
		ts.last = GetTickMiliseconds();
		timerList.append( ts );
	}

	static void TimerDel( WinID wid, int tid )
	{
		for ( int i = 0, n = timerList.count(); i < n; )
		{
			TimerStruct* ts = timerList.ptr() + i;

			if ( ts->winId == wid && ts->timerId == tid )
			{
				timerList.del( i );
				n--;
			}
			else { i++; }
		}
	}

	static void TimerReset( WinID wid, int tid, unsigned period )
	{
		if ( !period ) { period = 0; }

		for ( int i = 0, n = timerList.count(); i < n; i++ )
		{
			TimerStruct* ts = timerList.ptr() + i;

			if ( ts->winId == wid && ts->timerId == tid )
			{
				timerList[i].period = period;
				return;
			}
		}

		__TimerAppend( wid, tid, period );
	}

	void TimerDel( WinID wid )
	{
		for ( int i = 0; i < timerList.count(); )
		{
			if ( timerList[i].winId == wid )
			{
				timerList.del( i );
			}
			else
			{
				i++;
			}
		}
	}

	struct RunTimersNode
	{
		WinID wid;
		int tid;
	};


	unsigned RunTimers()
	{

		ccollect<RunTimersNode> runList;
		unsigned tim = GetTickMiliseconds();
		int i, n;

//printf("count timers %i\n", timerList.count());
		for ( i = 0, n = timerList.count(); i < n; i++ )
		{
			TimerStruct* ts = timerList.ptr() + i;

//printf("tim%i (%i,%i)\n",1, tim, ts->last);

			if ( tim - ts->last >= ts->period )
			{
				ts->last = tim;
				RunTimersNode node;
				node.wid = ts->winId;
				node.tid = ts->timerId;
				runList.append( node );
			}
		}

//printf("running timers %i\n", runList.count());
		for ( i = 0; i < runList.count(); )
		{
			Win* w = GetWinByID( runList[i].wid );

			if ( w )
			{
				w->EventTimer( runList[i].tid ); //...
				i++;
			}
			else { runList.del( i ); }
		}

		//DWORD
		unsigned minTim = 0xFFFF;
		tim = GetTickMiliseconds();

		for ( i = 0, n = timerList.count(); i < n; i++ )
		{
			TimerStruct* ts = timerList.ptr() + i;
			//DWORD
			unsigned t = tim - ts->last;

			if ( t >= ts->period )
			{
				minTim = 0;
			}
			else
			{
				//DWORD
				unsigned m = ts->period - t;

				if ( minTim > m )
				{
					minTim = m;
				}
			}
		}

		return minTim;
	}


////////////////////////////////////////////////// Win


	void Win::EndModal( int id )
	{
		if ( modal && !blockedBy )
		{
			( ( ModalStruct* )modal )->EndModal( id );
		}
	}


	void Win::Paint( GC& gc, const crect& paintRect )
	{
		gc.SetFillColor( 0xA0A0A0 );
		gc.FillRect( paintRect ); //CCC
	}


	bool Win::IsEnabled()
	{
		for ( Win* p = this; p; p = p->parent )
		{
			if ( !( p->state & S_ENABLED ) ) { return false; }

			if ( p->IsModal() || p->Type() == Win::WT_MAIN ) { break; }
		}

		return true;
	}

	bool Win::IsVisible()
	{
		return ( state & S_VISIBLE ) != 0;
	}

	void Win::Enable( bool en )
	{
		if ( en )
		{
			SetState( S_ENABLED );
		}
		else
		{
			ClearState( S_ENABLED );
		}

		if ( IsVisible() )
		{
			Invalidate();
		}
	}

	void Win::SetTimer( int id, unsigned period )
	{
		TimerReset/*Append*/( GetID(), id, period );
	}

	void Win::ResetTimer( int id, unsigned period )
	{
		TimerReset( GetID(), id, period );
	}

	void Win::DelTimer( int id )
	{
		TimerDel( GetID(), id );
	}

	void Win::DelAllTimers()
	{
		TimerDel( GetID() );
	}

	bool Win::Event( cevent* pEvent )
	{
		switch ( pEvent->Type() )
		{
			case EV_KEYUP:
			case EV_KEYDOWN:
				return EventKey( ( cevent_key* )pEvent );

			case EV_CLOSE:
				return EventClose();

			case EV_SETFOCUS:
			{
//			Invalidate();
				return EventFocus( true );
			}

			case EV_KILLFOCUS:
//			Invalidate();
				return EventFocus( false );

			case EV_SHOW:
				return EventShow( ( ( cevent_show* )pEvent )->Show() );

			case EV_MOUSE_PRESS:
				if ( ( whint & WH_CLICKFOCUS ) != 0 && !InFocus() )
				{
					SetFocus();
				}

			case EV_MOUSE_MOVE:
			case EV_MOUSE_RELEASE:
			case EV_MOUSE_DOUBLE:
				return EventMouse( ( cevent_mouse* )pEvent );

			case EV_ENTER:
			case EV_LEAVE:
				EventEnterLeave( pEvent );
				return true;

			case EV_SIZE:
				RecalcLayouts();
				EventSize( ( cevent_size* )pEvent );
				return true;

			case EV_MOVE:
				EventMove( ( cevent_move* )pEvent );
				return true;

			case EV_ACTIVATE:
				return EventActivate( ( ( cevent_activate* )pEvent )->Activated(), ( ( cevent_activate* )pEvent )->Who() );

			default:
				return false;
		}
	}



	bool Win::EventMouse( cevent_mouse* pEvent ) {  return false; }
	bool Win::EventKey( cevent_key* pEvent ) {   return false;}

	bool Win::EventChildKey( Win* child, cevent_key* pEvent )
	{
		//dbg_printf("Win::EventChildKey key=0x%x mod=%d\n", pEvent->Key(), pEvent->Mod());
		// check if any child recognizes the key as its hot key
		if ( ( pEvent->Mod() & KM_ALT ) != 0 )
		{
			for ( int i = 0; i < childList.count(); i++ )
			{
				Win* tchild = childList[i];
				Win* tHotkeyWin;

				if ( ( tHotkeyWin = tchild->IsHisHotKey( pEvent ) ) != 0 )
				{
					tHotkeyWin->SetFocus();
					return tHotkeyWin->EventKey( pEvent );
				}
			}
		}

		return false;
	}

	bool Win::EventFocus( bool recv )
	{
		if ( recv && lastFocusChild )
		{
			Win* w = GetWinByID( lastFocusChild );

//printf("SET FOCUS TO LAST FOCUS CHILD (%p)\n",w);
			if ( w ) { w->SetFocus(); }
		}

		return false;
	}

	bool Win::EventClose() { if ( this->IsModal() ) { EndModal( CMD_CANCEL ); } return true;}
	bool Win::EventShow( bool ) { return true; }
	void Win::EventTimer( int tid ) {};
	bool Win::EventActivate( bool activated, Win* w ) { return false; }
	void Win::EventEnterLeave( cevent* pEvent ) {}
	void Win::EventSize( cevent_size* pEvent ) {}
	void Win::EventMove( cevent_move* pEvent ) {}
	void Win::ThreadSignal( int id, int data ) {}
	void Win::ThreadStopped( int id, void* data ) {}

	bool Win::Command( int id, int subId, Win* win, void* data )
	{
		return parent ? parent->Command( id, subId,  win, data ) : false;
	}

	bool Win::Broadcast( int id, int subId, Win* win, void* data )
	{
		return false;
	}


	int Win::SendBroadcast( int id, int subId, Win* win, void* data, int level )
	{
		if ( level <= 0 ) { return 0; }

		level--;
		ccollect<WinID> list;
		int n = 0;
		int i;

		for ( i = 0; i < childList.count(); i++ )
		{
			list.append( childList[i]->GetID() );
		}

		for ( i = 0; i < list.count(); i++ )
		{
			Win* w = GetWinByID( list[i] );

			if ( w ) { n += w->SendBroadcast( id, subId, win, data, level ); }
		}

		if ( Broadcast( id, subId, win, data ) ) { n++; }

		return n;
	}

	void Win::SetIdealSize()
	{
		if ( lSize.x.ideal > 0 && lSize.y.ideal > 0 )
		{
			crect r = ScreenRect();
			crect cr = ClientRect();

			int xplus = ( r.right - r.left + 1 ) - ( cr.right - cr.left + 1 );
			int yplus = ( r.bottom - r.top + 1 ) - ( cr.bottom - cr.top + 1 );
			Move( crect( r.left, r.top, r.left + lSize.x.ideal + xplus, r.top + lSize.y.ideal + yplus ) ); //???
		}
	}


	bool Win::UnblockTree( WinID id )
	{
		if ( blockedBy != id )
		{
			return false;
		}

		blockedBy = 0;

		for ( int i = 0; i < childList.count(); i++ )
		{
			childList[i]->UnblockTree( id );
		}

		return true;
	}


	bool Win::IsOneParentWith( WinID h )
	{
		Win* w = GetWinByID( h );

		if ( !w ) { return false; }

		while ( w->Parent() ) { w = w->Parent(); }

		Win* t = this;

		while ( t->Parent() ) { t = t->Parent(); }

		return t == w;
	}

	void Win::PopupTreeList( ccollect<WinID>& list )
	{
		list.append( this->handle );

		for ( int i = 0; i < childList.count(); i++ )
			if ( childList[i]->type & Win::WT_POPUP )
			{
				childList[i]->PopupTreeList( list );
			}
	}

	Win* Win::FocusNPChild( bool next )
	{
		WinID c = lastFocusChild;
		Win** list = childList.ptr();
		int count = childList.count();
		int i = 0;
		Win* p = 0;

		for ( i = 0; i < count; i++ )
			if ( list[i]->handle == c )
			{
				p = list[i];
				break;
			};

		if ( p )
		{
			int n = i;

			if ( next )
			{
				for ( i = n + 1; i < count; i++ )
					if ( ( list[i]->whint &  WH_TABFOCUS ) && list[i]->IsVisible() )
					{
						list[i]->SetFocus();
						return list[i];
					}

				for ( i = 0; i < n; i++ )
					if ( ( list[i]->whint &  WH_TABFOCUS ) && list[i]->IsVisible() )
					{
						list[i]->SetFocus();
						return list[i];
					}
			}
			else
			{
				for ( i = n - 1; i >= 0; i-- )
					if ( ( list[i]->whint &  WH_TABFOCUS ) && list[i]->IsVisible() )
					{
						list[i]->SetFocus();
						return list[i];
					}

				for ( i = count - 1; i > n; i-- )
					if ( ( list[i]->whint &  WH_TABFOCUS ) && list[i]->IsVisible() )
					{
						list[i]->SetFocus();
						return list[i];
					}
			}

			if ( !p->InFocus() )
			{
				p->SetFocus();
			}

			return p;

		}
		else
		{
			for ( i = 0; i < count; i++ )
				if ( ( list[i]->whint &  WH_TABFOCUS ) && list[i]->IsVisible() )
				{
					list[i]->SetFocus();
					return list[i];
				}
		}

		return 0;
	}


	void Win::OnChangeStyles()
	{
	}

	void Win::StylesChanged( Win* w )
	{
		if ( !w ) { return; }

		int count = w->ChildCount();

		for ( int i = 0; i < count; i++ )
		{
			Win* ch = w->GetChild( i );

			if ( !ch ) { break; }

			StylesChanged( ch );
		};

		w->OnChangeStyles();

		w->RecalcLayouts();

		w->Invalidate();
	}

	cfont* Win::GetChildFont( Win* w, int fontId )
	{
		return parent ? parent->GetChildFont( w, fontId ) : SysGetFont( w, fontId );
	}

	void Win::ThreadCreate( int id, void* ( *f )( void* ), void* d )
	{
		wth_CreateThread( this, id, f, d );
	}



	unsigned ColorTone( unsigned color, int tone /*0-255*/ )
	{
		unsigned char R = ( unsigned char )( color % 0x100 ), G = ( unsigned char )( ( color / 0x100 ) % 0x100 ), B = ( unsigned char )( ( color / 0x10000 ) % 0x100 );

		if ( tone >= 0 )
		{
			unsigned char t = tone % 0x100;
			R += ( ( 0x100 - R ) * t ) / 0x100;
			G += ( ( 0x100 - G ) * t ) / 0x100;
			B += ( ( 0x100 - B ) * t ) / 0x100;
		}
		else
		{
			unsigned char t = ( -tone ) % 0x100;
			R -= ( ( R * t ) / 0x100 );
			G -= ( ( G * t ) / 0x100 );
			B -= ( ( B * t ) / 0x100 );
		}

		return ( ( B % 0x100 ) << 16 ) + ( ( G % 0x100 ) << 8 ) + ( R % 0x100 );
	}

	void DrawPixelList( GC& gc, unsigned short* s, int x, int y, unsigned color )
	{
		int n = *( s++ );

		for ( int i = 0; i < n; i++, s++, y++ )
		{
			unsigned c = *s;

			for ( int j = 0; j < 16; j++, c >>= 1 )
				if ( c & 1 ) { gc.SetPixel( x + j, y, color ); }
		}
	}




/////////////////////////////////////// cevent's

	cevent::~cevent() {}
	cevent_show::~cevent_show() {}
	cevent_input::~cevent_input() {}
	cevent_key::~cevent_key() {}
	cevent_mouse::~cevent_mouse() {}
	cevent_activate::~cevent_activate() {}
	cevent_size::~cevent_size() {}
	cevent_move::~cevent_move() {}

///////////////////////////////////////   Win Threads

	WthInternalEvent wthInternalEvent;

	enum WTHCMD
	{
		WTH_CMD_NONE = 0,
		WTH_CMD_EVENT = 1,
		WTH_CMD_END = 2
	};

	struct WTHNode
	{
		volatile thread_t th;
		volatile int id;
		Win* volatile w;
		volatile int data;
		void* ( *volatile func )( void* );
		void* volatile fData;
		volatile int cmd;
		WTHNode* volatile thNext;
		WTHNode* volatile winNext;
		WTHNode* volatile cmdNext;
		WTHNode(): id( 0 ), w( 0 ), data( 0 ), func( 0 ), fData( 0 ), cmd( 0 ), thNext( 0 ), winNext( 0 ), cmdNext( 0 ) {}
	};

	inline unsigned th_key( thread_t th )
	{
		unsigned key = 0;

		for ( auto i = 0; i < sizeof( th ); i++ ) { key += ( ( unsigned char* )&th )[i]; }

		return key;
	}

	inline unsigned win_key( Win* w )
	{
		return ( unsigned )( w - ( Win* )0 );
	}

	class WTHash
	{
		friend void wth_DropWindow( Win* w );
		friend void wth_CreateThread( Win* w, int id, void* ( *f )( void* ), void* d );
		friend bool WinThreadSignal( int data );
		friend void* _swl_win_thread_func( void* data );
		friend void wth_DoEvents();

#define WTH_HASH_SIZE (331)
		WTHNode* volatile thList[WTH_HASH_SIZE];
		WTHNode* volatile winList[WTH_HASH_SIZE];
		WTHNode* volatile cmdFirst;
		WTHNode* volatile cmdLast;

		Mutex mutex;
	public:
		WTHash()
			: cmdFirst( 0 ), cmdLast( 0 )
		{
			int i;

			for ( i = 0; i < WTH_HASH_SIZE; i++ ) { thList[i] = 0; }

			for ( i = 0; i < WTH_HASH_SIZE; i++ ) { winList[i] = 0; }
		}
		~WTHash() {}
	} wtHash;


	void* _swl_win_thread_func( void* data )
	{
		WTHNode* p = ( ( WTHNode* )data );
		void* ret = p->func( p->fData );

		MutexLock lock( &wtHash.mutex );
		unsigned tN = th_key( p->th ) % WTH_HASH_SIZE;

		WTHNode* volatile* t = &( wtHash.thList[tN] );

		while ( *t )
		{
			if ( t[0] == p )
			{
				t[0] = p->thNext;
				p->thNext = 0;
				break;
			}

			t = &( t[0]->thNext );
		}

		if ( !p->cmd )
		{
			p->cmdNext = 0;

			if ( wtHash.cmdLast )
			{
				wtHash.cmdLast->cmdNext = p;
			}
			else
			{
				wtHash.cmdFirst = p;
			}

			wtHash.cmdLast = p;
		}

		p->cmd = WTH_CMD_END;

		try
		{
			wthInternalEvent.SetSignal();
		}
		catch ( cexception* e )   //???
		{
			e->destroy();
		}

		return ret;
	}

	void wth_CreateThread( Win* w, int id, void* ( *f )( void* ), void* d )
	{
		WTHNode* p = new WTHNode;
		p->w = w;
		p->id = id;
		p->func = f;
		p->fData = d;
		MutexLock lock( &wtHash.mutex );

		int err = thread_create( const_cast<thread_t*>( &( p->th ) ), _swl_win_thread_func, p );

		if ( err ) { throw_syserr( err, "can't create window thread" ); }

		unsigned tN = th_key( p->th ) % WTH_HASH_SIZE;
		unsigned wN = win_key( w ) % WTH_HASH_SIZE;
		p->winNext = wtHash.winList[wN];
		wtHash.winList[wN] = p;
		p->thNext =  wtHash.thList[tN];
		wtHash.thList[tN] = p;
	}

	bool WinThreadSignal( int data )
	{
		thread_t th = thread_self();
		unsigned n = th_key( th ) % WTH_HASH_SIZE;
//printf("Find tn=%i (%p)\n", n, th);
		MutexLock lock( &wtHash.mutex );

		wthInternalEvent.SetSignal();

		for ( WTHNode* p = wtHash.thList[n]; p; p = p->thNext )
			if ( thread_equal( th, p->th ) )
			{
				if ( !p->cmd )
				{
					p->cmdNext = 0;

					if ( wtHash.cmdLast )
					{
						wtHash.cmdLast->cmdNext = p;
					}
					else
					{
						wtHash.cmdFirst = p;
					}

					wtHash.cmdLast = p;
				}

				p->cmd = WTH_CMD_EVENT;
				p->data = data;

				return p->w != 0;
			}

//printf("thread not found %p\n", th);
		return false;
	}

	void wth_DropWindow( Win* w )
	{
		MutexLock lock( &wtHash.mutex );
		unsigned n = win_key( w ) % WTH_HASH_SIZE;
		WTHNode* volatile* p = &( wtHash.winList[n] );

		while ( *p )
		{
			if ( p[0]->w == w )
			{
				p[0]->w = 0;
				WTHNode* t = p[0];
				p[0] = p[0]->winNext;
				t->winNext = 0;
			}
			else { p = &( p[0]->winNext ); }
		}
	}


	void wth_DoEvents()
	{
		WTHNode* last = 0;

		{
			MutexLock lock( &wtHash.mutex );
			last = wtHash.cmdLast;
			wthInternalEvent.ClearSignal();
		}

		if ( !last ) { return; }

		while ( true )
		{
			MutexLock lock( &wtHash.mutex );
			WTHNode* p = wtHash.cmdFirst;

			if ( !p ) { break; }

			if ( p->cmd == WTH_CMD_END )
			{
				if ( p->w )
				{
					unsigned n = win_key( p->w ) % WTH_HASH_SIZE;

					for ( WTHNode * volatile* pp = &( wtHash.winList[n] ); pp[0]; pp = &( pp[0]->winNext ) )
						if ( pp[0] == p )
						{
							pp[0] = p->winNext;
							break;
						}
				}

				wtHash.cmdFirst = p->cmdNext;

				if ( !wtHash.cmdFirst ) { wtHash.cmdLast = 0; }

				lock.Unlock();

				try
				{
					void* ret = 0;
					thread_join( p->th, &ret );

					if ( p->w != 0 )
					{
						p->w->ThreadStopped( p->id, ret );
					}

//printf("End of Thread Win=%p, id=%i, ret = %p\n", p->w, p->id, ret);

				}
				catch ( ... )
				{
					delete p;
				};

				delete p;

			}
			else   if ( p->cmd == WTH_CMD_EVENT )
			{
				Win* w = p->w;
				int id = p->id;
				int data = p->data;
				p->cmd = 0;
				wtHash.cmdFirst   = p->cmdNext;

				if ( !wtHash.cmdFirst ) { wtHash.cmdLast = 0; }

				lock.Unlock();

				if ( w )
				{
					w->ThreadSignal( id, data );
				}

//printf("Thread Evant Win=%p, id=%i, data=%i\n",w,id,data);

			}
			else
			{
				fprintf( stderr, "bad state in void wth_DoEvents()\n" );
			}

			if ( p == last ) { break; }
		}
	}


///////////////////////////////////////////// ClipboardText

	void ClipboardText::CopyFrom( const ClipboardText& a )
	{
		Clear();
		int i;

		for ( i = 0; i < a.list.count(); i++ )
		{
			std::vector<unicode_t> p( BUF_SIZE );
			memcpy( p.data(), a.list.const_item( i ).data(), BUF_SIZE * sizeof( unicode_t ) );
			list.append( p );
		}

		count = a.count;
	}

	ClipboardText::ClipboardText()
		:  count( 0 )
	{
	}

	ClipboardText::ClipboardText( const ClipboardText& a )
		:  count( 0 )
	{
		CopyFrom( a );
	}

	ClipboardText& ClipboardText::operator = ( const ClipboardText& a )
	{
		CopyFrom( a );
		return *this;
	}


	void ClipboardText::Clear()
	{
		list.clear();
		count = 0;
	}

	void ClipboardText::Append( unicode_t c )
	{
		int n = count + 1;
		int bc = ( n + BUF_SIZE - 1 ) / BUF_SIZE;

		while ( list.count() < bc )
		{
			std::vector<unicode_t> p( BUF_SIZE );
			list.append( p );
		}

		count++;
		n = count - 1;

		list[n / BUF_SIZE][n % BUF_SIZE] = c;
	}

	void ClipboardText::AppendUnicodeStr( const unicode_t* c )
	{
		while ( c && *c )
		{
			this->Append( *c );
			c++;
		}
	}

/////////////////////////////////    Image32 /////////////////////////////

	void Image32::alloc( int w, int h )
	{
		std::vector<uint32_t> d( w * h );

		_data = d;
		_width = w;
		_height = h;
	}

	void Image32::clear() { _width = _height = 0; _data.clear(); }

	void Image32::fill( int left, int top, int right, int bottom,  unsigned c )
	{
		if ( left < 0 ) { left = 0; }

		if ( right > _width ) { right = _width; }

		if ( top < 0 ) { top = 0; }

		if ( bottom > _height ) { bottom = _height; }

		if ( left >= right || top >= bottom ) { return; }

		for ( int i = top; i < bottom; i++ )
		{
			uint32_t* p = &_data[i * _width];
			uint32_t* end = p + right;
			p += left;

			for ( ; p < end; p++ ) { *p = c; }
		}
	}

	void Image32::copy( const Image32& a )
	{
		if ( a._width != _width || a._height != _height ) { alloc( a._width, a._height ); }

		if ( _width > 0 && _height > 0 )
		{
			memcpy( _data.data(), a._data.data(), _width * _height * sizeof( uint32_t ) );
		}
	}


#define FROM_RGBA32(src,R,G,B,A) { A = ((src)>>24)&0xFF; B=((src)>>16)&0xFF; G=((src)>>8)&0xFF; R=(src)&0xFF; }
#define FROM_RGBA32_PLUS(src,R,G,B,A) { A += ((src)>>24)&0xFF; B+=((src)>>16)&0xFF; G+=((src)>>8)&0xFF; R+=(src)&0xFF; }
#define TO_RGBA32(R,G,B,A,n) ((((A)/(n))&0xFF)<<24) | ((((B)/(n))&0xFF)<<16) | ((((G)/(n))&0xFF)<<8) | ((((R)/(n))&0xFF))

	void Image32::copy( const Image32& a, int w, int h )
	{
		if ( a._width == w && a._height == h )
		{
			copy( a );
			return;
		}

		if ( !w || !h )
		{
			clear();
			return;
		}

		alloc( w, h );

		if ( a._width <= 0 || a._height <= 0 ) { return; }

		//lost scheme, not highest quality, but fast

		std::vector< std::pair<int, int> > p( w );

		int x;
		int wN = a._width / w;
		int wN2 = wN / 2;

		for ( x = 0; x < w; x++ )
		{
			int t = ( x * a._width ) / w;
			int t1 = t - wN2;
			int t2 = t1 + wN;

			if ( t2 == t1 ) { t2 = t1 + 1; }

			p[x].first  = ( t1 >= 0 ) ? t1 : 0;
			p[x].second = ( t2 <= a._width ) ? t2 : a._width;
		}

		int hN = a._height / h;

		int y;

		for ( y = 0; y < h; y++ )
		{

			uint32_t* line =  &_data[y * _width];
			int hN2 = hN / 2;

			if ( !hN )
			{
				const uint32_t* a_line =  &a._data[ ( ( y * a._height ) / h ) * a._width ];

				if ( !wN )
					for ( x = 0; x < w; x++ ) { line[x] = a_line[p[x].first]; }
				else
				{
					for ( x = 0; x < w; x++ )
					{
						unsigned R, G, B, A;
						int n1 = p[x].first;
						int n2 = p[x].second;
						FROM_RGBA32( a_line[n1], R, G, B, A );

						for ( int i = n1 + 1; i < n2; i++ ) { FROM_RGBA32_PLUS( a_line[i], R, G, B, A ); }

						n2 -= n1;
						line[x] = n2 ? TO_RGBA32( R, G, B, A, n2 ) : 0xFF;
					}
				}
			}
			else
			{

				int t = ( y * a._height ) / h;
				int t1 = t - hN2;
				int t2 = t1 + hN;

				if ( t1 < 0 ) { t1 = 0; }

				if ( t2 > a._height ) { t2 = a._height; }

				for ( x = 0; x < w; x++ )
				{

					unsigned R = 0, G = 0, B = 0, A = 0;
					int n1 = p[x].first;
					int n2 = p[x].second;

					for ( int iy = t1; iy < t2; iy++ )
					{
						const uint32_t* a_line =  &a._data[ iy * a._width ];

						for ( int i = n1; i < n2; i++ ) { FROM_RGBA32_PLUS( a_line[i], R, G, B, A ); }
					}

					n2 -= n1;
					n2 *= ( t2 - t1 );
					line[x] = n2 ? TO_RGBA32( R, G, B, A, n2 ) : 0xFF00;
				}
			}
		}
	}

	void Image32::copy( const XPMImage& xpm )
	{
		const unsigned* colors = xpm.Colors();
		const int* data = xpm.Data();
		clear();
		int w = xpm.Width();
		int h = xpm.Height();

		if ( w <= 0 || h <= 0 ) { return; }

		alloc( w, h );

		for ( size_t i = 0; i != _data.size( ); i++, data++ )
		{
			_data[i] = ( *data >= 0 ) ? colors[*data] : 0xFF000000;
		}
	}


//////////// XPMImage

	void XPMImage::Clear()
	{
		colors.clear();
		colorCount = 0;
		none = -1;
		width = height = 0;
		data.clear();
	}

	struct color_tree_node
	{
		bool isColor;
		union
		{
			color_tree_node* p[0x100];
			int c[0x100];
		};
		color_tree_node( bool _is_color )
			:  isColor( _is_color )
		{
			int i;

			if ( _is_color ) { for ( i = 0; i < 0x100; i++ ) { c[i] = -1; } }
			else { for ( i = 0; i < 0x100; i++ ) { p[i] = 0; } }
		}
		~color_tree_node();
	};

	color_tree_node::~color_tree_node()
	{
		if ( !isColor )
		{
			for ( int i = 0; i < 0x100; i++ ) if ( p[i] ) { delete p[i]; }
		}
	}

	static void InsertColorId( color_tree_node* p, const char* s, int count, int colorId )
	{
		if ( !p ) { return; }

		while ( count > 0 )
		{
			unsigned char n = *( s++ );
			count--;


			if ( p->isColor )
			{

				if ( !count )
				{
					p->c[n] = colorId;
				}

				//else ignore

				return;
			}
			else
			{
				if ( count )
				{
					if ( !p->p[n] )
					{
						p->p[n] = new color_tree_node( count == 1 );
					}

					p = p->p[n];
				}
				else { return; }   //else ignore
			}
		}
	}

	static int GetColorId( color_tree_node* p, const char* s, int count )
	{
		while ( p && count > 0 )
		{
			unsigned char n = *( s++ );
			count--;

			if ( p->isColor )
			{
				return p->c[n];
			}
			else
			{
				p = p->p[n];
			}
		}

		return -1;
	}

	inline void SS( const char*& s ) { while ( *s > 0 && *s <= 32 ) { s++; } }

	static int SInt( const char*& s )
	{
		int n = 0;

		for ( ; *s && *s >= '0' && *s <= '9'; s++ )
		{
			n = n * 10 + ( *s - '0' );
		}

		return n;
	}

	static unsigned SHex( const char*& s )
	{
		unsigned n = 0;

		for ( ; *s; s++ )
		{
			if ( *s >= '0' && *s <= '9' )
			{
				n = ( n << 4 ) + ( *s - '0' );
			}
			else if ( *s >= 'a' && *s <= 'f' )
			{
				n = ( n << 4 ) + ( *s - 'a' ) + 10;
			}
			else if ( *s >= 'A' && *s <= 'F' )
			{
				n = ( n << 4 ) + ( *s - 'A' ) + 10;
			}
			else { return n; }
		}

		return n;
	}

	static unsigned HSVtoRGB( unsigned hsv )
	{
		return hsv; //!!!
	}


	bool XPMImage::Load( const char** list, int count )
	{
		Clear();

		if ( count < 1 )
		{
//printf("err 0\n");
			return false;
		}

		const char* s = list[0];

		list++;
		count--;

		SS( s );
		int w = SInt( s );
		SS( s );
		int h = SInt( s );
		SS( s );
		int nc = SInt( s );
		SS( s );
		int cpp = SInt( s );

		if ( !( w > 0 && h > 0 && nc > 0 && cpp > 0 ) || w > 16000 || h > 16000 || nc > 0xFFFFF || cpp > 4 )
		{
//printf("err 1\n");
			return false;
		}

		colors.resize( nc );
		{
			for ( int i = 0; i < nc; i++ ) { colors[i] = 0; }
		}
		data.resize( w * h );
		{
			int* p = data.data();

			for ( int n = w * h; n > 0; n--, p++ ) { *p = -1; }
		}

		colorCount = nc;
		width = w;
		height = h;


		int i;

		color_tree_node tree( cpp == 1 );

		for ( i = 0; i < nc && count > 0; i++, count--, list++ )
		{
			const char* s = list[0];
			int l = strlen( s );

			if ( l <= cpp )
			{
				return false;
//printf("err 2\n");
			}

			s += cpp;


			while ( *s )
			{
				SS( s );

				if ( *s == 'c' )
				{
					s++;
					SS( s );

					if ( *s == '#' )
					{
						s++;
						unsigned c = SHex( s );
						colors[i] = ( c & 0xFF00FF00 ) | ( ( c >> 16 ) & 0xFF ) | ( ( c << 16 ) & 0xFF0000 );

					}
					else if ( *s == '%' )
					{
						s++;
						colors[i] = HSVtoRGB( SHex( s ) );
					}
					else if (    ( s[0] == 'N' || s[0] == 'n' ) &&
					             ( s[1] == 'O' || s[1] == 'o' ) &&
					             ( s[2] == 'N' || s[2] == 'n' ) &&
					             ( s[3] == 'E' || s[3] == 'e' )
					        )
					{
						s += 4;
						break;
					}
					else
					{
						//ignore
						while ( *s < 0 || *s > 32 ) { s++; }
					}

					InsertColorId( &tree, list[0], cpp, i );

					break;
				}
				else
				{
					break;   //ignore
				}
			};


		}

		int* dataPtr = data.data();

		for ( i = 0; i < h && count > 0; i++, count--, list++, dataPtr += width )
		{
			const char* s = list[0];
			int l = strlen( s );
			int n = l / cpp;

			for ( int j = 0; j < n; j++, s += cpp )
			{
				int id = GetColorId( &tree, s, cpp );
//printf("%i,", id);
				dataPtr[j] = id;
			}
		}

		return true;
	}

/////////////////////////// cicon //////////////////////////////////

	Mutex iconCopyMutex;
	Mutex iconListMutex;

	static std::unordered_map< int, ccollect< clPtr< cicon > > > cmdIconList;

	void cicon::SetCmdIcon( int cmd, const Image32& image, int w, int h )
	{
		MutexLock lock( &iconListMutex );
		ccollect< clPtr<cicon> >& p = cmdIconList[cmd];
		int n = p.count();

		clPtr<cicon> pIcon = new cicon();
		pIcon->Load( image, w, h );

		for ( int i = 0; i < n; i++ )
		{
			if ( p[i]->Width() == w && p[i]->Height() == h )
			{
				p[i] = pIcon;
				return;
			}
		}

		cmdIconList[cmd].append( pIcon );
	}

	void cicon::SetCmdIcon( int cmd, const char** s, int w, int h )
	{
		XPMImage im;
		im.Load( s, 200000 );
		Image32 image32;
		image32.copy( im );
		cicon::SetCmdIcon( cmd, image32, w, h );
	}


	void cicon::ClearCmdIcons( int cmd )
	{
		MutexLock lock( &iconListMutex );
		cmdIconList.erase( cmd );
	}

	inline int IconDist( const cicon& a, int w, int h )
	{
		w -= a.Width();

		if ( w < 0 ) { w = -w; }

		h -= a.Height();

		if ( h < 0 ) { h = -h; }

		return w + h;
	}


	void cicon::Load( int cmd, int w, int h )
	{
		if ( data ) { Clear(); }

		MutexLock lock( &iconListMutex );

		auto i = cmdIconList.find( cmd );

		if ( i == cmdIconList.end() ) { return; }

		ccollect< clPtr<cicon> >* p = &( i->second );

		if ( !p ) { return; }

		int n = p->count();

		if ( n <= 0 ) { return; }

		clPtr<cicon>* t = p->ptr();

		cicon* minIcon = t[0].ptr();
		t++;

		int minDist = IconDist( *minIcon, w, h );


		if ( !minDist )
		{
			Copy( *minIcon );
			return;
		}

		for ( int i = 1; i < n; i++, t++ )
		{
			int dist = IconDist( t[i].ptr()[0], w, h );

			if ( !dist )
			{
				Copy( t[i].ptr()[0] );
				return;
			}

			if ( minDist > dist )
			{
				minIcon = t[i].ptr();
				minDist = dist;
			}
		}

		Load( minIcon->data->image, w, h );
	}

	void cicon::Load( const char** list, int w, int h )
	{
		XPMImage xpm;
		xpm.Load( list, 100000 );
		Image32 im;
		im.copy( xpm );
		Load( im, w, h );
	}

	void cicon::Copy( const cicon& a )
	{
		if ( data ) { Clear(); }

		if ( !a.data ) { return; }

		MutexLock lock( &iconCopyMutex );
		data = a.data;
		data->counter++;
	}

	void cicon::Load( const Image32& image, int w, int h )
	{
		if ( data ) { Clear(); }

		IconData* p = new IconData;

		try
		{
			p->counter = 1;
//printf("load cicon %i, %i\n", w, h);
			p->image.copy( image, w, h );
		}
		catch ( ... )
		{
			delete p;
			throw;
		}

		data = p;
	}


	void cicon::Clear()
	{
		if ( data )
		{
			MutexLock lock( &iconCopyMutex );
			data->counter--;

			if ( data->counter <= 0 ) { delete data; }

			data = 0;
		}
	}


	inline unsigned Dis( unsigned a )
	{
		unsigned b = ( ( a >> 16 ) & 0xFF );
		unsigned g = ( ( a >> 8 ) & 0xFF );
		unsigned r = ( ( a ) & 0xFF );
		unsigned m =  ( r + g + b ) / 2 + 30;

		if ( m > 230 ) { m = 230; }

		return ( a & 0xFF000000 ) + ( m << 16 ) + ( m << 8 ) + m;
	}

	void MakeDisabledImage32( Image32* dest, const Image32& src )
	{
		if ( !dest ) { return; }

		dest->copy( src );
		int n = dest->height() * dest->width();

		if ( n <= 0 ) { return; }

		uint32_t* p = dest->line( 0 );

		for ( ; n > 0; n--, p++ )
		{
			*p = Dis( *p );
		}
	}
}; //namespace wal

namespace wal
{

////////////////////////////  Ui

	UiCache::UiCache(): updated( false ) {}
	UiCache::~UiCache() {}

	int GetUiID( const char* name )
	{
		static std::unordered_map< std::string, int> hash;
		static int id = 0;

		auto i = hash.find( name );

		if ( i != hash.end() ) { return i->second; }

		id++;
		hash[name] = id;

//printf("%3i '%s'\n", id, name);

		return id;
	}

	class UiStream
	{
	public:
		UiStream() {}
		enum { EOFCHAR = EOF };
		virtual const char* Name();
		virtual int Next();
		virtual ~UiStream();
	};

	class UiStreamFile: public UiStream
	{
		std::vector<sys_char_t> _name;
		std::string _name_utf8;
		BFile _f;
	public:
		UiStreamFile( const sys_char_t* s ): _name( new_sys_str( s ) ), _name_utf8( sys_to_utf8( s ) ) { _f.Open( s ); }
		virtual const char* Name();
		virtual int Next();
		virtual ~UiStreamFile();
	};

	class UiStreamMem: public UiStream
	{
		const char* s;
	public:
		UiStreamMem( const char* data ): s( data ) {}
		virtual int Next();
		virtual ~UiStreamMem();
	};


	const char* UiStream::Name() { return ""; }
	int UiStream::Next() { return EOFCHAR; }
	UiStream::~UiStream() {}

	const char* UiStreamFile::Name() { return _name_utf8.data(); }
	int UiStreamFile::Next() { return _f.GetC(); }
	UiStreamFile::~UiStreamFile() {}

	int UiStreamMem::Next() { return *s ? *( s++ ) : EOF; }
	UiStreamMem::~UiStreamMem() {}


	struct UiParzerBuf
	{
		std::vector<char> data;
		int size;
		int pos;
		UiParzerBuf(): size( 0 ), pos( 0 ) {}
		void Clear() { pos = 0; }
		void Append( int c );
	};

	void UiParzerBuf::Append( int c )
	{
		if ( pos >= size )
		{
			int n = ( pos + 1 ) * 2;
			std::vector<char> p( n );

			if ( pos > 0 )
			{
				memcpy( p.data(), data.data(), pos );
			}

			data = p;
			size = n;
		}

		data[pos++] = c;
	}


///////////////////////////////// UiParzer
	class UiParzer
	{
		UiStream* _stream;
		int _cc;
		int _tok;
		int NextChar();
		void SS() { while ( _cc >= 0 && _cc <= ' ' ) { NextChar(); } }
		UiParzerBuf _buf;
		int64_t _i;
		int _cline;
	public:
		enum TOKENS
		{
			TOK_ID = -500,   // /[_\a][_\a\d]+/
			TOK_STR, // "...\?..."
			TOK_INT,
			TOK_EOF
		};

		void Syntax( const char* s = "" );
		UiParzer( UiStream* stream );
		int Tok() const { return _tok; }
		int64_t Int() const { return _i; }
		const char* Str() { return _buf.data.data(); }
		int Next();
		void Skip( int tok );
		void SkipAll( int tok, int mincount = 1 );
		~UiParzer();
	};

	void UiParzer::Skip( int t )
	{
		if ( _tok != t )
		{
			Syntax( GetFormattedString( "symbol not found '%c'", t ).c_str() );
		}

		Next();
	}

	void UiParzer::SkipAll( int t, int mincount )
	{
		int n;

		for ( n = 0; _tok == t; n++ ) { Next(); }

		if ( n < mincount )
		{
			Syntax( GetFormattedString( "symbol not found '%c'", t ).c_str() );
		}
	}

	void UiParzer::Syntax( const char* s )
	{
		throw_msg( "Syntax error in '%s' line %i: %s", _stream->Name(), _cline + 1, s );
	}

	UiParzer::UiParzer( UiStream* stream )
		:  _stream( stream ),
		   _cc( 0 ),
		   _tok( 0 ),
		   _cline( 0 )
	{
		NextChar();
		Next();
	}

	UiParzer::~UiParzer() {}

	int UiParzer::NextChar()
	{
		if ( _cc == UiStream::EOFCHAR )
		{
			return _cc;
		}

		_cc = _stream->Next();

		if ( _cc == '\n' ) { _cline++; }

		return _cc;
	}



	int UiParzer::Next()
	{
begin:
		SS();

		if ( _cc == '_' || IsAlpha( _cc ) )
		{
			_buf.Clear();
			_buf.Append( _cc );
			NextChar();

			while ( IsAlpha( _cc ) || IsDigit( _cc ) || _cc == '_' || _cc == '-' )
			{
				_buf.Append( _cc );
				NextChar();
			};

			_buf.Append( 0 );

			_tok = TOK_ID;

			return _tok;
		}

		if ( _cc == '/' )
		{
			NextChar();

			if ( _cc != '/' )
			{
				_tok = '/';
				return _tok;
			}

			//comment
			while ( _cc != UiStream::EOFCHAR && _cc != '\n' )
			{
				NextChar();
			}

			goto begin;
		}

		if ( _cc == '"' || _cc == '\'' )
		{
			_buf.Clear();
			int bc = _cc;
			NextChar();

			while ( _cc != bc )
			{
				if ( _cc == UiStream::EOFCHAR )
				{
					Syntax( "invalid string constant" );
				}

				if ( _cc == '\\' )
				{
					NextChar();

					if ( _cc == UiStream::EOFCHAR )
					{
						Syntax( "invalid string constant" );
					}
				}

				_buf.Append( _cc );
				NextChar();
			}

			_buf.Append( 0 );
			_tok = TOK_STR;
			NextChar();
			return _tok;
		}

		if ( IsDigit( _cc ) )
		{
			int64_t n = 0;

			if ( _cc == '0' )
			{
				NextChar();


				if ( _cc == 'x' || _cc == 'X' )
				{
					NextChar();

					while ( true )
					{
						if ( IsDigit( _cc ) )
						{
							n = n * 16 + ( _cc - '0' );
						}
						else if ( _cc >= 'a' && _cc <= 'f' )
						{
							n = n * 16 + ( _cc - 'a' ) + 10;
						}
						else if ( _cc >= 'A' && _cc <= 'F' )
						{
							n = n * 16 + ( _cc - 'A' ) + 10;
						}
						else
						{
							break;
						}

						NextChar();
					};

					_i = n;

					_tok = TOK_INT;

					return _tok;
				}
			}

			for ( ; IsDigit( _cc ); NextChar() )
			{
				n = n * 10 + ( _cc - '0' );
			}

			_i = n;
			_tok = TOK_INT;
			return _tok;
		};

		if ( _cc == UiStream::EOFCHAR )
		{
			_tok = TOK_EOF;
			return _tok;
		}

		_tok = _cc;
		NextChar();
		return _tok;
	}

	int64_t UiValueNode::Int()
	{
		if ( !( flags & INT ) )
		{
			if ( !( flags & STR ) ) { return 0; }

			int64_t n = 0;
			const char* t = s.data();

			if ( !t || !*t ) { return 0; }

//			bool minus = false;

			if ( *t == '-' )
			{
//				minus = true;
				t++;
			}

			if ( *t == '+' ) { t++; }

			for ( ; *t >= '0' && *t <= '9'; t++ ) { n = n * 10 + ( *t - '0' ); }

			i = n;
			flags |= INT;
		}

		return i;
	}

	const char* UiValueNode::Str()
	{
		if ( !( flags & STR ) )
		{
			if ( !( flags & INT ) ) { return ""; }

			s = ToString(i);
			flags |= STR;
		}

		return s.data();
	}

	class UiValue;

	struct UiSelector: public iIntrusiveCounter
	{
		struct Node
		{
			int idC;
			int idN;
			bool oneStep;
			Node(): idC( 0 ), idN( 0 ), oneStep( true ) {}
			Node( int c, int n, bool o ): idC( c ), idN( n ), oneStep( o ) {}
		};

		struct CondNode
		{
			bool no;
			int id;
			CondNode(): no( false ), id( 0 ) {}
			CondNode( bool n, int i ): no( n ), id( i ) {}
		};

		struct ValueNode
		{
			int id;
			UiValue* data;
			ValueNode(): id( 0 ), data( 0 ) {}
			ValueNode( int i, UiValue* v ): id( i ), data( v ) {}
		};

		ccollect<Node> stack;
		int item;
		ccollect<CondNode> cond;
		ccollect<ValueNode> val;

		enum {CMPN = 4};
		unsigned char cmpLev[CMPN];

		int Cmp( const clPtr<UiSelector>& a )
		{
			int i = 0;

			while ( i < CMPN && cmpLev[i] == a->cmpLev[i] ) { i++; }

			return ( i >= CMPN ) ? 0 : cmpLev[i] < a->cmpLev[i] ? -1 : 1;
		};

		void AppendObj( int c, int n, bool o )
		{
			stack.append( Node( c, n, o ) );

			if ( n ) { cmpLev[0]++; }

			if ( c ) { cmpLev[1]++; }
		}

		void AppendCond( bool no, int id ) { cond.append( CondNode( no, id ) ); cmpLev[3]++; }
		void AppendVal( int id, UiValue* v ) { val.append( ValueNode( id, v ) ); }
		void SetItem( int id ) {item = id; cmpLev[2] = id ? 1 : 0; }


		UiSelector(): item( 0 )   { for ( int i = 0; i < CMPN; i++ ) { cmpLev[i] = 0; } }

		void Parze( UiParzer& parzer );
	};

	void UiSelector::Parze( UiParzer& parzer )
	{
		while ( true )
		{
			bool oneStep = false;

			if ( parzer.Tok() == '>' )
			{
				oneStep = true;
				parzer.Next();
			}

			if ( parzer.Tok() == '*' )
			{
				parzer.Next();
				AppendObj( 0, 0, oneStep );
				continue;
			}

			int classId = 0;

			if ( parzer.Tok() == UiParzer::TOK_ID )
			{
				classId = GetUiID( parzer.Str() );
				parzer.Next();

			}

			int nameId = 0;

			if ( parzer.Tok() == '#' )
			{
				parzer.Next();

				if ( parzer.Tok() != UiParzer::TOK_ID )
				{
					parzer.Syntax( "object name not found" );
				}

				nameId = GetUiID( parzer.Str() );
				parzer.Next();
			}

			if ( !classId && !nameId ) { break; }

			AppendObj( classId, nameId, oneStep );
		}

		if ( parzer.Tok() == '@' ) //item
		{
			parzer.Next();

			if ( parzer.Tok() != UiParzer::TOK_ID ) { parzer.Syntax( "item name not found" ); }

			SetItem( item = GetUiID( parzer.Str() ) );
			parzer.Next();
		}

		while ( parzer.Tok() == ':' )
		{
			parzer.Next();
			bool no = false;

			if ( parzer.Tok() == '!' )
			{
				parzer.Next();
				no = true;
			}

			if ( parzer.Tok() != UiParzer::TOK_ID ) { parzer.Syntax( "condition name not found" ); }

			AppendCond( no, GetUiID( parzer.Str() ) );
			parzer.Next();
		}
	}

	bool UiValue::ParzeNode( UiParzer& parzer )
	{
		if ( parzer.Tok() == UiParzer::TOK_INT )
		{
			list.append( new UiValueNode( parzer.Int() ) );
		}
		else   if ( parzer.Tok() == UiParzer::TOK_STR )
		{
			list.append( new UiValueNode( parzer.Str() ) );
		}
		else
		{
			return false;
		}

		parzer.Next();
		return true;
	}

	void UiValue::Parze( UiParzer& parzer )
	{
		if ( parzer.Tok() == '(' )
		{
			parzer.Next();

			while ( true )
			{
				if ( !ParzeNode( parzer ) ) { break; }

				if ( parzer.Tok() != ',' ) { break; }

				parzer.Next();
			};

			parzer.Skip( ')' );
		}
		else
		{
			ParzeNode( parzer );
		}
	}

	UiValue::UiValue() {}
	UiValue::~UiValue() {}

	class UiRules: public iIntrusiveCounter
	{
		friend class UiCache;
		ccollect< clPtr<UiSelector>, 0x100>  selectors;
		UiValue* values;
	public:
		UiRules(): values( 0 ) {}
		void Parze( UiParzer& parzer );
		~UiRules();
	};



	UiRules::~UiRules()
	{
		UiValue* v = values;

		while ( v )
		{
			UiValue* t = v;
			v = v->next;
			delete t;
		}
	}

	void UiRules::Parze( UiParzer& parzer )
	{
		while ( true )
		{
			parzer.SkipAll( ';', 0 );

			if ( parzer.Tok() == UiParzer::TOK_EOF ) { break; }

			ccollect< clPtr<UiSelector> > slist;

			while ( true )
			{
				if ( parzer.Tok() == '{' ) { break; }

				clPtr<UiSelector> sel = new UiSelector();
				slist.append( sel );
				sel->Parze( parzer );

				int i = 0;

				while ( i < selectors.count() )
				{
					clPtr<UiSelector> other = selectors[i];

					if ( sel->Cmp( other ) >= 0 ) { break; }

					i++;
				}

				if ( i < selectors.count() )
				{
					selectors.insert( i, sel );
				}
				else
				{
					selectors.append( sel );
				}

				if ( parzer.Tok() != ',' ) { break; }

				parzer.Next();
			}

			parzer.Skip( '{' );

			if ( !slist.count() ) { parzer.Syntax( "empty list of selectors" ); }

			while ( true )
			{
				if ( parzer.Tok() != UiParzer::TOK_ID ) { break; }

				int id = GetUiID( parzer.Str() );
				parzer.Next();

				parzer.Skip( ':' );

				values = new UiValue( values );
				values->Parze( parzer );

				for ( int i = 0; i < slist.count(); i++ )
				{
					slist[i]->AppendVal( id, values );
				}

				if ( parzer.Tok() == '}' ) { break; }

				parzer.SkipAll( ';' );
			}

			parzer.Skip( '}' );
		}
	}

	void UiCache::Update( UiRules& rules, ObjNode* orderList, int orderlistCount )
	{
//printf("UiCache::Update 1: %i\n", orderlistCount);
		hash.clear();
		clPtr<UiSelector>* psl = rules.selectors.ptr();
		int count = rules.selectors.count();

		for ( int i = 0; count > 0; count--, psl++, i++ )
		{
			clPtr<UiSelector> s = rules.selectors.get( i );
			int scount = s->stack.count();

			if ( scount > 0 )
			{
				if ( orderlistCount > 0 )
				{
					UiSelector::Node* sp = s->stack.ptr() + scount - 1;
					ObjNode* op = orderList;
					int n = orderlistCount;

					bool oneStep = true;

					while ( scount > 0 && n > 0 )
					{
						if ( ( !sp->idC || sp->idC == op->classId ) && ( !sp->idN || sp->idN == op->nameId ) )
						{
							oneStep = sp->oneStep;
							sp--;
							scount--;
							op++;
							n--;
						}
						else
						{
							if ( oneStep ) { break; }

							op++;
							n--;
						}
					}

					if ( scount <= 0 && s->val.count() )
					{
						int count = s->val.count();

//printf("'");
						for ( int i = 0; i < count; i++ )
						{
							hash[s->val[i].id].append( Node( s.ptr(), s->val[i].data ) );
//printf("*");
						}

//printf("'\n");
					}
				}
			}
		}

//printf("cache hash count=%i\n", hash.count());
		updated = true;
	}


	UiValue* UiCache::Get( int id, int item, int* condList )
	{
		auto i = hash.find( id );

		if ( i == hash.end() ) { return nullptr; }

		ccollect<Node>* ids = &( i->second );

		if ( !ids ) { return nullptr; }

		Node* p = ids->ptr();
		int count = ids->count();

		for ( ; count > 0; count--, p++ )
		{
			clPtr<UiSelector> s = p->s;

			if ( !item && s->item ) { continue; }

			if ( item && s->item && s->item != item ) { continue; }

			int i;
			int n = s->cond.count();

			for ( i = 0; i < n; i++ )
			{
				int cid = s->cond[i].id;
				bool no = s->cond[i].no;

				bool found = false;

				if ( condList )
				{
					for ( int* t = condList; *t; t++ )
						if ( *t == cid )
						{
							found = true;
							break;
						}
				}

				if ( found == no ) { break; }
			}

			if ( i >= n ) { return p->v; }
		}

		return 0;
	}


	static clPtr<UiRules> globalUiRules;

	static void ClearWinCache( const WINHASHNODE* pnode, void* )
	{
		if ( pnode->win )
		{
			pnode->win->UiCacheClear();
		}
	}

	void UiReadFile( const sys_char_t* fileName )
	{
		UiStreamFile stream( fileName );
		UiParzer parzer( &stream );

		clPtr<UiRules> rules  = new UiRules();
		rules->Parze( parzer );
		globalUiRules = rules;
		for ( const auto& i : winhash ) ClearWinCache( &(i.second), nullptr );
	}

	void UiReadMem( const char* s )
	{
		UiStreamMem stream( s );
		UiParzer parzer( &stream );

		clPtr<UiRules> rules  = new UiRules();
		rules->Parze( parzer );
		globalUiRules = rules;
		for ( const auto& i : winhash ) ClearWinCache( &(i.second), nullptr );
	}

	void UiCondList::Set( int id, bool yes )
	{
		int i;

		for ( i = 0; i < N; i++ ) if ( buf[i] == id ) { buf[i] = 0; }

		if ( yes )
		{
			for ( i = 0; i < N; i++ )
				if ( !buf[i] )
				{
					buf[i] = id;
					return;
				}
		}
	}

	int uiColor = GetUiID( "color" );
	int uiHotkeyColor = GetUiID( "hotkey-color" );
	int uiBackground = GetUiID( "background" );
	int uiFocusFrameColor = GetUiID( "focus-frame-color" );
	int uiFrameColor = GetUiID( "frame-color" );
	int uiButtonColor = GetUiID( "button-color" );
	int uiMarkColor = GetUiID( "mark-color" );
	int uiMarkBackground = GetUiID( "mark-background" );
	int uiCurrentItem = GetUiID( "current-item" );
	int uiCurrentItemFrame = GetUiID( "current-item-frame-color" );
	int uiLineColor = GetUiID( "line-color" );
	int uiPointerColor = GetUiID( "pointer-color" );
	int uiOdd = GetUiID( "odd" );

	int uiVariable = GetUiID( "variable" );
	int uiValue = GetUiID( "value" );

	int uiEnabled = GetUiID( "enabled" );
	int uiFocus = GetUiID( "focus" );
	int uiItem = GetUiID( "item" );
	int uiClassWin = GetUiID( "Win" );

	unsigned Win::UiGetColor( int id, int itemId, UiCondList* cl, unsigned def )
	{
		if ( !globalUiRules.ptr() ) { return def; }

		int buf[0x100];
		int pos = 0;

		if ( cl )
		{
			for ( int i = 0; i < UiCondList::N && i < 0x100 - 10; i++ )
				if ( cl->buf[i] )
				{
					buf[pos++] = cl->buf[i];
				}
		}

		if ( IsEnabled() )
		{
			buf[pos++] = uiEnabled;
		}

		if ( InFocus() )
		{
			buf[pos++] = uiFocus;
		}

		buf[pos++] = 0;

//printf("1\n");
		if ( !uiCache.Updated() )
		{
//printf("2\n");
			//Update(UiRules &rules, ObjNode *orderList, int orderlistCount)
			ccollect<UiCache::ObjNode> wlist;

			for ( Win* w = this; w; w = w->Parent() )
			{
				wlist.append( UiCache::ObjNode( w->UiGetClassId(), w->UiGetNameId() ) );
			}

			uiCache.Update( *globalUiRules.ptr(), wlist.ptr(), wlist.count() );
		}

//printf("3\n");
		UiValue* v = uiCache.Get( id, itemId, buf );
//printf("4 %p\n", v);
//if (v) printf("5 %i\n", v->Int());
		return v ? unsigned( v->Int() ) : def;
	};

	int Win::UiGetClassId()
	{
		return uiClassWin;
	}

} //namespace wal
