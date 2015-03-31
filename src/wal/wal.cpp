/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#include "wal.h"
#include "../unicode_lc.h"

#define __STDC_FORMAT_MACROS
#include <stdint.h>
#if !defined(_MSC_VER) || _MSC_VER >= 1700
#  include <inttypes.h>
#endif

#if !defined(_WIN32)
#	include "utf8proc/utf8proc.h"
#endif

#include <string>
#include <sstream>

namespace wal
{
	std::vector<unicode_t> new_unicode_str( const unicode_t* s )
	{
		if ( !s ) { return std::vector<unicode_t>(); }

		const unicode_t* p;

		for ( p = s; *p; ) { p++; }

		int len = p - s;
		std::vector<unicode_t> r( len + 1 );

		if ( len ) { memcpy( r.data(), s, len * sizeof( unicode_t ) ); }

		r[len] = 0;
		return r;
	}

	std::vector<sys_char_t> new_sys_str( const sys_char_t* s )
	{
		if ( !s ) { return std::vector<sys_char_t>(); }

		int len = sys_strlen( s );
		std::vector<sys_char_t> r( len + 1 );

		if ( len ) { memcpy( r.data(), s, len * sizeof( sys_char_t ) ); }

		r[len] = 0;
		return r;
	}

	std::vector<sys_char_t> utf8_to_sys( const char* s )
	{
		if ( !s ) { return std::vector<sys_char_t>(); }

		int symbolCount = utf8_symbol_count( s );
		std::vector<unicode_t> unicodeBuf( symbolCount + 1 );
		utf8_to_unicode( unicodeBuf.data(), s );

		int sys_len = sys_string_buffer_len( unicodeBuf.data(), symbolCount );
		std::vector<sys_char_t> sysBuf( sys_len + 1 );
		unicode_to_sys( sysBuf.data(), unicodeBuf.data(), symbolCount );
		return sysBuf;
	};


	std::string sys_to_utf8( const sys_char_t* s )
	{
		if ( !s ) { return std::string(); }

		int symbolCount = sys_symbol_count( s );
		std::vector<unicode_t> unicodeBuf( symbolCount + 1 );
		sys_to_unicode( unicodeBuf.data(), s );
		int utf8Len = utf8_string_buffer_len( unicodeBuf.data(), symbolCount );
		
		//TODO: memory leak ?!!!
		char* Buf = (char*)alloca( utf8Len + 1 );
		unicode_to_utf8( Buf, unicodeBuf.data(), symbolCount );
		return std::string( Buf );
	}


	std::vector<unicode_t> utf8_to_unicode( const char* s )
	{
		if ( !s ) { return std::vector<unicode_t>(); }

		int symbolCount = utf8_symbol_count( s );
		std::vector<unicode_t> unicodeBuf( symbolCount + 1 );
		utf8_to_unicode( unicodeBuf.data(), s );
		return unicodeBuf;
	}

	std::vector<unicode_t> utf8str_to_unicode( const std::string& s )
	{
		return utf8_to_unicode( s.c_str() );
	}

	std::string unicode_to_utf8( const unicode_t* u )
	{
		if ( !u ) { return std::string(); }

		int size = utf8_string_buffer_len( u );
		char* Buf = (char*)alloca( size + 1 );
		unicode_to_utf8( Buf, u );
		return std::string( Buf );
	}

	std::string unicode_to_utf8_string( const unicode_t* u )
	{
		if ( !u ) { return std::string(); }

		int size = utf8_string_buffer_len( u );
		char* s = (char*)alloca( size + 1 );
		unicode_to_utf8( s, u );
		return std::string( s );
	}

	bool unicode_is_equal( const unicode_t* s, const unicode_t* ss )
	{
		if ( !s || !ss ) { return false; }

		while ( *ss != 0 ) if ( *s++ != *ss++ )
			{
				return false;
			}

		if ( *ss == 0 && *s == 0 ) { return true; }

		return false;
	}

	// not null-safe like strcmp
	int unicode_strcmp(const unicode_t* s1, const unicode_t* s2)
	{
		for (; *s1 == *s2; s1++, s2++)
		{
			if (*s1 == 0)
				return 0;
		}
		return *s1 > *s2 ? 1 : -1;
	}

	// not null-safe, like stricmp
	int unicode_stricmp(const unicode_t* s1, const unicode_t* s2)
	{
		for (;; s1++, s2++)
		{
			unicode_t c1 = UnicodeLC(*s1);
			unicode_t c2 = UnicodeLC(*s2);
			if (c1 != c2)
				return c1 > c2 ? 1 : -1;
			if (c1 == 0)
				return 0;
		}
	}

	std::vector<unicode_t> unicode_get_last_word( const unicode_t* Str, const unicode_t** LastWordStart, bool UsePathSeparator )
	{
		if ( !Str ) return std::vector<unicode_t>();

		std::vector<unicode_t> Result;

		const unicode_t* S = Str;
		const unicode_t* StartPos = Str;

		while ( *S )
		{
			if ( UsePathSeparator )
			{
				if ( IsPathSeparator(*S) || *S == ' ' ) StartPos = S+1;
				if ( LastWordStart ) *LastWordStart = StartPos;
			}
			else
			{
				if ( *S == ' ' ) StartPos = S+1;
				if ( LastWordStart ) *LastWordStart = StartPos;
			}
			S++;
		}

		return ( *StartPos ) ? new_unicode_str( StartPos ) : std::vector<unicode_t>();
	}

	bool unicode_starts_with_and_not_equal( const unicode_t* Str, const unicode_t* SubStr, bool IgnoreCase )
	{
		if ( !Str || !SubStr )
		{
			return false;
		}

		while ( *SubStr != 0 )
		{
			unicode_t S = *Str++;
			unicode_t SS = *SubStr++;
			if ( IgnoreCase )
			{
				S = UnicodeLC( S );
				SS = UnicodeLC( SS );
			}
			
			if ( S != SS )
			{
				return false;
			}
		}

		return !( *Str == 0 && *SubStr == 0 );
	}

	bool utf8_starts_with_and_not_equal( const char* Str, const char* SubStr )
	{
		if (!Str || !SubStr) { return false; }

		const char* S = Str;
		const char* SS = SubStr;

		while (*SS != 0) if (*S++ != *SS++)
		{
			return false;
		}

		if (*SS == 0 && *S == 0) { return false; }

		return true;
	}

	int unicode_strlen( const unicode_t* s )
	{
		if ( !s ) { return 0; }

		const unicode_t* p = s;

		while ( *p ) { p++; }

		return p - s;
	}

	unicode_t* unicode_strchr( const unicode_t* s, unicode_t c )
	{
		while ( *s != c && *s ) { s++; }

		return ( unicode_t* )( *s ? s : 0 );
	}

	unicode_t* unicode_strrchr(const unicode_t* s, unicode_t c)
	{
		const unicode_t* ps = s + unicode_strlen(s);
		while (*ps != c && ps != s) { ps--; }

		return (unicode_t*)(*ps == c ? ps : 0);
	}

	unicode_t* unicode_strcpy(unicode_t* d, const unicode_t* s)
	{
		if ( !d || !s ) { return NULL; }

		unicode_t* ret = d;

		while ( ( *d++ = *s++ ) != 0 )
			;

		return ret;
	}

	// copy unlit end of string, or when n chars copid, whichever comes first.
	// d is always 0-ended
	unicode_t* unicode_strncpy0( unicode_t* d, const unicode_t* s, int n )
	{
		if ( !d || !s ) { return NULL; }

		unicode_t* ret = d;

		for ( ;; )
		{
			if ( n-- == 0 )
			{
				*d = 0;
				break;
			}

			if ( ( *d++ = *s++ ) == 0 )
			{
				break;
			}
		}

		return ret;
	}

	void unicode_strcat( unicode_t* d, const unicode_t* s )
	{
		if ( !d || !s ) { return; }

		while ( *d )
		{
			d++;
		}

		while ( ( *d++ = *s++ ) != 0 )
			;
	}

	unicode_t* unicode_strdup( const unicode_t* s )
	{
		return unicode_strcpy( new unicode_t[unicode_strlen( s ) + 1], s );
	}


	////////////  system File implementation

	void File::Throw( const sys_char_t* name )
	{
		if ( !name ) { name = _fileName.data(); }

		static const char noname[] = "<NULL>";
		throw_syserr( 0, "'%s'", name ? sys_to_utf8( name ).data() : noname );
	}

	void File::Throw() { Throw( 0 ); }

	bool LookAhead( const unicode_t* p, unicode_t* OutNextChar )
	{
		if ( !p ) { return false; }

		if ( !*p ) { return false; }

		if ( OutNextChar ) { *OutNextChar = *( p + 1 ); }

		return true;
	}

	void PopLastNull( std::vector<unicode_t>* S )
	{
		if ( S && !S->empty() && S->back() == 0 ) { S->pop_back(); }
	}

	bool LastCharEquals( const std::vector<unicode_t>& S, unicode_t Ch )
	{
		if ( S.empty() ) { return false; }

		return S.back() == Ch;
	}

	bool LastCharEquals( const unicode_t* S, unicode_t Ch )
	{
		if ( !S ) { return false; }

		unicode_t PrevCh = *S;

		while ( *S )
		{
			PrevCh = *S;
			S++;
		}

		return PrevCh == Ch;
	}

	bool IsPathSeparator( const unicode_t Ch )
	{
		return ( Ch == '\\' ) || ( Ch == '/' );
	}

	void ReplaceSpaces( std::vector<unicode_t>* S )
	{
		if ( !S ) { return; }

		for ( auto i = 0; i != S->size(); i++ )
		{
			unicode_t Ch = S->at( i );

			if ( Ch == 32 || Ch == 9 )
			{
				S->at( i ) = 0x00B7;
			}
		}
	}

	void ReplaceTrailingSpaces( std::vector<unicode_t>* S )
	{
		if ( !S ) { return; }

		for ( auto i = S->size(); i -- > 0; )
		{
			unicode_t Ch = S->at( i );

			if ( !Ch ) { continue; }

			if ( Ch == 32 || Ch == 9 )
			{
				S->at( i ) = 0x00B7;
			}
			else
			{
				break;
			}
		}
	}

	bool IsEqual_Unicode_CStr( const unicode_t* U, const char* S, bool CaseSensitive )
	{
		if ( !U && !S ) { return true; }

		if ( !U ) { return false; }

		if ( !S ) { return false; }

		const unicode_t* UPtr = U;
		const char*      SPtr = S;

		while ( *UPtr || *SPtr )
		{
			unicode_t ChU = CaseSensitive ? *UPtr : UnicodeLC( *UPtr );
			char      ChS = CaseSensitive ? *SPtr : tolower( *SPtr );

			if ( ChU != ChS ) { return false; }

			UPtr++;
			SPtr++;
		}

		return true;
	}

	static char g_HexChars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

	char GetHexChar( int n )
	{
		return g_HexChars[ n & 0xF ];
	}

	std::wstring IntToHexStr( uint64_t Value, size_t Padding )
	{
		const int BUFFER = 1024;

		char buf[BUFFER];

#if defined( _WIN32 )
#  if _MSC_VER >= 1400
		_ui64toa_s( Value, buf, BUFFER - 1, 16 );
#  else
		_ui64toa( Value, buf, 16 );
#  endif
#else
		Lsnprintf( buf, BUFFER - 1, "%" PRIx64, Value );
#endif

		std::vector<unicode_t> Str = utf8_to_unicode( buf );

		std::wstring Result( Str.data() );

		if ( Padding && Padding > Result.length() )
		{
			Result.insert( Result.begin( ), Padding - Result.length( ), '0' );
		}

		return Result;
	}

	uint64_t HexStrToInt( const unicode_t* Str )
	{
		if ( !Str ) return 0;

		std::string utf8 = unicode_to_utf8( Str );

		uint64_t i = 0x0;

		std::stringstream Convert( utf8 );

		Convert >> std::hex >> i;

		return i;
	}

	std::vector<unicode_t> normalize_unicode_NFC( const unicode_t* Str )
	{
		if ( !Str ) return std::vector<unicode_t>();
#if !defined(_WIN32)
		std::string UTFstr = unicode_to_utf8( Str );
		uint8_t* NormString = utf8proc_NFC( (const uint8_t*)UTFstr.data() );
		std::vector<unicode_t> Result = utf8_to_unicode( (const char*)NormString );

		free( NormString );

		return Result;
#else
		return new_unicode_str( Str );
#endif
	}

	std::string normalize_utf8_NFC( const char* Str )
	{
		if ( !Str ) return std::string();
#if !defined(_WIN32)
		uint8_t* NormString = utf8proc_NFC( (const uint8_t*)Str );

		if(!NormString) return std::string(Str);
		
		std::string Result = (const char*)NormString;

		free( NormString );

		return Result;
#else
		return std::string( Str );
#endif
	}

std::string GetEnvVariable( const char* VarName )
{
	if ( !VarName ) return std::string();

#if _MSC_VER > 1700
	char* value;
	size_t size;
	_dupenv_s( &value, &size, VarName );
#else
	const char* value = getenv( VarName );
#endif

	if ( !value ) { return std::string(); }

	std::string Result( value );

#if _MSC_VER > 1700
	// deallocate after _dupenv_s()
	free( value );
#endif

	return Result;
}

}; //namespace wal
