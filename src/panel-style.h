/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#pragma once

struct PanetItemColor
{
	int bg;
	int text;
};

struct PanelHeaderColors
{
	int bg;
	int text;
};

struct PanelFooterColors
{
	int bg;
	int text;
	int selText;
	int operText;
};

struct PanelColors
{
	int modeLine;

	int border1;
	int border2;
	int border3;
	int border4;

	int hLine;

	int v1; //painting of vertical lines
	int v2;
	int v3;

	void ( *itemF )( PanetItemColor* p,
	                 bool operPanel,
	                 bool currentPanel,

	                 bool active,
	                 bool selected,

	                 bool bad,
	                 bool hidden,
	                 bool dir
	               );

	void ( *headF )( PanelHeadColor* p, bool selectedPanel );

	PanelFootColor foot;
};

extern PanelColors defPanelColors;
extern PanelColors blackPanelColors;
