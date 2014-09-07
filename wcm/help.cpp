#include <swl.h>
#include "help.h"
#include "ncfonts.h"
#include "wcm-config.h"
#include "ltext.h"
#include "globals.h"
#include "bfile.h"
#include "string-util.h"
#ifdef _WIN32
#include "w32util.h"
#endif

using namespace wal;

// GCC
#ifdef __GNUC__
#  define PLATFORM_GCC
#  ifdef __clang__
#     define __COMPILER_VER__ "Clang " __VERSION__
#  else
#     define __COMPILER_VER__ "GCC " __VERSION__
#  endif
#endif

// Microsoft Visual C++
#ifdef _MSC_VER
#  define PLATFORM_MSVC
#define __COMPILER_VER__ "Microsoft Visual C++"
#endif

#if defined(_WIN64)
#  define BUILD_OS "Win64"
#elif defined(_WIN32)
#  define BUILD_OS "Win32"
#elif defined(__APPLE__)
#	define BUILD_OS "OS X"
#else
#  define BUILD_OS "Linux"
#endif


static char verString[] = "Wal Commander v 0.16.2 GitHub Edition (" __DATE__ "  " __TIME__ " via " __COMPILER_VER__ " for " BUILD_OS ")";

struct HelpStyle
{
	cfont* font;
	unsigned fg;
	unsigned bg;
	HelpStyle(): font( 0 ), fg( 0 ), bg( 0xFFFFFF ) {}
	HelpStyle( cfont* f, unsigned c, unsigned b ) : font( f ), fg( c ), bg( b ) {}
	cfont* Font() { return this ? font : ( cfont* )0; }
	unsigned Fg() { return this ? fg : 0; }
	unsigned Bg() { return this ? bg : 0xFFFFFF; }
};

//HelpGC сделан для кэширования операций GetTextExtents а то в X11 (со стандартными X11 фонтами) это пиздец как медленно

class HelpGC
{
	struct Node
	{
		cfont* font;
		clPtr<cstrhash<cpoint, unicode_t> > pHash;
		Node( cfont* f = 0 ): font( f ) { pHash = new cstrhash<cpoint, unicode_t>; }
	};
	ccollect<Node> _list;
	wal::GC* _gc;
	cfont* _font;
	HelpGC() {};
public:
	HelpGC( wal::GC* gc ): _gc( gc ), _font( 0 ) {}
	void Set( cfont* f ) { if ( _font != f ) { _gc->Set( f ); _font = f; } }
	void SetFillColor( unsigned c ) { _gc->SetFillColor( c ); }
	void SetTextColor( unsigned c ) { _gc->SetTextColor( c ); }
	void TextOut( int x, int y, const unicode_t* s ) { _gc->TextOut( x, y, s ); }
	void TextOutF( int x, int y, const unicode_t* s ) { _gc->TextOutF( x, y, s ); }
	cpoint GetTextExtents( const unicode_t* s );
	void FillRect( crect& r ) { _gc->FillRect( r ); }
	~HelpGC();
};

cpoint HelpGC::GetTextExtents( const unicode_t* s )
{
	cstrhash<cpoint, unicode_t>* h = 0;

	for ( int i = 0; i < _list.count(); i++ )
		if ( _list[i].font == _font )
		{
			h = _list[i].pHash.ptr();
			break;
		}

	if ( !h )
	{
		Node node( _font );
		h = node.pHash.ptr();
		_list.append( node );
	}

	cpoint* pp = h->exist( s );

	if ( pp )
	{
		return *pp;
	}

	cpoint p = _gc->GetTextExtents( s );
	h->put( ( unicode_t* )s, p );
	return p;
}

HelpGC::~HelpGC() {};



struct HelpNode: public iIntrusiveCounter
{
	HelpStyle* _style;
	//минимальная и максимальная ширина
	int _min; //если 0 то считается пробелом и может быть проигнорирован в начале или конце строки
	int _max;

	cpoint _pos; //относительно хозяина (хозяином и выставляется после prepare)
	cpoint _size; //текущая ширина и высота

	HelpNode( HelpStyle* style, int min = 0, int max = 10000 ): _style( style ), _min( min ), _max( max ), _pos( 0, 0 ), _size( 0, 0 ) {}
	virtual void Init( HelpGC& gc ); //инициализируется по данным и определяет min и max
	virtual void Prepare( int width ); //подготовка к размеру, выставляет _size
	virtual void Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect ); //нарисовать в нужном месте (обязан зарисовать весь размер _size)
	virtual ~HelpNode();
private:
	HelpNode() {}
};

struct HelpNodeSpace: public HelpNode
{
	int _count;

	HelpNodeSpace( HelpStyle* s = 0, int n = 1 ): HelpNode( s ), _count( n ) {}
	virtual void Init( HelpGC& gc );
	virtual void Prepare( int width );
	virtual void Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect );
	virtual ~HelpNodeSpace();
};


struct HelpNodeH: public HelpNode
{
	int _count;

	HelpNodeH( HelpStyle* s = 0, int n = 1 ): HelpNode( s ), _count( n ) {}
	virtual void Init( HelpGC& gc );
	virtual void Prepare( int width );
	virtual ~HelpNodeH();
};

struct HelpNodeV: public HelpNode
{
	int _count;

	HelpNodeV( HelpStyle* s = 0, int n = 1 ): HelpNode( s ), _count( n ) {}
	virtual void Init( HelpGC& gc );
	virtual void Prepare( int width );
	virtual ~HelpNodeV();
};

//текст фиксированной длины и ширины
struct HelpNodeWord: public HelpNode
{
	std::vector<unicode_t> _txt;

	HelpNodeWord( HelpStyle* style, const char* utf8, const char* addr = 0 );
	virtual void Init( HelpGC& gc );
	virtual void Prepare( int w );
	virtual void Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect );
	virtual ~HelpNodeWord();
};

//список элементов (по умолчанию расположенных вертикально)
struct HelpNodeList: public HelpNode
{

	struct Node
	{
		clPtr<HelpNode> item;
		bool paint; //расчитывается ProbeWidth
		Node() {}
		Node( clPtr<HelpNode> w ): item( w ) {}
	};

	ccollect<Node> _list;
	//int currentSelected;

	HelpNodeList( HelpStyle* style );
	int Count() const { return _list.count(); }
	void Append( clPtr<HelpNode> item );
	virtual void Init( HelpGC& gc );
	virtual void Prepare( int width );
	virtual void Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect );
	virtual ~HelpNodeList();
};

//абзац из элементов, элементы могут занать несколько строк
struct HelpNodeParagraph: public HelpNodeList
{


	enum {ALIGN_LEFT = 0, ALIGN_RIGHT, ALIGN_CENTER, ALIGN_WIDTH};
	int _align;

	HelpNodeParagraph( HelpStyle* style ): HelpNodeList( style ), _align( ALIGN_LEFT ) {};

	void SetAlign( int a ) { _align = a; }

	virtual void Init( HelpGC& gc );
	virtual void Prepare( int w );
	virtual ~HelpNodeParagraph();
};


struct HelpNodeTable: public HelpNode
{
	struct Pair
	{
		int minV, maxV;
		Pair(): minV( 0 ), maxV( 0 ) {}
		Pair( int _min, int _max ): minV( _min ), maxV( _max ) {}
	};

	ccollect<clPtr<ccollect<clPtr<HelpNode> > > > _tab;

	int _cols; //заполняется Init
	std::vector<Pair> _colPair; //создается Init

	void Append( clPtr<HelpNode> item ) { _tab[_tab.count() - 1]->append( item ); }
	void NL() { _tab.append( new ccollect< clPtr<HelpNode> > ); }

	HelpNodeTable( HelpStyle* s ): HelpNode( s )
	{
		NL(); //добавляем первую строку
	};

	virtual void Init( HelpGC& gc );
	virtual void Prepare( int width );
	virtual void Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect );
	virtual ~HelpNodeTable();
};


///// HelpNode

void HelpNode::Init( HelpGC& gc ) {}

void HelpNode::Prepare( int width )
{
	if ( width > _max ) { width = _max; }

	if ( width < _min ) { width = _min; }

	_size.x = width;
}
void HelpNode::Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect ) {}

HelpNode::~HelpNode() {}


///// HelpNodeSpace

void HelpNodeSpace::Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect )
{
	gc.SetFillColor( _style->Bg() );
	crect r( x, y, x + _size.x, y + _size.y );
	gc.FillRect( r );
}

void HelpNodeSpace::Init( HelpGC& gc )
{
	gc.Set( _style->Font() );
	static unicode_t sp[] = {' ', 0};
	_size = gc.GetTextExtents( sp );
	_size.x *= _count;
	_min = _max = _size.x;
}

void HelpNodeSpace::Prepare( int w ) {}

HelpNodeSpace::~HelpNodeSpace() {}



//регион (прямоугольный) из которого можно удалить прямоугольники и потом заполнить (не трогая удаленную площадь)
class HelpRgn
{
	struct Node
	{
		crect rect;
		Node* next;
		Node(): next( 0 ) {}
		Node( crect& r ): rect( r ), next( 0 ) {}
	};
	Node* list;
public:
	HelpRgn( crect rect ): list( 0 ) { if ( !rect.IsEmpty() ) { list = new Node( rect ); } }
	void Clear() { while ( list ) { Node* p = list; list = list->next; delete p; } }

	void Minus( crect rect );
	int Fill( HelpGC& gc );
	int Fill( wal::GC& gc );

	~HelpRgn();
};

HelpRgn::~HelpRgn() { Clear(); }


int HelpRgn::Fill( HelpGC& gc )
{
	int n = 0;

	for ( Node* p = list; p; p = p->next, n++ ) { gc.FillRect( p->rect ); }

	return n;
}


int HelpRgn::Fill( wal::GC& gc )
{
	int n = 0;

	for ( Node* p = list; p; p = p->next, n++ ) { gc.FillRect( p->rect ); }

	return n;
}

void HelpRgn::Minus( crect rect )
{
	if ( rect.IsEmpty() ) { return; }

	Node** pp = &list;

	while ( *pp )
	{
		{
			//проверяем пересечение
			crect& pr = pp[0]->rect;

			if ( pr.top >= rect.bottom || pr.bottom <= rect.top ||
			     pr.left >= rect.right || pr.right <= rect.left )
			{
				pp = &( pp[0]->next );
				continue;
			}
		}

		crect r = pp[0]->rect;

		{
			//free old
			Node* p = pp[0];
			pp[0] = p->next;
			delete p;
		}

		int top = r.top;

		if ( r.top < rect.top )
		{
			crect n( r.left, r.top, r.right, rect.top );
			ASSERT( !n.IsEmpty() );
			Node* p = new Node( n );
			p->next = pp[0];
			pp[0] = p;
			pp = &( p->next );
			top = rect.top;
		}

		int bottom = r.bottom;

		if ( r.bottom > rect.bottom )
		{
			crect n( r.left, rect.bottom, r.right, r.bottom );
			ASSERT( !n.IsEmpty() );
			Node* p = new Node( n );
			p->next = pp[0];
			pp[0] = p;
			pp = &( p->next );
			bottom = rect.bottom;
		}

		if ( r.left < rect.left )
		{
			crect n( r.left, top, rect.left, bottom );
			ASSERT( !n.IsEmpty() );
			Node* p = new Node( n );
			p->next = pp[0];
			pp[0] = p;
			pp = &( p->next );
		}

		if ( r.right > rect.right )
		{
			crect n( rect.right, top, r.right, bottom );
			ASSERT( !n.IsEmpty() );
			Node* p = new Node( n );
			p->next = pp[0];
			pp[0] = p;
			pp = &( p->next );
		}
	}
}



///// HelpNodeTable

void HelpNodeTable::Init( HelpGC& gc )
{
	_cols = 0;
	int i;

	for ( i = 0; i < _tab.count(); i++ )
		if ( _cols < _tab[i]->count() )
		{
			_cols = _tab[i]->count();
		}

	if ( _cols <= 0 )
	{
		_size.Set( 0, 0 );
		_min = _max = 0;
		return;
	}

	_colPair.resize( _cols );

	for ( i = 0; i < _tab.count(); i++ )
	{
		for ( int j = 0; j < _tab[i]->count(); j++ )
		{
			HelpNode& a = *( _tab[i]->get( j ).ptr() );
			a.Init( gc );

			if ( _colPair[j].minV < a._min ) { _colPair[j].minV = a._min; }

			if ( _colPair[j].maxV < a._max ) { _colPair[j].maxV = a._max; }
		}
	}

	_min = 0;
	_max = 0;

	for ( i = 0; i < _cols; i++ )
	{
		_min += _colPair[i].minV;
		_max += _colPair[i].maxV;
	}
}

void HelpNodeTable::Prepare( int width )
{
	if ( _cols < 0 )
	{
		_size.Set( 0, 0 );
		return;
	}

	int w = _min;
	int n = _cols;
	int i;

	std::vector<int> cw( _cols );

	for ( i = 0; i < _cols; i++ ) { cw[i] = _colPair[i].minV; }

	//распределяем добавочную длину
	while ( w < width && n > 0 )
	{
		int plus = ( width - w ) / n;

		if ( !plus ) { break; }

		n = 0;

		for ( i = 0; i < _cols; i++ )
		{
			int t = _colPair[i].maxV - cw[i];

			if ( t > plus ) { t = plus; }

			if ( t > 0 )
			{
				cw[i] += t;
				w += t;

				if ( cw[i] < _colPair[i].maxV )
				{
					n++;
				}
			}
		}
	}

	int H = 0;

	for ( i = 0; i < _tab.count(); i++ )
	{
		int lineH = 0;
		int x = 0;

		for ( int j = 0; j < _tab[i]->count(); j++ )
		{
			HelpNode* p = _tab[i]->get( j ).ptr();
			p->Prepare( cw[j] );

			if ( lineH < p->_size.y ) { lineH = p->_size.y; }

			p->_pos.Set( x, H );
			x += cw[j];
		}

		H += lineH;
	}

	_size.Set( w, H );
}

void HelpNodeTable::Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect )
{
	crect rect( x, y, x + _size.x, y + _size.y );

	if ( rect.left < visibleRect.left ) { rect.left = visibleRect.left; }

	if ( rect.right > visibleRect.right ) { rect.right = visibleRect.right; }

	if ( rect.top < visibleRect.top ) { rect.top = visibleRect.top; }

	if ( rect.bottom > visibleRect.bottom ) { rect.bottom = visibleRect.bottom; }

	HelpRgn rgn( rect );

	for ( int i = 0; i < _tab.count(); i++ )
	{
		for ( int j = 0; j < _tab[i]->count(); j++ )
		{
			HelpNode* p = _tab[i]->get( j ).ptr();
			int itemX = x + p->_pos.x;
			int itemY = y + p->_pos.y;
			cpoint iSize = p->_size;

			crect r( itemX, itemY, itemX + iSize.x, itemY + iSize.y );

			if ( r.Cross( visibleRect ) )
			{
				p->Paint( gc, itemX, itemY, selected, visibleRect );
				rgn.Minus( r );
			}
		}
	}

	gc.SetFillColor( _style->Bg() );
	rgn.Fill( gc );
}

HelpNodeTable::~HelpNodeTable() {}


///// HelpNodeV
void HelpNodeV::Init( HelpGC& gc )
{
	gc.Set( _style->Font() );
	int V = gc.GetTextExtents( ABCString ).y;
	_min = -1;
	_max = 0;
	_size.x = 0;
	_size.y = ( V * _count ) / 10;
}

void  HelpNodeV::Prepare( int w ) {}

HelpNodeV::~HelpNodeV() {}


///// HelpNodeH

void HelpNodeH::Init( HelpGC& gc )
{
	gc.Set( _style->Font() );
	int H = gc.GetTextExtents( ABCString ).x / ABCStringLen;
	_min = _max = _size.x = ( H * _count ) / 10;
	_size.y = 0;
}


void HelpNodeH::Prepare( int w ) {}

HelpNodeH::~HelpNodeH() {}

///// HelpNodeWord
HelpNodeWord::HelpNodeWord( HelpStyle* style, const char* utf8, const char* addr )
	:  HelpNode( style ),
	   _txt( utf8_to_unicode( utf8 ) )
{
}

void HelpNodeWord::Init( HelpGC& gc )
{
	gc.Set( _style->Font() );
	_size = gc.GetTextExtents( _txt.data() );
	_min = _max = _size.x;
}

void HelpNodeWord::Prepare( int w ) {}

void HelpNodeWord::Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect )
{
	gc.Set( _style->Font() );
	gc.SetFillColor( _style->Bg() );
	gc.SetTextColor( _style->Fg() );
	gc.TextOutF( x, y, _txt.data() );
}


HelpNodeWord::~HelpNodeWord() {}

///// HelpNodeList

HelpNodeList::HelpNodeList( HelpStyle* style )
	: HelpNode( style, 0, 0 )
{}

void HelpNodeList::Append( clPtr<HelpNode> word )
{
	_list.append( Node( word ) );
}

void HelpNodeList::Init( HelpGC& gc )
{
	_min = 0;
	_max = 0;

	for ( int i = 0; i < _list.count(); i++ )
	{
		HelpNode* p = _list[i].item.ptr();
		p->Init( gc );

		if ( _min < p->_min ) { _min = p->_min; }

		if ( _max < p->_max ) { _max = p->_max; }
	}
}

void HelpNodeList::Prepare( int width )
{
	//вертикальный расчет
	int count = _list.count();

	int w = 0;
	int h = 0;

	for ( int i = 0; i < count; i++ )
	{
		HelpNode* p = _list[i].item.ptr();
		p->Prepare( width );
		cpoint size = p->_size;
		p->_pos.x = 0;
		p->_pos.y = h;
		_list[i].paint = true;
		h += size.y;

		if ( w < size.x ) { w = size.x; }
	}

	_size.Set( w, h );
}


void HelpNodeList::Paint( HelpGC& gc, int x, int y, bool selected, crect visibleRect )
{
	crect rect( x, y, x + _size.x, y + _size.y );

	if ( rect.left < visibleRect.left ) { rect.left = visibleRect.left; }

	if ( rect.right > visibleRect.right ) { rect.right = visibleRect.right; }

	if ( rect.top < visibleRect.top ) { rect.top = visibleRect.top; }

	if ( rect.bottom > visibleRect.bottom ) { rect.bottom = visibleRect.bottom; }

	HelpRgn rgn( rect );

	int count = _list.count();

	for ( int i = 0; i < count; i++ )
		if ( _list[i].paint )
		{
			int itemX = x + _list[i].item->_pos.x;
			int itemY = y + _list[i].item->_pos.y;
			cpoint iSize = _list[i].item->_size;
			crect r( itemX, itemY, itemX + iSize.x, itemY + iSize.y );

			if ( r.Cross( visibleRect ) )
			{
				_list[i].item->Paint( gc, itemX, itemY, selected, visibleRect );
				rgn.Minus( r );
			}
		}

	gc.SetFillColor( _style->Bg() );
	rgn.Fill( gc );
}


HelpNodeList::~HelpNodeList() {}

///// HelpNodeParagraph

void HelpNodeParagraph::Init( HelpGC& gc )
{
	int minW = 0, maxW = 0;

	for ( int i = 0; i < _list.count(); i++ )
	{
		_list[i].item->Init( gc );

		if ( minW < _list[i].item->_min ) { minW = _list[i].item->_min; }

		maxW += _list[i].item->_max;
	}

	_min = minW;
	_max = maxW;
}

void HelpNodeParagraph::Prepare( int width )
{
	int w = _min, h = 0;
	int itemH = 0;
	int pos = 0;
	int i;

	if ( w < width ) { w = width; }

	//if (w>_max) w = _max;

	ccollect<Node*> curLine;

	for ( i = 0; i < _list.count(); i++ )
	{
		_list[i].item->Prepare( ( i == _list.count() - 1 ) ? width : 0 ); //для непоследних элементовпо минимальному размеру
		int n = _list[i].item->_size.x;

		if ( pos > 0 && pos + n > w )
		{
			{
				//выравниваем
				if ( _align == ALIGN_RIGHT )
				{
					for ( int t = 0; t < curLine.count(); t++ ) { curLine[t]->item->_pos.x += w - pos; }
				}
				else if ( _align == ALIGN_CENTER )
				{
					for ( int t = 0; t < curLine.count(); t++ ) { curLine[t]->item->_pos.x += ( w - pos ) / 2; }
				}
				else if ( _align == ALIGN_WIDTH && curLine.count() > 1 )
				{
					int add = ( w - pos ) / ( curLine.count() - 1 );
					int n = 0;

					for ( int t = 0; t < curLine.count(); t++, n += add )
					{
						curLine[t]->item->_pos.x += n;
					}
				}
			}

			pos = 0;
			h += itemH;
			itemH = 0;
			curLine.clear();
		}

		_list[i].item->_pos.x = pos;
		_list[i].item->_pos.y = h;

		_list[i].paint = pos > 0 || _list[i].item->_min != 0; //игнорируем пробелы в начале строки

		if ( _list[i].paint )
		{
			pos += n;

			if ( itemH < _list[i].item->_size.y )
			{
				itemH = _list[i].item->_size.y;
			}

			curLine.append( &( _list[i] ) );
		}
	}

	if ( curLine.count() > 0 ) //выравниваем
	{
		if ( _align == ALIGN_RIGHT )
			for ( int t = 0; t < curLine.count(); t++ ) { curLine[t]->item->_pos.x += w - pos; }
		else if ( _align == ALIGN_CENTER )
			for ( int t = 0; t < curLine.count(); t++ ) { curLine[t]->item->_pos.x += ( w - pos ) / 2; }
	}


	h += itemH;
	_size.Set( w, h );
}

HelpNodeParagraph::~HelpNodeParagraph() {};







///// HelpParzer

class HelpParzerStream
{
public:
	virtual int GetChar(); //eof is 0
	virtual ~HelpParzerStream();
};

int HelpParzerStream::GetChar() { return 0; }
HelpParzerStream::~HelpParzerStream() {}

class HelpParzerMem: public HelpParzerStream
{
	const char* str;
public:
	HelpParzerMem( const char* s ): str( s ) {}
	virtual int GetChar(); //eof is 0
	virtual ~HelpParzerMem();
};

int HelpParzerMem::GetChar() { return *str ? *( str++ ) : 0; }
HelpParzerMem::~HelpParzerMem() {}

/*

   / - экранирующий символ, т.е. // - /

   /l /r /c /w -выравнивание абзаца (побеждает последний в абзаце) по умолчанию /l

   // %+ %- включить, выключить автоперенос (в пределах блока) по умолчанию включен

   <s [id]> смена текущего стиля (в пределах блока)
   /n переход на новый абзац
   <v [N]> -вертикальная вставка
   <h [N]> -горизонтальная вставка (размар 1/10 от ширины символа текущего фонта (средней ширины A B и С))

   @t - таблица
   @n - конец строки таблицы
   @c - конец колонки в строке таблицы
   @e - конец таблицы

   блок ограничивается символами { и }

*/

struct HelpParzerBlockData
{
	HelpStyle* style;
	HelpParzerBlockData(): style( 0 ) {}
};

class HelpParzer
{
	HelpParzerStream* stream;
	cstrhash<HelpStyle>* styles;

	int cchar;
	int backChar;
	int NextChar() { if ( backChar ) { cchar = backChar; backChar = 0; } else { cchar = stream->GetChar(); } return cchar; }
	void PutCharBack( int c ) { backChar = cchar; cchar = c; }

	enum Tokens { TOK_EOF = 0, TOK_SPACE = -1000, TOK_WORD, TOK_NL, TOK_ALEFT, TOK_ARIGHT, TOK_ACENTER, TOK_AWIDTH,
	              TOK_STYLE,
	              TOK_H, TOK_V,
	              TOK_TAB, TOK_TAB_END,
	              TOK_TAB_NL, TOK_TAB_NF
	            };
	int tok; //current token
	int num;
	ccollect<char, 0x100> word; //or style name
	int NextToken();

	struct BlockNode
	{
		HelpParzerBlockData data;
		BlockNode* next;
		BlockNode( HelpParzerBlockData& d, BlockNode* nx ): data( d ), next( nx ) {}
	};

	BlockNode* blockStack;
	void PopBlockData( HelpParzerBlockData& d ) { if ( blockStack ) { BlockNode* p = blockStack; blockStack = blockStack->next; currentBData = p->data; delete p; } }
	void PushBlockData( HelpParzerBlockData& d ) { blockStack = new BlockNode( d, blockStack ); }

	HelpParzerBlockData currentBData;

public:
	HelpParzer( HelpParzerStream* _stream, cstrhash<HelpStyle>* _styles )
		: stream( _stream ), styles( _styles ), blockStack( 0 ), tok( 'a' ), cchar( 'a' ), backChar( 0 )
	{
		if ( styles ) { currentBData.style = styles->exist( "def" ); }

		NextChar();
		NextToken();
	}

	clPtr<HelpNode> Parze();
	clPtr<HelpNode> ParzeTable();

	~HelpParzer();
};

inline bool IsSpaceChar( int c ) { return ( c > 0 && c <= 32 ); }

int HelpParzer::NextToken()
{
begin:

	if ( tok == TOK_EOF ) { return tok; }

	if ( IsSpaceChar( cchar ) )
	{
		tok = TOK_SPACE;

		while ( IsSpaceChar( cchar ) ) { NextChar(); }

		return tok;
	}

	word.clear();

	while ( true )
	{
		if ( IsSpaceChar( cchar ) )
		{
			if ( word.count() > 0 ) { tok = TOK_WORD; word.append( 0 ); return tok; }

			goto begin;
		}

		if ( !cchar )
		{
			if ( word.count() > 0 ) { tok = TOK_WORD; word.append( 0 ); return tok; }

			tok = TOK_EOF;
			return tok;
		}

		if ( cchar == '{' || cchar == '}' )
		{
			if ( word.count() > 0 ) { tok = TOK_WORD; word.append( 0 ); return tok; }

			tok = cchar;
			NextChar();
			return tok;
		}

		if ( cchar == '/' )
		{
			NextChar();

			if ( word.count() > 0 ) { PutCharBack( '/' ); tok = TOK_WORD; word.append( 0 ); return tok; }

			switch ( cchar )
			{
				case 'n':
					tok = TOK_NL;
					NextChar();
					return tok;
					break;

				case 'l':
					tok = TOK_ALEFT;
					NextChar();
					return tok;

				case 'r':
					tok = TOK_ARIGHT;
					NextChar();
					return tok;

				case 'c':
					tok = TOK_ACENTER;
					NextChar();
					return tok;

				case 'w':
					tok = TOK_AWIDTH;
					NextChar();
					return tok;

				default:
					word.append( cchar );
					NextChar();
					continue;
			}

			word.append( cchar );
			NextChar();
			continue;
		}


		if ( cchar == '@' )
		{
			NextChar();

			if ( word.count() > 0 ) { PutCharBack( '@' ); tok = TOK_WORD; word.append( 0 ); return tok; }

			switch ( cchar )
			{
				case 't':
					tok = TOK_TAB;
					NextChar();
					return tok;

				case 'e':
					tok = TOK_TAB_END;
					NextChar();
					return tok;

				case 'n':
					tok = TOK_TAB_NL;
					NextChar();
					return tok;

				case 'c':
					tok = TOK_TAB_NF;
					NextChar();
					return tok;

				default:
					word.append( cchar );
					NextChar();
					continue;
			}

			word.append( cchar );
			NextChar();
			continue;
		}


		if ( cchar == '<' )
		{
			NextChar();

			if ( word.count() > 0 ) { PutCharBack( '<' ); tok = TOK_WORD; word.append( 0 ); return tok; }

			switch ( cchar )
			{
				case 's':
					NextChar();

					while ( IsSpaceChar( cchar ) ) { NextChar(); }

					while ( cchar && !IsSpaceChar( cchar ) && cchar != '>' )
					{
						word.append( cchar );
						NextChar();
					}

					word.append( 0 );

					while ( IsSpaceChar( cchar ) ) { NextChar(); }

					if ( cchar == '>' ) { NextChar(); }

					tok = TOK_STYLE;
					return tok;

				case 'v':
				{
					NextChar();

					while ( IsSpaceChar( cchar ) ) { NextChar(); }

					int n = 0;

					for ( ; cchar && cchar >= '0' && cchar <= '9'; NextChar() )
					{
						n = n * 10 + cchar - '0';
					}

					num = n ? n : 10;

					while ( IsSpaceChar( cchar ) ) { NextChar(); }

					if ( cchar == '>' ) { NextChar(); }

					tok = TOK_V;
					return tok;
				}

				case 'h':
				{
					NextChar();

					while ( IsSpaceChar( cchar ) ) { NextChar(); }

					int n = 0;

					for ( ; cchar && cchar >= '0' && cchar <= '9'; NextChar() )
					{
						n = n * 10 + cchar - '0';
					}

					num = n ? n : 10;

					while ( IsSpaceChar( cchar ) ) { NextChar(); }

					if ( cchar == '>' ) { NextChar(); }

					tok = TOK_H;
					return tok;
				}


				default:
					word.append( cchar );
					NextChar();
					continue;
			}

			word.append( cchar );
			NextChar();
			continue;
		}

		word.append( cchar );
		NextChar();
	}

	return tok;
}

clPtr<HelpNode> HelpParzer::ParzeTable()
{
	HelpNodeTable* pTable;
	clPtr<HelpNode> ret = pTable = new HelpNodeTable( currentBData.style );

	while ( true )
	{
		if ( tok == TOK_EOF ) { break; }

		if ( tok == TOK_TAB_END ) { NextToken(); break; }

		switch ( tok )
		{

			case TOK_TAB_NL:
				pTable->NL();
				NextToken();
				pTable->Append( Parze() );
				continue;

			case TOK_TAB_NF:
				NextToken();
				pTable->Append( Parze() );
				continue;

			default:
				pTable->Append( Parze() );
				continue;
		};

		NextToken();
	}

	return ret;
}


clPtr<HelpNode> HelpParzer::Parze()
{
	HelpNodeList* pList;
	clPtr<HelpNode> ret = pList = new HelpNodeList( currentBData.style );

	HelpNodeParagraph* pPar;
	clPtr<HelpNode> paragraph = pPar = new HelpNodeParagraph( currentBData.style );

	while ( true )
	{
		if ( tok == TOK_EOF ) { break; }

		if ( tok == TOK_TAB_END ) { break; }

		if ( tok == TOK_TAB_NL ) { break; }

		if ( tok == TOK_TAB_NF ) { break; }

		switch ( tok )
		{
			case '{':
				this->PushBlockData( currentBData );
				break;

			case '}':
				this->PopBlockData( currentBData );
				break;

			case TOK_SPACE:
				pPar->Append( new HelpNodeSpace( this->currentBData.style, 1 ) );
				break;

			case TOK_V:
				pPar->Append( new HelpNodeV( this->currentBData.style, num ) );
				break;

			case TOK_H:
				pPar->Append( new HelpNodeH( this->currentBData.style, num ) );
				break;

			case TOK_WORD:
				pPar->Append( new HelpNodeWord( this->currentBData.style, word.ptr(), 0 ) );
				break;

			case TOK_NL:
				if ( pPar->Count() > 0 ) { pList->Append( paragraph ); }

				paragraph = pPar = new HelpNodeParagraph( currentBData.style );
				break;

			case TOK_ALEFT:
				pPar->SetAlign( HelpNodeParagraph::ALIGN_LEFT );
				break;

			case TOK_ARIGHT:
				pPar->SetAlign( HelpNodeParagraph::ALIGN_RIGHT );
				break;

			case TOK_ACENTER:
				pPar->SetAlign( HelpNodeParagraph::ALIGN_CENTER );
				break;

			case TOK_AWIDTH:
				pPar->SetAlign( HelpNodeParagraph::ALIGN_WIDTH );
				break;

			case TOK_STYLE:
			{
				HelpStyle* s = styles->exist( word.ptr() );

				if ( s ) { currentBData.style = s; }
			}
			break;

			case TOK_TAB:
				NextToken();
				pPar->Append( ParzeTable() );
				//ParzeTable забирает токен окончания таблицы
				continue;

			default:
				break;
		};

		NextToken();
	};

	//if (pPar->Count()>0)
	pList->Append( paragraph );

	return ret;
}

HelpParzer::~HelpParzer() {}



class HelpFile
{
	cstrhash< std::vector<char> > hash;
	bool loaded;
	bool LoadFile( sys_char_t* name );
	void Load();
public:
	HelpFile(): loaded( false ) {}
	const char* GetTheme( const char* theme );
	~HelpFile();
};

bool HelpFile::LoadFile( sys_char_t* name )
{
	try
	{
		BFile f;
		ccollect<char, 1000> collector;
//printf("file-'%s'\n", name);
		f.Open( name );

		std::vector<char> thName;

		char buf[4096];

		while ( f.GetStr( buf, sizeof( buf ) ) )
		{
			char* s = buf;

			if ( s[0] == '[' )
			{
				s++;

				while ( IsSpaceChar( *s ) ) { s++; }

				ccollect<char> str;

				while ( *s && *s != ']' && !IsSpaceChar( *s ) ) { str.append( *s ); s++; }

				while ( IsSpaceChar( *s ) ) { s++; }

				if ( *s == ']' && str.count() > 0 ) //ok
				{

					str.append( 0 );

					if ( thName.data() )
					{
						collector.append( 0 );
						hash[thName.data()] = collector.grab();
					}

					thName = str.grab();
					continue;
				}
			}

			if ( thName.data() )
			{
				int n = strlen( buf );
				collector.append_list( buf, n );
			}

		}

		if ( thName.data() )
		{
			collector.append( 0 );
			hash[thName.data()] = collector.grab();
		}

		f.Close();

	}
	catch ( cexception* ex )
	{
		ex->destroy();
		return false;
	}

	return true;
}


void HelpFile::Load()
{
	if ( loaded ) { return; }

	loaded = true;

	const char* langId = wcmConfig.systemLang.data() ? wcmConfig.systemLang.data() : "+";

	if ( langId[0] == '-' ) { return; }

	if ( langId[0] != '+' )
	{
#ifdef _WIN32
		LoadFile( carray_cat<sys_char_t>( GetAppPath().data(),
		                                  utf8_to_sys( carray_cat<char>( "\\lang\\help.", langId ).data() ).data() ).data() );
#else

		if ( !LoadFile( utf8_to_sys( carray_cat<char>( "install-files/share/wcm/lang/help.", langId ).data() ).data() ) )
		{
			LoadFile( utf8_to_sys( carray_cat<char>( UNIX_CONFIG_DIR_PATH "/lang/help.", langId ).data() ).data() );
		}

#endif
		return;
	};

#ifdef _WIN32
	if ( !LoadFile( carray_cat<sys_char_t>( GetAppPath().data(),
	                                        utf8_to_sys( carray_cat<char>( "\\lang\\help.", sys_locale_lang_ter() ).data() ).data() ).data() )
	   )
		LoadFile( carray_cat<sys_char_t>( GetAppPath().data(),
		                                  utf8_to_sys( carray_cat<char>( "\\lang\\help.", sys_locale_lang() ).data() ).data() ).data() );

#else

	if (
	   !LoadFile( utf8_to_sys( carray_cat<char>( "install-files/share/wcm/lang/help.", sys_locale_lang_ter() ).data() ).data() ) &&
	   !LoadFile( utf8_to_sys( carray_cat<char>( "install-files/share/wcm/lang/help.", sys_locale_lang() ).data() ).data() ) &&
	   !LoadFile( utf8_to_sys( carray_cat<char>( UNIX_CONFIG_DIR_PATH "/lang/help.", sys_locale_lang_ter() ).data() ).data() )
	)
	{
		LoadFile( utf8_to_sys( carray_cat<char>( UNIX_CONFIG_DIR_PATH "/lang/help.", sys_locale_lang() ).data() ).data() );
	}

#endif
}

const char* HelpFile::GetTheme( const char* theme )
{
	Load();
	std::vector<char>* p = hash.exist( theme );
	return p ? p->data() : 0;
}


HelpFile::~HelpFile() {}



/////


#include "nc.h"

extern const char* helpData_main;
extern const char* helpData_edit;
extern const char* helpData_view;

static HelpFile helpFile;

static clPtr<HelpNode> GetHelpNode( const char* theme, cstrhash<HelpStyle>* pStyles )
{
	const char* ptext = helpFile.GetTheme( theme );

	if ( !ptext )
	{
		if ( !strcmp( theme, "main" ) ) { ptext = helpData_main; }
		else if ( !strcmp( theme, "edit" ) ) { ptext = helpData_edit; }
		else if ( !strcmp( theme, "view" ) ) { ptext = helpData_view; }
	}

	if ( !ptext ) { return 0; }

	HelpParzerMem mstream( ptext );
	HelpParzer parzer( &mstream, pStyles );
	return parzer.Parze();
}

int UiClassHelpWin  = GetUiID( "HelpWin" );

class HelpWin: public Win
{
	clPtr<HelpNode> data;
	int dataWidth;
	int dataHeight;

	int xOffset;
	int yOffset;

	int captureDelta;
	ScrollBar vScroll;
	ScrollBar hScroll;
	Layout layout;
	crect helpRect;
	crect scrollRect;
	cstrhash<HelpStyle> styles;

	HelpStyle style_head;
	HelpStyle style_def;
	HelpStyle style_red;
	HelpStyle style_bold;

protected:
	void CalcScroll();
public:
	HelpWin( const char* theme, Win* _parent, crect* rect );
	bool Ok() { return data.ptr() != 0; }
	void MoveXOffset( int n );
	void MoveYOffset( int n );
	virtual void Paint( wal::GC& gc, const crect& paintRect );
	virtual void EventSize( cevent_size* pEvent );
	virtual bool EventKey( cevent_key* pEvent );
	virtual bool EventMouse( cevent_mouse* pEvent );
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual void EventTimer( int tid );
	virtual int UiGetClassId();
	virtual ~HelpWin();
};

int HelpWin::UiGetClassId() { return UiClassHelpWin; }

static int uiClassHelpWin  = GetUiID( "HelpWin" );
static int uiStyleHead = GetUiID( "style-head" );
static int uiStyleDef = GetUiID( "style-def" );
static int uiStyleRed = GetUiID( "style-red" );
static int uiStyleBold = GetUiID( "style-bold" );

HelpWin::HelpWin( const char* theme, Win* parent, crect* rect )
	:  Win( Win::WT_CHILD, WH_TABFOCUS | WH_CLICKFOCUS, parent, rect, uiClassHelpWin ),
	   data( 0 ),
	   xOffset( 0 ), yOffset( 0 ),
	   captureDelta( 0 ),
	   vScroll( 0, this, true, true ),
	   hScroll( 0, this, false ),
	   layout( 4, 4 ),
	   style_head( helpHeadFont.ptr(), UiGetColor( uiColor, uiStyleHead, 0, 0 ), UiGetColor( uiBackground, uiStyleHead, 0, 0xFFFFFF ) ),
	   style_def ( helpTextFont.ptr(), UiGetColor( uiColor, uiStyleDef, 0, 0 ), UiGetColor( uiBackground, uiStyleDef, 0, 0xFFFFFF ) ),
	   style_red ( helpTextFont.ptr(), UiGetColor( uiColor, uiStyleRed, 0, 0 ), UiGetColor( uiBackground, uiStyleRed, 0, 0xFFFFFF ) ),
	   style_bold( helpBoldFont.ptr(), UiGetColor( uiColor, uiStyleBold, 0, 0 ), UiGetColor( uiBackground, uiStyleBold, 0, 0xFFFFFF ) )
{
	styles["def"] = style_def;
	styles["red"] = style_red;
	styles["head"] = style_head;
	styles["bold"] = style_bold;

	vScroll.Show();
	vScroll.Enable();
	hScroll.Show();
	hScroll.Enable();

	vScroll.SetManagedWin( this );
	hScroll.SetManagedWin( this );

	layout.AddWin( &vScroll, 1, 2 );
	layout.AddWin( &hScroll, 2, 1 );
	layout.AddRect( &helpRect, 1, 1 );
	layout.AddRect( &scrollRect, 2, 2 );
	layout.LineSet( 0, 2 );
	layout.LineSet( 3, 2 );
	layout.ColSet( 0, 2 );
	layout.ColSet( 3, 2 );
	layout.SetColGrowth( 1 );
	layout.SetLineGrowth( 1 );
	this->SetLayout( &layout );
	this->RecalcLayouts();

	data = GetHelpNode( theme, &styles );

	if ( data.ptr() )
	{
		wal::GC gc( this );
		HelpGC hgc( &gc );
		data->Init( hgc );
		data->Prepare( 600 );
	}
}

void HelpWin::CalcScroll()
{
	if ( dataHeight <= 0 ) { dataHeight = 1; }

	if ( dataWidth <= 0 ) { dataWidth = 1; }

	ScrollInfo vsi, hsi;
	vsi.pageSize = helpRect.Height();
	vsi.size = dataHeight;
	vsi.pos = yOffset;

	hsi.pageSize = helpRect.Width();
	hsi.size = dataWidth;
	hsi.pos = xOffset;

	bool vVisible = vScroll.IsVisible();
	vScroll.Command( CMD_SCROLL_INFO, SCMD_SCROLL_VCHANGE, this, &vsi );

	bool hVisible = hScroll.IsVisible();
	hScroll.Command( CMD_SCROLL_INFO, SCMD_SCROLL_HCHANGE, this, &hsi );

	if ( vVisible != vScroll.IsVisible() || hVisible != hScroll.IsVisible() )
	{
		this->RecalcLayouts();
	}

}

void HelpWin::MoveXOffset( int n )
{
	if ( n + helpRect.Width() >= dataWidth ) { n = dataWidth - helpRect.Width(); }

	if ( n < 0 ) { n = 0; }

	if ( xOffset != n )
	{
		xOffset = n;
		CalcScroll();
		Invalidate();
	}
}

void HelpWin::MoveYOffset( int n )
{
	if ( n + helpRect.Height() >= dataHeight ) { n = dataHeight - helpRect.Height(); }

	if ( n < 0 ) { n = 0; }

	if ( yOffset != n )
	{
		yOffset = n;
		CalcScroll();
		Invalidate();
	}
}


bool HelpWin::Command( int id, int subId, Win* win, void* data )
{
	if ( id == CMD_SCROLL_INFO )
	{
		switch ( subId )
		{
			case SCMD_SCROLL_LINE_UP:
				MoveYOffset( yOffset - 10 );
				break;

			case SCMD_SCROLL_LINE_DOWN:
				MoveYOffset( yOffset + 10 );
				break;

			case SCMD_SCROLL_PAGE_UP:
				MoveYOffset( yOffset - helpRect.Height() );
				break;

			case SCMD_SCROLL_PAGE_DOWN:
				MoveYOffset( yOffset + helpRect.Height() );
				break;

			case SCMD_SCROLL_LINE_LEFT:
				MoveXOffset( xOffset - 10 );
				break;

			case SCMD_SCROLL_LINE_RIGHT:
				MoveXOffset( xOffset + 10 );
				break;

			case SCMD_SCROLL_PAGE_LEFT:
				MoveXOffset( xOffset - helpRect.Width() );
				break;

			case SCMD_SCROLL_PAGE_RIGHT:
				MoveXOffset( xOffset + helpRect.Width() );
				break;

			case SCMD_SCROLL_TRACK:
				if ( win == &vScroll )
				{
					MoveYOffset( ( ( int* )data )[0] );
				}
				else
				{
					MoveXOffset( ( ( int* )data )[0] );
				}

				break;
		}

		return true;
	}

	return Win::Command( id, subId, win, data );
}

void HelpWin::Paint( wal::GC& gc, const crect& paintRect )
{
	crect rect = ClientRect();
	Draw3DButtonW2( gc, rect, 0x808080, false );

	if ( !scrollRect.IsEmpty() )
	{
		gc.SetFillColor( 0xD0D0D0 );
		gc.FillRect( scrollRect ); //CCC
	}

	rect.Dec();

	if ( data.ptr() )
	{
		HelpRgn rgn( rect );
		rgn.Minus( crect( helpRect.left - xOffset, helpRect.top - yOffset,
		                  helpRect.left + data->_size.x - xOffset, helpRect.top + data->_size.y - yOffset ) );
		gc.SetFillColor( data->_style->Bg() );
		rgn.Fill( gc );

		gc.SetClipRgn( &helpRect );
		HelpGC hgc( &gc );
		data->Paint( hgc, helpRect.left - xOffset, helpRect.top - yOffset, false, rect );
	}
	else
	{
		gc.SetFillColor( 0xFFFFFF ); //0x808080);
		gc.FillRect( rect );
	}
}

void HelpWin::EventSize( cevent_size* pEvent )
{
	if ( data.ptr() )
	{
		data->Prepare( helpRect.Width() < 500 ? 500 : helpRect.Width() );
		dataWidth = data->_size.x;
		dataHeight = data->_size.y;
	}
	else
	{
		dataWidth = 1;
		dataHeight = 1;
	}

	this->CalcScroll();
}

bool HelpWin::EventMouse( cevent_mouse* pEvent )
{
	if ( !IsEnabled() ) { return false; }

	if ( pEvent->Type() == EV_MOUSE_PRESS )
	{

		if ( pEvent->Button() == MB_X1 )
		{
			MoveYOffset( yOffset - helpRect.Height() / 3 );
			return true;
		}

		if ( pEvent->Button() == MB_X2 )
		{
			MoveYOffset( yOffset + helpRect.Height() / 3 );
			return true;
		}
	}


//	int n = (pEvent->Point().y - helpRect.top)/this->itemHeight + first;

	/*
	   if (pEvent->Type() == EV_MOUSE_PRESS && pEvent->Button() == MB_L && listRect.In(pEvent->Point()))
	   {
	      captureDelta=0;
	      MoveCurrent(n);
	      this->SetCapture();
	      this->SetTimer(0,100);
	      return true;
	   }

	   if (pEvent->Type() == EV_MOUSE_DOUBLE)
	   {
	      MoveCurrent(n);
	      Command(CMD_ITEM_CLICK, GetCurrent(), this, 0);
	      return true;
	   }
	*/
	if ( pEvent->Type() == EV_MOUSE_MOVE && IsCaptured() )
	{
		if ( pEvent->Point().y >= helpRect.top && pEvent->Point().y <= helpRect.bottom )
		{
			captureDelta = 0;
		}
		else
		{
			captureDelta = ( pEvent->Point().y > helpRect.bottom ) ? +1 : ( pEvent->Point().y < helpRect.top ) ? -1 : 0;
		}

		return true;
	}

	if ( pEvent->Type() == EV_MOUSE_RELEASE && pEvent->Button() == MB_L )
	{
		this->ReleaseCapture();
		this->DelTimer( 0 );
		return true;
	}

	return false;
}

void HelpWin::EventTimer( int tid )
{
	if ( tid == 0 && captureDelta )
	{
//		int n = current + captureDelta;
//		if (n>=0 && n<this->count)
//			MoveCurrent(n);
	}
}

bool HelpWin::EventKey( cevent_key* pEvent )
{
	if ( pEvent->Type() == EV_KEYDOWN )
	{
		switch ( pEvent->Key() )
		{
			case VK_DOWN:
				MoveYOffset( yOffset + 10 );
				break;

			case VK_UP:
				MoveYOffset( yOffset - 10 );
				break;

			case VK_END:
				MoveYOffset( dataHeight - helpRect.Height() );
				break;

			case VK_HOME:
				MoveYOffset( 0 );
				break;

			case VK_PRIOR:
				MoveYOffset( yOffset - helpRect.Height() );
				break;

			case VK_NEXT:
				MoveYOffset( yOffset + helpRect.Height() );
				break;

			case VK_LEFT:
				MoveXOffset( xOffset - 10 );
				break;

			case VK_RIGHT:
				MoveXOffset( xOffset + 10 );
				break;

			default:
				return false;
		}

		Invalidate();
		return true;
	}

	return false;
}

HelpWin::~HelpWin() {}




/////
class HelpDlg: public NCDialog
{
	Layout lo;
	StaticLine ver;
	HelpWin helpWin;
//	cstrhash<HelpStyle> styles;
public:
	HelpDlg( NCDialogParent* parent, const char* name, const char* theme )
		:  NCDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( name ).data(), bListCancel ),
		   lo( 10, 10 ),
		   ver( 0, this, utf8_to_unicode( verString ).data() ),
		   helpWin( theme, this,  0 )
	{
		lo.SetColGrowth( 0 );
		lo.SetColGrowth( 2 );

		ver.Show();
		ver.Enable();
		lo.AddWin( &ver, 0, 1 );
		helpWin.Show();
		helpWin.Enable();
		lo.LineSet( 1, 5 );
		lo.AddWin( &helpWin, 2, 0, 2, 2 );

		this->AddLayout( &lo );
		this->SetEnterCmd( CMD_OK );

		MaximizeIfChild();

		if ( !createDialogAsChild )
		{
			LSize ls = helpWin.GetLSize();

			if ( ls.x.minimal < 500 ) { ls.x.minimal = 500; }

			if ( ls.y.minimal < 300 ) { ls.y.minimal = 300; }

			helpWin.SetLSize( ls );
			SetPosition();
		}

		helpWin.SetFocus();
	}
	bool Ok() { return helpWin.Ok(); }
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual ~HelpDlg();
};

bool HelpDlg::Command( int id, int subId, Win* win, void* data )
{
	return NCDialog::Command( id, subId, win, data );
}

HelpDlg::~HelpDlg() {}


void Help( NCDialogParent* parent, const char* theme )
{
	HelpDlg dlg( parent, _LT( "Help" ), theme );

	if ( !dlg.Ok() ) { return; } //не нашел тему

	dlg.DoModal();
}
