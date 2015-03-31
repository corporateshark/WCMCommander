/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#pragma once

#include <wal.h>

using namespace wal;

template <class T> inline int CmpStr( T* a, T* b )
{
	for ( ; *a && *a == *b; a++, b++ );

	return ( *a ? ( *b ? ( *a < *b ? -1 : ( *a == *b ? 0 : 1 ) ) : 1 ) : ( *b ? -1 : 0 ) );
}


/*
   strings for storing and converting different charsets
      id == -1 - unicode

      rest - as in enum CHARSET_ID
      id == 0   - this is latin1
      id == 1 - utf8
      ...
*/

#define CS_UNICODE (-1)

class cs_string
{
public:
	struct Node: public iIntrusiveCounter
	{
		int m_Encoding;
		std::vector<char> m_ByteBuffer;
	};
private:
	clPtr<Node> m_Data;

public:
	cs_string(): m_Data() {}
	explicit cs_string( const unicode_t* );
	explicit cs_string( const char* utf8Str );

	void set( const unicode_t* s );
	void set( int cs, const void* );
	void set( int cs, const void*, int len );

	cs_string& operator = ( const unicode_t* );
	cs_string& operator = ( const char* utf8Str ) { set( CS_UTF8, utf8Str ); return *this; }

	void copy( const cs_string& a );
	void copy( const cs_string&, int cs );

	int cs() const { return m_Data ? m_Data->m_Encoding    : 0; }
	const void* str() const { return m_Data ? m_Data->m_ByteBuffer.data() : nullptr; }
	void clear() { m_Data = nullptr; }
	bool is_null() const { return m_Data.IsNull(); }
};


class FSPath;

class FSString
{
	friend class FSPath;
	static unicode_t unicode0;
	static char char0;
	cs_string _primary;
	mutable cs_string _temp;
public:
	FSString() {};
	FSString( const FSString& a ): _primary( a._primary ), _temp( a._temp ) {}
	FSString( const unicode_t* u ): _primary( u ) {};
	FSString( const char* utf8str ): _primary( utf8str ) {};
	FSString( const std::string& utf8str ):FSString(utf8str.c_str()) {};
	FSString( int cs, const void* p ) { _primary.set( cs, p ); }
	FSString( int cs, const void* p, int len ) { _primary.set( cs, p, len );  }

	FSString& operator = ( const FSString& a ) { _primary = a._primary; _temp = a._temp; return *this; };
//	FSString& operator = (const unicode_t *u){ _primary = u; _temp.clear(); return *this; }
	FSString& operator = ( const char* utf8str ) { _primary = utf8str; _temp.clear(); return *this; }

	void Clear() { _primary.clear(); _temp.clear(); }
	void ClearTemp() { _temp.clear(); }

	void Copy( const FSString& a ) { _temp.clear(); _primary.copy( a._primary ); }

	int PrimaryCS() { return _primary.cs(); }
	const void* Get( int cs ) const;
	const unicode_t* GetUnicode() const { return ( const unicode_t* )Get( CS_UNICODE ); }
	const char* GetUtf8() const { return ( const char* )Get( CS_UTF8 ); }

	void Set( int cs, const void* p ) { _primary.set( cs, p ); _temp.clear(); }
	void SetUnicode( const unicode_t* u ) { _temp.clear(); _primary = u; }

	void SetSys( const sys_char_t* p ); //{ _temp.clear(); _primary.set( sys_charset_id, p); }

	bool IsNull() const { return _primary.is_null(); }

	bool IsEmpty() const
	{
		if ( _primary.is_null() ) { return true; }

		if ( _primary.cs() == CS_UNICODE ) { return ( ( unicode_t* )_primary.str() )[0] == 0; }

		return ( ( char* )_primary.str() )[0] == 0;
	}

	bool IsDot() const   //.
	{
		if ( IsNull() ) { return false; }

		if ( _primary.cs() == CS_UNICODE )
		{
			return ( ( unicode_t* )_primary.str() )[0] == '.' && ( ( unicode_t* )_primary.str() )[1] == 0;
		}

		return ( ( char* )_primary.str() )[0] == '.' && ( ( char* )_primary.str() )[1] == 0;
	}

	bool IsHome() const   //~
	{
		if ( IsNull() ) { return false; }

		if ( _primary.cs() == CS_UNICODE )
		{
			return ( ( unicode_t* )_primary.str() )[0] == '~' && ( ( unicode_t* )_primary.str() )[1] == 0;
		}

		return ( ( char* )_primary.str() )[0] == '~' && ( ( char* )_primary.str() )[1] == 0;
	}


	bool Is2Dot() const   //..
	{
		if ( IsNull() ) { return false; }

		if ( _primary.cs() == CS_UNICODE )
		{
			return ( ( unicode_t* )_primary.str() )[0] == '.' && ( ( unicode_t* )_primary.str() )[1] == '.' && ( ( unicode_t* )_primary.str() )[2] == 0;
		}

		return ( ( char* )_primary.str() )[0] == '.' && ( ( char* )_primary.str() )[1] == '.' && ( ( char* )_primary.str() )[2] == 0;
	}


	int Cmp( FSString& a );
	int CmpNoCase( FSString& a );

	static void dbg_printf_unicode(const char* label, const unicode_t* u)
	{
#ifdef _DEBUG
		FSString s(u);
		dbg_printf("%s:%s:\n", label, s.GetUtf8());
#endif
	}

	~FSString() {};
};


//root directory is an empty string, e.g. if there is empty string at the beginning, then this is absolute path, if not, then relative
//if empty string at the middle of the path then this is bad

//NOT thread-safe !!!

class FSPath
{
	ccollect<FSString, 16> data;
	int cacheCs;

	std::vector<char> cache;
	int cacheSize;
	int cacheSplitter;

	void SetCacheSize( int n )
	{
		if ( cacheSize >= n ) { return; }

		cache.resize( n );
		cacheSize = n;
	}

	void MakeCache( int cs, unicode_t splitter );
	void _Set( int cs, const void* v );
public:
	FSPath(): cacheCs( -2 ), cacheSize( 0 ), cacheSplitter( -1 ) {};

	void Copy( const FSPath& );
	void Copy(const FSPath&, int elementCount);

	FSPath( const FSPath& a ): cacheCs( -2 ), cacheSize( 0 ) { Copy( a ); }
	FSPath( int cs, const void* v ): cacheCs( -2 ), cacheSize( 0 ) { _Set( cs, v ); }
	FSPath( FSString& s ): cacheCs( -2 ), cacheSize( 0 ) { if ( !s.IsEmpty() ) { _Set( s._primary.cs(), s._primary.str() ); } }

	void Set( const unicode_t* s ) { _Set( CS_UNICODE, s ); }
	void Set( int cs, const void* v ) { _Set( cs, v ); }

	int Count() const { return data.count(); }

	FSPath& operator = ( const FSPath& a ) { Copy( a ); return *this; }

	FSString* GetItem( int n ) { return n >= 0 && n < data.count() ? &( data[n] ) : 0; }

	void SetItem( int n, int cs, const void* v ); // n>=0 && n<=Count(); if n == Count then one more element is added to the end
	void Push( int cs, const void* v ) { SetItem( Count(), cs, v ); }

	void SetItemStr( int n, const FSString& str ); // n>=0 && n<=Count(); if n == Count then one more element is added to the end
	void PushStr( const FSString& str ) { SetItemStr( Count(), str ); }

	bool Pop() { if ( data.count() <= 0 ) { return false; } data[data.count() - 1].Clear(); data.del( data.count() - 1 ); cacheCs = -2; return true; }
	void Clear() { data.clear(); cacheCs = -2; }

	bool IsAbsolute() { return Count() > 0 && data[0].IsEmpty(); }

	const void* GetString( int cs, char delimiter = DIR_SPLITTER ) { if ( cacheCs != cs || cacheSplitter != delimiter ) { MakeCache( cs, delimiter ); } return cache.data(); }
	const unicode_t* GetUnicode( char delimiter = DIR_SPLITTER ) { return ( const unicode_t* ) GetString( CS_UNICODE, delimiter ); }
	const char* GetUtf8( char delimiter = DIR_SPLITTER ) { return ( const char* ) GetString( CS_UTF8, delimiter ); }

	bool Equals(FSPath* that);

	void dbg_printf(const char* label)
	{
#ifdef _DEBUG
		::dbg_printf("%s:(%d): ", label, Count());
		for (int i = 0; i < Count(); i++)
		{
			const char* s = GetItem(i)->GetUtf8();
			::dbg_printf("%s:", s ? s : "null");
		}
		::dbg_printf("\n");
#endif
	}
	~FSPath();
};


/////////////////////////////////////// FSString

inline const void* FSString::Get( int cs ) const
{
	if ( !_primary.str() )
	{
		if ( cs == CS_UNICODE ) { return &unicode0; }

		return &char0;
	}

	if ( _primary.cs() == cs ) { return _primary.str(); }

	if ( !_temp.str() || _temp.cs() != cs )
	{
		_temp.copy( _primary, cs );
	}

	if ( !_temp.str() )
	{
		if ( cs == CS_UNICODE ) { return &unicode0; }

		return &char0;
	}

	return _temp.str();
};
