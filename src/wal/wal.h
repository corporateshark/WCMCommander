/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#ifndef _____WAL_H
#define _____WAL_H

#include "wal_tmpls.h"
#include "wal_sys_api.h"

#include <string>

namespace wal
{

#ifdef _WIN32
#define snprintf _snprintf
#endif

	typedef std::wstring UString;
	typedef std::string LString;

	std::vector<unicode_t> new_unicode_str( const unicode_t* );
	std::vector<sys_char_t> new_sys_str( const sys_char_t* );

	std::vector<sys_char_t> utf8_to_sys( const char* s );
	std::string sys_to_utf8( const sys_char_t* s );

	std::vector<unicode_t> sys_to_unicode_array( const sys_char_t* s, int size = -1 );
	std::vector<sys_char_t> unicode_to_sys_array( const unicode_t* s, int size = -1 );

	std::vector<sys_char_t> sys_error_str( int err );
	std::vector<unicode_t> sys_error_unicode( int err );

	std::string sys_error_utf8( int err );

	std::vector<unicode_t> utf8_to_unicode( const char* s );
	std::string unicode_to_utf8( const unicode_t* u );
	std::string unicode_to_utf8_string( const unicode_t* u );
	std::vector<unicode_t> utf8str_to_unicode( const std::string& s );

	bool unicode_is_equal( const unicode_t* Str, const unicode_t* SubStr );
	int unicode_strcmp(const unicode_t* s1, const unicode_t* s2);
	int unicode_stricmp(const unicode_t* s1, const unicode_t* s2);
	bool unicode_starts_with_and_not_equal( const unicode_t* Str, const unicode_t* SubStr, bool IgnoreCase = false );
	bool utf8_starts_with_and_not_equal( const char* Str, const char* SubStr );
	std::vector<unicode_t> unicode_get_last_word( const unicode_t* Str, const unicode_t** LastWordStart, bool UsePathSeparator );

///////////////////  Exceptions ///////////////////////////////////////////////

//all exceptions created by "new" operator (except oom)
//all must deleted by destroy member function
//messages must be in utf8

	class cexception
	{
	public:
		cexception() {};
		void destroy();
		virtual const char* message();
		virtual ~cexception();
	};

	class coom: public cexception
	{
	public:
		coom() {};
		virtual const char* message();
		virtual ~coom();
	};

	class cmsg: public cexception
	{
		std::vector<char> _msg;
	public:
		cmsg( const char* s );
		virtual const char* message();
		virtual ~cmsg();
	};

	class csyserr: public cmsg
	{
	public:
		int code;
		csyserr( int _code, const char* msg );
		virtual ~csyserr();
	};

	class cstop_exception: public cexception
	{
	public:
		cstop_exception() {};
		virtual const char* message();
		virtual ~cstop_exception();
	};

	void throw_oom();
	void throw_msg( const char* format, ... );
	void throw_syserr( int err = 0, const char* format = 0, ... );
	void throw_stop();

	void init_exceptions();

//////////////////////// ///////////////////////////////////////////////////////////

#define CLASS_COPY_PROTECTION(a) private: a(const a&){}; a& operator = (const a&){return *this;};

////////////////////////  C++ thread wrappers //////////////////////////////////////

#ifndef WAL_THREAD_CHECK
#define WAL_THREAD_CHECK(a) if (!(a)) abort();
#endif

	class MutexLock;
	class Cond;

	class Mutex
	{
		friend class MutexLock;
		friend class Cond;
		mutex_t _mutex;
	public:
		Mutex();
		void Lock();
		void Unlock();
		~Mutex();
	};

	class MutexLock
	{
		Mutex* _pmutex;
		bool _locked;
	public:
		MutexLock( Mutex* m, bool lock = true );
		void Lock();
		void Unlock();
		~MutexLock();
	};

	class Cond
	{
		cond_t _cond;
	public:
		Cond();
		void Wait( Mutex* mutex );
		void Signal();
		void Broadcast();
		~Cond();
	};



////////////////// C++ system file wrapper (bo buffered io)

	class File
	{
		file_t _fd;
		std::vector<sys_char_t> _fileName;
		void Throw( const sys_char_t* name );
		void Throw();
	public:
		File() : _fd( FILE_NULL ) { }
		void Open( const sys_char_t* name, int open_flag = FOPEN_READ );
		void Close();
		int Read( void* buf, int size );
		int Write( void* buf, int size );
		seek_t Seek( seek_t distance, SEEK_FILE_MODE method = FSEEK_BEGIN );
		~File(); //closes without exceptions (better to call Close first)
	};

	class BFile
	{
		File _f;
		char _buf[0x20000];
		int _size;
		int _pos;
		bool FillBuf()
		{
			if ( _size < sizeof( _buf ) ) { return false; }

			_size = _f.Read( _buf, sizeof( _buf ) );
			_pos = 0;
			return _size > 0;
		}
	public:
		BFile(): _size( sizeof( _buf ) ), _pos( sizeof( _buf ) ) {}
		void Open( const sys_char_t* s ) { _f.Open( s ); FillBuf(); }
		void Close() { _f.Close(); _size = sizeof( _buf ); _pos = sizeof( _buf );}
		int GetC() { return ( _pos < _size || FillBuf() ) ? _buf[_pos++] : EOF; }

		char* GetStr( char* b, int bSize )
		{
			char* s = b;
			int n = bSize - 1;

			if ( _pos >= _size && _size < sizeof( _buf ) )
			{
				return 0;
			}

			for ( ; n > 0 ; n--, s++ )
			{
				int c = GetC();

				if ( c == EOF )
				{
					*s = 0;
					return b;
				}

				*s = char( c & 0xFF );

				if ( c == '\n' )
				{
					s++;
					*s = 0;
					return b;
				}
			}

			*s = 0;

			for ( ;; ) { int c = GetC(); if ( c == '\n' || c == EOF ) { break; } }

			return b;
		}

		~BFile() {}
	};


////////////////// C++ thread wrappers implementation //////////////////

//
/// Mutex
//
	inline Mutex::Mutex()
	{
		mutex_create( &_mutex );
	}

	inline void Mutex::Lock()
	{
		mutex_lock( &_mutex );
	}

	inline void Mutex::Unlock()
	{
		mutex_unlock( &_mutex );
	}

	inline Mutex::~Mutex()
	{
		mutex_delete( &_mutex );
	}


//
// MutexLock
//

	inline MutexLock::MutexLock( Mutex* m, bool lock )
		: _pmutex( m ), _locked( lock )
	{
		if ( lock )
		{
			_pmutex->Lock();
		}
	}

	inline void MutexLock::Lock()
	{
		if ( !_locked )
		{
			_pmutex->Lock();
			_locked = true;
		}
	}

	inline void MutexLock::Unlock()
	{
		if ( _locked )
		{
			_pmutex->Unlock();
			_locked = false;
		}
	}

	inline MutexLock::~MutexLock()
	{
		Unlock();
	}

//
//  Cond implementation
//

	inline Cond::Cond()
	{
		cond_create( &_cond );
	}

	inline void Cond::Wait( Mutex* mutex )
	{
		cond_wait( &_cond, &( mutex->_mutex ) );
	}

	inline void Cond::Signal()
	{
		cond_signal( &_cond );
	}

	inline void Cond::Broadcast()
	{
		cond_broadcast( &_cond );
	}

	inline Cond::~Cond()
	{
		cond_delete( &_cond );
	}




///////////////////////////////// system File implamantation

	inline void File::Open( const sys_char_t* name, int open_flag )
	{
		Close();
		_fd = file_open( name, open_flag );

		if ( !valid_file( _fd ) ) { Throw( name ); }

		_fileName = new_sys_str( name );
	}

	inline void File::Close()
	{
		if ( !valid_file( _fd ) ) { return; }

		if ( file_close( _fd ) ) { Throw(); }

		_fileName.clear();

		_fd = FILE_NULL;
	}

	inline int File::Read( void* buf, int size )
	{
		int ret = file_read( _fd, buf, size );

		if ( ret < 0 ) { Throw(); }

		return ret;
	}

	inline int File::Write( void* buf, int size )
	{
		int ret = file_write( _fd, buf, size );

		if ( ret < 0 ) { Throw(); }

		return ret;
	}

	inline seek_t File::Seek( seek_t distance, SEEK_FILE_MODE method )
	{
		seek_t ret = file_seek( _fd, distance, method );

		if ( ret < 0 ) { Throw(); }

		return ret;
	}

	inline File::~File() { if ( _fd != FILE_NULL ) { file_close( _fd ); _fd = FILE_NULL;  } }





//////////////////////////////////////////////////////////////////////////////////
	inline std::vector<unicode_t> sys_to_unicode_array( const sys_char_t* s, int size )
	{
		int l = sys_symbol_count( s, size );
		std::vector<unicode_t> p( l + 1 );

		if ( l > 0 ) { sys_to_unicode( p.data(), s, size ); }

		p[l] = 0;
		return p;
	}

	inline std::vector<sys_char_t> unicode_to_sys_array( const unicode_t* s, int size )
	{
		int l = sys_string_buffer_len( s, size );
		std::vector<sys_char_t> p( l + 1 );

		if ( l > 0 ) { unicode_to_sys( p.data(), s, size ); }

		p[l] = 0;
		return p;
	}

	inline std::vector<sys_char_t> sys_error_str( int err )
	{
		sys_char_t buf[0x100];
		return new_sys_str( sys_error_str( err, buf, sizeof( buf ) / sizeof( sys_char_t ) ) );
	}


	inline std::vector<unicode_t> sys_error_unicode( int err )
	{
		sys_char_t buf[0x100];
		return sys_to_unicode_array( sys_error_str( err, buf, sizeof( buf ) / sizeof( sys_char_t ) ) );
	}

	inline std::string sys_error_utf8( int err )
	{
		sys_char_t buf[0x100];
		return sys_to_utf8( sys_error_str( err, buf, sizeof( buf ) / sizeof( sys_char_t ) ) );
	}

	bool LookAhead( const unicode_t* p, unicode_t* OutNextChar );
	void PopLastNull( std::vector<unicode_t>* S );
	bool LastCharEquals( const std::vector<unicode_t>& S, unicode_t Ch );
	bool LastCharEquals( const unicode_t* S, unicode_t Ch );
	bool IsPathSeparator( const unicode_t Ch );
	/// replace all tabs and spaces with the U+00B7 character
	void ReplaceSpaces( std::vector<unicode_t>* S );
	/// replace trailing tabs and spaces with the U+00B7 character
	void ReplaceTrailingSpaces( std::vector<unicode_t>* S );
	/// compare unicode_t* string to char* string
	bool IsEqual_Unicode_CStr( const unicode_t* U, const char* S, bool CaseSensitive = true );
	/// n is 0...15
	char GetHexChar( int n );
	/// convert int to hexadecimal string
	std::wstring IntToHexStr( uint64_t Value, size_t Padding = 0 );
	/// convert hexadecimal string to int
	uint64_t HexStrToInt( const unicode_t* Str );
	/// Unicode normalization
	std::vector<unicode_t> normalize_unicode_NFC( const unicode_t* str );
	std::string            normalize_utf8_NFC( const char* str );

	std::string GetEnvVariable( const char* VarName );
}; //namespace wal

#endif
