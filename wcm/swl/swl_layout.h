/*
   Copyright (c) by Valery Goryachev (Wal)
*/


#ifndef LAYOUT_H
#define LAYOUT_H

namespace wal
{

	struct LSRange
	{
		int minimal, maximal, ideal;

		LSRange(): minimal( 0 ), maximal( 0 ), ideal( 0 ) {}
		LSRange( int mi, int mx, int id ): minimal( mi ), maximal( mx ), ideal( id ) {}
		void Check() { if ( ideal < minimal ) { ideal = minimal; } if ( maximal < ideal ) { maximal = ideal; } }
		void Plus( const LSRange& a ) { minimal += a.minimal; maximal += a.maximal; ideal += a.ideal; }

		LSRange& Max( const LSRange& a )
		{
			if ( minimal < a.minimal ) { minimal = a.minimal; }

			if ( maximal < a.maximal ) { maximal = a.maximal; }

			if ( ideal < a.ideal ) { ideal = a.ideal; }

			return *this;
		}
	};

	struct LSize
	{
		LSRange x, y;

		LSize() {}
		LSize( const cpoint& p ): x( p.x, p.x, p.x ), y( p.y, p.y, p.y ) { }
		void Set( const crect& rect )
		{
			x.minimal = x.maximal = x.ideal = rect.Width();
			y.minimal = y.maximal = y.ideal = rect.Height();
		}
		void Set( const cpoint& point )
		{
			x.minimal = x.maximal = x.ideal = point.x;
			y.minimal = y.maximal = y.ideal = point.y;
		}
		LSize Max( const LSize& a ) { x.Max( a.x ); y.Max( a.y ); return *this; }
	};

	struct SpaceStruct
	{
		bool growth;
		LSRange initRange, range;
		int size;

		void Clear() { range = initRange; size = 0; }
		SpaceStruct(): growth( false ), size( 0 ) {};
	};

//структура для сбора списка окон, которым требуется изменение позиции или размера
//чтоб потом оптом все изменить
	struct WSS
	{
		Win* w;
		crect rect;
	};

	struct LItem: public iIntrusiveCounter
	{
		//надо включить выравнивание
		int r1, r2, c1, c2;
		LItem( int _r1, int _r2, int _c1, int _c2 ): r1( _r1 ), r2( _r2 ), c1( _c1 ), c2( _c2 ) {}
		virtual void GetLSize( LSize* ls ) = 0;
		virtual void* ObjPtr() = 0;
		virtual void SetPos( crect rect, wal::ccollect<WSS>& wList ) = 0;
		virtual ~LItem();
	};

	struct LItemWin: public LItem
	{
		Win* w;
		LItemWin( Win* _w, int _r1, int _r2, int _c1, int _c2 ): LItem( _r1, _r2, _c1, _c2 ), w( _w ) {}
		virtual void GetLSize( LSize* ls );
		virtual void SetPos( crect rect, wal::ccollect<WSS>& wList );
		virtual void* ObjPtr();
		virtual ~LItemWin();
	};

//GetWindowRect

	struct LItemLayout: public LItem
	{
		Layout* l;
		LItemLayout( Layout* _l, int _r1, int _r2, int _c1, int _c2 ): LItem( _r1, _r2, _c1, _c2 ), l( _l ) {}
		virtual void GetLSize( LSize* ls );
		virtual void SetPos( crect rect, wal::ccollect<WSS>& wList );
		virtual void* ObjPtr();
		virtual ~LItemLayout();
	};

	struct LItemRect: public LItem
	{
		crect* rect;
		LItemRect( crect* r, int _r1, int _r2, int _c1, int _c2 ): LItem( _r1, _r2, _c1, _c2 ), rect( r ) {}
		virtual void GetLSize( LSize* ls );
		virtual void SetPos( crect rect, wal::ccollect<WSS>& wList );
		virtual void* ObjPtr();
		virtual ~LItemRect();
	};


	class Layout
	{
		friend class Win;
		friend struct LItemLayout;
		wal::ccollect< clPtr<LItem> > objList;
		wal::ccollect< SpaceStruct > lines;
		wal::ccollect< SpaceStruct > columns;
		crect currentRect;
		LSize size;
		bool valid;

		int GLineCount( int r1, int r2 ) { int n = 0; for ( int i = r1; i <= r2; i++ ) if ( lines[i].growth ) { n++; } return n; }
		int GColCount( int c1, int c2 ) { int n = 0; for ( int i = c1; i <= c2; i++ ) if ( columns[i].growth ) { n++; } return n; }

		void Recalc();
	public:
		Layout( int lineCount, int colCount );
		void DelObj( void* p );
		void AddWin( Win* w, int r1, int c1, int r2 = -1, int c2 = -1 );
		void AddLayout( Layout* l, int r1, int c1, int r2 = -1, int c2 = -1 );
		void AddRect( crect* rect, int r1, int c1, int r2 = -1, int c2 = -1 );
		void GetLSize( LSize* ls );
		LSize GetLSize() { LSize ls; GetLSize( &ls ); return ls; }
		void SetPos( crect rect, wal::ccollect<WSS>& wList );
		void LineSet( int nLine, int _min = -1, int _max = -1, int _ideal = -1 );
		void ColSet( int nCol, int _min = -1, int _max = -1, int _ideal = -1 );
		void SetLineGrowth( int n, bool enable = true );
		void SetColGrowth( int n, bool enable = true );
		~Layout();
	};

}; //namespace wal

#endif
