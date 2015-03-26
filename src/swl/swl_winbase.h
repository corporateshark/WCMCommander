/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#pragma once

namespace wal
{
	extern int uiClassButton;
	extern int uiClassEditLine;
	extern int uiClassMenuBar;
	extern int uiClassPopupMenu;
	extern int uiClassSButton;
	extern int uiClassScrollBar;
	extern int uiClassTextList;
	extern int uiClassVListWin;
	extern int uiClassStatic;
	extern int uiClassToolTip;

	enum BASE_COMMANDS
	{
		_CMD_BASE_BEGIN_POINT_ = -99,
		CMD_ITEM_CLICK,      //may happen in objects with sub-elements when Enter is pressed or doubleclick (VList for example)
		CMD_ITEM_CHANGED,
		CMD_SBUTTON_INFO, //with subcommands
		CMD_SCROLL_INFO,     //with subcommands
		CMD_MENU_INFO,    //with subcommands
		CMD_EDITLINE_INFO //with subcommands
	};

	void BaseInit();
	void Draw3DButtonW2( GC& gc, crect r, unsigned bg, bool up );

	extern unicode_t ABCString[];
	extern int ABCStringLen;

	// Menu text that is split to 3 parts by hot key.
	// The hot key character is prepended by '&', like in Windows menu
	class MenuTextInfo
	{
		unicode_t* strBeforeHk;
		unicode_t* strHk;
		unicode_t* strAfterHk;
		unicode_t* strFull; // required to calculate text extents quickly
		unicode_t* strRaw; // return const value for editing. Update the value with SetText()
		unicode_t hotkeyUpperCase;
		void ParseHkText( const unicode_t* inStr );
		void Clear();
		void Init( const MenuTextInfo& src );
	public:
		explicit MenuTextInfo( const unicode_t* inStr );
		MenuTextInfo() : strBeforeHk( 0 ), strHk( 0 ), strAfterHk( 0 ), strFull( 0 ), strRaw ( 0 ), hotkeyUpperCase( 0 ) {}
		MenuTextInfo( const MenuTextInfo& src ) { Init( src ); }
		MenuTextInfo& operator=( const MenuTextInfo& src ) { Clear(); Init( src ); return *this; };
		~MenuTextInfo() { Clear(); }

		void SetText( const unicode_t* inStr );
		const unicode_t* GetRawText() const { return strRaw; }
		void DrawItem( GC& gc, int x, int y, int color_text, int color_hotkey ) const;
		void DrawItem( GC& gc, int x, int y, int color_text, int color_hotkey, bool antialias ) const;
		cpoint GetTextExtents( GC& gc, cfont* font = 0 ) const
		{
			if ( strFull == 0 ) { return cpoint( 0, 0 ); }

			if ( font ) { gc.Set( font ); }

			return gc.GetTextExtents( strFull );
		}
		int isEmpty() const { return strFull == 0; }
		bool isHotkeyMatching( unicode_t charUpperCase ) const { return hotkeyUpperCase != 0 && charUpperCase == hotkeyUpperCase; }
	};

	class Button: public Win
	{
	private:
		bool         m_Pressed;
		MenuTextInfo m_Text;
		clPtr<cicon> m_Icon;
		int          m_CommandId;
		bool         m_ShowIcon;

		bool HasIcon() const { return m_Icon.ptr() && m_ShowIcon; }
		void SendCommand() { Command( m_CommandId, 0, this, 0 ); }
	public:
		Button( int nId, Win* parent, const unicode_t* txt,  int id, crect* rect = 0, int iconX = 16, int iconY = 16 );
		void Set( const unicode_t* txt, int id, int iconX = 16, int iconY = 16 );
		int CommandId() const { return m_CommandId; }
		virtual void Paint( GC& gc, const crect& paintRect );
		virtual bool EventFocus( bool recv );
		virtual bool EventMouse( cevent_mouse* pEvent );
		virtual bool EventKey( cevent_key* pEvent );
		virtual Win* IsHisHotKey( cevent_key* pEvent );
		virtual void OnChangeStyles();
		virtual void SetShowIcon( bool Show ) { m_ShowIcon = Show; }
		virtual int UiGetClassId();
		virtual ~Button();
	};

	enum SButtonInfo
	{
		// subcommands of CMD_SBUTTON_INFO
		SCMD_SBUTTON_CHECKED = 1,
		SCMD_SBUTTON_CLEARED = 0
	};

//Checkbox and Radiobutton
	class SButton: public Win
	{
		bool isSet;
		//std::vector<unicode_t> text;
		MenuTextInfo text;
		int group;
	public:
		SButton( int nId, Win* parent, unicode_t* txt, int _group, bool _isSet = false, crect* rect = 0 );

		bool IsSet() const { return isSet; }
		void Change( bool _isSet );
		virtual void Paint( GC& gc, const crect& paintRect );
		virtual bool EventMouse( cevent_mouse* pEvent );
		virtual bool EventKey( cevent_key* pEvent );
		virtual Win* IsHisHotKey( cevent_key* pEvent );
		virtual bool EventFocus( bool recv );
		virtual bool Broadcast( int id, int subId, Win* win, void* data );
		virtual int UiGetClassId();
		virtual ~SButton();
	};


	class EditBuf
	{
		std::vector<unicode_t> data;
		int size;   //size of the buffer
		int count;  //count of symbols in string
		int cursor; //cursor position
		int marker; //point, from which to cursor is selected block

		enum { STEP = 0x100 };
		void SetSize( int n );
		void InsertBlock( int pos, int n );
		void DeleteBlock( int pos, int n );

	public:

		void Begin  ( bool mark = false )  { cursor = 0; if ( !mark ) { marker = cursor; } }
		void End ( bool mark = false )  { cursor = count; if ( !mark ) { marker = cursor; } }
		void Left   ( bool mark = false )  { if ( cursor > 0 ) { cursor--; } if ( !mark ) { marker = cursor; } }
		void Right  ( bool mark = false )  { if ( cursor < count ) { cursor++; } if ( !mark ) { marker = cursor; } }

		void CtrlLeft  ( bool mark = false );
		void CtrlRight ( bool mark = false );

		void SetCursor ( int n, bool mark = false ) { if ( n > count ) { n = count - 1; } if ( n < 0 ) { n = 0; } cursor = n; if ( !mark ) { marker = cursor; }}
		void Unmark ()       { marker = cursor; }

		bool Marked() const { return cursor != marker; }
		bool DelMarked();
		bool Marked( int n ) const
		{
			return cursor != marker &&
			       ( ( cursor < marker && n >= cursor && n < marker ) || ( cursor > marker && n < cursor && n >= marker ) );
		}

		void Insert( unicode_t t );
		void Insert( const unicode_t* txt );
		void Del( bool DelteWord );
		void Backspace( bool DeleteWord );
		void Set( const unicode_t* s, bool mark = false );

		const unicode_t* Ptr() const { return data.data(); }
		unicode_t* Ptr() { return data.data(); }
		unicode_t operator []( int n ) { return data[n]; }

		int   Count()  const { return count; }
		int   Cursor() const { return cursor; }
		int   Marker() const { return marker; }

		void  Clear() { marker = cursor = count = 0; }

		const std::vector<unicode_t>& GetText() const { return data; }
		void SetText( const std::vector<unicode_t>& t ) { Set( t.data(), false ); }

		EditBuf( const unicode_t* s ): size( 0 ), count( 0 ) { Set( s );}
		EditBuf(): size( 0 ), count( 0 ), cursor( 0 ), marker( 0 ) {}

		static int GetCharGroup( unicode_t c ); //for word left/right (0 - space)
	};

	enum EditLineCmd
	{
		// subcommands of CMD_EDITLINE_INFO
		SCMD_EDITLINE_CHANGED = 100,
		SCMD_EDITLINE_DELETED,
		SCMD_EDITLINE_INSERTED
	};

	class clValidator: public iIntrusiveCounter
	{
	public:
		virtual ~clValidator() {};
		virtual bool IsValid( const std::vector<unicode_t>& Str ) const = 0;
	};

	class clUnsignedInt64Validator: public clValidator
	{
	public:
		virtual bool IsValid( const std::vector<unicode_t>& Str ) const override;
	};

	/// validate date in format dd.mm.yyyy
	class clDateValidator: public clValidator
	{
	public:
		virtual bool IsValid( const std::vector<unicode_t>& Str ) const override;
	};

	/// validate time in format HH:MM:SS,MSEC
	class clTimeValidator: public clValidator
	{
	public:
		virtual bool IsValid( const std::vector<unicode_t>& Str ) const override;
	};

	class EditLine: public Win
	{
	public:
		enum FLAGS
		{
			USEPARENTFOCUS = 1,
			READONLY = 2
		};

	private:
		bool _use_alt_symbols;
		unsigned _flags;
		bool UseParentFocus() const { return (_flags & USEPARENTFOCUS) != 0; }
		EditBuf text;
		int _chars;
		bool cursorVisible;
		bool passwordMode;
		bool showSpaces;
		// quick search line accepts alt keys. Edit controls do not: alt is used for hotkeys in dialogs
		bool doAcceptAltKeys;
		int first;
		bool frame3d;
		int charH;
		int charW;
		bool m_ReplaceMode;

		clPtr<clValidator> m_Validator;

		CaptureSD captureSD;

		void DrawCursor( GC& gc );
		bool CheckCursorPos(); //true - if redrawing is needed
		void ClipboardCopy();
		void ClipboardPaste();
		void ClipboardCut();
		int GetCharPos( cpoint p );
	protected:
		virtual void Changed() { if ( Parent() ) { Parent()->Command( CMD_EDITLINE_INFO, SCMD_EDITLINE_CHANGED, this, 0 ); } }
		virtual void SendCommand(const int cmd)
		{
			if ( Parent() )
			{
				Parent()->Command( CMD_EDITLINE_INFO, cmd, this, 0 );
			}
		}
	public:
		EditLine( int nId, Win* parent, const crect* rect, const unicode_t* txt, int chars = 10, bool frame = true, unsigned flags = 0 );
		void SetAcceptAltKeys() { doAcceptAltKeys = true; }
		virtual void Paint( GC& gc, const crect& paintRect );
		virtual bool EventMouse( cevent_mouse* pEvent );
		virtual bool EventKey( cevent_key* pEvent );
		virtual void EventTimer( int tid );
		virtual bool EventFocus( bool recv );
		virtual void EventSize( cevent_size* pEvent );
		virtual bool InFocus() override;
		virtual void SetFocus() override;
		void Clear();
		void SetText( const unicode_t* txt, bool mark = false );
		void SetText( const std::string& utf8txt, bool mark = false );
		void Insert( unicode_t t );
		void Insert( const unicode_t* txt );
		bool IsEmpty() const { return text.Count() == 0; }
		bool IsReadOnly() const { return (_flags & READONLY) != 0; }
		int GetCursorPos() { return text.Cursor(); }
		int GetMarkerPos() { return text.Marker(); }
		void SetCursorPos( int c, bool mark = false ) { text.SetCursor( c, mark ); }
		void SetReplaceMode( bool ReplaceMode ) { m_ReplaceMode = ReplaceMode; }
		std::vector<unicode_t> GetText() const;
		std::string GetTextStr() const;
		void SetPasswordMode( bool enable = true ) { passwordMode = enable; Invalidate(); }
		void SetShowSpaces( bool enable = true ) { showSpaces = enable; Invalidate(); }
		virtual int UiGetClassId();
		virtual void OnChangeStyles();
		void EnableAltSymbols( bool e ) { _use_alt_symbols = e; }
		virtual ~EditLine();
		void SetValidator( clPtr<clValidator> Validator ) { m_Validator = Validator; }
	};

	extern cpoint GetStaticTextExtent( GC& gc, const unicode_t* s, cfont* font );

	class StaticLine: public Win
	{
	public:
		enum ALIGN
		{
			LEFT = -1,
			CENTER = 0,
			RIGHT = 1
		};
		//MenuTextInfo text;
		std::vector<unicode_t> text;
		ALIGN align;
		int width; //width in font HEIGHTS
	public:
		StaticLine( int nId, Win* parent, const unicode_t* txt, crect* rect = nullptr, ALIGN al = LEFT, int w = -1 );
		virtual void Paint( GC& gc, const crect& paintRect );
		//void SetText( const unicode_t* txt ) { text.SetText(txt) ; Invalidate(); }
		void SetTextUtf8( const std::string& txt );
		void SetText( const unicode_t* txt );
		virtual int UiGetClassId();
	};

	// single line label that may have '&' in the text to designate hotkey
	// Has master dialog control, which is returned by IsHisHotKey on matching hotkey
	class StaticLabel : public Win
	{
		MenuTextInfo text;
		Win* master;
	public:
		StaticLabel( int nId, Win* parent, const unicode_t* txt, Win* _master = 0, crect* rect = 0 );
		virtual void Paint( GC& gc, const crect& paintRect );
		virtual Win* IsHisHotKey( cevent_key* pEvent );
		virtual int UiGetClassId();
	};


	enum ScrollCmd
	{
		//subcommands of CMD_SCROLL_INFO
		SCMD_SCROLL_VCHANGE = 1,
		SCMD_SCROLL_HCHANGE,

		SCMD_SCROLL_LINE_UP,
		SCMD_SCROLL_LINE_DOWN,
		SCMD_SCROLL_LINE_LEFT,
		SCMD_SCROLL_LINE_RIGHT,
		SCMD_SCROLL_PAGE_UP,
		SCMD_SCROLL_PAGE_DOWN,
		SCMD_SCROLL_PAGE_LEFT,
		SCMD_SCROLL_PAGE_RIGHT,
		SCMD_SCROLL_TRACK
	};

	struct ScrollInfo
	{
		seek_t m_Size;
		seek_t m_PageSize;
		seek_t m_Pos;
		bool   m_AlwaysHidden;
		ScrollInfo()
			: m_Size( 0 )
			, m_PageSize( 0 )
			, m_Pos( 0 )
			, m_AlwaysHidden( false )
		{}
		ScrollInfo( seek_t _size, seek_t _pageSize, seek_t _pos )
			: m_Size( _size )
			, m_PageSize( _pageSize )
			, m_Pos( _pos )
			, m_AlwaysHidden( false )
		{}
		bool operator==( const ScrollInfo& o )
		{
			return m_Size == o.m_Size && m_PageSize == o.m_PageSize && m_Pos == o.m_Pos && m_AlwaysHidden == o.m_AlwaysHidden;
		}
	};

	class ScrollBar: public Win
	{
		bool vertical;
		ScrollInfo si;

		int len; //length between end buttons
		int bsize; // length of middle button
		int bpos; // length from middle to first button

		crect b1Rect, b2Rect, b3Rect;

		bool trace; // state of tracing
		int traceBPoint; // point of mouse press from the beginning of middle button (set when tracing)

		bool autoHide;

		bool b1Pressed;
		bool b2Pressed;

		Win* managedWin;

		CaptureSD captureSD;

		void Recalc( cpoint* newSize = 0 );
		void SendManagedCmd( int subId, void* data );
	public:
		ScrollBar( int nId, Win* parent, bool _vertical, bool _autoHide = true,  crect* rect = 0 );
		void SetScrollInfo( ScrollInfo* s );
		void SetManagedWin( Win* w = 0 ) { managedWin = w; SetScrollInfo( 0 ); }
		virtual void Paint( GC& gc, const crect& paintRect );
		virtual bool Command( int id, int subId, Win* win, void* data );
		virtual bool EventMouse( cevent_mouse* pEvent );
		virtual void EventTimer( int tid );
		virtual void EventSize( cevent_size* pEvent );
		virtual int UiGetClassId();
		virtual ~ScrollBar();
	};


	class VListWin: public Win
	{
	public:
		enum SelectType { NO_SELECT = 0, SINGLE_SELECT, MULTIPLE_SELECT };
		enum BorderType { NO_BORDER = 0, SINGLE_BORDER, BORDER_3D};
	private:
		SelectType selectType;
		BorderType borderType;

		int itemHeight;
		int itemWidth;

		int xOffset;
		int count;
		int first;
		int current;
		int pageSize;
		int captureDelta;

		unsigned borderColor; //used only for SINGLE_BORDER
		unsigned bgColor; //for 3d border and scroll block

		CaptureSD captureSD;

		ScrollBar vScroll;
		ScrollBar hScroll;

		Layout layout;

		crect listRect; //rect in which we will draw the list
		crect scrollRect;

		IntList selectList;

	protected:
		void SetCount( int );
		void SetItemSize( int h, int w );
		void CalcScroll();
//	void SetBlockLSize(int);
//	void SetBorderColor(unsigned);

		int GetCount() { return count; }
		int GetItemHeight() { return itemHeight; }
		int GetItemWidth() { return itemWidth; }

		void SetCurrent( int );
	public:
		VListWin( WTYPE t, unsigned hints, int nId, Win* _parent, SelectType st, BorderType bt, crect* rect );
		virtual void DrawItem( GC& gc, int n, crect rect );

		void MoveCurrent( int n, bool mustVisible = true );
		void MoveFirst( int n );
		void MoveXOffset( int n );
		void SetNoCurrent();

		int GetCurrent() const { return current; }
		int GetPageFirstItem() const { return first; }
		int GetPageItemCount() const { return pageSize; }


		void ClearSelection();
		void ClearSelected( int n1, int n2 );
		void SetSelected( int n1, int n2 );
		bool IsSelected( int );

		virtual void Paint( GC& gc, const crect& paintRect );
		virtual void EventSize( cevent_size* pEvent );
		virtual bool EventKey( cevent_key* pEvent );
		virtual bool EventMouse( cevent_mouse* pEvent );
		virtual bool EventFocus( bool recv );
		virtual bool Command( int id, int subId, Win* win, void* data );
		virtual void EventTimer( int tid );
		virtual int UiGetClassId();
	};


	struct TLNode
	{
		int pixelWidth;
		std::vector<unicode_t> str;
		long intData;
		void* ptrData;
		TLNode() : pixelWidth( -1 ), intData( 0 ), ptrData( 0 ) {}
		TLNode( const unicode_t* s, int i = 0, void* p = 0 ) : pixelWidth( -1 ), str( new_unicode_str( s ) ), intData( i ), ptrData( p ) {}
	};

	class TextList: public VListWin
	{
		std::vector<TLNode> list;
		bool valid;
		int fontH;
		int fontW;
	public:
		TextList( WTYPE t, unsigned hints, int nId, Win* _parent, SelectType st, BorderType bt, crect* rect );
		virtual void DrawItem( GC& gc, int n, crect rect );
		void Clear();
		void Append( const unicode_t* txt, int i = 0, void* p = 0 );
		void DataRefresh();

		const unicode_t* GetCurrentString() { int cnt = list.size(), c = GetCurrent(); return c < 0 || c >= cnt ? 0 : list[ c ].str.data(); }
		void* GetCurrentPtr() { int cnt = list.size(), c = GetCurrent(); return c < 0 || c >= cnt ? 0 : list[ c ].ptrData; }
		int GetCurrentInt() { int cnt = list.size(), c = GetCurrent(); return c < 0 || c >= cnt ? -1 : list[ c ].intData; }

		void SetHeightRange( LSRange range ); //in characters
		void SetWidthRange( LSRange range ); //in characters
		virtual int UiGetClassId();
		virtual ~TextList();
	};



	enum CmdMenuInfo
	{
		//subcommands of CMD_MENU_INFO
		SCMD_MENU_CANCEL = 0,
		SCMD_MENU_SELECT = 1
	};

	class PopupMenu;

	class MenuData: public iIntrusiveCounter
	{
	public:
		enum TYPE { CMD = 1, SPLIT,   SUB   };

		struct Node
		{
			int type;
			int id;
			//std::vector<unicode_t> leftText;
			MenuTextInfo leftText;

			std::vector<unicode_t> rightText;
			MenuData* sub;
			//Node(): type( 0 ), sub( 0 ) {}
			Node( int _type, int _id, const unicode_t* s,  const unicode_t* rt, MenuData* _sub )
				: type( _type ), id( _id ), leftText( s ), sub( _sub )
			{
				//if ( s ) { leftText = new_unicode_str( s ); }

				if ( rt ) { rightText = new_unicode_str( rt ); }
			}
		};

	private:
		friend class PopupMenu;

		wal::ccollect<Node> list;
	public:
		MenuData();
		int Count() { return list.count(); }
		void AddCmd( int id, const unicode_t* s, const unicode_t* rt = 0 ) { Node node( CMD, id, s, rt, 0 ); list.append( node ); }
		void AddCmd( int id, const char* s, const char* rt = 0 ) { AddCmd( id, utf8_to_unicode( s ).data(), rt ? utf8_to_unicode( rt ).data() : 0 ); }
		void AddSplit() { Node node( SPLIT, 0, 0, 0, 0 ); list.append( node ); }
		void AddSub( const unicode_t* s, MenuData* data ) { Node node( SUB, 0, s, 0, data ); list.append( node ); }
		void AddSub( const char* s, MenuData* data ) { AddSub( utf8_to_unicode( s ).data(), data ); }
		~MenuData() {}
	};

	class PopupMenu: public Win
	{
		struct Node
		{
			MenuData::Node* data;
			crect rect;
			bool enabled;
		};
		wal::ccollect<Node> list;
		int selected;
		clPtr<PopupMenu> sub;
		bool SetSelected( int n );
		bool OpenSubmenu();
		Win* cmdOwner;

		bool IsCmd( int n );
		bool IsSplit( int n );
		bool IsSub( int n );
		bool IsEnabled( int n );

		int fontHeight;
		int leftWidth;
		int rightWidth;

		void DrawItem( GC& gc, int n );
	public:
		PopupMenu( int nId, Win* parent, MenuData* d, int x, int y, Win* _cmdOwner = 0 );
		virtual bool EventMouse( cevent_mouse* pEvent );
		virtual bool EventKey( cevent_key* pEvent );
		virtual void Paint( GC& gc, const crect& paintRect );
		virtual bool Command( int id, int subId, Win* win, void* data );
		virtual int UiGetClassId();
		virtual ~PopupMenu();
	};

	int DoPopupMenu( int nId, Win* parent, MenuData* d, int x, int y );

	class MenuBar: public Win
	{
		struct Node
		{
			//std::vector<unicode_t> text;
			MenuTextInfo text;
			MenuData* data;
			Node( MenuTextInfo _text, MenuData* _data ) : text( _text ), data( _data ) {}
		};

		wal::ccollect<Node> list;
		wal::ccollect<crect> rectList;
		int select;
		int lastMouseSelect;
		clPtr<PopupMenu> sub;

		crect ItemRect( int n );
		void InvalidateRectList() { rectList.clear(); }
		void DrawItem( GC& gc, int n );
		void SetSelect( int n );
		void OpenSub();
		void Left() { SetSelect( select <= 0 ? ( list.count() - 1 ) : ( select - 1 ) ); }
		void Right() { SetSelect( select == list.count() - 1  ? 0 : select + 1 ); }
		int GetPointItem( cpoint p );
	public:
		MenuBar( int nId, Win* parent, crect* rect = 0 );
		void Paint( GC& gc, const crect& paintRect );
		virtual bool EventMouse( cevent_mouse* pEvent );
		virtual bool EventKey( cevent_key* pEvent );
		virtual bool Command( int id, int subId, Win* win, void* d );
		virtual bool EventFocus( bool recv );
		virtual void EventEnterLeave( cevent* pEvent );
		virtual void EventSize( cevent_size* pEvent );
		virtual int UiGetClassId();
		virtual int GetSelect() const;
		virtual void OnChangeStyles();

		void Clear() { list.clear(); InvalidateRectList(); }
		void Add( MenuData* data, const unicode_t* text );
		virtual ~MenuBar();
	};

////////////////////////// ComboBox

	class ComboBox: public Win
	{
	public:
		enum FLAGS
		{
			MODE_UP  = 1,
			READONLY = 2
		};
	
    private:
		unsigned _flags;
		CaptureSD captureSD;
		Layout _lo;
		
        struct Node
		{
			std::vector<unicode_t> text;
			void* data;
		};
		
        EditLine _edit;
		crect _buttonRect;
		ccollect<Node, 0x100> _list;
        clPtr<TextList> _box;
//		int _cols;
		int _rows;
		int _current;

	protected:
		void OpenBox();
		void RefreshBox();

		bool IsEditLine( Win* w ) const { return w == &_edit; }

	public:
		ComboBox( int nId, Win* parent, int cols, int rows, unsigned flags = 0,  crect* rect = 0 );
        virtual ~ComboBox() {}
		
        virtual void Paint( GC& gc, const crect& paintRect );
		virtual bool EventMouse( cevent_mouse* pEvent );
        virtual bool EventKey( cevent_key* pEvent );
		virtual bool EventFocus( bool recv );
		virtual bool Command( int id, int subId, Win* win, void* d );
		virtual void OnChangeStyles();
		virtual int UiGetClassId();
		
        void Clear();
		void Append( const unicode_t* text, void* data = 0 );
		void Append( const char* text, void* data = 0 );

		std::vector<unicode_t> GetText() const;
		std::string GetTextStr() const;
		void SetText( const unicode_t* txt, bool mark = false );
		void SetText( const std::string& utf8txt, bool mark = false );
		void InsertText( unicode_t t );
		void InsertText( const unicode_t* txt );
		int GetCursorPos() { return _edit.GetCursorPos(); }
		int GetMarkerPos() { return _edit.GetMarkerPos(); }
		void SetCursorPos( int c, bool mark = false ) { _edit.SetCursorPos( c, mark ); }

		bool IsReadOnly() const { return (_flags & ComboBox::READONLY) != 0; }
		int Count() const { return _list.count(); }
		int Current() const { return _current; }
		const unicode_t* ItemText( int n );

		void* ItemData( int n );
		void MoveCurrent( int n, bool notify = true );
		bool IsBoxOpened() { return _box.ptr() != 0; }
		void CloseBox();
		
        virtual bool OnOpenBox();
		virtual void OnCloseBox();
		virtual void OnItemChanged( int ItemIndex );
	};

//ToolTip is global, setting new one is replaceing previous
	void ToolTipShow( Win* w, int x, int y, const unicode_t* s );
	void ToolTipShow( int x, int y, const char* s );
	void ToolTipHide();


}// namespace wal
