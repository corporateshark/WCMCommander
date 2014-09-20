/*
 * Part of Wal Commander GitHub Edition
 * https://github.com/corporateshark/WalCommander
 * walcommander@linderdaum.com
 */

#pragma once

#include "ncdialogs.h"

class NCWin;

struct WcmConfig
{
private:
	enum eMapType { MT_BOOL, MT_INT, MT_STR };
	/// no need to make this polymorphic type
	struct sNode
	{
		sNode()
		 : m_Type( MT_INT )
		 , m_Section( NULL )
		 , m_Name( NULL )
		 , m_Current()
		 , m_Default()
		{}
		sNode( eMapType Type, const char* Section, const char* Name )
		 : m_Type( Type )
		 , m_Section( Section )
		 , m_Name( Name )
		 , m_Current()
		 , m_Default()
		{}
		eMapType    m_Type;
		const char* m_Section;
		const char* m_Name;
		/// holds a pointer to the currently valid value
		union
		{
			int* m_Int;
			bool* m_Bool;
			std::vector<char>* m_Str;
		} m_Current;

		int GetDefaultInt() const { return m_Default.m_Int; }
		bool GetDefaultBool() const { return m_Default.m_Bool; }
		const char* GetDefaultStr() const { return m_Default.m_Str; }

		int GetCurrentInt() const { return m_Current.m_Int ? *m_Current.m_Int : 0; }
		bool GetCurrentBool() const { return m_Current.m_Bool ? *m_Current.m_Bool : false; }
		const char* GetCurrentStr() const { return m_Current.m_Str ? m_Current.m_Str->data() : NULL; }

		static sNode CreateIntNode( const char* Section, const char* Name, int* pInt, int DefaultValue )
		{
			sNode N( MT_INT, Section, Name );
			N.m_Current.m_Int = pInt;
			N.m_Default.m_Int = DefaultValue;
			return N;
		}
		static sNode CreateBoolNode( const char* Section, const char* Name, bool* pBool, bool DefaultValue )
		{	
			sNode N( MT_BOOL, Section, Name );
			N.m_Current.m_Bool = pBool;
			N.m_Default.m_Bool = DefaultValue;
			return N;
		}
		static sNode CreateStrNode( const char* Section, const char* Name, std::vector<char>* pStr, const char* DefaultValue )
		{
			sNode N( MT_STR, Section, Name );
			N.m_Current.m_Str = pStr;
			N.m_Default.m_Str = DefaultValue;
			return N;
		}

	private:
		/// default value
		union
		{
			int m_Int;
			bool m_Bool;
			const char* m_Str;
		} m_Default;
	};

public:

// System settings
	bool systemAskOpenExec;
	bool systemEscPanel;
	bool systemBackSpaceUpDir;
	bool systemAutoComplete;
	bool systemShowHostName;
	std::vector<char> systemLang; //"+" - auto "-" -internal eng.

// Panel settings
	bool panelShowHiddenFiles;
	bool panelCaseSensitive;
	bool panelSelectFolders;
	bool panelShowDotsInRoot;
	bool panelShowFolderIcons;
	bool panelShowExecutableIcons;
	int panelModeLeft;
	int panelModeRight;

// Editor settings
	bool editSavePos;
	bool editAutoIdent;
	int editTabSize;
	bool editShl;

// Terminal settings
	int terminalBackspaceKey;

// Style settings
	int styleColorMode;
	bool styleShowToolBar;
	bool styleShowButtonBar;

// Window position and size to be restored on the next startup
	int windowX;
	int windowY;
	int windowWidth;
	int windowHeight;

// Fonts
	std::vector<char> panelFontUri;
	std::vector<char> viewerFontUri;
	std::vector<char> editorFontUri;
	std::vector<char> dialogFontUri;
	std::vector<char> terminalFontUri;
	std::vector<char> helpTextFontUri;
	std::vector<char> helpBoldFontUri;
	std::vector<char> helpHeadFontUri;

	/// store properties of the currently active fonts in ...Uri fields
	void ImpCurrentFonts();

// Paths of the panels to be restored on the next startup
	std::vector<char> leftPanelPath;
	std::vector<char> rightPanelPath;

	WcmConfig();
	void Load( NCWin* nc );
	void Save( NCWin* nc );

private:
	void MapInt( const char* Section, const char* Name, int* pInt, int DefaultValue );
	void MapBool( const char* Section, const char* Name, bool* pInt, bool DefaultValue );
	void MapStr( const char* Section, const char* Name, std::vector<char>* pStr, const char* DefaultValue = NULL );

private:
	std::vector<sNode> m_MapList;
};

extern WcmConfig wcmConfig;

void InitConfigPath();

bool DoPanelConfigDialog( NCDialogParent* parent );
bool DoEditConfigDialog( NCDialogParent* parent );
bool DoStyleConfigDialog( NCDialogParent* parent );
bool DoSystemConfigDialog( NCDialogParent* parent );
bool DoTerminalConfigDialog( NCDialogParent* parent );

bool LoadStringList( const char* section, ccollect< std::vector<char> >& list );
void SaveStringList( const char* section, ccollect< std::vector<char> >& list );
