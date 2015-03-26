/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "color-style.h"


using namespace wal;
/*
How the color scheme works:

Colors are defined by a set of the statements in the rules text:
CONTROL_CLASSID@ITEMID:CONDITION[:CONDITION2...] {COLOR_NAME: 0xBBGGRR; ...}

ITEMID and CONDITION are optional, and can be omitted. To define default colorset for all classes
that are not explicitly defined in the rules use '*' for CONTROL_CLASSID

String tokens for CONTROL_CLASSID,ITEMID,CONDITION,COLOR_NAME from the  rules are converted to integers
upon parsing the rules text on startup. These integers can be retrieved by color consumers via
int ::GetUiID(char* itemName)

String for CONTROL_CLASSID is defined by control C++ class with Win::UiGetClassId() or its overrides.

CONDITION is set/reset with UiCondList::Set()

Finally 0xBBGGRR is retrieved via
Win::UiGetColor( int CONTROL_CLASSID, int ITEMID, UiCondList* conditionList, unsigned default0xBBGGRR)

The 0xBBGGRR is used in
GC::SetTextColor()/GC::SetFillColor()/etc

*/

static char uiDefaultWcmRules[] =
   "* {color: 0; background: 0xD8E9EC; hotkey-color:0x0000FF ;focus-frame-color : 0x00A000; button-color: 0xD8E9EC;  }"
   "*@item:odd		{color: 0; background: 0xE0FFE0; }"
   "*@item:current-item	{color: 0xFFFFFF; background: 0x800000; }"
   "*@item			{color: 0; background: 0xFFFFFF; }"

   "ScrollBar { button-color: 0xD8E9EC;  }"
   "EditLine:!enabled { background: 0xD8E9EC; }"
   "EditLine:focus {color: 0; background: 0x909000; mark-color: 0xFFFFFF; mark-background : 0x800000; frame-color: 0xD8E9EC }"
   "EditLine {color: 0; background: 0x909000; frame-color: 0xD8E9EC; mark-color: 0xFFFFFF; mark-background : 0x808080; }"

   "#variable { color : 0x800000 }"
   "#value { color : 0x009000 }"

   "Progress { color:0xA00000; frame-color: 0x808080; background: 0xD8E9EC }"

   "command-line:!enabled { background: 0; }"
   "command-line:focus {color: 0xFFFFFF; background: 0; mark-color: 0xFFFFFF; mark-background : 0x800000; frame-color: 0xD8E9EC }"
   "command-line {color: 0xFFFFFF; background: 0; frame-color: 0xD8E9EC; mark-color: 0xFFFFFF; mark-background : 0x808080; }"

   "Autocomplete:!enabled { background: 0xB0B000; }"
   "Autocomplete:current-item {color: 0xFFFFFF; background: 0x000000; mark-color: 0xFFFFFF; mark-background : 0x000000; frame-color: 0x000000 }"
   "Autocomplete:current-item:odd {color: 0xFFFFFF; background: 0x000000; mark-color: 0xFFFFFF; mark-background : 0x000000; frame-color: 0x000000 }"
   "Autocomplete:odd {color: 0xFFFFFF; background: 0xA9A900; frame-color: 0xFFFFFF; mark-color: 0xFFFFFF; mark-background : 0x000000; }"
   "Autocomplete {color: 0xFFFFFF; background: 0xB0B000; frame-color: 0xFFFFFF; mark-color: 0xFFFFFF; mark-background : 0x000000; }"

   "Panel { color: 0xFFFF00; background: 0x800000; }"             // default foreground color for all items - light cyan; RGB(0,255,255)
   "Panel@item:selected-panel:current-item { color: 0x000000 }"         // default foreground color for all items in under cursor - black
   "Panel@item:selected:current-item { color: 0x00FFFF }"            // default foreground color for all selected items in under cursor - light yellow

   "Panel@item:exe { color: 0x00FF00 }"                           // foreground color for executables - light green
   "Panel@item:exe:selected-panel:current-item { color: 0x00FF00 }"     // foreground color for executables under cursor - light green
   "Panel@item:exe:selected:current-item { color: 0x00FFFF }"        // foreground color for selected executables under cursor - light yellow

   "Panel@item:dir { color: 0xFFFFFF }"                           // foreground color for dir names - white
   "Panel@item:dir:selected-panel:current-item { color: 0xFFFFFF }"     // foreground color for dir names under cursor - white
   "Panel@item:dir:selected:current-item { color: 0x00FFFF }"        // foreground color for selected dir names under cursor - light yellow

   "Panel@item:hidden { color: 0x808000 }"                        // foreground color for "hidden" items - dark cyan
   "Panel@item:hidden:selected-panel:current-item { color: 0x808080 }"  // foreground color for "hidden" items under cursor - dark gray; RGB(128,128,182)
   "Panel@item:hidden:selected:current-item { color: 0x00FFFF }"     // foreground color for selected "hidden" items under cursor - light yellow

   "Panel@item:link { color: 0xBBBB00 }"                          // foreground color for links - dark cyan
   "Panel@item:link :selected-panel:current-item { color: 0xBBBB00 }"      // foreground color for links under cursor - dark cyan
   "Panel@item:link:selected:current-item { color: 0x00FFFF }"          // foreground color for selected links under cursor - light yellow

   "Panel@item:bad { color: 0xA0 }"
   "Panel@item:selected { color: 0x00FFFF }"
   "Panel@item:selected-panel:current-item { background: 0x808000 }" // background color of cursor - dark cyan; RGB(0,128,128)
//"Panel@item:selected-panel:current-item:oper-state { background: 0xFF }"
   "Panel ScrollBar, Viewer ScrollBar, Editor ScrollBar {  background: 0xB0B000; button-color: 0x909000 }"

   // border-color1...4 - color of external borders, 1 is outmost, 4 is inmost. Make 2 and 4 light cyan RGB(0,255,255) and 1 and 3 equal to the color of background (dark blue)
   "Panel { border-color1: 0x800000; border-color2: 0xFFFF00; border-color3: 0x800000; border-color4: 0xFFFF00;"

   // vline-color1..3 - color of lines - colums separators. Make first two of them light cyan, 2 pixel thick and third line paint to the color of background (dark blue)
   " vline-color1: 0xFFFF00; vline-color2:0xFFFF00; vline-color3:0x800000;"

   // line-color - color of horizontal top and bottom pane above and below items list. Make it light cyan
   " line-color:0xFFFF00;"

   "summary-color: 0xFFFFFF; }"

   "Panel@footer { color: 0xFFFF00; background: 0x800000; }"         // footer - full info (name, size, date etc.) at the bottom of panel about the item under cursor - light cyan on dark blue

   "Panel { summary-color: 0xFFFF00; }"                           // summary - total size of files in current forlder at the bottom of panel; light cyan
   "Panel:have-selected { summary-color: 0x00FFFF; }"             // total size of selected items at the bottom of panel, light yellow
   "Panel:oper-state { summary-color: 0xFF; }"

   "ButtonWin { color: 0xFFFFFF; background: 0; }"
   "ButtonWin Button { color: 0; background: 0xB0B000; }"
   "EditLine#command-line {color: 0xFFFFFF; background: 0; mark-color: 0; mark-background : 0x800000; }"
   "ToolTip { color: 0; background: 0x80FFFF; }"
   "ToolBar { color: 0; background: 0xB0B000; }"
   "MenuBar { color: 0; background: 0xB0B000; current-item-frame-color:0x00FF00; }"
   "MenuBar:current-item { color: 0xFFFFFF; background: 0;  } "
   "PopupMenu@item {color: 0xFFFFFF; background: 0xA0A000; pointer-color:0; line:0; } "
   "PopupMenu@item:current-item { background: 0x404000; } "
   "PopupMenu { background: 0xC0C000; frame-color: 0xFFFFFF; } "

   "drive-dlg { background: 0xB0B000; }"
   "drive-dlg * { background: 0xB0B000; color: 0xFFFFFF; }"
   "drive-dlg * @item { color: 0xFFFFFF; background: 0xB0B000; comment-color: 0xCCCCCC; first-char-color:0xFFFF }"
   "drive-dlg * @item:current-item { color: 0xFFFFFF;  background: 0; comment-color: 0xCCCCCC; first-char-color:0xFFFF }"

   "#messagebox-red { color: 0xFFFFFF; background: 0xFF; }"
   "#messagebox-red Static { color: 0xFFFFFF; background: 0xFF; }"
   "HelpWin@style-head { color: 0xFFFFFF; }"
   "HelpWin@style-def  { color: 0xFFFFFF; }"
   "HelpWin@style-red  { color: 0xFF; }"
   "HelpWin@style-bold { color: 0xFFFF; }"
   "HelpWin { background: 0x808000; }"

   "Viewer { background: 0x800000; color:0xFFFFFF; ctrl-color:0xFF0000; "
   "	makr-color:	0; mark-background: 0xAAAA00; line-color:0x00FFFF; "
   "	hid: 0xB00000; load-background:	0xD00000; load-color: 0xFF;}"
   "Editor { background: 0x800000; color:0xFFFFFF; mark-background: 0xAAAA00; mark-color: 0; cursor-color: 0x00FFFF; "
   "	DEF:0xFFFFFF; KEYWORD:0x00FFFF; COMMENT:0xFF8080; STRING:0xFFFF00; PRE:0x00FF00; NUM:0xFFFF80; OPER:0x20FFFF; ATTN:0x0000FF; }"
   "StringWin { color: 0xFFFF00; background: 0; }"
   "EditorHeadWin, ViewerHeadWin { color: 0; background: 0xB0B000; prefix-color:0xFFFFFF; cs-color:0xFFFFFF;}"

   "TabDialog { background: 0x606060; color:0xFFFFFF; }"
   "TabDialog * { background: 0x606060; color:0xFFFFFF; button-color: 0x808080; }"
   "TabDialog EditLine { background: 0xD0D0D0; color:0; frame-color: 0x606060; }"
   "TabDialog *@item:dir {color: 0xFFFF00; background: 0; }"
   "TabDialog *@item:current-item	{background: 0x808080; }"
   "TabDialog *@item {color: 0xFFFFFF; background: 0; }"

   "Shortcuts VListWin { background: 0xB0B000; first-char-color:0xFFFF; }"
   "Shortcuts VListWin@item {color: 0; background: 0xB0B000; }"
   "Shortcuts VListWin@item:current-item {color: 0xFFFFFF; background: 0x800000; }"

   ;


//!*************************************************************************************************


static char uiBlackWcmRules[] =
   "* {color: 0; background: 0xA0B0B0; hotkey-color:0x0000FF; focus-frame-color : 0x00A000; button-color: 0xD8E9EC;  }"
   "*@item:odd		{color: 0xFFFFFF; background: 0x101010; }"
   "*@item:current-item:focus	{color: 0xFFFFFF; background: 0x800000; }"
   "*@item:current-item	{color: 0xFFFFFF; background: 0x505050; }"
   "*@item			{color: 0xFFFFFF; background: 0; }"

   "Progress { color:0xA00000; frame-color: 0x808080; background: 0xD8E9EC }"

   "ScrollBar { button-color: 0xD8E9EC;  }"
   "EditLine:!enabled { background: 0xD8E9EC; }"
   "EditLine:focus {color: 0; background: 0x909000; mark-color: 0xFFFFFF; mark-background : 0x800000; frame-color: 0xA0B0B0 }"
   "EditLine {color: 0; background: 0x909000; frame-color: 0xA0B0B0; mark-color: 0xFFFFFF; mark-background : 0x808080; }"

   "command-line:!enabled { background: 0; }"
   "command-line:focus {color: 0xFFFFFF; background: 0; mark-color: 0xFFFFFF; mark-background : 0x800000; frame-color: 0xD8E9EC }"
   "command-line {color: 0; background: 0; frame-color: 0xD8E9EC; mark-color: 0xFFFFFF; mark-background : 0x808080; }"

   "Autocomplete:!enabled { background: 0xB0B000; }"
   "Autocomplete:current-item {color: 0xFFFFFF; background: 0xB0B000; mark-color: 0xFFFFFF; mark-background : 0x000000; frame-color: 0x000000 }"
   "Autocomplete:current-item:odd {color: 0xFFFFFF; background: 0xB0B000; mark-color: 0xFFFFFF; mark-background : 0x000000; frame-color: 0x000000 }"
   "Autocomplete:odd {color: 0xFFFFFF; background: 0x000000; frame-color: 0xFFFFFF; mark-color: 0xFFFFFF; mark-background : 0x000000; }"
   "Autocomplete {color: 0xFFFFFF; background: 0x000000; frame-color: 0xFFFFFF; mark-color: 0xFFFFFF; mark-background : 0x000000; }"

   "Panel { color: 0xA0A0A0; background: 0x000000; }"
   "Panel@item:exe { color: 0x80FF80 }"
   "Panel@item:dir { color: 0xFFFFFF }"
   "Panel@item:hidden { color: 0xB06060 }"
   "Panel@item:hidden:selected-panel:current-item { color: 0x804040 }"
   "Panel@item:bad { color: 0xA0 }"
   "Panel@item:selected { color: 0x00FFFF }"
   "Panel@item:selected-panel:current-item { background: 0xA0A000 }"

   "Panel@item:selected-panel:current-item:!exe:!dir:!hidden:!bad:!selected { color:0 }"

   "Panel@item:selected-panel:current-item:oper-state { background: 0xFF }"
   "Panel ScrollBar, Viewer ScrollBar, Editor ScrollBar {  background: 0x808080; button-color: 0xB0B000 }"
   "Panel { border-color1: 0x000000; border-color2: 0xFFFFFF; border-color3: 0xFFFFFF; border-color4: 0x000000;"
   " vline-color1: 0xFFFFFF; vline-color2:0x707070; vline-color3:0x700000;"
   " line-color:0xFFFFFF; summary-color: 0xFFFFFF; }"
   "Panel@footer { color: 0xFFFFFF; background: 0; }"
   "Panel { summary-color: 0xFFFF00; }"
   "Panel:have-selected { summary-color: 0xFFFF; }"
   "Panel:oper-state { summary-color: 0xFF; }"

   "ButtonWin { color: 0xFFFFFF; background: 0; }"
   "ButtonWin Button { color: 0; background: 0xA0A000; }"
   "EditLine#command-line {color: 0xFFFFFF; background: 0; mark-color: 0; mark-background : 0x800000; }"
   "ToolTip { color: 0; background: 0x80FFFF; }"
   "ToolBar { color: 0; background: 0xA0A000; }"
   "MenuBar { color: 0; background: 0xA0A000; current-item-frame-color:0x00FF00;}"
   "MenuBar:current-item { color: 0xFFFFFF; background: 0; } "
   "PopupMenu@item {color: 0xFFFFFF; background: 0xA0A000; pointer-color:0; line:0; } "
   "PopupMenu@item:current-item { background: 0x404000; } "
   "PopupMenu { background: 0xC0C000; frame-color: 0xFFFFFF; } "

   "#drive-dlg { background: 0xA0A000; }"
   "#drive-dlg * { background: 0xA0A000; color: 0xFFFFFF; }"
   "#drive-dlg * @item { color: 0xFFFFFF; background: 0xA0A000; comment-color: 0xFFFF00; first-char-color:0xFFFF }"
   "#drive-dlg * @item:current-item { color: 0xFFFFFF;  background: 0; comment-color: 0xFFFF00; first-char-color:0xFFFF }"

   "#messagebox-red { color: 0xFFFFFF; background: 0xFF; }"
   "#messagebox-red Static { color: 0xFFFFFF; background: 0xFF; }"
   "HelpWin@style-head { color: 0xFFFFFF; }"
   "HelpWin@style-def  { color: 0xFFFFFF; }"
   "HelpWin@style-red  { color: 0xFF; }"
   "HelpWin@style-bold { color: 0xFFFF; }"
   "HelpWin { background: 0; }"

   "Viewer { background: 0x000000; color:0xB0B0B0; ctrl-color:0x800000; "
   "	makr-color:	0x0; mark-background: 0xFFFFFF; line-color:0x008080; "
   "	hid: 0x600000; load-background:	0x300000; load-color: 0x80;}"
   "Editor { background: 0x000000; color:0xFFFFFF; mark-background: 0xFFFFFF; mark-color: 0; cursor-color: 0xFFFF00; "
   "	DEF:0xE0E0E0; KEYWORD:0x6080F0; COMMENT:0xA08080; STRING:0xFFFF00; PRE:0x00D000; NUM:0xFFFF80; OPER:0x20D0D0; ATTN:0x0000FF; }"
   "StringWin { color: 0xFFFF00; background: 0; }"
   "EditorHeadWin, ViewerHeadWin { color: 0; background: 0xA0A000; prefix-color:0xFFFFFF; cs-color:0xFFFFFF;}"

   "TabDialog { background: 0x606060; color:0xFFFFFF; }"
   "TabDialog * { background: 0x606060; color:0xFFFFFF; button-color: 0x808080; }"
   "TabDialog EditLine { background: 0xD0D0D0; color:0; frame-color: 0x606060; }"
   "TabDialog *@item:dir {color: 0xFFFF00; background: 0; }"
   "TabDialog *@item:current-item	{background: 0x808080; }"
   "TabDialog *@item {color: 0xFFFFFF; background: 0; }"

   "Shortcuts VListWin { background: 0; first-char-color:0xFFFF; }"
   "Shortcuts VListWin@item {color: 0xFFFFFF; background: 0; }"
   "Shortcuts VListWin@item:current-item {color: 0xFFFFFF; background: 0x800000; }"

   ;

//!*************************************************************************************************

static char uiWhiteWcmRules[] =
   "* {color: 0; background: 0xD8E9EC; hotkey-color:0x0000FF; focus-frame-color : 0x00A000; button-color: 0xD8E9EC;  }"
   "*@item:odd		{color: 0; background: 0xE0FFE0; }"
   "*@item:current-item	{color: 0xFFFFFF; background: 0x800000; }"
   "*@item			{color: 0; background: 0xFFFFFF; }"

   "Progress { color:0xA00000; frame-color: 0x808080; background: 0xD8E9EC }"

   "ScrollBar { button-color: 0xD8E9EC;  }"
   "EditLine:!enabled { background: 0xD8E9EC; }"
   "EditLine:focus {color: 0; background: 0xFFFFFF; mark-color: 0xFFFFFF; mark-background : 0x800000; frame-color: 0xD8E9EC }"
   "EditLine {color: 0; background: 0xFFFFFF; frame-color: 0xD8E9EC; mark-color: 0xFFFFFF; mark-background : 0x808080; }"

   "command-line:!enabled { background: 0xD8E9EC; }"
   "command-line:focus {color: 0; background: 0xFFFFFF; mark-color: 0xFFFFFF; mark-background : 0x800000; frame-color: 0xD8E9EC }"
   "command-line {color: 0; background: 0xFFFFFF; frame-color: 0xD8E9EC; mark-color: 0xFFFFFF; mark-background : 0x808080; }"

   "Autocomplete:!enabled { background: 0xFFFFFF; }"
   "Autocomplete:current-item {color: 0xFFFFFF; background: 0x800000; mark-color: 0xFFFFFF; mark-background : 0x800000; frame-color: 0x800000 }"
   "Autocomplete:current-item:odd {color: 0xFFFFFF; background: 0x800000; mark-color: 0xFFFFFF; mark-background : 0x800000; frame-color: 0x800000 }"
   "Autocomplete:odd {color: 0x000000; background: 0xFFFFFF; frame-color: 0xFFFFFF; mark-color: 0xFFFFFF; mark-background : 0x000000; }"
   "Autocomplete {color: 0x000000; background: 0xFFFFFF; frame-color: 0x000000; mark-color: 0xFFFFFF; mark-background : 0x000000; }"

   "Panel { color: 0x000000; background: 0xF8F8F8; }"
   "Panel@item:odd { background: 0xE0E0E0 }"
   "Panel@item:exe { color: 0x306010 }"
   "Panel@item:dir { color: 0 }"
   "Panel@item:hidden { color: 0xA08080 }"
   "Panel@item:hidden:selected-panel:current-item { color: 0x808080 }"
   "Panel@item:bad { color: 0xA0 }"
   "Panel@item:selected { color: 0x0000FF }"
   "Panel@item:selected-panel:current-item { background: 0x800000; color:0xFFFFFF }"
   "Panel@item:selected-panel:current-item:oper-state { background: 0xFF }"
   "Panel ScrollBar, Viewer ScrollBar, Editor ScrollBar {  background: 0xE0E0E0; button-color: 0xFFD080 }"
   "Panel { border-color1: 0x800000; border-color2: 0xD8E9EC; border-color3: 0xD8E9EC; border-color4: 0xD8E9EC;"
   " vline-color1: 0xE0E0E0; vline-color2:0x800000; vline-color3:0xE0E0E0;"
   " line-color:0x800000; summary-color: 0xFFFFFF; }"
   "Panel@footer { color: 0; background: 0xD8E9EC; }"
   "Panel@header:selected-panel	{ color: 0; background: 0xFFD0A0; }"
   "Panel@header 			{ color: 0; background: 0xD8E9EC; }"
   "Panel { summary-color: 0x800000; }"
   "Panel:have-selected { summary-color: 0xFF; }"
   "Panel:oper-state { summary-color: 0x8080; }"

   "ButtonWin { color: 0; background: 0xD8E9EC; }"
   "ButtonWin Button { color: 0; background: 0xD8E9EC; }"
   "EditLine#command-line {color: 0; background: 0xFFFFFF; mark-color: 0xFFFFFF; mark-background : 0x800000; }"
   "ToolTip { color: 0; background: 0x80FFFF; }"
   "ToolBar { color: 0; background: 0xD8E9EC; }"
   "MenuBar { color: 0; background: 0xD8E9EC; current-item-frame-color:0x00A000; }"
   "MenuBar:current-item { color: 0xFFFFFF; background: 0;  } "

   "PopupMenu@item {color: 0; background: 0xFFFFFF; pointer-color:0; line:0; } "
   "PopupMenu@item:current-item { background: 0xB0C0C0; } "
   "PopupMenu { background: 0xF0F0F0; frame-color: 0x808080; } "

   "#drive-dlg { background: 0xD8E9EC; }"
   "#drive-dlg * { background: 0xD8E9EC; color: 0; first-char-color:0xFF; }"
   "#drive-dlg * @item { color: 0; background: 0xD8E9EC; comment-color: 0x808000;  }"
   "#drive-dlg * @item:current-item { color: 0xFFFFFF;  background: 0; comment-color: 0xFFFF00; }"

   "#messagebox-red { color: 0xFFFFFF; background: 0xFF; }"
   "#messagebox-red Static { color: 0xFFFFFF; background: 0xFF; }"
   "HelpWin@style-head { color: 0; }"
   "HelpWin@style-def  { color: 0; }"
   "HelpWin@style-red  { color: 0xFF; }"
   "HelpWin@style-bold { color: 0x8080; }"
   "HelpWin { background:  0xF8F8F8; }"

   "Viewer { background:0xF8F8F8 ; color:0x000000; ctrl-color:0xFF0000; "
   "	makr-color:	0x0; mark-background: 0xFFFFFF; line-color:0x800000; "
   "	hid: 0xD0D0D0; load-background:	0xD00000; load-color: 0xFF;}"
   "Editor { background:0xF8F8F8 ; color:0x000000; mark-background: 0x800000; mark-color: 0xFFFFFF; cursor-color: 0xFF0000; "
   "	DEF:0; KEYWORD:0xFF0000; COMMENT:0xA0A0A0; STRING:0x808000; PRE:0x008000; NUM:0x000080; OPER:0x000080; ATTN:0x0000FF; }"
   "StringWin { color: 0; background: 0xD8E9EC; }"
   "EditorHeadWin, ViewerHeadWin { color: 0; background: 0xD8E9EC; prefix-color:0x808000; cs-color:0x0000FF;}"

   "TabDialog { background: 0x606060; color:0xFFFFFF; }"
   "TabDialog * { background: 0x606060; color:0xFFFFFF; button-color: 0x808080; }"
   "TabDialog EditLine { background: 0xD0D0D0; color:0; frame-color: 0x606060; }"
   "TabDialog *@item:dir {color: 0xFFFF00; background: 0; }"
   "TabDialog *@item:current-item	{background: 0x808080; }"
   "TabDialog *@item {color: 0xFFFFFF; background: 0; }"

   "Shortcuts VListWin { background: 0xFFFFFF; first-char-color:0xFF; }"
   "Shortcuts VListWin@item {color: 0; background: 0xFFFFFF; }"
   "Shortcuts VListWin@item:current-item {color: 0xFFFFFF; background: 0x800000; }"

   ;


void SetColorStyle( int style )
{
	try
	{
		switch ( style )
		{
			case 1:
				UiReadMem( uiBlackWcmRules );
				break;

			case 2:
				UiReadMem( uiWhiteWcmRules );
				break;

			default:
				UiReadMem( uiDefaultWcmRules );
		}
	}
	catch ( cexception* ex )
	{
		fprintf( stderr, "error:%s\n", ex->message() );
		ex->destroy();
	}
}
