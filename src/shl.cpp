/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "swl.h"
#include "shl.h"
#include "strmasks.h"

namespace SHL
{
	using namespace wal;

	inline int ToUpper( int c ) { return  ( c >= 'a' && c <= 'z' ) ? c - 'a' + 'A' : c; }
	inline int ToLower( int c ) { return  ( c >= 'A' && c <= 'Z' ) ? c - 'A' + 'a' : c; }


	const char* ShlStream::Name() { return ""; }
	int ShlStream::Next() { return EOFCHAR; }
	ShlStream::~ShlStream() {}

	const char* ShlStreamFile::Name() { return _name_utf8.data(); }
	int ShlStreamFile::Next() { return _f.GetC(); }
	ShlStreamFile::~ShlStreamFile() {}


/////////////////////////////////////  Words

	void Words::Add( const char* s, int color )
	{
		if ( _sens )
		{
			_hash[s] = color;
		}
		else
		{
			std::vector<char> p( strlen( s ) + 1 );
			char* t;

			for ( t = p.data(); *s; s++, t++ )
			{
				*t = ToUpper( *s );
			}

			*t = 0;
			_hash[p.data()] = color;
		}
	}

	bool Words::Exist( const char* s, const char* end, int* color )
	{
		if ( s >= end ) { return false; }

		int l = end - s;
		char buf[0x100];
		std::vector<char> buf2;
		char* p;

		if ( l >= sizeof( buf ) )
		{
			buf2.resize( l + 1 );
			p = buf2.data();
		}
		else
		{
			p = buf;
		}

		memcpy( p, s, l * sizeof( char ) );
		p[l] = 0;

		if ( !_sens )
			for ( char* t = p; *t; t++ ) { *t = ToUpper( *t ); }

		int* pc = _hash.exist( p );

		if ( pc )
		{
			*color = *pc;
			return true;
		}

		return false;
	}

	Words::~Words() {}

////////////////////////// RuleNode

	void RuleNode::Set( int c, bool caseSensitive, char cnt )
	{
		_type = TYPE_CHAR;
		_count = cnt;
		_ch[0] = c;

		if ( caseSensitive )
		{
			_ch[1] = c;
		}
		else
		{
			_ch[0] = ToLower( c );
			_ch[1] = ToUpper( c );
		}
	}

	void RuleNode::Set( Chars* ch, char cnt )
	{
		_type = TYPE_MASK;
		_count = cnt;
		_chars = ch;
	}

	inline const unsigned char* RuleNode::Ok( const unsigned char* s,  const unsigned char* end )
	{

		if ( _type == TYPE_MASK )
		{

			if ( s < end )
			{
				if ( !_chars->IsSet( *s ) )
				{
					return ( _count == '*' ) ? s  : 0;
				}

				s++;

				if ( _count == '1' ) { return s; }

				for ( ; s < end; s++ )
					if ( !_chars->IsSet( *s ) )
					{
						return s;
					}

				//s == end
				return ( _chars->IsSet( '\n' ) ) ? s + 1 : s;
			}

			if ( s > end )
			{
				return ( _count == '*' ) ? s : 0;
			}

			//s == end

			if ( _chars->IsSet( '\n' ) )
			{
				return s + 1;
			}

			return ( _count == '*' ) ? s : 0;
		}


		//type == TYPE_CHAR
		if ( s < end )
		{
			if ( *s != _ch[0] && *s != _ch[1] )
			{
				return ( _count == '*' ) ? s  : 0;
			}

			s++;

			if ( _count == '1' ) { return s; }

			for ( ; s < end; s++ )
				if ( *s != _ch[0] && *s != _ch[1] )
				{
					return s;
				}

			//s == end
			return ( _ch[0] == '\n' ) ?  s + 1 : s;
		}

		if ( s > end ) { return _count == '*' ? s : 0; }

		//s == end
		if ( _ch[0] == '\n' )
		{
			return s + 1;
		}

		return ( _count == '*' ) ? s : 0;
	}


/////////////////////////// Rule


	inline const unsigned char* Rule::Ok( const unsigned char* s,  const unsigned char* end, ColorId* pColor )
	{
		RuleNode* p = _list.ptr();

		const unsigned char* first = s;

		for ( int n = _list.count(); s && n > 0; n--, p++ )
		{
			s = p->Ok( s, end );
		}


		if ( !s ) { return 0; }

		if ( pColor )
		{
			int c;

			if (  ( _words && _words->Exist( ( const char* )first, ( const char* )s, &c ) ) ||
			      ( _ns_words && _ns_words->Exist( ( const char* )first, ( const char* )s, &c ) ) )
			{
				*pColor =  ( c >= 0 ) ? c : _color;
			}
			else
			{
				*pColor = _color;
			}
		}

		return s;
	}

	Rule::~Rule() {}

///////////////////// State

	inline State* State::Next( const unsigned char** pS,  const unsigned char* end,  ColorId* pColorId )
	{
		Rule** p = _rules.ptr();
		const unsigned char* s = *pS;

		for ( int n = _rules.count(); n > 0; n--, p++ )
		{

			ColorId col;
			const unsigned char* t = p[0]->Ok( s, end, &col );

			if ( t )
			{
				if ( pColorId )
				{
					*pColorId = col >= 0 ? col : _color;
				}

				*pS = t;

				return p[0]->_nextState ? p[0]->_nextState : this;
			}
		}

		if ( pColorId )
		{
			*pColorId = _color;
		}

		*pS = s + 1;
		return this;
	}

	State::~State() {}



///////////////////////// Shl

	Shl::Shl()
		:  _charsList( 0 ),
		   _wordsList( 0 ),
		   _ruleList( 0 ),
		   _startState( -1 )
	{
	}


	StateId Shl::ScanLine( const unsigned  char* s, const unsigned  char* end, StateId state )
	{
		if ( state < 0 || state >= _states.count() )
		{
			return 0;
		}

		State* p = _states[state].ptr();

		while ( s <= end )
		{
			p = p->Next( &s, end, 0 );

		}

		return p->_id;
	}

	StateId Shl::ScanLine( const unsigned char* s, char* colors, int count, StateId state )
	{

		if ( state < 0 || state >= _states.count() )
		{
			return 0;
		}

		const unsigned  char* end = s + count;

		State* p = _states[state].ptr();

		while ( s <= end )
		{
			ColorId color;
			const unsigned char* t = s;

			p = p->Next( &s, end, &color );
			const unsigned char* te = ( s > end ) ? end : s;

			for ( ; t < te; t++, colors++ )
			{
				*colors = color;
			};
		}

		return p->_id;
	}

	Shl::~Shl()
	{
		for ( Chars* c = _charsList; c; )
		{
			Chars* t = c;
			c = c->_next;
			delete t;
		}

		for ( Words* w = _wordsList; w; )
		{
			Words* t = w;
			w = w->_next;
			delete t;
		}

		for ( Rule* r = _ruleList; r; )
		{
			Rule* t = r;
			r = r->_next;
			delete t;
		}
	}


	struct ShlParzerBuf
	{
		std::vector<char> data;
		int size;
		int pos;
		ShlParzerBuf(): size( 0 ), pos( 0 ) {}
		void Clear() { pos = 0; }
		void Append( int c );
	};

	void ShlParzerBuf::Append( int c )
	{
		if ( pos >= size )
		{
			int n = ( pos + 1 ) * 2;
			std::vector<char> p( n );

			if ( pos > 0 )
			{
				memcpy( p.data(), data.data(), pos );
			}

			data = p;
			size = n;
		}

		data[pos++] = c;
	}

	enum TOKENS
	{
		TOK_ID = -500,   // /[_\a][_\a\d]+/
		TOK_STR, // "...\?..."
		TOK_CHARS, // [.$()..]
		TOK_RULE, // <...>
		TOK_EOF
	};


///////////////////////////////// ShlParzer
	class ShlParzer
	{
		ShlStream* _stream;
		int _cc;
		int _tok;
		int NextChar();
		void SS() { while ( _cc >= 0 && _cc <= ' ' ) { NextChar(); } }
		ShlParzerBuf _buf;
		int _cline;
	public:
		void Syntax( const char* s = "" );
		ShlParzer( ShlStream* stream );
		int Tok() const { return _tok; }
		const char* Str() { return _buf.data.data(); }
		int Next();
		void Skip( int tok );
		void SkipAll( int tok, int mincount = 1 );
		~ShlParzer();
	};

	void ShlParzer::Skip( int t )
	{
		if ( _tok != t )
		{
			char buf[64];
			Lsnprintf( buf, sizeof( buf ) - 1, "symbol not found '%c'", t );
			Syntax( buf );
		}

		Next();
	}

	void ShlParzer::SkipAll( int t, int mincount )
	{
		int n;

		for ( n = 0; _tok == t; n++ ) { Next(); }

		if ( n < mincount )
		{
			char buf[64];
			Lsnprintf( buf, sizeof( buf ), "symbol not found '%c'", t );
			Syntax( buf );
		}
	}

	void ShlParzer::Syntax( const char* s )
	{
		throw_msg( "Syntax error in '%s' line %i: %s", _stream->Name(), _cline + 1, s );
	}

	ShlParzer::ShlParzer( ShlStream* stream )
		:  _stream( stream ),
		   _cc( 0 ),
		   _tok( 0 ),
		   _cline( 0 )
	{
		NextChar();
		Next();
	}

	ShlParzer::~ShlParzer() {}

	int ShlParzer::NextChar()
	{
		if ( _cc == ShlStream::EOFCHAR )
		{
			return _cc;
		}

		_cc = _stream->Next();

		if ( _cc == '\n' ) { _cline++; }

		return _cc;
	}



	int ShlParzer::Next()
	{
begin:
		SS();

		if ( _cc == '_' || IsAlpha( _cc ) )
		{
			_buf.Clear();
			_buf.Append( _cc );
			NextChar();

			while ( IsAlpha( _cc ) || IsDigit( _cc ) || _cc == '_' )
			{
				_buf.Append( _cc );
				NextChar();
			};

			_buf.Append( 0 );

			_tok = TOK_ID;

			return _tok;
		}

		if ( _cc == '#' )
		{
			NextChar();

			while ( _cc != ShlStream::EOFCHAR && _cc != '\n' )
			{
				NextChar();
			}

			goto begin;
		}

		if ( _cc == '[' )
		{
			_buf.Clear();
			NextChar();

			while ( _cc != ']' )
			{
				if ( _cc == ShlStream::EOFCHAR )
				{
					Syntax( "']' expected " );
				}

				if ( _cc == '\\' )
				{
					NextChar();

					if ( _cc == ShlStream::EOFCHAR )
					{
						Syntax( "']' expected " );
					}

					_buf.Append( '\\' );
				}

				_buf.Append( _cc );
				NextChar();
			}

			_buf.Append( 0 );
			_tok = TOK_CHARS;
			NextChar();
			return _tok;

		}


		if ( _cc == '<' )
		{
			_buf.Clear();
			NextChar();

			while ( _cc != '>' )
			{
				if ( _cc == ShlStream::EOFCHAR )
				{
					Syntax( "'>' expected " );
				}

				if ( _cc == '\\' )
				{
					NextChar();

					if ( _cc == ShlStream::EOFCHAR )
					{
						Syntax( "'>' expected " );
					}

					_buf.Append( '\\' );
				}

				_buf.Append( _cc );
				NextChar();
			}

			_buf.Append( 0 );
			_tok = TOK_RULE;
			NextChar();
			return _tok;
		}


		if ( _cc == '"' || _cc == '\'' )
		{
			_buf.Clear();
			int bc = _cc;
			NextChar();

			while ( _cc != bc )
			{
				if ( _cc == ShlStream::EOFCHAR )
				{
					Syntax( "invalid string constant" );
				}

				if ( _cc == '\\' )
				{
					NextChar();

					if ( _cc == ShlStream::EOFCHAR )
					{
						Syntax( "invalid string constant" );
					}
				}

				_buf.Append( _cc );
				NextChar();
			}

			_buf.Append( 0 );
			_tok = TOK_STR;
			NextChar();
			return _tok;
		}

		if ( _cc == ShlStream::EOFCHAR )
		{
			_tok = TOK_EOF;
			return _tok;
		}

		_tok = _cc;
		NextChar();
		return _tok;
	}

/////////////////////// Chars

	static Chars* ParzeNamedChars( const char** pS, cstrhash<clPtr<Chars> >* mhash )
	{
		const char* s = *pS;

		if ( *s != '$' && s[1] != '(' )
		{
			return 0;
		}

		s += 2;
		ccollect<char> name;

		if ( !IsAlpha( *s ) ) { return 0; }

		name.append( *s++ );

		while ( IsAlpha( *s ) || IsDigit( *s ) )
		{
			name.append( *s++ );
		}

		name.append( 0 );

		if ( *s != ')' ) { return 0; };

		s++;

		clPtr<Chars>* p = mhash->exist( name.ptr() );

		if ( !p ) { return 0; }

		*pS = s;
		return p->ptr();
	}

	static int ParzeChar( const char** pS ) //return 0 if error
	{
		const char* s = *pS;

		if ( !*s ) { return 0; }

		int c = 0;

		if ( s[0] == '\\' && s[1] )
		{
			switch ( s[1] )
			{
				case 'n':
					c = '\n';
					break;

				case 't':
					c = '\t';
					break;

				default:
					c = s[1];
					break;
			};

			s += 2;
		}
		else
		{
			c = *s;
			s++;
		}

		if ( !c ) { return 0; }

		*pS = s;
		return c;
	}


	bool Chars::Parze( const char* s, cstrhash<clPtr<Chars> >* mhash )
	{
		Clear();

		bool negative = false;

		if ( *s == '^' )
		{
			negative = true;
			s++;
		}

		while ( true )
		{
			if ( !*s )
			{
				if ( negative )
				{
					for ( int i = 0; i < 0x100; i++ )
					{
						_data[i] = _data[i] ? 0 : 1;
					}
				}

				return true;
			}

			if ( *s == '$' )
			{
				Chars* p = ParzeNamedChars( &s, mhash );

				if ( !p ) { return false; }

				Add( *p );
				continue;
			}

			int c1 = ParzeChar( &s );

			if ( !c1 ) { return false; }

			c1 &= 0xFF;

			if ( *s == '-' )
			{
				s++;
				int c2 = ParzeChar( &s );

				if ( !c2 ) { return false; }

				c2 &= 0xFF;

				for ( int i = c1; i <= c2; i++ )
				{
					Add( i );
				}

			}
			else
			{
				Add( c1 );
			}
		}

		return false;
	}


/////////////////////// Shl

	Chars* Shl::AllocChars()
	{
		Chars* p = new Chars;
		p->_next = _charsList;
		_charsList = p;
		return p;
	}

	Words* Shl::AllocWords( bool sens )
	{
		Words* p = new Words( sens );
		p->_next = _wordsList;
		_wordsList = p;
		return p;
	}

	Rule*  Shl::AllocRule()
	{
		Rule* p = new Rule;
		p->_next = _ruleList;
		_ruleList = p;
		return p;
	}

	State* Shl::AllocState()
	{
		clPtr<State> p = new State( _states.count() );
		State* ret = p.ptr();
		_states.append( p );
		return ret;
	}

	State* Shl::GetState( const char* name, cstrhash<State*>& hash )
	{
		State** t = hash.exist( name );

		if ( !t )
		{
			State* p = AllocState();
			hash[name] = p;
			return p;
		}

		return *t;
	}


	void Shl::Parze( ShlStream* stream, std::unordered_map< std::string, int >& colors )
	{
		ShlParzer parzer( stream );

		cstrhash<State*> stateHash;
		cstrhash<clPtr<Chars> > charsHash;

		while ( true )
		{
			parzer.SkipAll( ';', 0 );

			if ( parzer.Tok() == TOK_EOF ) { break; }

			if ( parzer.Tok() != TOK_ID )
			{
				parzer.Syntax( "1" );
			}

			if ( !strcmp( parzer.Str(), "start" ) )
			{
				parzer.Next();

				if ( parzer.Tok() != TOK_ID )
				{
					parzer.Syntax( "state name expected" );
				}

				_startState = GetState( parzer.Str(), stateHash )->Id();
				parzer.Next();
				continue;
			};

			if ( !strcmp( parzer.Str(), "chars" ) )
			{
				parzer.Next();

				clPtr<Chars> ptr = new Chars;

				if ( parzer.Tok() != TOK_ID )
				{
					parzer.Syntax( "charlist name expected" );
				}

				Chars* p = ptr.ptr();
				charsHash[parzer.Str()] = ptr;

				parzer.Next();

				if ( parzer.Tok() != TOK_CHARS )
				{
					parzer.Syntax( "2" );
				}

				if ( !p->Parze( parzer.Str(), &charsHash ) )
				{
					parzer.Syntax( "3" );
				}

				parzer.Next();
				continue;
			};

			if ( !strcmp( parzer.Str(), "state" ) )
			{
				parzer.Next();

				ccollect<State*> list;

				while ( parzer.Tok() == TOK_ID )
				{
					list.append( GetState( parzer.Str(), stateHash ) );
					parzer.Next();
				}

				if ( parzer.Tok() != '{' )
				{
					continue;
				}

				parzer.Skip( '{' );

				while ( true )
				{
					parzer.SkipAll( ';', 0 );

					if ( parzer.Tok() != TOK_ID )
					{
						break;
					}

					if ( !strcmp( parzer.Str(), "color" ) )
					{
						parzer.Next();
						parzer.Skip( '=' );

						if ( parzer.Tok() != TOK_ID )
						{
							parzer.Syntax( "invalid color name" );
						}

						auto pColor = colors.find( parzer.Str() );

						if ( pColor == colors.end() )
						{
							parzer.Syntax( "invalid color name" );
						}

						for ( int i = 0; i < list.count(); i++ )
						{
							list[i]->SetColor( pColor->second );
						}

						parzer.Next();
					}
					else if ( !strcmp( parzer.Str(), "rule" ) )
					{
						parzer.Next();

						if ( parzer.Tok() != TOK_RULE )
						{
							parzer.Syntax( "invalid rule" );
						}

						Rule* rule = AllocRule();

						const char* d = parzer.Str();

						while ( *d )
						{
							if ( *d == '[' )
							{
								ccollect<char> str;
								d++;

								while ( true )
								{
									if ( !*d )
									{
										parzer.Syntax( "invalid rule" );
									}

									if ( *d == '\\' && d[1] )
									{
										str.append( *d );
										str.append( d[1] );
										d += 2;
										continue;
									}

									if ( *d == ']' ) { break; }

									str.append( *d );
									d++;
								};

								d++;

								str.append( 0 );

								Chars* chars = AllocChars();

								chars->Parze( str.ptr(), &charsHash );

								int count = '1';

								if ( *d == '*' || *d == '+' ) { count = *d; d++; }

								rule->Add( RuleNode( chars, count ) );
							}
							else

								if ( *d == '$' )
								{
									Chars* p = ParzeNamedChars( &d, &charsHash );

									if ( !p ) { parzer.Syntax( "5" ); }

									int count = '1';

									if ( *d == '*' || *d == '+' ) { count = *d; d++; }

									//charsHash destroyed after parsing, that's why we create another set
									Chars* chars = AllocChars();
									chars->Add( *p );

									rule->Add( RuleNode( chars, count ) );
								}
								else
								{

									int c = ParzeChar( &d );

									if ( !c ) { parzer.Syntax( "6" ); }

									int count = '1';

									if ( *d == '*' || *d == '+' ) { count = *d; d++; }

									rule->Add( RuleNode( c, true, count ) );                    //////// !!!!! (true) need to rework (finish)
								}
						}

						{
							bool allCount0 = true;

							for ( int i = 0; i < rule->_list.count(); i++ )
								if ( rule->_list[i]._count != '*' )
								{
									allCount0 = false;
									break;
								}

							if ( allCount0 ) { parzer.Syntax( "size of all items of rule '*'? at least one must be >'*' " ); }
						}

						parzer.Next();

						if ( parzer.Tok() == '{' )
						{
							parzer.Next();

							while ( true )
							{
								if ( parzer.Tok() != TOK_ID ) { break; }

								if ( !strcmp( parzer.Str(), "state" ) )
								{
									parzer.Next();
									parzer.Skip( '=' );

									if ( parzer.Tok() != TOK_ID ) { parzer.Syntax( "7" ); }

									rule->_nextState = GetState( parzer.Str(), stateHash );
									parzer.Next();
								}
								else if ( !strcmp( parzer.Str(), "color" ) )
								{
									parzer.Next();
									parzer.Skip( '=' );

									auto pc = colors.find( parzer.Str() );

									if ( pc == colors.end() )
									{
										parzer.Syntax( "color not found" );
									}

									rule->_color = pc->second;
									parzer.Next();
								}
								else if ( !strcmp( parzer.Str(), "words" ) || !strcmp( parzer.Str(), "ns_words" ) )
								{
									bool sens = !strcmp( parzer.Str(), "words" );

									int color = -1;
									parzer.Next();

									if ( parzer.Tok() == '(' )
									{
										parzer.Next();

										if ( parzer.Tok() != TOK_ID )
										{
											parzer.Syntax( "invalid color" );
										}

										auto pc = colors.find( parzer.Str() );

										if ( pc == colors.end() )
										{
											parzer.Syntax( "color not found" );
										}

										color = pc->second;
										parzer.Next();

										parzer.Skip( ')' );

									}

									parzer.Skip( '=' );
									parzer.Skip( '{' );

									Words* w = 0;

									if ( sens )
									{
										if ( !rule->_words ) { rule->_words = AllocWords( true ); }

										w = rule->_words;
									}
									else
									{
										if ( !rule->_ns_words ) { rule->_ns_words = AllocWords( false ); }

										w = rule->_ns_words;
									}

									while ( parzer.Tok() == TOK_STR )
									{
										w->Add( parzer.Str(), color );
										parzer.Next();

										if ( parzer.Tok() == ',' )
										{
											parzer.Next();
										}
										else { break; }
									}

									parzer.Skip( '}' );
								}
								else
								{
									parzer.Syntax( "8" );
								}


								parzer.SkipAll( ';' );
							}

							parzer.Skip( '}' );
							parzer.SkipAll( ';' );
						}

						for ( int i = 0; i < list.count(); i++ )
						{
							list[i]->AddRule( rule );
						}
					}
					else
					{
						break;
					}
				}

				parzer.Skip( '}' );
				continue;
			};

			parzer.Syntax( "9" );
		};

		if ( _startState < 0 )
		{
			parzer.Syntax( "start state not defined" );
		}
	}

///////////////////// StrList

	void StrList::Add( const char* s )
	{
		Node* p = new Node;
		p->str = utf8_to_unicode( s );
		p->next = first;
		first = p;
	}

	void StrList::Add( const unicode_t* s )
	{
		Node* p = new Node;
		p->str = new_unicode_str( s );
		p->next = first;
		first = p;
	}

	void StrList::Clear()
	{
		Node* p = first;
		first = 0;

		while ( p )
		{
			Node* t = p;
			p = p->next;
			delete t;
		};
	}
	/*
	   static void ParzeStrList( StrList& slist, ShlParzer& parzer )
	   {
	      while ( true )
	      {
	         if ( parzer.Tok() == TOK_STR )
	         {
	            slist.Add( parzer.Str() );
	            parzer.Next();
	            continue;
	         }

	         if ( parzer.Tok() != ',' ) { break; }

	         parzer.Next();
	      }
	   }
	*/
///////////////////////// ShlConf

	ShlConf::ShlConf()
	{
	}

	Shl* ShlConf::Get( const char* name, std::unordered_map< std::string, int >& colors )
	{
		clPtr<Node>* p = hash.exist( name );

		if ( !p || !p->ptr() )
		{
			return 0;
		}

		Node* node = p->ptr();

		if ( node->shl.ptr() ) { return node->shl.ptr(); }

		if ( !node->shlFileName.data() ) { return 0; }

		try
		{
			clPtr<Shl> shl = new Shl();
			ShlStreamFile stream( utf8_to_sys( node->shlFileName.data() ).data() );
			shl->Parze( &stream, colors );
			node->shl = shl;
			return node->shl.ptr();
		}
		catch ( cexception* ex )
		{
			fprintf( stderr, "%s\n", ex->message() );
			ex->destroy();
			return 0;
		}
	}

	Shl* ShlConf::Get( const unicode_t* path, const unicode_t* firstLine, std::unordered_map< std::string, int >& colors )
	{
		int count = ruleList.count();

		if ( count <= 0 ) { return 0; }

		int i;

		for ( i = 0; i < count; i++ )
		{
			Rule* r = ruleList[i].ptr();
			const char* id = 0;

			if ( r )
			{
				switch ( r->type )
				{
					case Rule::FIRST:
						if ( firstLine )
						{
							for ( StrList::Node* p = r->list.first; p; p = p->next )
							{
								if ( p->str.data() && accmask( firstLine, p->str.data() ) )
								{
									id = r->id.data();
									break;
								}
							}
						}

						break;

					case Rule::MASK:
					{
						for ( StrList::Node* p = r->list.first; p; p = p->next )
						{
							if ( p->str.data() && accmask( path, p->str.data() ) )
							{
								id = r->id.data();
								break;
							}
						}
					}
					break;

					default:
						break;
				};
			}

			if ( id ) { return Get( id, colors ); }
		}

		return 0;
	}

	void ShlConf::Parze( sys_char_t* filePath )
	{
		std::string utf8path = sys_to_utf8( filePath );

		try
		{
			std::string dirPath( "" );

			{

				const char* p = 0;
				const char* s;

				for ( s = utf8path.data(); *s; s++ )
					if ( *s == DIR_SPLITTER ) { p = s; }

				if ( p )
				{
					p++;
					int l = p - utf8path.data();
					dirPath.resize( l + 1 );

					char* dir = (char*)dirPath.data();

					for ( s = utf8path.data(); s < p; s++, dir++ )
					{
						*dir = *s;
					}

					*dir = 0;
				}
			}

			ShlStreamFile stream( filePath );
			ShlParzer parzer( &stream );

			while ( true )
			{
				parzer.SkipAll( ';', 0 );

				if ( parzer.Tok() == TOK_EOF ) { break; }

				if ( parzer.Tok() != TOK_ID ) { parzer.Syntax( "no id" ); }

				if ( !strcmp( parzer.Str(), "syntax" ) )
				{
					parzer.Next();

					if ( parzer.Tok() != TOK_STR ) { parzer.Syntax(); }

					std::string id = parzer.Str();

					clPtr<Node> node = new Node;
					parzer.Next();
					parzer.Skip( '{' );

					while ( true )
					{
						if ( parzer.Tok() != TOK_ID ) { break; }

						if ( !strcmp( parzer.Str(), "cfg" ) )
						{
							parzer.Next();
							parzer.Skip( '=' );

							if ( parzer.Tok() != TOK_STR )
							{
								parzer.Syntax( "no string" );
							}

							if (
#ifdef _WIN32
							   1
#else
							   parzer.Str()[0] != '/'
#endif
							)
							{
								int l1 = strlen( dirPath.data() );
								int l2 = strlen( parzer.Str() );
								std::vector<char> fp( l1 + l2 + 1 );

								if ( l1 )
								{
									memcpy( fp.data(), dirPath.data(), l1 );
								}

								if ( l2 )
								{
									memcpy( fp.data() + l1, parzer.Str(), l2 );
								}

								fp[l1 + l2] = 0;

								node->shlFileName = std::string( fp.data() );
							}
							else
							{
								node->shlFileName = parzer.Str();
							}

							parzer.Next();
						}
						else
						{
							parzer.Syntax();
						}

						parzer.SkipAll( ';' );
					};

					parzer.Skip( '}' );

					if ( node->shlFileName.data() )
					{
//printf("add %s\n", id.ptr());
						hash[id.data()] = node;
					}
				}
				else if ( !strcmp( parzer.Str(), "rule" ) )
				{
					parzer.Next();

					clPtr<Rule> rule = new Rule;

					if ( parzer.Tok() != TOK_ID ) { parzer.Syntax(); }

					if ( !strcmp( parzer.Str(), "first" ) )
					{
						rule->type = Rule::FIRST;
					}
					else if ( !strcmp( parzer.Str(), "mask" ) )
					{
						rule->type = Rule::MASK;
					}
					else
					{
						parzer.Syntax( "bad rule type" );
					}

					parzer.Next();
					parzer.Skip( '(' );

					while ( true )
					{
						if ( parzer.Tok() != TOK_STR ) { break; }

						rule->list.Add( parzer.Str() );
						parzer.Next();

						if ( parzer.Tok() != ',' ) { break; }

						parzer.Next();
					};

					parzer.Skip( ')' );

					if ( parzer.Tok() != TOK_STR )
					{
						parzer.Syntax( "no syntax id" );
					}

					rule->id = parzer.Str();
					ruleList.append( rule );

					parzer.Next();
				}
				else
				{
					parzer.Syntax( "bad keyword" );
				}

				parzer.SkipAll( ';' );
			}

		}
		catch ( cexception* ex )
		{
			fprintf( stderr, "ERR:%s\n", ex->message() );
			ex->destroy();
		}
	}

	ShlConf::~ShlConf() {}


}; //namespace SHL
