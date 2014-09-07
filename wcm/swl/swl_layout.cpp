/*
   Copyright (c) by Valery Goryachev (Wal)
*/


#include "swl.h"

namespace wal
{


	void Layout::AddWin( Win* w, int r1, int c1, int r2, int c2 )
	{
		if ( r2 < 0 ) { r2 = r1; }

		if ( c2 < 0 ) { c2 = c1; }

		if ( r1 > r2 || c1 > c2 ) { return; }

		if ( r1 < 0 || c1 < 0 || r2 >= lines.count() || c2 >= columns.count() ) { return; }

		//if (w->layout) w->layout->DelObj(w);
		if ( w->upLayout ) { w->upLayout->DelObj( w ); }

		w->upLayout = this;
		objList.append( new LItemWin( w, r1, r2, c1, c2 ) );
	}

	void Layout::AddLayout( Layout* l, int r1, int c1, int r2, int c2 )
	{
		if ( r2 < 0 ) { r2 = r1; }

		if ( c2 < 0 ) { c2 = c1; }

		if ( r1 > r2 || c1 > c2 ) { return; }

		if ( r1 < 0 || c1 < 0 || r2 >= lines.count() || c2 >= columns.count() ) { return; }

//??? if (w->layout) w->layout->DelObj(w);
		objList.append( new LItemLayout( l, r1, r2, c1, c2 ) );
	}

	void Layout::AddRect( crect* rect, int r1, int c1, int r2, int c2 )
	{
		if ( !rect ) { return; }

		if ( r2 < 0 ) { r2 = r1; }

		if ( c2 < 0 ) { c2 = c1; }

		if ( r1 > r2 || c1 > c2 ) { return; }

		if ( r1 < 0 || c1 < 0 || r2 >= lines.count() || c2 >= columns.count() ) { return; }

		objList.append( new LItemRect( rect, r1, r2, c1, c2 ) );
	}


	Layout::Layout( int lineCount, int colCount )
		:
		lines( lineCount ),
		columns( colCount ),
		currentRect( 0, 0, 0, 0 ),
		valid( false )
	{
//	int i; //ошибка была в конструсторе ccollect(int n)
//	for (i=0; i<lineCount; i++) lines.append();
//	for (i=0; i<colCount; i++) columns.append();
	}

	Layout::~Layout() {}

	void Layout::GetLSize( LSize* pls )
	{
		/* !!!??
		   if (valid)
		   {
		      *pls = size;
		      return;
		   }
		*/
		Recalc();
		int i;
		LSize ls;

		for ( i = 0; i < lines.count(); i++ )
		{
			ls.y.Plus( lines[i].range );
		}

		for ( i = 0; i < columns.count(); i++ )
		{
			ls.x.Plus( columns[i].range );
		}

		size = ls;
		*pls = ls;
	}

	void Layout::SetPos( crect rect, wal::ccollect<WSS>& wList )
	{
//	if (currentRect == rect) return;
		currentRect = rect;
		valid = false;
		this->Recalc();

		for ( int i = 0; i < objList.count(); i++ )
		{
			int j;
			crect r( rect.left, rect.top, 0, 0 );

			for ( j = 0; j < objList[i]->r1; j++ ) { r.top += lines[j].size; }

			r.bottom = r.top;

			for ( ;  j <= objList[i]->r2; j++ ) { r.bottom += lines[j].size; }

			for ( j = 0; j < objList[i]->c1; j++ ) { r.left += columns[j].size; }

			r.right = r.left;

			for ( ; j <= objList[i]->c2; j++ ) { r.right += columns[j].size; }

			objList[i]->SetPos( r, wList );
		}
	}

	void Layout::DelObj( void* p )
	{
		for ( int i = 0; i < objList.count(); i++ )
			if ( objList[i]->ObjPtr() == p )
			{
				valid = false;
				objList[i] = 0; ///из-за недочета ccollect.del :(
				objList.del( i );
				return;
			}
	}


	void Layout::LineSet( int nLine, int _min, int _max, int _ideal )
	{
		if ( nLine < 0 || nLine >= lines.count() ) { return; }

		if ( _min >= 0 ) { lines[nLine].initRange.minimal = _min; }

		if ( _max >= 0 ) { lines[nLine].initRange.maximal = _max; }

		if ( _ideal >= 0 ) { lines[nLine].initRange.ideal = _ideal; }

		lines[nLine].initRange.Check();
	}

	void Layout::ColSet( int nCol, int _min, int _max, int _ideal )
	{
		if ( nCol < 0 || nCol >= columns.count() ) { return; }

		if ( _min >= 0 ) { columns[nCol].initRange.minimal = _min; }

		if ( _max >= 0 ) { columns[nCol].initRange.maximal = _max; }

		if ( _ideal >= 0 ) { columns[nCol].initRange.ideal = _ideal; }

		/*lines*/ columns[nCol].initRange.Check();
	}

	void Layout::SetLineGrowth( int n, bool enable )
	{
		if ( n >= 0 && n < lines.count() )
		{
			lines[n].growth = enable;
		}
	}

	void Layout::SetColGrowth( int n, bool enable )
	{
		if ( n >= 0 && n < columns.count() )
		{
			columns[n].growth = enable;
		}
	}

	static void SetMinRangeN( SpaceStruct* list, int count, int size )
	{
		int i, n, addon, rem, space;

		for ( i = 0; i < count; i++ ) //выставляем по минимуму
		{
			size -=  list[i].range.minimal;
		}

		if ( size <= 0 ) { return; }

		n = 0;

		for ( i = 0; i < count; i++ )
			if ( list[i].growth ) { n++; }

		if ( n > 0 )
		{
			addon = size / n;
			rem = size % n;

			for ( i = 0; i < count; i++ )
				if ( list[i].growth )
				{
					list[i].range.minimal += addon;
					size -= addon;

					if ( rem > 0 ) { list[i].range.minimal++; rem--; size--; }
				}

			return;
		}

		while ( true )
		{
			n = 0;

			for ( i = 0; i < count; i++ )
				if ( list[i].range.minimal < list[i].range.maximal ) { n++; }

			if ( !n ) { break; }

			addon = size / n;

			if ( addon == 0 ) { addon = 1; }

			for ( i = 0; i < count; i++ )
			{
				space = list[i].range.maximal - list[i].range.minimal;

				if ( space <= 0 ) { continue; }

				if ( space > addon ) { space = addon; }

				list[i].range.minimal += space;
				size -= space;

				if ( size <= 0 ) { return; }
			}

			if ( size <= 0 ) { return; }
		}

		addon = size / count;
		rem = size % count;

		for ( i = 0; i < count; i++ )
		{
			list[i].range.minimal += addon;
			size -= addon;

			if ( rem > 0 ) { list[i].range.minimal++; rem--; size--; }
		}
	}

	static void SetIdealRangeN( SpaceStruct* list, int count, int size )
	{
		int i, n, addon, rem, space;

		for ( i = 0; i < count; i++ )
		{
			size -=  list[i].range.ideal;
		}

		if ( size <= 0 ) { return; }

		n = 0;

		for ( i = 0; i < count; i++ )
			if ( list[i].growth ) { n++; }

		if ( n > 0 )
		{
			addon = size / n;
			rem = size % n;

			for ( i = 0; i < count; i++ )
				if ( list[i].growth )
				{
					list[i].range.ideal += addon;
					size -= addon;

					if ( rem > 0 ) { list[i].range.ideal++; rem--; size--; }
				}

			return;
		}

		while ( true )
		{
			n = 0;

			for ( i = 0; i < count; i++ )
				if ( list[i].range.ideal < list[i].range.maximal ) { n++; }

			if ( !n ) { break; }

			addon = size / n;

			if ( addon == 0 ) { addon = 1; }

			for ( i = 0; i < count; i++ )
			{
				space = list[i].range.maximal - list[i].range.ideal;

				if ( space <= 0 ) { continue; }

				if ( space > addon ) { space = addon; }

				list[i].range.ideal += space;
				size -= space;

				if ( size <= 0 ) { return; }
			}

			if ( size <= 0 ) { return; }
		}

		addon = size / count;
		rem = size % count;

		for ( i = 0; i < count; i++ )
		{
			list[i].range.ideal += addon;
			size -= addon;

			if ( rem > 0 ) { list[i].range.ideal++; rem--; size--; }
		}
	}


	static void SetMaxRangeN( SpaceStruct* list, int count, int size )
	{
		int i, n, addon, rem;

		for ( i = 0; i < count; i++ )
		{
			size -=  list[i].range.maximal;
		}

		if ( size <= 0 ) { return; }

		n = 0;

		for ( i = 0; i < count; i++ )
			if ( list[i].growth ) { n++; }

		if ( n > 0 )
		{
			addon = size / n;
			rem = size % n;

			for ( i = 0; i < count; i++ )
				if ( list[i].growth )
				{
					list[i].range.maximal += addon;
					size -= addon;

					if ( rem > 0 ) { list[i].range.maximal++; rem--; size--; }
				}

			return;
		}

		addon = size / count;
		rem = size % count;

		for ( i = 0; i < count; i++ )
		{
			list[i].range.maximal += addon;
			size -= addon;

			if ( rem > 0 ) { list[i].range.maximal++; rem--; size--; }
		}
	}



	static void SetOptimalRange( SpaceStruct* list, int count, int size )
	{
		int i, n, addon, rem;

		for ( i = 0; i < count; i++ ) //выставляем по минимуму
		{
			size -= ( list[i].size = list[i].range.minimal );
		}

		if ( size <= 0 ) { return; } //goto ret;

		n = 0;

		for ( i = 0; i < count; i++ )
			if ( list[i].growth ) { n++; }

		if ( n > 0 )
		{
			addon = size / n;
			rem = size % n;

			for ( i = 0; i < count; i++ )
				if ( list[i].growth )
				{
					list[i].size += addon;
					size -= addon;

					if ( rem > 0 ) { list[i].size++; rem--; size--; }
				}

			return;
		}

		while ( true )
		{
			n = 0;

			for ( i = 0; i < count; i++ )
				if ( list[i].size < list[i].range.maximal ) { n++; }

			if ( !n ) { break; }

			addon = size / n;

			//int rem = size % n;
			if ( !addon ) { addon = 1; }

			for ( i = 0; i < count; i++ )
			{
				int space = list[i].range.maximal - list[i].size;

				if ( space <= 0 ) { continue; }

				if ( space > addon ) { space = addon; }

				list[i].size += space;
				size -= space;

				if ( size <= 0 ) { break; }
			}

			if ( size <= 0 ) { break; }
		}

//ret:
//	size = 0;
//	for (i = 0; i<count; i++) size+=list[i].size;
//	return size;
	}


	void Layout::Recalc()
	{
//!?  if (valid) return;
		int i;

		for ( i = 0; i < lines.count(); i++ )
		{
			lines[i].Clear();
		}

		for ( i = 0; i < columns.count(); i++ )
		{
			columns[i].Clear();
		}


		wal::ccollect<LSize> lSize( objList.count() );

		for ( i = 0; i < objList.count(); i++ )
		{
			objList[i]->GetLSize( lSize.ptr() + i );
		}

		for ( i = 0; i < objList.count(); i++ )
		{
			LItem* p = objList[i].ptr();
			LSize* ls = lSize.ptr() + i;

			if ( p->r1 == p->r2 )
			{
				int r = p->r1;

				if ( lines[r].range.minimal < ls->y.minimal )
				{
					lines[r].range.minimal = ls->y.minimal;
				}

				if ( lines[r].range.maximal < ls->y.maximal )
				{
					lines[r].range.maximal = ls->y.maximal;
				}

				if ( lines[r].range.ideal < ls->y.ideal )
				{
					lines[r].range.ideal = ls->y.ideal;
				}
			}

			if ( p->c1 == p->c2 )
			{
				int c = p->c1;

				if ( columns[c].range.minimal < ls->x.minimal )
				{
					columns[c].range.minimal = ls->x.minimal;
				}

				if ( columns[c].range.maximal < ls->x.maximal )
				{
					columns[c].range.maximal = ls->x.maximal;
				}

				if ( columns[c].range.ideal < ls->x.ideal )
				{
					columns[c].range.ideal = ls->x.ideal;
				}
			}
		}


		for ( i = 0; i < objList.count(); i++ )
		{
			LItem* p = objList[i].ptr();
			LSize* ls = lSize.ptr() + i;

			if ( p->r1 != p->r2 )
			{
				SetMinRangeN( lines.ptr() + p->r1, p->r2 - p->r1 + 1, ls->y.minimal );
				SetIdealRangeN( lines.ptr() + p->r1, p->r2 - p->r1 + 1, ls->y.ideal );
				SetMaxRangeN( lines.ptr() + p->r1, p->r2 - p->r1 + 1, ls->y.maximal );
			}

			if ( p->c1 != p->c2 )
			{
				SetMinRangeN  ( columns.ptr() + p->c1, p->c2 - p->c1 + 1, ls->x.minimal );
				SetIdealRangeN( columns.ptr() + p->c1, p->c2 - p->c1 + 1, ls->x.ideal );
				SetMaxRangeN  ( columns.ptr() + p->c1, p->c2 - p->c1 + 1, ls->x.maximal );

				/*чо за херня тут была
				SetMinRangeN(columns.ptr()+p->r1, p->r2-p->r1+1,ls->x.minimal);
				SetIdealRangeN(columns.ptr()+p->r1, p->r2-p->r1+1,ls->x.ideal);
				SetMaxRangeN(columns.ptr()+p->r1, p->r2-p->r1+1,ls->x.maximal);
				*/
			}
		}

		for ( i = 0; i < lines.count(); i++ ) { lines[i].range.Check(); }

		for ( i = 0; i < columns.count(); i++ ) { columns[i].range.Check(); }

		SetOptimalRange( lines.ptr(), lines.count(), currentRect.Height() );
		SetOptimalRange( columns.ptr(), columns.count(), currentRect.Width() );

		valid = true;
	}

	LItem::~LItem() {}

	LItemWin::~LItemWin() { if ( w ) { w->upLayout = 0; } }
	LItemRect::~LItemRect() {}
	LItemLayout::~LItemLayout() {};

	void LItemLayout::GetLSize( LSize* ls )
	{
		l->GetLSize( ls );
	}

	void LItemLayout::SetPos( crect rect, wal::ccollect<WSS>& wList )
	{
		LSize ls;
		l->GetLSize( &ls );

		if ( rect.Width() > ls.x.maximal ) { rect.right = rect.left + ls.x.maximal; }

		if ( rect.Height() > ls.y.maximal ) { rect.bottom = rect.top + ls.y.maximal; }

		l->SetPos( rect, wList );
	}


	void LItemWin::GetLSize( LSize* ls )
	{
		if ( w->IsVisible() ) { w->GetLSize( ls ); }
		else
		{
			ls->Set( cpoint( 0, 0 ) );
		}
	}


	void LItemWin::SetPos( crect rect, wal::ccollect<WSS>& wList )
	{
		LSize ls;

		if ( !w->IsVisible() ) { return; }

		w->GetLSize( &ls );

		if ( rect.Width() > ls.x.maximal ) { rect.right = rect.left + ls.x.maximal; }

		if ( rect.Height() > ls.y.maximal ) { rect.bottom = rect.top + ls.y.maximal; }

		if ( w->Rect() != rect )
		{
			WSS wss;
			wss.rect = rect;
			wss.w = w;
			wList.append( wss );
		}
	}


	void* LItemLayout::ObjPtr() { return l; }
	void* LItemWin::ObjPtr() { return w; }


	void LItemRect::GetLSize( LSize* ls )
	{
		static LSize l( cpoint( 0, 0 ) );
		*ls = l;
	}

	void LItemRect::SetPos( crect _rect, wal::ccollect<WSS>& wList )
	{
		if ( rect ) { *rect = _rect; }
	}

	void* LItemRect::ObjPtr() { return rect; }



}; //namespace wal
