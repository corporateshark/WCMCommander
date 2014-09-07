/*
   Copyright (c) by Valery Goryachev (Wal)
*/


#ifndef NCDIALOGS_H
#define NCDIALOGS_H

#include "swl.h"
#include "nchistory.h"
#include "operwin.h"

using namespace wal;

class NCShadowWin: public Win
{
public:
	NCShadowWin( Win* parent );
	virtual void Paint( wal::GC& gc, const crect& paintRect );
	virtual ~NCShadowWin();
};

struct ButtonDataNode
{
	const char* utf8text;
	int cmd;
};


class NCDialogParent: public OperThreadWin
{
	Layout _layout;
public:
	NCDialogParent( Win::WTYPE t, unsigned hints = 0, int nId = 0, Win* _parent = 0, const crect* rect = 0 );
	void AddLayout( Layout* p ) { _layout.AddLayout( p, 0, 0 ); RecalcLayouts(); }
	void DeleteLayout( Layout* p ) { _layout.DelObj( p ); }

	virtual ~NCDialogParent();
};


class NCDialog: public OperThreadWin
{
	NCShadowWin _shadow;
	/* unsigned _fcolor;
	   unsigned _bcolor;*/

	StaticLine _header;

	Layout _lo;
	Layout _buttonLo;
	Layout _headerLo;

	Layout _parentLo;

	crect _borderRect;
	crect _frameRect;


	ccollect<clPtr<Button> > _bList;
	int enterCmd;

	int m_nId;

protected:
	Win* GetDownButton();
	int GetFocusButtonNum();
public:
	NCDialog( bool child, int nId, NCDialogParent* parent, const unicode_t* headerText, ButtonDataNode* blist );
	void MaximizeIfChild( bool x = true, bool y = true ); //чтоб делать большие дочерние диалоги (типа поиска файлов и тд)
	void SetPosition();
	void AddWin( Win* w ) { _lo.AddWin( w, 4, 4 ); }
	void AddLayout( Layout* l ) { _lo.AddLayout( l, 4, 4 ); }
	void SetEnterCmd( int cmd ) { enterCmd = cmd; }
	virtual void Paint( wal::GC& gc, const crect& paintRect );
//	virtual unsigned GetChildColor(Win *w, int id);
	virtual bool EventShow( bool show );
	virtual bool EventKey( cevent_key* pEvent );
	virtual bool EventChildKey( Win* child, cevent_key* pEvent );
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual bool EventClose();
	virtual void EventSize( cevent_size* pEvent );
	virtual void EventMove( cevent_move* pEvent );
	virtual void CloseDialog( int cmd );
	virtual cfont* GetChildFont( Win* w, int fontId );

	virtual int UiGetClassId();

	virtual ~NCDialog();
};


class NCVertDialog: public NCDialog
{
protected:
	ccollect<Win*> order;
public:
	NCVertDialog( bool child, int nId, NCDialogParent* parent, const unicode_t* headerText, ButtonDataNode* blist )
		:  NCDialog( child, nId, parent, headerText, blist ) {}
	virtual bool EventChildKey( Win* child, cevent_key* pEvent );
	virtual ~NCVertDialog();
};


extern bool createDialogAsChild;
extern ButtonDataNode bListOk[];
extern ButtonDataNode bListCancel[];
extern ButtonDataNode bListOkCancel[];
extern ButtonDataNode bListYesNoCancel[];

enum
{
   CMD_KILL = 100,
   CMD_KILL_9 = 101
};

int NCMessageBox( NCDialogParent* parent, const char* utf8head, const char* utf8txt, bool red = false, ButtonDataNode* buttonList = bListOk );
int GoToLineDialog( NCDialogParent* parent );
int KillCmdDialog( NCDialogParent* parent, const unicode_t* cmd );

std::vector<unicode_t> InputStringDialog( NCDialogParent* parent, const unicode_t* message, const unicode_t* str = 0 );


class CmdHistoryDialog: public NCDialog
{
	NCHistory& _history;
	int _selected;
	TextList _list;
public:
	CmdHistoryDialog( int nId, NCDialogParent* parent, NCHistory& history );
	const unicode_t* Get() { return _list.GetCurrentString(); }
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual ~CmdHistoryDialog();
};

class DlgMenuData
{
	friend class DlgMenu;
	struct Node
	{
		std::vector<unicode_t> name;
		std::vector<unicode_t> comment;
		int cmd;
	};
	ccollect<Node> list;
public:
	DlgMenuData();
	int Count() const { return list.count(); }
	void Add( const unicode_t* name, const unicode_t* coment, int cmd );
	void Add( const char* utf8name, const char* utf8coment, int cmd );
	void AddSplitter();
	~DlgMenuData();
};


int RunDldMenu( int nUi, NCDialogParent* parent, const char* header, DlgMenuData* d );

#endif

