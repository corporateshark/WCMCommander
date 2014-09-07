/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#include "ncview.h"
#include "wcm-config.h"
#include "color-style.h"
#include "nc.h"
#include "search-tools.h"
#include "charsetdlg.h"
#include "ltext.h"

#ifdef _WIN32
#include <time.h>
#endif


using namespace wal;

#define VIEWER_COLS (2048)
#define CACHE_BLOCK_SIZE (0x4000) //(0x8000)


/*
   Node must have
      Node *queueNext
      Node *queuePrev
*/


template <class Node> class CacheQueue
{
	Node* first, *last;
public:
	CacheQueue(): first( 0 ), last( 0 ) {}
	void Add( Node* p ) { p->queuePrev = 0;  p->queueNext = first; if ( first ) { first->queuePrev = p; } else { first = last = p; } }
	void Del( Node* p )
	{
		if ( p->queuePrev ) { p->queuePrev->queueNext = p->queueNext; }
		else { first = p->queueNext; }

		if ( p->queueNext ) { p->queueNext->queuePrev = p->queuePrev; }
		else { last = p->queuePrev; }
	};
	void OnTop( Node* p ) { if ( p->queuePrev ) { Del( p ); Add( p ); } }
	void Clear() { first = last = 0; }
	Node* First() { return first; }
	Node* Last() { return last; }
};

struct VFCNode
{
	static Mutex mutex;
	int useCount;
	unsigned char data[CACHE_BLOCK_SIZE];
	VFCNode(): useCount( 0 ) {}
};

wal::Mutex VFCNode::mutex;

class VDataPtr
{
	VFCNode* ptr;
	void Clear()
	{
		if ( ptr )
		{
			MutexLock lock( &VFCNode::mutex );
			ptr->useCount--;

			if ( ptr->useCount <= 0 )
			{
				lock.Unlock(); //!!!
				delete ptr;
			}

			ptr = 0;
		}
	};
public:
	VDataPtr(): ptr( 0 ) {}
	VDataPtr( VFCNode* p ): ptr( p ) { if ( p ) { ptr->useCount++; } }
	VDataPtr( const VDataPtr& a ): ptr( a.ptr ) { if ( ptr ) { MutexLock lock( &VFCNode::mutex ); ptr->useCount++; } }
	VDataPtr& operator = ( const VDataPtr& a ) { Clear(); ptr = a.ptr; if ( ptr ) { MutexLock lock( &VFCNode::mutex ); ptr->useCount++; } return *this; }
	const unsigned char* Data() { return ptr ? ptr->data : 0; }
	~VDataPtr() { Clear(); }
};


class VFile;

class ViewerString
{
	friend class VFile;
public:
	struct Node
	{
		unicode_t ch;
		int p;
	};
private:
	seek_t begin;
	int bytes;

	int len;
	int dataSize;
	std::vector<Node> data;
public:
	ViewerString(): begin( 0 ), bytes( 0 ), len( 0 ), dataSize( VIEWER_COLS ), data( VIEWER_COLS ) {}
	void Clear() { len = 0; begin = 0; bytes = 0; }
	bool Add( unicode_t c, int p ) { if ( len >= dataSize ) { return false; } data[len].ch = c; data[len].p = p; len++; return true; }
	int Len() const { return len; }
	int Size() const { return dataSize; }
	seek_t Begin() const { return begin; }
	int Bytes() const { return bytes; }
	seek_t NextOffset() const { return begin + bytes; }
	unicode_t Last() { return len > 0 ? data[len - 1].ch : 0; }
	void Pop() { if ( len > 0 ) { len--; } }
	bool Full() const { return len >= dataSize; }
	Node* Data() { return data.data(); }
	~ViewerString() {}
};



class VFilePtr;

class VFile
{
	friend class VFilePtr;
	Mutex mutex;
	int useCount;

	clPtr<FS> fs;
	FSPath path;
	int fd;

	enum { TABLESIZE = 331, MAXCOUNT = 128 };

	int blockCount;

	seek_t _offset;
	seek_t _size;

	int _tabSize;

	struct Node
	{
		long bn;
		VDataPtr data;
		Node* next;

		Node* queueNext;
		Node* queuePrev;
	};

	Node* htable[TABLESIZE];
	CacheQueue<Node> queue;

	void CacheNormalize();
	VDataPtr CacheGet( long bn );
	VDataPtr CacheSet( long bn, VDataPtr data );
	void CacheClear();
	VDataPtr _Get( long bn, FSCInfo* info, bool lockMutex );

	void CheckOpen( FSCInfo* info );

	time_t _lastMTime;
public:
	VFile();
	VFile( clPtr<FS> _fs, FSPath _path, seek_t _size, int tabSize );
	VDataPtr Get( long bn, FSCInfo* info ) { return _Get( bn, info, true ); }
	bool CheckStat( FSCInfo* info );
	seek_t Size() const { return _size; }
	seek_t Align( seek_t offset, charset_struct* charset, FSCInfo* info );
	seek_t GetPrevLine( seek_t pos, int* pCols, charset_struct* charset, bool* nlFound,  FSCInfo* info );
	int ReadBlock( seek_t pos,  char* s, int size, FSCInfo* info );
	bool ReadString( seek_t pos, ViewerString& str, charset_struct* charset, FSCInfo* info );
	FSString Uri() { return fs.IsNull() ? FSString() : fs->Uri( path ); }
	~VFile();
};


VFile::VFile()
	: useCount( 0 ), fd( -1 ), blockCount( 0 ), _offset( 0 ), _size( 0 ), _tabSize( 8 ),
	  _lastMTime( 0 )
{
	int i;

	for ( i = 0; i < TABLESIZE; i++ ) { htable[i] = 0; }
}


VFile::VFile( clPtr<FS> _fs, FSPath _path, seek_t size, int tabSize )
	:  useCount( 0 ),
	   fs( _fs ),
	   fd( -1 ),
	   path( _path ),
	   blockCount( 0 ), _offset( 0 ), _size( size ),
	   _tabSize( tabSize ),
	   _lastMTime( 0 )
{
	int i;

	for ( i = 0; i < TABLESIZE; i++ ) { htable[i] = 0; }
}

bool VFile::CheckStat( FSCInfo* info )
{
	CheckOpen( info );
	FSStat st;
	int err = 0;

	if ( fs->FStat( fd, &st, &err, info ) )
	{
		throw_msg( "can`t stat file '%s' :%s", fs->Uri( path ).GetUtf8(), fs->StrError( err ).GetUtf8() );
	}

	time_t t = ( time_t ) st.mtime;

	if ( st.size != _size  || t != _lastMTime )
	{
		_size = st.size;
		_lastMTime = t;
		CacheClear();
//printf("V file changed\n");
		return true;
	}

	return false;
}

seek_t VFile::Align( seek_t x, charset_struct* charset, FSCInfo* info )
{
	if ( x <= 0 ) { return 0; }

	if ( x >= _size ) { return _size; }

	int a = 16;
	int b = 16;

	if ( a > x ) { a = x; }

	if ( x + b > _size )
	{
		b = _size - x;
	}

	char buf[64];
	int n = a + b;

	if ( n <= 0 ) { return x; }

	int bytes = this->ReadBlock( x - a, buf, n, info );

	if ( bytes <= n ) { return x; }

	char* s = buf + a;

	if ( *s >= 0 && *s <= ' ' || s[-1] >= 0 && s[-1] <= ' ' ) { return x; }

	s = charset->GetPrev( s + 1, buf );
	return s ? ( x - a ) + ( s - buf ) : x;
}

void VFile::CacheClear()
{

	queue.Clear();

	int i;

	for ( i = 0; i < TABLESIZE; i++ )
	{
		Node* p = htable[i];

		while ( p )
		{
			Node* t = p;
			p = p->next;
			delete t;
		}

		htable[i] = 0;
	}
}



VFile::~VFile()
{
	CacheClear();

	if ( fd >= 0 )
	{
		fs->Close( fd, 0, 0 );
	}
}


VDataPtr VFile::CacheGet( long bn )
{

	{
		Node* p = queue.First();

		if ( p && p->bn == bn )
		{
			return p->data;
		}
	}


	int n = bn % TABLESIZE;

	for ( Node* p = htable[n]; p; p = p->next )
		if ( p->bn == bn )
		{
			queue.OnTop( p );
			return p->data;
		}

	return 0;
}

void VFile::CacheNormalize()
{
	while ( blockCount >= MAXCOUNT )
	{
		Node* p = queue.Last();
		ASSERT( p );

		queue.Del( p );

		int k = p->bn % TABLESIZE;

		Node** t = &( htable[k] );

		for ( ; *t; t = &( t[0]->next ) )
			if ( t[0] == p )
			{
				t[0] = t[0]->next;
				delete p;
				t = 0;
				break;
			}

		ASSERT( t == 0 );
		blockCount --;
	}
}

VDataPtr VFile::CacheSet( long bn, VDataPtr data )
{
	int n = bn % TABLESIZE;
	Node* p;

	for ( p = htable[n]; p; p = p->next )
		if ( p->bn == bn )
		{
			p->data = data;
			queue.OnTop( p );
			return data;
		}

	CacheNormalize();

	p = new Node;
	p->bn = bn;
	p->data = data;
	p->next = htable[n];
	htable[n] = p;

	queue.Add( p );
	return data;
}

void VFile::CheckOpen( FSCInfo* info )
{
	if ( fd >= 0 ) { return; }

	if ( !fs.Ptr() )
	{
		throw_msg( "BOTVA: VFile fs not defined" );
	}

	int err;
	int ret = fs->OpenRead( path, FS::SHARE_READ | FS::SHARE_WRITE, &err, info );

//const char *ss = path.GetUtf8();

	if ( ret == -2 )
	{
		throw_stop();
	}

	if ( ret < 0 )
	{
		throw_msg( "can`t open file '%s' :%s", fs->Uri( path ).GetUtf8(), fs->StrError( err ).GetUtf8() );
	}

	fd = ret;

	if ( info && info->Stopped() ) { throw_stop(); }
}

VDataPtr VFile::_Get( long bn, FSCInfo* info, bool lockMutex )
{
	MutexLock lock( &mutex, lockMutex );

	VDataPtr ptr = CacheGet( bn );

	if ( ptr.Data() ) { return ptr; }

	CheckOpen( info );

	int err;

	seek_t pos = seek_t( bn ) * CACHE_BLOCK_SIZE;

	if ( pos != _offset )
	{
		int ret = fs->Seek( fd, FSEEK_BEGIN, pos, 0, &err, info );

		if ( ret )
		{
			if ( ret == -2 )
			{
				throw_stop();
			}
			else
			{
				throw_msg( "cant set file position (%s)", fs->StrError( err ).GetUtf8() );
			}
		}

		_offset = pos;

		if ( info && info->Stopped() ) { throw_stop(); }
	}

	ptr = new VFCNode;

	const unsigned char* s = ptr.Data();
	ASSERT( s );
	memset( ( unsigned char* )s, 0, CACHE_BLOCK_SIZE );

	int size = CACHE_BLOCK_SIZE;

	//fs может читать клочками, а не сразу весь блок, поэтому цикл
	while ( size > 0 )
	{
		int bytes = fs->Read( fd, ( unsigned char* )s, size, &err, info );

		if ( !bytes ) { break; }

		if ( bytes == -2 )
		{
			throw_stop();
		}

		if ( bytes < 0 )
		{
			throw_msg( "cant read file (%s)", fs->StrError( err ).GetUtf8() );
		}

		s += bytes;
		size -= bytes;
		_offset += bytes;
	}


	CacheSet( bn, ptr );

	if ( info && info->Stopped() ) { throw_stop(); }

	return ptr;
}

static const unsigned char* StrLastNL( const unsigned char* ptr, int n )
{
	if ( !ptr ) { return 0; }

	const unsigned char* last = 0;

	for ( const unsigned char* s = ptr, *end = ptr + n; s < end; s++ )
		if ( *s == '\n' ) { last = s; }

	return last;
}

static const unsigned char* StrFirstNL( const unsigned char* ptr, int n )
{
	if ( !ptr ) { return 0; }

	for ( const unsigned char* s = ptr, *end = ptr + n; s < end; s++ )
		if ( *s == '\n' ) { return s; }

	return 0;
}


seek_t VFile::GetPrevLine( seek_t filePos,  int* pCols, charset_struct* charset, bool* nlFound, FSCInfo* info )
{
	char tabChar = charset->tabChar;

	if ( filePos <= 0 ) { return filePos; }

	if ( nlFound ) { *nlFound = false; }

	char buf[0x100];
	int n = filePos > sizeof( buf ) ? sizeof( buf ) : filePos;
	filePos -= n;
	int count = ReadBlock( filePos, buf, n, info );

	if ( count != n )
	{
		if ( nlFound ) { *nlFound = true; }

		return 0;
	}

	char* s = buf + count;

	if ( s[-1] == '\n' )
	{
		if ( s - 1 <= buf )
		{
			return filePos;
		}

		s--;

		if ( s[-1] == '\r' )
		{
			if ( s - 1 <= buf )
			{
				return filePos;
			}

			s--;
		}
	}

	int tabSize = _tabSize;
	int freeCols = VIEWER_COLS;
	int tab = 0;

	while ( freeCols > 0 )
	{
		if ( s < buf + 16 && filePos > 0 )
		{
			int m = s - buf;
			int n  = sizeof( buf ) - m;

			if ( n > filePos ) { n = filePos; }

			if ( m > 0 ) { memmove( buf + n, buf, m ); }

			filePos -= n;
			int count = ReadBlock( filePos, buf, n, info );

			if ( count != n )
			{
				return 0;
			}

			s += n;
		}

		char* t = charset->GetPrev( s, buf );

		if ( !t )
		{
			return filePos;
		}

		if ( *t == '\n' )
		{
			if ( nlFound ) { *nlFound = true; }

			break;
		}

		if ( *t == tabChar/*'\t'*/ )
		{
			if ( freeCols < tabSize )
			{
				break;
			}

			tab = tabSize;
			freeCols -= tabSize;
		}
		else
		{
			if ( tab > 0 )
			{
				if ( tab == 1 )
				{
					if ( freeCols < tabSize )
					{
						break;
					}

					tab = tabSize;
					freeCols -= tabSize;
				}
				else
				{
					tab--;
				}
			}
			else
			{
				freeCols--;
			}
		}

		s = t;
	}

	if ( pCols ) { *pCols = VIEWER_COLS - freeCols; }

	return filePos + ( s - buf );
}

int VFile::ReadBlock( seek_t offset, char* s, int count, FSCInfo* info )
{
	int bn = offset / CACHE_BLOCK_SIZE;
	int pos = offset % CACHE_BLOCK_SIZE;
	VDataPtr ptr = Get( bn, info );

	if ( offset > _size ) { return 0; }

	if ( count + offset > _size ) { count = _size - offset; }

	int ret = count;


	while ( count > 0 )
	{
		int n = CACHE_BLOCK_SIZE - pos;

		if ( n > count )
		{
			n = count;
		}

		if ( n > 0 )
		{
			memcpy( s, ptr.Data() + pos, n );
			count -= n;
			s += n;

			if ( count <= 0 ) { break; }
		}

		bn++;
		pos = 0;
		ptr = Get( bn, info );
	}

	return ret;
}

bool VFile::ReadString( seek_t offset, ViewerString& str, charset_struct* charset, FSCInfo* info )
{
	int tabSize = _tabSize;

	str.Clear();
	str.begin = offset;

	if ( offset >= Size() )
	{
		return false;
	}

	char buf[0x100];
	int pos = 0;

	int count = ReadBlock( offset, buf, sizeof( buf ), info );
	offset += count;
	int lbPos = 0; //начало буфера от начала строки в байтах

	char tabChar = charset->tabChar;

	while ( true )
	{
		if ( count - pos < 32 )
		{
			if ( pos < count && pos > 0 )
			{
				memmove( buf, buf + pos, count - pos );
				lbPos += pos;
				count -= pos;
				pos = 0;
			}

			int n = ReadBlock( offset, buf + count, sizeof( buf ) - count, info );

			offset += n;
			count += n;
		}

		if ( pos >= count ) { break; }

		if ( buf[pos] == '\n' )
		{
			if ( str.Last() == '\r' ) { str.Pop(); }

			pos++;
			break;
		}

		if ( str.Full() ) { break; }


		int cp = lbPos + pos;

		if ( buf[pos] == tabChar /*'\t'*/ )
		{
			pos++;

			for ( int n = tabSize - ( str.Len() % tabSize ); n > 0; n-- )
				if ( !str.Add( ' ', cp ) )
				{
					goto end;
				}

			continue;
		}

		//bool tab;
		unicode_t c = charset->GetChar( buf + pos, buf + count );
		char* s = charset->GetNext( buf + pos, buf + count );

		pos = s ? s - buf : count;
		str.Add( c, cp );

	}

	str.bytes = lbPos + pos;
end:


	return true;
}




class VFilePtr
{
	VFile* ptr;

	void Clear()
	{
		if ( ptr )
		{
			MutexLock lock( &ptr->mutex );
			ptr->useCount--;

			if ( ptr->useCount <= 0 )
			{
				lock.Unlock(); //!!!
				delete ptr;
			}

			ptr = 0;
		}
	};
public:
	VFilePtr(): ptr( 0 ) {}
	VFilePtr( VFile* p ): ptr( p )
	{
		if ( ptr )
		{
			ptr->useCount++;
		}
	}
	VFilePtr( const VFilePtr& a )
		: ptr( a.ptr )
	{
		if ( ptr )
		{
			MutexLock lock( &ptr->mutex );
			ptr->useCount++;
		}
	}

	VFilePtr& operator = ( const VFilePtr& a )
	{
		Clear();
		ptr = a.ptr;

		if ( ptr )
		{
			MutexLock lock( &ptr->mutex );
			ptr->useCount++;
		}

		return *this;
	}

	VFile* Ptr() { return ptr; }
	VFile* operator ->() { return ptr; }
	~VFilePtr() { Clear(); }
};



class FSCViewerInfo: public FSCInfo
{
public:
	Mutex mutex;
	bool stopped;
	FSCViewerInfo(): stopped( false ) {}
	void Reset() { MutexLock lock( &mutex ); stopped = false; }
	void SetStop() { MutexLock lock( &mutex ); stopped = true; }
	virtual bool Stopped();
	virtual ~FSCViewerInfo();
};

bool FSCViewerInfo::Stopped()
{
	MutexLock lock( &mutex );
	return stopped;
}

FSCViewerInfo::~FSCViewerInfo() {}



struct ViewerEvent
{
	enum
	{
	   NO = 0,
	   //SET, //set offset, trach must be in first line
	   VTRACK, HTRACK,
	   UP, DOWN, LEFT, RIGHT,
	   PAGEUP, PAGEDOWN, PAGELEFT, PAGERIGHT,
	   HOME, END,
	   LEFTSTEP, RIGHTSTEP,
	   FOUND
	};

	int type;
	int64 track;
	int64 e;

	ViewerEvent(): type( 0 ), track( 0 ), e( 0 ) {}
	ViewerEvent( int t ): type( t ), track( 0 ), e( 0 ) {}
	ViewerEvent( int t, int64 tr ): type( t ), track( tr ), e( 0 ) {}
	ViewerEvent( int t, int64 tr, int64 n1 ): type( t ), track( tr ), e( n1 ) {}
	void Clear() { type = NO; }
};


class ViewerThreadData
{
	static int NewTid();
	int tid;
	VFilePtr file;
public:
	seek_t initOffset;

	enum FLAGS { FSTOP = 1, FMODE = 2, FSIZE = 4, FEVENT = 8, FTIMER = 0x10 };

	FSCViewerInfo info;

	Mutex mutex;
	Cond cond;

	int inFlags;
	ViewerEvent inEvent;
	ViewerMode inMode;
	ViewerSize inSize;

	void SetEvent( const ViewerEvent& e );

	VSData ret;
	VFPos pos;
	std::vector<char> error;
	int64 loadStartTime;

	int Id() const {return tid; }

	ViewerThreadData( VFilePtr f ): tid( NewTid() ), file( f ), initOffset( 0 ), inFlags( 0 ), loadStartTime( 0 ) {}

	VFile* File() { return file.Ptr(); }
	VFilePtr FilePtr() { return file; }

};

int ViewerThreadData::NewTid()
{
	static int lastId = 0;
	lastId = ( lastId + 1 ) % 0x10000;
	return lastId + 1;
}

void ViewerThreadData::SetEvent( const ViewerEvent& e )
{
	MutexLock lock( &mutex );
	inEvent = e;
	inFlags |= FEVENT;
	cond.Signal();
}

inline int VStrWrapCount( int lineCols, int cols )
{
	if ( lineCols <= cols ) { return 1; }

	if ( lineCols % cols )
	{
		return lineCols / cols + 1;
	}

	return lineCols / cols;
}


void* ViewerThread( void* param )
{
	static const int HSTEP = 20;
	static const int tabSize = 8;

	ASSERT( param );
	ViewerThreadData* tData = ( ViewerThreadData* ) param;

	VSData ret;
	VFPos pos;
	VFile* file = tData->File();
	ViewerString str;
	VMarker marker;

	bool toEndOnChange = false;

	while ( true )
	{
		try
		{
			int flags;
			ViewerMode mode;
			ViewerSize size;
			ViewerEvent event;

			{
				//lock
				MutexLock lock( &tData->mutex );

				if ( !tData->inFlags )
				{
					tData->cond.Wait( &tData->mutex );
				}

				flags = tData->inFlags;

				if ( ( flags & ViewerThreadData::FSTOP ) != 0 ) { break; }

				mode = tData->inMode;
				size = tData->inSize;
				event = tData->inEvent;
				tData->inFlags = 0;
			};

			charset_struct* charset =  mode.charset;

			ret.SetSize( size );
			ret.mode = mode;

			if ( flags == ViewerThreadData::FTIMER )
			{
				if ( !file->CheckStat( &tData->info ) )
				{
					continue;
				}

				if ( !( flags & ViewerThreadData::FEVENT ) && toEndOnChange )
				{
					flags |= ViewerThreadData::FEVENT;
					event.type = ViewerEvent::END;
				}

			}
			else
			{
				MutexLock lock( &tData->mutex );
				tData->loadStartTime = time( 0 );

				file->CheckStat( &tData->info );
			}

			if ( ( flags & ViewerThreadData::FSIZE ) || ( flags & ViewerThreadData::FMODE ) )
			{
				pos.col = 0;
				pos.begin = file->Align( pos.begin, charset, &tData->info );
				marker.Clear();
				toEndOnChange = false;
			}

			if ( flags & ViewerThreadData::FEVENT )
			{
				toEndOnChange = false;

				switch ( event.type )
				{
					case ViewerEvent::FOUND:
						marker.Set( event.track, event.e );
						break;

					case ViewerEvent::END:
						toEndOnChange = true;

						//no break
					case ViewerEvent::VTRACK:
					case ViewerEvent::UP:
					case ViewerEvent::DOWN:
					case ViewerEvent::PAGEUP:
					case ViewerEvent::PAGEDOWN:
					case ViewerEvent::HOME:
						marker.Clear();
						break;
				}


				if ( mode.hex )
				{
					seek_t fSize = file->Size();
					int bytes = size.cols * size.rows;

					switch ( event.type )
					{
						case ViewerEvent::HOME:
							pos.begin = 0;
							break;

						case ViewerEvent::END:
							if ( fSize >= bytes ) { pos.begin = fSize - bytes; }

							break;

						case ViewerEvent::DOWN:
							if ( pos.begin + bytes < fSize ) { pos.begin += size.cols; }

							break;

						case ViewerEvent::UP:
							pos.begin  = pos.begin > size.cols ? pos.begin - size.cols : 0;
							break;

						case ViewerEvent::LEFTSTEP:
							if ( pos.begin > 0 ) { pos.begin--; }

							break;

						case ViewerEvent::RIGHTSTEP:
							if ( pos.begin + 1 < fSize ) { pos.begin++; }

							break;

						case ViewerEvent::FOUND:
						case ViewerEvent::VTRACK:
							pos.begin = event.track;

							if ( pos.begin < 0 ) { pos.begin = 0; }

							break;

						case ViewerEvent::PAGEUP:
						{
							int n = ( size.rows > 1 ? size.rows - 1 : 1 ) * size.cols;
							pos.begin = pos.begin > n ? pos.begin - n : 0;
						}
						break;

						case ViewerEvent::PAGEDOWN:
						{
							int n = ( size.rows > 1 ? size.rows - 1 : 1 ) * size.cols;

							if ( pos.begin + bytes < fSize )
							{
								pos.begin += n;
							}
						}
						break;
					};
				}
				else if ( mode.wrap )
				{
					seek_t p;

					switch ( event.type )
					{
						case ViewerEvent::HOME:
							pos.begin = 0;
							pos.col = 0;
							break;

						case ViewerEvent::END:
						{
							p = file->Size();

							int col = 0;
							int r = size.rows;

							while ( r > 0 )
							{
								int lineCols = 0;
								p = file->GetPrevLine( p, &lineCols, charset, 0, &tData->info );

								if ( !p ) { break; }

								int n = VStrWrapCount( lineCols, size.cols );

								if ( n > r )
								{
									col = ( n - r ) * size.cols;
									break;
								}
								else
								{
									r -= n;
								}
							}

							pos.begin = p;
							pos.col = col;
						}
						break;

						case ViewerEvent::DOWN:
							if ( file->ReadString( pos.begin, str, charset, &tData->info ) )
							{
								if ( str.Len() - pos.col < size.cols )
								{
									p = str.Begin() + str.Bytes();

									if ( p && p < file->Size() )
									{
										pos.begin = p;
										pos.col = 0;
									}
								}
								else
								{
									pos.col += size.cols;
								}
							}

							break;

						case ViewerEvent::UP:
							if ( pos.col > 0 )
							{
								if ( pos.col >= size.cols )
								{
									pos.col -= size.cols;
								}
								else
								{
									pos.col = 0;
								}
							}
							else
							{
								if ( pos.begin > 0 )
								{
									int lineCols = 0;
									p = file->GetPrevLine( pos.begin, &lineCols, charset, 0, &tData->info );
									pos.begin = p;
									pos.col = ( VStrWrapCount( lineCols, size.cols ) - 1 ) * size.cols;
								}
							}

							break;

						case ViewerEvent::VTRACK:
						{
							int n = event.track;

							if ( n < 0 ) { n = 0; }

							pos.begin = file->GetPrevLine( file->Align( n, charset, &tData->info ),  0, charset, 0, &tData->info );
							pos.col = 0;
						}
						break;

						//temp
						case ViewerEvent::FOUND:
						{
							bool realLine = false;
							int lineCols = 0;
							seek_t n = file->GetPrevLine( file->Align( event.track + 1, charset, &tData->info ),  &lineCols, charset, &realLine, &tData->info );

							if ( realLine )
							{
								pos.begin = n;
								pos.col = ( VStrWrapCount( lineCols, size.cols ) - 1 ) * size.cols;
							}
							else
							{
								pos.begin = event.track;
								pos.col = 0;
							}
						}
						break;

						case ViewerEvent::PAGEUP:
							if ( pos.begin > 0 && size.cols > 0 )
							{

								p = pos.begin;

								int col = pos.col;
								int r = size.rows - 1;

								if ( r < 1 ) { r = 1; }

								int n = ( col + size.cols - 1 ) / size.cols;

								while ( true )
								{
									if ( n <= 0 )
									{
										col = 0;

										if ( p <= 0 ) { break; }

										int lineCols = 0;
										p = file->GetPrevLine( p, &lineCols, charset, 0, &tData->info );

										if ( !p ) { break; }

										n = VStrWrapCount( lineCols, size.cols );

										if ( n <= 0 ) { break; }
									}

									if ( n >= r )
									{
										col = ( n - r ) * size.cols;
										break;
									}
									else
									{
										r -= n;
										n = 0;
									};
								}

								pos.begin = p > 0 ? p : 0;
								pos.col = col;

							}

							break;

						case ViewerEvent::PAGEDOWN:
							if ( size.cols > 0 )
							{

								p = pos.begin;

								int col = pos.col;
								int r = size.rows - 1;

								if ( r < 1 ) { r = 1; }

								while ( r > 0 )
								{
									if ( !file->ReadString( p, str, charset, &tData->info ) )
									{
										break;
									}

									int n = VStrWrapCount( str.Len(), size.cols ) - ( col + size.cols - 1 ) / size.cols;

									if ( n < 0 ) { break; }

									if ( n >= r )
									{
										col += r * size.cols;
										break;
									}

									col = 0;
									r -= n;

									if ( str.Begin() + str.Bytes() >= file->Size() )
									{
										p = str.Begin();
										col = ( ( n > 0 ) ? n - 1 : 0 ) * size.cols;
										break;
									}

									p = str.Begin() + str.Bytes();
								}

								pos.begin = p;
								pos.col = col;
							}

							break;

					}
				}
				else
				{
					seek_t p;

					switch ( event.type )
					{
						case ViewerEvent::HOME:
							pos.begin = 0;
							pos.col = 0;
							break;

						case ViewerEvent::END:
						{
							p = file->Size();

							for ( int i = 0; p > 0 && i < size.rows; i++ )
							{
								p = file->GetPrevLine( p, 0, charset, 0, &tData->info );
							}

							pos.begin = p;
							pos.col = 0;
						}
						break;

						case ViewerEvent::DOWN:
							if ( file->ReadString( pos.begin, str, charset, &tData->info ) )
							{
								if ( str.NextOffset() < file->Size() )
								{
									pos.begin = str.NextOffset();
								}
							}

							break;

						case ViewerEvent::UP:
							if ( pos.begin > 0 )
							{
								pos.begin = file->GetPrevLine( pos.begin, 0, charset, 0, &tData->info );
							}

							break;

						case ViewerEvent::LEFT:
							if ( pos.col > 0 ) { pos.col--; }

							break;

						case ViewerEvent::RIGHT:
							if ( pos.col + size.cols < pos.maxLine )
							{
								pos.col++;
							}

							break;

						case ViewerEvent::LEFTSTEP:
							pos.col = pos.col > HSTEP ? pos.col - HSTEP : 0;
							break;

						case ViewerEvent::RIGHTSTEP:
						{
							int n = pos.maxLine - ( pos.col + size.cols );

							if ( n > HSTEP ) { n = HSTEP; }

							if ( n > 0 ) { pos.col += n; }
						}
						break;

						case ViewerEvent::PAGELEFT:
						{
							int step = size.cols - 1;

							if ( step > 0 )
							{
								pos.col = pos.col > step ? pos.col - step : 0;
							}
						}
						break;

						case ViewerEvent::PAGERIGHT:
						{
							int step = size.cols - 1;
							int n = pos.maxLine - ( pos.col + size.cols );

							if ( n > step ) { n = step; }

							if ( n > 0 ) { pos.col += n; }
						}
						break;

						case ViewerEvent::HTRACK:
						{
							int n = event.track;

							if ( n > pos.maxLine - size.cols ) { n = pos.maxLine - size.cols; }

							if ( n < 0 ) { n = 0; }

							pos.col = n;
						}
						break;

						case ViewerEvent::VTRACK:
						{
							int n = event.track;

							if ( n < 0 ) { n = 0; }

							pos.begin = file->GetPrevLine( file->Align( n, charset, &tData->info ), 0, charset, 0, &tData->info );
						}
						break;

						//temp
						case ViewerEvent::FOUND:
						{
							bool realLine = false;
							int lineCols = 0;
							seek_t n = file->GetPrevLine( file->Align( event.track + 1, charset, &tData->info ),  &lineCols, charset, &realLine, &tData->info );

							if ( realLine )
							{
								pos.begin = n;

								if ( pos.col + size.cols <= lineCols )
								{
									pos.col = lineCols - 5;

									if ( pos.col < 0 ) { pos.col = 0; }
								}
							}
							else
							{
								pos.begin = event.track;
								pos.col = 0;
							}
						}
						break;

						case ViewerEvent::PAGEUP:
							if ( pos.begin > 0 )
							{
								p = pos.begin;

								for ( int i = 0; p > 0 && i < size.rows - 1; i++ )
								{
									p = file->GetPrevLine( p, 0, charset, 0, &tData->info );
								}

								pos.begin = p;
							}

							break;

						case ViewerEvent::PAGEDOWN:
						{
							p = pos.begin;

							for ( int i = 0; i < size.rows - 1; i++ )
							{
								if ( !file->ReadString( p, str, charset, &tData->info ) )
								{
									break;
								}

								if ( str.NextOffset() >= file->Size() ) { break; }

								p = str.NextOffset();
							}

							if ( p ) { pos.begin = p; }
						}
						break;

					};
				};
			}

//Sleep(1000);

			if ( pos.begin >= file->Size() )
			{
				pos.begin = 0;
				pos.col = 0;
			}

			if ( mode.hex )
			{
				int count = size.rows * size.cols;
				unicode_t* p = ret.data.data();
				char* attr = ret.attr.data();

				seek_t offset = pos.begin;

				while ( count > 0 )
				{
					unsigned char buf[0x100];
					int n = count > sizeof( buf ) ? sizeof( buf ) : count;
					n = file->ReadBlock( offset, ( char* )buf, n, &tData->info );

					if ( n <= 0 ) { break; }

					for ( int i = 0; i < n; i++, p++, attr++ )
					{
						*p = buf[i];
						*attr = marker.In( offset + i ) ? 1 : 0;
					}

					offset += n;
					count -= n;
				}

				for ( ; count > 0; count--, p++, attr++ )
				{
					*p = 0x100;
					*attr = 0;
				}

				pos.col = 0;
				pos.end = pos.begin + size.cols * size.rows;
				pos.size = file->Size();
			}
			else   if ( mode.wrap )
			{
				seek_t offset = pos.begin;
				ViewerString str;
				tData->File()->ReadString( offset, str, charset, &tData->info );
				int col = pos.col;
				unicode_t* p = ret.data.data();
				char* attr = ret.attr.data();
				int r;

				for ( r = 0; r < size.rows; r++, p += size.cols, attr += size.cols )
				{
					int len = str.Len();
					int i;

					for ( i = 0; i + col < len && i < size.cols; i++ )
					{
						unicode_t c = str.Data()[i + col].ch;
						p[i] = c;
						attr[i] = marker.In( str.Data()[i + col].p + str.Begin() ) ? 1 : 0;
					}

					for ( ;  i < size.cols; i++ )
					{
						p[i] = ' ';
						attr[i] = 0;
					}

					col += size.cols;

					if ( col >= len )
					{
						if ( str.NextOffset() >= file->Size() ||
						     !file->ReadString( str.NextOffset(), str, charset, &tData->info ) )
						{
							r++;
							p += size.cols;
							attr += size.cols;
							break;
						}

						col = 0;
					}
				}

				for ( ; r < size.rows; r++, p += size.cols, attr += size.cols )
				{
					for ( int i = 0; i < size.cols; i++ )
					{
						p[i] = ' ';
						attr[i] = 0;
					}
				}

				pos.end = str.NextOffset();
				pos.size = file->Size();

			}
			else
			{
				seek_t offset = pos.begin;
				ViewerString str;
				unicode_t* p = ret.data.data();
				char* attr = ret.attr.data();
				int r;

				for ( r = 0; r < size.rows;
				      r++, p += size.cols, attr += size.cols )
				{
					if ( tData->File()->ReadString( offset, str, charset, &tData->info ) )
					{

						if ( str.Len() > pos.maxLine )
						{
							pos.maxLine = str.Len();
						}

						int i = 0;

						for ( i = 0; i + pos.col < str.Len() && i < size.cols; i++ )
						{
							unicode_t c = str.Data()[i + pos.col].ch;
							p[i] = c;
							attr[i] = marker.In( str.Data()[i + pos.col].p + str.Begin() ) ? 1 : 0;
						}

						for ( ;  i < size.cols; i++ )
						{
							p[i] = ' ';
							attr[i] = 0;
						}
					}
					else
					{
						break;
					}

					offset = str.NextOffset();

					if ( offset >= file->Size() )
					{
						r++;
						p += size.cols;
						attr += size.cols;
						break;
					}
				}

				for ( ; r < size.rows; r++, p += size.cols, attr += size.cols )
				{
					for ( int i = 0; i < size.cols; i++ )
					{
						p[i] = ' ';
						attr[i] = 0;
					}
				}

				pos.end = str.NextOffset();
				pos.size = file->Size();

			}

			pos.marker = marker;

			{
				//lock
				MutexLock lock( &tData->mutex );
				tData->ret = ret;
				tData->pos = pos;
				WinThreadSignal( 0 );
			}


		}
		catch ( cstop_exception* sx )
		{
			tData->info.Reset();
			sx->destroy();
			WinThreadSignal( 0 );
		}
		catch ( cexception* ex )
		{
			tData->error = new_char_str( ex->message() );
			ex->destroy();
			WinThreadSignal( 0 );
		}
		catch ( ... )
		{
			tData->error = new_char_str( "BUG: unhandled exception in void *ViewerThread(void *param)" );
			WinThreadSignal( 0 );
		}

		{
			MutexLock lock( &tData->mutex );
			tData->loadStartTime = 0;
		}
	}

	try
	{
		delete tData;
	}
	catch ( cexception* ex )
	{
		ex->destroy();
	}
	catch ( ... )
	{
		//???
	}

	return 0;
}


ViewWin::ViewWin( Win* parent )
	:   Win( WT_CHILD, 0, parent, 0, 0 ),
	    _lo( 5, 5 ),
	    vscroll( 0, this, true, false ),
	    hscroll( 0, this, false, true ),
	    threadData( 0 ),
	    charset( charset_table[GetFirstOperCharsetId()] ),
	    wrap( true ),
	    hex( false ),
	    loadingText( utf8_to_unicode( " ... Loading ... " ) ),
	    drawLoading( false )
{
	vscroll.Enable();
	vscroll.Show();
	vscroll.SetManagedWin( this );

	hscroll.Enable();
	hscroll.Show();
	hscroll.SetManagedWin( this );

	_lo.AddWin( &vscroll, 0, 1 );
	_lo.SetLineGrowth( 0 );
	_lo.AddWin( &hscroll, 1, 0 );

	_lo.AddRect( &viewRect, 0, 0 );
	_lo.SetColGrowth( 0 );
	_lo.AddRect( &emptyRect, 1, 1 );

	SetLayout( &_lo );
	LSRange lr( 0, 10000, 1000 );
	LSize ls;
	ls.x = ls.y = lr;
	SetLSize( ls );

	OnChangeStyles();
	SetTimer( 1, 1000 );
}

int uiClassViewer = GetUiID( "Viewer" );
int ViewWin::UiGetClassId() { return uiClassViewer; }

void ViewWin::EventTimer( int tid )
{
	if ( threadData && tid == 1 )
	{
		MutexLock lock( &threadData->mutex );
		threadData->inFlags |= ViewerThreadData::FTIMER;
		threadData->cond.Signal();

		int64 tim = time( 0 );

		if ( threadData->loadStartTime && tim != threadData->loadStartTime && !drawLoading )
		{
			drawLoading = true;
			Invalidate();
		}
	}
}

void ViewWin::WrapUnwrap()
{
	if ( threadData )
	{
		MutexLock lock( &threadData->mutex );
		wrap = !wrap;
		threadData->inMode.wrap = wrap;
		threadData->inFlags |= ViewerThreadData::FMODE;
		threadData->inFlags &= ~ViewerThreadData::FEVENT; //clear key events
		threadData->cond.Signal();
	}
}

void ViewWin::HexText()
{
	if ( threadData )
	{
		MutexLock lock( &threadData->mutex );
		hex = !hex;
		threadData->inMode.hex = hex;
		threadData->inFlags |= ViewerThreadData::FMODE;
		threadData->inFlags &= ~ViewerThreadData::FEVENT; //clear key events
		threadData->cond.Signal();
	}

	CalcSize();
}

void ViewWin::NextCharset()
{
	SetCharset( charset_table[GetNextOperCharsetId( charset->id )] );
}

void ViewWin::SetCharset( int n )
{
	SetCharset( charset_table[n] );
}


void ViewWin::SetCharset( charset_struct* cs )
{
	charset = cs;
	SendChanges();

	if ( threadData )
	{
		MutexLock lock( &threadData->mutex );
		threadData->inMode.charset = cs;
		threadData->inFlags |= ViewerThreadData::FMODE;
		threadData->inFlags &= ~ViewerThreadData::FEVENT; //clear key events
		threadData->cond.Signal();
	}
}


bool ViewWin::CalcSize()
{
	if ( threadData )
	{
		MutexLock lock( &threadData->mutex );
		int r = 0, c = 0;

		if ( threadData->inMode.hex )
		{

			//w = (10+2+size.cols*3+1+size.cols + size.cols/8)*charW;

			c = ( ( viewRect.Width() / charW - 13 ) * 8 ) / 33;

			c = ( c / 8 ) * 8;


			if ( c < 8 ) { c = 8; }

			r = viewRect.Height() / charH;
		}
		else
		{
			r = viewRect.Height() / charH;
			c = viewRect.Width() / charW;
		}

		if ( !threadData->inSize.Eq( r, c ) )
		{
			threadData->inSize.rows = r;
			threadData->inSize.cols = c;
			threadData->inFlags |= ViewerThreadData::FSIZE;
			threadData->inFlags &= ~ViewerThreadData::FEVENT; //clear key events
			threadData->cond.Signal();
			return true;
		}
	}

	return false;
}

void ViewWin::ThreadSignal( int id, int data )
{
	if ( threadData && threadData->Id() == id )
	{
		MutexLock lock( &threadData->mutex );
		drawLoading = false;

		if ( threadData->error.data() )
		{
			std::vector<char> s = threadData->error;

			lock.Unlock(); //!!!
			ClearFile();
			Parent()->Command( ID_QUIT, 0, this, 0 );
			NCMessageBox( ( NCDialogParent* )Parent(), "Viewer", s.data(),  true );
			return; //!!!

		}

		if ( threadData->inFlags ) { return; } //есть еще неотработанное событие, подождем его выполнения

		lastResult = threadData->ret;
		lastPos = threadData->pos;

	}

	SendChanges();
	CalcScroll();
	Invalidate(); //temp
}

void ViewWin::EventSize( cevent_size* pEvent )
{
	CalcSize();
}

bool ViewWin::EventKey( cevent_key* pEvent )
{
	if ( !threadData ) { return false; } //!!!

	if ( pEvent->Type() == EV_KEYDOWN )
	{
		bool shift = ( pEvent->Mod() & KM_SHIFT ) != 0;
		bool ctrl = ( pEvent->Mod() & KM_CTRL ) != 0;

		if ( ctrl )
		{
			switch ( pEvent->Key() )
			{
				case VK_NEXT:
					threadData->SetEvent( ViewerEvent( ViewerEvent::END ) );
					return true;

				case VK_PRIOR:
					threadData->SetEvent( ViewerEvent( ViewerEvent::HOME ) );
					return true;

				case VK_RIGHT:
					threadData->SetEvent( ViewerEvent( ViewerEvent::RIGHTSTEP ) );
					return true;

				case VK_LEFT:
					threadData->SetEvent( ViewerEvent( ViewerEvent::LEFTSTEP ) );
					return true;
			}
		}

		switch ( pEvent->Key() )
		{
			case VK_RIGHT:
				threadData->SetEvent( ViewerEvent( ViewerEvent::RIGHT ) );
				break;

			case VK_LEFT:
				threadData->SetEvent( ViewerEvent( ViewerEvent::LEFT ) );
				break;

			case VK_DOWN:
				threadData->SetEvent( ViewerEvent( ViewerEvent::DOWN ) );
				break;

			case VK_UP:
				threadData->SetEvent( ViewerEvent( ViewerEvent::UP ) );
				break;

			case VK_HOME:
				threadData->SetEvent( ViewerEvent( ViewerEvent::HOME ) );
				break;

			case VK_END:
				threadData->SetEvent( ViewerEvent( ViewerEvent::END ) );
				break;

			case VK_NEXT:
				threadData->SetEvent( ViewerEvent( ViewerEvent::PAGEDOWN ) );
				break;

			case VK_PRIOR:
				threadData->SetEvent( ViewerEvent( ViewerEvent::PAGEUP ) );
				break;
		};

		return true;

	}

	return false;
}

bool ViewWin::EventMouse( cevent_mouse* pEvent )
{
	if ( !threadData ) { return false; } //!!!

	switch ( pEvent->Type() )
	{

		case EV_MOUSE_PRESS:
		{
			if ( pEvent->Button() == MB_X1 )
			{
				threadData->SetEvent( ViewerEvent( ViewerEvent::PAGEUP ) );
				break;
			}

			if ( pEvent->Button() == MB_X2 )
			{
				threadData->SetEvent( ViewerEvent( ViewerEvent::PAGEDOWN ) );
				break;
			}
		}
		break;
	};

	return false;
}

static int uiColorCtrl = GetUiID( "ctrl-color" );
static int uiLoadColor = GetUiID( "load-color" );
static int uiLoadBackground = GetUiID( "load-background" );
static int uiHid = GetUiID( "hid" );

void ViewWin::OnChangeStyles()
{
	colors.bg   = UiGetColor( uiBackground, 0, 0, 0xFFFFFF );
	colors.fg   = UiGetColor( uiColor, 0, 0, 0 );
	colors.ctrl = UiGetColor( uiColorCtrl, 0, 0, 0xD00000 );
	colors.markFg  = UiGetColor( uiMarkColor, 0, 0, 0xFFFFFF );
	colors.markBg  = UiGetColor( uiMarkBackground, 0, 0, 0 );
	colors.lnFg = UiGetColor( uiLineColor, 0, 0, 0 ); //line number foreground (in hex mode)
	colors.hid  = UiGetColor( uiHid, 0, 0, 0xD00000 );
	colors.loadBg  = UiGetColor( uiLoadBackground, 0, 0, 0xFFFFFF );
	colors.loadFg  = UiGetColor( uiLoadColor, 0, 0, 0xFF );

	wal::GC gc( this );
	gc.Set( GetFont() );
	cpoint p = gc.GetTextExtents( ABCString );
	charW = p.x / ABCStringLen;
	charH = p.y;

	if ( !charW ) { charW = 10; }

	if ( !charH ) { charH = 10; }

	if ( !CalcSize() ) { Invalidate(); }
}

inline char HexChar( int n )
{
	n &= 0xF;
	return n >= 10 ? n - 10 + 'A' : n + '0';
}


//ret type 0 - normal, 1-spec symbol ('.'), else marked
static int PrepareText( unicode_t* buf, char* typeBuf, int bufSize, unicode_t* s, char* attr, int count )
{
	int n = ( bufSize > count ) ? count : bufSize;

	for ( int t = n; t > 0; t--, buf++, typeBuf++, s++, attr++ )
	{
		char type;

		if ( *s >= 0 && *s < 32/* || *s >= 0x80 && *s < 0xA0*/ )
		{
			*buf = '.';
			type = 1;
		}
		else
		{
			type = 0;
			*buf = *s;
		}

		if ( *attr ) { type = 2; }

		*typeBuf = type;
	}

	return n;
}

static void ViewPreparHexModeText( unicode_t* inStr, char* inAttr, unicode_t* u, char* attr, int count, charset_struct* charset )
{
	if ( count <= 0 ) { return; }

	std::vector<char> str( count );
	int i;

	for ( i = 0; i < count; i++ ) { str[i] = inStr[i]; }

	char* e = str.data() + count;
	char* s = str.data();
	i = 0;

	while ( i < count )
	{
		//bool tab;
		unicode_t c = charset->GetChar( s, e );
		char a = 0;

		if ( c < 32 /*|| c>=0x80 && c< 0xA0*/ )
		{
			c = '.';
			a = 1;
		}

		if ( *inAttr ) { a = 2; }

		inAttr++;

		char* t = charset->GetNext( s, e/*, &tab*/ );
		*u = c;
		*attr = a;

		u++;
		attr++;
		int n = ( t ? t : e ) - s;
		i++;
		s = t;

		for ( int j = 1; j < n && i < count; j++, i++, u++, attr++, inAttr++ )
		{
			*u = c;
			*attr = 3;

			if ( *inAttr ) { *attr = 2; }
		}
	}
}

static void ViewDrawPreparedText( ViewerColors* viewerColors,  wal::GC& gc, int x, int y, unicode_t* s, char* type, int count, int charH, int charW )
{
	while ( count > 0 )
	{
		char t = *type;
		int i = 1;

		while ( i < count && type[i] == t ) i++;

		int fg;
		int bg;

		switch ( t )
		{
			case 0:
				fg = viewerColors->fg;
				bg = viewerColors->bg;
				break;

			case 1:
				fg = viewerColors->ctrl;
				bg = viewerColors->bg;
				break;

			case 3:
				fg = viewerColors->hid;
				bg = viewerColors->bg;
				break;

			default:
				fg = viewerColors->markFg;
				bg = viewerColors->markBg;
				break;
		}

		gc.SetTextColor( fg );
		gc.SetFillColor( bg );
		gc.TextOutF( x, y, s, i );
		count -= i;
		s += i;
		type += i;
		x += i * charW;
	}
}


static void ViewDrawText( ViewerColors* viewerColors, wal::GC& gc, unicode_t* s, char* attr, /*unicode_t *sPrev, char *attrPrev,*/ int count,
                          int x, int y, int charH, int charW )
{
	unicode_t buf[0x100];
	char typeBuf[0x100];

	while ( count > 0 )
	{
		int n = PrepareText( buf, typeBuf, 0x100, s, attr, count );
		ViewDrawPreparedText( viewerColors, gc, x, y, buf, typeBuf, n, charH, charW );
		count -= n;
		s += n;
		attr += n;
		x += n * charW;
	}
}


void ViewWin::Paint( wal::GC& gc, const crect& paintRect )
{
	crect rect = this->ClientRect();

	ASSERT( lastResult.size.rows * lastResult.size.cols <= lastResult.dataSize );

	ViewerSize size = lastResult.size;
	ViewerMode mode = lastResult.mode;

	if ( !emptyRect.IsEmpty() )
	{
		gc.SetFillColor( colors.bg );
		gc.FillRect( emptyRect );
	}

	gc.Set( GetFont() );

	if ( mode.hex )
	{
		gc.SetFillColor( colors.bg );
		//gc.SetTextColor(0xFFFFFF);
		int y = viewRect.top;
		int x = viewRect.left;
		unicode_t* p = lastResult.data.data();

		int bytes = size.cols * size.rows;
		std::vector<unicode_t> txtDataBuf( bytes );
		std::vector<char> txtAttrBuf( bytes );

		ViewPreparHexModeText( lastResult.data.data(), lastResult.attr.data(), txtDataBuf.data(), txtAttrBuf.data(), bytes, charset );

		char* txtAttr = txtAttrBuf.data();
		unicode_t* txt = txtDataBuf.data();

		char* attr = lastResult.attr.data();
		seek_t offset = lastPos.begin;

		int w = ( 10 + 2 + size.cols * 3 + 1 + size.cols + size.cols / 8 ) * charW;

		for ( int r = 0; r < size.rows; r++, p += size.cols, txt += size.cols, attr += size.cols, txtAttr += size.cols, y += charH, offset += size.cols )
		{
			if ( *p == 0x100 )
			{
				crect rect( x, y, x + w, y + charH );
				gc.FillRect( rect );
				continue;
			}

			gc.SetTextColor( colors.lnFg );
			int i;
			int lx = x;
			unicode_t buf[64], *s;

			for ( i = 10, s = buf; i > 0; i--, s++ )
			{
				*s = HexChar( ( offset >> ( i - 1 ) * 4 ) & 0xF );
			}

			*s = 0;
			gc.TextOutF( x, y, buf );

			gc.SetTextColor( colors.fg );
			lx += 10 * charW;
			crect rect( lx, y, lx + charW * 2, y + charH );
			gc.FillRect( rect );
			lx += charW * 2;

			for ( i = 0; i < size.cols; i++ )
			{
				unicode_t c = p[i];

				if ( c != 0x100 )
				{
					buf[0] = HexChar( c >> 4 );;
					buf[1] = HexChar( c );
					gc.TextOutF( lx, y, buf, 2 );
				}
				else
				{
					rect.Set( lx, y, lx + charW * 2, y + charH );
					gc.FillRect( rect );
				}

				lx += 2 * charW;

				int rw = i > 0 && ( ( i + 1 ) % 8 ) == 0 ? 2 * charW : charW;

				rect.Set( lx, y, lx + rw, y + charH );
				gc.FillRect( rect );

				lx += rw;
			}

			rect.Set( lx, y, lx + charW, y + charH );
			gc.FillRect( rect );
			lx += charW;

			for ( i = 0; i < size.cols; i++ )
				if ( p[i] == 0x100 ) { break; }

			ViewDrawPreparedText( &colors, gc, lx, y, txt, txtAttr, size.cols, charH, charW );
			//ViewDrawText(gc, txt, attr, i , lx, y,   charH, charW);

			lx += i * charW;

			if ( i < size.cols )
			{
				rect.Set( lx, y, lx + ( size.cols - i )*charW, y + charH );
				gc.FillRect( rect );
				lx += charW;
			}
		}


		if ( x + w < viewRect.right )
		{
			crect r = viewRect;
			r.left = x + w;
			gc.FillRect( r );
		}

		if ( y < viewRect.Height() )
		{
			crect r = viewRect;
			r.top = y;
			gc.FillRect( r );
		}

	}
	else
	{
		gc.SetFillColor( colors.bg );
		gc.SetTextColor( colors.fg );

		int y = viewRect.top;
		int x = viewRect.left;
		unicode_t* p = lastResult.data.data();
		char* attr = lastResult.attr.data();

		for ( int i = 0; i < size.rows; i++, p += size.cols, attr += size.cols, y += charH )
		{
			ViewDrawText( &colors, gc, p, attr, size.cols, x, y,  charH, charW );
		}

		if ( y < viewRect.Height() )
		{
			crect r = viewRect;
			r.top = y;
			gc.FillRect( r );
		}

		if ( x + size.cols * charW < viewRect.right )
		{
			crect r = viewRect;
			r.left = x + size.cols * charW;
			gc.FillRect( r );
		}
	}

	if ( drawLoading && loadingText.data() )
	{
		cpoint pt = gc.GetTextExtents( loadingText.data() );
		gc.SetFillColor( colors.loadBg );
		gc.SetTextColor( colors.loadFg );
		int x = ( rect.Width() - pt.x ) / 2;
		int y = ( rect.Height() - pt.y ) / 2;
		gc.FillRect( crect( x, y - pt.y, x + pt.x, y ) );
		gc.TextOutF( x, y, loadingText.data() );
		gc.FillRect( crect( x, y + pt.y, x + pt.x, y + 2 * pt.y ) );
	}
}

void ViewWin::CalcScroll()
{
	if ( threadData )
	{
		MutexLock lock( &threadData->mutex );
		ScrollInfo hsi;
		hsi.pageSize = threadData->ret.size.cols;

		if ( threadData->ret.mode.wrap || threadData->ret.mode.hex )
		{
			hsi.size = 0;
			hsi.pos = 0;
		}
		else
		{
			hsi.size = threadData->pos.maxLine;
			hsi.pos = threadData->pos.col;
		}

		bool hVisible = hscroll.IsVisible();
		hscroll.Command( CMD_SCROLL_INFO, SCMD_SCROLL_HCHANGE, this, &hsi );

		ScrollInfo vsi;
		vsi.pageSize = threadData->pos.end - threadData->pos.begin;
		vsi.size = threadData->pos.size;
		vsi.pos = threadData->pos.begin;
		bool vVisible = vscroll.IsVisible();
		vscroll.Command( CMD_SCROLL_INFO, SCMD_SCROLL_VCHANGE, this, &vsi );

		if ( hVisible != hscroll.IsVisible() || vVisible != vscroll.IsVisible() )
		{
			lock.Unlock(); //!!!
			this->RecalcLayouts();
			CalcSize();
		}
	}
}


void ViewWin::SetFile( clPtr<FS> fsp, FSPath& path, seek_t size )
{
	ClearFile();
	VFilePtr vf = new VFile( fsp, path, size, wcmConfig.editTabSize );
	threadData =  new ViewerThreadData( vf );
	threadData->inMode.charset = charset;
	threadData->inMode.wrap = wrap;
	threadData->inMode.hex = hex;

	try
	{
		ThreadCreate( threadData->Id(), ViewerThread, threadData );
		CalcSize();
		drawLoading = true;

	}
	catch ( cexception* ex )
	{
		ex->destroy();
		delete threadData;
		threadData = 0;
	}
	catch ( ... )
	{
		delete threadData;
		threadData = 0;
	}
}

bool ViewWin::Command( int id, int subId, Win* win, void* data )
{
	if ( id != CMD_SCROLL_INFO )
	{
		return false;
	}

	switch ( subId )
	{
		case SCMD_SCROLL_LINE_RIGHT:
			threadData->SetEvent( ViewerEvent( ViewerEvent::RIGHT ) );
			break;

		case SCMD_SCROLL_LINE_LEFT:
			threadData->SetEvent( ViewerEvent( ViewerEvent::LEFT ) );
			break;

		case SCMD_SCROLL_LINE_DOWN:
			threadData->SetEvent( ViewerEvent( ViewerEvent::DOWN ) );
			break;

		case SCMD_SCROLL_LINE_UP:
			threadData->SetEvent( ViewerEvent( ViewerEvent::UP ) );
			break;

		case SCMD_SCROLL_PAGE_UP:
			threadData->SetEvent( ViewerEvent( ViewerEvent::PAGEUP ) );
			break;

		case SCMD_SCROLL_PAGE_DOWN:
			threadData->SetEvent( ViewerEvent( ViewerEvent::PAGEDOWN ) );
			break;

		case SCMD_SCROLL_PAGE_LEFT:
			threadData->SetEvent( ViewerEvent( ViewerEvent::PAGELEFT ) );
			break;

		case SCMD_SCROLL_PAGE_RIGHT:
			threadData->SetEvent( ViewerEvent( ViewerEvent::PAGERIGHT ) );
			break;

		case SCMD_SCROLL_TRACK:
			threadData->SetEvent( ViewerEvent( win == &hscroll ? ViewerEvent::HTRACK : ViewerEvent::VTRACK, ( ( int* )data )[0] ) );
			break;
	}

	return true;
}

void ViewWin::ClearFile()
{
	if ( threadData )
	{
		lastResult.Clear();
		MutexLock lock( &threadData->mutex );
		threadData->inFlags |= ViewerThreadData::FSTOP;
		threadData->info.SetStop();
		threadData->cond.Signal(); //!!!
		threadData = 0;
	};
}

int ViewWin::GetPercent()
{
	VFPos pos = lastPos;
	return ( pos.size > 0 ) ? ( pos.end * 100 ) / pos.size : 100;
}

int ViewWin::GetCol()
{
	if ( lastResult.mode.hex || lastResult.mode.wrap ) { return -1; }

	return lastPos.col;
}


FSString ViewWin::Uri()
{
	if ( threadData )
	{
		return threadData->File()->Uri();
	}

	return FSString();
}


ViewWin::~ViewWin()
{
	ClearFile();
}



///////////////////////////// search

struct VSTData
{
	Mutex mutex;
	//in
	VFilePtr file;
	std::vector<unicode_t> str;
	charset_struct* charset;
	bool sensitive;
	FSCViewerInfo info;
	bool hex;
	seek_t from;

	bool winClosed;
	bool threadStopped;
	//ret
	std::vector<char> err;
	seek_t begin;
	seek_t end;

	VSTData( VFilePtr f, const unicode_t* s, bool sens, charset_struct* cs, bool h, seek_t offset )
		: file( f ), str( new_unicode_str( s ) ), charset( cs ), sensitive( sens ), hex( h ), from( offset ),
		  winClosed( false ),
		  threadStopped( false )
	{}
};


void* VSThreadFunc( void* ptr )
{
	VSTData* data = ( VSTData* )ptr;

	seek_t begin = 0, end = 0;

	{
		//lock
		MutexLock lock( &data->mutex );
	}

	try
	{
		VSearcher search;
		search.Set( data->str.data(), data->sensitive, data->charset );

		int maxLen = search.MaxLen();
		int minLen = search.MinLen();
		int bufSize = 16000 + maxLen;
		std::vector<char> buf( bufSize );
		int count = 0;

		seek_t offset = data->from;

		if ( offset < 0 ) { offset = 0; }

		seek_t nrSize = data->file->Size() - offset;

		int n = bufSize > nrSize ? nrSize : bufSize;
		int bytes = data->file->ReadBlock( offset, buf.data(), n, &data->info );

		if ( bytes > 0 )
		{
			nrSize -= bytes;
			count = bytes;

			while ( true )
			{
				if ( count >= maxLen )
				{
					int n = count - maxLen + 1;
					int fBytes = 0;
					char* s = search.Search( buf.data(), buf.data() + n, &fBytes );

					if ( s )
					{
						begin = offset + ( s - buf.data() );
						end = begin + fBytes;
						break;
					}

					int t = count - n;

					if ( t > 0 ) { memmove( buf.data(), buf.data() + n, t ); }

					count = t;
					offset += n;
				}

				if ( nrSize <= 0 )
				{
					break;
				}

				int n = bufSize - count;

				if ( n > nrSize ) { n = nrSize; }

				int bytes = data->file->ReadBlock( offset + count, buf.data() + count, n, &data->info );

				if ( bytes <= 0 ) { break; }

				nrSize -= bytes;
				count += bytes;
			}

			if ( end == 0 )
			{
				if ( minLen < maxLen )
				{
					int n = maxLen - minLen;

					for ( ; n > 0 && count < bufSize ; n-- )
					{
						buf[count++] = 0;
					}

					if ( count >= maxLen )
					{

						int n = count - maxLen + 1;
						int fBytes = 0;
						char* s = search.Search( buf.data(), buf.data() + n, &fBytes );

						if ( s )
						{
							begin = offset + ( s - buf.data() );
							end = begin + fBytes;
						}
					}

				}
			}
		}


	}
	catch ( cexception* ex )
	{
		try { data->err = new_char_str( ex->message() ); }
		catch ( cexception* x ) { x->destroy(); }

		ex->destroy();
	}
	catch ( ... )
	{
		try { data->err = new_char_str( "BOTVA: unhabdled exception: void *VSThreadFunc(void *ptr) " ); }
		catch ( cexception* x ) { x->destroy(); }
	}

	{
		//lock
		MutexLock lock( &data->mutex );

		if ( data->winClosed )
		{
			lock.Unlock(); //!!!
			delete data;
			return 0;
		}

		data->threadStopped = true;
		data->begin = begin;
		data->end = end;
		WinThreadSignal( 0 );
	}

	return 0;
}

class VSearchDialog: public NCDialog
{
public:
	VSTData* data;
	VSearchDialog( NCDialogParent* parent, VFilePtr file, const unicode_t* str, bool sensitive, charset_struct* charset, bool hex, seek_t from )
		:  NCDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( _LT( "Search" ) ).data(), bListCancel ) //, 0xD8E9EC, 0)
	{
		SetPosition();
		data = new VSTData( file, str, sensitive, charset, hex, from );

		try
		{
			this->ThreadCreate( 1, VSThreadFunc, data );
		}
		catch ( ... )
		{
			delete data;
			data = 0;
			throw;
		}
	}
	virtual void ThreadStopped( int id, void* data );
	virtual ~VSearchDialog();
};

VSearchDialog::~VSearchDialog()
{
	if ( data )
	{
		MutexLock lock( &data->mutex );

		if ( data->threadStopped )
		{
			lock.Unlock(); //!!!
			delete data;
			data = 0;
		}
		else
		{
			data->winClosed = true;
			data->info.SetStop();
			data = 0;
		}
	}
}

void VSearchDialog::ThreadStopped( int id, void* data )
{
	EndModal( CMD_OK );
}


bool ViewWin::Search( const unicode_t* str, bool sensitive )
{
	if ( !threadData ) { return true; }

	seek_t offset = 0;

	{
		//lock
		MutexLock lock( &threadData->mutex );

		if ( threadData->pos.begin >= 0 ) { offset = threadData->pos.begin; }

		if ( !threadData->pos.marker.Empty() && threadData->pos.marker.end > offset )
		{
			offset = threadData->pos.marker.end;
		}
	}

	VSearchDialog dlg( ( NCDialogParent* )Parent(), this->threadData->FilePtr(), str, sensitive, charset, this->hex,
	                   offset
	                 );


	int ret = dlg.DoModal();

	if ( ret == ::CMD_CANCEL ) { return true; }

	if ( !dlg.data ) { return true; }

	if ( dlg.data->err.data() )
	{
		NCMessageBox( ( NCDialogParent* )Parent(), _LT( "Search" ), dlg.data->err.data(), true );
		return true;
	}

	if ( dlg.data->end == 0 ) { return false; }

	threadData->SetEvent( ViewerEvent( ViewerEvent::FOUND, dlg.data->begin, dlg.data->end ) );

	return true;;
}
