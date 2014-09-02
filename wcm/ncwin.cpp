/*
   Copyright (c) by Valery Goryachev (Wal) 2010
*/

#include <algorithm>

#include <sys/types.h>

#ifdef _WIN32
#  include <winsock2.h>
#else
#  include <signal.h>
#  include <sys/wait.h>
#endif

#include "ncwin.h"
#include "wcm-config.h"
#include "ncfonts.h"

#include "eloadsave.h"
#include "vfs-uri.h"
#include "vfs-smb.h"
#include "vfs-ftp.h"
#include "smblogon.h"
#include "ftplogon.h"
#include "sftpdlg.h"
#include "ext-app.h"
#include "charsetdlg.h"
#include "string-util.h"
#include "filesearch.h"
#include "help.h"
#include "shortcuts.h"
#include "fontdlg.h"
#include "color-style.h"
#include "search-dlg.h"
#include "shell-tools.h"
#include "dircalc.h"
#include "ltext.h"

#ifndef _WIN32
#  include "ux_util.h"
#endif

template <typename T> void SkipSpaces( T& p )
{
	while ( *p == ' ' ) { *p++; }
}

extern SearchAndReplaceParams searchParams;

static crect acWinRect( 0, 0, 850, 500 );

static unicode_t panelButtonStr[] = {'*', 0};

//static char verString[] = "Wal Commander v 0.8.2 beta\nCopyright (c) by Valery Goryachev 2011";

void NCWin::SetToolbarPanel()
{
	_toolBar.Clear();
	_toolBar.AddCmd( ID_REFRESH,   _LT( "Reread\ncurrent panel" ) );
	_toolBar.AddCmd( ID_CTRL_O,    _LT( "Show/Hide panel" ) );
	_toolBar.AddCmd( ID_PANEL_EQUAL, _LT( "=  panels" ) );
	_toolBar.AddSplitter();
	_toolBar.AddCmd( ID_SEARCH_2,  _LT( "Search files" ) );
	_toolBar.AddSplitter();
	_toolBar.AddCmd( ID_SHORTCUTS, _LT( "Show Shortcuts" ) );
	_toolBar.AddSplitter();
	_toolBar.AddCmd( ID_HISTORY,   _LT( "Show Command history" ) );
	_toolBar.Invalidate();
}

void NCWin::SetToolbarEdit()
{
	_toolBar.Clear();
	_toolBar.AddCmd( ID_SAVE,   _LT( "Save file" ) );
	_toolBar.AddSplitter();
	_toolBar.AddCmd( ID_SEARCH_2,  _LT( "Search text" ) );
	_toolBar.AddCmd( ID_REPLACE_TEXT, _LT( "Replace text" ) );
	_toolBar.AddSplitter();
	_toolBar.AddCmd( ID_UNDO,  _LT( "Undo" ) );
	_toolBar.AddCmd( ID_REDO,  _LT( "Redo" ) );
	_toolBar.Invalidate();
}

void NCWin::SetToolbarView()
{
	_toolBar.Clear();
	_toolBar.AddCmd( ID_SEARCH_2,  _LT( "Search text" ) );
	_toolBar.Invalidate();
}

int uiClassNCWin = GetUiID( "NCWin" );
int uiCommandLine = GetUiID( "command-line" );

int NCCommandLine::UiGetClassId()
{
	return uiCommandLine;
}

bool StartsWithAndNotEqual( const unicode_t* Str, const unicode_t* SubStr )
{
	const unicode_t* S = Str;
	const unicode_t* SS = SubStr;

	while ( *SS != 0 ) if ( *S++ != *SS++ )
	{
		return false;
	}

	if ( *SS == 0 && *S == 0 ) return false;

	return true;
}

void NCWin::EventSize( cevent_size* pEvent )
{
	Win::EventSize( pEvent );
}

void NCWin::NotifyAutoComplete()
{
	std::vector<unicode_t> Text = _edit.GetText();

	this->UpdateAutoComplete( Text );
}

void NCWin::NotifyAutoCompleteChange()
{
	const unicode_t* p = m_AutoCompleteList.GetCurrentString();
	if ( p && *p ) _edit.SetText( p, false );
}

void NCWin::UpdateAutoComplete( const std::vector<unicode_t>& CurrentCommand )
{
	if ( CurrentCommand.empty() || CurrentCommand[0] == 0 || !wcmConfig.systemAutoComplete )
	{
		m_AutoCompleteList.Hide( );
		return;
	}

	LSRange h( 10, 1000, 10 );
	LSRange w( 50, 1000, 30 );
	m_AutoCompleteList.SetHeightRange( h ); //in characters
	m_AutoCompleteList.SetWidthRange( w ); //in characters

	if ( _history.Count() > 0 && !m_AutoCompleteList.IsVisible() )
	{
		m_AutoCompleteList.MoveFirst( 0 );
		m_AutoCompleteList.MoveCurrent( 0 );
	}

	const int AutoCompleteMargin = 4;
	const int AutoCompleteListHeight = 220;
	const int Bottom = _leftPanel.Rect().bottom - AutoCompleteMargin;
	crect r;
	r.left = _editPref.Rect().right;
	r.top =  Bottom - AutoCompleteListHeight;
	r.right = Rect( ).right;
	r.bottom = Bottom;
	m_AutoCompleteList.Move( r );

	bool HasHistory = false;

	m_AutoCompleteList.Clear();
	m_AutoCompleteList.Append( 0 ); // empty line goes first

	for ( int i = 0; i < _history.Count(); i++ )
	{
		const unicode_t* Hist = _history[i];

		if ( StartsWithAndNotEqual( Hist, CurrentCommand.data() ) )
		{
			m_AutoCompleteList.Append( Hist, i );
			HasHistory = true;
		}
	}

	if ( HasHistory )
	{
		if ( !m_AutoCompleteList.IsVisible( ) ) m_AutoCompleteList.Show( );
		m_AutoCompleteList.Invalidate();
	}
	else
	{
		m_AutoCompleteList.Hide();
	}

}

NCWin::NCWin()
	:  NCDialogParent( WT_MAIN, WH_SYSMENU | WH_RESIZE | WH_MINBOX | WH_MAXBOX | WH_USEDEFPOS, uiClassNCWin, 0, &acWinRect ),
	   _lo( 5, 1 ),
	   _terminal( 0, this ),
	   _lpanel( 1, 2 ),
	   _ledit( 1, 2 ),
	   _buttonWin( this ),

	   _leftPanel( this, &wcmConfig.panelModeLeft ),
	   _rightPanel( this, &wcmConfig.panelModeRight ),

	   _edit( uiCommandLine, this, 0, 0, 10, false ),
	   _editPref( this ),
		_activityNotification( this ),
	   _panel( &_leftPanel ),
	   _menu( 0, this ),
	   _toolBar( this, 0, 16 ),
	   _mode( PANEL ),
	   _panelVisible( true ),
	   _viewer( this ),
	   _vhWin( this, &_viewer ),
	   _editor( this ),
	   _ehWin( this, &_editor ),
	   _execId( -1 ),
	   _shiftSelectType( -1 ),
		m_AutoCompleteList( Win::WT_CHILD, Win::WH_TABFOCUS | WH_CLICKFOCUS, 0, this, VListWin::SINGLE_SELECT, VListWin::BORDER_3D, NULL )
{
	m_BackgroundActivity = eBackgroundActivity_None;

	_execSN[0] = 0;

	_editPref.Show();
	_editPref.Enable();
	_leftPanel.OnTop();
	_rightPanel.OnTop();

	m_AutoCompleteList.Enable();
	m_AutoCompleteList.Hide();
	m_AutoCompleteList.OnTop();
	m_AutoCompleteList.SetFocus();

	_activityNotification.Set( utf8_to_unicode( "[0+1]" ).data( ) );
	_activityNotification.Hide();
	_activityNotification.Enable();
	_activityNotification.OnTop();

	if ( wcmConfig.showButtonBar )
	{
		_buttonWin.Show();
	}

	_buttonWin.Enable();

	_edit.Show();
	_edit.Enable();
	_terminal.Show();
	_terminal.Enable();
	_leftPanel.Show();
	_leftPanel.Enable();
	_rightPanel.Show();
	_rightPanel.Enable();
	_menu.Show();
	_menu.Enable();

	SetToolbarPanel();
	_toolBar.Enable();

	if ( wcmConfig.showToolBar )
	{
		_toolBar.Show();
	}

	_viewer.Enable();
	_vhWin.Enable();

	_editor.Enable();
	_ehWin.Enable();

	_ledit.AddWin( &_editPref, 0, 0 );
	_ledit.AddWin( &_edit, 0, 1 );

	_lo.AddWin( &_menu, 0, 0 );
	_lo.AddWin( &_ehWin, 0, 0 );
	_lo.AddWin( &_vhWin, 0, 0 );
	_lo.AddWin( &_toolBar, 1, 0 );

	_lo.AddLayout( &_ledit, 3, 0 );
	_lo.AddWin( &_buttonWin, 4, 0 );
	_lo.AddWin( &_terminal, 2, 0 );
	_lo.AddWin( &_viewer, 2, 0, 3, 0 );
	_lo.AddWin( &_editor, 2, 0, 3, 0 );
	_lo.SetLineGrowth( 2 );

	_lpanel.AddWin( &_leftPanel, 0, 0 );
	_lpanel.AddWin( &_rightPanel, 0, 1 );
	_lpanel.AddWin( &_activityNotification, 0, 0 );
	_lo.AddWin( &m_AutoCompleteList, 0, 1 );
	_lo.AddLayout( &_lpanel, 2, 0 );

	_buttonWin.Set( panelNormalButtons );

	_mdLeft.AddCmd( ID_PANEL_BRIEF_L,    _LT( "Brief" ),        "Ctrl-1" );
	_mdLeft.AddCmd( ID_PANEL_MEDIUM_L,   _LT( "Medium" ),       "Ctrl-2" );
	_mdLeft.AddCmd( ID_PANEL_TWO_COLUMNS_L, _LT( "Two columns" ),     "Ctrl-3" );
	_mdLeft.AddCmd( ID_PANEL_FULL_L,     _LT( "Full (name)" ),     "Ctrl-4" );
	_mdLeft.AddCmd( ID_PANEL_FULL_ST_L,  _LT( "Full (size, time)" ),  "Ctrl-5" );
	_mdLeft.AddCmd( ID_PANEL_FULL_ACCESS_L, _LT( "Full (access)" ),      "Ctrl-6" );
	_mdLeft.AddSplit();

	_mdRight.AddCmd( ID_PANEL_BRIEF_R,   _LT( "Brief" ),        "Ctrl-1" );
	_mdRight.AddCmd( ID_PANEL_MEDIUM_R,  _LT( "Medium" ),       "Ctrl-2" );
	_mdRight.AddCmd( ID_PANEL_TWO_COLUMNS_R,   _LT( "Two columns" ),     "Ctrl-3" );
	_mdRight.AddCmd( ID_PANEL_FULL_R, _LT( "Full (name)" ),     "Ctrl-4" );
	_mdRight.AddCmd( ID_PANEL_FULL_ST_R, _LT( "Full (size, time)" ),  "Ctrl-5" );
	_mdRight.AddCmd( ID_PANEL_FULL_ACCESS_R,   _LT( "Full (access)" ),      "Ctrl-6" );
	_mdRight.AddSplit();

	_mdLeftSort.AddCmd( ID_SORT_BY_NAME_L,   _LT( "SM>Name", "Name" ) );
	_mdLeftSort.AddCmd( ID_SORT_BY_EXT_L,   _LT( "SM>Extension", "Extension" ) );
	_mdLeftSort.AddCmd( ID_SORT_BY_MODIF_L,  _LT( "SM>Modif. Time", "Modif. Time" ) );
	_mdLeftSort.AddCmd( ID_SORT_BY_SIZE_L,   _LT( "SM>Size", "Size" ) );
	_mdLeftSort.AddCmd( ID_UNSORT_L,  _LT( "SM>Unsorted", "Unsorted" ) );

	_mdLeft.AddSub( _LT( "Sort mode" ) , &_mdLeftSort );
	_mdLeft.AddCmd( ID_DEV_SELECT_LEFT, _LT( "Change drive" ),  "Shift-F1" );

	_mdRightSort.AddCmd( ID_SORT_BY_NAME_R,  _LT( "SM>Name", "Name" ) );
	_mdRightSort.AddCmd( ID_SORT_BY_EXT_R,  _LT( "SM>Extension", "Extension" ) );
	_mdRightSort.AddCmd( ID_SORT_BY_MODIF_R, _LT( "SM>Modif. Time", "Modif. Time" ) );
	_mdRightSort.AddCmd( ID_SORT_BY_SIZE_R,  _LT( "SM>Size", "Size" ) );
	_mdRightSort.AddCmd( ID_UNSORT_R, _LT( "SM>Unsorted", "Unsorted" ) );

	_mdRight.AddSub( _LT( "Sort mode" ), &_mdRightSort );
	_mdRight.AddCmd( ID_DEV_SELECT_RIGHT, _LT( "Change drive" ), "Shift-F2" );

//#ifndef _WIN32 //пока там только 1 параметр для unix
//теперь 2
	_mdOptions.AddCmd( ID_CONFIG_SYSTEM, _LT( "System settings" ) );
//#endif
	_mdOptions.AddCmd( ID_CONFIG_PANEL,  _LT( "Panel settings" ) );
	_mdOptions.AddCmd( ID_CONFIG_EDITOR, _LT( "Editor settings" ) );

#ifndef _WIN32
	_mdOptions.AddCmd( ID_CONFIG_TERMINAL,  _LT( "Terminal settings" ) );
#endif

	_mdOptions.AddCmd( ID_CONFIG_STYLE,  _LT( "Styles" ) );
	_mdOptions.AddSplit();
	_mdOptions.AddCmd( ID_CONFIG_SAVE,   _LT( "Save setup" ),   "Shift-F9" );


	_menu.Add( &_mdLeft, utf8_to_unicode( _LT( "Left" ) ).data() );
	_menu.Add( &_mdFiles, utf8_to_unicode( _LT( "Files" ) ).data() );
	_menu.Add( &_mdCommands, utf8_to_unicode( _LT( "Commands" ) ).data() );
	_menu.Add( &_mdOptions, utf8_to_unicode( _LT( "Options" ) ).data() );
	_menu.Add( &_mdRight, utf8_to_unicode( _LT( "Right" ) ).data() );

	_mdFiles.AddCmd( ID_VIEW, _LT( "View" ),  "F3" );
	_mdFiles.AddCmd( ID_EDIT, _LT( "Edit" ),  "F4" );
	_mdFiles.AddCmd( ID_COPY, _LT( "Copy" ),  "F5" );
	_mdFiles.AddCmd( ID_MOVE, _LT( "Move" ),  "F6" );
	_mdFiles.AddCmd( ID_MKDIR, _LT( "Create new directory" ),   "F7" );
	_mdFiles.AddCmd( ID_DELETE, _LT( "Delete" ), "F8" );
	_mdFiles.AddSplit();
	_mdFiles.AddCmd( ID_GROUP_SELECT, _LT( "Select group" ) );
	_mdFiles.AddCmd( ID_GROUP_UNSELECT, _LT( "Unselect group" ) );
	_mdFiles.AddCmd( ID_GROUP_INVERT, _LT( "Invert group" ) );

	_mdCommands.AddCmd( ID_SEARCH_2, _LT( "Find file" ),  "Shift F7" );
	_mdCommands.AddCmd( ID_HISTORY,   _LT( "History" ),   "Shift-F8 (Ctrl-K)" );
	_mdCommands.AddCmd( ID_CTRL_O, _LT( "Panel on/off" ), "Ctrl-O" );
	_mdCommands.AddCmd( ID_PANEL_EQUAL, _LT( "Equal panels" ),  "Ctrl =" );
	_mdCommands.AddSplit();
	_mdCommands.AddCmd( ID_SHORTCUTS, _LT( "Folder shortcuts" ),   "Ctrl D" );

	_edit.SetFocus();

	char hostName[0x100] = "";

	if ( !gethostname( hostName, sizeof( hostName ) ) )
	{
	}

	int len = strlen( hostName );

	if ( len > 16 ) { len = 16; hostName[len] = 0; }

	hostName[len++] =
#ifdef _WIN32
	   '>';
#else
	   geteuid() ? '$' : '#';
#endif

	hostName[len] = 0;

	_editPref.Set( utf8_to_unicode( hostName ).data() );

	SetName( appName );

	this->AddLayout( &_lo );

	// apply position

	wal::crect Rect( wcmConfig.windowX, wcmConfig.windowY, wcmConfig.windowX + wcmConfig.windowWidth, wcmConfig.windowY + wcmConfig.windowHeight );

	if ( Rect.Width() > 0 && Rect.Height() > 0 )
	{
		NCDialogParent::Move( Rect, true );
	}

	// apply saved panel paths
//	printf( "Left = %s\n", wcmConfig.leftPanelPath.data() );
//	printf( "Right = %s\n", wcmConfig.rightPanelPath.data() );

	_leftPanel.LoadPathStringSafe( wcmConfig.leftPanelPath.data() );
	_rightPanel.LoadPathStringSafe( wcmConfig.rightPanelPath.data() );
}

bool NCWin::EventClose()
{
	wcmConfig.Save( this );

	switch ( _mode )
	{
		case PANEL:
			if ( !Blocked() ) { AppExit(); }

			break; //&& NCMessageBox(this, "Quit", "Do you want to quit?", false) == CMD_OK

		case EDIT:
			if ( !Blocked() ) { EditExit(); }

			break;

		case VIEW:
			if ( !Blocked() ) { ViewExit(); }

			break;
	}

	return true;
}

void NCWin::SetMode( MODE m )
{
	switch ( m )
	{
		case PANEL:
		{
			_viewer.Hide();
			_vhWin.Hide();
			_editor.Hide();
			_ehWin.Hide();

			if ( _panelVisible )
			{ _leftPanel.Show(); _rightPanel.Show(); _terminal.Hide();}
			else
			{ _leftPanel.Hide(); _rightPanel.Hide(); _terminal.Show();}

			if ( wcmConfig.showButtonBar ) { _buttonWin.Show(); }

			_edit.Show();
			//_terminal.Show();
			_editPref.Show();
			_menu.Show();
			SetToolbarPanel();

			if ( wcmConfig.showToolBar ) { _toolBar.Show(); }

			_edit.SetFocus();
			_buttonWin.Set( panelNormalButtons ); //!!!
		}
		break;

		case TERMINAL:
		{
			_viewer.Hide();
			_vhWin.Hide();
			_editor.Hide();
			_ehWin.Hide();
			_leftPanel.Hide();
			_rightPanel.Hide();
			_buttonWin.Hide();
			_edit.Hide();
			_terminal.Show();
			_editPref.Hide();
			_menu.Hide();
			_toolBar.Hide();
		}
		break;

		case VIEW:
			_viewer.Show();
			_vhWin.Show();
			_viewer.SetFocus();
			_editor.Hide();
			_ehWin.Hide();
			_leftPanel.Hide();
			_rightPanel.Hide();

			if ( wcmConfig.showButtonBar ) { _buttonWin.Show(); }

			_edit.Hide();
			_terminal.Hide();
			_editPref.Hide();
			_menu.Hide();
			SetToolbarView();

			if ( wcmConfig.showToolBar ) { _toolBar.Show(); }

			_buttonWin.Set( viewNormalButtons ); //!!!
			break;

		case EDIT:
		{
			_viewer.Hide();
			_vhWin.Hide();
			_leftPanel.Hide();
			_rightPanel.Hide();

			if ( wcmConfig.showButtonBar )
			{
				_buttonWin.Show();
			}

			_edit.Hide();
			_terminal.Hide();
			_editPref.Hide();
			_menu.Hide();
			SetToolbarEdit();

			if ( wcmConfig.showToolBar ) { _toolBar.Show(); }

			_editor.Show();
			_editor.SetFocus();
			_ehWin.Show();
			_buttonWin.Set( editNormalButtons ); //!!!
		}
		break;
	}

	_mode = m;

	UpdateActivityNotification();

	RecalcLayouts();
}

void NCWin::ExecuteFile()
{
	if ( _mode != PANEL ) { return; }

	FSNode* p =  _panel->GetCurrent();

	if ( !p || p->IsDir() ) { return; }

	if ( p->IsExe() )
	{
		FS* pFs = _panel->GetFS();

		if ( !pFs || pFs->Type() != FS::SYSTEM )
		{
			NCMessageBox( this, _LT( "Run" ),
			              _LT( "Can`t execute file in not system fs" ), true );
			return;
		}


#ifdef _WIN32
		static unicode_t w[2] = {'"', 0};
		StartExecute( carray_cat<unicode_t>( w, _panel->UriOfCurrent( ).GetUnicode( ), w ).data( ), _panel->GetFS( ), _panel->GetPath( ) );
		return;
#else
		const unicode_t*   fName = p->GetUnicodeName();
		int len = unicode_strlen( fName );
		std::vector<unicode_t> cmd( 2 + len + 1 );
		cmd[0] = '.';
		cmd[1] = '/';
		memcpy( cmd.data() + 2, fName, len * sizeof( unicode_t ) );
		cmd[2 + len] = 0;
		StartExecute( cmd.data(), _panel->GetFS(), _panel->GetPath() );
		return;
#endif
	}
}

#define CMD_OPEN_FILE 1000
#define CMD_EXEC_FILE 1001

void NCWin::PanelEnter()
{
	if ( _mode != PANEL ) { return; }

	FSNode* p =  _panel->GetCurrent();

	if ( !p || p->IsDir() )
	{
		_panel->DirEnter();
		return;
	}

	FS* pFs = _panel->GetFS();

	if ( !pFs )
	{
		return;
	}

	bool cmdChecked = false;
	std::vector<unicode_t> cmd;
	bool terminal = true;
	const unicode_t* pAppName = 0;

	if ( wcmConfig.systemAskOpenExec )
	{
		cmd = GetOpenCommand( _panel->UriOfCurrent().GetUnicode(), &terminal, &pAppName );
		cmdChecked = true;
	}

	if ( p->IsExe() )
	{
#ifndef _WIN32

		if ( wcmConfig.systemAskOpenExec && cmd.data() )
		{

			ButtonDataNode bListOpenExec[] = { {"Open", CMD_OPEN_FILE}, {"Execute", CMD_EXEC_FILE}, {"Cancel", CMD_CANCEL}, {0, 0}};

			static unicode_t emptyStr[] = {0};

			if ( !pAppName ) { pAppName = emptyStr; }

			int ret = NCMessageBox( this, "Open",
			                        carray_cat<char>( "Executable file: ", p->name.GetUtf8(), "\ncan be opened by: ", unicode_to_utf8( pAppName ).data(), "\nExecute or Open?" ).data(),
			                        false, bListOpenExec );

			if ( ret == CMD_CANCEL ) { return; }

			if ( ret == CMD_OPEN_FILE )
			{
#ifndef _WIN32
				if ( !terminal )
				{
					ExecNoTerminalProcess( cmd.data() );
					return;
				};

#endif
				StartExecute( cmd.data(), _panel->GetFS(), _panel->GetPath() );
				return;
			}
		}

#endif
		ExecuteFile();
		return;
	}

	if ( !cmdChecked )
	{
		cmd = GetOpenCommand( _panel->UriOfCurrent().GetUnicode(), &terminal, 0 );
	}

	if ( cmd.data() )
	{
#ifndef _WIN32
		if ( !terminal )
		{
			ExecNoTerminalProcess( cmd.data() );
			return;
		}
#endif

		StartExecute( cmd.data(), _panel->GetFS(), _panel->GetPath() );
	}
}

enum
{
   CMD_RC_RUN = 999,
   CMD_RC_OPEN_0 = 1000
};

struct AppMenuData
{
	struct Node
	{
		unicode_t* cmd;
		bool terminal;
		Node(): cmd( 0 ), terminal( 0 ) {}
		Node( unicode_t* c, bool t ): cmd( c ), terminal( t ) {}
	};
	ccollect<clPtr<MenuData> > mData;
	ccollect<Node> nodeList;
	MenuData* AppendAppList( AppList* list );
};

MenuData* AppMenuData::AppendAppList( AppList* list )
{
	if ( !list ) { return 0; }

	clPtr<MenuData> p = new MenuData();

	for ( int i = 0; i < list->Count(); i++ )
	{
		if ( list->list[i].sub.ptr() )
		{
			MenuData* sub = AppendAppList( list->list[i].sub.ptr() );
			p->AddSub( list->list[i].name.data(), sub );
		}
		else
		{
			p->AddCmd( nodeList.count() + CMD_RC_OPEN_0, list->list[i].name.data() );
			nodeList.append( Node( list->list[i].cmd.data(), list->list[i].terminal ) );
		}
	}

	MenuData* ret = p.ptr();
	mData.append( p );
	return ret;
}

void NCWin::RightButtonPressed( cpoint point )
{
	if ( _mode != PANEL ) { return; }

	FSNode* p =  _panel->GetCurrent();

	if ( !p ) { return; }

	if ( p->IsDir() ) { return; }

	clPtr<AppList> appList = GetAppList( _panel->UriOfCurrent().GetUnicode() );

	//if (!appList.data()) return;

	AppMenuData data;
	MenuData mdRes, *md = data.AppendAppList( appList.ptr() );

	if ( !md ) { md = &mdRes; }

	if ( p->IsExe() )
	{
		md->AddCmd( CMD_RC_RUN, _LT( "Execute" ) );
	}

	if ( !md->Count() ) { return; }

	int ret = DoPopupMenu( 0, this, md, point.x, point.y );

	_edit.SetFocus();

	if ( ret == CMD_RC_RUN )
	{
		ExecuteFile();
		return;
	}

	ret -= CMD_RC_OPEN_0;

	if ( ret < 0 || ret >= data.nodeList.count() ) { return; }


#ifndef _WIN32
	if ( !data.nodeList[ret].terminal )
	{
		ExecNoTerminalProcess( data.nodeList[ret].cmd );
		return;
	};

#endif

	StartExecute( data.nodeList[ret].cmd, _panel->GetFS(), _panel->GetPath() );

	return;
}


void NCWin::ReturnToDefaultSysDir()
{
#ifdef _WIN32
	wchar_t buf[4096] = L"";

	if ( GetSystemDirectoryW( buf, 4096 ) > 0 )
	{
		SetCurrentDirectoryW( buf );
	}

#else
	int r = chdir( "/" );
#endif
}

void NCWin::Home( PanelWin* p )
{
#ifdef _WIN32
	std::vector<unicode_t> homeUri;

	//find home
	{
		wchar_t homeDrive[0x100];
		wchar_t homePath[0x100];
		int l1 = GetEnvironmentVariableW( L"HOMEDRIVE", homeDrive, 0x100 );
		int   l2 = GetEnvironmentVariableW( L"HOMEPATH", homePath, 0x100 );

		if ( l1 > 0 && l1 < 0x100 && l2 > 0 && l2 < 0x100 )
		{
			homeUri = carray_cat<unicode_t>( Utf16ToUnicode( homeDrive ).data(), Utf16ToUnicode( homePath ).data() );
		}
	}

	if ( homeUri.data() )
	{
		clPtr<FS> checkFS[2];
		checkFS[0] = p->GetFSPtr();
		checkFS[1] = p == &_leftPanel ? _rightPanel.GetFSPtr() : _leftPanel.GetFSPtr();

		FSPath path;
		clPtr<FS> fs = ParzeURI( homeUri.data(), path, checkFS, 2 );

		if ( fs.IsNull() )
		{
			char buf[4096];
			FSString name = homeUri.data();
			snprintf( buf, sizeof( buf ), "bad home path: %s\n", name.GetUtf8() );
			NCMessageBox( this, "Home", buf, true );
		}
		else
		{
			p->LoadPath( fs, path, 0, 0, PanelWin::SET );
		}
	}

#else
	const sys_char_t* home = ( sys_char_t* ) getenv( "HOME" );

	if ( !home ) { return; }

	FSPath path( sys_charset_id, home );
	p->LoadPath( new FSSys(), path, 0, 0, PanelWin::SET );
#endif
}


void NCWin::StartExecute( const unicode_t* cmd, FS* fs,  FSPath& path )
{
#ifdef _WIN32
	_history.Put( cmd );

	if ( _terminal.Execute( this, 1, cmd, 0, fs->Uri( path ).GetUnicode() ) )
	{
		SetMode( TERMINAL );
	}


#else
	_history.Put( cmd );
	const unicode_t* pref = _editPref.Get();
	static unicode_t empty[] = {0};

	if ( !pref ) { pref = empty; }

	_terminal.TerminalReset();
	unsigned fg = 0xB;
	unsigned bg = 0;
	static unicode_t newLine[] = {'\n', 0};
	_terminal.TerminalPrint( newLine, fg, bg );
	_terminal.TerminalPrint( pref, fg, bg );
	_terminal.TerminalPrint( cmd, fg, bg );
	_terminal.TerminalPrint( newLine, fg, bg );

	int l = unicode_strlen( cmd );
	int i;

	if ( l >= 64 )
	{
		for ( i = 0; i < 64 - 1; i++ ) { _execSN[i] = cmd[i]; }

		_execSN[60] = '.';
		_execSN[61] = '.';
		_execSN[62] = '.';
		_execSN[63] = 0;
	}
	else
	{
		for ( i = 0; i < l; i++ ) { _execSN[i] = cmd[i]; }

		_execSN[l] = 0;
	}

	_terminal.Execute( this, 1, cmd, ( sys_char_t* )path.GetString( sys_charset_id ) );
	SetMode( TERMINAL );
#endif
	ReturnToDefaultSysDir(); //!!!
}


static int uiDriveDlg = GetUiID( "drive-dlg" );

void NCWin::SelectDrive( PanelWin* p, PanelWin* OtherPanel )
{
	if ( _mode != PANEL ) { return; }

#ifndef _WIN32
	ccollect< MntListNode > mntList;
	UxMntList( &mntList );
#endif
	DlgMenuData mData;

	FSString OtherPanelPath = OtherPanel->UriOfDir();

	mData.Add( OtherPanelPath.GetUnicode(), 0, ID_DEV_OTHER_PANEL );
	mData.AddSplitter();

#ifdef _WIN32
	std::vector<unicode_t> homeUri;

	//find home
	{
		wchar_t homeDrive[0x100];
		wchar_t homePath[0x100];
		int l1 = GetEnvironmentVariableW( L"HOMEDRIVE", homeDrive, 0x100 );
		int l2 = GetEnvironmentVariableW( L"HOMEPATH", homePath, 0x100 );

		if ( l1 > 0 && l1 < 0x100 && l2 > 0 && l2 < 0x100 )
		{
			homeUri = carray_cat<unicode_t>( Utf16ToUnicode( homeDrive ).data(), Utf16ToUnicode( homePath ).data() );
		}
	}

	if ( homeUri.data() )
	{
		mData.Add( _LT( "Home" ), 0, ID_DEV_HOME );
	}

	DWORD drv = GetLogicalDrives();

	for ( int i = 0, mask = 1; i < 'z' - 'a' + 1; i++, mask <<= 1 )
		if ( drv & mask )
		{
			char buf[0x100];
			snprintf( buf, sizeof( buf ), "%c:", i + 'A' );
			UINT driveType = GetDriveType( buf );
			const char* typeStr = "";

			switch ( driveType )
			{
				case DRIVE_REMOVABLE:
					typeStr = "removable";
					break;

				case DRIVE_FIXED:
					typeStr = "fixed";
					break;

				case DRIVE_REMOTE:
					typeStr = "remote";
					break;

				case DRIVE_CDROM:
					typeStr = "CDROM";
					break;

				case DRIVE_RAMDISK:
					typeStr = "RAM disk";
					break;
			}

			mData.Add( buf, typeStr, ID_DEV_MS0 + i );
		}

	mData.AddSplitter();
	mData.Add( "NETWORK", 0, ID_DEV_SMB );
#else
	mData.Add( "/", 0,  ID_DEV_ROOT );
	mData.Add( _LT( "Home" ), 0, ID_DEV_HOME );
#ifdef LIBSMBCLIENT_EXIST
	mData.Add( "Smb network", 0, ID_DEV_SMB );
	mData.Add( "Smb server", 0, ID_DEV_SMB_SERVER );
#endif

#endif

#if defined(LIBSSH_EXIST) || defined(LIBSSH2_EXIST)
	mData.Add( "SFTP", 0, ID_DEV_SFTP );
#endif

	mData.Add( "FTP", 0, ID_DEV_FTP );


#ifndef _WIN32  //unix mounts
	//ID_MNT_UX0
	{
		if ( mntList.count() > 0 )
		{
			mData.AddSplitter();
		}

		for ( int i = 0; i < 9 && i < mntList.count(); i++ )
		{
			//ccollect<std::vector<char> > strHeap;

			std::vector<unicode_t> un = sys_to_unicode_array( mntList[i].path.data() );
			static int maxNLen = 20;
			int nLen = unicode_strlen( un.data() );

			if ( nLen > maxNLen )
			{
				int n2 = maxNLen / 2;
				un[n2] = 0;
				un[n2 - 1] = '.';
				un[n2 - 2] = '.';
				un[n2 - 3] = '.';
				un = carray_cat<unicode_t>( un.data(), un.data() + nLen - n2 );
			}

			std::vector<unicode_t> ut = sys_to_unicode_array( mntList[i].type.data() );

			static int maxTLen = 10;

			if ( unicode_strlen( ut.data() ) > maxTLen )
			{
				ut[maxTLen] = 0;
				ut[maxTLen - 1] = '.';
				ut[maxTLen - 2] = '.';
				ut[maxTLen - 3] = '.';
			}

			char buf[64];
			snprintf( buf, sizeof( buf ), "%i ", i + 1 );

			mData.Add( carray_cat<unicode_t>( utf8_to_unicode( buf ).data(), un.data() ).data(), ut.data(), ID_MNT_UX0 + i );
		}
	}
#endif

	int res = RunDldMenu( uiDriveDlg, p, "Drive", &mData );
	_edit.SetFocus();

	if ( res == ID_DEV_OTHER_PANEL )
	{
		clPtr<FS> fs = OtherPanel->GetFSPtr();
		p->LoadPath( fs, OtherPanel->GetPath(), 0, 0, PanelWin::SET );
		return;
	}

#ifdef _WIN32

	if ( res >= ID_DEV_MS0 && res < ID_DEV_MS0 + ( 'z' - 'a' + 1 ) )
	{
		int drive = res - ID_DEV_MS0;
		FSPath path( CS_UTF8, "/" );
		clPtr<FS> fs = _panel->GetFSPtr();

		if ( !fs.IsNull() && fs->Type() == FS::SYSTEM && ( ( FSSys* )fs.Ptr() )->Drive() == drive )
		{
			path = _panel->GetPath();
		}
		else
		{
			PanelWin* p2 = &_leftPanel == _panel ? &_rightPanel : &_leftPanel;
			fs = p2->GetFSPtr();

			if ( !fs.IsNull() && fs->Type() == FS::SYSTEM && ( ( FSSys* )fs.Ptr() )->Drive() == drive )
			{
				path = p2->GetPath();
			}
			else
			{
				fs = 0;
			}
		}

		if ( fs.IsNull() )
		{
			fs = new FSSys( drive );
		}

		if ( !path.IsAbsolute() ) { path.Set( CS_UTF8, "/" ); }

		p->LoadPath( fs, path, 0, 0, PanelWin::SET );
		return;
	}

#else

	if ( res >= ID_MNT_UX0 && res < ID_MNT_UX0 + 100 )
	{
		int n = res - ID_MNT_UX0;

		if ( n < 0 || n >= mntList.count() ) { return; }

		clPtr<FS> fs = new FSSys();
		FSPath path( sys_charset_id, mntList[n].path.data() );
		p->LoadPath( fs, path, 0, 0, PanelWin::SET );
		return;
	}

#endif

	switch ( res )
	{
#ifndef _WIN32

		case ID_DEV_ROOT:
		{
			FSPath path( CS_UTF8, "/" );
			p->LoadPath( new FSSys(), path, 0, 0, PanelWin::SET );
		}
		break;
#endif

		case ID_DEV_HOME:
		{
			Home( p );
		}
		break;

#ifdef _WIN32

		case ID_DEV_SMB:
		{
			FSPath path( CS_UTF8, "/" );
			clPtr<FS> fs = new FSWin32Net( 0 );

			if ( !fs.IsNull() ) { p->LoadPath( fs, path, 0, 0, PanelWin::SET ); }
		}
		break;

#else

#ifdef LIBSMBCLIENT_EXIST

		case ID_DEV_SMB:
		{
			FSPath path( CS_UTF8, "/" );
			clPtr<FS> fs = new FSSmb() ;

			if ( !fs.IsNull() ) { p->LoadPath( fs, path, 0, 0, PanelWin::SET ); }
		}
		break;

		case ID_DEV_SMB_SERVER:
		{
			static FSSmbParam lastParams;
			FSSmbParam params = lastParams;

			if ( !GetSmbLogon( this, params, true ) ) { return; }

			params.isSet = true;
			clPtr<FS> fs = new FSSmb( &params ) ;
			FSPath path( CS_UTF8, "/" );

			if ( !fs.IsNull() )
			{
				lastParams = params;
				p->LoadPath( fs, path, 0, 0, PanelWin::SET );
			}
		}
		break;
#endif
#endif

		case ID_DEV_FTP:
		{
			static FSFtpParam lastParams;
			FSFtpParam params = lastParams;

			if ( !GetFtpLogon( this, params ) ) { return; }

			clPtr<FS> fs = new FSFtp( &params ) ;

			FSPath path( CS_UTF8, "/" );

			if ( !fs.IsNull() )
			{
				lastParams = params;
				p->LoadPath( fs, path, 0, 0, PanelWin::SET );
			}
		}
		break;

#if defined(LIBSSH_EXIST) || defined(LIBSSH2_EXIST)

		case ID_DEV_SFTP:
		{
			static FSSftpParam lastParams;
			FSSftpParam params = lastParams;

			if ( !GetSftpLogon( this, params ) ) { return; }

			params.isSet = true;
			clPtr<FS> fs = new FSSftp( &params ) ;
			FSPath path( CS_UTF8, "/" );

			if ( !fs.IsNull() )
			{
				lastParams = params;
				p->LoadPath( fs, path, 0, 0, PanelWin::SET );
			}
		}
		break;
#endif


	};
}

std::vector<unicode_t>::iterator FindSubstr( const std::vector<unicode_t>::iterator& begin, const std::vector<unicode_t>::iterator& end, const std::vector<unicode_t>& SubStr )
{
	if ( begin >= end ) return end;

	return std::search( begin, end, SubStr.begin(), SubStr.end() );
}

std::vector<unicode_t> MakeCommand( const std::vector<unicode_t>& cmd, const unicode_t* FileName )
{
	std::vector<unicode_t> Result( cmd );
	std::vector<unicode_t> Name = new_unicode_str( FileName );

	if ( Name.size() && Name.back() == 0 ) Name.pop_back();

	std::vector<unicode_t> Mask;
	Mask.push_back( '!' );
	Mask.push_back( '.' );
	Mask.push_back( '!' );	

	auto pos = FindSubstr( Result.begin(), Result.end(), Mask );

	while ( pos != Result.end() )
	{
		pos = Result.erase( pos, pos + Mask.size() );
		size_t idx = pos - Result.begin();
		Result.insert( pos, Name.begin(), Name.end() );
		pos = Result.begin() + idx;
		pos = FindSubstr( pos + Name.size(), Result.end(), Mask );
	}

	return Result;
}

void NCWin::ApplyCommandToList( const std::vector<unicode_t>& cmd, clPtr<FSList> list, PanelWin* Panel )
{
	if ( !cmd.data() ) { return; }
	if ( !list.ptr() || list->Count() <= 0 ) { return; }

	std::vector<FSNode*> nodes = list->GetArray();

	SetMode( TERMINAL );

	for ( auto i = nodes.begin(); i != nodes.end(); i++ )
	{
		FSNode* Node = *i;

		const unicode_t* Name = Node->GetUnicodeName();

		std::vector<unicode_t> Command = MakeCommand( cmd, Name );

		StartExecute( Command.data( ), Panel->GetFS( ), Panel->GetPath( ) );
	}

	SetMode( PANEL );
}

void NCWin::ApplyCommand()
{
	if ( _mode != PANEL ) { return; }

	std::vector<unicode_t> command = InputStringDialog( this, utf8_to_unicode( _LT( "Apply command to the selected files" ) ).data() );

	ApplyCommandToList( command, _panel->GetSelectedList(), _panel );
}

void NCWin::CreateDirectory()
{
	if ( _mode != PANEL ) { return; }

	try
	{
		std::vector<unicode_t> dir = InputStringDialog( this, utf8_to_unicode( _LT( "Create new directory" ) ).data() );

		if ( !dir.data() ) { return; }

		clPtr<FS> checkFS[2];
		checkFS[0] = _panel->GetFSPtr();
		checkFS[1] = GetOtherPanel()->GetFSPtr();

		FSPath path = _panel->GetPath();
		clPtr<FS> fs = ParzeURI( dir.data(), path, checkFS, 2 );

		if ( fs.IsNull() )
		{
			char buf[4096];
			FSString name = dir.data();
			snprintf( buf, sizeof( buf ), _LT( "can`t create directory:%s\n" ),
			          name.GetUtf8() );
			NCMessageBox( this, "CD", buf, true );
			return;
		}

		if ( !::MkDir( fs, path, this ) )
		{
			return;
		}

	}
	catch ( cexception* ex )
	{
		NCMessageBox( this, _LT( "Create directory" ), ex->message(), true );
		ex->destroy();
	};

	_leftPanel.Reread();

	_rightPanel.Reread();
}

void NCWin::QuitQuestion()
{
	if ( NCMessageBox( this, _LT( "Quit" ), _LT( "Do you want to quit?" ), false, bListOkCancel ) == CMD_OK )
	{
		wcmConfig.Save( this );
		AppExit();
	}
}

void NCWin::View()
{
	if ( _mode != PANEL ) { return; }

	try
	{
		FSPath path = _panel->GetPath();
		clPtr<FS> fs = _panel->GetFSPtr();

		clPtr<FSList> list = _panel->GetSelectedList();

		int cur = _panel->Current();

		if ( !cur )
		{
			//calc current dir
			DirCalc( fs, path, list, this );
			_panel->Repaint();
			return;
		};

		FSNode* p =  _panel->GetCurrent();

		if ( !p ) { return; }

		if ( p->IsDir() )
		{
			DirCalc( fs, path, list, this );
			_panel->Repaint();
			return;
		};

		path.Push( p->name.PrimaryCS(), p->name.Get( p->name.PrimaryCS() ) );

		if ( !( fs->Flags() & FS::HAVE_SEEK ) )
		{
			NCMessageBox( this, _LT( "View" ), _LT( "Can`t start viewer in this filesystem" ), true );
			return;
		};

		SetBackgroundActivity( eBackgroundActivity_Viewer );

		SetMode( VIEW );

		_viewer.SetFile( fs, path, p->Size() );

	}
	catch ( cexception* ex )
	{
		NCMessageBox( this, _LT( "View" ), ex->message(), true );
		ex->destroy();
	}
}

void NCWin::ViewExit()
{
	SetBackgroundActivity( eBackgroundActivity_None );

	if ( _mode != VIEW ) { return; }

	//...
	_viewer.ClearFile();
	SetMode( PANEL );
}


static cstrhash<sEditorScrollCtx, unicode_t> editPosHash;


void NCWin::Edit( bool enterFileName )
{
	if ( _mode != PANEL ) { return; }

	try
	{
		FSPath path = _panel->GetPath();;
		clPtr<FS> fs = _panel->GetFSPtr();

		if ( enterFileName )
		{
			static std::vector<unicode_t> savedUri;
			std::vector<unicode_t> uri = InputStringDialog( this, utf8_to_unicode( _LT( "File to edit" ) ).data(), savedUri.data() );

			if ( !uri.data() ) { return; }

			savedUri = new_unicode_str( uri.data() );

			clPtr<FS> cFs[2] = {fs, GetOtherPanel()->GetFSPtr() };

			fs = ParzeURI( uri.data(), path, cFs, 2 );

			if ( !fs.Ptr() ) { return; }

		}
		else
		{
			int cur = _panel->Current();

			if ( !cur ) { return; }

			FSNode* p =  _panel->GetCurrent();

			if ( !p || p->IsDir() ) { return; }

			path.Push( p->name.PrimaryCS(), p->name.Get( p->name.PrimaryCS() ) );
		}

		clPtr<MemFile> file = LoadFile( fs, path, this, enterFileName == true );

		if ( !file.ptr() ) { return; }

		_editor.Load( fs, path, *file.ptr() );


		if ( wcmConfig.editSavePos )
		{
			_editor.SetScrollCtx( editPosHash[fs->Uri( path ).GetUnicode()] );
		}
		else
		{
			_editor.SetCursorPos( EditPoint( 0, 0 ) );
		}

		SetBackgroundActivity( eBackgroundActivity_Editor );

		SetMode( EDIT );

	}
	catch ( cexception* ex )
	{
		NCMessageBox( this, _LT( "Edit" ), ex->message(), true );
		ex->destroy();
	}
}

static bool StrHaveSpace( const unicode_t* s )
{
	for ( ; *s; s++ )
		if ( *s == ' ' )
		{
			return true;
		}

	return false;
}

const unicode_t* NCWin::GetCurrentFileName() const
{
	if ( _panel->IsVisible() )
	{
		const unicode_t* p = _panel->GetCurrentFileName();

		// check for '..'
		if ( p && p[0] == '.' && p[1] == '.' )
		{
			p = _panel->GetPath().GetUnicode();
		}

		return p;
	}

	return NULL;
}

void  NCWin::PasteFileNameToCommandLine( const unicode_t* path )
{
	if ( path )
	{
		bool spaces = StrHaveSpace( path );

		if ( spaces ) { _edit.Insert( '"' ); }

		_edit.Insert( path );

		if ( spaces ) { _edit.Insert( '"' ); }

		_edit.Insert( ' ' );

		NotifyAutoComplete();
	}
}

void NCWin::PastePanelPath( PanelWin* panel )
{
	if ( _mode != PANEL ) { return; }

	if ( panel->IsVisible() )
	{
		FSString S = panel->UriOfDir();

		PasteFileNameToCommandLine( S.GetUnicode() );
	}
}

void NCWin::CtrlEnter()
{
	if ( _mode != PANEL ) { return; }

	if ( _panel->IsVisible() )
	{
		const unicode_t* p = GetCurrentFileName();

		PasteFileNameToCommandLine( p );
	}
}

void NCWin::CtrlF()
{
	if ( _mode != PANEL ) { return; }

	if ( _panel->IsVisible() )
	{
		FSString uri = _panel->UriOfCurrent();
		const unicode_t* str = uri.GetUnicode();
		bool spaces = StrHaveSpace( str );

		if ( spaces ) { _edit.Insert( '"' ); }

		_edit.Insert( str );

		if ( spaces ) { _edit.Insert( '"' ); }

		_edit.Insert( ' ' );

		NotifyAutoComplete();
	}
}


void NCWin::HistoryDialog()
{
	if ( _mode != PANEL ) { return; }

	CmdHistoryDialog dlg( 0, this, _history );
	int r = dlg.DoModal();

	if ( r != CMD_OK ) { return; }

	const unicode_t* s = dlg.Get();

	if ( !s ) { return; }

	_edit.SetText( s );

	NotifyAutoComplete( );
}


void NCWin::Delete()
{
	if ( _mode != PANEL ) { return; }

	clPtr<FS> fs = _panel->GetFSPtr();

	if ( fs.IsNull() ) { return; }

	FSPath path = _panel->GetPath();

	if ( _mode != PANEL ) { return; }

	int cur = _panel->Current();

	clPtr<FSList> list = _panel->GetSelectedList();

	if ( !list.ptr() || list->Count() <= 0 ) { return; }

	int n = list->Count();

	if ( n == 1 )
	{
		if ( NCMessageBox( this, _LT( "Delete" ),
		                   carray_cat<char>( _LT( "Do you wish to delete\n" ), list->First()->Name().GetUtf8() ).data(), false, bListOkCancel ) != CMD_OK )
		{
			return;
		}
	}
	else
	{
		char buf[0x100];
		sprintf( buf, _LT( "You have %i selected files\nDo you want to delete it?" ), n );

		if ( NCMessageBox( this, _LT( "Delete" ), buf, false ) != CMD_OK )
		{
			return;
		}
	}

	try
	{

		DeleteList( fs, path, list, this );
	}
	catch ( cexception* ex )
	{
		NCMessageBox( this, _LT( "Delete" ), ex->message(), true );
		ex->destroy();
	}

	if ( _panel->GetSelectedCounter().count == 0 )
	{
		// there are no multiple items selected in the panel
		_panel->KeyDown( false, 0 );
	}

	_leftPanel.Reread();
	_rightPanel.Reread();
}

void NCWin::Copy( bool shift )
{
	if ( _mode != PANEL ) { return; }

	clPtr<FS> srcFs = _panel->GetFSPtr();

	if ( srcFs.IsNull() ) { return; }

	FSPath srcPath = _panel->GetPath();

	if ( _mode != PANEL ) { return; }

	int cur = _panel->Current();

	clPtr<FSList> list = _panel->GetSelectedList();

	if ( !list.ptr() || list->Count() <= 0 ) { return; }

	clPtr<FS> destFs = GetOtherPanel()->GetFS();
	FSPath destPath = GetOtherPanel()->GetPath();

	FSString uri = GetOtherPanel()->UriOfDir();

	std::vector<unicode_t> str =  InputStringDialog( this, utf8_to_unicode( _LT( "Copy" ) ).data(),
	                                            shift ? _panel->GetCurrentFileName() : uri.GetUnicode() );

	if ( !str.data() || !str[0] ) { return; }

	const unicode_t* a = uri.GetUnicode();
	const unicode_t* b = str.data();

	while ( *a && *b && *a == *b ) { a++; b++; }

	if ( *a != *b )
	{
		clPtr<FS> checkFS[2];
		checkFS[0] = _panel->GetFSPtr();
		checkFS[1] = GetOtherPanel()->GetFSPtr();

		destPath = srcPath;
		destFs = ParzeURI( str.data(), destPath, checkFS, 2 );
	}

	clPtr<cstrhash<bool, unicode_t> > resList = CopyFiles( srcFs, srcPath, list, destFs, destPath, this );

	if ( resList.ptr() )
	{
		_panel->ClearSelection( resList.ptr() );
	}

	_leftPanel.Reread();
	_rightPanel.Reread();
}



void NCWin::Move( bool shift )
{
	if ( _mode != PANEL ) { return; }

	clPtr<FS> srcFs = _panel->GetFSPtr();

	if ( srcFs.IsNull() ) { return; }

	FSPath srcPath = _panel->GetPath();

	if ( _mode != PANEL ) { return; }

	int cur = _panel->Current();

	clPtr<FSList> list = _panel->GetSelectedList();

	if ( !list.ptr() || list->Count() <= 0 ) { return; }

	clPtr<FS> destFs = GetOtherPanel()->GetFS();
	FSPath destPath = GetOtherPanel()->GetPath();

	FSString uri = GetOtherPanel()->UriOfDir();

	std::vector<unicode_t> str =  InputStringDialog( this, utf8_to_unicode( _LT( "Move" ) ).data(),
	                                            shift ? _panel->GetCurrentFileName() : uri.GetUnicode() );

	if ( !str.data() || !str[0] ) { return; }

	const unicode_t* a = uri.GetUnicode();
	const unicode_t* b = str.data();

	while ( *a && *b && *a == *b ) { a++; b++; }

	if ( *a != *b )
	{
		clPtr<FS> checkFS[2];
		checkFS[0] = _panel->GetFSPtr();
		checkFS[1] = GetOtherPanel()->GetFSPtr();

		destPath = srcPath;
		destFs = ParzeURI( str.data(), destPath, checkFS, 2 );
	}

	MoveFiles( srcFs, srcPath, list, destFs, destPath, this );

	_leftPanel.Reread();
	_rightPanel.Reread();
}



void NCWin::Mark( bool enable )
{
	if ( _mode != PANEL ) { return; }

	std::vector<unicode_t> str =  InputStringDialog( this, utf8_to_unicode( _LT( enable ? "Select" : "Deselect" ) ).data(), utf8_to_unicode( "*" ).data() );

	if ( !str.data() || !str[0] ) { return; }

	_panel->Mark( str.data(), enable );
}

void NCWin::OnOffShl()
{
	wcmConfig.editShl = !wcmConfig.editShl;
	_editor.EnableShl( wcmConfig.editShl );
}

void NCWin::PanelEqual()
{
	if ( _mode != PANEL ) { return; }

	PanelWin* a = _panel;
	PanelWin* b = GetOtherPanel();
	b->LoadPath( a->GetFSPtr(), a->GetPath(), 0, 0, PanelWin::SET );
}



void NCWin::SaveSetup()
{
	try
	{
		wcmConfig.Save( this );
	}
	catch ( cexception* ex )
	{
		NCMessageBox( this, _LT( "Save setup" ), ex->message(), true );
		ex->destroy();
	}

}

void NCWin::Search()
{
	if ( _mode != PANEL ) { return; }

	FSPath goPath;

	if ( SearchFile( _panel->GetFSPtr(), _panel->GetPath(), this, &goPath ) )
	{
		if ( goPath.Count() > 0 )
		{
			FSString cur = *goPath.GetItem( goPath.Count() - 1 );
			goPath.Pop();
			_panel->LoadPath( _panel->GetFSPtr(), goPath, &cur, 0, PanelWin::SET );
		}
	}
}


void NCWin::Shortcuts()
{
	if ( _mode != PANEL ) { return; }

	clPtr<FS> ptr = _panel->GetFSPtr();
	FSPath path = _panel->GetPath();

	if ( ShortcutDlg( this, &ptr, &path ) )
	{
		_panel->LoadPath( ptr, path, 0, 0, PanelWin::SET );
	}
}

bool NCWin::EditSave( bool saveAs )
{

	if ( _mode != EDIT ) { return false; }

	try
	{
		FSPath path = _panel->GetPath();;
		clPtr<FS> fs = _panel->GetFSPtr();

		if ( saveAs )
		{
			static std::vector<unicode_t> savedUri;
			std::vector<unicode_t> uri = InputStringDialog( this, utf8_to_unicode( _LT( "Save file as ..." ) ).data(), savedUri.data() );

			if ( !uri.data() ) { return false; }

			savedUri = new_unicode_str( uri.data() );

			clPtr<FS> cFs[2] = {fs, GetOtherPanel()->GetFSPtr() };

			fs = ParzeURI( uri.data(), path, cFs, 2 );

			if ( !fs.Ptr() ) { return false; }

		}
		else
		{
			_editor.GetPath( path );
			fs = _editor.GetFS();
		}

		clPtr<MemFile> file = new MemFile;
		_editor.Save( *file.ptr() );

		if ( SaveFile( fs, path, file, this ) )
		{
			if ( saveAs )
			{
				_editor.SetPath( fs, path );
				SendBroadcast( CMD_NCEDIT_INFO, CMD_NCEDIT_CHANGES, this, 0, 2 );
			}

			_editor.ClearChangedStata();
		}

		return true;
	}
	catch ( cexception* ex )
	{
		NCMessageBox( this, _LT( "Edit" ), ex->message(), true );
		ex->destroy();
	}

	return false;
}

void NCWin::EditCharsetTable()
{
	if ( _mode != EDIT ) { return; }

	int ret;

	if ( SelectOperCharset( this, &ret, _editor.GetCharsetId() ) )
	{
		_editor.SetCharset( ret );
	}
}

void NCWin::EditSearch( bool next )
{
	if ( _mode != EDIT ) { return; }

	if ( ( next || DoSearchDialog( this, &searchParams ) ) && searchParams.txt.data() && searchParams.txt[0] )
	{
		if ( !_editor.Search( searchParams.txt.data(), searchParams.sens ) )
		{
			NCMessageBox( this, _LT( "Search" ), _LT( "String not found" ), true );
		}
	}
}

void NCWin::EditReplace()
{
	if ( _mode != EDIT ) { return; }

	if ( DoReplaceEditDialog( this, &searchParams ) && searchParams.txt.data() && searchParams.txt[0] )
	{
		static unicode_t empty = 0;
		unicode_t* to = searchParams.to.data() && searchParams.to[0] ? searchParams.to.data() : &empty;

		if ( !_editor.Replace( searchParams.txt.data(), to, searchParams.sens ) )
		{
			NCMessageBox( this, _LT( "Replace" ), _LT( "String not found" ), true );
		}
	}
}

void NCWin::ViewSearch( bool next )
{
	if ( _mode != VIEW ) { return; }

	if ( ( next || DoSearchDialog( this, &searchParams ) ) && searchParams.txt.data() && searchParams.txt[0] )
	{
		if ( !_viewer.Search( searchParams.txt.data(), searchParams.sens ) )
		{
			NCMessageBox( this, _LT( "Search" ), _LT( "String not found" ), true );
		}
	}
}

void NCWin::EditExit()
{
	SetBackgroundActivity( eBackgroundActivity_None );

	if ( _mode != EDIT ) { return; }

	clPtr<FS> fs = _editor.GetFS();

	if ( !fs.IsNull() && wcmConfig.editSavePos )
	{
		FSPath path;
		_editor.GetPath( path );
		editPosHash[fs->Uri( path ).GetUnicode()] = _editor.GetScrollCtx();
	}

	if ( _editor.Changed() )
	{
		int ret = NCMessageBox( this, _LT( "Edit" ), _LT( "File has changes\nsave it?" ), true, bListYesNoCancel );

		if ( ret == CMD_NO || ret == CMD_YES && EditSave( false ) )
		{
			_leftPanel.Reread();
			_rightPanel.Reread();
			SetMode( PANEL );
		}
	}
	else
	{
		_leftPanel.Reread();
		_rightPanel.Reread();
		SetMode( PANEL );
	}
}

void NCWin::ViewCharsetTable()
{
	if ( _mode != VIEW ) { return; }

	int ret;

	if ( SelectOperCharset( this, &ret, _viewer.GetCharsetId() ) )
	{
		_viewer.SetCharset( ret );
	}
}


#ifndef _WIN32

void NCWin::ExecNoTerminalProcess( const unicode_t* p )
{
	_history.Put( p );
	const unicode_t* pref = _editPref.Get();
	static unicode_t empty[] = {0};

	if ( !pref ) { pref = empty; }

	_terminal.TerminalReset();
	unsigned fg = 0xB;
	unsigned bg = 0;
	static unicode_t newLine[] = {'\n', 0};
	_terminal.TerminalPrint( newLine, fg, bg );
	_terminal.TerminalPrint( pref, fg, bg );
	_terminal.TerminalPrint( p, fg, bg );
	_terminal.TerminalPrint( newLine, fg, bg );

	SkipSpaces( p );

	if ( !*p ) { return; }

	char* dir = 0;
	FSPath dirPath;


	if ( _panel->GetFS() && _panel->GetFS()->Type() == FS::SYSTEM )
	{
		dirPath = _panel->GetPath();
		dir = ( char* )dirPath.GetString( sys_charset_id );
	}

	FSString s = p;
	sys_char_t* cmd = ( sys_char_t* ) s.Get( sys_charset_id );


	pid_t pid = fork();

	if ( pid < 0 ) { return; }

	if ( pid )
	{
		waitpid( pid, 0, 0 );
	}
	else
	{

		if ( !fork() )
		{
//printf("exec: %s\n", cmd);
			signal( SIGINT, SIG_DFL );
			static char shell[] = "/bin/sh";
			const char* params[] = {shell, "-c", cmd, NULL};

			if ( dir ) if ( chdir( dir ) ) {};

			execv( shell, ( char** ) params );

			exit( 1 );
		}

		exit( 0 );
	}
}
#endif

void NCWin::Tab( bool forceShellTab )
{
	m_AutoCompleteList.Hide();

	if ( _mode != PANEL ) { return; }

	if ( _panelVisible && !forceShellTab )
	{
		_panel =  GetOtherPanel();
		_leftPanel.Invalidate();
		_rightPanel.Invalidate();
	}
	else
	{
		int cursor = _edit.GetCursorPos();
		std::vector<unicode_t> p = ShellTabKey( this,  _panel->GetFSPtr(), _panel->GetPath(), _edit.GetText().data(), &cursor );

		if ( p.data() )
		{
			_edit.SetText( p.data() );
			_edit.SetCursorPos( cursor );
			_edit.Invalidate();
		}
	};
}

void NCWin::CheckKM( bool ctrl, bool alt, bool shift, bool pressed, int ks )
{
	ButtonWinData* data = 0;

	if ( ks == VK_LCONTROL || ks == VK_RCONTROL ) { ctrl = pressed; }

	if ( ks == VK_LSHIFT || ks == VK_RSHIFT ) { shift = pressed; }

	if ( ks == VK_LMENU || ks == VK_RMENU ) { alt = pressed; }

	switch ( _mode )
	{
		case PANEL:
			if ( ctrl )
			{
				data = panelControlButtons;
			}
			else if ( alt )
			{
				data = panelAltButtons;
			}
			else if ( shift )
			{
				data = panelShiftButtons;
			}
			else { data = panelNormalButtons; }

			break;

		case EDIT:
			if ( ctrl )
			{
				data = editCtrlButtons;
			}
			else if ( alt )
			{
				data = 0;
			}
			else if ( shift )
			{
				data = editShiftButtons;
			}
			else { data = editNormalButtons; }

			break;

		case VIEW:
			if ( ctrl )
			{
				data = 0;
			}
			else if ( alt )
			{
				data = 0;
			}
			else if ( shift )
			{
				data = viewShiftButtons;
			}
			else { data = viewNormalButtons; }

			break;

		default:
			return;
	};

	if ( _buttonWin.GetLastData() != data )
	{
		_buttonWin.Set( data );
	}
}

void NCWin::UpdateActivityNotification()
{
	if ( m_BackgroundActivity == eBackgroundActivity_None )
	{
		_activityNotification.Hide();
	}
	else
	{
		_activityNotification.Show();
	}

	if ( _mode != PANEL ) _activityNotification.Hide();
}

void NCWin::SetBackgroundActivity( eBackgroundActivity BackgroundActivity )
{
	m_BackgroundActivity = BackgroundActivity;

	UpdateActivityNotification();
}

void NCWin::SwitchToBackgroundActivity()
{
	m_AutoCompleteList.Hide();

	switch ( m_BackgroundActivity )
	{
	case eBackgroundActivity_None:
		break;
	case eBackgroundActivity_Editor:
		SetMode( EDIT );
		break;
	case eBackgroundActivity_Viewer:
		SetMode( VIEW );
		break;
	}
}

NCCommandLine::NCCommandLine( int nId, Win* parent, const crect* rect, const unicode_t* txt, int chars = 10, bool frame = true )
: EditLine( nId, parent, rect, txt, chars, frame )
{
}

bool NCCommandLine::EventKey( cevent_key* pEvent )
{
	bool Result = EditLine::EventKey( pEvent );

	NCWin* p = ( NCWin* )Parent();

	bool Pressed = pEvent->Type( ) == EV_KEYDOWN;

	if ( p && Pressed ) p->NotifyAutoComplete();
	
	return Result;
}

NCAutocompleteList::NCAutocompleteList( WTYPE t, unsigned hints, int nId, Win* _parent, SelectType st, BorderType bt, crect* rect )
: TextList( t, hints, nId, _parent, st, bt, rect )
{
}

bool NCAutocompleteList::EventKey( cevent_key* pEvent )
{
	bool Result = TextList::EventKey( pEvent );

	NCWin* Win = ( NCWin* )Parent();

	Win->NotifyAutoCompleteChange();

	return Result;
}

bool NCAutocompleteList::EventMouse( cevent_mouse* pEvent )
{
	bool Result = TextList::EventMouse( pEvent );

	NCWin* Win = ( NCWin* )Parent();

	Win->NotifyAutoCompleteChange();

	return Result;
}


std::vector<unicode_t> NCWin::FetchAndClearCommandLine()
{
	std::vector<unicode_t> txt = _edit.GetText();

	_edit.Clear();

	return txt;
}

bool IsCommand_CD( const unicode_t* p )
{
#ifdef _WIN32
	return ( p[0] == 'c' || p[0] == 'C' ) && ( p[1] == 'd' || p[1] == 'D' ) && ( !p[2] || p[2] == ' ' );
#else
	return ( p[0] == 'c' && p[1] == 'd' && ( !p[2] || p[2] == ' ' ) );
#endif
}

bool NCWin::StartCommand( const std::vector<unicode_t>& cmd, bool ForceNoTerminal )
{
	const unicode_t* p = cmd.data();

	SkipSpaces( p );

	printf( "StartCommand %s, %i\n", (const char*)p, (int)ForceNoTerminal );

	if ( !*p ) return false;

	if ( *p )
	{
		if ( IsCommand_CD( p ) )
		{
			// make a mutable copy
			std::vector<unicode_t> copy( cmd );

			unicode_t* p = copy.data();

			//change dir
			_history.Put( p );
			p += 2;

			SkipSpaces( p );

			std::vector<unicode_t> uHome;

			if ( !*p )
			{
				const sys_char_t* home = ( sys_char_t* ) getenv( "HOME" );

				if ( home )
				{
					uHome = sys_to_unicode_array( home );
					p = uHome.data();
				}
			}

			unicode_t* lastNoSpace = 0;

			for ( unicode_t* s = p; *s; s++ )
			{
				if ( *s != ' ' ) { lastNoSpace = s; }
			}

			if ( lastNoSpace ) { lastNoSpace[1] = 0; } //erase last spaces

			clPtr<FS> checkFS[2];
			checkFS[0] = _panel->GetFSPtr();
			checkFS[1] = GetOtherPanel()->GetFSPtr();

			FSPath path = _panel->GetPath();

			ccollect<unicode_t, 0x100> pre;
			int sc = 0;

			while ( *p )
			{
				if ( sc )
				{
					if ( *p == sc ) { sc = 0;  p++; continue; }
				}
				else if ( *p == '\'' || *p == '"' )
				{
					sc = *p;
					p++;
					continue;
				}
#ifndef _WIN32
				if ( *p == '\\' && !sc ) { p++; }
#endif
				if ( !p ) { break; }

				pre.append( *p );
				p++;
			}

			pre.append( 0 );
			p = pre.ptr();

			clPtr<FS> fs = ParzeURI( p, path, checkFS, 2 );

			if ( fs.IsNull() )
			{
				char buf[4096];
				FSString name = p;
				snprintf( buf, sizeof( buf ), _LT( "can`t change directory to:%s\n" ), name.GetUtf8() );
				NCMessageBox( this, "CD", buf, true );
			}
			else
			{
				_panel->LoadPath( fs, path, 0, 0, PanelWin::SET );
			}
		}
		else
		{
#ifndef _WIN32
			if ( p[0] == '&' || ForceNoTerminal )
			{
				_history.Put( p );
				if ( p[0] == '&' ) p++;
				ExecNoTerminalProcess( p );
			}
			else
#endif
			{
				FS* fs = _panel->GetFS();
				if ( fs && fs->Type() == FS::SYSTEM )
				{
					StartExecute( cmd.data(), _panel->GetFS(), _panel->GetPath() );
				}
				else
				{
					NCMessageBox( this, _LT( "Execute" ), _LT( "Can`t execute command in non system fs" ), true );
				}
			}
		}
	}

	return true;
}

bool NCWin::OnKeyDown( Win* w, cevent_key* pEvent, bool pressed )
{
	if ( Blocked() ) { return false; }

	unsigned mod = pEvent->Mod();
	unsigned fullKey = ( pEvent->Key() & 0xFFFF ) + ( mod << 16 );

	bool shift = ( pEvent->Mod() & KM_SHIFT ) != 0;

	if ( !shift ) { _shiftSelectType = -1; }

	bool ctrl = ( pEvent->Mod() & KM_CTRL ) != 0;
	bool alt = ( pEvent->Mod() & KM_ALT ) != 0;

	CheckKM( ctrl, alt, shift, pressed, pEvent->Key() );

	if ( pressed && ctrl &&
	     ( ( _mode == PANEL && _edit.InFocus() && !_panelVisible ) ||
	       ( _mode == TERMINAL ) ) )
	{
		if ( _terminal.Marked() )
		{
			if ( pEvent->Key() == VK_INSERT )
			{
				ClipboardText ct;
				_terminal.GetMarked( ct );
				ClipboardSetText( this, ct );
				_terminal.MarkerClear();
				return true;
			}
		}
	}

//printf("Key='%X'\n",pEvent->Key());

	if ( _mode == PANEL && _edit.InFocus() )
	{
		if ( !pressed ) { return false; }

		if ( pEvent->Key() == VK_TAB && ( pEvent->Mod() & KM_CTRL ) )
		{
			SwitchToBackgroundActivity();
		}

		if ( pEvent->Key() == VK_O && ( pEvent->Mod() & KM_CTRL ) )
		{
			ShowPanels( !_panelVisible );
			return true;
		}

		if ( pEvent->Key() == VK_U && ( pEvent->Mod() & KM_CTRL ) )
		{
			FSPath LeftPath = _leftPanel.GetPath();
			FSPath RightPath = _rightPanel.GetPath();
			clPtr<FS> LeftFS = _leftPanel.GetFSPtr();
			clPtr<FS> RightFS = _rightPanel.GetFSPtr();
			_leftPanel.LoadPath( RightFS, RightPath, 0, 0, PanelWin::SET );
			_rightPanel.LoadPath( LeftFS, LeftPath, 0, 0, PanelWin::SET );
			return true;
		}

		if ( alt && !shift && !ctrl )
		{
			wchar_t c = pEvent->Char();

			if ( c && c >= 0x20 )
			{
				clPtr<cevent_key> key = _panel->QuickSearch( pEvent );
				_edit.SetFocus();

				if ( key.ptr() ) { OnKeyDown( this, key.ptr(), key->Type() == EV_KEYDOWN ); }

				return true;
			}
		}

		if ( _panelVisible )
		{
			switch ( fullKey )
			{
				case FC( VK_DOWN, KM_SHIFT ):
					_panel->KeyDown( shift, &_shiftSelectType );
					return true;
				case VK_DOWN:
					if ( m_AutoCompleteList.IsVisible() )
					{
						m_AutoCompleteList.EventKey( pEvent );
						return true;
					}
					_panel->KeyDown( shift, &_shiftSelectType );
					return true;

				case FC( VK_UP, KM_SHIFT ):
					_panel->KeyUp( shift, &_shiftSelectType );
					return true;
				case VK_UP:
					if ( m_AutoCompleteList.IsVisible() )
					{
						m_AutoCompleteList.EventKey( pEvent );
						return true;
					}
					_panel->KeyUp( shift, &_shiftSelectType );
					return true;

				case FC( VK_END, KM_SHIFT ):
				case VK_END:
					_panel->KeyEnd( shift, &_shiftSelectType );
					return true;

				case FC( VK_HOME, KM_SHIFT ):
				case VK_HOME:
					_panel->KeyHome( shift, &_shiftSelectType );
					return true;

				case FC( VK_PRIOR, KM_SHIFT ):
				case VK_PRIOR:
					_panel->KeyPrior( shift, &_shiftSelectType );
					return true;

				case FC( VK_PRIOR, KM_CTRL ):
					if ( !_panel->DirUp() )
					{
						SelectDrive( _panel, GetOtherPanel() );
					}

					return true;

				case FC( VK_NEXT,  KM_CTRL ):
					_panel->DirEnter();
					return true;

				case FC( VK_NEXT, KM_SHIFT ):
				case VK_NEXT:
					_panel->KeyNext( shift, &_shiftSelectType );
					return true;

				case FC( VK_LEFT, KM_SHIFT ):
				case VK_LEFT:
					_panel->KeyLeft( shift, &_shiftSelectType );
					return true;

				case FC( VK_RIGHT, KM_SHIFT ):
				case VK_RIGHT:
					_panel->KeyRight( shift, &_shiftSelectType );
					return true;

				case VK_ADD:
					Mark( true );
					return true;

				case VK_SUBTRACT:
					Mark( false );
					return true;

				case VK_MULTIPLY:
					_panel->Invert();
					return true;

				case FC( VK_1, KM_CTRL ):
					_panel->SetViewMode( PanelWin::BRIEF );
					return true;

				case FC( VK_2, KM_CTRL ):
					_panel->SetViewMode( PanelWin::MEDIUM );
					return true;

				case FC( VK_3, KM_CTRL ):
					_panel->SetViewMode( PanelWin::TWO_COLUMNS );
					return true;

				case FC( VK_4, KM_CTRL ):
					_panel->SetViewMode( PanelWin::FULL );
					return true;

				case FC( VK_5, KM_CTRL ):
					_panel->SetViewMode( PanelWin::FULL_ST );
					return true;

				case FC( VK_6, KM_CTRL ):
					_panel->SetViewMode( PanelWin::FULL_ACCESS );
					return true;

				case FC( VK_F1, KM_SHIFT ):
				case FC( VK_F1, KM_ALT ):
					SelectDrive( &_leftPanel, &_rightPanel );
					return true;

				case FC( VK_F2, KM_SHIFT ):
				case FC( VK_F2, KM_ALT ):
					SelectDrive( &_rightPanel, &_leftPanel );
					return true;

				case FC( VK_F7, KM_SHIFT ):
				case FC( VK_F7, KM_ALT ):
					Search();
					return true;

				case FC( VK_OEM_PLUS, KM_CTRL ):
					PanelEqual();
					return true;

				case FC( VK_H, KM_CTRL ):
					wcmConfig.panelShowHiddenFiles = !wcmConfig.panelShowHiddenFiles;
					SendConfigChanged();
					return true;

				case FC( VK_SLASH, KM_CTRL ):
				case FC( VK_BACKSLASH, KM_CTRL ):
				case FC( VK_DIVIDE, KM_CTRL ):
					_panel->DirRoot();
					return true;

				case FC( VK_GRAVE, KM_CTRL ):
					Home( _panel );
					break;
			}
		}


		switch ( fullKey )
		{

			case FC( VK_X, KM_CTRL ):
			case VK_DOWN:
				if ( m_AutoCompleteList.IsVisible() )
				{
					m_AutoCompleteList.EventKey( pEvent );
					break;
				}
				_edit.SetText( _history.Next() );
				break;

			case FC( VK_E, KM_CTRL ):
			case VK_UP:
				if ( m_AutoCompleteList.IsVisible() )
				{
					m_AutoCompleteList.EventKey( pEvent );
					break;
				}
				_edit.SetText( _history.Prev() );
				break;

			case VK_INSERT:
				_panel->KeyIns();
				break;

			case FC( VK_INSERT, KM_CTRL ):
			{
				ClipboardText ct;

				if ( _edit.IsVisible() && !_edit.IsEmpty() )
				{
					// command line is not empty - copy it to clipboard
					std::vector<unicode_t> txt = _edit.GetText();
					const unicode_t* p = txt.data();

					if ( p )
					{
						ct.AppendUnicodeStr( p );
						ClipboardSetText( this, ct );
					}
				}
				else
				{
					const unicode_t* p = GetCurrentFileName();

					if ( p )
					{
						ct.AppendUnicodeStr( p );
						ClipboardSetText( this, ct );
					}
				}

				return true;
			}

			case VK_ESCAPE:
				if ( m_AutoCompleteList.IsVisible() )
				{
					m_AutoCompleteList.Hide();
				}
				else if ( wcmConfig.systemEscPanel )
				{
					if ( _edit.IsVisible() && !_edit.IsEmpty() )
					{
						// if the command line is not empty - clear it
						_edit.Clear();
					}
					else if ( wcmConfig.systemEscPanel )
					{
						ShowPanels( !_panelVisible );
						break;
					}
				}
				break;

			case FC( VK_ESCAPE, KM_SHIFT ):
			case FC( VK_ESCAPE, KM_CTRL ):
			case FC( VK_ESCAPE, KM_ALT ):
				m_AutoCompleteList.Hide();

				if ( !_edit.InFocus() )
				{
					return false;
				}

				_edit.Clear();
				break;

			case FC( VK_D, KM_CTRL ):
				Shortcuts();
				break;

			case FC( VK_F, KM_CTRL ):
				CtrlF();
				break;

			case FC( VK_J, KM_CTRL ):
				CtrlEnter();
				break;

			case FC( VK_K, KM_CTRL ):
				HistoryDialog();
				break;

			case FC( VK_R, KM_CTRL ):
				_panel->Reread();
				_panel->Invalidate();
				break;

			case FC( VK_S, KM_CTRL ):
			{
				clPtr<cevent_key> key = _panel->QuickSearch( 0 );
				_edit.SetFocus();

				if ( key.ptr() ) { OnKeyDown( this, key.ptr(), key->Type() == EV_KEYDOWN ); }
			}

			return true;

			case VK_NEXT:
				_terminal.PageDown();
				break;

			case VK_PRIOR:
				_terminal.PageUp();
				break;

			case FC( VK_NUMPAD_RETURN, KM_CTRL ):
			case FC( VK_RETURN, KM_CTRL ):
				if ( _edit.IsVisible() )
				{
					CtrlEnter();
				}

				break;

			case FC( VK_BRACKETLEFT, KM_CTRL ):
			{
				if ( _edit.IsVisible() )
				{
					PastePanelPath( &_leftPanel );
				}

				break;
			}

			case FC( VK_BRACKETRIGHT, KM_CTRL ):
			{
				if ( _edit.IsVisible() )
				{
					PastePanelPath( &_rightPanel );
				}

				break;
			}

			case VK_TAB:
				Tab( false );
				break;

			case FC( VK_TAB, KM_SHIFT ):
				Tab( true );
				break;

			case FC( VK_NUMPAD_RETURN, KM_SHIFT ):
			case FC( VK_RETURN, KM_SHIFT ):
			{
				m_AutoCompleteList.Hide();
				if ( _edit.IsVisible() )
				{
					if ( StartCommand( FetchAndClearCommandLine(), true ) ) break;
				}
				if ( _panelVisible ) PanelEnter();
				break;
			}
			case VK_NUMPAD_RETURN:
			case VK_RETURN:
			{
				m_AutoCompleteList.Hide();
				if ( _edit.IsVisible() )
				{
					if ( StartCommand( FetchAndClearCommandLine(), false ) ) break;
				}
				if ( _panelVisible ) PanelEnter();
				break;
			}

			case VK_F1:
				Help( this, "main" );
				break;

			case FC( VK_F3, KM_CTRL ):
				_panel->SortByName();
				break;

			case FC( VK_F4, KM_CTRL ):
				_panel->SortByExt();
				break;

			case FC( VK_F5, KM_CTRL ):
				_panel->SortByMTime();
				break;

			case FC( VK_F6, KM_CTRL ):
				_panel->SortBySize();
				break;

			case FC( VK_F7, KM_CTRL ):
				_panel->DisableSort();
				break;

			case FC( VK_F4, KM_SHIFT ):
				Edit( true );
				break;

			case FC( VK_F9, KM_SHIFT ):
				SaveSetup();
				break;

			case VK_NUMPAD_CENTER:
			case VK_F3:
				View();
				break;

			case VK_F4:
				Edit( false );
				break;

			case VK_F5:
				Copy( false );
				break;

			case FC( VK_F5, KM_SHIFT ):
				Copy( true );
				break;

			case VK_F6:
				Move( false );
				break;

			case FC( VK_F6, KM_SHIFT ):
				Move( true );
				break;

			case VK_F7:
				CreateDirectory();
				break;

			case FC( VK_G, KM_CTRL ):
				ApplyCommand();
				break;

			case FC( VK_F8, KM_ALT ):
			case FC( VK_F8, KM_SHIFT ):
				HistoryDialog();
				break;

			case VK_F8:
				Delete();
				break;

			case VK_DELETE:
			{
				if ( !_edit.IsVisible() || _edit.IsEmpty() )
				{
					Delete();
					return true;
				}

				return false;
			}

			case VK_F9:
				_menu.SetFocus();
				break;

			case VK_F10:
				QuitQuestion();
				break;

			case VK_BACK:
				if ( wcmConfig.systemBackSpaceUpDir )
				{
					if ( !_edit.IsVisible() || _edit.IsEmpty() )
					{
						_panel->DirUp();
						return true;
					}
				}

				return false;

			default:
				return false;
		}

		return true;
	}
	else if ( _mode == TERMINAL )
	{
#ifndef _WIN32

		if ( !pressed ) { return false; }

#endif

		switch ( fullKey )
		{
			case FC( VK_O, KM_CTRL ):
			case VK_ESCAPE:
			{
				m_AutoCompleteList.Hide();
				if ( pressed ) SwitchToBackgroundActivity();					
				return true;
			}

			case FC( VK_INSERT, KM_SHIFT ):
				if ( pressed ) { _terminal.Paste(); }

				return true;
#ifdef _WIN32

			case FC( VK_C, KM_ALT | KM_CTRL ):
			{
				if ( NCMessageBox( this, _LT( "Stop" ), _LT( "Drop current console?" ) , false, bListOkCancel ) == CMD_OK )
				{
					_terminal.DropConsole();
				}
			}

			return true;
#else

			case FC( VK_C, KM_ALT | KM_CTRL ):
				if ( _execId > 0 )
				{
					int ret = KillCmdDialog( this, _execSN );

					if ( _execId > 0 )
					{
						if ( ret == CMD_KILL_9 ) { kill( _execId, SIGKILL ); }
						else if ( ret == CMD_KILL ) { kill( _execId, SIGTERM ); }
					}
				}

				return true;
#endif
		}

#ifdef _WIN32
		_terminal.Key( pEvent );
#else
		_terminal.Key( pEvent->Key(), pEvent->Char() );
#endif
		return true;
	}
	else

		if ( _mode == EDIT )
		{
			if ( !pressed ) { return false; }

			switch ( fullKey )
			{
				case FC( VK_TAB, KM_CTRL ):
					SetMode( PANEL );
					break;
				case FC( VK_O, KM_CTRL ):
					SetMode( TERMINAL );
					break;
				case VK_F4:
				case VK_F10:
				case VK_ESCAPE:
					EditExit();
					break;

				case FC( VK_G, KM_CTRL ):
				case FC( VK_F8, KM_ALT ):
				{
					int n = GoToLineDialog( this );

					if ( n > 0 ) { _editor.GoToLine( n - 1 ); }

					break;
				}

				case VK_F1:
					Help( this, "edit" );
					break;

				case VK_F2:
					EditSave( false );
					break;

				case FC( VK_H, KM_CTRL ):
					OnOffShl();
					break;

				case FC( VK_F2, KM_SHIFT ):
					EditSave( true );
					break;

				case VK_F7:
					EditSearch( false );
					break;

				case FC( VK_F7, KM_SHIFT ):
					EditSearch( true );
					break;

				case FC( VK_F7, KM_CTRL ):
					EditReplace();
					break;

				case VK_F8:
					_editor.NextCharset();
					break;

				case FC( VK_F8, KM_SHIFT ):
					EditCharsetTable();
					break;

				default:
					return false;
			}

			return true;

		}
		else

			if ( _mode == VIEW )
			{
				if ( !pressed ) { return false; }

				switch ( fullKey )
				{
					case FC( VK_O, KM_CTRL ):
						SetMode( TERMINAL );
						break;
					case VK_F3:
					case VK_F10:
					case VK_ESCAPE:
						ViewExit();
						break;

					case VK_F1:
						Help( this, "view" );
						break;

					case VK_F2:
						_viewer.WrapUnwrap();
						break;

					case VK_F4:
						_viewer.HexText();
						break;

					case VK_F7:
						ViewSearch( false );
						break;

					case VK_SPACE:
					case FC( VK_F7, KM_SHIFT ):
						ViewSearch( true );
						break;

					case VK_F8:
						_viewer.NextCharset();
						break;

					case FC( VK_F8, KM_SHIFT ):
						ViewCharsetTable();
						break;

					default:
						return false;
				}

				return true;

			}


	return false;
}

bool NCWin::EventChildKey( Win* child, cevent_key* pEvent )
{
	return  OnKeyDown( child, pEvent, pEvent->Type() == EV_KEYDOWN );
}

bool NCWin::EventKey( cevent_key* pEvent )
{
	return  OnKeyDown( this, pEvent, pEvent->Type() == EV_KEYDOWN );
}

void NCWin::ThreadSignal( int id, int data )
{
	if ( id == 1 ) { _execId = data; }
}

void NCWin::ThreadStopped( int id, void* data )
{
	_execId = -1;
	_execSN[0] = 0;
	SetMode( PANEL );
	_leftPanel.Reread();
	_rightPanel.Reread();
}

bool NCWin::Command( int id, int subId, Win* win, void* data )
{
	if ( id == 0 ) { return true; }

	if ( _mode == PANEL )
	{
		_edit.SetFocus();

		switch ( id )
		{
			case ID_HELP:
				Help( this, "main" );
				return true;

			case ID_MKDIR:
				CreateDirectory();
				return true;

			case ID_VIEW:
				View();
				return true;

			case ID_EDIT:
				Edit( false );
				return true;

			case ID_EDIT_INP:
				Edit( true );
				return true;

			case ID_COPY:
				Copy( false );
				return true;

			case ID_COPY_SHIFT:
				Copy( true );
				return true;

			case ID_MOVE:
				Move( false );
				return true;

			case ID_MOVE_SHIFT:
				Move( true );
				return true;

			case ID_DELETE:
				Delete();
				return true;

			case ID_MENU:
				_menu.SetFocus();
				return false;

			case ID_QUIT:
				QuitQuestion();
				return true;

			case ID_SORT_BY_NAME:
				_panel->SortByName();
				return true;

			case ID_SORT_BY_EXT:
				_panel->SortByExt();
				return true;

			case ID_SORT_BY_SIZE:
				_panel->SortBySize();
				return true;

			case ID_SORT_BY_MODIF:
				_panel->SortByMTime();
				return true;

			case ID_UNSORT:
				_panel->DisableSort();
				return true;

			case ID_SORT_BY_NAME_L:
				_leftPanel.SortByName();
				return true;

			case ID_SORT_BY_EXT_L:
				_leftPanel.SortByExt();
				return true;

			case ID_SORT_BY_MODIF_L:
				_leftPanel.SortByMTime();
				return true;

			case ID_SORT_BY_SIZE_L:
				_leftPanel.SortBySize();
				return true;

			case ID_UNSORT_L:
				_leftPanel.DisableSort();
				return true;

			case ID_SORT_BY_NAME_R:
				_rightPanel.SortByName();
				return true;

			case ID_SORT_BY_EXT_R:
				_rightPanel.SortByExt();
				return true;

			case ID_SORT_BY_MODIF_R:
				_rightPanel.SortByMTime();
				return true;

			case ID_SORT_BY_SIZE_R:
				_rightPanel.SortBySize();
				return true;

			case ID_UNSORT_R:
				_rightPanel.DisableSort();
				return true;

			case ID_GROUP_SELECT:
				Mark( true );
				return true;

			case ID_GROUP_UNSELECT:
				Mark( false );
				return true;

			case ID_GROUP_INVERT:
				_panel->Invert();
				return true;

			case ID_SEARCH_2:
				this->Search();
				return true;

			case ID_CTRL_O:
				ShowPanels( !_panelVisible );
				return true;

			case ID_HISTORY:
				HistoryDialog();
				return true;

			case ID_PANEL_EQUAL:
				PanelEqual();
				return true;

			case ID_SHORTCUTS:
				Shortcuts();
				return true;

			case ID_REFRESH:
				_panel->Reread();
				_panel->Invalidate();
				return true;

			case ID_PANEL_BRIEF_L:
				_leftPanel.SetViewMode( PanelWin::BRIEF );
				return true;

			case ID_PANEL_MEDIUM_L:
				_leftPanel.SetViewMode( PanelWin::MEDIUM );
				return true;

			case ID_PANEL_TWO_COLUMNS_L:
				_leftPanel.SetViewMode( PanelWin::TWO_COLUMNS );
				return true;

			case ID_PANEL_FULL_L:
				_leftPanel.SetViewMode( PanelWin::FULL );
				return true;

			case ID_PANEL_FULL_ST_L:
				_leftPanel.SetViewMode( PanelWin::FULL_ST );
				return true;

			case ID_PANEL_FULL_ACCESS_L:
				_leftPanel.SetViewMode( PanelWin::FULL_ACCESS );
				return true;

			case ID_PANEL_BRIEF_R:
				_rightPanel.SetViewMode( PanelWin::BRIEF );
				return true;

			case ID_PANEL_MEDIUM_R:
				_rightPanel.SetViewMode( PanelWin::MEDIUM );
				return true;

			case ID_PANEL_TWO_COLUMNS_R:
				_rightPanel.SetViewMode( PanelWin::TWO_COLUMNS );
				return true;

			case ID_PANEL_FULL_R:
				_rightPanel.SetViewMode( PanelWin::FULL );
				return true;

			case ID_PANEL_FULL_ST_R:
				_rightPanel.SetViewMode( PanelWin::FULL_ST );
				return true;

			case ID_PANEL_FULL_ACCESS_R:
				_rightPanel.SetViewMode( PanelWin::FULL_ACCESS );
				return true;

			case ID_DEV_SELECT_LEFT:
				SelectDrive( &_leftPanel, &_rightPanel );
				return true;

			case ID_DEV_SELECT_RIGHT:
				SelectDrive( &_rightPanel, &_leftPanel );
				return true;

			case ID_CONFIG_SYSTEM:
				if ( DoSystemConfigDialog( this ) ) { SendConfigChanged(); }

				break;

			case ID_CONFIG_PANEL:
				if ( DoPanelConfigDialog( this ) ) { SendConfigChanged(); }

				break;

			case ID_CONFIG_EDITOR:
				if ( DoEditConfigDialog( this ) ) { SendConfigChanged(); }

				break;

			case ID_CONFIG_TERMINAL:
				if ( DoTerminalConfigDialog( this ) ) { SendConfigChanged(); }

				break;

			case ID_CONFIG_STYLE:
				if ( DoStyleConfigDialog( this ) )
				{
					if ( wcmConfig.showToolBar )
					{
						_toolBar.Show( SHOW_INACTIVE );
					}
					else
					{
						_toolBar.Hide();
					}

					if ( wcmConfig.showButtonBar )
					{
						_buttonWin.Show( SHOW_INACTIVE );
					}
					else
					{
						_buttonWin.Hide();
					}

					InitFonts();
					SendConfigChanged();
					StylesChanged( this );
				};

				break;

			case ID_CONFIG_SAVE:
				SaveSetup();
				return true;

		};

		return true;
	}
	else if ( _mode == EDIT )
	{
		switch ( id )
		{
			case ID_HELP:
				Help( this, "edit" );
				return true;

			case ID_SAVE:
				EditSave( false );
				return true;

			case ID_SAVE_AS:
				EditSave( true );
				return true;

			case ID_CHARSET:
				_editor.NextCharset();
				return true;

			case ID_CHARSET_TABLE:
				EditCharsetTable();
				return true;

			case ID_GOTO_LINE:
			{
				int n = GoToLineDialog( this );

				if ( n > 0 ) { _editor.GoToLine( n - 1 ); }

				break;
			}

			case ID_SEARCH_2:
			case ID_SEARCH_TEXT:
				EditSearch( false );
				return true;

			case ID_REPLACE_TEXT:
				EditReplace();
				return true;

			case ID_UNDO:
				_editor.Undo();
				return true;

			case ID_REDO:
				_editor.Redo();
				return true;

			case ID_QUIT:
				EditExit();
				return true;
		};

		return true;
	}
	else if ( _mode == VIEW )
	{
		switch ( id )
		{
			case ID_HELP:
				Help( this, "view" );
				return true;

			case ID_CHARSET:
				_viewer.NextCharset();
				return true;

			case ID_CHARSET_TABLE:
				ViewCharsetTable();
				return true;

			case ID_WRAP:
				_viewer.WrapUnwrap();
				return true;

			case ID_HEX:
				_viewer.HexText();
				return true;

			case ID_SEARCH_2:
			case ID_SEARCH_TEXT:
				ViewSearch( false );
				return true;

			case ID_QUIT:
				ViewExit();
				return true;
		};

		return true;
	}


	return false;
}

NCWin::~NCWin() {}


////////////////////////  ButtonWin

/*
   //для переводчика
   _LT("BB>Help")
   _LT("BB>View")
   _LT("BB>Edit")
   _LT("BB>Copy")
   _LT("BB>Move")
   _LT("BB>MkDir")
   _LT("BB>Delete")
   _LT("BB>Menu")
   _LT("BB>Quit")
   _LT("BB>Name")
   _LT("BB>Extens")
   _LT("BB>Modif")
   _LT("BB>Size")
   _LT("BB>Unsort")
   _LT("BB>Left")
   _LT("BB>Right")
   _LT("BB>Edit...")
   _LT("BB>Copy")
   _LT("BB>Rename")
   _LT("BB>Save")
   _LT("BB>Help")
   _LT("BB>Save")
   _LT("BB>Exit")
   _LT("BB>Search")
   _LT("BB>Charset")
   _LT("BB>Exit")
   _LT("BB>Table")
   _LT("BB>Replace")
   _LT("BB>Help")
   _LT("BB>Wrap/Un...")
   _LT("BB>Exit")
   _LT("BB>Hex/Text")
   _LT("BB>Search")
   _LT("BB>Charset")
   _LT("BB>Exit")
   _LT("BB>Table")
*/

ButtonWinData panelNormalButtons[] =
{
	{"Help", ID_HELP },
	{"", 0}, //{"UserMn", ID_USER_MENU},
	{"View", ID_VIEW},
	{"Edit", ID_EDIT},
	{"Copy", ID_COPY},
	{"Move", ID_MOVE},
	{"MkDir", ID_MKDIR},
	{"Delete", ID_DELETE},
	{"Menu", ID_MENU},
	{"Quit", ID_QUIT},
	{0}
};

ButtonWinData panelControlButtons[] =
{
	{"", 0}, //{L"Left", ID_LEFT },
	{"", 0}, //{L"Right", ID_RIGHT},
	{"Name", ID_SORT_BY_NAME},
	{"Extens", ID_SORT_BY_EXT},
	{"Modif", ID_SORT_BY_MODIF},
	{"Size", ID_SORT_BY_SIZE},
	{"Unsort", ID_UNSORT},
	{"", 0},
	{"", 0},
	{"", 0},
	{0}
};

ButtonWinData panelAltButtons[] =
{
	{"Left", 0},
	{"Right", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"Find", ID_SEARCH_2},
	{"History", ID_HISTORY},
	{"", 0},
	{"", 0},
	{0}
};

ButtonWinData panelShiftButtons[] =
{
	{"Left",  ID_DEV_SELECT_LEFT},
	{"Right", ID_DEV_SELECT_RIGHT},
	{"", 0},
	{"Edit...", ID_EDIT_INP},
	{"Copy", ID_COPY_SHIFT},
	{"Rename", ID_MOVE_SHIFT},
	{"", 0},
	{"", 0},
	{"Save", ID_CONFIG_SAVE},
	{"", 0},
	{"", 0},
	{0}
};


ButtonWinData editNormalButtons[] =
{
	{"Help", ID_HELP },
	{"Save", ID_SAVE},
	{"", 0},
	{"Exit", ID_QUIT},
	{"", 0},
	{"", 0},
	{"Search", ID_SEARCH_TEXT},
	{"Charset", ID_CHARSET},
	{"", 0},
	{"Exit", ID_QUIT},
	{0}
};


ButtonWinData editShiftButtons[] =
{
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"Table", ID_CHARSET_TABLE},
	{"", 0},
	{"", 0},
	{0}
};

ButtonWinData editCtrlButtons[] =
{
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"Replace", ID_REPLACE_TEXT},
	{"", 0},
	{"", 0},
	{"", 0},
	{0}
};


ButtonWinData viewNormalButtons[] =
{
	{"Help", ID_HELP },
	{"Wrap/Un...", ID_WRAP},
	{"Exit", ID_QUIT},
	{"Hex/Text", ID_HEX},
	{"", 0},
	{"", 0},
	{"Search", ID_SEARCH_TEXT},
	{"Charset", ID_CHARSET},
	{"", 0},
	{"Exit", ID_QUIT},
	{0}
};

ButtonWinData viewShiftButtons[] =
{
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"", 0},
	{"Table", ID_CHARSET_TABLE},
	{"", 0},
	{"", 0},
	{0}
};


static unicode_t NN1[] = {'1', 0};
static unicode_t NN2[] = {'2', 0};
static unicode_t NN3[] = {'3', 0};
static unicode_t NN4[] = {'4', 0};
static unicode_t NN5[] = {'5', 0};
static unicode_t NN6[] = {'6', 0};
static unicode_t NN7[] = {'7', 0};
static unicode_t NN8[] = {'8', 0};
static unicode_t NN9[] = {'9', 0};
static unicode_t NN10[] = {'1', '0', 0};
static unicode_t* BWNums[] = {NN1, NN2, NN3, NN4, NN5, NN6, NN7, NN8, NN9, NN10, 0};

void ButtonWin::OnChangeStyles()
{
	int i;

	for ( i = 0; i < 10; i++ )
	{
		Win* w = _buttons[i].ptr();

		LSize ls = w->GetLSize();
		ls.x.maximal = 1000;
		ls.x.minimal = 10;
		w->SetLSize( ls );
	}

	wal::GC gc( ( Win* )0 );
	gc.Set( dialogFont.ptr() );
	cpoint maxW( 1, 1 );

	for ( i = 0; i < 10 && BWNums[i]; i++ )
	{
		cpoint pt = _nSizes[i] = gc.GetTextExtents( BWNums[i] );

		if ( maxW.x < pt.x ) { maxW.x = pt.x; }

		if ( maxW.y < pt.y ) { maxW.y = pt.y; }

		_nSizes[i] = pt;
	}

	for ( i = 0; i < 10; i++ )
	{
		_lo.AddRect( &( _rects[i] ), 0, i * 2 );
		_lo.ColSet( i * 2, maxW.x + 2 );
	}

	SetLSize( _lo.GetLSize() );
}

ButtonWin::ButtonWin( Win* parent )
	:  Win( WT_CHILD, 0, parent ),
	   _lo( 1, 20 )
{
	int i;

	for ( i = 0; i < 10; i++ )
	{
		static unicode_t emptyStr[] = {' ', 0};
		_buttons[i] = new Button( 0, this, emptyStr, 0, 0 ); //, 12, 12);
		Win* w = _buttons[i].ptr();
		w->SetTabFocusFlag( false );
		w->SetClickFocusFlag( false );
		w->Show();
		_lo.AddWin( w, 0, i * 2 + 1 );
	}

	SetLayout( &_lo );
	OnChangeStyles();
};

void ButtonWin::Set( ButtonWinData* list )
{
	lastData = list;

	if ( !list )
	{
		for ( int i = 0; i < 10; i++ )
		{
			static unicode_t s[] = {' ', 0};
			_buttons[i]->Set( s, 0 );
			_buttons[i]->Enable( false );
		}
	}
	else
	{
		for ( int i = 0; i < 10 && list->txt; i++, list++ )
		{
			_buttons[i]->Set( utf8_to_unicode( _LT( carray_cat<char>( "BB>", list->txt ).data(), list->txt ) ).data(), list->commandId ); //, 12, 12);
			_buttons[i]->Enable( list->commandId != 0 );
		}
	}
}

int uiClassButtonWin = GetUiID( "ButtonWin" );

int ButtonWin::UiGetClassId()
{
	return uiClassButtonWin;
}

void ButtonWin::Paint( wal::GC& gc, const crect& paintRect )
{
	crect r = ClientRect();
	gc.SetFillColor( UiGetColor( uiBackground, 0, 0, 0xFFFFFF ) );
	gc.FillRect( r );
	gc.SetTextColor( UiGetColor( uiColor, 0, 0, 0 ) );

	gc.Set( dialogFont.ptr() );

	for ( int i = 0; i < 10; i++ )
	{
		gc.TextOutF(
		   _rects[i].left + ( _rects[i].Width() - _nSizes[i].x ) / 2,
		   _rects[i].top + ( _rects[i].Height() - _nSizes[i].y ) / 2,
		   BWNums[i] );
	}
}

ButtonWin::~ButtonWin() {}


/////////////////////////////////////////// StringWin

int uiClassStringWin = GetUiID( "StringWin" );
int StringWin::UiGetClassId() { return uiClassStringWin; }

StringWin::StringWin( Win* parent )
	: Win( WT_CHILD, 0, parent, 0 ), textSize( 0, 0 )
{
}

void StringWin::OnChangeStyles()
{
	if ( !text.data() )
	{
		SetLSize( LSize( cpoint( 0, 0 ) ) );
		return;
	}

	defaultGC->Set( GetFont() );
	textSize = defaultGC->GetTextExtents( text.data() );
	LSize ls( textSize );
	ls.y.maximal = textSize.y;
	ls.x.maximal = textSize.x;
	SetLSize( ls );
}

void StringWin::Paint( wal::GC& gc, const crect& paintRect )
{
	gc.Set( GetFont() );
	crect r = ClientRect();
	gc.SetFillColor( UiGetColor( uiBackground, 0, 0, 0 ) );
	gc.FillRect( r );
	gc.SetTextColor( UiGetColor( uiColor, 0, 0, 0xFFFFFF ) );
	gc.TextOutF( 0, ( r.Height() - textSize.y ) / 2, text.data() );
}

void StringWin::Set( const unicode_t* txt )
{
	if ( !txt )
	{
		text.clear();
		SetLSize( LSize( cpoint( 0, 0 ) ) );
		return;
	}

	text = new_unicode_str( txt );
	OnChangeStyles();
}

StringWin::~StringWin() {}

/////////////////////////////////////////// EditorHeadWin

static int uiPrefixColor = GetUiID( "prefix-color" );
static int uiCSColor = GetUiID( "cs-color" );


static void _DrawUnicode( wal::GC& gc, const crect& rect, const unicode_t* s, int fg, int bg )
{
	gc.SetFillColor( bg );
	gc.FillRect( rect );
	gc.SetTextColor( fg );
	gc.TextOutF( rect.left, rect.top, s );
}

void EditorHeadWin::OnChangeStyles()
{
	wal::GC gc( this );
	gc.Set( dialogFont.ptr() ); //GetFont());
	cpoint p = gc.GetTextExtents( ABCString );
	chW = p.x /= ABCStringLen;
	chH = p.y;
	p.y += 6;
	LSize lSize( p );
	lSize.x.maximal = 10000;
	SetLSize( lSize );
	prefixWidth = gc.GetTextExtents( prefixString.Str() ).x;
}

void EditorHeadWin::CheckSize()
{
	crect r = ClientRect();
	int y = 3;
	int x = 3;
	prefixRect.Set( x, y, x + prefixWidth, y + chH );

	int xr = r.right - 3;
	int w = chW * 16;
	posRect.Set( xr - w, y, xr, y + chH );
	xr -= w;

	w = chW * 15;
	csRect.Set( xr - w, y, xr, y + chH );
	xr -= w;

	w = chW * 10;
	symRect.Set( xr - w, y, xr, y + chH );
	xr -= w;

	nameRect.Set( prefixRect.right, y, symRect.left, y + chH );
}

void EditorHeadWin::EventSize( cevent_size* pEvent )
{
	CheckSize();
}

int uiClassEditorHeadWin = GetUiID( "EditorHeadWin" );
int EditorHeadWin::UiGetClassId() { return uiClassEditorHeadWin; };

EditorHeadWin::EditorHeadWin( Win* parent, EditWin* pEdit )
	:  Win( WT_CHILD, 0, parent, 0 ),
	   prefixString( utf8_to_unicode( _LT( "Edit:" ) ).data() ),
	   _edit( pEdit )
{
	OnChangeStyles();
	CheckSize();
}

bool EditorHeadWin::UpdateName()
{
	FSString uri;
	clPtr<FS> fs = _edit->GetFS();

	if ( !fs.IsNull() )
	{
		FSPath path;
		_edit->GetPath( path );
		uri = fs->Uri( path );
	}

	if ( nameString.Eq( uri.GetUnicode() ) ) { return false; }

	nameString.Set( uri.GetUnicode() );
	return true;
}

bool EditorHeadWin::UpdateCS()
{
	unicode_t buf[64];
	utf8_to_unicode   ( buf, _edit->GetCharsetName() );

	if ( csString.Eq( buf ) ) { return false; }

	csString.Set( buf );
	return true;
}

bool EditorHeadWin::UpdatePos()
{
	char cBuf[64];
	sprintf( cBuf, "%i /%i  %i", _edit->GetCursorLine() + 1, _edit->GetLinesCount(), _edit->GetCursorCol() + 1 );
	unicode_t uBuf[64];

	for ( int i = 0; i < 32; i++ ) if ( ( uBuf[i] = cBuf[i] ) == 0 ) { break; }

	uBuf[32] = 0;

	if ( posString.Eq( uBuf ) ) { return false; }

	posString.Set( uBuf );
	return true;
}

bool EditorHeadWin::UpdateSym()
{
	char cBuf[64] = "";
	int32 sym = _edit->GetCursorSymbol();

	if ( sym >= 0 )
	{
		sprintf( cBuf, "%i (%X)", sym, sym );
	}

	unicode_t uBuf[64];

	for ( int i = 0; i < 32; i++ ) if ( ( uBuf[i] = cBuf[i] ) == 0 ) { break; }

	uBuf[32] = 0;

	if ( symString.Eq( uBuf ) ) { return false; }

	symString.Set( uBuf );
	return true;
}


bool EditorHeadWin::Broadcast( int id, int subId, Win* win, void* data )
{
	if ( win == _edit )
	{
		wal::GC gc( this );
		gc.Set( dialogFont.ptr() );

		if ( UpdateSym() ) { DrawSym( gc ); }

		if ( UpdateCS() ) { DrawCS( gc ); }

		if ( UpdatePos() ) { DrawPos( gc ); }

		return true;
	}

	return false;
}

void EditorHeadWin::DrawCS( wal::GC& gc )
{
	unsigned bgColor  = UiGetColor( uiBackground, 0, 0, 0x808080 );
	_DrawUnicode( gc, csRect, csString.Str(), UiGetColor( uiCSColor, 0, 0, 0 ), bgColor );
}

void EditorHeadWin::DrawPos( wal::GC& gc )
{
	unsigned bgColor  = UiGetColor( uiBackground, 0, 0, 0x808080 );
	_DrawUnicode( gc, posRect, posString.Str(), UiGetColor( uiColor, 0, 0, 0 ), bgColor );
}

void EditorHeadWin::DrawSym( wal::GC& gc )
{
	unsigned bgColor  = UiGetColor( uiBackground, 0, 0, 0x808080 );
	_DrawUnicode( gc, symRect, symString.Str(), UiGetColor( uiColor, 0, 0, 0 ), bgColor );
}


void EditorHeadWin::Paint( wal::GC& gc, const crect& paintRect )
{
	unsigned bgColor  = UiGetColor( uiBackground, 0, 0, 0x808080 );
	crect r = ClientRect();
	gc.SetFillColor( bgColor );
	gc.FillRect( r );
	Draw3DButtonW2( gc, r, bgColor, true );
	gc.Set( dialogFont.ptr() ); //GetFont());

	r.Dec();
	r.Dec();
	r.Dec();

	_DrawUnicode( gc, prefixRect, prefixString.Str(), UiGetColor( uiPrefixColor, 0, 0, 0 ), bgColor );
	UpdateName();
	_DrawUnicode( gc, nameRect, nameString.Str(), UiGetColor( uiColor, 0, 0, 0 ), bgColor );
	UpdateSym();
	DrawSym( gc );
	UpdateCS();
	DrawCS( gc );
	UpdatePos();
	DrawPos( gc );
}


bool EditorHeadWin::EventMouse( cevent_mouse* pEvent )
{
	switch ( pEvent->Type() )
	{
		case EV_MOUSE_MOVE:
			break;

		case EV_MOUSE_PRESS:
		case EV_MOUSE_DOUBLE:
		{
			cpoint p = pEvent->Point();

			if ( csRect.In( p ) )
			{
				Command( ID_CHARSET_TABLE, 0, this, 0 );
			}
			else if ( posRect.In( p ) )
			{
				Command( ID_GOTO_LINE, 0, this, 0 );
			}

		}
		break;

		case EV_MOUSE_RELEASE:
			break;
	};

	return true;
}

EditorHeadWin::~EditorHeadWin() {}



/////////////////////////////////////////// ViewerEHeadWin


void ViewerHeadWin::OnChangeStyles()
{
	wal::GC gc( this );
	gc.Set( dialogFont.ptr() ); //GetFont());
	cpoint p = gc.GetTextExtents( ABCString );
	chW = p.x /= ABCStringLen;
	chH = p.y;
	p.y += 6;
	LSize lSize( p );
	lSize.x.maximal = 10000;
	SetLSize( lSize );
	prefixWidth = gc.GetTextExtents( prefixString.Str() ).x;
}

void ViewerHeadWin::CheckSize()
{
	crect r = ClientRect();
	int y = 3;
	int x = 3;
	prefixRect.Set( x, y, x + prefixWidth, y + chH );

	int xr = r.right - 3;
	int w = chW * 5;
	percentRect.Set( xr - w, y, xr, y + chH );
	xr -= w;

	w = chW * 15;
	csRect.Set( xr - w, y, xr, y + chH );
	xr -= w;

	w = chW * 10;
	colRect.Set( xr - w, y, xr, y + chH );
	xr -= w;

	nameRect.Set( prefixRect.right, y, colRect.left, y + chH );
}

void ViewerHeadWin::EventSize( cevent_size* pEvent )
{
	CheckSize();
}

static int uiClassViewerHeadWin = GetUiID( "ViewerHeadWin" );
int ViewerHeadWin::UiGetClassId() { return uiClassViewerHeadWin; }

ViewerHeadWin::ViewerHeadWin( Win* parent, ViewWin* pView )
	:  Win( WT_CHILD, 0, parent, 0 ),
	   prefixString( utf8_to_unicode( _LT( "View:" ) ).data() ),
	   _view( pView )
{
	OnChangeStyles();
	CheckSize();
}



bool ViewerHeadWin::UpdateName()
{
	FSString uri = _view->Uri();

	if ( nameString.Eq( uri.GetUnicode() ) ) { return false; }

	nameString.Set( uri.GetUnicode() );
	return true;
}

bool ViewerHeadWin::UpdateCS()
{
	unicode_t buf[64];
	utf8_to_unicode   ( buf, _view->GetCharsetName() );

	if ( csString.Eq( buf ) ) { return false; }

	csString.Set( buf );
	return true;
}

bool ViewerHeadWin::UpdatePercent()
{
	char cBuf[64] = "";
	int p = _view->GetPercent();

	if ( p >= 0 )
	{
		sprintf( cBuf, "%i%%", p );
	}

	unicode_t uBuf[64];

	for ( int i = 0; i < 32; i++ ) if ( ( uBuf[i] = cBuf[i] ) == 0 ) { break; }

	uBuf[32] = 0;

	if ( percentString.Eq( uBuf ) ) { return false; }

	percentString.Set( uBuf );
	return true;
}

bool ViewerHeadWin::UpdateCol()
{
	char cBuf[64] = "";
	int col = _view->GetCol();

	if ( col >= 0 )
	{
		sprintf( cBuf, "%i", col + 1 );
	}

	unicode_t uBuf[64];

	for ( int i = 0; i < 32; i++ ) if ( ( uBuf[i] = cBuf[i] ) == 0 ) { break; }

	uBuf[32] = 0;

	if ( colString.Eq( uBuf ) ) { return false; }

	colString.Set( uBuf );
	return true;
}




bool ViewerHeadWin::Broadcast( int id, int subId, Win* win, void* data )
{
	if ( win == _view )
	{
		wal::GC gc( this );
		gc.Set( dialogFont.ptr() );

		if ( UpdateCol() ) { DrawCol( gc ); }

		if ( UpdateCS() ) { DrawCS( gc ); }

		if ( UpdatePercent() ) { DrawPercent( gc ); }

		return true;
	}

	return false;
}

void ViewerHeadWin::DrawCS( wal::GC& gc )
{
	unsigned bgColor  = UiGetColor( uiBackground, 0, 0, 0x808080 );
	_DrawUnicode( gc, csRect, csString.Str(), UiGetColor( uiCSColor, 0, 0, 0 ), bgColor );
}

void ViewerHeadWin::DrawCol( wal::GC& gc )
{
	unsigned bgColor  = UiGetColor( uiBackground, 0, 0, 0x808080 );
	_DrawUnicode( gc, colRect, colString.Str(), UiGetColor( uiColor, 0, 0, 0 ), bgColor );
}

void ViewerHeadWin::DrawPercent( wal::GC& gc )
{
	unsigned bgColor  = UiGetColor( uiBackground, 0, 0, 0x808080 );
	_DrawUnicode( gc, percentRect, percentString.Str(), UiGetColor( uiColor, 0, 0, 0 ), bgColor );
}

void ViewerHeadWin::Paint( wal::GC& gc, const crect& paintRect )
{
	unsigned bgColor  = UiGetColor( uiBackground, 0, 0, 0x808080 );
	crect r = ClientRect();
	gc.SetFillColor( bgColor );
	gc.FillRect( r );
	Draw3DButtonW2( gc, r, bgColor, true );
	gc.Set( dialogFont.ptr() ); //GetFont());

	r.Dec();
	r.Dec();
	r.Dec();

	_DrawUnicode( gc, prefixRect, prefixString.Str(), UiGetColor( uiPrefixColor, 0, 0, 0 ), bgColor );
	UpdateName();
	_DrawUnicode( gc, nameRect, nameString.Str(), 0, bgColor );
	UpdateCol();
	DrawCol( gc );
	UpdateCS();
	DrawCS( gc );
	UpdatePercent();
	DrawPercent( gc );
}

bool ViewerHeadWin::EventMouse( cevent_mouse* pEvent )
{
	switch ( pEvent->Type() )
	{
		case EV_MOUSE_MOVE:
			break;

		case EV_MOUSE_PRESS:
		case EV_MOUSE_DOUBLE:
		{
			cpoint p = pEvent->Point();

			if ( csRect.In( p ) )
			{
				Command( ID_CHARSET_TABLE, 0, this, 0 );
			}
		}
		break;

		case EV_MOUSE_RELEASE:
			break;
	};

	return true;
}

ViewerHeadWin::~ViewerHeadWin() {}

