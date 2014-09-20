/*
 * Part of Wal Commander GitHub Edition
 * https://github.com/corporateshark/WalCommander
 * walcommander@linderdaum.com
 */

#include "ftplogon.h"

#include "ncdialogs.h"
#include "operthread.h" //для carray_cat
#include "charsetdlg.h"
#include "ltext.h"


class FtpLogonDialog: public NCVertDialog
{
	Layout iL;
public:
	StaticLabel serverText;
	SButton  anonymousButton;
	StaticLabel userText;
	StaticLabel passwordText;
	StaticLabel portText;
	StaticLabel charsetText;
	int charset;
	StaticLine charsetIdText;

	EditLine serverEdit;
	EditLine userEdit;
	EditLine passwordEdit;
	EditLine portEdit;
	Button charsetButton;
	SButton  passiveButton;

//	ccollect<Win*> order;

	FtpLogonDialog( NCDialogParent* parent, FSFtpParam& params );
	virtual bool EventChildKey( Win* child, cevent_key* pEvent );
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual ~FtpLogonDialog();
};

FtpLogonDialog::~FtpLogonDialog() {}

FtpLogonDialog::FtpLogonDialog( NCDialogParent* parent, FSFtpParam& params )
	:  NCVertDialog( ::createDialogAsChild, 0,  parent, utf8_to_unicode( _LT( "FTP logon" ) ).data(), bListOkCancel ),
	   iL( 16, 3 ),
	   serverText(0, this, utf8_to_unicode(_LT("&Server:")).data(), &serverEdit),
	   anonymousButton( 0, this, utf8_to_unicode( _LT( "&Anonymous logon" ) ).data(), 0, params.anonymous ),
	   userText(0, this, utf8_to_unicode(_LT("&Login:")).data(), &userEdit),
	   passwordText(0, this, utf8_to_unicode(_LT("Pass&word:")).data(), &passwordEdit),
	   portText(0, this, utf8_to_unicode(_LT("&Port:")).data(), &portEdit),
	   charsetText(0, this, utf8_to_unicode(_LT("&Charset:")).data(), &charsetButton),

	   charset( params.charset ),
	   charsetIdText( 0, this, utf8_to_unicode( "***************" ).data() ), // placeholder

	   serverEdit  ( 0, this, 0, 0, 16 ),
	   userEdit ( 0, this, 0, 0, 16 ),
	   passwordEdit   ( 0, this, 0, 0, 16 ),
	   portEdit ( 0, this, 0, 0, 16 ),

	   charsetButton( 0, this, utf8_to_unicode( ">" ).data() , 1000 ),
	   passiveButton( 0, this, utf8_to_unicode( _LT( "Passi&ve mode" ) ).data(), 0, params.passive )
{
	serverEdit.SetText( params.server.Data(), true );
	userEdit.SetText( params.user.Data(), true );
	passwordEdit.SetText( params.pass.Data(), true );

	char buf[0x100];
	snprintf( buf, sizeof( buf ), "%i", params.port );
	portEdit.SetText( utf8_to_unicode( buf ).data(), true );

	bool focus = false;

	iL.AddWin( &serverText,  0, 0, 0, 0 );
	serverText.Enable();
	serverText.Show();
	iL.AddWin( &serverEdit,  0, 1, 0, 1 );
	serverEdit.Enable();
	serverEdit.Show();

	if ( !focus && !params.server.Data()[0] ) { serverEdit.SetFocus(); focus = true; }


	iL.AddWin( &anonymousButton, 1, 0, 1, 2 );
	anonymousButton.Enable();
	anonymousButton.Show();

	iL.AddWin( &userText, 2, 0, 2, 0 );
	userText.Enable();
	userText.Show();
	iL.AddWin( &userEdit, 2, 1, 2 , 1 );

	if ( !params.anonymous ) { userEdit.Enable(); }

	userEdit.Show();

	if ( !focus && !params.user.Data()[0] ) { userEdit.SetFocus(); focus = true; }

	passwordEdit.SetPasswordMode();
	iL.AddWin( &passwordText, 3, 0, 3, 0 );
	passwordText.Enable();
	passwordText.Show();
	iL.AddWin( &passwordEdit, 3, 1, 3, 1 );

	if ( !params.anonymous ) { passwordEdit.Enable(); }

	passwordEdit.Show();

	if ( !focus && !params.pass.Data()[0] ) { passwordEdit.SetFocus(); focus = true; }

	iL.AddWin( &portText, 4, 0, 4, 0 );
	portText.Enable();
	portText.Show();
	iL.AddWin( &portEdit, 4, 1, 4, 1 );
	portEdit.Enable();
	portEdit.Show();

	iL.AddWin( &charsetText, 5, 0, 5, 0 );
	charsetText.Enable();
	charsetText.Show();
	charsetIdText.SetText( utf8_to_unicode( charset_table[params.charset]->name ).data() );
	iL.AddWin( &charsetIdText, 5, 1, 5, 1 );
	charsetIdText.Enable();
	charsetIdText.Show();
	iL.AddWin( &charsetButton, 5, 2 );
	charsetButton.Enable();
	charsetButton.Show();

	iL.AddWin( &passiveButton, 6, 0, 6, 1 );
	passiveButton.Enable();
	passiveButton.Show();

	/*
	   iL.ColSet(0, 20, 20, 20);
	   iL.ColSet(1, 2, 2, 2);
	   iL.ColSet(2, 2, 2, 2);
	   iL.ColSet(3, 20, 200, 100);
	   iL.ColSet(4, 2, 2, 2);
	*/
	AddLayout( &iL );
	SetEnterCmd( CMD_OK );

	order.append( &serverEdit );
	order.append( &anonymousButton );
	order.append( &userEdit );
	order.append( &passwordEdit );
	order.append( &portEdit );
	order.append( &charsetButton );
	order.append( &passiveButton );

	SetPosition();
}

bool FtpLogonDialog::Command( int id, int subId, Win* win, void* data )
{
	if ( id == CMD_SBUTTON_INFO && win == &anonymousButton )
	{
		userEdit.Enable( subId != SCMD_SBUTTON_CHECKED );
		passwordEdit.Enable( subId != SCMD_SBUTTON_CHECKED );
		return true;
	}
	else if ( id == 1000 )
	{
		int ret;

		if ( SelectCharset( ( NCDialogParent* )Parent(), &ret, charset ) )
		{
			charset = ret;
			charsetIdText.SetText( utf8_to_unicode( charset_table[ret]->name ).data() );
		}

		return true;
	}

	return NCVertDialog::Command( id, subId, win, data );
}

bool FtpLogonDialog::EventChildKey( Win* child, cevent_key* pEvent )
{
	if ( pEvent->Type() == EV_KEYDOWN )
	{
		/*    if (pEvent->Key() == VK_UP || pEvent->Key() == VK_DOWN)
		      {
		         int n = -1;
		         int count = order.count();
		         Win *button = GetDownButton();
		         if (button) count++;

		         for (int i = 0; i < order.count(); i++) if (order[i]->InFocus()) { n = i; break; }

		         if (pEvent->Key() == VK_UP)
		            n = (n + count - 1) % count;
		         else if (pEvent->Key() == VK_DOWN)
		            n = (n + 1) % count;

		         if (n >= 0 && n < order.count())
		            order[n]->SetFocus();
		         else
		            if (button) button->SetFocus();

		         return true;
		      } else */
		if ( pEvent->Key() == VK_RETURN && charsetButton.InFocus() ) //prevent autoenter
		{
			return false;
		}

	};

	return NCVertDialog::EventChildKey( child, pEvent );
}

bool GetFtpLogon( NCDialogParent* parent, FSFtpParam& params )
{
	FtpLogonDialog dlg( parent, params );

	if ( dlg.DoModal() == CMD_OK )
	{
		params.server  = dlg.serverEdit.GetText().data();
		params.user = dlg.userEdit.GetText().data();
		params.pass = dlg.passwordEdit.GetText().data();
		params.port = atoi( unicode_to_utf8( dlg.portEdit.GetText().data() ).data() );
		params.anonymous = dlg.anonymousButton.IsSet();
		params.passive = dlg.passiveButton.IsSet();
		params.isSet   = true;
		params.charset = dlg.charset;
		return true;
	}

	return false;
}

