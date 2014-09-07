#include "search-tools.h"
#include "unicode_lc.h"

inline   int SBigNode::Eq( const char* str ) const
{
	const char* s = str;
	const char* t = a;

	while ( *t && *s == *t ) { t++; s++; }

	if ( !*t ) { return t - a; }

	if ( !b[0] ) { return 0; }

	t = b;
	s = str;

	while ( *t && *s == *t ) { t++; s++; }

	if ( !*t ) { return t - b; }

	return 0;
}

inline bool SBigNode::Eq( const SBigNode& x ) const
{
	const char* s = a;
	const char* t = x.a;
	int i = 0;

	while ( *s && *s == *t && i < 16 ) { i++; s++; t++; }

	if ( *s != *t ) { return false; }

	s = b;
	t = x.b;

	i = 0;

	while ( *s && *s == *t && i < 16 ) { i++; s++; t++; }

	if ( *s != *t ) { return false; }

	return true;
}

inline   bool SBigNode::Set( unicode_t c, bool sens, charset_struct* charset )
{
	unicode_t u1 = c;
	unicode_t u2 = c;

	if ( !sens )
	{
		u2 = UnicodeLC( c );

		if ( u1 == u2 ) { u2 = UnicodeUC( c ); }
	}

	int bad = 0;
	char* s = charset->unicode_to_cs( a, &u1, 1, &bad );

	if ( bad )
	{
		a[0] = b[0] = 0;
		return false;
	}

	*s = 0;

	if ( u1 == u2 )
	{
		b[0] = 0;
	}
	else
	{
		bad = 0;
		s = charset->unicode_to_cs( b, &u2, 1, &bad );

		if ( bad )
		{
			b[0] = 0;
		}
		else { *s = 0; }
	}

	return true;
}

inline   int SBigNode::MaxLen()
{
	int n1 = strlen( a );

	if ( !b[0] ) { return n1; }

	int n2 = strlen( b );
	return n1 > n2 ? n1 : n2;
}
inline   int SBigNode::MinLen()
{
	int n1 = strlen( a );

	if ( !b[0] ) { return n1; }

	int n2 = strlen( b );
	return n1 < n2 ? n1 : n2;
}


static char* VSearchStr( char* s, char* end, SNode* list ) //считается, что после end есть строка длины(list)-1
{
	for ( ; s < end; s++ )
		if ( list[0].Eq( *s ) )
		{
			char* t = s + 1;
			SNode* p = list + 1;

			while ( true )
			{
				if ( !p->a ) { return s; }

				if ( !p->Eq( *t ) ) { break; }

				t++;
				p++;
			}
		}

	return 0;
}

static char* VSearchStr( char* s, char* end, SBigNode* list, int* fBytes )
{
	for ( ; s < end; s++ )
	{
		int n = list[0].Eq( s );

		if ( n > 0 )
		{
			char* t = s + n;
			SBigNode* p = list + 1;

			while ( true )
			{
				if ( !p->a[0] )
				{
					if ( fBytes )
					{
						*fBytes = t - s;
					}

					return s;
				}

				n = p->Eq( t );

				if ( !n ) { break; }

				t += n;
				p++;
			}
		}
	}

	return 0;
}


static char* VSearchStr( char* s, char* end, char* cs, int csLen )
{
	for ( ; s < end; s++ )
		if ( *s == *cs )
		{
			char* t = s + 1;
			char* p = cs + 1;
			int n = csLen - 1;

			while ( n > 0 && *t == *p ) { t++; p++; n--; }

			if ( n <= 0 ) { return s; }
		}

	return 0;
}


void VSearcher::Set( unicode_t* uStr, bool sens, charset_struct* charset ) //throw
{
	sBigList.clear();
	sList.clear();
	bytes.clear();
	_cs = charset;
	mode = 0;

	int maxSymLen = 0;
	bool one = true;
	int i;

	for ( ; *uStr; uStr++ )
	{
		SBigNode node;

		if ( !node.Set( *uStr, sens, charset ) )
		{
			unicode_t x[2] = { *uStr, 0};
			throw_msg( "symbol '%s' not exist in charset '%s'", unicode_to_utf8( x ).data(), charset->name );
		}

		int n = node.MaxLen();

		if ( maxSymLen < n ) { maxSymLen = n; }

		if ( node.b[0] ) { one = false; }

		sBigList.append( node );
	}

	sBigList.append( SBigNode() );

	mode = 1;

	if ( one )
	{
		for ( i = 0; i < sBigList.count(); i++ )
		{
			for ( char* s = sBigList[i].a; *s; s++ )
			{
				bytes.append( *s );
			}
		}

		mode = 3;
	}
	else if ( maxSymLen == 1 )
	{
		for ( i = 0; i < sBigList.count(); i++ )
		{
			sList.append( SNode( sBigList[i].a[0], sBigList[i].b[0] ) );
		}

		mode = 2;
	}
}

char* VSearcher::Search( char* s, char* end, int* fBytes )
{
	char* ret = 0;

	switch ( mode )
	{
		case 1:
			ret = VSearchStr( s, end, sBigList.ptr(), fBytes );
			break;

		case 2:
			ret = VSearchStr( s, end, sList.ptr() );

			if ( ret && fBytes ) { *fBytes = sList.count() - 1; }

			break;

		case 3:
			ret = VSearchStr( s, end, bytes.ptr(), bytes.count() );

			if ( ret && fBytes ) { *fBytes = bytes.count(); }

			break;
	};

	return ret;
}

bool VSearcher::Eq( const VSearcher& a ) const
{
	if ( mode != a.mode ) { return false; }

	switch ( mode )
	{
		case 1:
		{
			if ( sBigList.count() != a.sBigList.count() ) { return false; }

			for ( int i = 0; i < sBigList.count(); i++ )
				if ( !sBigList.const_item( i ).Eq( a.sBigList.const_item( i ) ) ) { return false; }
		}

		return true;

		case 2:
		{
			if ( sList.count() != a.sList.count() ) { return false; }

			for ( int i = 0; i < sList.count(); i++ )
				if ( !sList.const_item( i ).Eq( a.sList.const_item( i ) ) ) { return false; }
		}

		return true;

		case 3:
		{
			if ( bytes.count() != a.bytes.count() ) { return false; }

			for ( int i = 0; i < bytes.count(); i++ )
				if ( bytes.const_item( i ) != a.bytes.const_item( i ) ) { return false; }
		}

		return true;
	};

	return false;
}

int VSearcher::MinLen()
{
	switch ( mode )
	{
		case 1:
		{
			int ret = 0;

			for ( int i = 0; i < sBigList.count(); i++ ) { ret += sBigList[i].MinLen(); }

			return ret;
		}

		case 2:
			return sList.count() - 1;

		case 3:
			return bytes.count();
	};

	return 0;
}

int VSearcher::MaxLen()
{
	switch ( mode )
	{
		case 1:
		{
			int ret = 0;

			for ( int i = 0; i < sBigList.count(); i++ ) { ret += sBigList[i].MaxLen(); }

			return ret;
		}

		case 2:
			return sList.count() - 1;

		case 3:
			return bytes.count();
	};

	return 0;
}


///////////////////// MegaSearcher

/*
class MegaSearcher {
   ccollect< clPtr< VSearcher > > list;
public:
   bool Set(unicode_t *uStr, bool sens, charset_struct *charset = 0);
   char *Search(char *s, char *end, int *fBytes, charset_struct **retcs);
};
*/

bool MegaSearcher::Set( unicode_t* uStr, bool sens, charset_struct* charset )
{
	list.clear();

	if ( charset )
	{
		clPtr<VSearcher> p = new VSearcher();

		try
		{
			p->Set( uStr, sens, charset );
		}
		catch ( cexception* ex )
		{
			ex->destroy();
			return false;
		}

		list.append( p );
		return true;
	}

	charset_struct* cslist[128];
	int csCount = charset_table.GetList( cslist, 128 );

	for ( int i = 0; i < csCount; i++ )
	{
		clPtr<VSearcher> p = new VSearcher();

		try
		{
			charset  = cslist[i];

			if ( !charset ) { continue; }

			p->Set( uStr, sens, charset );
		}
		catch ( cexception* ex )
		{
			ex->destroy();
			continue;
		}

		bool exist = false;

		for ( int j = 0; j < list.count(); j++ )
			if ( list[j]->Eq( p.ptr()[0] ) )
			{
				exist = true;
				break;
			}

		if ( !exist )
		{
			list.append( p );
		}
	}

	return list.count() > 0;
}


char* MegaSearcher::Search( char* s, char* end, int* fBytes, charset_struct** retcs )
{
	for ( int i = 0; i < list.count(); i++ )
	{
		char* r = list[i]->Search( s, end, fBytes );

		if ( r )
		{
			if ( retcs ) { *retcs = list[i]->Charset(); }

			return r;
		}
	}

	return 0;
}

int MegaSearcher::MinLen()
{
	int ret = 0;

	for ( int i = 0; i < list.count(); i++ )
	{
		int n = list[i]->MinLen();

		if ( !ret || ret > n ) { ret = n; }
	}

	return ret;
}

int MegaSearcher::MaxLen()
{
	int ret = 0;

	for ( int i = 0; i < list.count(); i++ )
	{
		int n = list[i]->MinLen();

		if ( ret < n ) { ret = n; }
	}

	return ret;
}

