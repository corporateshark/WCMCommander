/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#include "swl.h"
// XXX: refactor to move the header to .
#include "../unicode_lc.h"

#include <unordered_map>

namespace wal
{

	MenuData::MenuData() {}

// temporary thing
	std::unordered_map<int, clPtr<cicon> > iconList;

	cicon* GetCmdIcon( int cmd )
	{
		{
			auto i = iconList.find( cmd );

			if ( i != iconList.end() ) { return i->second.ptr(); }
		}

		clPtr<cicon> pic = new cicon( cmd, 16, 16 );
		cicon* t = pic.ptr();
		iconList[cmd] = pic;
		return t;
	}

	unsigned short RightMenuPointer[] = {7, 1, 3, 7, 0xF, 7, 3, 1};

#define MENU_SPLITTER_H 3
#define MENU_LEFT_BLOCK 24
#define MENU_RIGHT_BLOCK 16
#define MENU_ITEM_HEIGHT_ADD (8)
#define MENU_ICON_SIZE 16
#define MENU_TEXT_OFFSET 3

	bool PopupMenu::IsCmd( int n ) { return n >= 0 && n < list.count() && list[n].data->type == MenuData::CMD; }
	bool PopupMenu::IsSplit( int n ) { return n >= 0 && n < list.count() && list[n].data->type == MenuData::SPLIT; }
	bool PopupMenu::IsSub( int n ) { return n >= 0 && n < list.count() && list[n].data->type == MenuData::SUB; }
	bool PopupMenu::IsEnabled( int n ) { return n < 0 || n >= list.count() || list[n].enabled; }


	int uiClassPopupMenu = GetUiID( "PopupMenu" );
	int PopupMenu::UiGetClassId() {   return uiClassPopupMenu; }

	void PopupMenu::DrawItem( GC& gc, int n )
	{
		if ( n < 0 || n >= list.count() ) { return; }

		UiCondList ucl;

		if ( n == selected ) { ucl.Set( uiCurrentItem, true ); }

		int color_text = UiGetColor( uiColor, uiItem, &ucl, 0x0 );
		int color_hotkey = UiGetColor( uiHotkeyColor, uiItem, &ucl, 0x0 );
		int color_bg = UiGetColor( uiBackground, uiItem, &ucl, 0xFFFFFF );
		int color_left = UiGetColor( uiBackground, 0, 0, 0xFFFFFF );

		crect r = list[n].rect;
		int height = r.Height();
		r.right = MENU_LEFT_BLOCK;
		gc.SetFillColor( n == selected ? color_bg : color_left );
		gc.FillRect( r );
		r = list[n].rect;
		r.left = MENU_LEFT_BLOCK ;
		gc.SetFillColor( color_bg );
		gc.FillRect( r );

		unsigned colorLine = UiGetColor( uiLineColor, uiItem, &ucl, 0 );
		gc.SetLine( colorLine );
		gc.MoveTo( r.left, r.top );
		gc.LineTo( r.left, r.bottom );

		if ( IsSplit( n ) )
		{
			gc.SetLine( colorLine );
			int y = r.top + ( r.Height() - 1 ) / 2;
			gc.MoveTo( 1, y );
			gc.LineTo( r.right, y );
		}
		else
		{
			gc.Set( GetFont() );
			MenuTextInfo& lText = ( list[n].data->leftText );
			//unicode_t* lText = list[n].data->leftText.data();
			unicode_t* rText = list[n].data->rightText.data();

			//if ( lText ) { gc.TextOutF( MENU_LEFT_BLOCK + MENU_TEXT_OFFSET, r.top + ( height - fontHeight ) / 2, lText ); }
			if ( !lText.isEmpty() )
			{
				lText.DrawItem( gc, MENU_LEFT_BLOCK + MENU_TEXT_OFFSET, r.top + ( height - fontHeight ) / 2, color_text, color_hotkey );
			}

			if ( rText )
			{
				gc.SetTextColor( color_text );
				gc.TextOutF( MENU_LEFT_BLOCK + MENU_TEXT_OFFSET + leftWidth + fontHeight, r.top + ( height - fontHeight ) / 2, rText );
			}

			if ( IsSub( n ) )
			{
				int y = r.top + ( height - RightMenuPointer[0] ) / 2;
				DrawPixelList( gc, RightMenuPointer, r.right - 10 , y, UiGetColor( uiPointerColor, uiItem, &ucl, 0 ) );
			}

			if ( IsCmd( n ) )
			{
				int y = r.top + ( height - MENU_ICON_SIZE ) / 2;
				int x =  ( MENU_LEFT_BLOCK - MENU_ICON_SIZE ) / 2;
				gc.DrawIcon( x, y, GetCmdIcon( list[n].data->id ) );
			}

		}
	}

	PopupMenu::PopupMenu( int nId, Win* parent, MenuData* d, int x, int y, Win* _cmdOwner )
		:  Win( Win::WT_POPUP,
		        Win::WH_CLICKFOCUS, //0,
		        parent, 0, nId ),
		selected( 0 ),
		cmdOwner( _cmdOwner ),
		leftWidth( 0 ),
		rightWidth( 0 )
	{
		GC gc( this );
		gc.Set( GetFont() );
		int itemH = gc.GetTextExtents( ABCString ).y;

		fontHeight = itemH;

		if ( itemH < MENU_ICON_SIZE ) { itemH = MENU_ICON_SIZE; }

		itemH += MENU_ITEM_HEIGHT_ADD;

		int height = 1;
		int count = d->list.count();
		int i;

		for ( int i = 0; i < count; i++ )
		{
			Node node;
			node.data = &d->list[i];
			node.rect.left = 1;
			node.rect.top = height;

			if ( node.data->type == MenuData::SPLIT )
			{
				node.rect.bottom = node.rect.top + MENU_SPLITTER_H;
				node.enabled = true;
			}
			else
			{
				cpoint p;
				MenuTextInfo& leftText = node.data->leftText;

				if ( !node.data->leftText.isEmpty() )
				{
					p = leftText.GetTextExtents( gc );

					if ( leftWidth < p.x ) { leftWidth = p.x; }
				}

				if ( node.data->rightText.data() )
				{
					p = gc.GetTextExtents( node.data->rightText.data() );

					if ( rightWidth < p.x ) { rightWidth = p.x; }
				}

				node.rect.bottom = node.rect.top + itemH;
				node.enabled = ( node.data->type == MenuData::CMD && cmdOwner ) ? cmdOwner->Command( CMD_CHECK, node.data->id, this, 0 ) : true;
			}

			height += node.rect.Height() - 1;
			list.append( node );
		}

		int maxWidth = 3 + MENU_LEFT_BLOCK + MENU_RIGHT_BLOCK + MENU_TEXT_OFFSET + leftWidth;

		if ( rightWidth > 0 )
		{
			maxWidth += rightWidth + fontHeight;
		}

		height += 2;
		int itemW = maxWidth - 2;

		for ( i = 0; i < count; i++ )
		{
			list[i].rect.right = list[i].rect.left + itemW;
		}

		crect rect( x, y, x + maxWidth, y + height );
		this->Move( rect );
	}

	bool PopupMenu::OpenSubmenu()
	{
		if ( !sub.ptr() && IsSub( selected ) )
		{
			crect rect = this->ScreenRect();
			sub = new PopupMenu( 0, this, list[selected].data->sub, rect.right, rect.top + list[selected].rect.top, cmdOwner );
			sub->Show( Win::SHOW_INACTIVE );
			sub->Enable( true );
			return true;
		}

		return false;
	}

	bool PopupMenu::Command( int id, int subId, Win* win, void* d )
	{
		if ( id == CMD_CHECK )
		{
			return Parent() ? Parent()->Command( id, subId, this, d ) : false;
		}

		if ( IsModal() ) { EndModal( id ); }

		return ( Parent() ) ?  Parent()->Command( id, subId, win, d ) : false;
	}

	bool PopupMenu::SetSelected( int n )
	{
		if ( n == selected ) { return true; }

		if ( n < 0 || n > list.count() || IsSplit( n ) ) { return false; }

		if ( sub.ptr() ) { sub.clear(); }

		int old = selected;
		selected = n;
		GC gc( this );
		DrawItem( gc, old );
		DrawItem( gc, selected );
		return true;
	}

	bool PopupMenu::EventMouse( cevent_mouse* pEvent )
	{
		int N = -1;
		cpoint point = pEvent->Point();

		for ( int i = 0; i < list.count(); i++ )
		{
			crect* pr = &( list[i].rect );

			if ( point.x >= pr->left && point.x < pr->right && point.y >= pr->top && point.y < pr->bottom )
			{
				N = i;
				break;
			}
		}

		switch ( pEvent->Type() )
		{
			case EV_MOUSE_MOVE:
				if ( N < 0 )
				{
					if ( !sub.ptr() ) { return false; }

					crect rect = ScreenRect();
					crect sr = sub->ScreenRect();
					pEvent->Point().x += rect.left - sr.left;
					pEvent->Point().y += rect.top - sr.top;
					return sub->EventMouse( pEvent );
				}

				if ( SetSelected( N ) )
				{
					if ( this->IsSub( selected ) && IsEnabled( selected ) )
					{
						OpenSubmenu();
					}
				}

				return true;


			case EV_MOUSE_PRESS:
				if ( N < 0 )
				{
					if ( sub.ptr() )
					{
						crect rect = ScreenRect();
						crect sr = sub->ScreenRect();
						pEvent->Point().x += rect.left - sr.left;
						pEvent->Point().y += rect.top - sr.top;

						if ( sub->EventMouse( pEvent ) ) { return true; }
					}

					if ( IsModal() )
					{
						EndModal( 0 );
					}
					else if ( Parent() ) { Parent()->Command( CMD_MENU_INFO, SCMD_MENU_CANCEL, this, 0 ); }

					return true;
				}

				if ( SetSelected( N ) )
				{
					if ( IsCmd( selected ) && IsEnabled( selected ) )
					{
						int id = list[N].data->id;

						if ( IsModal() ) { EndModal( id ); }
						else if ( Parent() ) { Parent()->Command( id, 0, this, 0 ); }
					}
					else
					{
						OpenSubmenu();
					}
				}

				return true;
		}

		return false;
	}

	bool PopupMenu::EventKey( cevent_key* pEvent )
	{
		if ( sub.ptr() && sub->EventKey( pEvent ) ) { return true; }

		if ( pEvent->Type() == EV_KEYDOWN )
		{
			int i;

			switch ( pEvent->Key() )
			{
				case VK_DOWN:
					for ( i = ( selected >= 0 && selected + 1 < list.count() ) ? selected + 1 : 0; i < list.count(); i++ )
					{
						if ( !IsSplit( i ) )
						{
							SetSelected( i );
							return true;
						}
					}

					break;

				case  VK_UP:
					for ( i = ( selected > 0 ) ? selected - 1 : list.count() - 1; i >= 0; i-- )
					{
						if ( !IsSplit( i ) )
						{
							SetSelected( i );
							return true;
						}
					}

					break;


				case VK_RIGHT:
					if ( OpenSubmenu() ) { return true; };

					break;

				//case VK_LEFT:
				// return false; // to prevent grabbing default case

				case VK_NUMPAD_RETURN:
				case VK_RETURN:
					if ( IsCmd( selected ) && IsEnabled( selected ) )
					{
						int id = list[selected].data->id;

						if ( IsModal() )
						{
							EndModal( id );
						}
						else if ( Parent() )
						{
							Parent()->Command( id, 0, this, 0 );
						}

						return true;
					}

					return OpenSubmenu();

				case VK_ESCAPE:
					if ( IsModal() )
					{
						EndModal( 0 );
					}
					else if ( Parent() )
					{
						Parent()->Command( CMD_MENU_INFO, SCMD_MENU_CANCEL, this, 0 );
					}
					else {/* Botva */}

					return true;

				default:
				{
					// check if hotkey matches, and process
					// XXX: pEvent->Key() returns key (like Shift-F1, etc). isHotkeyMatching() expects unicode char, which is not the same
					unicode_t c = UnicodeUC( pEvent->Char() );

					for ( int i = 0; i < list.count(); i++ )
					{
						MenuData::Node* node = list[i].data;

						if ( node->leftText.isHotkeyMatching( c ) )
						{
							if ( node->id != 0 ) // menu command
							{
								if ( Parent() )
								{
									Parent()->Command( node->id, 0, this, 0 );
								}

								return true;
							}
							else // sumbenu
							{
								SetSelected( i );
								OpenSubmenu();
								return true;
							}
						}
					}

					return false;
				}

			}
		}

		return false;
	}


	void PopupMenu::Paint( GC& gc, const crect& paintRect )
	{
		crect rect = ClientRect();
		gc.SetFillColor( UiGetColor( uiBackground, 0, 0, 0xFFFFFF ) );
		gc.FillRect( rect );
		DrawBorder( gc, rect, UiGetColor( uiFrameColor, 0, 0, 0 ) );

		for ( int i = 0; i < list.count(); i++ )
		{
			DrawItem( gc, i );
		}
	}

	PopupMenu::~PopupMenu() {}


	int DoPopupMenu( int nId, Win* parent, MenuData* d, int x, int y )
	{
		PopupMenu menu( nId, parent, d, x, y );
		menu.Show();
		menu.Enable();
		menu.SetCapture();
		int ret = 0;

		try
		{
			ret = menu.DoModal();
			menu.ReleaseCapture();
		}
		catch ( ... )
		{
			menu.ReleaseCapture();
			throw;
		}

		return ret;
	}



}; //namespace wal
