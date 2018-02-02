/*
   Copyright (c) by Valery Goryachev (Wal)
*/


#ifndef WAL_SYS_API_H
#define WAL_SYS_API_H

#ifdef _WIN32
#  if !defined( NOMINMAX )
#     define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <pthread.h>
#  include <string.h>
#  include <stdlib.h>
#endif

#include <errno.h>

#include "System/Types.h"

namespace wal
{
	typedef wchar_t unicode_t;

	typedef int64_t seek_t;

#ifdef _WIN32
	//typedef unsigned short sys_char_t;
	typedef wchar_t sys_char_t;

	inline int sys_strlen( const sys_char_t* s ) { return int( wcslen( s ) ); }

	typedef HANDLE file_t;

#  define FILE_NULL INVALID_HANDLE_VALUE

	inline bool valid_file( file_t fd ) { return fd != FILE_NULL; }

#  define DIR_SPLITTER '\\'

#else
	typedef char sys_char_t;
	inline int sys_strlen( const sys_char_t* s ) { return strlen( s ); }

	typedef int file_t;

	enum {FILE_NULL = -1 };

	inline bool valid_file( file_t fd ) { return fd >= 0; }

#  define DIR_SPLITTER '/'

	typedef pthread_t thread_t;
	typedef pthread_mutex_t mutex_t;
	typedef pthread_cond_t cond_t;


#endif

/////////////////////////////////////////// charset defs
	struct charset_struct
	{
		int id;
		const char* name;
		const char* comment;

		char tabChar;

		unicode_t* ( *cs_to_unicode )( unicode_t* buf, const char* s, int size, int* badCount );
		char* ( *unicode_to_cs )( char* s, const unicode_t* buf, int usize, int* badCount );
		int ( *symbol_count )( const char* s, int size );
		int ( *string_buffer_len )( const unicode_t* s, int ulen );

		bool IsTab( char* s, char* endLine ) { return ( s && s < endLine && *s == tabChar ); }
		char* ( *GetNext )( char* s, char* endLine );
		char* ( *_GetPrev )( char* s, char* beginLine );
		unicode_t ( *GetChar )( char* s, char* endLine );
		int ( *SetChar )( char* buf, unicode_t ch ); //return count of chars in buffer (buf is 8 chars minimun)
		char* GetPrev( char* s, char* beginLine );

		charset_struct( int _id, const char* _name, const char* _comment );
	};

	inline char* charset_struct::GetPrev( char* s, char* beginLine )
	{
		if ( _GetPrev ) { return _GetPrev( s, beginLine ); }

		if ( s <=  beginLine ) { return 0; }

		char* p = 0;
		char* t = beginLine;

		while ( t )
		{
			p = t;
			t = GetNext( t, s );
		}

		return p;
	}


	enum CHARSET_ID
	{
		CS_LATIN1 = 0,
		CS_ISO8859_1 = 0,
		CS_UTF8 = 1,
		CS_CP437,
		CS_CP737,
		CS_CP775,
		CS_CP850,
		CS_CP852,
		CS_CP855,
		CS_CP857,
		CS_CP860,
		CS_CP861,
		CS_CP862,
		CS_CP863,
		CS_CP864,
		CS_CP865,
		CS_CP866,
		CS_CP869,
		CS_CP874,
		CS_EBCDIC_037,
		CS_EBCDIC_1025,
		CS_EBCDIC_1026,
		CS_EBCDIC_500,
		CS_EBCDIC_875,

		CS_ISO8859_2,
		CS_ISO8859_3,
		CS_ISO8859_4,
		CS_ISO8859_5,
		CS_ISO8859_6,
		CS_ISO8859_7,
		CS_ISO8859_8,
		CS_ISO8859_9,
		CS_ISO8859_10,
		CS_ISO8859_11,
		CS_ISO8859_13,
		CS_ISO8859_14,
		CS_ISO8859_15,
		CS_ISO8859_16,
		CS_KOI8R,
		CS_KOI8U,
		CS_MAC_CYRILLIC,
		CS_MAC_GREEK,
		CS_MAC_ICELAND,
		CS_MAC_LATIN2,
		CS_MAC_ROMAN,
		CS_MAC_TURKISH,
		CS_WIN1250,
		CS_WIN1251,
		CS_WIN1252,
		CS_WIN1253,
		CS_WIN1254,
		CS_WIN1255,
		CS_WIN1256,
		CS_WIN1257,
		CS_WIN1258
		//CS_end
	};


#ifndef _WIN32
	extern int sys_charset_id;
#endif

	extern charset_struct charsetLatin1;
//extern charset_struct charsetUtf8;

	class CharsetTable
	{
//public:
		enum { TABLESIZE = 128 };
		charset_struct* table[TABLESIZE];
	public:
		CharsetTable(); //{ for (int i = 0; i<TABLESIZE; i++) table[i]=&charsetLatin1; }
		void Add( charset_struct* );
		charset_struct* Get( int id ) { return id >= 0 && id < TABLESIZE && table[id] ? table[id] : &charsetLatin1; }
		charset_struct* operator [] ( int id ) { return Get( id ); }
		const char* NameById( int id );
		int IdByName( const char* s );
		int GetList( charset_struct**, int size );
	};

	extern CharsetTable charset_table;

	/*inline charset_struct* get_charset(int id){
	   charset_struct *p = charset_table;
	   while (p->id != id && p->id>=0) p++;
	   return p->id >=0 ? p : 0;
	}*/

	/*
	const char *CharsetNameByID(int id);
	int CharsetIdByName(const char *s);
	*/

//////////////////////////////////////////

	enum OPEN_FILE_FLAG
	{
		FOPEN_READ = 1,
		FOPEN_WRITE = 2,
		FOPEN_RW = 3,

		FOPEN_CREATE = 4, //open if exist and create if not exist
		FOPEN_SYNC = 8,
		FOPEN_TRUNC = 16 //truncate if exist
	};

	enum SEEK_FILE_MODE
	{
#ifdef _WIN32
		FSEEK_BEGIN = FILE_BEGIN,
		FSEEK_POS   = FILE_CURRENT,
		FSEEK_END   = FILE_END
#else
		FSEEK_BEGIN = SEEK_SET,
		FSEEK_POS   = SEEK_CUR,
		FSEEK_END   = SEEK_END
#endif
	};

#ifdef _WIN32
	inline bool SysErrorIsFileNotFound( int e ) { return e == ERROR_FILE_NOT_FOUND; }
#else
	inline bool SysErrorIsFileNotFound( int e ) { return e == ENOENT; }
#endif


#ifdef _WIN32

	//return count of characters (length of buffer for conversion to unicode)
	extern int sys_symbol_count( const sys_char_t* s, int len = -1 );

	//return count of sys_chars for saving s
	extern int sys_string_buffer_len( const unicode_t* s, int ulen = -1 ); //not including \0

	// return point of appended \0
	extern unicode_t* sys_to_unicode(
	   unicode_t* buf,
	   const sys_char_t* s, int len = -1,
	   int* badCount = 0 );

	// return point of appended \0
	extern sys_char_t* unicode_to_sys(
	   sys_char_t* s,
	   const unicode_t* buf, int ulen = -1,
	   int* badCount = 0 );

#else
	//return count of characters (length of buffer for conversion to unicode)
	extern int ( *fp_sys_symbol_count )( const sys_char_t* s, int len );

	//return count of sys_chars for saving s
	extern int ( *fp_sys_string_buffer_len )( const unicode_t* s, int ulen ); //not including \0

	// return point of appended \0
	extern unicode_t* ( *fp_sys_to_unicode )(
	   unicode_t* buf,
	   const sys_char_t* s, int len,
	   int* badCount );

	// return point of appended \0
	extern sys_char_t* ( *fp_unicode_to_sys )(
	   sys_char_t* s,
	   const unicode_t* buf, int len,
	   int* badCount );

	inline int sys_symbol_count( const sys_char_t* s, int len = -1 ) { return  fp_sys_symbol_count( s, len ); } ;
	inline int sys_string_buffer_len( const unicode_t* s, int ulen = -1 ) { return fp_sys_string_buffer_len( s, ulen ); }
	inline unicode_t* sys_to_unicode(
	   unicode_t* buf,
	   const sys_char_t* s, int len = -1,
	   int* badCount = 0 ) { return fp_sys_to_unicode( buf, s, len, badCount ); }

	inline sys_char_t* unicode_to_sys(
	   sys_char_t* s,
	   const unicode_t* buf, int ulen = -1,
	   int* badCount = 0 ) { return fp_unicode_to_sys( s, buf, ulen, badCount ); }
#endif

	unicode_t*  latin1_to_unicode ( unicode_t* buf, const char* s, int size = -1, int* badCount = 0 );
	char*    unicode_to_latin1 ( char* s, const unicode_t* buf, int usize = -1, int* badCount = 0 );
	int      latin1_symbol_count  ( const char* s, int size = -1 );
	int      latin1_string_buffer_len( const unicode_t* buf, int ulen = -1 );

	unicode_t*  utf8_to_unicode      ( unicode_t* buf, const char* s, int size = -1, int* badCount = 0 );
	char*    unicode_to_utf8      ( char* s, const unicode_t* buf, int usize = -1, int* badCount = 0 );
	int      utf8_symbol_count ( const char* s, int size = -1 );
	int      utf8_string_buffer_len  ( const unicode_t* buf, int ulen = -1 );


	inline int get_sys_error()
	{
#ifdef _WIN32
		return GetLastError();
#else
		return errno;
#endif
	}

	inline void set_sys_error( int e )
	{
#ifdef _WIN32
		SetLastError( e );
#else
		errno = e;
#endif
	}

	sys_char_t* sys_error_str( int err, sys_char_t* buf, int size );

	int unicode_strlen( const unicode_t* s );
	unicode_t* unicode_strchr( const unicode_t* s, unicode_t c );
	unicode_t* unicode_strrchr(const unicode_t* s, unicode_t c );
	unicode_t* unicode_strcpy( unicode_t* d, const unicode_t* s );
	// copy unlit end of string, or when n chars copid, whichever comes first.
	// d is always 0-ended
	unicode_t* unicode_strncpy0( unicode_t* d, const unicode_t* s, int n );
	void unicode_strcat( unicode_t* d, const unicode_t* s );
	unicode_t* unicode_strdup( const unicode_t* s );

//for internal use
	class SysStringStruct
	{
#define SSS_BUF_SIZE 1024
#define SSS_UNICODE_BUF_SIZE 1024

		sys_char_t buffer[SSS_BUF_SIZE];
		sys_char_t* p;

		void set_oom()
		{
#ifdef _WIN32
			set_sys_error( ERROR_OUTOFMEMORY );
#else
			set_sys_error( ENOMEM );
#endif
		}

		void set_unicode( const unicode_t* s, int len = -1 )
		{
			int sys_len = sys_string_buffer_len( s, len );

			if ( sys_len < SSS_BUF_SIZE ) { p = buffer; }
			else { p = ( sys_char_t* ) malloc( sizeof( sys_char_t ) * ( sys_len + 1 ) ); }

			if ( p )
			{
				unicode_to_sys( p, s, len );
				/*
				printf("len=%i\n", len);
				char *t =p;
				for (;*t;t++)
				   printf("!'%c'\n", char(*t));
				*/

			}
			else { set_oom(); }
		}
#undef SSS_BUF_SIZE


		void set_utf8( const char* s, int len = -1 )
		{
			unicode_t unicode_buffer[SSS_UNICODE_BUF_SIZE];
			int unicode_len = utf8_symbol_count( s, len );
			unicode_t* uPtr = ( unicode_len < SSS_UNICODE_BUF_SIZE ) ? unicode_buffer : ( unicode_t* )malloc( sizeof( unicode_t ) * ( unicode_len + 1 ) );

			if ( uPtr )
			{
				utf8_to_unicode( uPtr, s, len );
				/*
				unicode_t *t =uPtr;
				for (;*t;t++)
				   printf("'%c'\n", char(*t));
				*/

				set_unicode( uPtr, unicode_len );

				if ( uPtr != unicode_buffer ) { free( uPtr ); }
			}
			else { set_oom(); }
		}

	public:
		SysStringStruct( const char* s ): p( 0 ) { set_utf8( s );}
		const sys_char_t* get() { return p; }
		~SysStringStruct() { if ( p && p != buffer ) { free( p ); } }
	};


	file_t file_open( const sys_char_t* filename,  int open_flag = FOPEN_READ );
	file_t file_open_utf8( const char* filename, int open_flag = FOPEN_READ );

	inline int file_read( file_t fid, void* buf, int size )
	{
#ifdef _WIN32
		DWORD retsize;
		return ReadFile( fid, buf, size, &retsize, 0 ) ? retsize : -1;
#else
		return read( fid, buf, size );
#endif
	}

	inline int file_write( file_t fid, void* buf, int size )
	{
#ifdef _WIN32
		DWORD retsize;
		return WriteFile( fid, buf, size, &retsize, 0 ) ? retsize : -1;
#else
		return write( fid, buf, size );
#endif
	}

	inline seek_t file_seek( file_t fid, seek_t distance, SEEK_FILE_MODE method = FSEEK_BEGIN )
	{
#ifdef _WIN32
		LARGE_INTEGER li;
		li.QuadPart = distance;
		li.LowPart = SetFilePointer ( fid, li.LowPart, &li.HighPart, method );

		if ( li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR )
		{
			return -1;
		}

		return li.QuadPart;
#else
		return lseek( fid, distance, method );
#endif
	}

	inline int file_close( file_t fid )
	{
#ifdef _WIN32
		return CloseHandle( fid ) ? 0 : -1;
#else
		return close( fid );
#endif
	}

#ifdef _WIN32

	struct _thread_info;
	typedef _thread_info* thread_t;

	typedef CRITICAL_SECTION mutex_t;

	struct cond_t
	{
		HANDLE ev[2]; //??volatile
	};

	int thread_create( thread_t* th, void* ( *f )( void* ), void* arg, bool detached = false );
	int thread_join( thread_t th, void** val );
	thread_t thread_self();
	inline bool thread_equal( thread_t a, thread_t b ) { return a == b; }

	inline int mutex_create( mutex_t* m ) { InitializeCriticalSection( m ); return 0; }
	inline int mutex_delete( mutex_t* m ) { DeleteCriticalSection( m ); return 0; }
	inline int mutex_lock( mutex_t* m ) { EnterCriticalSection( m ); return 0; }
	inline int mutex_trylock( mutex_t* m ) { return TryEnterCriticalSection( m ) ? 0 : EBUSY ; }
	inline int mutex_unlock( mutex_t* m ) { LeaveCriticalSection( m ); return 0; }

	int cond_create( cond_t* c );
	int cond_delete( cond_t* c );
	int cond_wait( cond_t* c, mutex_t* m );
	int cond_signal( cond_t* c );
	int cond_broadcast( cond_t* c );


//...
#else
	inline int thread_create( thread_t* th, void* ( *f )( void* ), void* arg, bool detached = false )
	{
		int r = pthread_create( th, 0, f, arg );

		if ( !r && detached ) { r = pthread_detach( *th ); }

		return r;
	}

	inline int thread_join( thread_t th, void** val ) { return pthread_join( th, val ); }
	inline int mutex_create( mutex_t* m ) { return pthread_mutex_init( m, 0 ); }
	inline int mutex_delete( mutex_t* m ) { return pthread_mutex_destroy( m ); }
	inline int mutex_lock( mutex_t* m ) { return pthread_mutex_lock( m ); }
	inline int mutex_trylock( mutex_t* m ) { return pthread_mutex_trylock( m ); }
	inline int mutex_unlock( mutex_t* m ) { return pthread_mutex_unlock( m ); }
	inline int cond_create( cond_t* c ) { return pthread_cond_init( c, 0 ); }
	inline int cond_delete( cond_t* c ) { return pthread_cond_destroy( c ); }
	inline int cond_wait( cond_t* c, mutex_t* m ) { return pthread_cond_wait( c, m ); }
	inline int cond_timedwait( cond_t* c, mutex_t* m, timespec* t ) { return pthread_cond_timedwait( c, m, t ); }
	inline int cond_signal( cond_t* c ) { return pthread_cond_signal( c );}
	inline int cond_broadcast( cond_t* c ) { return pthread_cond_broadcast( c );};
	inline thread_t thread_self() { return pthread_self(); }
	inline bool thread_equal( thread_t a, thread_t b ) { return pthread_equal( a, b ) != 0; }

#endif


#ifdef _DEBUG
	void dbg_printf( const char* format, ... );
#else
	inline void dbg_printf( const char* format, ... ) { }
#endif

	const char* sys_locale_lang();
	const char* sys_locale_ter();
	const char* sys_locale_lang_ter();

}; //namespace wal

using namespace wal;

#endif
