/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include <algorithm>

#include <wal.h>
#include "wcm-config.h"
#include "color-style.h"
#include "string-util.h"
#include "vfs.h"
#include "fontdlg.h"
#include "ncfonts.h"
#include "ncwin.h"
#include "ltext.h"
#include "globals.h"
#include "folder-history.h"
#include "view-history.h"
#include "nchistory.h"

#ifdef _WIN32
#  include "w32util.h"
#endif

#define __STDC_FORMAT_MACROS
#include <stdint.h>

#if !defined(_MSC_VER) || _MSC_VER >= 1700
#  include <inttypes.h>
#endif

#include <map>


std::string* IniHash::Find( const char* section, const char* var )
{
	cstrhash< std::string >* h = hash.exist( section );

	if ( !h )
	{
		return 0;
	}

	return h->exist( var );
}

std::string* IniHash::Create( const char* section, const char* var )
{
	return &(hash[section][var]);
}

void IniHash::Delete( const char* section, const char* var )
{
	hash[section].del( var, false );
}

void IniHash::SetStrValue( const char* section, const char* var, const char* value )
{
	if ( !value )
	{
		Delete( section, var ); return;
	}
	
	std::string* p = Create( section, var );
	if ( p )
	{
		*p = value;
	}
}

void IniHash::SetIntValue( const char* section, const char* var, int value )
{
	SetStrValue( section, var, ToString(value).c_str() );
}

void IniHash::SetBoolValue( const char* section, const char* var, bool value )
{
	SetIntValue( section, var, value ? 1 : 0 );
}

const char* IniHash::GetStrValue( const char* section, const char* var, const char* def )
{
	std::string* p = Find( section, var );
	return (p && p->data()) ? p->data() : def;
}

int IniHash::GetIntValue( const char* section, const char* var, int def )
{
	std::string* p = Find( section, var );
	return (p && p->data()) ? atoi( p->data() ) : def;
}

bool IniHash::GetBoolValue( const char* section, const char* var, bool def )
{
	const int n = GetIntValue( section, var, def ? 1 : 0 );
	return n ? true : false;
}


#ifndef _WIN32

#define DEFAULT_CONFIG_PATH UNIX_CONFIG_DIR_PATH "/config.default"

class TextInStream
{
private:
	int bufSize;
	std::vector<char> buffer;
	int pos;
	int count;
	bool FillBuffer()
	{
		if ( pos < count ) { return true; }
		if ( count <= 0 ) { return false; }
		count = Read( buffer.data(), bufSize );
		pos = 0;
		return count > 0;
	}

public:
	TextInStream( int _bSize = 1024 )
		: bufSize( _bSize > 0 ? _bSize : 1024 )
		, pos( 1 )
		, count( 1 )
	{
		buffer.resize( bufSize );
	}
	~TextInStream() {}

	int GetC()
	{
		FillBuffer();
		return pos < count ? buffer[pos++] : EOF;
	}

	bool GetLine( char* s, int size );

protected:
	virtual int Read( char* buf, int size ) = 0; //can throw
};


bool TextInStream::GetLine( char* s, int size )
{
	if ( size <= 0 ) { return true; }

	if ( !FillBuffer() ) { return false; }

	if ( size == 1 ) { *s = 0; return true; }

	size--;

	while ( true )
	{
		if ( pos >= count && !FillBuffer() ) { break; }

		char* b = buffer.data() + pos;
		char* e = buffer.data() + count;

		for ( ; b < e; b++ ) if ( *b == '\n' ) { break; }

		if ( size > 0 )
		{
			int n = b - ( buffer.data() + pos );

			if ( n > size ) { n = size; }

			memcpy( s, buffer.data() + pos, n );
			size -= n;
			s += n;
		}

		pos = b - buffer.data();

		if ( b < e )
		{
			pos++;
			break;
		}
	}

	*s = 0;

	return true;
}

class TextOutStream
{
private:
	int bufSize;
	std::vector<char> buffer;
	int pos;
public:
	TextOutStream( int _bSize = 1024 ): bufSize( _bSize > 0 ? _bSize : 1024 ), pos( 0 ) { buffer.resize( bufSize ); }
	void Flush() {  if ( pos > 0 ) { Write( buffer.data(), pos ); pos = 0; } }
	void PutC( int c ) { if ( pos >= bufSize ) { Flush(); } buffer[pos++] = c; }
	void Put( const char* s, int size );
	void Put( const char* s ) { Put( s, strlen( s ) ); }
	~TextOutStream() {}
protected:
	virtual void Write( char* buf, int size ) = 0; //can throw
	void Clear() { pos = 0; }
};


void TextOutStream::Put( const char* s, int size )
{
	while ( size > 0 )
	{
		if ( pos >= bufSize ) { Flush(); }

		int n = bufSize - pos;

		if ( n > size ) { n = size; }

		memcpy( buffer.data() + pos, s, n );
		pos += n;
		size -= n;
		s += n;
	}
}

class SysTextFileIn: public TextInStream
{
	File f;
public:
	SysTextFileIn( int bSize = 4096 ): TextInStream( bSize ) {}
	void Open( const sys_char_t* fileName ) { f.Open( fileName, FOPEN_READ ); }
	void Close() { f.Close(); }
	~SysTextFileIn() {}

protected:
	virtual int Read( char* buf, int size );
};

int SysTextFileIn::Read( char* buf, int size ) { return f.Read( buf, size ); }


class SysTextFileOut: public TextOutStream
{
	File f;
public:
	SysTextFileOut( int bSize = 4096 ): TextOutStream( bSize ) {}
	void Open( const sys_char_t* fileName ) { Clear(); f.Open( fileName, FOPEN_WRITE | FOPEN_CREATE | FOPEN_TRUNC ); }
	void Close() { f.Close(); }
	~SysTextFileOut() {}

protected:
	virtual void Write( char* buf, int size );
};

void SysTextFileOut::Write( char* buf, int size ) {  f.Write( buf, size ); }


static FSPath configDirPath( CS_UTF8, "???" );


void LoadIniHash( IniHash& iniHash, const sys_char_t* fileName )
{
	SysTextFileIn in;

	try
	{
		in.Open( fileName );
	}
	catch ( csyserr* ex )
	{
		if ( SysErrorIsFileNotFound( ex->code ) )
		{
			ex->destroy();
			return;
		}

		throw;
	}

	char buf[4096];
	std::string section;

	while ( in.GetLine( buf, sizeof( buf ) ) )
	{

		char* s = buf;

		while ( IsSpace( *s ) ) { s++; }

		if ( !*s || *s == '#' ) { continue; }

		if ( *s == '[' )
		{
			s++;

			while ( IsSpace( *s ) ) { s++; }

			char* t = s;

			while ( *t && *t != ']' ) { t++; }

			if ( *t != ']' ) { continue; }

			while ( t > s && IsSpace( *( t - 1 ) ) ) { t--; }

			*t = 0;

			section = s;

		}
		else
		{
			if ( section.empty() ) { continue; }

			char* t = s;

			while ( *t && *t != '=' ) { t++; }

			if ( *t != '=' ) { continue; }

			char* v = t + 1;

			while ( t > s && IsSpace( *( t - 1 ) ) ) { t--; }

			*t = 0;

			while ( IsSpace( *v ) ) { v++; }

			t = v;

			while ( *t ) { t++; }

			while ( t > v && IsSpace( *( t - 1 ) ) ) { t--; }

			*t = 0;

			iniHash.SetStrValue( section.data(), s, v );
		}
	}

	in.Close();
}

void IniHashLoad( IniHash& iniHash, const char* sectName )
{
	FSPath path = configDirPath;
	path.Push( CS_UTF8, carray_cat<char>( sectName, ".cfg" ).data() );
	
	LoadIniHash( iniHash, (sys_char_t*) path.GetString( sys_charset_id ) );
}

inline bool strless( const char* a, const char* b )
{
	const char* s1 = a;
	const char* s2 = b;

	while ( *s1 && *s1 == *s2 ) { s1++; s2++; };

	return *s1 <= *s2;
}

void SaveIniHash( IniHash& iniHash, const sys_char_t* fileName )
{
	SysTextFileOut out;
	out.Open( fileName );

	if ( iniHash.Size() > 0 )
	{
		std::vector<const char*> secList = iniHash.Keys();

		std::sort( secList.begin(), secList.end(), strless );

		for ( int i = 0, count = secList.size(); i < count; i++ )
		{
			out.Put( "\n[" );
			out.Put( secList[i] );
			out.Put( "]\n" );
			cstrhash< std::string >* h = iniHash.Exist( secList[i] );

			if ( !h ) { continue; }

			std::vector<const char*> varList = h->keys();

			std::sort( varList.begin(), varList.end(), strless );

			for ( int j = 0; j < h->count(); j++ )
			{
				out.Put( varList[j] );
				out.PutC( '=' );
				std::string* p = h->exist( varList[j] );

				if ( p && p->data() ) { out.Put( p->data() ); }

				out.PutC( '\n' );
			}
		}
	}

	out.Flush();
	out.Close();
}

void IniHashSave( IniHash& iniHash, const char* sectName )
{
	FSPath path = configDirPath;
	path.Push( CS_UTF8, carray_cat<char>( sectName, ".cfg" ).data() );
	
	SaveIniHash( iniHash, (sys_char_t*) path.GetString( sys_charset_id ) );
}


bool LoadStringList( const char* section, std::vector< std::string >& list )
{
	try
	{
		SysTextFileIn in;

		FSPath path = configDirPath;
		path.Push( CS_UTF8, carray_cat<char>( section, ".cfg" ).data() );
		in.Open( ( sys_char_t* )path.GetString( sys_charset_id ) );

		char buf[4096];

		while ( in.GetLine( buf, sizeof( buf ) ) )
		{
			char* s = buf;

			while ( *s > 0 && *s <= ' ' ) { s++; }

			if ( *s ) { list.push_back( std::string( s ) ); }
		}
	}
	catch ( cexception* ex )
	{
		ex->destroy();
		return false;
	}

	return true;
}


void SaveStringList( const char* section, std::vector< std::string >& list )
{
	try
	{
		SysTextFileOut out;

		FSPath path = configDirPath;
		path.Push( CS_UTF8, carray_cat<char>( section, ".cfg" ).data() );
		out.Open( ( sys_char_t* )path.GetString( sys_charset_id ) );

		for ( int i = 0; i < ( int )list.size(); i++ )
		{
			if ( list[i].c_str() && list[i][0] )
			{
				out.Put( list[i].c_str() );
				out.PutC( '\n' );
			}
		}

		out.Flush();
		out.Close();

	}
	catch ( cexception* ex )
	{
		ex->destroy();
		return ;
	}
}


#else

static const char* regapp = "WCM commander";
static const char* regcomp = "WCM";

#define COMPANY regcomp
#define APPNAME regapp

static HKEY GetAppProfileKey()
{
	if ( !regapp || !regcomp ) { return NULL; }

	HKEY hsoft = NULL;
	HKEY hcomp = NULL;
	HKEY happ  = NULL;

	if ( RegOpenKeyExA( HKEY_CURRENT_USER, "software", 0, KEY_WRITE | KEY_READ,
	                    &hsoft ) == ERROR_SUCCESS )
	{
		DWORD dw;

		if ( RegCreateKeyExA( hsoft, COMPANY, 0, REG_NONE,
		                      REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ, NULL,
		                      &hcomp, &dw ) == ERROR_SUCCESS )
		{
			RegCreateKeyExA( hcomp, APPNAME, 0, REG_NONE,
			                 REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ, NULL,
			                 &happ, &dw );
		}
	}

	if ( hsoft != NULL ) { RegCloseKey( hsoft ); }

	if ( hcomp != NULL ) { RegCloseKey( hcomp ); }

	return happ;
}

HKEY GetSection( HKEY hKey, const char* sectname )
{
	ASSERT( sectname && *sectname );
	if ( !hKey )
	{
		return NULL;
	}

	DWORD dw;
	HKEY hsect;
	RegCreateKeyEx( hKey, sectname, 0, REG_NONE,
		REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ | KEY_QUERY_VALUE, NULL,
		&hsect, &dw );

	return hsect;
}

HKEY GetSection( const char* sectname )
{
	ASSERT( sectname && *sectname );
	HKEY happ = GetAppProfileKey();
	if ( !happ )
	{
		return NULL;
	}

	HKEY hsect = GetSection( happ, sectname );
	RegCloseKey( happ );
	return hsect;
}

std::vector<std::string> RegReadKeys( HKEY hKey )
{
	if ( !hKey )
	{
		return std::vector<std::string>();
	}

	std::vector<std::string> keys;

	DWORD dwIndex = 0, dwSize;
	char buf[256];
	while ( RegEnumKeyEx( hKey, dwIndex++, buf, &(dwSize = 256), NULL, NULL, NULL, NULL ) == ERROR_SUCCESS )
	{
		std::string str( buf );
		keys.push_back( str );
	}

	return keys;
}

void RegReadIniHash( HKEY hKey, IniHash& iniHash )
{
	char bufName[256];
	char bufValue[4096];

	std::vector<std::string> keys = RegReadKeys( hKey );
	for ( int i = 0, count = keys.size(); i < count; i++ )
	{
		std::string section = keys.at( i );
		HKEY hSect = GetSection( hKey, section.c_str() );

		DWORD dwIndex = 0, dwNameSize, dwValueSize;
		while ( RegEnumValue( hSect, dwIndex++, bufName, &(dwNameSize = 256), NULL, NULL, (LPBYTE) bufValue, &(dwValueSize = 4096) ) == ERROR_SUCCESS )
		{
			iniHash.SetStrValue( section.c_str(), bufName, bufValue );
		}

		RegCloseKey( hSect );
	}
}

void RegWriteIniHash( HKEY hKey, IniHash& iniHash )
{
	std::vector<const char*> secList = iniHash.Keys();
	//std::sort( secList.begin(), secList.end(), strless );

	for ( int i = 0, sectCount = secList.size(); i < sectCount; i++ )
	{
		const char* section = secList[i];
		cstrhash<std::string>* h = iniHash.Exist( section );
		if ( !h )
		{
			continue;
		}

		std::vector<const char*> varList = h->keys();
		//std::sort( varList.begin(), varList.end(), strless );
		HKEY hSect = GetSection( hKey, section );
		if ( !hSect )
		{
			continue;
		}

		for ( int j = 0, varCount = varList.size(); j < varCount; j++ )
		{
			const char* var = varList[j];
			std::string* p = h->exist( var );
			if ( p && p->c_str() )
			{
				RegSetValueEx( hSect, var, 0, REG_SZ, (LPBYTE) p->c_str(), p->size() + 1 );
			}
		}

		RegCloseKey( hSect );
	}
}

DWORD RegReadInt( const char* sect, const  char* what, DWORD def )
{
	HKEY hsect = GetSection( sect );

	if ( !hsect ) { return def; }

	DWORD dwValue;
	DWORD dwType;
	DWORD dwCount = sizeof( DWORD );
	LONG lResult = RegQueryValueEx( hsect, ( LPTSTR )what, NULL, &dwType,
	                                ( LPBYTE )&dwValue, &dwCount );
	RegCloseKey( hsect );

	if ( lResult == ERROR_SUCCESS )
	{
		ASSERT( dwType == REG_DWORD );
		ASSERT( dwCount == sizeof( dwValue ) );
		return ( UINT )dwValue;
	}

	return def;
}

std::string RegReadString( char const* sect, const char* what, const char* def )
{
	HKEY hsect = GetSection( sect );

	if ( !hsect ) { return def ? def : std::string(); }

	std::string strValue;
	DWORD dwType, dwCount;
	LONG lResult = RegQueryValueEx( hsect, ( LPTSTR )what, NULL, &dwType, NULL, &dwCount );

	if ( lResult == ERROR_SUCCESS )
	{
		ASSERT( dwType == REG_SZ );

		//TODO: possible memory leak, needs to be checked!!!
		char* Buf = (char*)alloca( dwCount + 1 );
		Buf[dwCount] = 0;

		lResult = RegQueryValueEx( hsect, ( LPTSTR )what, NULL, &dwType, ( LPBYTE )Buf, &dwCount );

		strValue = std::string( Buf );
	}

	RegCloseKey( hsect );

	if ( lResult == ERROR_SUCCESS )
	{
		ASSERT( dwType == REG_SZ );
		return strValue;
	}

	return def ? def : std::string();
}

int RegGetBinSize( const char* sect, const char* what )
{
	HKEY hsect = GetSection( sect );

	if ( hsect == NULL ) { return -1; }

	DWORD dwType, dwCount;
	LONG lResult = RegQueryValueEx( hsect, ( LPTSTR )what, NULL, &dwType,
	                                NULL, &dwCount );
	RegCloseKey( hsect );

	if ( lResult != ERROR_SUCCESS ) { return -1; }

	return dwCount;

}

bool RegReadBin( const char* sect, const char* what, const void* data, int size )
{
	HKEY hsect = GetSection( sect );

	if ( hsect == NULL ) { return false; }

	DWORD dwType, dwCount;
	LONG lResult = RegQueryValueEx( hsect, ( LPTSTR )what, NULL, &dwType,
	                                NULL, &dwCount );

	if ( lResult != ERROR_SUCCESS || dwCount != ( DWORD )size )
	{
		RegCloseKey( hsect );
		return false;
	}

	if ( lResult == ERROR_SUCCESS )
	{
		ASSERT( dwType == REG_BINARY );
		lResult = RegQueryValueEx( hsect, ( LPTSTR )what, NULL, &dwType,
		                           ( LPBYTE )data, &dwCount );
	}

	RegCloseKey( hsect );
	return ( lResult == ERROR_SUCCESS );
}

bool RegWriteInt( const char* sect, const char* what, DWORD data )
{
	HKEY hsect = GetSection( sect );

	if ( hsect == NULL ) { return false; }

	LONG lResult = RegSetValueEx( hsect, what, 0, REG_DWORD,
	                              ( LPBYTE )&data, sizeof( data ) );
	RegCloseKey( hsect );
	return lResult == ERROR_SUCCESS;
}

bool RegWriteString( const char* sect, const char* what, const char* data )
{
	if ( !data ) { return false; }

	HKEY hsect = GetSection( sect );

	if ( hsect == NULL ) { return false; }

	LONG lResult = RegSetValueEx( hsect, what, 0, REG_SZ,
	                              ( LPBYTE )data, strlen( data ) + 1 );
	RegCloseKey( hsect );
	return lResult == ERROR_SUCCESS;
}

bool RegWriteBin( const char* sect, const char* what, const void* data, int size )
{
	HKEY hsect = GetSection( sect );

	if ( hsect == NULL ) { return false; }

	LONG lResult = RegSetValueEx( hsect, what, 0, REG_BINARY,
	                              ( LPBYTE )data, size );
	RegCloseKey( hsect );
	return lResult == ERROR_SUCCESS;
}

bool LoadStringList( const char* section, std::vector< std::string >& list )
{
	char name[64];
	list.clear();

	for ( int i = 1; ; i++ )
	{
		Lsnprintf( name, sizeof( name ), "v%i", i );
		std::string s = RegReadString( section, name, "" );

		if ( s.empty() ) { break; }

		list.push_back( s );
	}

	return true;
}

void SaveStringList( const char* section, std::vector< std::string >& list )
{
	int n = 1;
	char name[64];

	for ( size_t i = 0; i < list.size(); i++ )
	{
		if ( list[i].data() && list[i][0] )
		{
			Lsnprintf( name, sizeof( name ), "v%i", n );

			if ( !RegWriteString( section, name, list[i].data() ) )
			{
				break;
			}

			n++;
		}
	}

	Lsnprintf( name, sizeof( name ), "v%i", n );
	RegWriteString( section, name, "" );
}

void IniHashLoad( IniHash& iniHash, const char* sectName )
{
	HKEY hKey = GetSection( sectName );
	if ( hKey )
	{
		RegReadIniHash( hKey, iniHash );
		RegCloseKey( hKey );
	}
}

void IniHashSave( IniHash& iniHash, const char* sectName )
{
	HKEY hKey = GetSection( sectName );
	if ( hKey )
	{
		RegWriteIniHash( hKey, iniHash );
		RegCloseKey( hKey );
	}
}

#endif

const char* sectionSystem = "system";
const char* sectionPanel = "panel";
const char* sectionEditor = "editor";
const char* sectionViewer = "viewer";
const char* sectionTerminal = "terminal";
const char* sectionFonts = "fonts";

static const char* CommandsHistorySection = "CommandsHistory";
static const char* FilesAssociationsSection = "FilesAssociations";
static const char* HighlightingRulesSection = "HighlightingRules";
static const char* UserMenuSection = "UserMenu";

clWcmConfig::clWcmConfig()
	: systemAskOpenExec( true )
	, systemEscPanel( true )
	, systemEscCommandLine( true )
	, systemBackSpaceUpDir( false )
	, systemAutoComplete( true )
	, systemAutoSaveSetup( true )
	, systemShowHostName( false )
	, systemStorePasswords( false )
	, systemLang( "+" )

	, panelShowHiddenFiles( true )
	, panelCaseSensitive( false )
	, panelSelectFolders( false )
	, panelShowDotsInRoot( false )
	, panelShowFolderIcons( true )
	, panelShowExecutableIcons( true )
	, panelShowLinkIcons( true )
	, panelShowScrollbar( true )
	, panelShowSpacesMode( ePanelSpacesMode_Trailing )
	, panelModeLeft( 0 )
	, panelModeRight( 0 )

	, editSavePos( true )
	, editAutoIdent( false )
	, editTabSize( 3 )
	, editShl( true )
	, editClearHistoryAfterSaving( true )

	, terminalBackspaceKey( 0 )

	, styleShow3DUI( false )
	, styleColorMode( 0 )
	, styleShowToolBar( true )
	, styleShowButtonBar( true )
	, styleShowButtonBarIcons( true )
	, styleShowMenuBar( true )

	, windowX( 0 )
	, windowY( 0 )
	, windowWidth( 0 )
	, windowHeight( 0 )
{
#ifndef _WIN32
	MapBool( sectionSystem, "ask_open_exec", &systemAskOpenExec, systemAskOpenExec );
#endif
	MapBool( sectionSystem, "esc_panel", &systemEscPanel, systemEscPanel );
	MapBool( sectionSystem, "esc_panel", &systemEscCommandLine, systemEscCommandLine );
	MapBool( sectionSystem, "back_updir", &systemBackSpaceUpDir, systemBackSpaceUpDir );
	MapBool( sectionSystem, "auto_complete", &systemAutoComplete, systemAutoComplete );
	MapBool( sectionSystem, "auto_save_setup", &systemAutoSaveSetup, systemAutoSaveSetup );
	MapBool( sectionSystem, "show_hostname", &systemShowHostName, systemShowHostName );
	MapBool( sectionSystem, "store_passwords", &systemStorePasswords, systemStorePasswords );
	MapStr( sectionSystem,  "lang", &systemLang );

	MapBool( sectionSystem, "show_toolbar", &styleShowToolBar, styleShowToolBar );
	MapBool( sectionSystem, "show_buttonbar", &styleShowButtonBar, styleShowButtonBar );
	MapBool( sectionSystem, "show_buttonbaricons", &styleShowButtonBarIcons, styleShowButtonBarIcons );
	MapBool( sectionSystem, "show_menubar", &styleShowMenuBar, styleShowMenuBar );
	MapBool( sectionPanel, "show_3d_ui", &styleShow3DUI, styleShow3DUI );
	MapInt( sectionPanel, "color_mode", &styleColorMode, styleColorMode );

	MapBool( sectionPanel, "show_hidden_files",   &panelShowHiddenFiles, panelShowHiddenFiles );
	MapBool( sectionPanel, "case_sensitive_sort", &panelCaseSensitive, panelCaseSensitive );
	MapBool( sectionPanel, "select_folders",   &panelSelectFolders, panelSelectFolders );
	MapBool( sectionPanel, "show_dots",     &panelShowDotsInRoot, panelShowDotsInRoot );
	MapBool( sectionPanel, "show_foldericons",     &panelShowFolderIcons, panelShowFolderIcons );
	MapBool( sectionPanel, "show_executableicons",     &panelShowExecutableIcons, panelShowExecutableIcons );
	MapBool( sectionPanel, "show_linkicons",     &panelShowLinkIcons, panelShowLinkIcons );
	MapBool( sectionPanel, "show_scrollbar",     &panelShowScrollbar, panelShowScrollbar );
	MapInt( sectionPanel,  "show_spaces_mode",     ( int* )&panelShowSpacesMode, panelShowSpacesMode );
	MapInt( sectionPanel,  "mode_left",     &panelModeLeft, panelModeLeft );
	MapInt( sectionPanel,  "mode_right",    &panelModeRight, panelModeRight );

#ifdef _WIN32
	const char* defPanelPath = "C:\\";
#else
	const char* defPanelPath = "/";
#endif

	MapStr( sectionPanel,  "left_panel_path", &leftPanelPath, defPanelPath );
	MapStr( sectionPanel,  "right_panel_path", &rightPanelPath, defPanelPath );

	MapBool( sectionEditor, "save_file_position",    &editSavePos, editSavePos );
	MapBool( sectionEditor, "auto_ident",   &editAutoIdent, editAutoIdent );
	MapInt( sectionEditor, "tab_size",   &editTabSize, editTabSize );
	MapBool( sectionEditor, "highlighting", &editShl, editShl );
	MapBool( sectionEditor, "editClearHistoryAfterSaving", &editClearHistoryAfterSaving, editClearHistoryAfterSaving );

	MapInt( sectionTerminal, "backspace_key",  &terminalBackspaceKey, terminalBackspaceKey );

	MapStr( sectionFonts, "panel_font",  &panelFontUri );
	MapStr( sectionFonts, "viewer_font", &viewerFontUri );
	MapStr( sectionFonts, "editor_font", &editorFontUri );
	MapStr( sectionFonts, "dialog_font", &dialogFontUri );
	MapStr( sectionFonts, "terminal_font",  &terminalFontUri );
	MapStr( sectionFonts, "helptext_font",  &helpTextFontUri );
	MapStr( sectionFonts, "helpbold_font",  &helpBoldFontUri );
	MapStr( sectionFonts, "helphead_font",  &helpHeadFontUri );

	MapInt( sectionSystem,  "windowX",      &windowX,      windowX );
	MapInt( sectionSystem,  "windowY",      &windowY,      windowY );
	MapInt( sectionSystem,  "windowWidth",  &windowWidth,  windowWidth );
	MapInt( sectionSystem,  "windowHeight", &windowHeight, windowHeight );
}

void clWcmConfig::ImpCurrentFonts()
{
	panelFontUri = g_PanelFont.ptr() ? g_PanelFont->uri() : "";
	viewerFontUri = g_ViewerFont.ptr() ? g_ViewerFont->uri() : "";
	editorFontUri = g_EditorFont.ptr() ? g_EditorFont->uri() : "";
	dialogFontUri  = g_DialogFont.ptr() ? g_DialogFont->uri() : "";
	terminalFontUri  = g_TerminalFont.ptr() ? g_TerminalFont->uri() : "";
	helpTextFontUri  = g_HelpTextFont.ptr() ? g_HelpTextFont->uri() : "";
	helpBoldFontUri  = g_HelpBoldFont.ptr() ? g_HelpBoldFont->uri() : "";
	helpHeadFontUri  = g_HelpHeadFont.ptr() ? g_HelpHeadFont->uri() : "";
}

void clWcmConfig::MapInt( const char* Section, const char* Name, int* pInt, int def )
{
	sNode Node = sNode::CreateIntNode( Section, Name, pInt, def );
	m_MapList.push_back( Node );
}

void clWcmConfig::MapBool( const char* Section, const char* Name, bool* pBool, bool def )
{
	sNode Node = sNode::CreateBoolNode( Section, Name, pBool, def );
	m_MapList.push_back( Node );
}

void clWcmConfig::MapStr( const char* Section, const char* Name, std::string* pStr, const char* def )
{
	sNode Node = sNode::CreateStrNode( Section, Name, pStr, def );
	m_MapList.push_back( Node );
}

class clConfigHelper
{
public:
	clConfigHelper() : m_SectionName( "" ) {}

	void SetSectionName( const char* SectionName )
	{
		m_SectionName = SectionName;
	}

protected:
	const char* m_SectionName;
};

class clConfigWriter: public clConfigHelper
{
public:
#if defined(_WIN32)
	clConfigWriter() {}
#else
	explicit clConfigWriter( IniHash& hash ) : m_Hash( hash ) {}
#endif

	void Write( const char* KeyNamePattern, int i, const std::string&  Data )
	{
		this->Write( KeyNamePattern, i, Data.c_str() );
	}

	void Write( const char* KeyNamePattern, int i, const char* Data )
	{
		char Buf[4096];
		Lsnprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		RegWriteString( m_SectionName, Buf, Data );
#else
		m_Hash.SetStrValue( m_SectionName, Buf, Data );
#endif
	}
	void WriteBool( const char* KeyNamePattern, int i, bool Data )
	{
		char Buf[4096];
		Lsnprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		RegWriteInt( m_SectionName, Buf, ( int )Data );
#else
		m_Hash.SetBoolValue( m_SectionName, Buf, Data );
#endif
	}
	void WriteInt( const char* KeyNamePattern, int i, int Data )
	{
		char Buf[4096];
		Lsnprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		RegWriteInt( m_SectionName, Buf, Data );
#else
		m_Hash.SetIntValue( m_SectionName, Buf, Data );
#endif
	}

private:
#if !defined(_WIN32)
	IniHash& m_Hash;
#endif
};

class clConfigReader: public clConfigHelper
{
public:
#if defined(_WIN32)
	clConfigReader() {}
#else
	explicit clConfigReader( IniHash& hash ): m_Hash( hash ) {}
#endif

	std::string Read( const char* KeyNamePattern, int i )
	{
		char Buf[4096];
		Lsnprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		std::string Result = RegReadString( m_SectionName, Buf, "" );
#else
		std::string Result = m_Hash.GetStrValue( m_SectionName, Buf, "" );
#endif
		return Result;
	}
	bool ReadBool( const char* KeyNamePattern, int i, bool DefaultValue )
	{
		char Buf[4096];
		Lsnprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		bool Result = RegReadInt( m_SectionName, Buf, DefaultValue ) != 0;
#else
		bool Result = m_Hash.GetBoolValue( m_SectionName, Buf, DefaultValue );
#endif
		return Result;
	}
	int ReadInt( const char* KeyNamePattern, int i, int DefaultValue )
	{
		char Buf[4096];
		Lsnprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		int Result = RegReadInt( m_SectionName, Buf, DefaultValue );
#else
		int Result = m_Hash.GetIntValue( m_SectionName, Buf, DefaultValue );
#endif
		return Result;
	}

private:
#if !defined(_WIN32)
	IniHash& m_Hash;
#endif
};

void SaveFileAssociations( NCWin* nc
#ifndef _WIN32
                           , IniHash& hash
#endif
                         )
{
	if ( !nc ) { return; }

#if defined(_WIN32)
	clConfigWriter Cfg;
#else
	clConfigWriter Cfg( hash );
#endif
	Cfg.SetSectionName( FilesAssociationsSection );

	const std::vector<clNCFileAssociation>& Assoc = g_Env.GetFileAssociations();

	for ( size_t i = 0; i < Assoc.size(); i++ )
	{
		const clNCFileAssociation& A = Assoc[i];

		std::string Mask_utf8 = unicode_to_utf8_string( A.GetMask().data() );
		std::string Description_utf8 = unicode_to_utf8_string( A.GetDescription().data() );
		std::string Execute_utf8 = unicode_to_utf8_string( A.GetExecuteCommand().data() );
		std::string ExecuteSecondary_utf8 = unicode_to_utf8_string( A.GetExecuteCommandSecondary().data() );
		std::string View_utf8 = unicode_to_utf8_string( A.GetViewCommand().data() );
		std::string ViewSecondary_utf8 = unicode_to_utf8_string( A.GetViewCommandSecondary().data() );
		std::string Edit_utf8 = unicode_to_utf8_string( A.GetEditCommand().data() );
		std::string EditSecondary_utf8 = unicode_to_utf8_string( A.GetEditCommandSecondary().data() );

		Cfg.Write( "Mask%i", i, Mask_utf8 );
		Cfg.Write( "Description%i", i, Description_utf8 );
		Cfg.Write( "Execute%i", i, Execute_utf8 );
		Cfg.Write( "ExecuteSecondary%i", i, ExecuteSecondary_utf8 );
		Cfg.Write( "View%i", i, View_utf8 );
		Cfg.Write( "ViewSecondary%i", i, ViewSecondary_utf8 );
		Cfg.Write( "Edit%i", i, Edit_utf8 );
		Cfg.Write( "EditSecondary%i", i, EditSecondary_utf8 );
		Cfg.WriteBool( "HasTerminal%i", i, A.GetHasTerminal() );
	}

	// end marker
	Cfg.Write( "Mask%i", Assoc.size(), "" );
}

void LoadFileAssociations( NCWin* nc
#ifndef _WIN32
                           , IniHash& hash
#endif
                         )
{
	if ( !nc ) { return; }

#if defined(_WIN32)
	clConfigReader Cfg;
#else
	clConfigReader Cfg( hash );
#endif
	Cfg.SetSectionName( FilesAssociationsSection );

	int i = 0;

	std::vector<clNCFileAssociation> Assoc;

	while ( true )
	{
		std::string Mask = Cfg.Read( "Mask%i", i );
		std::string Description = Cfg.Read( "Description%i", i );
		std::string Execute = Cfg.Read( "Execute%i", i );
		std::string ExecuteSecondary = Cfg.Read( "ExecuteSecondary%i", i );
		std::string View = Cfg.Read( "View%i", i );
		std::string ViewSecondary = Cfg.Read( "ViewSecondary%i", i );
		std::string Edit = Cfg.Read( "Edit%i", i );
		std::string EditSecondary = Cfg.Read( "EditSecondary%i", i );
		bool HasTerminal = Cfg.ReadBool( "HasTerminal%i", i, true );

		if ( Mask.empty() ) { break; }

		clNCFileAssociation A;
		A.SetMask( utf8str_to_unicode( Mask ) );
		A.SetDescription( utf8str_to_unicode( Description ) );
		A.SetExecuteCommand( utf8str_to_unicode( Execute ) );
		A.SetExecuteCommandSecondary( utf8str_to_unicode( ExecuteSecondary ) );
		A.SetViewCommand( utf8str_to_unicode( View ) );
		A.SetViewCommandSecondary( utf8str_to_unicode( ViewSecondary ) );
		A.SetEditCommand( utf8str_to_unicode( Edit ) );
		A.SetEditCommandSecondary( utf8str_to_unicode( EditSecondary ) );
		A.SetHasTerminal( HasTerminal );
		Assoc.push_back( A );

		i++;
	}

	g_Env.SetFileAssociations( Assoc );
}

void SaveUserMenu( NCWin* nc
#ifndef _WIN32
                   , IniHash& hash
#endif
                 )
{
	if ( !nc ) { return; }

#if defined(_WIN32)
	clConfigWriter Cfg;
#else
	clConfigWriter Cfg( hash );
#endif
	Cfg.SetSectionName( UserMenuSection );

	const std::vector<clNCUserMenuItem>& Items = g_Env.GetUserMenuItems();

	for ( size_t i = 0; i < Items.size(); i++ )
	{
		const clNCUserMenuItem& A = Items[i];

		std::string Description_utf8 = unicode_to_utf8_string( A.GetDescription().GetRawText() );
		std::string Execute_utf8 = unicode_to_utf8_string( A.GetCommand().data() );

		Cfg.Write( "Description%i", i, Description_utf8 );
		Cfg.Write( "Execute%i", i, Execute_utf8 );
	}

	// end marker
	Cfg.Write( "Mask%i", Items.size(), "" );
}

void LoadUserMenu( NCWin* nc
#ifndef _WIN32
                   , IniHash& hash
#endif
                 )
{
	if ( !nc ) { return; }

#if defined(_WIN32)
	clConfigReader Cfg;
#else
	clConfigReader Cfg( hash );
#endif
	Cfg.SetSectionName( UserMenuSection );

	int i = 0;

	std::vector<clNCUserMenuItem> Items;

	while ( true )
	{
		std::string Description = Cfg.Read( "Description%i", i );
		std::string Execute = Cfg.Read( "Execute%i", i );

		if ( Description.empty() ) { break; }

		clNCUserMenuItem A;
		A.SetDescription( utf8str_to_unicode( Description ) );
		A.SetCommand( utf8str_to_unicode( Execute ) );
		Items.push_back( A );

		i++;
	}

	g_Env.SetUserMenuItems( Items );
}

void SaveFileHighlightingRules( NCWin* nc
#ifndef _WIN32
                                , IniHash& hash
#endif
                              )
{
	if ( !nc ) { return; }

#if defined(_WIN32)
	clConfigWriter Cfg;
#else
	clConfigWriter Cfg( hash );
#endif
	Cfg.SetSectionName( HighlightingRulesSection );

	const std::vector<clNCFileHighlightingRule>& Rules = g_Env.GetFileHighlightingRules();

	for ( size_t i = 0; i < Rules.size(); i++ )
	{
		const clNCFileHighlightingRule& A = Rules[i];

		std::string Mask_utf8 = unicode_to_utf8_string( A.GetMask().data() );
		std::string Description_utf8 = unicode_to_utf8_string( A.GetDescription().data() );

		char Buf[0xFFFF];
		Lsnprintf( Buf, sizeof( Buf ) - 1, "Min = %" PRIu64 " Max = %" PRIu64 " Attribs = %" PRIu64, A.GetSizeMin(), A.GetSizeMax(), A.GetAttributesMask() );

		Cfg.Write( "Mask%i", i, Mask_utf8 );
		Cfg.Write( "Description%i", i, Description_utf8 );
		Cfg.Write( "SizeAttribs%i", i, Buf );
		Cfg.WriteInt( "ColorNormal%i", i, A.GetColorNormal() );
		Cfg.WriteInt( "ColorNormalBackground%i", i, A.GetColorNormalBackground() );
		Cfg.WriteInt( "ColorSelected%i", i, A.GetColorSelected() );
		Cfg.WriteInt( "ColorSelectedBackground%i", i, A.GetColorSelectedBackground() );
		Cfg.WriteInt( "ColorUnderCursorNormal%i", i, A.GetColorUnderCursorNormal() );
		Cfg.WriteInt( "ColorUnderCursorNormalBackground%i", i, A.GetColorUnderCursorNormalBackground() );
		Cfg.WriteInt( "ColorUnderCursorSelected%i", i, A.GetColorUnderCursorSelected() );
		Cfg.WriteInt( "ColorUnderCursorSelectedBackground%i", i, A.GetColorUnderCursorSelectedBackground() );
		Cfg.WriteBool( "MaskEnabled%i", i, A.IsMaskEnabled() );
	}

	// end marker
	Cfg.Write( "Mask%i", Rules.size(), "" );
}

void LoadFileHighlightingRules( NCWin* nc
#ifndef _WIN32
                                , IniHash& hash
#endif
                              )
{
	if ( !nc ) { return; }

#if defined(_WIN32)
	clConfigReader Cfg;
#else
	clConfigReader Cfg( hash );
#endif
	Cfg.SetSectionName( HighlightingRulesSection );

	int i = 0;

	std::vector<clNCFileHighlightingRule> Rules;

	while ( true )
	{
		std::string Mask = Cfg.Read( "Mask%i", i );
		std::string Description = Cfg.Read( "Description%i", i );

		std::string Line = Cfg.Read( "SizeAttribs%i", i );

		uint64_t SizeMin, SizeMax, AttribsMask;

		int NumRead = 0;

		if ( !Line.empty() )
		{
			NumRead = Lsscanf( Line.c_str(), "Min = %" PRIu64 " Max = %" PRIu64 " Attribs = %" PRIu64, &SizeMin, &SizeMax, &AttribsMask );
		}

		if ( NumRead != 3 )
		{
			SizeMin = 0;
			SizeMax = 0;
			AttribsMask = 0;
		}

		if ( !Mask.data() || !*Mask.data() ) { break; }

		clNCFileHighlightingRule R;
		R.SetMask( utf8str_to_unicode( Mask ) );
		R.SetDescription( utf8str_to_unicode( Description ) );
		R.SetSizeMin( SizeMin );
		R.SetSizeMax( SizeMax );
		R.SetAttributesMask( AttribsMask );
		R.SetColorNormal( Cfg.ReadInt( "ColorNormal%i", i, R.GetColorNormal() ) );
		R.SetColorNormalBackground( Cfg.ReadInt( "ColorNormalBackground%i", i, R.GetColorNormalBackground() ) );
		R.SetColorSelected( Cfg.ReadInt( "ColorSelected%i", i, R.GetColorSelected() ) );
		R.SetColorSelectedBackground( Cfg.ReadInt( "ColorSelectedBackground%i", i, R.GetColorSelectedBackground() ) );
		R.SetColorUnderCursorNormal( Cfg.ReadInt( "ColorUnderCursorNormal%i", i, R.GetColorUnderCursorNormal() ) );
		R.SetColorUnderCursorNormalBackground( Cfg.ReadInt( "ColorUnderCursorNormalBackground%i", i, R.GetColorUnderCursorNormalBackground( ) ) );
		R.SetColorUnderCursorSelected( Cfg.ReadInt( "ColorUnderCursorSelected%i", i, R.GetColorUnderCursorSelected() ) );
		R.SetColorUnderCursorSelectedBackground( Cfg.ReadInt( "ColorUnderCursorSelectedBackground%i", i, R.GetColorUnderCursorSelectedBackground() ) );

		Rules.push_back( R );

		i++;
	}

	g_Env.SetFileHighlightingRules( Rules );
}

void SaveCommandsHistory( NCWin* nc
#ifndef _WIN32
                          , IniHash& hash
#endif
                        )
{
	if ( !nc ) { return; }

#if defined(_WIN32)
	clConfigWriter Cfg;
#else
	clConfigWriter Cfg( hash );
#endif
	Cfg.SetSectionName( CommandsHistorySection );

	int Count = nc->GetHistory()->Count();

	for ( int i = 0; i < Count; i++ )
	{
		const unicode_t* Hist = ( *nc->GetHistory() )[Count - i - 1];

		Cfg.Write( "Command%i", i, unicode_to_utf8_string( Hist ) );
	}
}

void LoadCommandsHistory( NCWin* nc
#ifndef _WIN32
                          , IniHash& hash
#endif
                        )
{
	if ( !nc ) { return; }

#if defined(_WIN32)
	clConfigReader Cfg;
#else
	clConfigReader Cfg( hash );
#endif
	Cfg.SetSectionName( CommandsHistorySection );

	nc->GetHistory()->Clear();

	int i = 0;

	while ( true )
	{
		std::string Cmd = Cfg.Read( "Command%i", i );

		if ( Cmd.empty() ) { break; }

		nc->GetHistory()->Put( utf8str_to_unicode( Cmd ).data() );

		i++;
	}
}

bool FileExists( const char* name )
{
	struct stat sb;

	if ( stat( name, &sb ) ) { return false; }

	return true;
}

void clWcmConfig::Load( NCWin* nc, const std::string& StartupDir )
{
#ifdef _WIN32

	for ( size_t i = 0; i < m_MapList.size(); i++ )
	{
		sNode& Node = m_MapList[i];

		if ( Node.m_Type == MT_BOOL && Node.m_Current.m_Bool != 0 )
		{
			*Node.m_Current.m_Bool = RegReadInt( Node.m_Section, Node.m_Name, Node.GetDefaultBool() ) != 0;
		}
		else if ( Node.m_Type == MT_INT && Node.m_Current.m_Int != 0 )
		{
			*Node.m_Current.m_Int = RegReadInt( Node.m_Section, Node.m_Name, Node.GetDefaultInt() );
		}
		else if ( Node.m_Type == MT_STR && Node.m_Current.m_Str != 0 )
		{
			*Node.m_Current.m_Str = RegReadString( Node.m_Section, Node.m_Name, Node.GetDefaultStr() );
		}
	}


	LoadCommandsHistory( nc );
	LoadFileAssociations( nc );
	LoadFileHighlightingRules( nc );
	LoadUserMenu( nc );

#else
	IniHash hash;
	FSPath path = configDirPath;
	path.Push( CS_UTF8, "config" );

	LoadIniHash( hash, DEFAULT_CONFIG_PATH );
	LoadIniHash( hash, ( sys_char_t* )path.GetString( sys_charset_id ) );

	for ( size_t i = 0; i < m_MapList.size(); i++ )
	{
		sNode& Node = m_MapList[i];

		if ( Node.m_Type == MT_BOOL && Node.m_Current.m_Bool != 0 )
		{
			*Node.m_Current.m_Bool = hash.GetBoolValue( Node.m_Section, Node.m_Name, Node.GetDefaultBool() );
		}
		else if ( Node.m_Type == MT_INT && Node.m_Current.m_Int != 0 )
		{
			*Node.m_Current.m_Int = hash.GetIntValue( Node.m_Section, Node.m_Name, Node.GetDefaultInt() );
		}
		else if ( Node.m_Type == MT_STR && Node.m_Current.m_Str != 0 )
		{
			const char* s = hash.GetStrValue( Node.m_Section, Node.m_Name, Node.GetDefaultStr() );

			if ( s )
			{
				*Node.m_Current.m_Str = s;
			}
			else
			{
				( *Node.m_Current.m_Str ).clear();
			}
		}

	}

	LoadCommandsHistory( nc, hash );
	LoadFileAssociations( nc, hash );
	LoadFileHighlightingRules( nc, hash );
	LoadUserMenu( nc, hash );

#endif

	if ( editTabSize <= 0 || editTabSize > 64 ) { editTabSize = 3; }

	LoadFoldersHistory();
	LoadViewHistory();
	LoadFieldsHistory();
}

void clWcmConfig::Save( NCWin* nc )
{
	if ( nc )
	{
		leftPanelPath = nc->GetLeftPanel( )->UriOfDir().GetUtf8();
		rightPanelPath = nc->GetRightPanel( )->UriOfDir( ).GetUtf8();
		crect Rect = nc->ScreenRect();
		windowX = Rect.top;
		windowY = Rect.left;
		windowWidth = Rect.Width();
		windowHeight = Rect.Height();
	}

#ifdef _WIN32

	for ( size_t i = 0; i < m_MapList.size( ); i++ )
	{
		sNode& Node = m_MapList[i];

		if ( Node.m_Type == MT_BOOL && Node.m_Current.m_Bool != 0 )
		{
			RegWriteInt( Node.m_Section, Node.m_Name, *Node.m_Current.m_Bool );
		}
		else if ( Node.m_Type == MT_INT && Node.m_Current.m_Int != 0 )
		{
			RegWriteInt( Node.m_Section, Node.m_Name, *Node.m_Current.m_Int );
		}
		else if ( Node.m_Type == MT_STR && Node.m_Current.m_Str != 0 )
		{
			RegWriteString( Node.m_Section, Node.m_Name, Node.m_Current.m_Str->data() );
		}
	}

	SaveCommandsHistory( nc );
	SaveFileAssociations( nc );
	SaveFileHighlightingRules( nc );
	SaveUserMenu( nc );

#else
	IniHash hash;
	FSPath path = configDirPath;
	path.Push( CS_UTF8, "config" );
	
	LoadIniHash( hash, ( sys_char_t* )path.GetString( sys_charset_id ) );

	for ( size_t i = 0; i < m_MapList.size(); i++ )
	{
		sNode& Node = m_MapList[i];

		if ( Node.m_Type == MT_BOOL && Node.m_Current.m_Bool != 0 )
		{
			hash.SetBoolValue( Node.m_Section, Node.m_Name, *Node.m_Current.m_Bool );
		}
		else if ( Node.m_Type == MT_INT && Node.m_Current.m_Int != 0 )
		{
			hash.SetIntValue( Node.m_Section, Node.m_Name, *Node.m_Current.m_Int );
		}
		else if ( Node.m_Type == MT_STR && Node.m_Current.m_Str != 0 )
		{
			hash.SetStrValue( Node.m_Section, Node.m_Name, Node.m_Current.m_Str->data() );
		}
	}

	SaveCommandsHistory( nc, hash );
	SaveFileAssociations( nc, hash );
	SaveFileHighlightingRules( nc, hash );
	SaveUserMenu( nc, hash );

	SaveIniHash( hash,( sys_char_t* ) path.GetString( sys_charset_id ) );
#endif

	SaveFoldersHistory();
	SaveViewHistory();
	SaveFieldsHistory();
}


void InitConfigPath()
{
#if !defined( _WIN32 )

	const sys_char_t* home = ( sys_char_t* ) getenv( "HOME" );

	if ( home )
	{
		FSPath path( sys_charset_id, home );
		path.Push( CS_UTF8, ".wcm" );
		FSSys fs;
		FSStat st;
		int err;

		if ( fs.Stat( path, &st, &err, 0 ) )
		{
			if ( fs.IsENOENT( err ) ) //directory not exists
			{
				if ( fs.MkDir( path, 0700, &err,  0 ) )
				{
					fprintf( stderr, "can't create config directory %s (%s)", path.GetUtf8(), fs.StrError( err ).GetUtf8() );
					return;
				}

			}
			else
			{
				fprintf( stderr, "can't create config directory statuc %s (%s)", path.GetUtf8(), fs.StrError( err ).GetUtf8() );
				return;
			}
		}
		else if ( !st.IsDir() )
		{
			fprintf( stderr, "err: '%s' is not directory", path.GetUtf8() );
			return;
		}

		configDirPath = path;
	}
	else
	{
		fprintf( stderr, "err: HOME env value not found" );
	}

#endif
}



////////////////////////////////  PanelOptDlg

class PanelOptDialog: public NCVertDialog
{
	Layout iL;
public:
	SButton  showHiddenButton;
	SButton  caseSensitive;
	SButton  selectFolders;
	SButton  showDotsInRoot;
	SButton  showFolderIcons;
	SButton  showExecutableIcons;
	SButton  showLinkIcons;
	SButton  showScrollbar;

	StaticLine showSpacesStatic;
	SButton  showSpacesNoneButton;
	SButton  showSpacesAllButton;
	SButton  showSpacesTrailingButton;

	PanelOptDialog( NCDialogParent* parent );
	virtual ~PanelOptDialog();
};

PanelOptDialog::~PanelOptDialog() {}

PanelOptDialog::PanelOptDialog( NCDialogParent* parent )
	:  NCVertDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( _LT( "Panel settings" ) ).data(), bListOkCancel )
	, iL( 16, 3 )
	, showHiddenButton( 0, this, utf8_to_unicode( _LT( "Show &hidden files" ) ).data(), 0, g_WcmConfig.panelShowHiddenFiles )
	, caseSensitive( 0, this, utf8_to_unicode( _LT( "C&ase sensitive sort" ) ).data(), 0, g_WcmConfig.panelCaseSensitive )
	, selectFolders( 0, this, utf8_to_unicode( _LT( "Select &folders" ) ).data(), 0, g_WcmConfig.panelSelectFolders )
	, showDotsInRoot( 0, this, utf8_to_unicode( _LT( "Show .. in the &root folder" ) ).data(), 0, g_WcmConfig.panelShowDotsInRoot )
	, showFolderIcons( 0, this, utf8_to_unicode( _LT( "Show folder &icons" ) ).data(), 0, g_WcmConfig.panelShowFolderIcons )
	, showExecutableIcons( 0, this, utf8_to_unicode( _LT( "Show &executable icons" ) ).data(), 0, g_WcmConfig.panelShowExecutableIcons )
	, showLinkIcons( 0, this, utf8_to_unicode( _LT( "Show &link icons" ) ).data(), 0, g_WcmConfig.panelShowLinkIcons )
	, showScrollbar( 0, this, utf8_to_unicode( _LT( "Show &scrollbar" ) ).data(), 0, g_WcmConfig.panelShowScrollbar )
	, showSpacesStatic( 0, this, utf8_to_unicode( _LT( "Show spaces as · in names:" ) ).data() )
	, showSpacesNoneButton( 0, this, utf8_to_unicode( _LT( "No" ) ).data(), 1, g_WcmConfig.panelShowSpacesMode != 1 && g_WcmConfig.panelShowSpacesMode != 2 )
	, showSpacesAllButton( 0, this,  utf8_to_unicode( _LT( "All" ) ).data(), 1, g_WcmConfig.panelShowSpacesMode == 1 )
	, showSpacesTrailingButton( 0, this, utf8_to_unicode( _LT( "Trailing only" ) ).data(), 1, g_WcmConfig.panelShowSpacesMode == 2 )
{
	iL.AddWinAndEnable( &showHiddenButton,  0, 0 );
	showHiddenButton.SetFocus();
	iL.AddWinAndEnable( &caseSensitive,  1, 0 );
	iL.AddWinAndEnable( &selectFolders,  2, 0 );
	iL.AddWinAndEnable( &showDotsInRoot, 3, 0 );
	iL.AddWinAndEnable( &showFolderIcons, 4, 0 );
	iL.AddWinAndEnable( &showExecutableIcons, 5, 0 );
	iL.AddWinAndEnable( &showLinkIcons, 6, 0 );
	iL.AddWinAndEnable( &showScrollbar, 7, 0 );
	iL.AddWinAndEnable( &showSpacesStatic, 8, 0 );
	iL.AddWinAndEnable( &showSpacesNoneButton, 9, 0 );
	iL.AddWinAndEnable( &showSpacesAllButton, 10, 0 );
	iL.AddWinAndEnable( &showSpacesTrailingButton, 11, 0 );

	AddLayout( &iL );
	SetEnterCmd( CMD_OK );

	showHiddenButton.SetFocus();

	order.append( &showHiddenButton );
	order.append( &caseSensitive );
	order.append( &selectFolders );
	order.append( &showDotsInRoot );
	order.append( &showFolderIcons );
	order.append( &showExecutableIcons );
	order.append( &showLinkIcons );
	order.append( &showScrollbar );
	order.append( &showSpacesNoneButton );
	order.append( &showSpacesAllButton );
	order.append( &showSpacesTrailingButton );
	SetPosition();
}

bool DoPanelConfigDialog( NCDialogParent* parent )
{
	PanelOptDialog dlg( parent );

	if ( dlg.DoModal() == CMD_OK )
	{
		g_WcmConfig.panelShowHiddenFiles = dlg.showHiddenButton.IsSet();
		g_WcmConfig.panelCaseSensitive = dlg.caseSensitive.IsSet();
		g_WcmConfig.panelSelectFolders = dlg.selectFolders.IsSet();
		g_WcmConfig.panelShowDotsInRoot = dlg.showDotsInRoot.IsSet();
		g_WcmConfig.panelShowFolderIcons  = dlg.showFolderIcons.IsSet();
		g_WcmConfig.panelShowExecutableIcons  = dlg.showExecutableIcons.IsSet();
		g_WcmConfig.panelShowLinkIcons  = dlg.showLinkIcons.IsSet();
		g_WcmConfig.panelShowScrollbar = dlg.showScrollbar.IsSet();

		if ( dlg.showSpacesTrailingButton.IsSet() )
		{
			g_WcmConfig.panelShowSpacesMode = ePanelSpacesMode_Trailing;
		}
		else if ( dlg.showSpacesAllButton.IsSet() )
		{
			g_WcmConfig.panelShowSpacesMode = ePanelSpacesMode_All;
		}
		else
		{
			g_WcmConfig.panelShowSpacesMode = ePanelSpacesMode_None;
		}

		return true;
	}

	return false;
}


////////////////////////////////  EditOptDlg


class EditOptDialog: public NCVertDialog
{
	Layout iL;
public:
	SButton  saveFilePosButton;
	SButton  autoIdentButton;
	SButton  shlButton;
	SButton  clearHistoryButton;

	StaticLabel tabText;
	EditLine tabEdit;

	EditOptDialog( NCDialogParent* parent );
//	virtual bool EventChildKey(Win* child, cevent_key* pEvent);
//	virtual bool Command(int id, int subId, Win *win, void *data);
	virtual ~EditOptDialog();
};

EditOptDialog::~EditOptDialog() {}

EditOptDialog::EditOptDialog( NCDialogParent* parent )
	:  NCVertDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( _LT( "Editor" ) ).data(), bListOkCancel ),
	   iL( 16, 2 ),

	   saveFilePosButton( 0, this, utf8_to_unicode( _LT( "Save file &position" ) ).data(), 0, g_WcmConfig.editSavePos ),
	   autoIdentButton( 0, this, utf8_to_unicode( _LT( "Auto &indent" ) ).data(), 0, g_WcmConfig.editAutoIdent ),
	   shlButton( 0, this, utf8_to_unicode( _LT( "Syntax &highlighting" ) ).data(), 0, g_WcmConfig.editShl ),
	   clearHistoryButton( 0, this, utf8_to_unicode( _LT( "&Clear history after saving" ) ).data(), 0, g_WcmConfig.editClearHistoryAfterSaving ),
	   tabText( 0, this, utf8_to_unicode( _LT( "&Tab size:" ) ).data(), &tabEdit ),
	   tabEdit( 0, this, 0, 0, 16 )
{
	char buf[0x100];
	Lsnprintf( buf, sizeof( buf ) - 1, "%i", g_WcmConfig.editTabSize );
	tabEdit.SetText( utf8_to_unicode( buf ).data(), true );

	iL.AddWinAndEnable( &saveFilePosButton,  0, 0, 0, 1 );
	iL.AddWinAndEnable( &autoIdentButton,    1, 0, 1, 1 );
	iL.AddWinAndEnable( &shlButton,          2, 0, 2, 1 );
	iL.AddWinAndEnable( &clearHistoryButton, 3, 0, 3, 1 );
	iL.AddWinAndEnable( &tabText,            4, 0, 4, 0 );
	iL.AddWinAndEnable( &tabEdit,            4, 1, 5, 1 );
	AddLayout( &iL );
	SetEnterCmd( CMD_OK );

	saveFilePosButton.SetFocus();

	order.append( &saveFilePosButton );
	order.append( &autoIdentButton );
	order.append( &shlButton );
	order.append( &clearHistoryButton );
	order.append( &tabEdit );
	SetPosition();
}

bool DoEditConfigDialog( NCDialogParent* parent )
{
	EditOptDialog dlg( parent );

	if ( dlg.DoModal() == CMD_OK )
	{
		g_WcmConfig.editSavePos = dlg.saveFilePosButton.IsSet();
		g_WcmConfig.editAutoIdent = dlg.autoIdentButton.IsSet();
		g_WcmConfig.editShl = dlg.shlButton.IsSet();
		g_WcmConfig.editClearHistoryAfterSaving = dlg.clearHistoryButton.IsSet();

		int tabSize = atoi( unicode_to_utf8( dlg.tabEdit.GetText().data() ).data() );

		if ( tabSize > 0 && tabSize <= 64 )
		{
			g_WcmConfig.editTabSize = tabSize;
		}

		return true;
	}

	return false;
}



////////////////////////// StyleOptDialog

class StyleOptDialog: public NCVertDialog
{
	void RefreshFontInfo();
	Layout iL;
public:
	struct Node
	{
		std::string name;
		cfont* oldFont;
		std::string* pUri;
		clPtr<cfont> newFont;
		bool fixed;
		Node(): oldFont( 0 ) {}
		Node( const char* n, bool fix,  cfont* old, std::string* uri )
			: name( n )
			, oldFont( old )
			, pUri( uri )
			, fixed( fix )
		{}
	};

	ccollect<Node>* pList;

	SButton  styleShow3DUIButton;
	StaticLine colorStatic;
	SButton  styleDefButton;
	SButton  styleBlackButton;
	SButton  styleWhiteButton;

	StaticLine showStatic;
	SButton  showToolbarButton;
	SButton  showButtonbarButton;
	SButton  showButtonbarIconsButton;
	SButton  showMenubarButton;

	StaticLine fontsStatic;
	TextList fontList;
	StaticLine fontNameStatic;
	Button changeButton;
	Button changeX11Button;


	StyleOptDialog( NCDialogParent* parent, ccollect<Node>* p );
	virtual bool EventChildKey( Win* child, cevent_key* pEvent );
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual ~StyleOptDialog();
};

StyleOptDialog::~StyleOptDialog() {}

void StyleOptDialog::RefreshFontInfo()
{
	int count = pList->count();
	int cur = fontList.GetCurrent();

	const char* s = "";

	if ( count >= 0 && cur >= 0 && cur < count )
	{
		int n = fontList.GetCurrentInt();

		if ( pList->get( n ).newFont.ptr() )
		{
			s = pList->get( n ).newFont->printable_name();
		}
		else if ( pList->get( n ).oldFont )
		{
			s = pList->get( n ).oldFont->printable_name();
		}
	}

	fontNameStatic.SetText( utf8_to_unicode( s ).data() );
}

#define CMD_CHFONT 1000
#define CMD_CHFONTX11 1001

StyleOptDialog::StyleOptDialog( NCDialogParent* parent, ccollect<Node>* p )
	:  NCVertDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( _LT( "Style" ) ).data(), bListOkCancel ),
	   iL( 16, 3 ),
	   pList( p ),
	   styleShow3DUIButton( 0, this, utf8_to_unicode( _LT( "&3D buttons" ) ).data(), 0, g_WcmConfig.styleShow3DUI ),
	   colorStatic( 0, this, utf8_to_unicode( _LT( "Colors:" ) ).data() ),
	   styleDefButton( 0, this, utf8_to_unicode( _LT( "&Default colors" ) ).data(), 1, g_WcmConfig.styleColorMode != 1 && g_WcmConfig.styleColorMode != 2 ),
	   styleBlackButton( 0, this,  utf8_to_unicode( _LT( "&Black" ) ).data(), 1, g_WcmConfig.styleColorMode == 1 ),
	   styleWhiteButton( 0, this, utf8_to_unicode( _LT( "&White" ) ).data(), 1, g_WcmConfig.styleColorMode == 2 ),

	   showStatic( 0, this, utf8_to_unicode( _LT( "Items:" ) ).data() ),
	   showToolbarButton( 0, this, utf8_to_unicode( _LT( "Show &toolbar" ) ).data(), 0, g_WcmConfig.styleShowToolBar ),
	   showButtonbarButton( 0, this, utf8_to_unicode( _LT( "Show b&uttonbar" ) ).data(), 0, g_WcmConfig.styleShowButtonBar ),
	   showButtonbarIconsButton( 0, this, utf8_to_unicode( _LT( "Show buttonbar &icons" ) ).data(), 0, g_WcmConfig.styleShowButtonBarIcons ),
	   showMenubarButton( 0, this, utf8_to_unicode( _LT( "Show &menubar" ) ).data(), 0, g_WcmConfig.styleShowMenuBar ),

	   fontsStatic( 0, this, utf8_to_unicode( _LT( "Fonts:" ) ).data() ),
	   fontList( Win::WT_CHILD, WH_TABFOCUS | WH_CLICKFOCUS, 0, this, VListWin::SINGLE_SELECT, VListWin::BORDER_3D, 0 ),
	   fontNameStatic( 0, this, utf8_to_unicode( "--------------------------------------------------" ).data() ),
	   changeButton( 0, this, utf8_to_unicode( _LT( "Set &font..." ) ).data(), CMD_CHFONT ),
	   changeX11Button( 0, this, utf8_to_unicode( _LT( "Set &X11 font..." ) ).data(), CMD_CHFONTX11 )
{
	iL.AddWinAndEnable( &styleShow3DUIButton, 0, 1 );
	iL.AddWinAndEnable( &colorStatic, 1, 0 );
	iL.AddWinAndEnable( &styleDefButton, 2, 1 );
	iL.AddWinAndEnable( &styleBlackButton,  3, 1 );
	iL.AddWinAndEnable( &styleWhiteButton,  4, 1 );
	iL.AddWinAndEnable( &showStatic,  5, 0 );
	iL.AddWinAndEnable( &showToolbarButton, 6, 1 );
	iL.AddWinAndEnable( &showButtonbarButton,  7, 1 );
	iL.AddWinAndEnable( &showButtonbarIconsButton,  8, 1 );
	iL.AddWinAndEnable( &showMenubarButton,  9, 1 );
	iL.LineSet( 10, 10 );
	iL.AddWinAndEnable( &fontsStatic, 11, 0 );
	iL.ColSet( 0, 12, 12, 12 );
	iL.SetColGrowth( 1 );

	for ( int i = 0; i < pList->count(); i++ )
	{
		fontList.Append( utf8_to_unicode( pList->get( i ).name.data() ).data(), i );
	}

	fontList.MoveCurrent( 0 );
	RefreshFontInfo();

	LSize lsize = fontList.GetLSize();
	lsize.y.minimal = lsize.y.ideal = 100;
	lsize.y.maximal = 1000;
	lsize.x.minimal = lsize.x.ideal = 300;
	lsize.x.maximal = 1000;
	fontList.SetLSize( lsize );

	iL.AddWinAndEnable( &fontList, 11, 1 );

	fontNameStatic.Enable();
	fontNameStatic.Show();

	lsize = fontNameStatic.GetLSize();
	lsize.x.minimal = 500;
	lsize.x.maximal = 1000;
	fontNameStatic.SetLSize( lsize );

	iL.AddWin( &fontNameStatic, 12, 1 );
#ifdef USEFREETYPE
	iL.AddWinAndEnable( &changeButton, 13, 1 );
#endif

#ifdef _WIN32
	iL.AddWinAndEnable( &changeButton, 13, 1 );
#else
	iL.AddWinAndEnable( &changeX11Button, 14, 1 );
	iL.LineSet( 13, 10 );
#endif

#ifdef USEFREETYPE
	LSize l1 = changeButton.GetLSize();
	LSize l2 = changeX11Button.GetLSize();

	if ( l1.x.minimal < l2.x.minimal ) { l1.x.minimal = l2.x.minimal; }

	l1.x.maximal = l1.x.minimal;
	changeButton.SetLSize( l1 );
	changeX11Button.SetLSize( l1 );
#endif

	AddLayout( &iL );
	SetEnterCmd( CMD_OK );

	styleDefButton.SetFocus();

	order.append( &styleShow3DUIButton );
	order.append( &styleDefButton );
	order.append( &styleBlackButton );
	order.append( &styleWhiteButton );
	order.append( &showToolbarButton );
	order.append( &showButtonbarButton );
	order.append( &showButtonbarIconsButton );
	order.append( &showMenubarButton );
	order.append( &fontList );
	order.append( &changeButton );
	order.append( &changeX11Button );
	SetPosition();
}

bool StyleOptDialog::Command( int id, int subId, Win* win, void* data )
{

	if ( win == &fontList )
	{
		RefreshFontInfo();
		return true;
	}

#ifdef _WIN32

	if ( id == CMD_CHFONT )
	{
		int count = pList->count();
		int cur = fontList.GetCurrent();

		if ( count <= 0 || cur < 0 || cur >= count ) { return true; }

		LOGFONT lf;
		std::string* pUri = pList->get( fontList.GetCurrentInt() ).pUri;
		cfont::UriToLogFont( &lf, pUri && pUri->data() ?  pUri->data() : 0 );

		CHOOSEFONT cf;
		memset( &cf, 0, sizeof( cf ) );
		cf.lStructSize = sizeof( cf );
		cf.hwndOwner = GetID();
		cf.lpLogFont = &lf;
		cf.Flags = CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT ;

		if ( pList->get( fontList.GetCurrentInt() ).fixed )
		{
			cf.Flags |= CF_FIXEDPITCHONLY;
		}


		if ( ChooseFont( &cf ) )
		{
			clPtr<cfont> p = new cfont( cfont::LogFontToUru( lf ).data() );

			if ( p.ptr() )
			{
				pList->get( fontList.GetCurrentInt() ).newFont = p;
				RefreshFontInfo();
			}
		}

		return true;
	}

#else

	if ( id == CMD_CHFONT )
	{
		int count = pList->count();
		int cur = fontList.GetCurrent();

		if ( count <= 0 || cur < 0 || cur >= count ) { return true; }

		std::string* pUri = pList->get( fontList.GetCurrentInt() ).pUri;

		clPtr<cfont> p = SelectFTFont( ( NCDialogParent* )Parent(), pList->get( fontList.GetCurrentInt() ).fixed, ( pUri && !pUri->empty() ) ? pUri->c_str() : nullptr );

		if ( p.ptr() )
		{
			pList->get( fontList.GetCurrentInt() ).newFont = p;
			RefreshFontInfo();
		}

		return true;
	}

	if ( id == CMD_CHFONTX11 )
	{
		int count = pList->count();
		int cur = fontList.GetCurrent();

		if ( count <= 0 || cur < 0 || cur >= count ) { return true; }

		clPtr<cfont> p = SelectX11Font( ( NCDialogParent* )Parent(), pList->get( fontList.GetCurrentInt() ).fixed );

		if ( p.ptr() )
		{
			pList->get( fontList.GetCurrentInt() ).newFont = p;
			RefreshFontInfo();
		}

		return true;
	}

#endif

	return NCVertDialog::Command( id, subId, win, data );
}

bool StyleOptDialog::EventChildKey( Win* child, cevent_key* pEvent )
{
	if ( pEvent->Type() == EV_KEYDOWN )
	{
		bool IsReturn = ( pEvent->Key() == VK_RETURN ) && ( child == &changeButton || child == &changeX11Button );
		bool IsUpDown = ( pEvent->Key() == VK_UP || pEvent->Key() == VK_DOWN ) && child == &fontList;

		if ( IsReturn || IsUpDown )
		{
			return false;
		}
	};

	return NCVertDialog::EventChildKey( child, pEvent );
}

bool DoStyleConfigDialog( NCDialogParent* parent )
{
	g_WcmConfig.ImpCurrentFonts();
	ccollect<StyleOptDialog::Node> list;
	list.append( StyleOptDialog::Node(  _LT( "Panel" ) ,  false,   g_PanelFont.ptr(), &g_WcmConfig.panelFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Dialog" ),  false,   g_DialogFont.ptr(), &g_WcmConfig.dialogFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Viewer" ),  true,    g_ViewerFont.ptr(), &g_WcmConfig.viewerFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Editor" ),  true,    g_EditorFont.ptr(), &g_WcmConfig.editorFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Terminal" ),   true, g_TerminalFont.ptr(), &g_WcmConfig.terminalFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Help text" ),  false,   g_HelpTextFont.ptr(), &g_WcmConfig.helpTextFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Help bold text" ),   false,   g_HelpBoldFont.ptr(), &g_WcmConfig.helpBoldFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Help header text" ), false,   g_HelpHeadFont.ptr(), &g_WcmConfig.helpHeadFontUri ) );

	StyleOptDialog dlg( parent, &list );

	if ( dlg.DoModal() == CMD_OK )
	{
		if ( dlg.styleBlackButton.IsSet() )
		{
			g_WcmConfig.styleColorMode = 1;
		}
		else if ( dlg.styleWhiteButton.IsSet() )
		{
			g_WcmConfig.styleColorMode = 2;
		}
		else
		{
			g_WcmConfig.styleColorMode = 0;
		}

		SetColorStyle( g_WcmConfig.styleColorMode );

		g_WcmConfig.styleShow3DUI = dlg.styleShow3DUIButton.IsSet();
		g_WcmConfig.styleShowToolBar = dlg.showToolbarButton.IsSet( );
		g_WcmConfig.styleShowButtonBar = dlg.showButtonbarButton.IsSet( );
		g_WcmConfig.styleShowButtonBarIcons = dlg.showButtonbarIconsButton.IsSet( );
		g_WcmConfig.styleShowMenuBar = dlg.showMenubarButton.IsSet( );

		for ( int i = 0; i < list.count(); i++ )
		{
			if ( list[i].newFont.ptr() && list[i].newFont->uri()[0] && list[i].pUri )
			{
				*( list[i].pUri ) = list[i].newFont->uri();
			}
		}

		if ( parent ) { parent->Command( CMD_MENU_INFO, SCMD_MENU_CANCEL, nullptr, nullptr ); }

		return true;
	}

	return false;
}

struct LangListNode
{
	std::string id;
	std::string name;
	LangListNode() {}
	LangListNode( const char* i, const char* n ): id( i ), name( n ) {}
};

class CfgLangDialog: public NCDialog
{
	TextList _list;
	ccollect<LangListNode>* nodeList;
public:
	CfgLangDialog( NCDialogParent* parent, const char* id, ccollect<LangListNode>* nl )
		:  NCDialog( createDialogAsChild, 0, parent, utf8_to_unicode( _LT( "Language" ) ).data(), bListOkCancel ),
		   _list( Win::WT_CHILD, Win::WH_TABFOCUS | WH_CLICKFOCUS, 0, this, VListWin::SINGLE_SELECT, VListWin::BORDER_3D, 0 ),
		   nodeList( nl )

	{
		_list.Append( utf8_to_unicode( _LT( "Autodetect" ) ).data() ); //0
		_list.Append( utf8_to_unicode( _LT( "English" ) ).data() ); //1

		for ( int i = 0; i < nl->count(); i++ )
		{
			_list.Append( utf8_to_unicode( nl->get( i ).name.data() ).data() );
		}

		int cur = 0;

		if ( id[0] == '+' ) { cur = 0; }
		else if ( id[0] == '-' ) { cur = 1; }
		else
		{
			for ( int i = 0; i < nl->count(); i++ )
				if ( !strcmp( id, nl->get( i ).id.data() ) )
				{
					cur = i + 2;
					break;
				}
		}

		_list.MoveCurrent( cur );

		_list.Enable();
		_list.Show();
		_list.SetFocus();
		LSRange h( 10, 1000, 10 );
		LSRange w( 50, 1000, 30 );
		_list.SetHeightRange( h ); //in characters
		_list.SetWidthRange( w ); //in characters

		AddWin( &_list );
		SetEnterCmd( CMD_OK );
		SetPosition();
	};

	const char* GetId();

	virtual bool Command( int id, int subId, Win* win, void* data );

	virtual ~CfgLangDialog();
};

const char* CfgLangDialog::GetId()
{
	int n = _list.GetCurrent();

	if ( n <= 0 ) { return "+"; }

	if ( n == 1 ) { return "-"; }

	n -= 2;

	if ( n >= nodeList->count() ) { return "+"; }

	return nodeList->get( n ).id.data();
}

bool CfgLangDialog::Command( int id, int subId, Win* win, void* data )
{
	if ( id == CMD_ITEM_CLICK && win == &_list )
	{
		EndModal( CMD_OK );
	}

	return NCDialog::Command( id, subId, win, data );
}

CfgLangDialog::~CfgLangDialog() {}


inline bool IsSpace( char c ) { return c > 0 && c <= 0x20; }

static bool LangListLoad( sys_char_t* fileName, ccollect<LangListNode>& list )
{
	list.clear();

	try
	{
		BFile f;
		f.Open( fileName );
		char buf[4096];

		while ( f.GetStr( buf, sizeof( buf ) ) )
		{
			char* s = buf;

			while ( IsSpace( *s ) ) { s++; }

			if ( *s == '#' ) { continue; }

			if ( !*s ) { continue; }

			ccollect<char, 0x100> id;
			ccollect<char, 0x100> name;

			while ( *s && !IsSpace( *s ) )
			{
				id.append( *s );
				s++;
			}

			while ( IsSpace( *s ) ) { s++; }

			int lastNs = -1;

			for ( int i = 0; *s; i++, s++ )
			{
				if ( *s == '#' ) { break; }

				if ( !IsSpace( *s ) ) { lastNs = i; }

				name.append( *s );
			}

			if ( id.count() <= 0 || lastNs < 0 ) { continue; }

			id.append( 0 );
			name.append( 0 );
			name[lastNs + 1] = 0;

			LangListNode( id.ptr(), name.ptr() );
			list.append( LangListNode( id.ptr(), name.ptr() ) );
		}
	}
	catch ( cexception* ex )
	{
		ex->destroy();
		return false;
	}

	return true;
}


////////////////////////////////  SysOptDlg

class SysOptDialog: public NCVertDialog
{
	Layout m_iL;
public:
	std::string m_CurLangId;
	ccollect<LangListNode> m_List;
	void SetCurLang( const char* id );

	SButton  m_AskOpenExecButton;
	SButton  m_EscPanelButton;
	SButton  m_EscCommandLineButton;
	SButton  m_BackUpDirButton;
	SButton  m_AutoCompleteButton;
	SButton  m_AutoSaveSetupButton;
	SButton  m_ShowHostNameButton;
	SButton  m_StorePasswordsButton;

	StaticLabel m_LangStatic;
	StaticLine m_LangVal;
	Button m_LangButton;

	SysOptDialog( NCDialogParent* parent );
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual bool EventChildKey( Win* child, cevent_key* pEvent );
	virtual ~SysOptDialog();
};

void SysOptDialog::SetCurLang( const char* id )
{
	m_CurLangId = std::string( id );

	if ( id[0] == '-' )
	{
		m_LangVal.SetText( utf8_to_unicode( _LT( "English" ) ).data() );
	}
	else if ( id[0] == '+' )
	{
		m_LangVal.SetText( utf8_to_unicode( _LT( "Autodetect" ) ).data() );
	}
	else
	{
		for ( int i = 0; i < m_List.count(); i++ )
		{
			if ( !strcmp( m_List[i].id.data(), id ) )
			{
				m_LangVal.SetText( utf8_to_unicode( m_List[i].name.data( ) ).data( ) );
				return;
			}
		}

		m_LangVal.SetText( utf8_to_unicode( id ).data( ) );
	}
}

SysOptDialog::~SysOptDialog() {}

SysOptDialog::SysOptDialog( NCDialogParent* parent )
	: NCVertDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( _LT( "System settings" ) ).data(), bListOkCancel )
	, m_iL( 16, 3 )
	, m_AskOpenExecButton( 0, this, utf8_to_unicode( _LT( "Ask user if Exec/Open conflict" ) ).data(), 0, g_WcmConfig.systemAskOpenExec )
	, m_EscPanelButton( 0, this, utf8_to_unicode( _LT( "Enable &ESC key to show/hide panels" ) ).data(), 0, g_WcmConfig.systemEscPanel )
	, m_EscCommandLineButton( 0, this, utf8_to_unicode( _LT( "Enable &ESC key to clear the command line" ) ).data(), 0, g_WcmConfig.systemEscCommandLine )
	, m_BackUpDirButton( 0, this, utf8_to_unicode( _LT( "Enable &BACKSPACE key to go up dir" ) ).data(), 0, g_WcmConfig.systemBackSpaceUpDir )
	, m_AutoCompleteButton( 0, this, utf8_to_unicode( _LT( "Enable &autocomplete" ) ).data(), 0, g_WcmConfig.systemAutoComplete )
	, m_AutoSaveSetupButton( 0, this, utf8_to_unicode( _LT( "Auto &save setup" ) ).data(), 0, g_WcmConfig.systemAutoSaveSetup )
	, m_ShowHostNameButton( 0, this, utf8_to_unicode( _LT( "Show &host name" ) ).data(), 0, g_WcmConfig.systemShowHostName )
	, m_StorePasswordsButton( 0, this, utf8_to_unicode( _LT( "Store &passwords" ) ).data(), 0, g_WcmConfig.systemStorePasswords )
	, m_LangStatic( 0, this, utf8_to_unicode( _LT( "&Language:" ) ).data( ), &m_LangButton )
	, m_LangVal( 0, this, utf8_to_unicode( "______________________" ).data( ) )
	, m_LangButton( 0, this, utf8_to_unicode( ">" ).data( ), 1000 )
{

#ifndef _WIN32
	m_iL.AddWinAndEnable( &m_AskOpenExecButton, 0, 0, 0, 2 );
#endif
	m_iL.AddWinAndEnable( &m_EscPanelButton, 1, 0, 1, 2 );
	m_iL.AddWinAndEnable( &m_EscCommandLineButton, 2, 0, 2, 2 );
	m_iL.AddWinAndEnable( &m_BackUpDirButton, 3, 0, 3, 2 );
	m_iL.AddWinAndEnable( &m_AutoCompleteButton, 4, 0, 4, 2 );
	m_iL.AddWinAndEnable( &m_AutoSaveSetupButton, 5, 0, 5, 2 );
	m_iL.AddWinAndEnable( &m_ShowHostNameButton, 6, 0, 6, 2 );
	m_iL.AddWinAndEnable( &m_StorePasswordsButton, 7, 0, 7, 2 );

	m_iL.AddWinAndEnable( &m_LangStatic, 8, 0 );
	m_iL.AddWinAndEnable( &m_LangVal, 8, 2 );
	m_iL.AddWinAndEnable( &m_LangButton, 8, 1 );

	m_iL.SetColGrowth( 2 );

	AddLayout( &m_iL );
	SetEnterCmd( CMD_OK );

#ifndef _WIN32
	m_AskOpenExecButton.SetFocus();
	order.append( &m_AskOpenExecButton );
#endif
	order.append( &m_EscPanelButton );
	order.append( &m_EscCommandLineButton );
	order.append( &m_BackUpDirButton );
	order.append( &m_AutoCompleteButton );
	order.append( &m_AutoSaveSetupButton );
	order.append( &m_ShowHostNameButton );
	order.append( &m_StorePasswordsButton );
	order.append( &m_LangButton );

	SetPosition();

#ifdef _WIN32
	LangListLoad( carray_cat<sys_char_t>( GetAppPath( ).data( ), utf8_to_sys( "\\lang\\list" ).data( ) ).data( ), m_List );
#else

	if ( !LangListLoad( utf8_to_sys( "install-files/share/wcm/lang/list" ).data(), m_List ) )
	{
		LangListLoad( utf8_to_sys( UNIX_CONFIG_DIR_PATH "/lang/list" ).data(), m_List );
	}

#endif

	SetCurLang( g_WcmConfig.systemLang.data() ? g_WcmConfig.systemLang.data() : "+" );
}

bool SysOptDialog::Command( int id, int subId, Win* win, void* data )
{
	if ( id == 1000 )
	{
		CfgLangDialog dlg( ( NCDialogParent* )Parent(), m_CurLangId.c_str(), &m_List );

		if ( dlg.DoModal() == CMD_OK )
		{
			SetCurLang( dlg.GetId() );
		}

		return true;
	}

	return NCVertDialog::Command( id, subId, win, data );
}

bool SysOptDialog::EventChildKey( Win* child, cevent_key* pEvent )
{
	if ( pEvent->Type() == EV_KEYDOWN )
	{
		if ( pEvent->Key() == VK_RETURN && m_LangButton.InFocus() ) //prevent autoenter
		{
			return false;
		}

	};

	return NCVertDialog::EventChildKey( child, pEvent );
}


bool DoSystemConfigDialog( NCDialogParent* parent )
{
	SysOptDialog dlg( parent );

	if ( dlg.DoModal() == CMD_OK )
	{
		g_WcmConfig.systemAskOpenExec = dlg.m_AskOpenExecButton.IsSet();
		g_WcmConfig.systemEscPanel = dlg.m_EscPanelButton.IsSet();
		g_WcmConfig.systemEscCommandLine = dlg.m_EscCommandLineButton.IsSet();
		g_WcmConfig.systemBackSpaceUpDir = dlg.m_BackUpDirButton.IsSet( );
		g_WcmConfig.systemAutoComplete = dlg.m_AutoCompleteButton.IsSet( );
		g_WcmConfig.systemAutoSaveSetup = dlg.m_AutoSaveSetupButton.IsSet( );
		g_WcmConfig.systemShowHostName = dlg.m_ShowHostNameButton.IsSet( );
		g_WcmConfig.systemStorePasswords = dlg.m_StorePasswordsButton.IsSet( );
		const char* s = g_WcmConfig.systemLang.data();

		if ( !s ) { s = "+"; }

		bool langChanged = strcmp( dlg.m_CurLangId.data( ), s ) != 0;
		g_WcmConfig.systemLang = dlg.m_CurLangId.data( );

		if ( langChanged )
		{
			NCMessageBox( parent, _LT( "Note" ),
			              _LT( "Language changed. \nFor effect you must save config and restart" ), false );
		}

		return true;
	}

	return false;
}


////////////////////////// TerminalOptDialog

class TerminalOptDialog: public NCVertDialog
{
	Layout iL;
public:
	StaticLine backspaceKeyStatic;
	SButton  backspaceAsciiButton;
	SButton  backspaceCtrlHButton;

	TerminalOptDialog( NCDialogParent* parent );
	//virtual bool EventChildKey(Win* child, cevent_key* pEvent);
	//virtual bool Command(int id, int subId, Win *win, void *data);
	virtual ~TerminalOptDialog();
};


TerminalOptDialog::TerminalOptDialog( NCDialogParent* parent )
	:  NCVertDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( _LT( "Terminal options" ) ).data(), bListOkCancel ),
	   iL( 16, 3 ),
	   backspaceKeyStatic( 0, this, utf8_to_unicode( _LT( "Backspace key:" ) ).data() ),
	   backspaceAsciiButton( 0, this, utf8_to_unicode( "ASCII DEL" ).data(), 1, g_WcmConfig.terminalBackspaceKey == 0 ),
	   backspaceCtrlHButton( 0, this,  utf8_to_unicode( "Ctrl H" ).data(), 1, g_WcmConfig.terminalBackspaceKey == 1 )
{
	iL.AddWin( &backspaceKeyStatic,   0, 0, 0, 1 );
	backspaceKeyStatic.Enable();
	backspaceKeyStatic.Show();
	iL.AddWin( &backspaceAsciiButton, 1, 1 );
	backspaceAsciiButton.Enable();
	backspaceAsciiButton.Show();
	iL.AddWin( &backspaceCtrlHButton, 2, 1 );
	backspaceCtrlHButton.Enable();
	backspaceCtrlHButton.Show();

	iL.ColSet( 0, 10, 10, 10 );
	iL.SetColGrowth( 1 );

	AddLayout( &iL );
	SetEnterCmd( CMD_OK );

	backspaceAsciiButton.SetFocus();

	order.append( &backspaceAsciiButton );
	order.append( &backspaceCtrlHButton );
	SetPosition();
}

TerminalOptDialog::~TerminalOptDialog() {}


bool DoTerminalConfigDialog( NCDialogParent* parent )
{
	TerminalOptDialog dlg( parent );

	if ( dlg.DoModal() == CMD_OK )
	{
		g_WcmConfig.terminalBackspaceKey = dlg.backspaceCtrlHButton.IsSet() ? 1 : 0;
		return true;
	}

	return false;
}
