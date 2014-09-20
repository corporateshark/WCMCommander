/*
 * Part of Wal Commander GitHub Edition
 * https://github.com/corporateshark/WalCommander
 * walcommander@linderdaum.com
 */

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

#ifdef _WIN32
#  include "w32util.h"
#endif

#include <map>

WcmConfig wcmConfig;

#ifndef _WIN32

#define DEFAULT_CONFIG_PATH UNIX_CONFIG_DIR_PATH "/config.default"

class TextInStream
{
	int bufSize;
	std::vector<char> buffer;
	int pos, count;
	bool FillBuffer() { if ( pos < count ) { return true; } if ( count <= 0 ) { return false; } count = Read( buffer.data(), bufSize ); pos = 0; return count > 0; }
public:
	TextInStream( int _bSize = 1024 ): bufSize( _bSize > 0 ? _bSize : 1024 ), pos( 1 ), count( 1 ) { buffer.resize( bufSize ); }
	int GetC() { FillBuffer(); return pos < count ? buffer[pos++] : EOF; }
	bool GetLine( char* s, int size );
	~TextInStream() {}
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

inline bool IsSpace( int c ) { return c > 0 && c <= 32; }
inline bool IsLatinChar( int c ) { return (c >= 'a' && c <= 'z' )|| (c >= 'A' && c <= 'Z'); }
inline bool IsDigit( int c ) { return c >= '0' && c <= '9'; }




class IniHash
{
	cstrhash< cstrhash< std::vector<char> > > hash;
	std::vector<char>* Find( const char* section, const char* var );
	std::vector<char>* Create( const char* section, const char* var );
	void Delete( const char* section, const char* var );
public:
	IniHash();
	void SetStrValue( const char* section, const char* var, const char* value );
	void SetIntValue( const char* section, const char* var, int value );
	void SetBoolValue( const char* section, const char* var, bool value );
	const char* GetStrValue( const char* section, const char* var, const char* def );
	int GetIntValue( const char* section, const char* var, int def );
	bool GetBoolValue( const char* section, const char* var, bool def );
	void Clear();
	void Load( const sys_char_t* fileName );
	void Save( const sys_char_t* fileName );
	~IniHash();
};

IniHash::IniHash() {}

std::vector<char>* IniHash::Find( const char* section, const char* var )
{
	cstrhash< std::vector<char> >* h = hash.exist( section );

	if ( !h ) { return 0; }

	return h->exist( var );
}

std::vector<char>* IniHash::Create( const char* section, const char* var ) { return &( hash[section][var] ); }
void IniHash::Delete( const char* section, const char* var ) { hash[section].del( var, false ); }
void IniHash::SetStrValue( const char* section, const char* var, const char* value ) { if ( !value ) { Delete( section, var ); return;}; std::vector<char>* p = Create( section, var ); if ( p ) { *p = new_char_str( value ); } }
void IniHash::SetIntValue( const char* section, const char* var, int value ) { char buf[64]; int_to_char<int>( value, buf ); SetStrValue( section, var, buf ); }
void IniHash::SetBoolValue( const char* section, const char* var, bool value ) { SetIntValue( section, var, value ? 1 : 0 ); }
const char* IniHash::GetStrValue( const char* section, const char* var, const char* def ) { std::vector<char>* p =  Find( section, var ); return ( p && p->data() ) ? p->data() : def; }
int IniHash::GetIntValue( const char* section, const char* var, int def ) { std::vector<char>* p =  Find( section, var ); return ( p && p->data() ) ? atoi( p->data() ) : def; }
bool IniHash::GetBoolValue( const char* section, const char* var, bool def ) { int n = GetIntValue( section, var, def ? 1 : 0 ); return n ? true : false; }

void IniHash::Clear() { hash.clear(); }

void IniHash::Load( const sys_char_t* fileName )
{
//	Clear();
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
	std::vector<char> section;

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

			section = new_char_str( s );

		}
		else
		{
			if ( !section.data() ) { continue; }

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

			SetStrValue( section.data(), s, v );
		}
	}

	in.Close();
}

void IniHash::Save( const sys_char_t* fileName )
{
	SysTextFileOut out;
	out.Open( fileName );

	if ( hash.count() > 0 )
	{
		std::vector<const char*> secList = hash.keys();
		sort2m<const char*>( secList.data(), hash.count(), strless< const char* > );

		for ( int i = 0; i < hash.count(); i++ )
		{
			out.Put( "\n[" );
			out.Put( secList[i] );
			out.Put( "]\n" );
			cstrhash< std::vector<char> >* h = hash.exist( secList[i] );

			if ( !h ) { continue; }

			std::vector<const char*> varList = h->keys();
			sort2m<const char*>( varList.data(), h->count(), strless< const char* > );

			for ( int j = 0; j < h->count(); j++ )
			{
				out.Put( varList[j] );
				out.PutC( '=' );
				std::vector<char>* p = h->exist( varList[j] );

				if ( p && p->data() ) { out.Put( p->data() ); }

				out.PutC( '\n' );
			}
		}
	}

	out.Flush();
	out.Close();
}

IniHash::~IniHash() {}

bool LoadStringList( const char* section, ccollect< std::vector<char> >& list )
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

			if ( *s ) { list.append( new_char_str( s ) ); }
		}
	}
	catch ( cexception* ex )
	{
		ex->destroy();
		return false;
	}

	return true;
}


void SaveStringList( const char* section, ccollect< std::vector<char> >& list )
{
	try
	{
		SysTextFileOut out;

		FSPath path = configDirPath;
		path.Push( CS_UTF8, carray_cat<char>( section, ".cfg" ).data() );
		out.Open( ( sys_char_t* )path.GetString( sys_charset_id ) );

		for ( int i = 0; i < list.count(); i++ )
		{
			if ( list[i].data() && list[i][0] )
			{
				out.Put( list[i].data() );
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
//старый клочек, надо перепроверить
static const char* regapp = "Wal commander";
static const char* regcomp = "Wal";

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

static HKEY GetSection( const char* sectname )
{
	ASSERT( sectname && *sectname );
	HKEY happ = GetAppProfileKey();

	if ( !happ ) { return NULL; }

	DWORD dw;
	HKEY hsect;
	RegCreateKeyEx( happ, sectname, 0, REG_NONE,
	                REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_READ, NULL,
	                &hsect, &dw );
	RegCloseKey( happ );
	return hsect;
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

std::vector<char> RegReadString( char const* sect, const char* what, const char* def )
{
	HKEY hsect = GetSection( sect );

	if ( !hsect ) { return def ? new_char_str( def ) : std::vector<char>( 0 ); }

	std::vector<char> strValue;
	DWORD dwType, dwCount;
	LONG lResult = RegQueryValueEx( hsect, ( LPTSTR )what, NULL, &dwType,
	                                NULL, &dwCount );

	if ( lResult == ERROR_SUCCESS )
	{
		ASSERT( dwType == REG_SZ );
		strValue.resize( dwCount + 1 );
		strValue[dwCount] = 0;

		lResult = RegQueryValueEx( hsect, ( LPTSTR )what, NULL, &dwType,
		                           ( LPBYTE )strValue.data(), &dwCount );
	}

	RegCloseKey( hsect );

	if ( lResult == ERROR_SUCCESS )
	{
		ASSERT( dwType == REG_SZ );
		return strValue;
	}

	return def ? new_char_str( def ) : std::vector<char>( 0 );
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

bool LoadStringList( const char* section, ccollect< std::vector<char> >& list )
{
	char name[64];
	list.clear();

	for ( int i = 1; ; i++ )
	{
		snprintf( name, sizeof( name ), "v%i", i );
		std::vector<char> s = RegReadString( section, name, "" );

		if ( !s.data() || !s[0] ) { break; }

		list.append( s );
	}

	return true;
}

void SaveStringList( const char* section, ccollect< std::vector<char> >& list )
{
	int n = 1;
	char name[64];

	for ( int i = 0; i < list.count(); i++ )
	{
		if ( list[i].data() && list[i][0] )
		{
			snprintf( name, sizeof( name ), "v%i", n );

			if ( !RegWriteString( section, name, list[i].data() ) )
			{
				break;
			}

			n++;
		}
	}

	snprintf( name, sizeof( name ), "v%i", n );
	RegWriteString( section, name, "" );
}


#endif

const char* sectionSystem = "system";
const char* sectionPanel = "panel";
const char* sectionEditor = "editor";
const char* sectionViewer = "viewer";
const char* sectionTerminal = "terminal";
const char* sectionFonts = "fonts";

WcmConfig::WcmConfig()
	:  systemAskOpenExec( true ),
	   systemEscPanel( true ),
	   systemBackSpaceUpDir( false ),
		systemAutoComplete( true ),
		systemShowHostName( false ),
	   systemLang( new_char_str( "+" ) ),

	   panelShowHiddenFiles( true ),
	   panelCaseSensitive( false ),
	   panelSelectFolders( false ),
		panelShowDotsInRoot( false ),
		panelShowFolderIcons( true ),
		panelShowExecutableIcons( true ),
		panelModeLeft( 0 ),
	   panelModeRight( 0 ),

	   editSavePos( true ),
	   editAutoIdent( false ),
	   editTabSize( 3 ),
	   editShl( true ),

	   terminalBackspaceKey( 0 ),

		styleColorMode( 0 ),
		styleShowToolBar( true ),
		styleShowButtonBar( true ),

	   windowX(0),
		windowY(0),
		windowWidth(0),
		windowHeight(0)
{
	leftPanelPath = new_char_str( "" );
	rightPanelPath = new_char_str( "" );

#ifndef _WIN32
	MapBool( sectionSystem, "ask_open_exec", &systemAskOpenExec, systemAskOpenExec );
#endif
	MapBool( sectionSystem, "esc_panel", &systemEscPanel, systemEscPanel );
	MapBool( sectionSystem, "back_updir", &systemBackSpaceUpDir, systemBackSpaceUpDir );
	MapBool( sectionSystem, "auto_complete", &systemAutoComplete, systemAutoComplete );
	MapBool( sectionSystem, "show_hostname", &systemShowHostName, systemShowHostName );
	MapStr( sectionSystem,  "lang", &systemLang );

	MapBool( sectionSystem, "show_toolbar", &styleShowToolBar, styleShowToolBar );
	MapBool( sectionSystem, "show_buttonbar", &styleShowButtonBar, styleShowButtonBar );
	MapInt( sectionPanel, "color_mode", &styleColorMode, styleColorMode );

	MapBool( sectionPanel, "show_hidden_files",   &panelShowHiddenFiles, panelShowHiddenFiles );
	MapBool( sectionPanel, "case_sensitive_sort", &panelCaseSensitive, panelCaseSensitive );
	MapBool( sectionPanel, "select_folders",   &panelSelectFolders, panelSelectFolders );
	MapBool( sectionPanel, "show_dots",     &panelShowDotsInRoot, panelShowDotsInRoot );
	MapBool( sectionPanel, "show_foldericons",     &panelShowFolderIcons, panelShowFolderIcons );
	MapBool( sectionPanel, "show_executableicons",     &panelShowExecutableIcons, panelShowExecutableIcons );
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

void WcmConfig::ImpCurrentFonts()
{
	panelFontUri = new_char_str( panelFont.ptr() ? panelFont->uri() : "" );
	viewerFontUri = new_char_str( viewerFont.ptr() ? viewerFont->uri() : "" );
	editorFontUri = new_char_str( editorFont.ptr() ? editorFont->uri() : "" );
	dialogFontUri  = new_char_str( dialogFont.ptr() ? dialogFont->uri() : "" );
	terminalFontUri  = new_char_str( terminalFont.ptr() ? terminalFont->uri() : "" );
	helpTextFontUri  = new_char_str( helpTextFont.ptr() ? helpTextFont->uri() : "" );
	helpBoldFontUri  = new_char_str( helpBoldFont.ptr() ? helpBoldFont->uri() : "" );
	helpHeadFontUri  = new_char_str( helpHeadFont.ptr() ? helpHeadFont->uri() : "" );
}

void WcmConfig::MapInt( const char* Section, const char* Name, int* pInt, int def )
{
	sNode Node = sNode::CreateIntNode( Section, Name, pInt, def );
	m_MapList.push_back( Node );
}

void WcmConfig::MapBool( const char* Section, const char* Name, bool* pBool, bool def )
{
	sNode Node = sNode::CreateBoolNode( Section, Name, pBool, def );
	m_MapList.push_back( Node );
}

void WcmConfig::MapStr( const char* Section, const char* Name, std::vector<char>* pStr, const char* def )
{
	sNode Node = sNode::CreateStrNode( Section, Name, pStr, def );
	m_MapList.push_back( Node );
}

static const char* CommandsHistorySection = "CommandsHistory";
static const char* FilesAssociationsSection = "FilesAssociations";

class clConfigHelper
{
public:
	clConfigHelper() :m_SectionName( "" ) {}

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
	explicit clConfigWriter( IniHash& hash ) :m_Hash( hash ) {}
#endif

	void Write( const char* KeyNamePattern, int i, const char* Data )
	{
		char Buf[4096];
		snprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		RegWriteString( m_SectionName, Buf, Data );
#else
		m_Hash.SetStrValue( m_SectionName, Buf, Data );
#endif
	}
	void WriteBool( const char* KeyNamePattern, int i, bool Data )
	{
		char Buf[4096];
		snprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		RegWriteInt( m_SectionName, Buf, (int)Data );
#else
		m_Hash.SetBoolValue( m_SectionName, Buf, Data );
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
	explicit clConfigReader( IniHash& hash ):m_Hash( hash ) {}
#endif

	std::vector<unicode_t> Read( const char* KeyNamePattern, int i )
	{
		char Buf[4096];
		snprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		std::vector<unicode_t> Result = utf8_to_unicode( RegReadString( m_SectionName, Buf, "" ).data( ) );
#else
		std::vector<unicode_t> Result = utf8_to_unicode( m_Hash.GetStrValue( m_SectionName, Buf, "" ) );
#endif
		return Result;
	}
	bool ReadBool( const char* KeyNamePattern, int i, bool DefaultValue )
	{
		char Buf[4096];
		snprintf( Buf, sizeof( Buf ), KeyNamePattern, i );
#ifdef _WIN32
		bool Result = RegReadInt( m_SectionName, Buf, DefaultValue ) != 0;
#else
		bool Result = m_Hash.GetBoolValue( m_SectionName, Buf, DefaultValue );
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
	if ( !nc ) return;

#if defined(_WIN32)
	clConfigWriter Cfg;
#else
	clConfigWriter Cfg( hash );
#endif
	Cfg.SetSectionName( FilesAssociationsSection );

	const std::vector<clNCFileAssociation>& Assoc = nc->GetFileAssociations();

	for ( size_t i = 0; i < Assoc.size(); i++ )
	{
		const clNCFileAssociation& A = Assoc[i];

		std::vector<char> Mask_utf8 = unicode_to_utf8( A.GetMask().data() );
		std::vector<char> Description_utf8 = unicode_to_utf8( A.GetDescription().data() );
		std::vector<char> Execute_utf8 = unicode_to_utf8( A.GetExecuteCommand().data() );
		std::vector<char> ExecuteSecondary_utf8 = unicode_to_utf8( A.GetExecuteCommandSecondary().data() );
		std::vector<char> View_utf8 = unicode_to_utf8( A.GetViewCommand().data() );
		std::vector<char> ViewSecondary_utf8 = unicode_to_utf8( A.GetViewCommandSecondary().data() );
		std::vector<char> Edit_utf8 = unicode_to_utf8( A.GetEditCommand().data() );
		std::vector<char> EditSecondary_utf8 = unicode_to_utf8( A.GetEditCommandSecondary().data() );

		Cfg.Write( "Mask%i", i, Mask_utf8.data( ) );
		Cfg.Write( "Description%i", i, Description_utf8.data( ) );
		Cfg.Write( "Execute%i", i, Execute_utf8.data( ) );
		Cfg.Write( "ExecuteSecondary%i", i, ExecuteSecondary_utf8.data( ) );
		Cfg.Write( "View%i", i, View_utf8.data( ) );
		Cfg.Write( "ViewSecondary%i", i, ViewSecondary_utf8.data( ) );
		Cfg.Write( "Edit%i", i, Edit_utf8.data( ) );
		Cfg.Write( "EditSecondary%i", i, EditSecondary_utf8.data( ) );
		Cfg.WriteBool( "HasTerminal%i", i, A.GetHasTerminal() );
	}

	// end marker
	Cfg.Write( "Mask%i", Assoc.size(), "" );
}

void LoadFilesAssociations( NCWin* nc
#ifndef _WIN32
, IniHash& hash
#endif
)
{
	if ( !nc ) return;

#if defined(_WIN32)
	clConfigReader Cfg;
#else
	clConfigReader Cfg( hash );
#endif
	Cfg.SetSectionName( FilesAssociationsSection );

	int i = 0;

	std::vector<clNCFileAssociation> Assoc;

	while (true)
	{
		std::vector<unicode_t> Mask = Cfg.Read( "Mask%i", i );
		std::vector<unicode_t> Description = Cfg.Read( "Description%i", i );
		std::vector<unicode_t> Execute = Cfg.Read( "Execute%i", i );
		std::vector<unicode_t> ExecuteSecondary = Cfg.Read( "ExecuteSecondary%i", i );
		std::vector<unicode_t> View = Cfg.Read( "View%i", i );
		std::vector<unicode_t> ViewSecondary = Cfg.Read( "ViewSecondary%i", i );
		std::vector<unicode_t> Edit = Cfg.Read( "Edit%i", i );
		std::vector<unicode_t> EditSecondary = Cfg.Read( "EditSecondary%i", i );
		bool HasTerminal = Cfg.ReadBool( "HasTerminal%i", i, true );

		if ( !Mask.data() || !*Mask.data() ) break;

		clNCFileAssociation A;
		A.SetMask( Mask );
		A.SetDescription( Description );
		A.SetExecuteCommand( Execute );
		A.SetExecuteCommandSecondary( ExecuteSecondary );
		A.SetViewCommand( View );
		A.SetViewCommandSecondary( ViewSecondary );
		A.SetEditCommand( Edit );
		A.SetEditCommandSecondary( EditSecondary );
		A.SetHasTerminal( HasTerminal );
		Assoc.push_back( A );

		i++;
	}

	nc->SetFileAssociations( Assoc );
}

void SaveCommandsHistory( NCWin* nc
#ifndef _WIN32
, IniHash& hash
#endif
)
{
	if ( !nc ) return;

#if defined(_WIN32)
	clConfigWriter Cfg;
#else
	clConfigWriter Cfg( hash );
#endif
	Cfg.SetSectionName( CommandsHistorySection );

	int Count = nc->GetHistory()->Count();

	for ( int i = 0; i < Count; i++ )
	{
		const unicode_t* Hist = (*nc->GetHistory())[Count-i-1];

		Cfg.Write( "Command%i", i, unicode_to_utf8( Hist ).data( ) );
	}
}

void LoadCommandsHistory( NCWin* nc
#ifndef _WIN32
, IniHash& hash
#endif
)
{
	if ( !nc ) return;

#if defined(_WIN32)
	clConfigReader Cfg;
#else
	clConfigReader Cfg( hash );
#endif
	Cfg.SetSectionName( CommandsHistorySection );

	nc->GetHistory()->Clear();

	int i = 0;

	while (true)
	{
		std::vector<unicode_t> Cmd = Cfg.Read( "Command%i", i );

		if ( !Cmd.data() || !*Cmd.data() ) break;

		nc->GetHistory()->Put( Cmd.data() );

		i++;
	}
}

extern std::map<std::vector<unicode_t>, sEditorScrollCtx> g_EditPosHash;
extern std::map<std::vector<unicode_t>, int> g_ViewPosHash;

void LoadEditorPositions()
{
	ccollect< std::vector<char> > Positions;

	LoadStringList( "EditorPositions", Positions );

	g_EditPosHash.clear();

	for ( int i = 0; i != Positions.count(); i++ )
	{
		std::vector<char> Line = Positions[i];

		int FL, L, P;
		char Buf[0xFFFF];

		int NumRead = sscanf( Line.data(), "FL = %i L = %i P = %i FN = %65535s", &FL, &L, &P, Buf );

		if ( NumRead != 4 ) break;

//		printf( "FL = %i L = %i P = %i FN = %s\n", FL, L, P, Buf );

		std::vector<unicode_t> FileName = utf8_to_unicode( Buf );
		sEditorScrollCtx Ctx;
		Ctx.m_FirstLine = FL;
		Ctx.m_Point.line = L;
		Ctx.m_Point.pos = P;

		g_EditPosHash[ FileName ] = Ctx;
	}
}

void LoadViewerPositions()
{
	ccollect< std::vector<char> > Positions;

	LoadStringList( "ViewerPositions", Positions );

	g_ViewPosHash.clear();

	for ( int i = 0; i != Positions.count(); i++ )
	{
		std::vector<char> Line = Positions[i];

		int L;
		char Buf[0xFFFF];

		int NumRead = sscanf( Line.data(), "L = %i FN = %65535s", &L, Buf );

		if ( NumRead != 2 ) break;

		std::vector<unicode_t> FileName = utf8_to_unicode( Buf );

		g_ViewPosHash[ FileName ] = L;
	}
}

void WcmConfig::Load( NCWin* nc )
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
	LoadFilesAssociations( nc );

#else
	IniHash hash;
	FSPath path = configDirPath;
	path.Push( CS_UTF8, "config" );

	hash.Load( DEFAULT_CONFIG_PATH );
	hash.Load( ( sys_char_t* )path.GetString( sys_charset_id ) );

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
				*Node.m_Current.m_Str = new_char_str( s );
			}
			else
			{
				( *Node.m_Current.m_Str ).clear();
			}
		}

	}

	LoadCommandsHistory( nc, hash );
	LoadFilesAssociations( nc, hash );

#endif

	if ( editTabSize <= 0 || editTabSize > 64 ) { editTabSize = 3; }

	LoadEditorPositions();
	LoadViewerPositions();
}

void SaveEditorPositions()
{
	ccollect< std::vector<char> > Positions;

	for ( auto i = g_EditPosHash.begin(); i != g_EditPosHash.end(); i++ )
	{
		std::vector<unicode_t> FileName = i->first;
		sEditorScrollCtx       Ctx      = i->second;

		std::vector<char> FileName_utf8 = unicode_to_utf8( FileName.data() );

		char Buf[0xFFFF];
		snprintf( Buf, sizeof( Buf ) - 1, "FL = %i L = %i P = %i FN = %s", Ctx.m_FirstLine, Ctx.m_Point.line, Ctx.m_Point.pos, FileName_utf8.data() );

		Positions.append( new_char_str( Buf ) );
	}

	SaveStringList( "EditorPositions", Positions );
}

void SaveViewerPositions()
{
	ccollect< std::vector<char> > Positions;

	for ( auto i = g_ViewPosHash.begin(); i != g_ViewPosHash.end(); i++ )
	{
		std::vector<unicode_t> FileName = i->first;
		int                    Line     = i->second;

		std::vector<char> FileName_utf8 = unicode_to_utf8( FileName.data() );

		char Buf[0xFFFF];
		snprintf( Buf, sizeof( Buf ) - 1, "L = %i FN = %s", Line, FileName_utf8.data() );

		Positions.append( new_char_str( Buf ) );
	}

	SaveStringList( "ViewerPositions", Positions );
}

void WcmConfig::Save( NCWin* nc )
{
	if ( nc )
	{
		leftPanelPath = new_char_str( nc->GetLeftPanel( )->UriOfDir().GetUtf8() );
		rightPanelPath = new_char_str( nc->GetRightPanel( )->UriOfDir( ).GetUtf8() );
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

#else
	IniHash hash;
	FSPath path = configDirPath;
	path.Push( CS_UTF8, "config" );
	hash.Load( ( sys_char_t* )path.GetString( sys_charset_id ) );

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

	hash.Save( ( sys_char_t* )path.GetString( sys_charset_id ) );
#endif

	SaveEditorPositions();
	SaveViewerPositions();
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
			if ( fs.IsENOENT( err ) ) //директорий не существует
			{
				if ( fs.MkDir( path, 0700, &err,  0 ) )
				{
					fprintf( stderr, "can`t create config directory %s (%s)", path.GetUtf8(), fs.StrError( err ).GetUtf8() );
					return;
				}

			}
			else
			{
				fprintf( stderr, "can`t create config directory statuc %s (%s)", path.GetUtf8(), fs.StrError( err ).GetUtf8() );
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

	PanelOptDialog( NCDialogParent* parent );
	virtual ~PanelOptDialog();
};

PanelOptDialog::~PanelOptDialog() {}

PanelOptDialog::PanelOptDialog( NCDialogParent* parent )
 :  NCVertDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( _LT( "Panel settings" ) ).data(), bListOkCancel )
 , iL( 16, 3 )
 , showHiddenButton( 0, this, utf8_to_unicode( _LT( "Show &hidden files" ) ).data(), 0, wcmConfig.panelShowHiddenFiles )
 , caseSensitive( 0, this, utf8_to_unicode( _LT( "&Case sensitive sort" ) ).data(), 0, wcmConfig.panelCaseSensitive )
 , selectFolders( 0, this, utf8_to_unicode( _LT( "Select &folders" ) ).data(), 0, wcmConfig.panelSelectFolders )
 , showDotsInRoot( 0, this, utf8_to_unicode( _LT( "Show .. in the &root folder" ) ).data(), 0, wcmConfig.panelShowDotsInRoot )
 , showFolderIcons( 0, this, utf8_to_unicode( _LT( "Show folder &icons" ) ).data(), 0, wcmConfig.panelShowFolderIcons )
 , showExecutableIcons( 0, this, utf8_to_unicode( _LT( "Show &executable icons" ) ).data(), 0, wcmConfig.panelShowExecutableIcons )
{
	iL.AddWinAndEnable( &showHiddenButton,  0, 0 );
	showHiddenButton.SetFocus();
	iL.AddWinAndEnable( &caseSensitive,  1, 0 );
	iL.AddWinAndEnable( &selectFolders,  2, 0 );
	iL.AddWinAndEnable( &showDotsInRoot, 3, 0 );
	iL.AddWinAndEnable( &showFolderIcons, 4, 0 );
	iL.AddWinAndEnable( &showExecutableIcons, 5, 0 );

	AddLayout( &iL );
	SetEnterCmd( CMD_OK );

	showHiddenButton.SetFocus();

	order.append( &showHiddenButton );
	order.append( &caseSensitive );
	order.append( &selectFolders );
	order.append( &showDotsInRoot );
	order.append( &showFolderIcons );
	order.append( &showExecutableIcons );
	SetPosition();
}

bool DoPanelConfigDialog( NCDialogParent* parent )
{
	PanelOptDialog dlg( parent );

	if ( dlg.DoModal() == CMD_OK )
	{
		wcmConfig.panelShowHiddenFiles = dlg.showHiddenButton.IsSet();
		wcmConfig.panelCaseSensitive = dlg.caseSensitive.IsSet();
		wcmConfig.panelSelectFolders = dlg.selectFolders.IsSet();
		wcmConfig.panelShowDotsInRoot = dlg.showDotsInRoot.IsSet();
		wcmConfig.panelShowFolderIcons  = dlg.showFolderIcons.IsSet();
		wcmConfig.panelShowExecutableIcons  = dlg.showExecutableIcons.IsSet();
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

	   saveFilePosButton( 0, this, utf8_to_unicode( _LT( "Save file &position" ) ).data(), 0, wcmConfig.editSavePos ),
	   autoIdentButton( 0, this, utf8_to_unicode( _LT( "Auto &indent" ) ).data(), 0, wcmConfig.editAutoIdent ),
	   shlButton( 0, this, utf8_to_unicode( _LT( "Syntax &highlighting" ) ).data(), 0, wcmConfig.editShl ),
	   tabText(0, this, utf8_to_unicode(_LT("&Tab size:")).data(), &tabEdit),
	   tabEdit( 0, this, 0, 0, 16 )
{
	char buf[0x100];
	snprintf( buf, sizeof( buf ) - 1, "%i", wcmConfig.editTabSize );
	tabEdit.SetText( utf8_to_unicode( buf ).data(), true );

	iL.AddWin( &saveFilePosButton, 0, 0, 0, 1 );
	saveFilePosButton.Enable();
	saveFilePosButton.Show();
	iL.AddWin( &autoIdentButton,   1, 0, 1, 1 );
	autoIdentButton.Enable();
	autoIdentButton.Show();
	iL.AddWin( &shlButton,      2, 0, 2, 1 );
	shlButton.Enable();
	shlButton.Show();

	iL.AddWin( &tabText,     3, 0, 3, 0 );
	tabText.Enable();
	tabText.Show();
	iL.AddWin( &tabEdit,     4, 1, 4, 1 );
	tabEdit.Enable();
	tabEdit.Show();
	AddLayout( &iL );
	SetEnterCmd( CMD_OK );

	saveFilePosButton.SetFocus();

	order.append( &saveFilePosButton );
	order.append( &autoIdentButton );
	order.append( &shlButton );
	order.append( &tabEdit );
	SetPosition();
}

bool DoEditConfigDialog( NCDialogParent* parent )
{
	EditOptDialog dlg( parent );

	if ( dlg.DoModal() == CMD_OK )
	{
		wcmConfig.editSavePos = dlg.saveFilePosButton.IsSet();
		wcmConfig.editAutoIdent = dlg.autoIdentButton.IsSet();
		wcmConfig.editShl = dlg.shlButton.IsSet();

		int tabSize = atoi( unicode_to_utf8( dlg.tabEdit.GetText().data() ).data() );

		if ( tabSize > 0 && tabSize <= 64 )
		{
			wcmConfig.editTabSize = tabSize;
		}

		if ( !wcmConfig.editSavePos )
		{
			g_EditPosHash.clear();
			g_ViewPosHash.clear();
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
		std::vector<char> name;
		cfont* oldFont;
		std::vector<char>* pUri;
		clPtr<cfont> newFont;
		bool fixed;
		Node(): oldFont( 0 ) {}
		Node( const char* n, bool fix,  cfont* old, std::vector<char>* uri ): name( new_char_str( n ) ), oldFont( old ), pUri( uri ), fixed( fix ) {}
	};

	ccollect<Node>* pList;

	StaticLine colorStatic;
	SButton  styleDefButton;
	SButton  styleBlackButton;
	SButton  styleWhiteButton;

	StaticLine showStatic;
	SButton  showToolbarButton;
	SButton  showButtonbarButton;

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
	   colorStatic( 0, this, utf8_to_unicode( _LT( "Colors:" ) ).data() ),
	   styleDefButton( 0, this, utf8_to_unicode( _LT( "&Default colors" ) ).data(), 1, wcmConfig.styleColorMode != 1 && wcmConfig.styleColorMode != 2 ),
	   styleBlackButton( 0, this,  utf8_to_unicode( _LT( "&Black" ) ).data(), 1, wcmConfig.styleColorMode == 1 ),
	   styleWhiteButton( 0, this, utf8_to_unicode( _LT( "&White" ) ).data(), 1, wcmConfig.styleColorMode == 2 ),

	   showStatic( 0, this, utf8_to_unicode( _LT( "Items:" ) ).data() ),
	   showToolbarButton( 0, this, utf8_to_unicode( _LT( "Show &toolbar" ) ).data(), 0, wcmConfig.styleShowToolBar ),
	   showButtonbarButton( 0, this, utf8_to_unicode( _LT( "Show &buttonbar" ) ).data(), 0, wcmConfig.styleShowButtonBar ),

	   fontsStatic( 0, this, utf8_to_unicode( _LT( "Fonts:" ) ).data() ),
	   fontList( Win::WT_CHILD, WH_TABFOCUS | WH_CLICKFOCUS, 0, this, VListWin::SINGLE_SELECT, VListWin::BORDER_3D, 0 ),
	   fontNameStatic( 0, this, utf8_to_unicode( "--------------------------------------------------" ).data() ),
	   changeButton( 0, this, utf8_to_unicode( _LT( "Set &font..." ) ).data(), CMD_CHFONT ),
	   changeX11Button( 0, this, utf8_to_unicode( _LT( "Set &X11 font..." ) ).data(), CMD_CHFONTX11 )
{
	iL.AddWin( &colorStatic, 0, 0 );
	colorStatic.Enable();
	colorStatic.Show();
	iL.AddWin( &styleDefButton, 1, 1 );
	styleDefButton.Enable();
	styleDefButton.Show();
	iL.AddWin( &styleBlackButton,  2, 1 );
	styleBlackButton.Enable();
	styleBlackButton.Show();
	iL.AddWin( &styleWhiteButton,  3, 1 );
	styleWhiteButton.Enable();
	styleWhiteButton.Show();

	iL.AddWin( &showStatic,  4, 0 );
	showStatic.Enable();
	showStatic.Show();
	iL.AddWin( &showToolbarButton, 5, 1 );
	showToolbarButton.Enable();
	showToolbarButton.Show();
	iL.AddWin( &showButtonbarButton,  6, 1 );
	showButtonbarButton.Enable();
	showButtonbarButton.Show();

	iL.LineSet( 7, 10 );
	iL.AddWin( &fontsStatic, 8, 0 );
	fontsStatic.Enable();
	fontsStatic.Show();

	iL.ColSet( 0, 10, 10, 10 );
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

	iL.AddWin( &fontList, 8, 1 );
	fontList.Enable();
	fontList.Show();

	fontNameStatic.Enable();
	fontNameStatic.Show();

	lsize = fontNameStatic.GetLSize();
	lsize.x.minimal = 500;
	lsize.x.maximal = 1000;
	fontNameStatic.SetLSize( lsize );

	iL.AddWin( &fontNameStatic, 9, 1 );
#ifdef USEFREETYPE
	iL.AddWin( &changeButton, 10, 1 );
	changeButton.Enable();
	changeButton.Show();
#endif

#ifdef _WIN32
	iL.AddWin( &changeButton, 10, 1 );
	changeButton.Enable();
	changeButton.Show();
#else
	iL.AddWin( &changeX11Button, 11, 1 );
	changeX11Button.Enable();
	changeX11Button.Show();
	iL.LineSet( 12, 10 );
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

	order.append( &styleDefButton );
	order.append( &styleBlackButton );
	order.append( &styleWhiteButton );
	order.append( &showToolbarButton );
	order.append( &showButtonbarButton );
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
		std::vector<char>* pUri = pList->get( fontList.GetCurrentInt() ).pUri;
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

		std::vector<char>* pUri = pList->get( fontList.GetCurrentInt() ).pUri;

		clPtr<cfont> p = SelectFTFont( ( NCDialogParent* )Parent(), pList->get( fontList.GetCurrentInt() ).fixed, ( pUri && pUri->data() ) ? pUri->data() : 0 );

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
		if (
		   (pEvent->Key() == VK_RETURN && ( child == &changeButton || child == &changeX11Button ) ) ||
		   ( ( pEvent->Key() == VK_UP || pEvent->Key() == VK_DOWN ) && child == &fontList)
		)
		{
			return false;
		}
	};

	return NCVertDialog::EventChildKey( child, pEvent );
}

bool DoStyleConfigDialog( NCDialogParent* parent )
{
	wcmConfig.ImpCurrentFonts();
	ccollect<StyleOptDialog::Node> list;
	list.append( StyleOptDialog::Node(  _LT( "Panel" ) ,  false,   panelFont.ptr(), &wcmConfig.panelFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Dialog" ),  false,   dialogFont.ptr(), &wcmConfig.dialogFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Viewer" ),  true,    viewerFont.ptr(), &wcmConfig.viewerFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Editor" ),  true,    editorFont.ptr(), &wcmConfig.editorFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Terminal" ),   true, terminalFont.ptr(), &wcmConfig.terminalFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Help text" ),  false,   helpTextFont.ptr(), &wcmConfig.helpTextFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Help bold text" ),   false,   helpBoldFont.ptr(), &wcmConfig.helpBoldFontUri ) );
	list.append( StyleOptDialog::Node(  _LT( "Help header text" ), false,   helpHeadFont.ptr(), &wcmConfig.helpHeadFontUri ) );

	StyleOptDialog dlg( parent, &list );

	if ( dlg.DoModal() == CMD_OK )
	{
		if ( dlg.styleBlackButton.IsSet() )
		{
			wcmConfig.styleColorMode = 1;
		}
		else if ( dlg.styleWhiteButton.IsSet() )
		{
			wcmConfig.styleColorMode = 2;
		}
		else
		{
			wcmConfig.styleColorMode = 0;
		}

		SetColorStyle( wcmConfig.styleColorMode );

		wcmConfig.styleShowToolBar = dlg.showToolbarButton.IsSet( );
		wcmConfig.styleShowButtonBar = dlg.showButtonbarButton.IsSet( );

		for ( int i = 0; i < list.count(); i++ )
			if ( list[i].newFont.ptr() && list[i].newFont->uri()[0] && list[i].pUri )
			{
				*( list[i].pUri ) = new_char_str( list[i].newFont->uri() );
			}

		return true;
	}

	return false;
}

struct LangListNode
{
	std::vector<char> id;
	std::vector<char> name;
	LangListNode() {}
	LangListNode( const char* i, const char* n ): id( new_char_str( i ) ), name( new_char_str( n ) ) {}
};

class CfgLangDialog: public NCDialog
{
	TextList _list;
	ccollect<LangListNode>* nodeList;
public:
	CfgLangDialog( NCDialogParent* parent, char* id, ccollect<LangListNode>* nl )
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
	Layout iL;
public:
	std::vector<char> curLangId;
	ccollect<LangListNode> list;
	void SetCurLang( const char* id );

	SButton  askOpenExecButton;
	SButton  escPanelButton;
	SButton  backUpDirButton;
	SButton  autoCompleteButton;
	SButton  showHostNameButton;

	StaticLabel langStatic;
	StaticLine langVal;
	Button langButton;

	SysOptDialog( NCDialogParent* parent );
	virtual bool Command( int id, int subId, Win* win, void* data );
	virtual bool EventChildKey( Win* child, cevent_key* pEvent );
	virtual ~SysOptDialog();
};

void SysOptDialog::SetCurLang( const char* id )
{
	curLangId = new_char_str( id );

	if ( id[0] == '-' )
	{
		langVal.SetText( utf8_to_unicode( _LT( "English" ) ).data() );
	}
	else if ( id[0] == '+' )
	{
		langVal.SetText( utf8_to_unicode( _LT( "Autodetect" ) ).data() );
	}
	else
	{
		for ( int i = 0; i < list.count(); i++ )
		{
			if ( !strcmp( list[i].id.data(), id ) )
			{
				langVal.SetText( utf8_to_unicode( list[i].name.data() ).data() );
				return;
			}
		}

		langVal.SetText( utf8_to_unicode( id ).data() );
	}
}

SysOptDialog::~SysOptDialog() {}

SysOptDialog::SysOptDialog( NCDialogParent* parent )
	:  NCVertDialog( ::createDialogAsChild, 0, parent, utf8_to_unicode( _LT( "System settings" ) ).data(), bListOkCancel ),
	   iL( 16, 3 )

	   , askOpenExecButton( 0, this, utf8_to_unicode( _LT( "Ask user if Exec/Open conflict" ) ).data(), 0, wcmConfig.systemAskOpenExec )
	   , escPanelButton( 0, this, utf8_to_unicode( _LT( "Enable &ESC key to show/hide panels" ) ).data(), 0, wcmConfig.systemEscPanel )
	   , backUpDirButton( 0, this, utf8_to_unicode( _LT( "Enable &BACKSPACE key to go up dir" ) ).data(), 0, wcmConfig.systemBackSpaceUpDir )
	   , autoCompleteButton( 0, this, utf8_to_unicode( _LT( "Enable &autocomplete" ) ).data(), 0, wcmConfig.systemAutoComplete )
	   , showHostNameButton( 0, this, utf8_to_unicode( _LT( "Show &host name" ) ).data(), 0, wcmConfig.systemShowHostName )
	   , langStatic(0, this, utf8_to_unicode(_LT("&Language:")).data(), &langButton)
	   , langVal( 0, this, utf8_to_unicode( "______________________" ).data() )
	   , langButton( 0, this, utf8_to_unicode( ">" ).data(), 1000 )
{

#ifndef _WIN32
	iL.AddWinAndEnable( &askOpenExecButton, 0, 0, 0, 2 );
#endif
	iL.AddWinAndEnable( &escPanelButton, 1, 0, 1, 2 );
	iL.AddWinAndEnable( &backUpDirButton, 2, 0, 2, 2 );
	iL.AddWinAndEnable( &autoCompleteButton, 3, 0, 3, 2 );
	iL.AddWinAndEnable( &showHostNameButton, 4, 0, 4, 2 );

	iL.AddWinAndEnable( &langStatic,     5, 0 );
	iL.AddWinAndEnable( &langVal,     5, 2 );
	iL.AddWinAndEnable( &langButton,     5, 1 );

	iL.SetColGrowth( 2 );

	AddLayout( &iL );
	SetEnterCmd( CMD_OK );

#ifndef _WIN32
	askOpenExecButton.SetFocus();
	order.append( &askOpenExecButton );
#endif
	order.append( &escPanelButton );
	order.append( &backUpDirButton );
	order.append( &autoCompleteButton );
	order.append( &showHostNameButton );
	order.append( &langButton );

	SetPosition();

#ifdef _WIN32
	LangListLoad( carray_cat<sys_char_t>( GetAppPath().data(), utf8_to_sys( "\\lang\\list" ).data() ).data(), list );
#else

	if ( !LangListLoad( utf8_to_sys( "install-files/share/wcm/lang/list" ).data(), list ) )
	{
		LangListLoad( utf8_to_sys( UNIX_CONFIG_DIR_PATH "/lang/list" ).data(), list );
	}

#endif

	SetCurLang( wcmConfig.systemLang.data() ? wcmConfig.systemLang.data() : "+" );
}

bool SysOptDialog::Command( int id, int subId, Win* win, void* data )
{
	if ( id == 1000 )
	{
		CfgLangDialog dlg( ( NCDialogParent* )Parent(), curLangId.data(), &list );

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
		if ( pEvent->Key() == VK_RETURN && langButton.InFocus() ) //prevent autoenter
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
		wcmConfig.systemAskOpenExec = dlg.askOpenExecButton.IsSet();
		wcmConfig.systemEscPanel = dlg.escPanelButton.IsSet();
		wcmConfig.systemBackSpaceUpDir = dlg.backUpDirButton.IsSet();
		wcmConfig.systemAutoComplete = dlg.autoCompleteButton.IsSet();
		wcmConfig.systemShowHostName = dlg.showHostNameButton.IsSet();
		const char* s = wcmConfig.systemLang.data();

		if ( !s ) { s = "+"; }

		bool langChanged = strcmp( dlg.curLangId.data(), s ) != 0;
		wcmConfig.systemLang = new_char_str( dlg.curLangId.data() );

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
	   backspaceAsciiButton( 0, this, utf8_to_unicode( "ASCII DEL" ).data(), 1, wcmConfig.terminalBackspaceKey == 0 ),
	   backspaceCtrlHButton( 0, this,  utf8_to_unicode( "Ctrl H" ).data(), 1, wcmConfig.terminalBackspaceKey == 1 )
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
		wcmConfig.terminalBackspaceKey = dlg.backspaceCtrlHButton.IsSet() ? 1 : 0;
		return true;
	}

	return false;
}
