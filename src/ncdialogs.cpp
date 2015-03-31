/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "globals.h"
#include "nc.h"
#include "ncfonts.h"
#include "ncdialogs.h"
#include "wcm-config.h"
#include "string-util.h"
#include "ltext.h"
#include "unicode_lc.h"
#include "panel.h"
#include "nceditline.h"

bool createDialogAsChild //= false;
   = true;

NCShadowWin::NCShadowWin( Win* parent )
	: Win( Win::WT_CHILD, 0, parent ) //, &crect(0,0,300,100))
{

}


void NCShadowWin::Paint( wal::GC& gc, const crect& paintRect )
{
	crect cr = ClientRect();
	gc.SetFillColor( 0 ); //::g_WcmConfig.whiteStyle ? 0x404040 : 0x01);
	gc.FillRect( cr );
}

NCShadowWin::~NCShadowWin() {}


void NCDialog::CloseDialog( int cmd )
{
	EndModal( cmd );
}


cfont* NCDialog::GetChildFont( Win* w, int fontId )
{
	return g_DialogFont.ptr();
}

bool NCDialog::Command( int id, int subId, Win* win, void* data )
{
	if ( id && win && win->UiGetClassId() == uiClassButton )
	{
		CloseDialog( id );
		return true;
	}

	return false;
}

int uiClassNCDialog = GetUiID( "NCDialog" );

int NCDialog::UiGetClassId()
{
	return m_nId;
//	return uiClassNCDialog;
}

NCDialog::NCDialog( bool asChild, int nId, NCDialogParent* parent, const unicode_t* headerText, ButtonDataNode* blist ) //, unsigned bcolor, unsigned fcolor)
	: OperThreadWin( asChild ? Win::WT_CHILD : WT_MAIN /*WT_POPUP*/, 0, nId, parent ), //, &crect(0,0,300,100)),
	  _shadow( parent ),
//	_fcolor(fcolor),
//	_bcolor(bcolor),
	  _header( nId, this, headerText ),
	  _lo( 9, 9 ),
	  _buttonLo( 3, 16 ),
	  _headerLo( 3, 3 ),
	  _parentLo( 3, 3 ),
	  enterCmd( 0 ),
	  m_nId( nId )
{
	Enable();

	_lo.SetLineGrowth( 4 );
	_lo.SetColGrowth( 4 );


	_lo.ColSet( 0, 2 );
	_lo.ColSet( 1, 10 );
	_lo.ColSet( 2, 3 );

	_lo.ColSet( 3, 5 );
	_lo.ColSet( 5, 5 );

	_lo.ColSet( 6, 3 );
	_lo.ColSet( 7, 10 );
	_lo.ColSet( 8, 2 );

	_lo.LineSet( 0, 2 );
	_lo.LineSet( 1, 10 );
	_lo.LineSet( 2, 3 );

	_lo.LineSet( 3, 3 );
	//_lo.LineSet(5, 5);

	_lo.LineSet( 6, 3 );
	_lo.LineSet( 7, 5 );
	_lo.LineSet( 8, 2 );

	_lo.ColSet( 4, 16, 100000 );
	_lo.LineSet( 4, 16, 100000 );

	_lo.AddRect( &_borderRect, 0, 0, 8, 8 );
	_lo.AddRect( &_frameRect, 2, 2, 6, 6 );

	_headerLo.ColSet( 0, 1, 1000 );
	_headerLo.ColSet( 2, 1, 1000 );
	_headerLo.LineSet( 0, 2 );
	_headerLo.LineSet( 2, 2 );
	_headerLo.AddWin( &_header, 1, 1 );
	_lo.AddLayout( &_headerLo, 1, 4 );

	_buttonLo.ColSet( 0, 10, 1000 );
	_buttonLo.ColSet( 15, 10, 1000 );
	_buttonLo.LineSet( 0, 2 );
	_buttonLo.LineSet( 2, 2 );

	_parentLo.ColSet( 0, 20, 1000 );
	_parentLo.ColSet( 2, 20, 1000 );
	_parentLo.LineSet( 0, 20, 1000 );
	_parentLo.LineSet( 2, 20, 1000 );
	_parentLo.AddWin( this, 1, 1 );

	_parentLo.SetLineGrowth( 0 );
	_parentLo.SetLineGrowth( 2 );

	_parentLo.SetColGrowth( 0 );
	_parentLo.SetColGrowth( 2 );

	if ( blist )
	{
		int n = 0;
		int minW = 0;

		for ( ; blist->utf8text && n < 7; blist++, n++ )
		{

			clPtr<Button> p = new Button( 0, this, utf8_to_unicode( _LT( carray_cat<char>( "DB>", blist->utf8text ).data(), blist->utf8text ) ).data(), blist->cmd );

			p->Show();
			p->Enable();

			if ( n > 0 )
			{
				_buttonLo.ColSet( n * 2, 5 );
			}

			_buttonLo.AddWin( p.ptr(), 1, n * 2 + 1 );

			if ( minW < p->GetLSize().x.minimal )
			{
				minW = p->GetLSize().x.minimal;
			}

			_bList.append( p );
//break; ???

		}

		if ( minW > 0 )
			for ( int i = 0; i < _bList.count(); i++ )
			{
				LSize s = _bList[i]->GetLSize();
				s.x.minimal = s.x.maximal = minW;
				_bList[i]->SetLSize( s );
			};

		if ( _bList.count() )
		{
			_bList[0]->SetFocus();
		}

	}

	_lo.AddLayout( &_buttonLo, 5, 4 );

	_header.Enable();
	_header.Show();

	SetLayout( &_lo );

	if ( Type() == WT_CHILD && parent )
	{
		parent->AddLayout( &_parentLo );
	}


	SetPosition();

	SetName( appName );
}

Win* NCDialog::GetDownButton()
{
	return _bList.count() > 0 ? _bList[0].ptr() : 0;
}


void NCDialog::MaximizeIfChild( bool x, bool y )
{
	if ( Type() == WT_CHILD && Parent() )
	{
		_parentLo.SetLineGrowth( 0, !y );
		_parentLo.SetLineGrowth( 2, !y );
		_parentLo.SetLineGrowth( 1, y );
		_parentLo.SetColGrowth( 0, !x );
		_parentLo.SetColGrowth( 2, !x );
		_parentLo.SetColGrowth( 1, x );
		Parent()->RecalcLayouts();
	}
}

void NCDialog::SetPosition()
{

	if ( Type() == WT_CHILD )
	{
		LSize ls = _lo.GetLSize();
//		ls.x.maximal = ls.x.minimal;
//		ls.y.maximal = ls.y.minimal;
		SetLSize( ls );

		if ( Parent() )
		{
			Parent()->RecalcLayouts();
		}

		return;
	}


	LSize ls = _lo.GetLSize();

	crect r = Rect();

	int w =  ls.x.ideal > 0 ? ls.x.ideal : 20;
	int h =  ls.y.ideal > 0 ? ls.y.ideal : 20;

	r.right  = r.left + w;
	r.bottom = r.top + h;

	Win* parent = Parent();

	if ( parent )
	{
		crect pr = parent->Rect();

		int w = r.Width();
		int h = r.Height();

		int dx = ( pr.Width() - w ) / 2;
		int dy = ( pr.Height() - h ) / 2;


		r.left = pr.left + dx;
		r.right = r.left + w;
		r.top  = pr.top  + dy;
		r.bottom = r.top + h;

	}

	Move( r );
}

bool NCDialog::EventClose()
{
	CloseDialog( CMD_CANCEL );
	return false;
}

void NCDialog::EventSize( cevent_size* pEvent )
{
	if ( Type() == WT_CHILD && Parent() )
	{
		SetPosition();
		crect r = Rect();
		r.left += 10;
		r.top += 10;
		r.right += 10;
		r.bottom += 10;

		if ( r != _shadow.Rect() ) { _shadow.Move( r ); }

	}

	Win::EventSize( pEvent );
}


void NCDialog::EventMove( cevent_move* pEvent )
{
	if ( Type() == WT_CHILD && Parent() )
	{
		SetPosition();
		crect r = Rect();
		r.left += 10;
		r.top += 10;
		r.right += 10;
		r.bottom += 10;

		if ( r != _shadow.Rect() ) { _shadow.Move( r ); }

	}

	Win::EventMove( pEvent );
}


bool NCDialog::EventKey( cevent_key* pEvent )
{
	if ( pEvent->Type() == EV_KEYDOWN && pEvent->Key() == VK_ESCAPE )
	{
		CloseDialog( CMD_CANCEL );
		return true;
	};

	return Win::EventKey( pEvent );
}

int NCDialog::GetFocusButtonNum()
{
	for ( int i = 0; i < _bList.count(); i++ )
		if ( _bList[i]->InFocus() ) { return i; }

	return -1;
}

bool NCDialog::EventChildKey( Win* child, cevent_key* pEvent )
{
	if ( pEvent->Type() == EV_KEYDOWN )
	{
		//dbg_printf("key=0x%x\n",pEvent->Key());
		if ( pEvent->Key() == VK_ESCAPE )
		{
			CloseDialog( CMD_CANCEL );
			return true;
		}
		else if ( pEvent->Key() == VK_TAB )
		{
			if ( ( pEvent->Mod() & KM_SHIFT ) != 0 )
			{
				FocusPrevChild();
			}
			else
			{
				FocusNextChild();
			}

			return true;
		}

// on UNIX shift-tab gives XK_ISO_Left_Tab
#ifdef XK_ISO_Left_Tab
		else if ( pEvent->Key() == XK_ISO_Left_Tab )
		{
			FocusPrevChild();
			return true;
		}

#endif // XK_ISO_Left_Tab
		else if ( pEvent->Key() == VK_RETURN || pEvent->Key() == VK_NUMPAD_RETURN )
		{
			if ( enterCmd && GetFocusButtonNum() < 0 )
			{
				CloseDialog( enterCmd );
				return true;
			}
		}
		else if ( pEvent->Key() == VK_LEFT || pEvent->Key() == VK_RIGHT )
		{
			int i = GetFocusButtonNum();

			if ( i >= 0 )
			{
				int n;

				if ( pEvent->Key() == VK_LEFT )
				{
					n = i ? i - 1 : _bList.count() - 1;
				}
				else
				{
					n = ( ( i + 1 ) % _bList.count() );
				}

				if ( n != i )
				{
					_bList[n]->SetFocus();
				}

				return true;
			}
		}

	};

	return Win::EventChildKey( child, pEvent );
}

//namespace wal {
//extern Display* display;
//};

bool NCDialog::EventShow( bool show )
{
	if ( show && Type() == WT_CHILD && Parent() )
	{
		SetPosition();
		crect r = Rect();
		_shadow.Move( crect( r.left + 10, r.top + 10, 10 + r.right, 10 + r.bottom ) );
		_shadow.Show( SHOW_INACTIVE );
		_shadow.OnTop();
	}

	OnTop();
	SetFocus();

	return Win::EventShow( show );
}


void NCDialog::Paint( wal::GC& gc, const crect& paintRect )
{
	int bcolor = UiGetColor( uiBackground, 0, 0, 0x808080 );

	gc.SetFillColor( bcolor );
	gc.FillRect( _borderRect );
	Draw3DButtonW2( gc, _borderRect, bcolor, true );
	crect rect = _frameRect;
	Draw3DButtonW2( gc, _frameRect, bcolor, false );
	rect.Dec();
	rect.Dec();
	DrawBorder( gc, rect, 0 );
}


NCDialog::~NCDialog()
{
	if ( Type() == WT_CHILD && Parent() )
	{
		( ( NCDialogParent* )Parent() )->DeleteLayout( &_parentLo );
	}
}

/////////////////////////////////// NCVertDialog

bool NCVertDialog::EventChildKey( Win* child, cevent_key* pEvent )
{
    // try to preprocess key combinations like Cntrl + VK_DOWN
    if ( dynamic_cast<ComboBox*>(child) && child->EventKey(pEvent) )
    {
        return true;
    }

	if ( pEvent->Type() == EV_KEYDOWN )
	{
		if ( pEvent->Key() == VK_UP || pEvent->Key() == VK_DOWN )
		{
			int n = -1;
			int count = order.count();
			Win* button = GetDownButton();

			if ( button ) { count++; }

			for ( int i = 0; i < order.count(); i++ ) if ( order[i]->InFocus() ) { n = i; break; }

			if ( pEvent->Key() == VK_UP )
			{
				n = ( n + count - 1 ) % count;
			}
			else if ( pEvent->Key() == VK_DOWN )
			{
				n = ( n + 1 ) % count;
			}

			if ( n >= 0 && n < order.count() )
			{
				order[n]->SetFocus();
			}
			else if ( button ) { button->SetFocus(); }

			return true;
		}
	}

    return NCDialog::EventChildKey( child, pEvent );
}


NCVertDialog::~NCVertDialog() {}

/*
   //for ltext
   _LT("DB>Ok")
   _LT("DB>Cancel")
   _LT("DB>Yes")
   _LT("DB>No")
*/

char OkButtonText[] = "O&k";
char CancelButtonText[] = "&Cancel";
char PanelButtonText[] = "&Panel";
char YesButtonText[] = "&Yes";
char NoButtonText[] = "&No";

ButtonDataNode bListOk[] = { {OkButtonText, CMD_OK}, {0, 0}};
ButtonDataNode bListCancel[] = { {CancelButtonText, CMD_CANCEL}, {0, 0}};
ButtonDataNode bListOkCancel[] = { {OkButtonText, CMD_OK}, {CancelButtonText, CMD_CANCEL}, {0, 0}};
ButtonDataNode bListOkCancelPanel[] = { { OkButtonText, CMD_OK }, { CancelButtonText, CMD_CANCEL }, { PanelButtonText, CMD_PUT_RESULTS_TO_TEMP_PANEL }, { 0, 0 } };
ButtonDataNode bListYesNoCancel[] = { {YesButtonText, CMD_YES}, {NoButtonText, CMD_NO}, {CancelButtonText, CMD_CANCEL}, {0, 0}};

char cmdKillTxt[] = " kill ";
char cmdKill9Txt[] = " kill -9 ";

ButtonDataNode bListKill[] = { {CancelButtonText, CMD_CANCEL}, {cmdKillTxt, CMD_KILL}, {cmdKill9Txt, CMD_KILL_9}, {0, 0}};

int uiNCMessageBox = GetUiID( "messagebox" );
int uiNCRedMessageBox = GetUiID( "messagebox-red" );

int NCMessageBox( NCDialogParent* parent, const char* utf8head, const char* utf8txt, bool red, ButtonDataNode* buttonList )
{
	std::vector<unicode_t> str = utf8_to_unicode( utf8txt );
	ccollect<unicode_t> buf;

	unicode_t* s = str.data();
	int lLine = 0;

	for ( ; *s; s++ )
	{
		if ( lLine > 100 )
		{
			buf.append( '\n' );
			lLine = 0;
		}

		if ( *s == '\n' )
		{
			lLine = 0;
		}

		buf.append( *s );
		lLine++;
	}

	buf.append( 0 );


	NCDialog d( ::createDialogAsChild, red ? uiNCRedMessageBox : uiNCMessageBox, parent, utf8_to_unicode( utf8head ).data(), buttonList ); //, red ? 0xFF:0xD8E9EC, red ? 0xFFFFFF :0x1);
	StaticLine text( 0, &d, buf.ptr() );

	text.Show();
	text.Enable();
	Layout lo( 3, 3 );
	lo.LineSet( 0, 5 );
	lo.LineSet( 2, 5 );
	lo.ColSet( 0, 15 );
	lo.ColSet( 2, 15 );
	lo.AddWin( &text, 1, 1 );
	d.AddLayout( &lo );

	d.SetPosition();
	return d.DoModal();
}


class GoToDialog: public NCVertDialog
{
	EditLine edit;
public:
	GoToDialog( int nId, NCDialogParent* parent )
		:  NCVertDialog( ::createDialogAsChild, nId, parent, utf8_to_unicode( "Go to line" ).data(), bListOkCancel ), //0xD8E9EC, 0),
		   edit( 0, ( Win* )this, 0, 0 )
	{
		edit.Enable();
		edit.Show();
		AddWin( &edit );
		order.append( &edit );
		edit.SetFocus();
		SetPosition();
	}

	std::vector<unicode_t> GetText() { return edit.GetText(); };

	virtual ~GoToDialog();
};

GoToDialog::~GoToDialog() {}


int GoToLineDialog( NCDialogParent* parent )
{
	GoToDialog d( 0, parent );
	d.SetEnterCmd( CMD_OK );
	int r = d.DoModal();

	if ( r != CMD_OK ) { return -1; }

	std::vector<unicode_t> str = d.GetText();
	int n = 0;
	unicode_t* s = str.data();

	while ( *s == ' ' ) { s++; }

	for ( ; *s >= '0' && *s <= '9'; s++ )
	{
		n = n * 10 + *s - '0';
	}

	return n;
}

class clInputStrDialogBase : public NCVertDialog
{
public:
	clInputStrDialogBase( NCDialogParent* Parent, const unicode_t* Message )
		: NCVertDialog( ::createDialogAsChild, 0, Parent, Message, bListOkCancel )
	{
	}
	virtual ~clInputStrDialogBase() {}

	virtual std::vector<unicode_t> GetText() const = 0;

	virtual std::vector<unicode_t> ShowDialog();
};

std::vector<unicode_t> clInputStrDialogBase::ShowDialog()
{
	SetEnterCmd( CMD_OK );

	if ( DoModal() != CMD_OK )
	{
		return std::vector<unicode_t>();
	}

	return GetText();
}


class clInputStrDialog : public clInputStrDialogBase
{
private:
	EditLine m_StrEdit;

public:
	clInputStrDialog( NCDialogParent* Parent, const unicode_t* Message, const unicode_t* Str )
		: clInputStrDialogBase( Parent, Message )
		, m_StrEdit( 0, (Win*) this, 0, 0, 100 )
	{
		m_StrEdit.Enable();
		m_StrEdit.Show();
		AddWin( &m_StrEdit );
		order.append( &m_StrEdit );

		if ( Str )
		{
			m_StrEdit.SetText( Str, true );
		}

		m_StrEdit.SetFocus();
		SetPosition();
	}
	virtual ~clInputStrDialog() {}

	virtual std::vector<unicode_t> GetText() const override
	{
		return m_StrEdit.GetText();
	}
};


class clInputFieldDialog : public clInputStrDialogBase
{
private:
	clNCEditLine m_FieldEdit;

public:
	clInputFieldDialog( const char* FieldName, NCDialogParent* Parent, const unicode_t* Message, const unicode_t* Str )
		: clInputStrDialogBase( Parent, Message )
		, m_FieldEdit( FieldName, 0, (Win*)this, 0, 100, 7 )
	{
		m_FieldEdit.Enable();
		m_FieldEdit.Show();
		AddWin( &m_FieldEdit );
		order.append( &m_FieldEdit );

		if ( Str )
		{
			m_FieldEdit.SetText( Str, true );
		}

		m_FieldEdit.SetFocus();
		SetPosition();
	}
	virtual ~clInputFieldDialog() {}

	virtual std::vector<unicode_t> GetText() const override
	{
		return m_FieldEdit.GetText();
	}

	virtual std::vector<unicode_t> ShowDialog() override
	{
		std::vector<unicode_t> Res = clInputStrDialogBase::ShowDialog();
		if ( Res.size() > 0 )
		{
			m_FieldEdit.AddCurrentTextToHistory();
		}

		return Res;
	}
};


std::vector<unicode_t> InputStringDialog( NCDialogParent* Parent, const unicode_t* Message, const unicode_t* Str )
{
	return InputStringDialog( nullptr, Parent, Message, Str );
}

std::vector<unicode_t> InputStringDialog( const char* FieldName, NCDialogParent* Parent, const unicode_t* Message, const unicode_t* Str )
{
	if ( !FieldName )
	{
		clInputStrDialog Dlg( Parent, Message, Str );
		return Dlg.ShowDialog();
	}

	clInputFieldDialog Dlg( FieldName, Parent, Message, Str );
	return Dlg.ShowDialog();
}

int uiKillCmdDialog = GetUiID( "KillCmdDialog" );

int KillCmdDialog( NCDialogParent* parent, const unicode_t* cmd )
{
	NCDialog d( ::createDialogAsChild, uiKillCmdDialog, parent, utf8_to_unicode( "Kill command" ).data(), bListKill ); //,  0xD8E9EC, 0);
	StaticLine text( 0, &d, cmd );

	text.Show();
	text.Enable();
	Layout lo( 3, 3 );
	lo.LineSet( 0, 5 );
	lo.LineSet( 2, 5 );
	lo.ColSet( 0, 15 );
	lo.ColSet( 2, 15 );
	lo.AddWin( &text, 1, 1 );
	d.AddLayout( &lo );

	d.SetPosition();
	return d.DoModal();

}

////////////////////////////////////////////////////////////////////////////

CmdHistoryDialog::CmdHistoryDialog( int nId, NCDialogParent* parent, NCHistory& history )
	:  NCDialog( createDialogAsChild, nId, parent, utf8_to_unicode( " History " ).data(), bListOkCancel ),
	   _history( history ),
	   _selected( history.Count() - 1 ),
	   _list( Win::WT_CHILD, Win::WH_TABFOCUS | WH_CLICKFOCUS, 0, this, VListWin::SINGLE_SELECT, VListWin::BORDER_3D, 0 )
{
	for ( int i = _history.Count() - 1; i >= 0; i-- ) { _list.Append( _history[i] ); }

	_list.Enable();
	_list.Show();
	_list.SetFocus();
	LSRange h( 10, 1000, 10 );
	LSRange w( 50, 1000, 30 );
	_list.SetHeightRange( h ); //in characters
	_list.SetWidthRange( w ); //in characters

	if ( _history.Count() > 0 )
	{
		_list.MoveFirst( 0 );
		_list.MoveCurrent( _history.Count() - 1 );
	}


	AddWin( &_list );
	SetEnterCmd( CMD_OK );
	SetPosition();
};


bool CmdHistoryDialog::Command( int id, int subId, Win* win, void* data )
{
	if ( id == CMD_ITEM_CLICK && win == &_list )
	{
		EndModal( CMD_OK );
	}

	return NCDialog::Command( id, subId, win, data );
}

////////////////////////////////////////////////////////////////////////////
NCDialogParent::NCDialogParent( Win::WTYPE t, unsigned hints, int nId , Win* _parent , const crect* rect )
	:  OperThreadWin( t, hints, nId, _parent, rect ), _layout( 1, 1 )
{
	_layout.SetLineGrowth( 0 );
	_layout.SetColGrowth( 0 );
	_layout.ColSet( 0, 32, 100000 );
	_layout.LineSet( 0, 32, 100000 );
	SetLayout( &_layout );
}

NCDialogParent::~NCDialogParent() {}



////////////////////////////////////////////  DlgMenu

void clMenuData::Add( const unicode_t* name, const unicode_t* comment1, const unicode_t* comment2, int cmd, int icon )
{
	Node node;

	if ( name ) { node.name = new_unicode_str( name ); }

	if ( comment1 ) { node.comment1 = new_unicode_str( comment1 ); }

	if ( comment2 ) { node.comment2 = new_unicode_str( comment2 ); }

	node.cmd = cmd;
	node.icon = icon;

	list.append( node );
}

void clMenuData::Add( const char* utf8name, const char* utf8coment1, const char* utf8coment2, int cmd, int icon )
{
	Add( utf8name ? utf8_to_unicode( utf8name ).data() : 0,
	     utf8coment1 ? utf8_to_unicode( utf8coment1 ).data() : 0,
	     utf8coment2 ? utf8_to_unicode( utf8coment2 ).data() : 0,
	     cmd,
	     icon
	   );
}

void clMenuData::AddSplitter()
{
	Node node;
	node.cmd = 0;
	list.append( node );
}

class clSelectDriveDlgMenu: public Win
{
	clMenuData* _data;
	int _current;
	int _itemH;
	int _width;
	int _nameW, _comment1W, _comment2W;
	int _splitterH;
	int _splitterW;
public:
	clSelectDriveDlgMenu( Win* parent, clMenuData* data );

	void SetCurrent( int n, bool searchUp );

	virtual bool EventKey( cevent_key* pEvent );
	virtual void Paint( wal::GC& gc, const crect& paintRect );
	virtual bool EventMouse( cevent_mouse* pEvent );
	virtual bool EventShow( bool show );
	virtual int UiGetClassId();
};


void clSelectDriveDlgMenu::SetCurrent( int n, bool searchUp )
{
	int count = _data->Count();

	if ( count <= 0 ) { _current = -1; return; }

	if ( n >= count ) { n = count - 1; }

	if ( n <= 0 ) { n = 0; }

	while ( n >= 0 && n < count )
	{
		if ( _data->list[n].cmd ) { break; }

		n = n + ( searchUp ? -1 : 1 );
	}

	_current = n;
}

//int uiDlgMenu = GetUiID("DlgMenu");
int clSelectDriveDlgMenu::UiGetClassId()
{
	return uiClassButton/*DlgMenu*/;
} //for dialog accepts it as a button


bool clSelectDriveDlgMenu::EventShow( bool show )
{
	if ( show ) { SetCapture(); }
	else { ReleaseCapture(); }

	return true;
}


clSelectDriveDlgMenu::clSelectDriveDlgMenu( Win* parent, clMenuData* data )
	: Win( Win::WT_CHILD, Win::WH_TABFOCUS | Win::WH_CLICKFOCUS, parent )
	, _data( data )
	, _current( 0 )
	, _itemH( 16 )
	, _width( 50 )
	, _nameW( 10 )
	, _comment1W( 0 )
	, _comment2W( 0 )
	, _splitterH( 5 )
	, _splitterW( 1 )
{
	wal::GC gc( this );
	gc.Set( GetFont() );

	int w = 0;
	int height = 0;

	static unicode_t A[] = {'A', 'B', 'C'};
	cpoint p = gc.GetTextExtents( A, 3 );
	_itemH = p.y;

	if ( _itemH < 16 ) { _itemH = 16; }

	_splitterH = _itemH / 4;

	if ( _splitterH < 3 ) { _splitterH = 3; }

	for ( int i = 0 ; i < _data->Count(); i++ )
	{
		unicode_t* name = _data->list[i].name.data();

		if ( _data->list[i].cmd != 0 )
		{
			if ( name )
			{
				cpoint p = gc.GetTextExtents( name );

				if ( _nameW < p.x ) { _nameW = p.x; }
			}

			unicode_t* comment1 = _data->list[i].comment1.data();
			unicode_t* comment2 = _data->list[i].comment2.data();

			if ( comment1 )
			{
				cpoint p = gc.GetTextExtents( comment1 );

				if ( _comment1W < p.x ) { _comment1W = p.x; }
			}

			if ( comment2 )
			{
				cpoint p = gc.GetTextExtents( comment2 );

				if ( _comment2W < p.x ) { _comment2W = p.x; }
			}

			height += _itemH;
		}
		else
		{
			height += _splitterH;
		}
	}

	w =  16 + 5 + _nameW + 5 + _comment1W + 30 + _comment2W;

	if ( _width < w ) { _width = w; }

	SetLSize( LSize( cpoint( _width, height ) ) );
}

inline bool EqFirst( const unicode_t* s, int c )
{
	return s && UnicodeLC( *s ) == c;
}

bool clSelectDriveDlgMenu::EventKey( cevent_key* pEvent )
{
	if ( _data->Count() && pEvent->Type() == EV_KEYDOWN )
	{

		switch ( pEvent->Key() )
		{
			case VK_R:
			{
				bool IsCtrl = ( pEvent->Mod() & KM_CTRL ) != 0;
				if ( IsCtrl )
				{
					Command( ID_RESTART_DIALOG, 0, this, nullptr );
					return false;
				}
				break;
			}

			case VK_UP:
				SetCurrent( _current - 1, true );
				break;

			case VK_DOWN:
				SetCurrent( _current + 1, false );
				break;

			case VK_HOME:
			case VK_LEFT:
				SetCurrent( 0, false );
				break;

			case VK_END:
			case VK_RIGHT:
				SetCurrent( _data->Count() - 1, true ); ;
				break;

			case VK_NUMPAD_RETURN:
			case VK_RETURN:
				if ( _current >= 0 && _current < _data->Count() && _data->list[_current].cmd != 0 )
				{
					Command( _data->list[_current].cmd, 0, this, 0 );
				}

				break;

			default:
			{
				unicode_t c = UnicodeLC( pEvent->Char() );
				int i;
				int n = 0;
				int count = _data->Count();

				for ( i = 0; i < count; i++ )
					if ( EqFirst( _data->list[i].name.data(), c ) ) { n++; }

				if ( n == 1 )
				{
					for ( i = 0; i < count; i++ )
						if ( EqFirst( _data->list[i].name.data(), c ) )
						{
							_current = i;
							Command( _data->list[i].cmd, 0, this, 0 );
							break;
						};

					break;
				}

				if ( n > 1 )
				{
					for ( i = _current + 1; i < count; i++ )
						if ( EqFirst( _data->list[i].name.data(), c ) ) { goto t; }

					for ( i = 0; i <= _current && i < count; i++ )
						if ( EqFirst( _data->list[i].name.data(), c ) ) { goto t; }

t:
					;
					_current = i;
					break;
				}

			}

			return Win::EventKey( pEvent );
		}

		Invalidate();
	};

	return Win::EventKey( pEvent );
}


bool clSelectDriveDlgMenu::EventMouse( cevent_mouse* pEvent )
{
	switch ( pEvent->Type() )
	{
		case EV_MOUSE_MOVE:
			break;

		case EV_MOUSE_PRESS:
		case EV_MOUSE_DOUBLE:
		{
			cpoint p = pEvent->Point();
			int count = _data->Count();

			if ( !count || p.x < 0 || p.x > _width || p.y < 0 )
			{
				Command( CMD_CANCEL, 0, this, 0 );
				return true;
			}

			if ( pEvent->Button() != MB_L ) { break; }

			int n = -1;
			int h = 0;

			for ( int i = 0; i < _data->Count(); i++ )
			{
				int ih = _data->list[i].cmd ? _itemH : _splitterH;

				if ( p.y < h + ih )
				{
					if ( _data->list[i].cmd != 0 ) { n = i; }

					break;
				}

				h += ih;
			}

			if ( n >= 0 )
			{
				_current = n;
				Invalidate();
				Command( _data->list[n].cmd, 0, this, 0 );
			}
		}
		break;

		case EV_MOUSE_RELEASE:
			break;
	};

	return true;
}

static int uiFcColor = GetUiID( "first-char-color" );
static int uiCommentColor = GetUiID( "comment-color" );

void clSelectDriveDlgMenu::Paint( wal::GC& gc, const crect& paintRect )
{
	cfont* font = GetFont();
	gc.Set( font );
	int y = 0;

	int bgColor = UiGetColor( uiBackground, 0, 0, 0xB0B000 );

	int count = _data->Count();

	int Splitters = 0;
	int SplitterTop = 0;
	int SplitterBottom = 0;

	for ( int i = 0; i < count; i++ )
	{
		if ( _data->list[i].cmd == 0 )
		{
			gc.SetFillColor( bgColor );
			gc.FillRect( crect( 0, y, _width, y + _splitterH ) );
			crect rect( 0, y + 1, 0 + _width, y + 2 );
			gc.SetFillColor( ColorTone( bgColor, -150 ) );
			gc.FillRect( rect );
			rect.top += 1;
			rect.bottom += 1;
			gc.SetFillColor( ColorTone( bgColor, +50 ) );
			gc.FillRect( rect );
			y += _splitterH;
			Splitters++;

			if ( Splitters == 1 ) { SplitterTop = y; }

			if ( Splitters == 2 ) { SplitterBottom = y - _splitterH; }
		}
		else
		{
			UiCondList ucl;

			if ( i == _current ) { ucl.Set( uiCurrentItem, true ); }

			unsigned bg = UiGetColor( uiBackground, uiItem, &ucl, 0xFFFFFF );
			unsigned textColor = UiGetColor( uiColor, uiItem, &ucl, 0 );
			unsigned fcColor = UiGetColor( uiFcColor, uiItem, &ucl, 0xFF );
			unsigned commentColor = UiGetColor( uiCommentColor, uiItem, &ucl, 0 );

			gc.SetFillColor( bg );
			gc.FillRect( crect( 0, y, _width, y + _itemH ) );

			cicon icon;

			if ( _data->list[i].icon >= 0 )
			{
				icon.Load( _data->list[i].icon, 16, 16 );
			}
			else
			{
				icon.Load( _data->list[i].cmd, 16, 16 );
			}

			gc.DrawIcon( 0, y, &icon );
			gc.SetTextColor( textColor );
			int x = 16 + 5;

			const unicode_t* name = _data->list[i].name.data();
			const unicode_t* comment1 = _data->list[i].comment1.data();
			const unicode_t* comment2 = _data->list[i].comment2.data();

			if ( name )
			{
				gc.TextOutF( x, y, name );
				gc.SetTextColor( fcColor );
				gc.TextOutF( x, y, name, 1 );
			}

			if ( comment1 )
			{
				gc.SetTextColor( commentColor );
				gc.TextOutF( x + _nameW + 5, y, comment1 );
			}

			if ( comment2 )
			{
				gc.SetTextColor( commentColor );
				gc.TextOutF( x + _nameW + 5 + _comment1W + 30, y, comment2 );
			}

			y += _itemH;
		}
	}

	if ( _comment2W && SplitterTop && SplitterBottom )
	{
		int cx = _nameW + 5 + _comment1W + 30;
		gc.SetFillColor( ColorTone( bgColor, -150 ) );
		gc.FillRect( crect( cx, SplitterTop, cx + _splitterW, SplitterBottom ) );
	}
}

int RunDldMenu( int nUi, NCDialogParent* parent, const char* header, clMenuData* data )
{
	if ( !data || data->Count() <= 0 ) { return CMD_CANCEL; }

	NCDialog dlg( true, nUi, parent, utf8_to_unicode( header ).data(), 0 ); //, ::g_WcmConfig.whiteStyle ? 0xD8E9EC : 0xB0B000, 0xFFFFFF);
	clSelectDriveDlgMenu mtable( &dlg, data );
	mtable.Show();
	mtable.Enable();
	mtable.SetFocus();
	dlg.AddWin( &mtable );
	int Result = dlg.DoModal();
	return Result;
}

