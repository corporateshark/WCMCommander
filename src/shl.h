/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#pragma once

#include <wal.h>

#include <unordered_map>

namespace SHL
{
	using namespace wal;

	typedef int ColorId;
	typedef int StateId;

	class Chars;
	class Words;
	class Rule;
	class State;
	class Shl;
	struct RuleNode;

	class Chars: public iIntrusiveCounter
	{

		friend class Shl;
		unsigned char _data[0x100];
		Chars* _next;

	public:

		void Clear()         { memset( _data, 0, sizeof( _data ) ); }
		Chars(): _next( 0 )     { Clear(); }
		bool IsSet( unsigned c )  const { return _data[c & 0xFF] != 0;   }
		void Add( unsigned c )    { _data[c & 0xFF] = 1; }
		void Add( Chars& a )      { for ( int i = 0; i < 0x100; i++ ) { _data[i] |= a._data[i]; } }
		void Add( unsigned c1, unsigned c2 ) { c1 &= 0xFF; c2 &= 0xFF; for ( unsigned i = c1; i <= c2; i++ ) { _data[i] = 1; } }
		void Del( unsigned c )    { _data[c & 0xFF] = 0; }
		void Not()        { for ( int i = 0; i < 0x100; i++ ) { _data[i] = _data[i] ? 0 : 1; } }
		bool Parze( const char* s, cstrhash<clPtr<Chars> >* mhash );
	};

	class Words
	{
		friend class Shl;
		bool     _sens; //case sensitive
		cstrhash<int>  _hash;
		Words* _next;
	public:
		Words( bool sens ): _sens( sens ), _next( 0 ) {}
		void Add( const char* s, int color );
		bool Exist( const char* s, const char* end, int* color );
		~Words();
	};

	struct RuleNode
	{

		enum { TYPE_CHAR = 1, TYPE_MASK};

		char _type;
		char _count;

		union
		{
			unsigned char _ch[2]; //2 symbols for case unsens (if sens then in ch[1] must be 0), now doesn't work like this
			Chars* _chars;
		};

		char Type() const { return _type; }
		char Count() const { return _count; }
		unsigned char* Ch() { return _ch; }
		Chars* GetChars() { return _chars; }

		void Set( int ch, bool caseSensitive, char count );
		void Set( Chars* chars, char count );

		RuleNode() : _type( TYPE_CHAR ), _count( '1' )      { _ch[0] = _ch[1] = '?'; }
		RuleNode( int ch, bool caseSensitive, char count )   { Set( ch, caseSensitive, count ); }
		RuleNode( Chars* chars, char count )        { Set( chars, count ); }
		const unsigned char* Ok( const unsigned char* pS,  const unsigned char* end );
	};

	class Rule
	{
		friend class Shl;
		friend class State;
		//should be at least one node with width other than * or infinite loop
		ccollect<RuleNode, 100> _list;
		ColorId _color;
		Words*    _words; //case sensitive
		Words*    _ns_words; //no case sensitive
		State* _nextState;
		Rule*  _next;
	public:
		Rule(): _color( -1 ), _words( 0 ), _ns_words( 0 ), _nextState( 0 ), _next( 0 ) {}

		ColorId Color() const { return _color; }
		bool Valid()      { for ( int i = 0; i < _list.count(); i++ ) if ( _list[i].Count() == '1' || _list[i].Count() == '+' ) { return true; } return false; }

		void Add( const RuleNode& node )   { _list.append( node ); }
		void SetColor( ColorId c )   { _color = c; }
		void SetWords( Words* w )    { _words = w; }
		void SetNextState( State* s )   { _nextState = s; }

		const unsigned char* Ok( const unsigned char* s,  const unsigned char* end, ColorId* pColor );
		~Rule();
	};


	class State: public iIntrusiveCounter
	{
		friend class Shl;
		int      _id;
		ColorId     _color;
		ccollect<Rule* > _rules;
	public:
		State( int n ): _id( n ), _color( 0 ) {}
		void AddRule( Rule* p ) { _rules.append( p ); }
		void SetColor( ColorId c ) { _color = c; }
		StateId Id() const { return _id; }
		State* Next( const unsigned char** pS,  const unsigned char* end,  ColorId* pColorId );
		~State();
	};

	class ShlStream
	{
	public:
		ShlStream() {}
		enum { EOFCHAR = EOF };
		virtual const char* Name();
		virtual int Next();
		virtual ~ShlStream();
	};

	class ShlStreamFile: public ShlStream
	{
		std::vector<sys_char_t> _name;
		std::string _name_utf8;
		BFile _f;
	public:
		ShlStreamFile( sys_char_t* s ): _name( new_sys_str( s ) ), _name_utf8( sys_to_utf8( s ) ) { _f.Open( s ); }
		virtual const char* Name();
		virtual int Next();
		virtual ~ShlStreamFile();
	};

	class Shl: public iIntrusiveCounter
	{
		Chars* _charsList;
		Words* _wordsList;
		Rule*   _ruleList;
		ccollect<clPtr<State> > _states;
		StateId  _startState;

		Chars* AllocChars();
		Words* AllocWords( bool sens = true );
		Rule*  AllocRule();
		State* AllocState();
		State* GetState( const char* name, cstrhash<State*>& hash );
		void SetStartId( StateId id ) { _startState = id; }
	public:
		Shl();
		void Parze( ShlStream* stream, std::unordered_map< std::string, int >& colors );
		StateId GetStartId() const { return _startState; }
		StateId ScanLine( const unsigned  char* s, const unsigned  char* end, StateId state );
		StateId ScanLine( const unsigned char* s, char* colors, int count, StateId state );
		~Shl();
	};

	struct StrList
	{
		struct Node
		{
			std::vector<unicode_t> str;
			Node* next;
		};

		Node* first;
		StrList(): first( 0 ) {}
		void Add( const char* s ); //utf8
		void Add( const unicode_t* s );
		void Clear();
		~StrList() { Clear(); }
	};


	class ShlConf: public iIntrusiveCounter
	{

		struct Node: public iIntrusiveCounter
		{
			std::string name;
			std::string shlFileName;
//		clPtr< StrList > first;
//		clPtr< StrList > mimes;
//		clPtr< StrList > masks;
			clPtr< Shl > shl;
		};

		struct Rule: public iIntrusiveCounter
		{
			enum Type { FIRST = 1, MASK = 2 };
			Type type;
			StrList list;
			std::string id;
		};

		cstrhash<clPtr<Node> > hash;
		ccollect<clPtr<Rule> > ruleList;

		Shl* Get( const char* name, std::unordered_map< std::string, int >& colors );
	public:
		ShlConf();
		Shl* Get( const unicode_t* path, const unicode_t* firstLine, std::unordered_map< std::string, int >& colors );
		void Parze( sys_char_t* filePath );
		~ShlConf();
	};



}; //namespace SHL
