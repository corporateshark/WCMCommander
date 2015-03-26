/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#if defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include "vfspath.h"
#include "unicode_lc.h"

/////////////////////////////////////////////// FSPath///////////////////////////////


void FSPath::SetItem( int n, int cs, const void* v ) // n>=0 && n<=Count(); if n == Count then add one more element to the end
{
	if ( n < 0 || n > Count() ) { return; }

	cacheCs = -2;

	if ( n == Count() ) { data.append( FSString( cs, v ) ); }
	else { data[n] = FSString( cs, v ); }
}

void FSPath::SetItemStr( int n, const FSString& str ) // n>=0 && n<=Count(); if n == Count then add one more element to the end
{
	if ( n < 0 || n > Count() ) { return; }

	cacheCs = -2;

	//avoid effect of cptr
	FSString s;
	s.Copy( str );

	if ( n == Count() ) { data.append( s ); }
	else { data[n] = s; }
}


void FSPath::Copy( const FSPath& a )
{
	cacheCs = -2;
	data.clear();

	for ( int i = 0 ; i < a.Count(); i++ )
	{
		FSString s;
		s.Copy( a.data.const_item( i ) );
		data.append( s );
	}
}

void FSPath::Copy(const FSPath& a, int elementCount)
{
	cacheCs = -2;
	data.clear();
	if (elementCount<0 || elementCount>a.Count())
		elementCount = a.Count();

	for (int i = 0; i < elementCount; i++)
	{
		FSString s;
		s.Copy(a.data.const_item(i));
		data.append(s);
	}

}

void FSPath::_Set( int cs, const void* v )
{
	data.clear();
	cacheCs = -2;

	if ( cs == CS_UNICODE )
	{
		unicode_t* p = ( unicode_t* )v;

		if ( !*p ) { return; }

		while ( *p )
		{
			unicode_t* t = p;

			while ( *t != '\\' && *t != '/' && *t ) { t++; }

			int len = t - p;
			data.append( FSString( cs, p, len ) );
			p = ( *t ) ? t + 1 : t;
		}
	}
	else
	{
		char* p = ( char* )v;

		if ( !p || !*p ) { return; }

		while ( *p )
		{
			char* t = p;

			while ( *t != '\\' && *t != '/' && *t ) { t++; }

			int len = t - p;

			data.append( FSString( cs, p, len ) );
			p = ( *t ) ? t + 1 : t;
		}
	}
}

void FSPath::MakeCache( int cs, unicode_t splitter )
{
	ASSERT( data.count() >= 0 );

	if ( Count() == 1 && data[0].IsEmpty() ) // then simple "/"
	{
		if ( cs == CS_UNICODE )
		{
			SetCacheSize( 2 * sizeof( unicode_t ) );
			( ( unicode_t* )cache.data() )[0] = splitter;
			( ( unicode_t* )cache.data() )[1] = 0;
		}
		else
		{
			SetCacheSize( 2 );
			cache[0] = char( splitter & 0xFF );
			cache[1] = 0;
		}

		cacheCs = cs;
		cacheSplitter = splitter;
		return;
	}

	int i, l;

	if ( cs == CS_UNICODE )
	{
		for ( i = l = 0; i < data.count(); i++ )
		{
			l += unicode_strlen( ( unicode_t* )data[i].Get( cs ) );
		}

		l += data.count() - 1; //delimiters

		if ( l < 0 ) { l = 0; }

		SetCacheSize( ( l + 1 )*sizeof( unicode_t ) );

		unicode_t* p = ( unicode_t* ) cache.data();

		for ( i = 0; i < data.count(); i++ )
		{
			const void* v = data[i].Get( cs );
			l = unicode_strlen( ( unicode_t* )v );

			if ( l ) { memcpy( p, v, l * sizeof( unicode_t ) ); }

			p += l;

			if ( i + 1 < data.count() ) { *( p++ ) = splitter; }      /////////////// !!!
		}

		*p++ = 0;
	}
	else
	{
		for ( i = l = 0; i < data.count(); i++ )
		{
			l += strlen( ( char* )data[i].Get( cs ) );
		}

		l += data.count() - 1; //delimiters

		if ( l < 0 ) { l = 0; }

		SetCacheSize( ( l + 1 )*sizeof( char ) );

		char* p = cache.data();

		for ( i = 0; i < data.count(); i++ )
		{
			const void* v = data[i].Get( cs );
			l = strlen( ( char* )v );

			if ( l ) { strcpy( p, ( char* )v ); }

			p += l;

			if ( i + 1 < data.count() ) { *( p++ ) = char( splitter & 0xFF ); }        /////////////// !!!
		}

		*p++ = 0;
	}

	cacheCs = cs;
	cacheSplitter = splitter;
}

bool FSPath::Equals(FSPath* that) 
{
	int size = Count();
	if (!that || size != that->Count())
		return false;
	for (int i = 0; i < size; i++)
	{
		if (this->GetItem(i)->Cmp(*that->GetItem(i)) != 0)
			return false;
	}
	return true;
}

FSPath::~FSPath() {}


////////////////////////////////// cs_string /////////////////////////////////////////
inline clPtr<cs_string::Node> new_node( int size, int cs )
{
	clPtr<cs_string::Node> p = new cs_string::Node();

	p->m_Encoding = cs;
	p->m_ByteBuffer.resize( size );

	return p;
}

inline clPtr<cs_string::Node> new_node( int cs, const std::vector<char>& ByteBuffer )
{
	clPtr<cs_string::Node> p = new cs_string::Node();

	p->m_Encoding = cs;
	p->m_ByteBuffer = ByteBuffer;

	return p;
}

inline clPtr<cs_string::Node> new_node( int cs, const char* s )
{
	if ( !s || s[0] == 0 ) { return nullptr; }

	int l = strlen( s );
	int size = l + 1;
	clPtr<cs_string::Node> p = new_node( size, cs );
	memcpy( p->m_ByteBuffer.data(), s, size );
	return p;
}

inline clPtr<cs_string::Node> new_node( int cs, const char* s, int len )
{
	if ( len <= 0 ) { return nullptr; }

	int size = len + 1;
	clPtr<cs_string::Node> p  = new_node( size, cs );
	memcpy( p->m_ByteBuffer.data(), s, len );
	p->m_ByteBuffer.data()[len] = 0;
	return p;
}

clPtr<cs_string::Node> new_node( const unicode_t* s )
{
	if ( !s || s[0] == 0 ) { return 0; }

	const unicode_t* t = s;

	while ( *t ) { t++; }

	int len = t - s;

	int size = ( len + 1 ) * sizeof( unicode_t );
	clPtr<cs_string::Node> p = new_node( size, CS_UNICODE );
	memcpy( p->m_ByteBuffer.data(), s, size );
	return p;
}

inline clPtr<cs_string::Node> new_node( const unicode_t* s, int len )
{
	if ( len <= 0 ) { return 0; }

	int size = ( len + 1 ) * sizeof( unicode_t );
	clPtr<cs_string::Node> p = new_node( size, CS_UNICODE );
	memcpy( p->m_ByteBuffer.data(), s, len * sizeof( unicode_t ) );
	( ( unicode_t* )p->m_ByteBuffer.data() )[len] = 0;
	return p;
}

cs_string::cs_string( const unicode_t* s )
	: m_Data( new_node( s ) )
{
}

cs_string::cs_string( const char* utf8Str )
	: m_Data( new_node( CS_UTF8, utf8Str ) )
{
}

void cs_string::copy( const cs_string& a )
{
	clear();

	if ( a.m_Data )
	{
		m_Data = new_node( a.m_Data->m_Encoding, a.m_Data->m_ByteBuffer );
	}
}

void cs_string::set( const unicode_t* s )
{
	m_Data = new_node( s );
}

void cs_string::set( int cs, const void* s )
{
	m_Data = ( cs == CS_UNICODE ) ?
	         new_node( ( const unicode_t* )s ) :
	         new_node( cs, ( const char* )s );
}

void cs_string::set( int cs, const void* s, int len )
{
	m_Data = ( cs == CS_UNICODE ) ?
	         new_node( ( const unicode_t* )s, len ) :
	         new_node( cs, ( const char* )s, len );
}


void cs_string::copy( const cs_string& a, int cs_id )
{
	if ( !a.m_Data ) { clear(); return; }

	int acs = a.cs();

	if ( acs < CS_UNICODE || cs_id < CS_UNICODE )
	{
		fprintf( stderr, "BUG 1 cs_string::copy acs=%i, cs_id=%i\n", acs, cs_id );
		return;
	}

	if ( cs_id == acs )
	{
		copy( a );
	}
	else
	{
		unicode_t buf[0x100], *ptr = 0;
		unicode_t* u;
		ASSERT( acs >= -1 /*&& acs < CS_end*/ );
		ASSERT( cs_id >= -1 /*&& cs_id < CS_end*/ );

		try
		{
			clear();

			if ( acs == CS_UNICODE )
			{
				u = ( unicode_t* )( a.m_Data->m_ByteBuffer.data() );
			}
			else
			{
				charset_struct* old_charset = charset_table[acs];
				int sym_count = old_charset->symbol_count( a.m_Data->m_ByteBuffer.data(), -1 );

				if ( sym_count >= 0x100 )
				{
					ptr = new unicode_t[sym_count + 1];
					u = ptr;
				}
				else
				{
					u = buf;
				}

				( void )old_charset->cs_to_unicode( u, a.m_Data->m_ByteBuffer.data(), -1, 0 );
				u[sym_count] = 0;
			}

			if ( cs_id == CS_UNICODE )
			{
				m_Data = new_node( u );
			}
			else
			{
				charset_struct* new_charset = charset_table[cs_id];

				int len = new_charset->string_buffer_len( u, -1 );
				m_Data = new_node( len + 1, cs_id );
				new_charset->unicode_to_cs( m_Data->m_ByteBuffer.data(), u, -1, 0 );
			}

			if ( ptr )
			{
				delete [] ptr;
				ptr = 0;
			}

		}
		catch ( ... )
		{
			if ( ptr ) { delete [] ptr; }

			throw;
		}
	}
}


//////////////////////////////////////// FSString ///////////////////////////////////

unicode_t FSString::unicode0 = 0;
char FSString::char0 = 0;


int FSString::Cmp( FSString& a )
{
	if ( !_primary.str() ) { return ( !a._primary.str() ) ? 0 : -1; }

	if ( !a._primary.str() ) { return 1; }

	return CmpStr<const unicode_t>( GetUnicode(), a.GetUnicode() );
}

int FSString::CmpNoCase( FSString& par )
{
	if ( !_primary.str() ) { return ( !par._primary.str() ) ? 0 : -1; }

	if ( !par._primary.str() ) { return 1; }

	const unicode_t* a = GetUnicode();
	const unicode_t* b = par.GetUnicode();
	unicode_t au = 0;
	unicode_t bu = 0;

	for ( ; *a; a++, b++ )
	{
		au = UnicodeLC( *a );
		bu = UnicodeLC( *b );

		if ( au != bu ) { break; }
	};

	return ( *a ? ( *b ? ( au < bu ? -1 : ( au == bu ? 0 : 1 ) ) : 1 ) : ( *b ? -1 : 0 ) );
}

void FSString::SetSys( const sys_char_t* p )
{
	_temp.clear();
#ifdef _WIN32
	int l = wcslen( p );
	std::vector<unicode_t> a( l + 1 );

	for ( int i = 0; i < l; i++ ) { a[i] = p[i]; }

	a[l] = 0;
	_primary.set( a.data() );
#else
	_primary.set( sys_charset_id, p );
#endif
}




