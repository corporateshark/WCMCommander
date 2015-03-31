/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "strconfig.h"
#include "swl.h"
#include <algorithm>

using namespace wal;


StrConfig::StrConfig() {}

void StrConfig::Clear() { varHash.clear(); }

inline const char* SS( const char* s ) { while ( IsSpace( *s ) ) { s++; } return s; }
inline const char* SNotS( const char* s ) { while ( *s && !IsSpace( *s ) ) { s++; } return s; }
inline const char* SNotC( const char* s, int c ) { while ( *s && *s != c ) { s++; } return s; }

inline int Upper( int c )
{
	if ( c >= 'a' && c <= 'z' ) { return c - 'a' + 'A'; }

	return c;
}

inline void UpStr(std::string& s)
{
	std::transform( s.begin(), s.end(), s.begin(), ::toupper );
}

inline void UpStr( char* s )
{
	for ( ; *s; s++ ) { *s = Upper( *s ); }
}


//syntax var = [val]; ..
//var - [_0-9a-zA-Z]+
//val - "..." or '..' or word without spaces and ; ,  \ - escape

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

		std::string value;

		while ( *s )
		{
			if ( c1 && *s == c1 ) { s++; break; }

			if ( *s == '\\' )
			{
				s++;

				if ( !*s ) { break; }

				value.push_back( *s );
				s++;
				continue;
			}

			if ( !c1 && ( IsSpace( *s ) || *s == ';' ) ) { break; }

			value.push_back( *s );
			s++;
		}

		//if (c1 && *s !=c1) return false;
		value.push_back( 0 );
		varHash[ var.ptr() ] = value;
	}

	return true;
}

std::vector<char> StrConfig::GetConfig()
{
	std::vector<char> res;
	std::vector<const char*> k = varHash.keys();
	int count = varHash.count();

	for ( int i = 0; i < count; i++ )
	{
		const char* s = k[i];

		for ( ; *s; s++ ) { res.push_back( *s ); }

		res.push_back( '=' );
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
				for ( ; *s; s++ ) { res.push_back( *s ); }
			}
			else
			{
				res.push_back( '"' );

				for ( ; *s; s++ )
				{
					if ( *s == '\'' || *s == '"' || *s == '\\' )
					{
						res.push_back( '\\' );
					}

					res.push_back( *s );
				}

				res.push_back( '"' );
			}
		}

		res.push_back( ';' );
	}

	res.push_back( 0 );
	return res;
}

void StrConfig::Set( const char* name, const char* value )
{
	std::string s( name );
	UpStr( s );
	varHash[s.data()] = std::string( value );
}

void StrConfig::Set( const char* name, int value )
{
	char buf[64];
	Lsnprintf( buf, sizeof( buf ) - 1, "%i", value );
	Set( name, buf );
}


const char* StrConfig::GetStrVal( const char* name )
{
	std::string s( name );
	UpStr( s );
	std::string* p = varHash.exist( s.data() );

	if ( p && p[0].data() )
	{
		return p[0].data();
	}

	return 0;
}

int StrConfig::GetIntVal( const char* name )
{
	std::string s( name );
	UpStr( s );
	std::string* p = varHash.exist( s.data() );

	if ( p && p[0].data() )
	{
		return atoi( p[0].data() );
	}

	return -1;
}

StrConfig::~StrConfig() {}
