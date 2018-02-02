/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * wcm@linderdaum.com
 */

#include "wal.h"
#include "swl.h"
#include "terminal.h"
#include <termios.h>
#include "wcm-config.h"
#include "globals.h"
#include <sys/ioctl.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>

static void FindLegacyFreeBSDMasterSlave( std::string* masterName, std::string* slaveName, int* masterFd )
{
	if ( !masterName || !slaveName || !masterFd ) return;

	static const char p1[] = "prsPRS";
	static const char p2[] = "0123456789ancdefghijklmnopqrstuv";

	char Buf[1024];

	for ( const char* s1 = p1; *s1; s1++ )
	{
		for ( const char* s2 = p2; *s2; s2++ )
		{
			snprintf( Buf, sizeof(Buf)-1, "/dev/pty%c%c", *s1, *s2 );

			*masterFd = open( Buf, O_RDWR | O_NDELAY );

			if ( *masterFd >= 0 )
			{
				*masterName = Buf;
				snprintf( Buf, sizeof(Buf)-1, "/dev/tty%c%c", *s1, *s2 );
				*slaveName = Buf;
				return;
			}
		}
	}

	throw_syserr( 0, "INTERNAL ERROR: can`t open any  pseudo terminal file" );
}

TerminalStream::TerminalStream()
 :  _masterFd( -1 )
{
	_masterFd = posix_openpt( O_RDWR );

	std::string masterName = "/dev/ptmx";
	std::string slaveName;

	if ( _masterFd >= 0 )
	{
		const char* name = ptsname( _masterFd );

		if ( !name )
		{
			throw_syserr( 0, "INTERNAL ERROR: can`t receive slave name for %s", masterName.c_str() );
		}

		slaveName = name;
	}
	else
	{
		FindLegacyFreeBSDMasterSlave( &masterName, &slaveName, &_masterFd );
	}

	if ( grantpt( _masterFd ) )
	{
		throw_syserr( 0, "INTERNAL ERROR: can`t grant  pseudo terminal (%s)", masterName.c_str() );
	}

	if ( unlockpt( _masterFd ) )
	{
		throw_syserr( 0, "INTERNAL ERROR: can`t unlock  pseudo terminal (%s)", masterName.c_str() );
	}

	_slaveName = new_sys_str( slaveName.c_str() );
}

int TerminalStream::Read( char* buf, int size )
{
	fd_set fr;
	FD_ZERO( &fr );
	FD_SET( _masterFd, &fr );

	timeval time;
	time.tv_sec = 5;
	int n = select( _masterFd + 1, &fr, 0, 0, &time );

	if ( n < 0 ) { return n; }

	n = read( _masterFd, buf, size );

	return n;
}

int TerminalStream::Write( char* buf, int size )
{
	int bytes = size;
	char* p = buf;

	while ( bytes > 0 )
	{
		fd_set fr;
		FD_ZERO( &fr );
		FD_SET( _masterFd, &fr );

		timeval time;
		time.tv_sec = 5;
		int n = select( _masterFd + 1, 0, &fr, 0, &time );

		if ( n < 0 ) { return n; }

		n = write( _masterFd, p, bytes );

		if ( n < 0 ) { return n; }

		p += n;
		bytes -= n;
	}

	return size;
}

int TerminalStream::SetSize( int rows, int cols )
{
	struct winsize ws;
	ws.ws_row = (unsigned short) ( rows > 0 ? rows : 1);
	ws.ws_col = (unsigned short) ( cols > 0 ? cols : 1);
	ws.ws_xpixel = 0;
	ws.ws_ypixel = 0;
	ioctl( _masterFd, TIOCSWINSZ, &ws );
	return 0;
}

TerminalStream::~TerminalStream()
{
	if ( _masterFd >= 0 )
	{
		close( _masterFd );
		_masterFd = -1;
	}
}

void* TerminalInputThreadFunc( void* data );
void* TerminalOutputThreadFunc( void* data );

Terminal::Terminal( /*int maxRows*/ )
	:
	_rows( 10 ),
	_cols( 10 ),
//	_maxRows(maxRows),
	_stream(),
	_shell( _stream.SlaveName() )
{
	_emulator.SetSize( _rows, _cols );
	int err = thread_create( &outputThread, TerminalOutputThreadFunc, this );

	if ( err ) { throw_syserr( err, "INTERNAL can`t create thread (Terminal::Terminal)" ); }
}



/////////////////////////////////////////////////////////////////////////////////////////
int Terminal::SetSize( int r, int c )
{
	_rows = r;
	_cols = c;
	_emulator.SetSize( _rows, _cols );
	_stream.SetSize( r, c );
	return 0;
}

void Terminal::Key( unsigned key, unsigned ch )
{
	switch ( key )
	{
		case VK_F1:
			Output( "\x1bOP", 3 );
			return;

		case VK_F2:
			Output( "\x1bOQ", 3 );
			return;

		case VK_F3:
			Output( "\x1bOR", 3 );
			return;

		case VK_F4:
			Output( "\x1bOS", 3 );
			return;

		case VK_F5:
			Output( "\x1b[15~", 5 );
			return;

		case VK_F6:
			Output( "\x1b[17~", 5 );
			return;

		case VK_F7:
			Output( "\x1b[18~", 5 );
			return;

		case VK_F8:
			Output( "\x1b[19~", 5 );
			return;

		case VK_F9:
			Output( "\x1b[20~", 5 );
			return;

		case VK_F10:
			Output( "\x1b[21~", 5 );
			return;

		case VK_F11:
			Output( "\x1b[23~", 5 );
			return;

		case VK_F12:
			Output( "\x1b[24~", 5 );
			return;

		case VK_UP:
			Output( _emulator.KbIsNormal() ? "\x1b[A" : "\x1bOA" , 3 );
			return;

		case VK_DOWN:
			Output( _emulator.KbIsNormal() ? "\x1b[B" : "\x1bOB" , 3 );
			return;

		case VK_RIGHT:
			Output( _emulator.KbIsNormal() ? "\x1b[C" : "\x1bOC" , 3 );
			return;

		case VK_LEFT:
			Output( _emulator.KbIsNormal() ? "\x1b[D" : "\x1bOD" , 3 );
			return;

		case VK_HOME:
			Output( _emulator.KbIsNormal() ? "\x1b[H" : "\x1bOH" , 3 );
			return;

		case VK_END:
			Output( _emulator.KbIsNormal() ? "\x1b[F" : "\x1bOF" , 3 );
			return;

		case VK_BACK:
			Output( g_WcmConfig.terminalBackspaceKey ? 8 : 127 );
			return; //херово конечно без блокировок обращаться, но во время работы терминала конфиг не меняется

		case VK_DELETE:
			Output( "\x1b[3~", 4 );
			return;

		case VK_NEXT:
			Output( "\x1b[6~", 4 );
			return;

		case VK_PRIOR:
			Output( "\x1b[5~", 4 );
			return;

		case VK_INSERT:
			Output( "\x1b[2~", 4 );
			return;

	}

	if ( !ch ) { return; }

	UnicodeOutput( ch );
}


int Terminal::ReadOutput( char* buf, int size )
{
	int i;

	for ( i = 0; i < size && !outQueue.Empty(); i++ )
	{
		buf[i] = outQueue.Get();
	}

	return i;
}

void Terminal::Output( const char c )
{
	MutexLock lock( &_outputMutex );
	outQueue.Put( c );
	_outputCond.Signal();
}

void Terminal::Output( const char* s, int size )
{
	if ( size <= 0 ) { return; }

	MutexLock lock( &_outputMutex );
	outQueue.Put( s, size );
	_outputCond.Signal();
}

inline void Terminal::OutAppendUnicode( unicode_t c )
{
	if ( !c ) { return; }

//пока в utf8

	if ( c < 0x800 )
	{
		if ( c < 0x80 )
		{
			outQueue.Put( c );
		}
		else
		{
			outQueue.Put( 0xC0 | ( c >> 6 ) );
			outQueue.Put( 0x80 | ( c & 0x3F ) );
		}
	}
	else
	{
		if ( c < 0x10000 ) //1110xxxx 10xxxxxx 10xxxxxx
		{
			outQueue.Put( 0x80 | ( c & 0x3F ) );
			c >>= 6;
			outQueue.Put( 0x80 | ( c & 0x3F ) );
			c >>= 6;
			outQueue.Put( 0xE0 | ( c & 0x0F ) );
		}
		else     //11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		{
			outQueue.Put( 0x80 | ( c & 0x3F ) );
			c >>= 6;
			outQueue.Put( 0x80 | ( c & 0x3F ) );
			c >>= 6;
			outQueue.Put( 0x80 | ( c & 0x3F ) );
			c >>= 6;
			outQueue.Put( 0xF0 | ( c & 0x07 ) );
		}
	}
}

void Terminal::UnicodeOutput( const unicode_t c )
{
	MutexLock lock( &_outputMutex );
	OutAppendUnicode( c );
	_outputCond.Signal();
}

void Terminal::UnicodeOutput( const unicode_t* s, int size )
{
	if ( size <= 0 ) { return; }

	MutexLock lock( &_outputMutex );

	for ( ; size > 0; size--, s++ ) { OutAppendUnicode( *s ); }

	_outputCond.Signal();
}


void Terminal::TerminalReset( bool clearScreen )
{
	MutexLock lock( &_inputMutex );
	_emulator.Reset( clearScreen );
}

void Terminal::TerminalPrint( const unicode_t* str, unsigned fg, unsigned bg )
{
	MutexLock lock( &_inputMutex );
	_emulator.InternalPrint( str, fg, bg );
}

#include <stdarg.h>


//#define  DBG dbg_printf
#define  DBG printf


void* TerminalInputThreadFunc( void* data )
{
	Terminal* terminal = ( Terminal* )data;

	while ( true )
	{
		while ( true )
		{
			char buffer[1024];
			int n = terminal->_stream.Read( buffer, sizeof( buffer ) );

			if ( n < 0 )
			{
				//...
				break;
			}

			if ( !n )
			{
				break; //eof
			}

			MutexLock lock( &terminal->_inputMutex );

			for ( int i = 0; i < n; i++ )
			{
				terminal->CharInput( buffer[i] );
			}

			WinThreadSignal( 0 );
		}

		//скорее всего shell сломался (перезапустится), ждем 1 секунду
		sleep( 1 );
	}

	return nullptr;
}

void* TerminalOutputThreadFunc( void* data )
{
	Terminal* terminal = ( Terminal* )data;

	while ( true )
	{
		while ( true )
		{
			terminal->_outputMutex.Lock();
			char buffer[0x100];
			int bytes = terminal->ReadOutput( buffer, sizeof( buffer ) );

			if ( bytes <= 0 )
			{
				timespec time;
				time.tv_sec = 5;
				terminal->_outputCond.TimedWait( &terminal->_outputMutex, &time );
				terminal->_outputMutex.Unlock();
				continue;
			}

			terminal->_outputMutex.Unlock();

			char* p = buffer;

			while ( bytes > 0 )
			{
				int n = terminal->_stream.Write( p, bytes );

				if ( n < 0 )
				{
					//...
					break;
				}

				p += n;
				bytes -= n;
			}
		}

		//скорее всего shell сломался (перезапустится), ждем 1 секунду
		sleep( 1 );
	}

	return nullptr;
}
