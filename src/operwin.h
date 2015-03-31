/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#pragma once

#include "swl.h"

using namespace wal;


typedef int ( * volatile OperCallback )( void* cbData );

class OperThreadWin;

class OperThreadNode
{
	friend class OperThreadWin;
	friend void* __123___OperThread( void* );

	OperThreadWin* volatile win;

	std::string threadInfo; //++volatile !!!
	OperThreadNode* volatile prev;
	OperThreadNode* volatile next;
	volatile bool stopped;
	Mutex mutex;
	void* volatile data;

	void* volatile cbData;
	Cond cbCond;
	volatile int  cbRet;
	OperCallback cbFunc;
public:
	OperThreadNode( OperThreadWin* w, const char* info, void* d )
		:  win( w ), threadInfo( info ), prev( 0 ), next( 0 ),
		   stopped( false ), data( d ),
		   cbData( 0 ), cbRet( -1 ), cbFunc( 0 )
	{}

	Mutex* GetMutex() { return &mutex; }

	void* Data() { return data; } //may be called and accessed to data only by locking the mutex recieved by calling GetMutex
	bool NBStopped() { return stopped; } //may be called only by locking the mutex recieved by calling GetMutex

	int CallBack( OperCallback f, void* data ); // ret < 0 if stopped

	//!!! id >= 2
	bool SendSignal( int id ) //always run when mutex is unlocked
	{ MutexLock lock( &mutex ); if ( stopped || id <= 1 ) { return false; } return WinThreadSignal( id ); }

	~OperThreadNode();
private:
	OperThreadNode() {}
	CLASS_COPY_PROTECTION( OperThreadNode );
};

//thread method, shouldn't have unhandled exceptions
//thread may send signals throught tData only
typedef void ( *OperThreadFunc )( OperThreadNode* node );


class OperThreadWin: public Win
{
	friend void* __123___OperThread( void* );
	int nextThreadId;
	int NewThreadID() { int n = nextThreadId; nextThreadId = ( ( nextThreadId + 1 ) % 0x10000 ); return n + 1; }

	int threadId;
	OperThreadNode* tNode;
	bool cbExecuted;
public:
	OperThreadWin( WTYPE t, unsigned hints = 0, int nId = 0, Win* _parent = nullptr, const crect* rect = nullptr )
		: Win( t, hints, _parent, rect, nId ), nextThreadId( 0 ), tNode( 0 ), cbExecuted( false ) {}

	void RunNewThread( const char* info, OperThreadFunc f, void* data ); //may throw exception
	virtual void OperThreadSignal( int info );
	virtual void OperThreadStopped();

	void StopThread();

	static void DBGPrintStoppingList();

	virtual void ThreadSignal( int id, int data );
	virtual void ThreadStopped( int id, void* data );
	virtual ~OperThreadWin();
};
