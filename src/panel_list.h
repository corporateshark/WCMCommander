/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#pragma once

#include "swl.h"
#include "vfs.h"
#include "vfs-uri.h"

using namespace wal;

enum LPanelSelectionType
{
	LPanelSelectionType_Disable = 0,
	LPanelSelectionType_Enable = 1,
	LPanelSelectionType_NotDefined = -1
};

struct PanelCounter
{
	int count;
	long long size;
	PanelCounter(): count( 0 ), size( 0 ) {}
	void Clear() { count = 0; size = 0; }
	void AddOne( long long _size ) { count++; size += _size;};
	void SubOne( long long _size ) { count--; size -= _size;};
};


class PanelList
{
public:

private:
	SORT_MODE sortMode;
	bool ascSort;

	clPtr<FSList> data;
	std::vector<FSNode*> list;
	int listCount;

	static unicode_t emptyStr[];
	static unicode_t upperStr[];

	PanelCounter filesCnNoDirs;
	PanelCounter filesCn;
	PanelCounter selectedCn;
	PanelCounter hiddenCn;

	bool showHidden;
	bool caseSensitive;

	void Sort();
	void MakeList();

public:

	PanelList( bool _showHidden, bool _case )
		:  sortMode( SORT_NAME ),
		   ascSort( true ),
		   listCount( 0 ),
		   showHidden( _showHidden ),
		   caseSensitive( _case )
	{
	}

	int   Count( bool RootDir ) const { return listCount + ( RootDir ? 0 : 1 ); }

	bool AscSort() const { return ascSort; }
	SORT_MODE SortMode() const { return sortMode; }

	PanelCounter FilesCounter( bool ShowDirs ) const { return ShowDirs ? filesCn : filesCnNoDirs; }
	PanelCounter SelectedCounter() const { return selectedCn; }
	PanelCounter HiddenCounter() const { return hiddenCn; }

	bool ShowHidden() const { return showHidden; }

	void Sort( SORT_MODE mode, bool asc ) { sortMode = mode; ascSort = asc; Sort(); }
	void DisableSort() { sortMode = SORT_NONE; MakeList(); }

	void SetData( clPtr<FSList> d )
	{
		try
		{
			data = d;
			MakeList();
			Sort();
		}
		catch ( ... )
		{
			data.clear();
			list.clear();
			throw;
		}
	}

	bool SetShowHidden( bool yes ) { if ( showHidden == yes ) { return false; } showHidden = yes; MakeList(); Sort(); return true; }
	bool SetCase( bool yes ) { if ( caseSensitive == yes ) { return false; } caseSensitive = yes; MakeList(); Sort(); return true; }


	FSNode* Get( int n, bool RootDir ) const //n is subtracted by 1
	{
		if ( RootDir )
		{
			return ( n >= 0 && n < listCount ) ? list[n] : 0;
		}

		return ( n > 0 && n <= listCount ) ? list[n - 1] : 0;
	}

	const unicode_t* GetFileName( int n, bool RootDir ) //n is subtracted by 1
	{
		if ( RootDir )
		{
			return n >= 0 && n < listCount ? list[n]->GetUnicodeName() : emptyStr;
		}

		return n > 0 && n <= listCount ? list[n - 1]->GetUnicodeName() : ( n == 0 ? upperStr : emptyStr );
	};

	int Find( FSString& str, bool RootDir )
	{
		int n = listCount;

		for ( int i = 0; i < n; i++ )
			if ( !str.Cmp( list[i]->Name() ) )
			{
				return i + ( RootDir ? 0 : 1 );
			}

		return -1;
	}

	int FindExactOrClosestSucceeding(FSNode& n, bool RootDir)
	{
		int i = FSNodeVectorSorter::BSearch(n, list, 
			 EXACT_OR_CLOSEST_SUCCEEDING_NODE,
			 ascSort, caseSensitive, sortMode);
		if (i > 0 && i >= (int)(list.size()))
		{
			i--;
		}
		return i + (RootDir ? 0 : 1);
	}

	clPtr<cstrhash<bool, unicode_t> > GetSelectedHash()
	{
		clPtr<cstrhash<bool, unicode_t> > hash = new cstrhash<bool, unicode_t>();
		int n = listCount;

		for ( int i = 0; i < n; i++ )
		{
			FSNode* p = list.data()[i];

			if ( p->IsSelected() )
			{
				hash->get( p->GetUnicodeName() ) = true;
			}
		}

		return hash;
	}

	void SetSelectedByHash( cstrhash<bool, unicode_t>* ph )
	{
		if ( !ph ) { return; }

		int n = listCount;
		selectedCn.Clear();

		for ( int i = 0; i < n; i++ )
		{
			FSNode* p = list.data()[i];

			if ( ph->exist( p->GetUnicodeName() ) )
			{
				p->SetSelected();
			}

			if ( p->IsSelected() ) { selectedCn.AddOne( p->st.size ); }
		}
	}

	void ClearSelection( cstrhash<bool, unicode_t>* resList )
	{
		int n = listCount;
		selectedCn.Clear();

		for ( int i = 0; i < n; i++ )
		{
			FSNode* p = list.data()[i];

			if ( resList->exist( p->GetUnicodeName() ) && p->IsSelected() )
			{
				p->ClearSelected();
			}

			if ( p->IsSelected() ) { selectedCn.AddOne( p->st.size ); }
		}
	}

	clPtr<FSList> GetSelectedList()
	{
		clPtr<FSList> plist = new FSList;

		if ( selectedCn.count <= 0 ) { return plist; }

		int n = listCount;

		for ( int i = 0; i < n; i++ )
		{
			FSNode* p = list.data()[i];

			if ( p && p->IsSelected() )
			{
				clPtr<FSNode> t = new FSNode( *p );
				t->originNode = p;
				plist->Append( t );
			}
		}

		return plist;
	}

	void InvertSelection( int n, bool RootDir ) //n is subtracted by 1
	{
		FSNode* p = NULL;

		if ( RootDir )
		{
			if ( n < 0 || n >= listCount ) { return; }

			p = list[n];
		}
		else
		{
			if ( n <= 0 || n > listCount ) { return; }

			p = list[n - 1];
		}

		if ( !p ) { return; }

		if ( p->IsSelected() )
		{
			p->ClearSelected();
			selectedCn.SubOne( p->Size() );
		}
		else
		{
			p->SetSelected();
			selectedCn.AddOne( p->Size() );
		}

	}

	void ShiftSelection( int n, LPanelSelectionType* selectType, bool RootDir );


	void InvertSelection();
	void Mark( const unicode_t* mask, bool enable );

	~PanelList() {};
};


