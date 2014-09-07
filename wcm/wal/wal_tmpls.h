/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#ifndef WAL_TMPLS_H
#define WAL_TMPLS_H

//Wal templates 2.1
//Created by Wal (Krasnoyarsk)

#include <stdlib.h>
#include <stdio.h>

#ifdef _MFC_VER
#include <afx.h>
#else
#ifndef ASSERT
#ifdef _DEBUG
#include <assert.h>
#define ASSERT(a) assert(a)
#else
#define ASSERT(a)
#endif
#endif
#endif

#include <vector>
#include "IntrusivePtr.h"

namespace wal
{

	template <class T, size_t Step = 32> class ccollect: public iIntrusiveCounter
	{
	public:
		ccollect();
		explicit ccollect( size_t n );
		void del( int n );
		void del( int n, int count );
		void set( int n, const T& a );
		T& get( int n );
		const T& const_item( int n ) const;
		const T* const_ptr() const;
		T* ptr();
		const std::vector<T>& grab() const;
		T& operator [] ( int n );
		void insert( int n );
		void insert( int n, const T& a );
		void insert( int n, int count );
		void append();
		void append ( const T& a );
		void append_list ( const T* list, int number );
		void append_n ( const T& p, int number );
		void append ( const ccollect& a );
		void clear();
		int count() const;
	private:
		std::vector<T> m_data;
	};

/// ccollect //////////////////////////////////////////////////////////

	template <class T, size_t Step> ccollect<T, Step>::ccollect()
	 : m_data()
	{
		m_data.reserve( Step );
	}

	template <class T, size_t Step> ccollect<T, Step>::ccollect( size_t n )
	 : m_data( n )
	{
	}

	template <class T, size_t Step> void ccollect<T, Step>::del( int n )
	{
		ASSERT( n >= 0 && n < ( int )m_data.size( ) );

		m_data.erase( m_data.begin( ) + n );
	}

	template <class T, size_t Step> void ccollect<T, Step>::del( int n, int count )
	{
		ASSERT( n >= 0 );
		ASSERT( count >= 0 );
		ASSERT( n + count <= ( int )m_data.size( ) );

		if ( count <= 0 )
		{
			return;
		}

		m_data.erase( m_data.begin() + n, m_data.begin() + n + count );
	}

	template <class T, size_t Step> inline void ccollect<T, Step>::set( int n, const T& a )
	{
		ASSERT( n >= 0 && n < (int)m_data.size() );
		m_data[n] = a;
	}

	template <class T, size_t Step> inline T& ccollect<T, Step>::get( int n )
	{
		ASSERT( n >= 0 && n < ( int )m_data.size() );
		return m_data[n];
	}

	template <class T, size_t Step> inline T* ccollect<T, Step>::ptr()
	{
		return m_data.data();
	}

	template <class T, size_t Step> const std::vector<T>& ccollect<T, Step>::grab() const
	{
		return m_data;
	}

	template <class T, size_t Step> inline const T& ccollect<T, Step>::const_item( int n ) const
	{
		ASSERT( n >= 0 && n < ( int )m_data.size( ) );
		return m_data[ n ];
	}

	template <class T, size_t Step> inline const T* ccollect<T, Step>::const_ptr() const
	{
		return m_data.data();
	}

	template <class T, size_t Step> T& ccollect<T, Step>::operator [] ( int n )
	{
		ASSERT( n >= 0 && n < (int)m_data.size() );
		return m_data[ n ];
	}


	template <class T, size_t Step> inline void ccollect<T, Step>::append()
	{
		m_data.push_back( T() );
	}

	template <class T, size_t Step> inline void ccollect<T, Step>::append ( const T& a )
	{
		m_data.push_back( a );
	}


	template <class T, size_t Step> void ccollect<T, Step>::append_list ( const T* p, int number )
	{
		m_data.reserve( m_data.size( ) + number );

		for ( int i = 0; i < number; i++, p++ )
		{
			m_data.push_back( *p );
		}
	}

	template <class T, size_t Step> void ccollect<T, Step>::append_n( const T& p, int number )
	{
		m_data.insert( m_data.end(), number, p );
	}


	template <class T, size_t Step> void ccollect<T, Step>::append ( const ccollect& a )
	{
		for ( int i = 0; i < a.count(); i++ )
		{
			append( a.const_item( i ) );
		}
	}

	template <class T, size_t Step> inline void ccollect<T, Step>::insert( int n )
	{
		ASSERT( n >= 0 && n <= (int)m_data.size() );

		m_data.insert( m_data.begin()+n, T() );
	}

	template <class T, size_t Step> inline void ccollect<T, Step>::insert( int n, const T& a )
	{
		ASSERT( n >= 0 && n <= ( int )m_data.size() );

		m_data.insert( m_data.begin()+n, a );
	}

	template <class T, size_t Step> inline void ccollect<T, Step>::clear()
	{
		m_data.clear();
	}

	template <class T, size_t Step> inline int ccollect<T, Step>::count() const
	{
		return m_data.size();
	}

/// hash tables ////////////////////////////////////////////////////////////////////////////
	typedef int HashIndex;
	HashIndex hash_lover_table_size( HashIndex size );
	HashIndex hash_upper_table_size( HashIndex size );


#if defined(_MSC_VER) && defined(_DEBUG)
#define FloatTableParam F_TP
#endif

	template <HashIndex minsize = 7, HashIndex minlevel = 4, HashIndex maxlevel = 4> class FloatTableParam
	{
	public:
		static bool need_expand( HashIndex count, HashIndex tableSize )
		{
			return ( count > tableSize * maxlevel );
		}

		static bool need_shrink( HashIndex count, HashIndex tableSize )
		{
			return ( tableSize > count * minlevel ) && ( tableSize > minsize );
		}

		static HashIndex lover( HashIndex tableSize )
		{
			HashIndex n = hash_lover_table_size( tableSize );
			return n >= minsize ? n : minsize;
		}

		static HashIndex upper( HashIndex tableSize )
		{
			HashIndex n = hash_upper_table_size( tableSize );
			return n >= minsize ? n : minsize;
		}
	};


	template <HashIndex size> class StaticTableParam
	{
	public:
		static bool need_expand( HashIndex count, HashIndex tableSize ) { return false;  }
		static bool need_shrink( HashIndex count, HashIndex tableSize ) {   return false;   }
		static HashIndex lover( HashIndex tableSize ) {   return size; }
		static HashIndex upper( HashIndex tableSize )  {  return size; }
	};


	template <class LT> class hash_iterator
	{
		const HashIndex tableSize;
		HashIndex i;
		std::vector<LT* >& table;
		LT* ptr;
		hash_iterator() {}
	public:
		void next()
		{
			if ( ptr )
			{
				ptr = ptr->next;
			}

			while  ( !ptr && i < tableSize )
			{
				ptr = table[i++];
			}
		}

		hash_iterator( std::vector<LT* >& tab, HashIndex size )
			: tableSize( size ), table( tab )
		{
			if ( size <= 0 )
			{
				ptr = NULL;
				i = 0;
			}
			else
			{
				i = 1;
				ptr = table[0];

				if ( !ptr )
				{
					next();
				}
			}
		}

		LT* get()
		{
			ASSERT( valid() );
			return ptr;
		}

		bool valid() { return ptr != NULL; }

	};



	template <class IT, class T, class P = FloatTableParam<> > class cinthash;

	/*
	   LT
	   const KT& key()
	   unsigned intKey()
	   член LT *next
	   конструктор копирования

	   KT - тип ключа
	   должен иметь operator unsigned()
	   должен сравниваться на ==

	   IC - true
	   если сначала сравнивать intKey(), а затем key()
	   иначе только key()

	   (если KT целочисленный и равен intKey() то сравнивать 2 раза незачем
	   (спорный момент))
	*/


	template <class LT, class KT, bool IC = true, class P = FloatTableParam<> > class internal_hash
	{
	public:
		internal_hash();
		internal_hash( const internal_hash& a );

		internal_hash& operator = ( const internal_hash& a );

		LT* find( unsigned intKey, const KT& key );

		LT* append( unsigned intKey, LT* listNode );

		bool del( unsigned intKey, const KT& key, bool shrink = true );

		HashIndex count() const;
		void foreach( void f( LT*, void* ), void* data ); //nt
		void clear();
		hash_iterator<LT> first();

		void statistic( cinthash<int, int>* result );

		~internal_hash();
	private:
		void realloc( int newSize ); //nt
		void copy( const internal_hash& a );
		HashIndex tableSize;
		HashIndex itemCount;
		std::vector<LT*> table;
	};


	template <class LT, class KT, bool IC, class P>
	internal_hash<LT, KT, IC, P>::internal_hash()
	{
		tableSize = 0;
		itemCount = 0;
	}

	template <class LT, class KT, bool IC, class P>
	internal_hash<LT, KT, IC, P>::internal_hash( const internal_hash& a )
	{
		copy( a );
	}

	template <class LT, class KT, bool IC, class P>
	inline LT* internal_hash<LT, KT, IC, P>::find( unsigned intKey, const KT& key )
	{
		if ( tableSize > 0 )
		{
			for ( LT* p = table[intKey % tableSize]; p ; p = p->next )
			{
				if ( IC )
				{
					if ( intKey == p->intKey() &&
					     p->key() == key ) //порядок операндов в == важен
					{
						return p;
					}
				}
				else
				{
					if ( p->key() == key ) //порядок операндов в == важен
					{
						return p;
					}
				}
			}
		}

		return NULL;
	}

	template <class LT, class KT, bool IC, class P>
	inline LT* internal_hash<LT, KT, IC, P>::append( unsigned intKey, LT* listNode )
	{
		try
		{
			if ( tableSize <= 0 )
			{
				realloc( P::upper( 1 ) );
			}
		}
		catch ( ... )
		{
			delete listNode;
			throw;
		}

		unsigned n = intKey % tableSize;
		listNode->next = table[n];
		table[n] = listNode;

		itemCount++;

		if ( P::need_expand( itemCount, tableSize ) )
		{
			realloc( P::upper( tableSize ) );
		}

		return listNode;
	}


	template <class LT, class KT, bool IC, class P>
	bool internal_hash<LT, KT, IC, P>::del( unsigned intKey, const KT& key, bool shrink )
	{
		if ( tableSize <= 0 )
		{
			return false;
		}

		HashIndex n = intKey % tableSize;

		LT** p;

		for ( p = table.data() + n; *p; p = &( p[0]->next ) )

			//порядок операндов сравнений важен
			//чтобы определять только встроенный оператор == в KT
			if ( IC )
			{
				if ( p[0]->intKey() == intKey &&  p[0]->key() == key )
				{
					break;
				}
			}
			else
			{
				if ( p[0]->key() == key )
				{
					break;
				}
			}

		if ( *p )
		{
			LT* t = *p;
			*p = t->next;
			delete t;
			itemCount--;

			if ( shrink && P::need_shrink( itemCount, tableSize ) )
			{
				realloc( P::lover( tableSize ) );
			}

			return true;
		}

		return false;
	}

	template <class LT, class KT, bool IC, class P>
	inline void internal_hash<LT, KT, IC, P>::foreach( void f( LT*, void* ), void* data )
	{
		for ( HashIndex i = 0; i < tableSize; i++ )
		{
			for ( LT* p = table[i]; p; p = p->next )
			{
				f( p, data );
			}
		}
	}

	template <class LT, class KT, bool IC, class P>
	inline HashIndex internal_hash<LT, KT, IC, P>::count() const
	{
		return itemCount;
	}


	template <class LT> void destroy_hash_table( std::vector<LT*> table, HashIndex size )
	{
		for ( HashIndex i = 0; i < size; i++ )
		{
			for ( LT* p = table[i]; p; )
			{
				LT* t = p;
				p = p->next;
				delete t;
			}
		}
	}

	template <class LT, class KT, bool IC, class P>
	void internal_hash<LT, KT, IC, P>::clear()
	{
		destroy_hash_table<LT>( table, tableSize );
		tableSize = 0;
		itemCount = 0;
	}

	template <class LT, class KT, bool IC, class P>
	inline hash_iterator<LT> internal_hash<LT, KT, IC, P>::first()
	{
		return hash_iterator<LT>( table, tableSize );
	}

	template <class LT, class KT, bool IC, class P>
	internal_hash<LT, KT, IC, P>::~internal_hash()
	{
		clear();
	}


	template <class LT, class KT, bool IC, class P>
	void internal_hash<LT, KT, IC, P>::realloc( int newSize )
	{
		ASSERT( newSize >= 0 );

		if ( tableSize == newSize )
		{
			return;
		}

		if ( newSize == 0 )
		{
			ASSERT( itemCount == 0 );
			table.clear();
			tableSize = 0;
		}
		else
		{
			std::vector<LT*> newTable( newSize );
			HashIndex i;

			for ( i = 0; i < newSize; i++ )
			{
				newTable[i] = NULL;
			}

			for ( i = 0; i < tableSize; i++ )
			{
				for ( LT* p = table[i]; p; )
				{
					LT* t = p;
					p = p->next;
					HashIndex n = t->intKey() % newSize;
					t->next = newTable[n];
					newTable[n] = t;
				}
			}

			table = newTable;
			tableSize = newSize;
		}
	}

	template <class LT, class KT, bool IC, class P>
	void internal_hash<LT, KT, IC, P>::copy( const internal_hash& a )
	{
		std::vector< LT*> tmpTable( a.tableSize );

		try
		{
			HashIndex i;

			for ( i = 0; i < a.tableSize; i++ )
			{
				tmpTable[i] = NULL;
			}

			for ( i = 0; i < a.tableSize; i++ )
			{
				for ( const LT* p = a.table.data()[i];
				      p;
				      p = p->next )
				{
					LT* tmp = new LT( *p );
					tmp->next = tmpTable[i];
					tmpTable[i] = tmp;
				}
			}
		}
		catch ( ... )
		{
			destroy_hash_table<LT>( tmpTable, a.tableSize );
			throw;
		}

		if ( table.data() )
		{
			destroy_hash_table<LT>( table, tableSize );
		}

		table = tmpTable;
		tableSize = a.tableSize;
		itemCount = a.itemCount;
	}

	template <class LT, class KT, bool IC, class P>
	inline internal_hash<LT, KT, IC, P>& internal_hash<LT, KT, IC, P>::operator = ( const internal_hash& a )
	{
		copy( a );
		return *this;
	}

	/*
	   T - must have const KT& key() const
	   KT must have
	      operator unsigned() const ;
	      bool operator == (const KT &a) const; ///const function!!!!!!!
	*/

	template <class T, class KT, class P = FloatTableParam<> > class chash
	{

		struct  Node
		{
			unsigned hashKey;
			T m_data;

			Node( const T& d, unsigned k ) : hashKey( k ), m_data( d ) {}
			Node( const Node& a ): m_data( a.m_data ), hashKey( a.hashKey ) {}
			const KT& key() { return m_data.key(); }
			unsigned intKey() { return hashKey; }
			Node* next;
		private:
			Node() {};
		};

		internal_hash<Node, KT, true, P> hash;
	public:
		chash() {}
		chash( const chash& a ): hash( a.hash ) {}
		HashIndex count() const { return hash.count(); };

		T* get( const KT& k )
		{
			Node* p = hash.find( unsigned( k ), k );
			return p ? &( p->m_data ) : NULL;
		}

		chash& operator = ( const chash& a )
		{
			hash = a.hash;
			return *this;
		}

		T* put( const T& data )
		{
			unsigned intk = unsigned( data.key() );
			Node* p = hash.find( intk, data.key() );

			if ( !p )
			{
				p = new Node( data, intk );
				hash.append( intk, p );
			}

			return &p->m_data;
		}

		bool del( const KT& k, bool shrink = true )
		{
			return hash.del( unsigned( k ), k, shrink );
		}

		void foreach( void ( *f )( T*, void* ), void* parm )
		{
			for ( hash_iterator<Node> i = hash.first(); i.valid(); i.next() )
			{
				f( &( i.get()->m_data ), parm );
			}
		}

		std::vector<T*> get_all()
		{
			int n = hash.count();
			std::vector<T*> ret( n );
			int j = 0;

			for ( hash_iterator<Node> i = hash.first(); i.valid(); i.next(), j++ )
			{
				ASSERT( j < n );
				ret[j] =  &( i.get()->m_data );
			}

			return ret;
		}

		void statistic( cinthash<int, int>* result )
		{
			hash.statistic( result );
		}

		void clear() { hash.clear();}
		~chash() {}
	};


	/*
	таблица, где индексом является целое число (IT)
	IT - целое число, или тип, приводимый к unsigned (), копируется и сравнивается на ==	
	*/

	template < class IT, class T, class P/* = FloatTableParam<>*/ > class cinthash
	{
		struct Node
		{
			IT k;
			T m_data;
			Node( const T& d, const IT& n ): k( n ), m_data( d ) {}
			Node( const Node& a ): m_data( a.m_data ), k( a.k ) {}
			const IT& key() const { return k; }
			unsigned intKey() const { return ( unsigned )k; }

			Node* next;
		private:
			Node() {}
		};

		internal_hash<Node, IT, false, P> hash;
		T defaultValue;
	public:
		cinthash() {}
		cinthash( const T& def ): defaultValue( def ) { }

		cinthash( const cinthash& a )
			: hash( a.hash ),
			  defaultValue( a.defaultValue )
		{
		}

		void setdefault( const T& dv ) { defaultValue = dv; }
		HashIndex count() const { return hash.count(); };

		T* exist( const IT& k )
		{
			Node* p = hash.find( unsigned( k ), k );
			return p ? &( p->m_data ) : NULL;
		}

		T& get( const IT& k )
		{
			Node* p = hash.find( unsigned( k ), k );

			if ( !p )
			{
				p = new Node( defaultValue, k );
				hash.append( unsigned( k ), p );
			}

			return p->m_data;
		}

		T& operator []( const IT& k ) { return get( k ); }
		cinthash& operator = ( const cinthash& a )
		{
			if ( this != &a )
			{
				hash = a.hash;
				defaultValue = a.defaultValue;
			}

			return *this;
		}

		T* put( IT k, const T& data )
		{
			Node* p = hash.find( unsigned( k ), data.key() );

			if ( !p )
			{
				p = hash.append( unsigned( k ), Node( data, k ) );
			}
			else
			{
				p->m_data = data;
			}

			return &( p->m_data );
		}

		bool del( const IT& k, bool shrink = true )
		{
			return hash.del( unsigned( k ), k, shrink );
		}

		void foreach( void ( *f )( const IT& key, T*, void* ), void* parm )
		{
			for ( hash_iterator<Node> i = hash.first(); i.valid(); i.next() )
			{
				f( i.get()->k, &( i.get()->m_data ), parm );
			}
		}

		std::vector<IT> keys()
		{
			int n = hash.count();
			std::vector<IT> ret( n );
			int j = 0;

			for ( hash_iterator<Node> i = hash.first(); i.valid(); i.next(), j++ )
			{
				ASSERT( j < n );
				ret[j] = i.get()->intKey();
			}

			ASSERT( j == hash.count() );
			return ret;
		}

		void statistic( cinthash<int, int>* result )
		{
			hash.statistic( result );
		}

		void clear() { hash.clear();}
		~cinthash() {}
	};

	template <class LT, class KT, bool IC, class P>
	void internal_hash<LT, KT, IC, P>::statistic( cinthash<int, int>* result )
	{
		result->clear();
		result->setdefault( 0 );

		for ( HashIndex i = 0; i < tableSize; i++ )
		{
			LT* p = table[i];
			int j;

			for ( j = 0; p; p = p->next, j++ )
			{
				NULL;
			}

			result->get( j )++;
		}
	}

	template <class T> class chstring
	{

		static int size( const T* s )
		{
			int n = 0;

			for ( ; *s; s++ )
			{
				n++;
			}

			return n + 1;
		}

		static void cp( T* d, const T* s )
		{
			while ( (*( d++ ) = *( s++ )) );
		}

		static T* newstr( const T* s, int size )
		{
			T* p = new T[size];
			cp( p, s );
			return p;
		}

		int strSize;
		T* ptr;

		void operator = ( const chstring& ) {}

		static bool eq( const T* a, const T* b )
		{
			for ( ; *a /* && *b */ && *a == *b; a++, b++ )
			{
				NULL;
			}

			return *a == *b;
		}

	public:
		chstring() { ptr = NULL; }
		chstring( const T* s )
		{
			strSize = size( s );
			ptr = newstr( s, strSize );
		}

		chstring( const chstring& a )
		{
			ASSERT( a.ptr );
			ptr = newstr( a.ptr, a.strSize );
			strSize = a.strSize;
		}

		void operator = ( const T* s )
		{
			int n = size( s );
			T* p = newstr( s, n );

			if ( ptr )
			{
				delete [] ptr;
			}

			ptr = p;
			strSize = n;
		}

		const T* str() { ASSERT( ptr ); return ptr; }

		static unsigned key( const T* s )
		{
			unsigned n = 0;

			for ( ; *s; s++ )
			{
				n = ( n << 1 ) ^ unsigned( *s );
			}

			return n;
		}

		bool operator == ( const T* s ) const
		{
			return eq( ptr, s );
		}

		unsigned key()
		{
			ASSERT( ptr );
			return key( ptr );
		}

		~chstring()
		{
			if ( ptr )
			{
				delete [] ptr;
			}
		}
	};


	template <class T, class CT = char, class P = FloatTableParam<> > class cstrhash: public iIntrusiveCounter
	{
		struct Node
		{
			T m_data;
			unsigned hashKey;
			chstring<CT> string;

			Node( const T& d, const CT* s, unsigned h )
				: m_data( d ), string( s ), hashKey( h )
			{
			}

			const chstring<CT>& key() const { return string; }
			unsigned intKey() const { return hashKey; }
			Node* next;
		private:
			Node() {}
		};

		internal_hash<Node, const CT*, true, P> hash;
		T defaultValue;
	public:
		cstrhash() {};

		cstrhash( const T& def )
			: defaultValue( def )
		{
		}

		cstrhash( const cstrhash& a )
			: hash( a.hash ),
			  defaultValue( a.defaultValue )
		{
		}

		void setdefault( const T& def ) { defaultValue = def; }
		HashIndex count() const { return hash.count(); };

		T* exist( const CT* s )
		{
			Node* p = hash.find( chstring<CT>::key( s ), s );
			return p ? &( p->m_data ) : NULL;
		}

		T& get( const CT* s )
		{
			unsigned hashKey = chstring<CT>::key( s );
			Node* p = hash.find( hashKey, s );

			if ( !p )
			{
				p = new Node( defaultValue, s, hashKey );
				hash.append( hashKey, p );
			}

			return p->m_data;
		}

		T& operator []( const CT* s ) { return get( s ); }

		cstrhash& operator = ( const cstrhash& a )
		{
			if ( this != &a )
			{
				hash = a.hash;
				defaultValue = a.defaultValue;
			}

			return *this;
		}

		T& put( CT* s, const T& data )
		{
			unsigned hashKey = chstring<CT>::key( s );
			Node* p = hash.find( hashKey, s );

			if ( !p )
			{
				p = new Node( data, s, hashKey );
				hash.append( hashKey, p );
			}
			else
			{
				p->m_data = data;
			}

			return p->m_data;
		}

		bool del( const CT* s, bool shrink = true )
		{
			return hash.del( chstring<CT>::key( s ), s, shrink );
		}

		void foreach( void ( *f )( const CT* s, T*, void* ), void* parm )
		{
			for ( hash_iterator<Node> i = hash.first(); i.valid(); i.next() )
			{
				f( i.get()->string.str(), &( i.get()->m_data ), parm );
			}
		}

		std::vector<const CT*> keys()
		{
			int n = hash.count();
			std::vector<const CT*> ret( n );
			int j = 0;

			for ( hash_iterator<Node> i = hash.first(); i.valid(); i.next(), j++ )
			{
				ASSERT( j < n );
				ret[j] = i.get()->string.str();
			}

			ASSERT( j == hash.count() );
			return ret;
		}

		void statistic( cinthash<int, int>* result )
		{
			hash.statistic( result );
		}

		void clear() { hash.clear();}

		~cstrhash() {};
	};


// merge sort /////////////////////////////////////////////////////////////////

	template <class T> void sort2m( T* buffer, int count, bool lessf( T* a, T* b ) )
	{
		T* p0 = buffer, *nb = NULL, *p1 = NULL, *tmp;
		nb = new T[count];
		p1 = nb;

		try
		{
			int sl = 1;

			do
			{
				//int sl2 = sl*2;
				T* pp0 = p0, *pp1 = p1;

				for ( int n = count; n > 0; )
				{
					int lc = n > sl ? sl : n;
					n -= lc;
					int rc = n > sl ? sl : n;
					n -= rc;
					T* lp, *rp;

					for ( lp = pp0, rp = pp0 + lc; lc > 0 && rc > 0; pp1++ )
						if ( lessf( lp, rp ) )
						{
							*pp1 = *( lp++ );
							lc--;
						}
						else
						{
							*pp1 = *( rp++ );
							rc--;
						}

					for ( ; lc > 0; lc--, lp++, pp1++ ) { *pp1 = *lp; }

					for ( ; rc > 0; rc--, rp++, pp1++ ) { *pp1 = *rp; }

					pp0 = rp;
				}

				{
					tmp = p1;
					p1 = p0;
					p0 = tmp;
				}

				sl *= 2;
			}
			while ( sl < count );

			if ( p0 != buffer )
				for ( int i = 0; i < count; i++ ) { *( p1++ ) = *( p0++ ); }
		}
		catch ( ... )
		{
			if ( nb ) { delete [] nb; }

			throw;
		}

		delete [] nb;
	}

	template <class T, class DataT> void sort2m_data( T* buffer, int count, bool lessf( T* a, T* b, DataT* data ), DataT* data )
	{
		T* p0 = buffer, *nb = NULL, *p1 = NULL, *tmp;
		nb = new T[count];
		p1 = nb;

		try
		{
			int sl = 1;

			do
			{
				int sl2 = sl * 2;
				T* pp0 = p0, *pp1 = p1;

				for ( int n = count; n > 0; )
				{
					int lc = n > sl ? sl : n;
					n -= lc;
					int rc = n > sl ? sl : n;
					n -= rc;
					T* lp, *rp;

					for ( lp = pp0, rp = pp0 + lc; lc > 0 && rc > 0; pp1++ )
						if ( lessf( lp, rp, data ) )
						{
							*pp1 = *( lp++ );
							lc--;
						}
						else
						{
							*pp1 = *( rp++ );
							rc--;
						}

					for ( ; lc > 0; lc--, lp++, pp1++ ) { *pp1 = *lp; }

					for ( ; rc > 0; rc--, rp++, pp1++ ) { *pp1 = *rp; }

					pp0 = rp;
				}

				{
					tmp = p1;
					p1 = p0;
					p0 = tmp;
				}

				sl *= 2;
			}
			while ( sl < count );

			if ( p0 != buffer )
				for ( int i = 0; i < count; i++ ) { *( p1++ ) = *( p0++ ); }
		}
		catch ( ... )
		{
			if ( nb ) { delete [] nb; }

			throw;
		}

		delete [] nb;
	}


	// если T - сравнивается
	template <class T> void sort2m( T* buffer, int count, bool ascend = true )
	{
		T* p0 = buffer, *nb = NULL, *p1 = NULL, *tmp;
		nb = new T[count];
		p1 = nb;

		try
		{
			int sl = 1;

			do
			{
				int sl2 = sl * 2;
				T* pp0 = p0, *pp1 = p1;

				for ( int n = count; n > 0; )
				{
					int lc = n > sl ? sl : n;
					n -= lc;
					int rc = n > sl ? sl : n;
					n -= rc;
					T* lp, *rp;

					for ( lp = pp0, rp = pp0 + lc; lc > 0 && rc > 0; pp1++ )

						if ( ( ascend ) == ( *lp <= *rp ) )
						{
							*pp1 = *( lp++ );
							lc--;
						}
						else
						{
							*pp1 = *( rp++ );
							rc--;
						}

					for ( ; lc > 0; lc--, lp++, pp1++ ) { *pp1 = *lp; }

					for ( ; rc > 0; rc--, rp++, pp1++ ) { *pp1 = *rp; }

					pp0 = rp;
				}

				{
					tmp = p1;
					p1 = p0;
					p0 = tmp;
				}

				sl *= 2;
			}
			while ( sl < count );

			if ( p0 != buffer )
				for ( int i = 0; i < count; i++ ) { *( p1++ ) = *( p0++ ); }
		}
		catch ( ... )
		{
			if ( nb ) { delete [] nb; }

			throw;
		}

		delete [] nb;
	}

}; //namespace wal

//for use sort2m<char *>(strArray, size, strless< char* >);
//or sort2m<const char *>(strArray, size, strless< const char* >);

template <class T> inline bool strless( T* a, T* b )
{
	T s1 = *a, s2 = *b;

	for ( ; *s1 && *s1 == *s2; s1++, s2++ )
	{
		NULL;
	}

	return *s1 <= *s2;
}


template <class T> inline char* positive_to_char_decimal( T n, char* s )
{
	char c[32];
	int i = 0;

	for ( ; n > 0; i++, n /= 10 )
	{
		c[i] = char( n % 10 ) + '0';
	}

	for ( i--; i >= 0; i-- )
	{
		*( s++ ) = c[i];
	}

	return s;
}

template <class T>  char* unsigned_to_char( T n, char* s, char terminator = 0 )
{
	if ( n == 0 )
	{
		*( s++ ) = '0';
	}
	else
	{
		s = positive_to_char_decimal<T>( n, s );
	}

	*( s++ ) = terminator;
	return s;
}

template <class T> char* int_to_char( T n, char* s )
{
	if ( n < 0 )
	{
		*( s++ ) = '-';
		n = -n;
	}

	return unsigned_to_char<T>( n, s );
}

#endif
