/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#pragma once

namespace wal
{

	struct ModalStruct
	{
		bool end;
		int id;
		ModalStruct(): end( false ), id( 0 ) {}
		void EndModal( int _id ) { end = true; id = _id;}
	};

#ifdef _WIN32

	class WthInternalEvent
	{
		HANDLE ev; //??volatile
	public:
		WthInternalEvent(): ev( CreateEvent( 0, TRUE, FALSE, 0 ) )
		{
			if ( !ev )
			{
				fprintf( stderr, "can't create internal pipe (WthInternalEvent)\n" );
				exit( 1 );
			}
		};

		void SetSignal() { SetEvent( ev ); }

		HANDLE SignalFD() { return ev; }

		void ClearSignal() { ResetEvent( ev ); }
		~WthInternalEvent() { if ( ev ) { CloseHandle( ev ); } }
	};

#else

	class WthInternalEvent
	{
		Mutex mutex;
		volatile int fd[2];
		volatile bool signaled;
	public:
		WthInternalEvent(): signaled( false )
		{
			if ( pipe( const_cast<int*>( fd ) ) )
			{
				fprintf( stderr, "can't create internal pipe (WthInternalEvent)\n" );
				exit( 1 );
			}
		};

		void SetSignal()
		{
			MutexLock lock( &mutex );

			if ( signaled ) { return; }

			char c = 0;
			int ret = write( fd[1], &c, sizeof( c ) );

			if ( ret < 0 ) { throw_syserr( 0, "internal error WthInternalEvent" ); }

			signaled = true;
		}

		int SignalFD() { return fd[0]; }

		void ClearSignal()
		{
			MutexLock lock( &mutex );

			if ( !signaled ) { return; }

			char c;
			int ret = read( fd[0], &c, sizeof( c ) );
			signaled = false;
		};

		~WthInternalEvent() { close( fd[0] ); close( fd[1] ); }
	};

#endif

	extern WthInternalEvent wthInternalEvent;


	void wth_CreateThread( Win* w, int id, void* ( *f )( void* ), void* d );
	bool WinThreadSignal( int data );
	void wth_DropWindow( Win* w );
	void wth_DoEvents();


}; //namespace wal
