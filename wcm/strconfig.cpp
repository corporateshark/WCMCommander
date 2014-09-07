#include "strconfig.h"

using namespace wal;


StrConfig::StrConfig() {}

void StrConfig::Clear() { varHash.clear(); }

inline bool IsSpace( int c ) { return c > 0 && c <= 32; }
inline bool IsAlpha( int c ) { return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'; }
inline bool IsDigit( int c ) { return c >= '0' && c <= '9'; }
inline const char* SS( const char* s ) { while ( IsSpace( *s ) ) { s++; } return s; }
inline const char* SNotS( const char* s ) { while ( *s && !IsSpace( *s ) ) { s++; } return s; }
inline const char* SNotC( const char* s, int c ) { while ( *s && *s != c ) { s++; } return s; }

inline int Upper( int c )
{
	if ( c >= 'a' && c <= 'z' ) { return c - 'a' + 'A'; }

	return c;
}

inline void UpStr( char* s )
{
	for ( ; *s; s++ ) { *s = Upper( *s ); }
}


//синтаксис var = [val]; ..
//var - [_0-9a-zA-Z]+
//val - "..." или '..' или слово без пробелов и ; ,  \ -экран

bool StrConfig::Load( const char* s )
{
	varHash.clear();

	while ( *s )
	{
		s = SS( s );

		if ( !*s ) { break; }

		if ( *s == ';' )
		{
			s++;
			continue;
		}

		ccollect<char, 0x100> var;

		while ( *s == '_' || IsAlpha( *s ) || IsDigit( *s ) )
		{
			var.append( Upper( *s ) );
			s++;
		}

		if ( !var.count() ) { return false; }

		var.append( 0 );
		s = SS( s );

		if ( *s != '=' ) { return false; }

		s++;
		s = SS( s );

		int c1 = 0;

		if ( *s == '\'' || *s == '"' ) { c1 = *s; s++; }

		ccollect<char, 0x100> value;

		while ( *s )
		{
			if ( c1 && *s == c1 ) { s++; break; }

			if ( *s == '\\' )
			{
				s++;

				if ( !*s ) { break; }

				value.append( *s );
				s++;
				continue;
			}

			if ( !c1 && ( IsSpace( *s ) || *s == ';' ) ) { break; }

			value.append( *s );
			s++;
		}

		//if (c1 && *s !=c1) return false;
		value.append( 0 );
		varHash[var.ptr()] = value.grab();
	}

	return true;
}

std::vector<char> StrConfig::GetConfig()
{
	ccollect<char, 0x100> res;
	std::vector<const char*> k = varHash.keys();
	int count = varHash.count();

	for ( int i = 0; i < count; i++ )
	{
		const char* s = k[i];

		for ( ; *s; s++ ) { res.append( *s ); }

		res.append( '=' );
		s = varHash[k[i]].data();

		if ( s )
		{
			bool simple = true;

			for ( const char* t = s; *t; t++ )
				if ( *t == ';' || *t == '\\' || *t == '\'' || *t == '"' || IsSpace( *t ) )
				{
					simple = false;
					break;
				};

			if ( simple )
			{
				for ( ; *s; s++ ) { res.append( *s ); }
			}
			else
			{
				res.append( '"' );

				for ( ; *s; s++ )
				{
					if ( *s == '\'' || *s == '"' || *s == '\\' )
					{
						res.append( '\\' );
					}

					res.append( *s );
				}

				res.append( '"' );
			}
		}

		res.append( ';' );
	}

	res.append( 0 );
	return res.grab();
}

void StrConfig::Set( const char* name, const char* value )
{
	std::vector<char> s = new_char_str( name );
	UpStr( s.data() );
	varHash[s.data()] = new_char_str( value );
}

void StrConfig::Set( const char* name, unsigned value )
{
	char buf[64];
	snprintf( buf, sizeof( buf ) - 1, "%i", value );
	Set( name, buf );
}


const char* StrConfig::GetStrVal( const char* name )
{
	std::vector<char> s = new_char_str( name );
	UpStr( s.data() );
	std::vector<char>* p = varHash.exist( s.data() );

	if ( p && p[0].data() )
	{
		return p[0].data();
	}

	return 0;
}

int StrConfig::GetIntVal( const char* name )
{
	std::vector<char> s = new_char_str( name );
	UpStr( s.data() );
	std::vector<char>* p = varHash.exist( s.data() );

	if ( p && p[0].data() )
	{
		return atoi( p[0].data() );
	}

	return -1;
}

StrConfig::~StrConfig() {}
