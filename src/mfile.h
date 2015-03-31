/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#pragma once

//for loading and saving editor's data

class MemFile: public iIntrusiveCounter
{
	enum BS { BLOCK_SIZE = 64 * 1024 };
	struct Node
	{
		char block[BLOCK_SIZE];
		Node* next;
		Node(): next( 0 ) {}
	};

	Node* first;
	Node* last;
	int endOffset;
	int dataSize;

	Node* readNode;
	int readOffset;
	int readSize;
public:
	MemFile() : first( 0 ), last( 0 ), endOffset( BLOCK_SIZE ), dataSize( 0 ), readNode( 0 ), readOffset( 0 ), readSize( 0 ) { }

	void Append( const char* s, int size )
	{
		while ( size > 0 )
		{
			int count = BLOCK_SIZE - endOffset;

			if ( count <= 0 )
			{
				Node* p = new Node;

				if ( !first ) { first = p; readNode = p; }

				if ( last ) { last->next = p; }

				last = p;
				endOffset = 0;
				count = BLOCK_SIZE;
			}

			if ( count > size ) { count = size; }

			memcpy( last->block + endOffset, s, count );
			s += count;
			endOffset += count;
			size -= count;
			dataSize += count;
		}
	}

	void Clear()
	{
		Node* p = first;

		while ( p )
		{
			Node* t = p;
			p = p->next;
			delete t;
		};

		last = 0;

		endOffset = BLOCK_SIZE;

		dataSize = 0;

		readNode = 0;

		readOffset = 0;

		readSize = 0;
	};

	void BeginRead() { readNode = first; readOffset = 0; readSize = 0; }

	int NonReadedSize() const { return dataSize - readSize; }

	int Read( char* s, int size )
	{
		int res = 0;

		while ( size > 0 )
		{
			if ( !readNode ) { return res; }

			int count = ( ( readNode == last ) ? endOffset : BLOCK_SIZE ) - readOffset;

			if ( count <= 0 )
			{
				readNode = readNode->next;
				readOffset = 0;
				continue;
			}

			if ( count > size ) { count = size; }

			memcpy( s, readNode->block + readOffset, count );
			s += count;
			readOffset += count;
			readSize += count;
			res += count;
			size -= count;
		}

		return res;
	}

	std::vector<char> ReadToChar( int ch, int* pCount, bool includeChar )
	{
		int res = 0;
		Node* p = readNode;
		int offset = readOffset;

		while ( p )
		{
			for ( char* s = p->block + offset; offset < BLOCK_SIZE; s++, res++, offset++ )
				if ( p == last && offset >= endOffset )
				{
					goto ttt;
				}
				else if ( *s == ch )
				{
					if ( includeChar ) { res++; }

					goto ttt;
				}

			p = p->next;
			offset = 0;
		}

ttt:
		*pCount = res;

		if ( res > 0 )
		{
			std::vector<char> a( res );
			int n = Read( a.data(), res );
			CHECK_RET( n == res, std::vector<char>() );
			return a;
		}

		return std::vector<char>();
	}

	~MemFile() { Clear(); }
};
