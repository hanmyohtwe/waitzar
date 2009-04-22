/*
 * Copyright 2007 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

//Defines for Unicode-enabled text.
//  As far as I know, these must appear _before_ including the windows.h include
//  so that we get the proper Unicode-enabled source.
#define _UNICODE
#define UNICODE

//Don't let Visual Studio warn us to use the _s functions
#define _CRT_SECURE_NO_WARNINGS

//Ironic that adding compliance now causes headaches compiling under VS2003
#define _CRT_NON_CONFORMING_SWPRINTFS

//Define to require a specific version of Windows.
#define _WIN32_WINNT 0x0500 //Run on Windows 2000, XP, and Vista (haven't tested NT or the "server"s yet)
#define _WIN32_IE 0x0500    //I don't understand why I need this, but the speech balloons won't compile unless I have it.
//#define _WIN32_WINNT 0x0410 //Run on Windows 98+, fails for KEYBOARD_INPUT

//Slim-down our list of definitions. Would you believe that this causes NO
//  noticeable size reduction on Windows XP, VS2003? Perhaps it helps
//  on Vista....
//Anyway, if you add a new function and get an "undefined" error, comment
//  the relevant #define out.
#define NOGDICAPMASKS       //- CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOKEYSTATES         //- MK_*
#define NOSYSCOMMANDS       //- SC_*
#define OEMRESOURCE         //- OEM Resource values
#define NOATOM              //- Atom Manager routines
#define NOCLIPBOARD         //- Clipboard routines
#define NOCOLOR             //- Screen colors
#define NODRAWTEXT          //- DrawText() and DT_*
#define NOKERNEL            //- All KERNEL defines and routines
#define NOMEMMGR            //- GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE          //- typedef METAFILEPICT
#define NOOPENFILE          //- OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL            //- SB_and scrolling routines
#define NOSERVICE           //- All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND             //- Sound driver routines
//#define NOTEXTMETRIC        //- typedef TEXTMETRIC and associated routines
#define NOWH                //- SetWindowsHook and WH_*
#define NOWINOFFSETS        //- GWL_*, GCL_*, associated routines
#define NOCOMM              //- COMM driver routines
#define NOKANJI             //- Kanji support stuff.
#define NOHELP              //- Help engine interface.
#define NOPROFILER          //- Profiler interface.
#define NODEFERWINDOWPOS    //- DeferWindowPos routines
#define NOMCX               //- Modem Configuration Extensions

//System includes
#include <windows.h>
#include <windowsx.h> //For GET_X_LPARAM
#include <psapi.h> //For getting a list of currently running processes
//#include <wingdi.h> //For the TEXTINFO stuff
#include <stdio.h>
#include <tchar.h>
#include <string>
#include <list>

//Our includes
#include "../cross_platform_library/waitzar/WordBuilder.h"
#include "../cross_platform_library/waitzar/SentenceList.h"
#include "../cross_platform_library/waitzar/wz_utilities.h"
#include "PulpCoreFont.h"
#include "OnscreenKeyboard.h"
#include "resource.h"
#include "Hotkeys.h"

using namespace waitzar;

//TEMP:
bool TEMP_FLAG = false;

//Current version
#define WAIT_ZAR_VERSION _T("1.6")

//Menu item texts
TCHAR* POPUP_UNI = _T("Unicode 5.1");
TCHAR* POPUP_ZG = _T("Zawgyi-One");
TCHAR* POPUP_WIN = _T("Win Innwa");
TCHAR* POPUP_LOOKUP = _T("&Look Up Word (F1)");

//Prototypes
BOOL turnOnHotkeys(BOOL on, bool affectLowercase, bool affectUppercase);
BOOL turnOnControlkeys(BOOL on);
BOOL turnOnNumberkeys(BOOL on);
BOOL turnOnPunctuationkeys(BOOL on);
bool turnOnExtendedKeys(bool on); //"Help keys"
bool turnOnHelpKeys(bool on); //Reduced to contain only the shift key
void switchToLanguage(BOOL toMM);
BOOL loadModel(HINSTANCE hInst);

//Better support for dragging
bool isDragging;
POINT dragFrom;

//Unique IDs
#define LANG_HOTKEY 142
#define STATUS_NID 144

//Custom message IDs
#define UWM_SYSTRAY    (WM_USER + 1)
#define UWM_HOTKEY_UP  (WM_USER + 2)

//Grr... notepad...
#define UNICOD_BOM 0xFEFF
#define BACKWARDS_BOM 0xFFFE

//Font conversion
int cachedEncoding;
TCHAR currEncStr[10];

//Brushes & Pens
HBRUSH g_WhiteBkgrd;
HBRUSH g_DarkGrayBkgrd;
HBRUSH g_YellowBkgrd;
HBRUSH g_GreenBkgrd;
HPEN g_GreenPen;
HPEN g_BlackPen;
HPEN g_EmptyPen;

//Global Variables
HINSTANCE hInst;
HICON mmIcon;
HICON engIcon;
WordBuilder *model;
PulpCoreFont *mmFontBlack;
PulpCoreFont *mmFontGreen;
PulpCoreFont *mmFontSmallWhite;
PulpCoreFont *mmFontSmallGray;
PAINTSTRUCT Ps;
WORD stopChar;
int numConfigOptions;
int numCustomWords;
INPUT *inputItems;
KEYBDINPUT keyInputPrototype;
bool helpIsCached;
wchar_t returnVal[500];

//Help Window resources
PulpCoreFont *helpFntKeys;
PulpCoreFont *helpFntFore;
PulpCoreFont *helpFntBack;
PulpCoreImage *helpCornerImg;
OnscreenKeyboard *helpKeyboard;
BLENDFUNCTION BLEND_FULL = { AC_SRC_OVER, 0, 0xFF, AC_SRC_ALPHA }; //NOTE: This requires premultiplied pixel values
POINT PT_ORIGIN;
HANDLE keyTrackThread;   //Handle to our thread
DWORD  keyTrackThreadID; //Its unique ID (never zero)
CRITICAL_SECTION threadCriticalSec; //Global critical section object
std::list<unsigned int> hotkeysDown; //If a wparam is in this list, it is being tracked
bool threadIsActive; //If "false", this thread must be woken to do anything useful
std::vector<wchar_t*> userDefinedWords; //Words the user types in. Stored with a negative +1 index
std::vector<wchar_t*> userDefinedWordsZg; //Cache of the Zawgyi version of the word typed
std::vector<unsigned short> userKeystrokeVector;
const char* systemDefinedWords = "`~!@#$%^&*()-_=+[{]}\\|;:'\"<>/? 1234567890"; //Special "words" used in our keyboard, like "(" and "`"
std::vector< std::pair <int, unsigned short>* > systemWordLookup;

//Special resources for tracking the caret
HANDLE caretTrackThread;
DWORD caretTrackThreadID;
POINT caretLatestPosition;

//Help window colors
#define COLOR_HELPFNT_KEYS        0x606060
#define COLOR_HELPFNT_FORE        0x000000
#define COLOR_HELPFNT_BACK        0x0019FF

//Configuration variables.
BOOL customDictWarning = FALSE;
TCHAR langHotkeyString[100];
char langHotkeyRaw[100];
BOOL typePhrases = TRUE;
BOOL dragBothWindowsTogether = TRUE;
BOOL typeBurmeseNumbers = TRUE;
BOOL showBalloonOnStart = TRUE;
BOOL alwaysRunElevated = FALSE;
BOOL highlightKeys = TRUE;
BOOL experimentalTextCursorTracking = TRUE;
BOOL dontLoadModel = FALSE;
BOOL allowNonBurmeseLetters = FALSE;
unsigned int maxDictionaryEntries = 0;
unsigned int maxNexusEntries = 0;
unsigned int maxPrefixEntries = 0;
char fontFileRegular[512];
char fontFileSmall[512];

//Double-buffering stuff - mainWindow
HWND mainWindow;
HDC mainDC;
HDC mainUnderDC;
HBITMAP mainBitmap;
BOOL mainWindowSkipMove = FALSE;

//Double-buffering stuff - secondaryWindow
HWND senWindow;
HDC senDC;
HDC senUnderDC;
HBITMAP senBitmap;
BOOL senWindowSkipMove = FALSE;

//Double-buffering stuff, tertiary window
HWND helpWindow;
HDC helpDC;
HDC helpUnderDC;
HBITMAP helpBitmap;

//Init properly
bool mainInitDone;
bool helpInitDone;

//Record-keeping
TCHAR currStr[100];
TCHAR currStrZg[100];
int currStrDictID;
TCHAR currLetterSt[100];
BOOL mmOn;
BOOL controlKeysOn = FALSE;
BOOL numberKeysOn = FALSE;
BOOL punctuationKeysOn = FALSE;
bool extendedKeysOn = false;
bool helpKeysOn = false;
SentenceList *sentence;
int prevProcessID;

//Default client sizes for our windows
int WINDOW_WIDTH = 240;
int WINDOW_HEIGHT = 120;
int SUB_WINDOW_WIDTH = 300;
int SUB_WINDOW_HEIGHT = 26;
int HELP_WINDOW_WIDTH = 200;
int HELP_WINDOW_HEIGHT = 200;

//Width/height of client area
int C_WIDTH;
int C_HEIGHT;
int SUB_C_WIDTH;
int SUB_C_HEIGHT;

//Try it differently for the help menu
SIZE HELP_CLIENT_SIZE;

//Calculate's integers
int firstLineStart;
int secondLineStart;
int thirdLineStart;
int fourthLineStart;
int borderWidth = 2;
int spaceWidth;

//Avoid crashing if explorer is running slowly
bool mainWindowIsVisible;
bool subWindowIsVisible;
bool helpWindowIsVisible;

//Log file, since the debugger doesn't like multi-process threads
bool isLogging = false;
FILE *logFile;



/** 
 * This thread is our locus-of-control for carets
 * It should probably always be run synchronously, to prevent another window from 
 *   grabbing focus while it's waiting.
 */
DWORD WINAPI UpdateCaretPosition(LPVOID args)
{
	//Loop forever
	//for (;;) {
		/*if (isLogging)
			fprintf(logFile, "Update caret called.\n");*/

		HWND foreWnd = GetForegroundWindow();
		if (IsWindowVisible(foreWnd)==TRUE) {
			DWORD foreID = GetWindowThreadProcessId(foreWnd, NULL);
			if (AttachThreadInput(caretTrackThreadID, foreID, TRUE)) {
				HWND focusWnd = GetFocus();
				HWND activeWnd = GetActiveWindow();

				//Debug
				/*TCHAR wndTitle[200];
				WINDOWINFO wndInfo;
				GetWindowText(activeWnd, wndTitle, 200);
				GetWindowInfo(activeWnd, &wndInfo);
				if (isLogging)
					fprintf(logFile, "  Active Window: %S (%i,%i)\n", wndTitle, wndInfo.rcWindow.right, wndInfo.rcWindow.bottom);*/

				if (IsWindowVisible(focusWnd)) {
					POINT mousePos;
					RECT clientUL;
					if (GetCaretPos(&mousePos) && GetWindowRect(focusWnd, &clientUL)!=0) {
						caretLatestPosition.x = clientUL.left + mousePos.x;
						caretLatestPosition.y = clientUL.top + mousePos.y;

						int caretHeight = SUB_WINDOW_HEIGHT;
						TEXTMETRICW tm;
						HFONT currFont = (HFONT)SendMessage(focusWnd, WM_GETFONT, 0, 0);
						if (GetTextMetrics(mainDC, &tm)!=0)
							caretHeight = tm.tmHeight;

						//We actually want the window slightly below this...
						caretLatestPosition.x -= 1;
						caretLatestPosition.y -= WINDOW_HEIGHT;
						caretLatestPosition.y -= (SUB_WINDOW_HEIGHT-caretHeight)/2;

						//fprintf(logFile, "  x and y: (%i,%i)\n", caretLatestPosition.x, caretLatestPosition.y);
					}

					//We might have accidentally gained focus:
					//SetForegroundWindow(foreWnd);
					//SetActiveWindow(activeWnd);
					//SetFocus(focusWnd);
				}

				//Finally
				AttachThreadInput(caretTrackThreadID, foreID, FALSE);
			}
		}

		//Sleep until woken
		//SuspendThread(caretTrackThread);
	//}
		return 0;
}



/**
 * This is our threaded locus-of-control, which is woken when a keypress is detected, and
 *   put to sleep when all keys have been released. It is very important that this
 *   thread run very fast; we operate on the assumption that it completes in less
 *   than 1 Thread cycle, including synchronization.
 * This thread is not activated if the "highlight keys" flag is off. This is intended to
 *   allow increased performance on slow systems, decreased annoyance for advanced users,
 *   and an ultimate fall-back if the thread is shown to deadlock or stall with continuous usage.
 * @args = always null.
 * @returns = 0 for success (never really returns)
 */
DWORD WINAPI TrackHotkeyReleases(LPVOID args)
{
	//Loop forever
	for (;;) {
		//Check every key. Since we generally are tracking a very small number of keys, it makes
		//  sense to put this into its own critical section

		//CRITICAL SECTION
		{
			EnterCriticalSection(&threadCriticalSec);

			/*if (isLogging)
				fprintf(logFile, "Thread:\n");*/

			//Loop through our list
			for (std::list<unsigned int>::iterator keyItr = hotkeysDown.begin(); keyItr != hotkeysDown.end();) {
				//Get the state of this key
				SHORT keyState = GetKeyState(helpKeyboard->getVirtualKeyID(*keyItr)); //We need to use CAPITAL letters for virtual keys. Gah!

				if ((keyState & 0x8000)==0) {
					//Send a hotkey_up event to our window (mimic the wparam used by WM_HOTKEY)
					/*if (isLogging)
						fprintf(logFile, "  Key up: %c  (%x)\n", *keyItr, keyState);*/

					if (PostMessage(mainWindow, UWM_HOTKEY_UP, *keyItr, 0)==0) {
						//SendMessage(mainWindow, UWM_HOTKEY_UP, *keyItr,0); //Send message seems to make no difference
						MessageBox(NULL, _T("Couldn't post message to Main Window"), _T("Error"), MB_OK);
					}

					//Key has been released, stop tracking it
					keyItr = hotkeysDown.erase(keyItr); //Returns the next valid value
				} else {
					//Normal iteration
					keyItr++;
				}
			}

			//Sleep, but for how long?
			if (hotkeysDown.empty()) {
				/*if (isLogging)
					fprintf(logFile, "  Sleep: forever\n");*/

				//Sleep until woken up
				threadIsActive = false;
				LeaveCriticalSection(&threadCriticalSec);
				SuspendThread(keyTrackThread);
			} else {
				/*if (isLogging)
					fprintf(logFile, "  Sleep: 10ms\n");*/

				//Sleep for 10ms, and continue tracking keyboard input
				LeaveCriticalSection(&threadCriticalSec);
				Sleep(10);
			}
		}
	}

	return 0;
}


void buildSystemWordLookup()
{
	for (size_t i=0; i<strlen(systemDefinedWords); i++) {
		char c = systemDefinedWords[i];
		int hotkey_id = 0;
		switch (c) {
			case '`':
				hotkey_id = HOTKEY_COMBINE;
				break;
			case '~':
				hotkey_id = HOTKEY_SHIFT_COMBINE;
				break;
			case '!':
				hotkey_id = HOTKEY_SHIFT_1;
				break;
			case '@':
				hotkey_id = HOTKEY_SHIFT_2;
				break;
			case '#':
				hotkey_id = HOTKEY_SHIFT_3;
				break;
			case '$':
				hotkey_id = HOTKEY_SHIFT_4;
				break;
			case '%':
				hotkey_id = HOTKEY_SHIFT_5;
				break;
			case '^':
				hotkey_id = HOTKEY_SHIFT_6;
				break;
			case '&':
				hotkey_id = HOTKEY_SHIFT_7;
				break;
			case '*':
				hotkey_id = HOTKEY_SHIFT_8;
				break;
			case '(':
				hotkey_id = HOTKEY_SHIFT_9;
				break;
			case ')':
				hotkey_id = HOTKEY_SHIFT_0;
				break;
			case '-':
				hotkey_id = HOTKEY_MINUS;
				break;
			case '_':
				hotkey_id = HOTKEY_SHIFT_MINUS;
				break;
			case '=':
				hotkey_id = HOTKEY_EQUALS;
				break;
			case '+':
				hotkey_id = HOTKEY_SHIFT_EQUALS;
				break;
			case '[':
				hotkey_id = HOTKEY_LEFT_BRACKET;
				break;
			case '{':
				hotkey_id = HOTKEY_SHIFT_LEFT_BRACKET;
				break;
			case ']':
				hotkey_id = HOTKEY_RIGHT_BRACKET;
				break;
			case '}':
				hotkey_id = HOTKEY_SHIFT_RIGHT_BRACKET;
				break;
			case ';':
				hotkey_id = HOTKEY_SEMICOLON;
				break;
			case ':':
				hotkey_id = HOTKEY_SHIFT_SEMICOLON;
				break;
			case '\'':
				hotkey_id = HOTKEY_APOSTROPHE;
				break;
			case '"':
				hotkey_id = HOTKEY_SHIFT_APOSTROPHE;
				break;
			case '\\':
				hotkey_id = HOTKEY_BACKSLASH;
				break;
			case '|':
				hotkey_id = HOTKEY_SHIFT_BACKSLASH;
				break;
			case '<':
				hotkey_id = HOTKEY_SHIFT_COMMA;
				break;
			case '>':
				hotkey_id = HOTKEY_SHIFT_PERIOD;
				break;
			case '/':
				hotkey_id = HOTKEY_FORWARDSLASH;
				break;
			case '?':
				hotkey_id = HOTKEY_SHIFT_FORWARDSLASH;
				break;
			case ' ':
				hotkey_id = HOTKEY_SHIFT_SPACE;
				break;
			case '1':
				hotkey_id = HOTKEY_1;
				break;
			case '2':
				hotkey_id = HOTKEY_2;
				break;
			case '3':
				hotkey_id = HOTKEY_3;
				break;
			case '4':
				hotkey_id = HOTKEY_4;
				break;
			case '5':
				hotkey_id = HOTKEY_5;
				break;
			case '6':
				hotkey_id = HOTKEY_6;
				break;
			case '7':
				hotkey_id = HOTKEY_7;
				break;
			case '8':
				hotkey_id = HOTKEY_8;
				break;
			case '9':
				hotkey_id = HOTKEY_9;
				break;
			case '0':
				hotkey_id = HOTKEY_0;
				break;
		}

		systemWordLookup.push_back(new std::pair<int, unsigned short>(hotkey_id, i));
	}
}



//myWin 2.1 rules for stacking
bool canStack(wchar_t letter) 
{
	return (letter>=0x1000 && letter<=0x1003)
		|| (letter>=0x1005 && letter<=0x1021);
}






/**
 * Create our inner-used Zawgyi-One fonts.
 */
void makeFont(HWND currHwnd)
{
	//Load our font resource (main fonts)
	{
		//Try to load our user-specified font image.
		if (strlen(fontFileRegular)>0) {
			size_t fLen = strlen(fontFileRegular);
			bool validFont = true;

			//Is the file a PNG file by name?
			if (fLen<5 || fontFileRegular[fLen-4]!='.' || fontFileRegular[fLen-3]!='p' || fontFileRegular[fLen-2]!='n' || fontFileRegular[fLen-1]!='g') {
				validFont = false;
			} else {
				//Does the file exist?
				FILE* fontFile = fopen(fontFileRegular, "rb");
				if (fontFile == NULL)
					validFont = false;
				else {
					//Try to read its data into a char[] array; get the size, too
					fseek (fontFile, 0, SEEK_END);
					long fileSize = ftell(fontFile);
					rewind(fontFile);
					char * file_buff = new char[fileSize];
					size_t file_buff_size = fread(file_buff, 1, fileSize, fontFile);
					fclose(fontFile);

					if (file_buff_size!=fileSize)
						validFont = false;
					else {
						//Ok, load our font
						mmFontBlack = new PulpCoreFont();
						mmFontBlack->init(file_buff, file_buff_size, mainDC);

						//Is our font in error? If so, load the embedded font
						if (mmFontBlack->isInError()==TRUE) {
							TCHAR errorStr[600];
							swprintf(errorStr, _T("Custom font didn't load correctly: %s"), mmFontBlack->getErrorMsg());
							MessageBox(NULL, errorStr, _T("Error"), MB_ICONERROR | MB_OK);

							validFont = false;
							delete mmFontBlack;
						}
					}
				}
			}

			//Did we make it?
			if (!validFont) {
				strcpy(fontFileRegular, "");
			}
		}


		//Do we need to load the internal font?
		if (strlen(fontFileRegular)==0) {
			HRSRC fontRes = FindResource(hInst, MAKEINTRESOURCE(WZ_FONT), _T("COREFONT"));
			if (!fontRes) {
				MessageBox(NULL, _T("Couldn't find WZ_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
				return;
			}

			//Get a handle from this resource.
			HGLOBAL res_handle = LoadResource(NULL, fontRes);
			if (!res_handle) {
				MessageBox(NULL, _T("Couldn't get a handle on WZ_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
				return;
			}

			//Create our PulpCoreFont (it's white when we load it, not black, by the way)
			mmFontBlack = new PulpCoreFont();
			mmFontBlack->init(fontRes, res_handle, mainDC);

			//Unlock this resource for later use.
			UnlockResource(res_handle);
		}

		//Is our embedded font in error?
		if (mmFontBlack->isInError()==TRUE) {
			TCHAR errorStr[600];
			swprintf(errorStr, _T("WZ Font didn't load correctly: %s"), mmFontBlack->getErrorMsg());

			MessageBox(NULL, errorStr, _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Copy-construct a new font
		mmFontGreen = new PulpCoreFont();
		mmFontGreen->init(mmFontBlack, mainDC);

		//Tint both to their respective colors
		mmFontGreen->tintSelf(0x008000);
		mmFontBlack->tintSelf(0x000000);
	}


	//Load our help window font: Keys
	{
		//First the resource
		HRSRC fontRes = FindResource(hInst, MAKEINTRESOURCE(WZ_HELP_KEY_FONT), _T("COREFONT"));
		if (!fontRes) {
			MessageBox(NULL, _T("Couldn't find WZ_HELP_KEY_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Get a handle from this resource.
		HGLOBAL res_handle = LoadResource(NULL, fontRes);
		if (!res_handle) {
			MessageBox(NULL, _T("Couldn't get a handle on WZ_HELP_KEY_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		helpFntKeys = new PulpCoreFont();
		helpFntKeys->init(fontRes, res_handle, helpDC);
		if (helpFntKeys->isInError()==TRUE) {
			TCHAR errorStr[600];
			swprintf(errorStr, _T("WZ Help Font (keys) didn't load correctly: %s"), helpFntKeys->getErrorMsg());

			MessageBox(NULL, errorStr, _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Tint to default
		helpFntKeys->tintSelf(COLOR_HELPFNT_KEYS);

		//Unlock this resource for later use.
		UnlockResource(res_handle);
	}

	//Load our help window font: Foreground
	{
		//First the resource
		HRSRC fontRes = FindResource(hInst, MAKEINTRESOURCE(WZ_HELP_FORE_FONT), _T("COREFONT"));
		if (!fontRes) {
			MessageBox(NULL, _T("Couldn't find WZ_HELP_FORE_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Get a handle from this resource.
		HGLOBAL res_handle = LoadResource(NULL, fontRes);
		if (!res_handle) {
			MessageBox(NULL, _T("Couldn't get a handle on WZ_HELP_FORE_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		helpFntFore = new PulpCoreFont();
		helpFntFore->init(fontRes, res_handle, helpDC);
		if (helpFntFore->isInError()==TRUE) {
			TCHAR errorStr[600];
			swprintf(errorStr, _T("WZ Help Font (foreground) didn't load correctly: %s"), helpFntFore->getErrorMsg());

			MessageBox(NULL, errorStr, _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Tint to default
		helpFntFore->tintSelf(COLOR_HELPFNT_FORE);

		//Unlock this resource for later use.
		UnlockResource(res_handle);
	}

	//Load our help window font: Background
	{
		//First the resource
		HRSRC fontRes = FindResource(hInst, MAKEINTRESOURCE(WZ_HELP_BACK_FONT), _T("COREFONT"));
		if (!fontRes) {
			MessageBox(NULL, _T("Couldn't find WZ_HELP_BACK_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Get a handle from this resource.
		HGLOBAL res_handle = LoadResource(NULL, fontRes);
		if (!res_handle) {
			MessageBox(NULL, _T("Couldn't get a handle on WZ_HELP_BACK_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		helpFntBack = new PulpCoreFont();
		helpFntBack->init(fontRes, res_handle, helpDC);
		if (helpFntBack->isInError()==TRUE) {
			TCHAR errorStr[600];
			swprintf(errorStr, _T("WZ Help Font (background) didn't load correctly: %s"), helpFntBack->getErrorMsg());

			MessageBox(NULL, errorStr, _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Tint to default
		helpFntBack->tintSelf(COLOR_HELPFNT_BACK);

		//Unlock this resource for later use.
		UnlockResource(res_handle);
	}


	//Load our help menu's corner image (used for keyboard keys)
	{
		//First the resource
		HRSRC imgRes = FindResource(hInst, MAKEINTRESOURCE(WZ_HELP_CORNER), _T("COREFONT"));
		if (!imgRes) {
			MessageBox(NULL, _T("Couldn't find WZ_HELP_CORNER"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Get a handle from this resource.
		HGLOBAL res_handle = LoadResource(NULL, imgRes);
		if (!res_handle) {
			MessageBox(NULL, _T("Couldn't get a handle on WZ_HELP_CORNER"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		helpCornerImg = new PulpCoreImage();
		helpCornerImg->init(imgRes, res_handle, helpDC);
		if (helpCornerImg->isInError()==TRUE) {
			TCHAR errorStr[600];
			swprintf(errorStr, _T("WZ Corner Image File didn't load correctly: %s"), helpCornerImg->getErrorMsg());

			MessageBox(NULL, errorStr, _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Unlock this resource for later use.
		UnlockResource(res_handle);
	}


	//Save resources if we don't use the second window
	if (typePhrases==FALSE)
		return;


	//Try to load our user-specified font image.
	if (strlen(fontFileSmall)>0) {
		size_t fLen = strlen(fontFileSmall);
		bool validFont = true;

		//Is the file a PNG file by name?
		if (fLen<5 || fontFileSmall[fLen-4]!='.' || fontFileSmall[fLen-3]!='p' || fontFileSmall[fLen-2]!='n' || fontFileSmall[fLen-1]!='g') {
			validFont = false;
		} else {
			//Does the file exist?
			FILE* fontFile = fopen(fontFileSmall, "rb");
			if (fontFile == NULL)
				validFont = false;
			else {
				//Try to read its data into a char[] array; get the size, too
				fseek (fontFile, 0, SEEK_END);
				long fileSize = ftell(fontFile);
				rewind(fontFile);
				char * file_buff = new char[fileSize];
				size_t file_buff_size = fread(file_buff, 1, fileSize, fontFile);
				fclose(fontFile);

				if (file_buff_size!=fileSize)
					validFont = false;
				else {
					//Ok, load our font
					mmFontSmallWhite = new PulpCoreFont();
					mmFontSmallWhite->init(file_buff, file_buff_size, senDC);

					//Is our font in error? If so, load the embedded font
					if (mmFontSmallWhite->isInError()==TRUE) {
						TCHAR errorStr[600];
						swprintf(errorStr, _T("Custom (small) font didn't load correctly: %s"), mmFontSmallWhite->getErrorMsg());
						MessageBox(NULL, errorStr, _T("Error"), MB_ICONERROR | MB_OK);

						validFont = false;
						delete mmFontSmallWhite;
					}
				}
			}
		}

		//Did we make it?
		if (!validFont) {
			strcpy(fontFileSmall, "");
		}
	}


	//Do we need to load the embedded font as backup?
	if (strlen(fontFileSmall)==0) {
		//Now, our small font (resource first!)
		HRSRC fontRes2 = FindResource(hInst, MAKEINTRESOURCE(WZ_SMALL_FONT), _T("COREFONT"));
		if (!fontRes2) {
			MessageBox(NULL, _T("Couldn't find WZ_SMALL_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		//Get a handle from this resource.
		HGLOBAL res_handle_2 = LoadResource(NULL, fontRes2);
		if (!res_handle_2) {
			MessageBox(NULL, _T("Couldn't get a handle on WZ_SMALL_FONT"), _T("Error"), MB_ICONERROR | MB_OK);
			return;
		}

		mmFontSmallWhite = new PulpCoreFont();
		mmFontSmallWhite->init(fontRes2, res_handle_2, senDC);

		//Unlock this resource for later use.
		UnlockResource(res_handle_2);
	}


	//Is our embedded font in error?
	if (mmFontSmallWhite->isInError()==TRUE) {
		TCHAR errorStr[600];
		swprintf(errorStr, _T("WZ Small Font didn't load correctly: %s"), mmFontSmallWhite->getErrorMsg());

		MessageBox(NULL, errorStr, _T("Error"), MB_ICONERROR | MB_OK);
		return;
	}



	//Tint
	mmFontSmallWhite->tintSelf(0xFFFFFF);

	//New copy
	mmFontSmallGray = new PulpCoreFont();
	mmFontSmallGray->init(mmFontSmallWhite, mainDC);
	mmFontSmallGray->tintSelf(0x333333);
	
}




BOOL waitzarAlreadyStarted()
{
	//Get all processes
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
        return FALSE;
	cProcesses = cbNeeded / sizeof(DWORD);

	//Check for "WaitZar"
	TCHAR szProcessName[MAX_PATH];
	for (unsigned int i=0; i<cProcesses; i++ ) {
		if( aProcesses[i] != 0 ) {
			//Open a handle to this process, get its name
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, FALSE, aProcesses[i]);
			if (hProcess!=NULL) {
				HMODULE hMod;
				DWORD cbNeeded;
				lstrcpy(szProcessName, _T("<unknown>"));
				if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
					GetModuleBaseName(hProcess, hMod, szProcessName, sizeof(szProcessName)/sizeof(TCHAR));
					if (lstrcmp(szProcessName, _T("WaitZar.exe"))==0)
						return TRUE;
				}
			}
        }
	}

	return FALSE;
}



void readUserWords() {
	//Read our words file, if it exists.
	numCustomWords = -1;
	FILE* userFile = fopen("mywords.txt", "r");
	if (userFile != NULL) {
		//Get file size
		fseek (userFile, 0, SEEK_END);
		long fileSize = ftell(userFile);
		rewind(userFile);

		//Read it all into an array, close the file.
		char * buffer = (char*) malloc(sizeof(char)*fileSize);
		size_t buff_size = fread(buffer, 1, fileSize, userFile);
		fclose(userFile);

		numCustomWords = 0;
		if (buff_size==0) {
			return; //Empty file.
		}

		//Finally, convert this array to unicode
		TCHAR * uniBuffer;
		size_t numUniChars = MultiByteToWideChar(CP_UTF8, 0, buffer, (int)buff_size, NULL, 0);
		uniBuffer = (TCHAR*) malloc(sizeof(TCHAR)*numUniChars);
		if (!MultiByteToWideChar(CP_UTF8, 0, buffer, (int)buff_size, uniBuffer, (int)numUniChars)) {
			MessageBox(NULL, _T("mywords.txt contains invalid UTF-8 characters.\n\nWait Zar will still function properly; however, your custom dictionary will be ignored."), _T("Warning"), MB_ICONWARNING | MB_OK);
			return;
		}
		delete [] buffer;

		//Skip the BOM, if it exists
		size_t currPosition = 0;
		if (uniBuffer[currPosition] == UNICOD_BOM)
			currPosition++;
		else if (uniBuffer[currPosition] == BACKWARDS_BOM) {
			MessageBox(NULL, _T("mywords.txt appears to be backwards. You should fix the Unicode encoding using Notepad or another Windows-based text utility.\n\nWait Zar will still function properly; however, your custom dictionary will be ignored."), _T("Warning"), MB_ICONWARNING | MB_OK);
			return;
		}

		//Read each line
		TCHAR* name = new TCHAR[100];
		char* value = new char[100];
		while (currPosition<numUniChars) {
			//Get the name/value pair using our nifty template function....
			readLine(uniBuffer, currPosition, numUniChars, true, true, false, (allowNonBurmeseLetters==TRUE), true, false, false, false, name, value);

			//Make sure both name and value are non-empty
			if (strlen(value)==0 || lstrlen(name)==0)
				continue;

			//Add this romanization
			if (!model->addRomanization(name, value)) {
				MessageBox(NULL, model->getLastError(), _T("Error adding Romanisation"), MB_ICONERROR | MB_OK);
			}
			numCustomWords++;
		}
		delete [] uniBuffer;
		delete [] name;
		delete [] value;


		if (numCustomWords>0 && customDictWarning==TRUE)
			MessageBox(NULL, _T("Warning! You are using a custom dictionary: \"mywords.txt\".\nThis feature of Wait Zar is EXPERIMENTAL; WaitZar.exe may crash.\n(You may disable this warning by setting mywordswarning = no in config.txt).\n\nPlease report any crashes at the issues page: \nhttp://code.google.com/p/waitzar/issues/list\n\nPress \"Ok\" to continue using Wait Zar."), _T("Warning..."), MB_ICONWARNING | MB_OK);

	}
}


void setEncoding(UINT encoding)
{
	if (encoding==ENCODING_WININNWA)
		lstrcpy(currEncStr, _T("WI"));
	else if (encoding==ENCODING_ZAWGYI)
		lstrcpy(currEncStr, _T("ZG"));
	else if (encoding==ENCODING_UNICODE)
		lstrcpy(currEncStr, _T("UNI"));

	if (model==NULL)
		cachedEncoding = encoding;
	else {
		model->setOutputEncoding(encoding);
		cachedEncoding = -1;
	}
}


void loadConfigOptions()
{
	//Default keys
	lstrcpy(langHotkeyString, _T("Ctrl+Shift"));
	strcpy(langHotkeyRaw, "^+");

	//Default encoding
	setEncoding(ENCODING_UNICODE);

	//Default font files
	strcpy(fontFileRegular, "");
	strcpy(fontFileSmall, "");

	//Read our config file, if it exists.
	numConfigOptions = -1;
	FILE* configFile = fopen("config.txt", "r");
	if (configFile == NULL)
		return;

	//Get file size
	fseek (configFile, 0, SEEK_END);
	long fileSize = ftell(configFile);
	rewind(configFile);

	//Read it all into an array, close the file.
	char * buffer = (char*) malloc(sizeof(char)*fileSize);
	size_t buff_size = fread(buffer, 1, fileSize, configFile);
	fclose(configFile);


	//Read each line
	numConfigOptions = 0;
	char* name = new char[100];
	char* value = new char[100];
	for (size_t i=0; i<buff_size;) {
		//Read name/value
		readLine(buffer, i, buff_size, true, false, false, false, true, false, true, false, name, value);

		//Are both name and value non-zero?
		if (strlen(name)==0 || strlen(value)==0)
			continue;

		//Deal with our name/value pair.
		if (strcmp(name, "mywordswarning")==0) {
			numConfigOptions++;
			if (strcmp(value, "yes")==0 || strcmp(value, "true")==0)
				customDictWarning = TRUE;
			else if (strcmp(value, "no")==0 || strcmp(value, "false")==0)
				customDictWarning = FALSE;
			else
				numConfigOptions--;
		} else if (strcmp(name, "lockwindows")==0) {
			numConfigOptions++;
			if (strcmp(value, "yes")==0 || strcmp(value, "true")==0)
				dragBothWindowsTogether = TRUE;
			else if (strcmp(value, "no")==0 || strcmp(value, "false")==0)
				dragBothWindowsTogether = FALSE;
			else
				numConfigOptions--;
		} else if (strcmp(name, "powertyping")==0) {
			numConfigOptions++;
			if (strcmp(value, "yes")==0 || strcmp(value, "true")==0)
				typePhrases = TRUE;
			else if (strcmp(value, "no")==0 || strcmp(value, "false")==0)
				typePhrases = FALSE;
			else
				numConfigOptions--;
		} else if (strcmp(name, "burmesenumerals")==0) {
			numConfigOptions++;
			if (strcmp(value, "yes")==0 || strcmp(value, "true")==0)
				typeBurmeseNumbers = TRUE;
			else if (strcmp(value, "no")==0 || strcmp(value, "false")==0)
				typeBurmeseNumbers = FALSE;
			else
				numConfigOptions--;
		} else if (strcmp(name, "ballooononstart")==0) {
			numConfigOptions++;
			if (strcmp(value, "yes")==0 || strcmp(value, "true")==0)
				showBalloonOnStart = TRUE;
			else if (strcmp(value, "no")==0 || strcmp(value, "false")==0)
				showBalloonOnStart = FALSE;
			else
				numConfigOptions--;
		} else if (strcmp(name, "alwayselevate")==0) {
			numConfigOptions++;
			if (strcmp(value, "yes")==0 || strcmp(value, "true")==0)
				alwaysRunElevated = TRUE;
			else if (strcmp(value, "no")==0 || strcmp(value, "false")==0)
				alwaysRunElevated = FALSE;
			else
				numConfigOptions--;
		} else if (strcmp(name, "trackcaret")==0) {
			numConfigOptions++;
			if (strcmp(value, "yes")==0 || strcmp(value, "true")==0)
				experimentalTextCursorTracking = TRUE;
			else if (strcmp(value, "no")==0 || strcmp(value, "false")==0)
				experimentalTextCursorTracking = FALSE;
			else
				numConfigOptions--;
		} else if (strcmp(name, "ignoremodel")==0) {
			numConfigOptions++;
			if (strcmp(value, "yes")==0 || strcmp(value, "true")==0)
				dontLoadModel = TRUE;
			else if (strcmp(value, "no")==0 || strcmp(value, "false")==0)
				dontLoadModel = FALSE;
			else
				numConfigOptions--;
		} else if (strcmp(name, "charaset")==0) {
			numConfigOptions++;
			if (strcmp(value, "any")==0)
				allowNonBurmeseLetters = TRUE;
			else if (strcmp(value, "burmese")==0 || strcmp(value, "myanmar")==0)
				allowNonBurmeseLetters = FALSE;
			else
				numConfigOptions--;
		} else if (strcmp(name, "defaultencoding")==0) {
			numConfigOptions++;
			if (strcmp(value, "wininnwa")==0)
				setEncoding(ENCODING_WININNWA);
			else if (strcmp(value, "zawgyi")==0)
				setEncoding(ENCODING_ZAWGYI);
			else if (strcmp(value, "unicode")==0 || strcmp(value, "parabaik")==0 || strcmp(value, "padauk")==0 || strcmp(value, "myanmar3")==0)
				setEncoding(ENCODING_UNICODE);
			else
				numConfigOptions--;
		} else if (strcmp(name, "hotkey")==0) {
			//Set it later
			strcpy(langHotkeyRaw, value);
			numConfigOptions++;
		} else if (strcmp(name, "dictionarysize")==0) {
			long val = atol(value);
			if (val>=0 && val<UINT_MAX) {
				maxDictionaryEntries = (unsigned int)val;
				numConfigOptions++;
			}
		} else if (strcmp(name, "nexussize")==0) {
			long val = atol(value);
			if (val>=0 && val<UINT_MAX) {
				maxNexusEntries = (unsigned int)val;
				numConfigOptions++;
			}
		} else if (strcmp(name, "prefixsize")==0) {
			long val = atol(value);
			if (val>=0 && val<UINT_MAX) {
				maxPrefixEntries = (unsigned int)val;
				numConfigOptions++;
			}
		} else if (strcmp(name, "fontfileregular")==0) {
			if (strcmp(value, "embedded")==0 || strcmp(value, "default")==0) {
			} else {
				strcpy(fontFileRegular, value);
			}
			numConfigOptions++;
		} else if (strcmp(name, "fontfilesmall")==0) {
			if (strcmp(value, "embedded")==0 || strcmp(value, "default")==0) {
			} else {
				strcpy(fontFileSmall, value);
			}
			numConfigOptions++;
		}

	}

	//Get rid of our buffer
	free(buffer);
	delete [] name;
	delete [] value;
}



BOOL registerInitialHotkey()
{
	UINT modifier = 0;
	UINT keycode = 0;
	size_t str_len = strlen(langHotkeyRaw);

	//Now, set the keycode
	//Additional rule: all keystroke modifiers must also themselves be modifiers
	keycode = langHotkeyRaw[str_len-1];
	switch(keycode) {
		case '!':
			lstrcpy(langHotkeyString, _T("Alt"));
			keycode = VK_MENU; //VK_MENU == VK_ALT
			modifier |= MOD_ALT;
			break;
		case '^':
			lstrcpy(langHotkeyString, _T("Ctrl"));
			keycode = VK_CONTROL;
			modifier |= MOD_CONTROL;
			break;
		case '+':
			lstrcpy(langHotkeyString, _T("Shift"));
			keycode = VK_SHIFT;
			modifier |= MOD_SHIFT;
			break;
		case '_':
			lstrcpy(langHotkeyString, _T("Space"));
			keycode = VK_SPACE;
			break;
		default:
			swprintf(langHotkeyString, _T("%C"), keycode);
	}

	//Now, set the modifiers
	TCHAR* temp = new TCHAR[100];
	for (size_t pos=0; pos<str_len-1; pos++) {
		switch(langHotkeyRaw[pos]) {
			case '!':
				swprintf(temp, _T("Alt+%s"), langHotkeyString);
				lstrcpy(langHotkeyString, temp);
				modifier |= MOD_ALT;
				break;
			case '^':
				swprintf(temp, _T("Ctrl+%s"), langHotkeyString);
				lstrcpy(langHotkeyString, temp);
				modifier |= MOD_CONTROL;
				break;
			case '+':
				swprintf(temp, _T("Shift+%s"), langHotkeyString);
				lstrcpy(langHotkeyString, temp);
				modifier |= MOD_SHIFT;
				break;
		}
	}

	//Additional rule: Capital letters require a shift modifier
	if (keycode>='A' && keycode<='Z') {
		swprintf(temp, _T("Shift+%s"), langHotkeyString);
		lstrcpy(langHotkeyString, temp);
		modifier |= MOD_SHIFT;
	}

	//Additional rule: Lowercase letters are coded by their uppercase value
	if (keycode>='a' && keycode<='z') {
		keycode -= 'a'-'A';
	}

	//Reclaim memory
	delete [] temp;

	return RegisterHotKey(mainWindow, LANG_HOTKEY, modifier, keycode);
}



/**
 * Load the Wait Zar language model.
 */
BOOL loadModel() {
	//Load our embedded resource, the WaitZar model
	HGLOBAL     res_handle = NULL;
	HRSRC       res;
    char *      res_data;
    DWORD       res_size;

	//Previous versions of Wait Zar used the Google sparse hash library; however, even with
	//  its small footprint, this method required too much memory. So, we'll just allocate
	//  a jagged array.

	//Special...
	int numberCheck = 0;

	if (dontLoadModel==TRUE) {
		//For any of the "size" values that are set to "default", let's see
		//   if we can come up with sensible defaults
		if (maxDictionaryEntries==0)
			maxDictionaryEntries = 3000; //WaitZar has only 2,400 words
		if (maxNexusEntries==0)
			maxNexusEntries = maxDictionaryEntries*2-maxDictionaryEntries/3; //Assume an even distribution of 3000 words of 5 letters each on 26 keys, then take 20% of this
		if (maxPrefixEntries==0)
			maxPrefixEntries = maxDictionaryEntries+maxDictionaryEntries/3; //Assume the same, and cut it by 5

		//Create our data structures
		//In total, this uses 41KB of raw memory just for storing our skeleton, so
		//  I estimate about 1MB of memory for actually storing the data. 
		//  That's a lot, but it's worth it so that people's custom mywords files don't crash randomly.
		unsigned short **dictionary = new unsigned short*[maxDictionaryEntries];
		unsigned int **nexus = new unsigned int*[maxNexusEntries];
		unsigned int **prefix = new unsigned int*[maxPrefixEntries];

		//Of all these, only nexus is assumed to have anything in it
		nexus[0] = new unsigned int[1];
		nexus[0][0] = 0;

		//This should totally work :P (yes, I tested it rigorously)
		model = new WordBuilder(dictionary, 0, maxDictionaryEntries, nexus, 1, maxNexusEntries, prefix, 0, maxPrefixEntries);
	} else {
		{
			//Load the resource as a byte array and get its size, etc.
			res = FindResource(hInst, MAKEINTRESOURCE(WZ_MODEL), _T("Model"));
			if (!res) {
				MessageBox(NULL, _T("Couldn't find WZ_MODEL"), _T("Error"), MB_ICONERROR | MB_OK);
				return FALSE;
			}
			res_handle = LoadResource(NULL, res);
			if (!res_handle) {
				MessageBox(NULL, _T("Couldn't get a handle on WZ_MODEL"), _T("Error"), MB_ICONERROR | MB_OK);
				return FALSE;
			}
			res_data = (char*)LockResource(res_handle);
			res_size = SizeofResource(NULL, res);

			//Save our "model"
			model = new WordBuilder(res_data, res_size, (allowNonBurmeseLetters==TRUE));

			//Done - This shouldn't matter, though, since the process only
			//       accesses it once and, fortunately, this is not an external file.
			UnlockResource(res_handle);
		}


		//We also need to load our easy pat-sint combinations
		{
			//Load the resource as a byte array and get its size, etc.
			res = FindResource(hInst, MAKEINTRESOURCE(WZ_EASYPS), _T("Model"));
			if (!res) {
				MessageBox(NULL, _T("Couldn't find WZ_EASYPS"), _T("Error"), MB_ICONERROR | MB_OK);
				return FALSE;
			}
			res_handle = LoadResource(NULL, res);
			if (!res_handle) {
				MessageBox(NULL, _T("Couldn't get a handle on WZ_EASYPS"), _T("Error"), MB_ICONERROR | MB_OK);
				return FALSE;
			}
			res_data = (char*)LockResource(res_handle);
			res_size = SizeofResource(NULL, res);

			//Now, read through each line and add it to the external words list.
			char pre[200];
			char curr[200];
			char post[200];
			wchar_t pre_uni[100];
			wchar_t curr_uni[100];
			wchar_t post_uni[100];
			size_t index = 0;

			//Skip the BOM
			//if (res_data[index] == 0xFE && res_data[index+1]==0xFF)
			//	index += 2;

			for (;index<res_size;) {
				//Left-trim
				while (res_data[index] == ' ')
					index++;

				//Comment? Empty line? If so, skip...
				if (res_data[index]=='#' || res_data[index]=='\n') {
					while (res_data[index] != '\n')
						index++;
					index++;
					continue;
				}

				//Init
				pre[0] = 0x0000;
				int pre_pos = 0;
				bool pre_done = false;
				curr[0] = 0x0000;
				int curr_pos = 0;
				bool curr_done = false;
				post[0] = 0x0000;
				int post_pos = 0;

				//Ok, look for pre + curr = post
				while (index<res_size) {
					if (res_data[index] == '\n') {
						index++;
						break;
					} else if (res_data[index] == '+') {
						//Switch modes
						pre_done = true;
						index++;
					} else if (res_data[index] == '=') {
						//Switch modes
						pre_done = true;
						curr_done = true;
						index++;
					} else if (res_data[index] == ' ' || res_data[index] == '\t') {
						//Indrement
						index++;
					} else {
						//Add this to the current string
						if (curr_done) {
							post[post_pos++] = res_data[index++];
						} else if (pre_done) {
							curr[curr_pos++] = res_data[index++];
						} else {
							pre[pre_pos++] = res_data[index++];
						}
					}
				}

				//Ok, seal the strings
				post[post_pos++] = 0x0000;
				curr[curr_pos++] = 0x0000;
				pre[pre_pos++] = 0x0000;

				//We now have to convert them to unicode...
				mymbstowcs(pre_uni, pre, 200);
				mymbstowcs(curr_uni, curr, 200);
				mymbstowcs(post_uni, post, 200);

				//Do we have anything?
				if (wcslen(post_uni)!=0 && wcslen(curr_uni)!=0 && wcslen(pre_uni)!=0) {
					//Ok, process these strings and store them
					if (!model->addShortcut(pre_uni, curr_uni, post_uni)) {
						MessageBox(NULL, model->getLastError(), _T("Error"), MB_ICONERROR | MB_OK);

						if (isLogging) {
							fprintf(logFile, "pre: ");
							for (unsigned int x=0; x<wcslen(pre_uni); x++)
								fprintf(logFile, "U+%x ", pre_uni[x]);
							fprintf(logFile, "\n");

							fprintf(logFile, "curr: ");
							for (unsigned int x=0; x<wcslen(curr_uni); x++)
								fprintf(logFile, "U+%x ", curr_uni[x]);
							fprintf(logFile, "\n");

							fprintf(logFile, "post: ");
							for (unsigned int x=0; x<wcslen(post_uni); x++)
								fprintf(logFile, "U+%x ", post_uni[x]);
							fprintf(logFile, "\n");
						}

						return FALSE;
					}
				}
			}

			//Done - This shouldn't matter, though, since the process only
			//       accesses it once and, fortunately, this is not an external file.
			UnlockResource(res_handle);
		}
	}

	return TRUE;
}


void ShowBothWindows(int cmdShow)
{
	bool show = (cmdShow==SW_SHOW);

	ShowWindow(mainWindow, cmdShow);
	mainWindowIsVisible = show;

	if (typePhrases==TRUE) {
		ShowWindow(senWindow, cmdShow);
		subWindowIsVisible = show;
	}
}



void switchToLanguage(BOOL toMM) {
	//Don't do anything if we are switching to the SAME language.
	if (toMM == mmOn)
		return;

	//Ok, switch
	BOOL res;
	if (toMM==TRUE) {
		res = turnOnHotkeys(TRUE, true, true) && turnOnPunctuationkeys(TRUE);
		//if (typeBurmeseNumbers==TRUE)
		res = turnOnNumberkeys(TRUE) && res; //JUST numbers, not control.

		//Turno on our extended key set, too, to capture things like "("
		res = turnOnExtendedKeys(true) && res;

		//Register our help key too
		if (RegisterHotKey(mainWindow, HOTKEY_HELP, NULL, VK_F1)==FALSE)
			res = FALSE;
	} else {
		res = turnOnHotkeys(FALSE, true, true);

		//It's possible we still have some hotkeys left on...
		if (controlKeysOn == TRUE)
			turnOnControlkeys(FALSE);
		if (numberKeysOn == TRUE)
			turnOnNumberkeys(FALSE);
		if (punctuationKeysOn == TRUE)
			turnOnPunctuationkeys(FALSE);
		if (extendedKeysOn)
			turnOnExtendedKeys(false);
		if (helpKeysOn)
			turnOnHelpKeys(false);

		//Turn off our help key
		if (UnregisterHotKey(mainWindow, HOTKEY_HELP)==FALSE)
			res = FALSE;
	}

	//Any errors?
	if (res==FALSE)
		MessageBox(NULL, _T("Some hotkeys could not be set..."), _T("Warning"), MB_ICONERROR | MB_OK);

	//Switch to our target language.
	mmOn = toMM;

	//Change icon in the tray
	NOTIFYICONDATA nid;
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = mainWindow;
	nid.uID = STATUS_NID;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; //States that the callback message, icon, and size tip are used.
	nid.uCallbackMessage = UWM_SYSTRAY; //Message to send to our window
	lstrcpy(nid.szTip, _T("WaitZar Myanmar Input System")); //Set tool tip text...
	if (mmOn)
		nid.hIcon = mmIcon;
	else
		nid.hIcon = engIcon;

	if (Shell_NotifyIcon(NIM_MODIFY, &nid) == FALSE) {
		TCHAR eTemp[200];
		swprintf(eTemp, _T("Can't switch icon.\nError code: %x"), GetLastError());
		MessageBox(NULL, eTemp, _T("Warning"), MB_ICONERROR | MB_OK);
	}

	//Any windows left?
	if (mmOn==FALSE) {
		ShowBothWindows(SW_HIDE);

		if (helpWindowIsVisible) {
			helpWindowIsVisible = false;
			ShowWindow(helpWindow, SW_HIDE);
		}	
	}
}


void reBlit()
{
	//Bit blit our back buffer to the front (should prevent flickering)
	BitBlt(mainDC,0,0,C_WIDTH,C_HEIGHT,mainUnderDC,0,0,SRCCOPY);
	if (typePhrases==TRUE)
		BitBlt(senDC,0,0,SUB_C_WIDTH,SUB_C_HEIGHT,senUnderDC,0,0,SRCCOPY);
}


//Can't just blit it; we have to use updatelayeredwindow
void reBlitHelp()
{
	if (UpdateLayeredWindow(helpWindow, GetDC(NULL), NULL, &HELP_CLIENT_SIZE, helpUnderDC, &PT_ORIGIN, 0, &BLEND_FULL, ULW_ALPHA)==FALSE) {
		TCHAR msg[500];
		swprintf(msg, _T("Help window failed to update: %i"), GetLastError());
		MessageBox(NULL, msg, _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		DestroyWindow(mainWindow);
	}
}


//Only blit part of the area
void reBlit(RECT blitArea)
{
	//Bit blit our back buffer to the front (should prevent flickering)
	BitBlt(mainDC,blitArea.left,blitArea.top,blitArea.right-blitArea.left,blitArea.bottom-blitArea.top,mainUnderDC,blitArea.left,blitArea.top,SRCCOPY);
	if (typePhrases==TRUE)
		BitBlt(senDC,blitArea.left,blitArea.top,blitArea.right-blitArea.left,blitArea.bottom-blitArea.top,senUnderDC,blitArea.left,blitArea.top,SRCCOPY);
}


//Can't just blit it; we have to use updatelayeredwindow
//NOTE: We have to track when this is called, instead of just repainting the entire window.
//NOTE: Seems like it's never called.
void reBlitHelp(RECT blitArea)
{
	if (UpdateLayeredWindow(helpWindow, GetDC(NULL), NULL, &HELP_CLIENT_SIZE, helpUnderDC, &PT_ORIGIN, 0, &BLEND_FULL, ULW_ALPHA)==FALSE) {
		TCHAR msg[500];
		swprintf(msg, _T("Help window failed to update: %i"), GetLastError());
		MessageBox(NULL, msg, _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		DestroyWindow(mainWindow);
	}
}


void initCalculate()
{
	//Figure out how big each of our areas is, and where they start
	spaceWidth = mmFontBlack->getStringWidth(_T(" "));
	firstLineStart = borderWidth;
	secondLineStart = firstLineStart + mmFontBlack->getHeight() + spaceWidth + borderWidth;
	thirdLineStart = secondLineStart + mmFontBlack->getHeight() + spaceWidth + borderWidth;
	fourthLineStart = thirdLineStart + (mmFontBlack->getHeight()*8)/13 + borderWidth;

	//Now, set the window's height
	WINDOW_HEIGHT = fourthLineStart;
}


void expandHWND(HWND hwnd, HDC &dc, HDC &underDC, HBITMAP &bmp, int newX, int newY, bool noMove, int newWidth, int newHeight, int &SAVED_CLIENT_WIDTH, int &SAVED_CLIENT_HEIGHT)
{
	//Resize the current window; use SetWindowPos() since it's easier...
	int flags = SWP_NOZORDER | SWP_NOACTIVATE;
	if (noMove)
		flags |= SWP_NOMOVE;
	SetWindowPos(hwnd, NULL, newX, newY, newWidth, newHeight, flags );
	RECT r;
	GetClientRect(hwnd, &r);
	SAVED_CLIENT_WIDTH = r.right;
	SAVED_CLIENT_HEIGHT = newHeight;

	//We also have to set our graphics contexts correctly. Also, throw out the old ones.
	DeleteDC(underDC);
	DeleteObject(bmp);
	dc = GetDC(hwnd);
	underDC = CreateCompatibleDC(dc);
	bmp = CreateCompatibleBitmap(dc, SAVED_CLIENT_WIDTH, SAVED_CLIENT_HEIGHT);
	SelectObject(underDC, bmp);
}


void expandHWND(HWND hwnd, HDC &dc, HDC &underDC, HBITMAP &bmp, int newWidth, int newHeight, int &SAVED_CLIENT_WIDTH, int &SAVED_CLIENT_HEIGHT)
{
	expandHWND(hwnd, dc, underDC, bmp, 0, 0, true, newWidth, newHeight, SAVED_CLIENT_WIDTH, SAVED_CLIENT_HEIGHT);
}



/**
 * Initialize our on-screen keyboard
 */
void initCalculateHelp()
{
	//Initialize our keyboard
	helpKeyboard = new OnscreenKeyboard(mmFontSmallWhite, helpFntKeys, helpFntFore, helpFntBack, helpCornerImg);
}


wchar_t* makeStringFromKeystrokes(std::vector<unsigned short> keystrokes)
{
	for (unsigned int i=0; i<keystrokes.size(); i++) {
		returnVal[i] = keystrokes[i];
	}
	returnVal[keystrokes.size()] = 0x0000;
	
	return returnVal;
}



/**
 * Re-figure the layout of our drawing area, resize if necessary, and
 * draw onto the back buffer. Finally, blit to the front buffer.
 */
void recalculate()
{
	//First things first: can we fit this in the current background?
	int cumulativeWidth = (borderWidth+1)*2;
	std::vector<UINT32> words =  model->getPossibleWords();
	for (size_t i=0; i<words.size(); i++) {
		cumulativeWidth += mmFontBlack->getStringWidth(model->getWordString(words[i]));
		cumulativeWidth += spaceWidth;
	}

	//If not, resize. Also, keep the size small when possible.
	if (cumulativeWidth>C_WIDTH)
		expandHWND(mainWindow, mainDC, mainUnderDC, mainBitmap, cumulativeWidth, C_HEIGHT, C_WIDTH, C_HEIGHT);
	else if (cumulativeWidth<WINDOW_WIDTH && C_WIDTH>WINDOW_WIDTH)
		expandHWND(mainWindow, mainDC, mainUnderDC, mainBitmap, WINDOW_WIDTH, C_HEIGHT, C_WIDTH, C_HEIGHT);
	else if (cumulativeWidth>WINDOW_WIDTH && cumulativeWidth<C_WIDTH)
		expandHWND(mainWindow, mainDC, mainUnderDC, mainBitmap, cumulativeWidth, C_HEIGHT, C_WIDTH, C_HEIGHT);

	//Background
	SelectObject(mainUnderDC, g_BlackPen);
	SelectObject(mainUnderDC, g_DarkGrayBkgrd);
	Rectangle(mainUnderDC, 0, 0, C_WIDTH, C_HEIGHT);

	//Background -second window
	if (typePhrases==TRUE) {
		//Draw the background
		SelectObject(senUnderDC, g_BlackPen);
		SelectObject(senUnderDC, g_DarkGrayBkgrd);
		Rectangle(senUnderDC, 0, 0, SUB_C_WIDTH, SUB_C_HEIGHT);

		//Draw each string
		std::list<int>::iterator printIT = sentence->begin();
		int currentPosX = borderWidth + 1;
		int cursorPosX = currentPosX;
		int counterCursorID=0;
		for (;printIT != sentence->end(); printIT++) {
			//Append this string
			wchar_t *strToDraw;
			bool delLater = false;
			if (*printIT>=0)
				strToDraw = model->getWordString(*printIT);
			else {
				int numSystemWords = strlen(systemDefinedWords);
				int id = -(*printIT)-1;
				if (id<numSystemWords) {
					strToDraw = new wchar_t[2];
					delLater = true;
					strToDraw[0] = systemDefinedWords[id];
					strToDraw[1] = 0x0000;
				} else
					strToDraw = userDefinedWordsZg[id-numSystemWords];
			}
			mmFontSmallWhite->drawString(senUnderDC, strToDraw, currentPosX, borderWidth+1);
			currentPosX += (mmFontSmallWhite->getStringWidth(strToDraw)+1);

			if (delLater)
				delete [] strToDraw;

			//Line? (don't print now; we also want to draw it at cursorIndex==-1)
			if (counterCursorID == sentence->getCursorIndex())
				cursorPosX = currentPosX;

			//Increment
			counterCursorID++;
		}

		//Draw the cursor
		MoveToEx(senUnderDC, cursorPosX-1, borderWidth+1, NULL);
		LineTo(senUnderDC, cursorPosX-1, SUB_C_HEIGHT-borderWidth-1);

		//Draw the current encoding
		int encStrWidth = mmFontSmallWhite->getStringWidth(currEncStr);
		SelectObject(senUnderDC, g_BlackPen);
		SelectObject(senUnderDC, g_GreenBkgrd);
		Rectangle(senUnderDC, SUB_C_WIDTH-encStrWidth-3, 0, SUB_C_WIDTH, SUB_C_HEIGHT);
		mmFontSmallWhite->drawString(senUnderDC, currEncStr, SUB_C_WIDTH-encStrWidth-2, SUB_C_HEIGHT/2-mmFontSmallWhite->getHeight()/2);
	}

	//White overlays
	SelectObject(mainUnderDC, g_EmptyPen);
	SelectObject(mainUnderDC, g_WhiteBkgrd);
	Rectangle(mainUnderDC, borderWidth+1, firstLineStart+1, C_WIDTH-borderWidth-1, secondLineStart-borderWidth);
	Rectangle(mainUnderDC, borderWidth+1, secondLineStart, C_WIDTH-borderWidth-1, thirdLineStart-borderWidth);
	Rectangle(mainUnderDC, borderWidth+1, thirdLineStart, C_WIDTH-borderWidth-1, fourthLineStart-borderWidth-1);

	//Now, draw the strings....
	PulpCoreFont* mmFont = mmFontBlack;
	int xOffset = 0;
	TCHAR digit[5];
	TCHAR extendedWordString[300];
	if (helpWindowIsVisible) {
		//Prepare the extended word string a bit early
		lstrcpy(currStrZg, currStr);
		waitzar::sortMyanmarString(currStrZg);
		lstrcpy(currStrZg, waitzar::renderAsZawgyi(currStrZg));
		lstrcpy(extendedWordString, currStrZg);

		//We only have one word: the guessed romanisation
		currStrDictID = -1;
		for (unsigned int i=0; i<model->getTotalDefinedWords(); i++) {
			//Does this word match?
			wchar_t *currWord = model->getWordString(i);
			if (wcscmp(currWord, extendedWordString)==0) {
				wcscpy(currLetterSt, currWord);
				currStrDictID = i;
				break;
			}
		}

		//Try a less-strict filtering
		// We leave this out... it shouldn't be necessary if we're filtering our strings.
		/*if (currMatchID==-1) {
			unsigned int currEnc = model->getOutputEncoding();
			model->setOutputEncoding(ENCODING_UNICODE);
			for (unsigned int i=0; i<model->getTotalDefinedWords(); i++) {
				wchar_t *currUni = makeStringFromKeystrokes(model->getWordKeyStrokes(i));
				if (wcscmp(currUni, currLetterSt)==0) {
					wcscpy(currLetterSt, currUni);
					currMatchID = i;
					break;
				}
			}
			model->setOutputEncoding(currEnc);
		}*/

		//Any match at all?
		if (currStrDictID!=-1) {
			char *romanWord = model->reverseLookupWord(currStrDictID);
			swprintf(currLetterSt, L"(%S)", romanWord);
			mmFontGreen->drawString(mainUnderDC, currLetterSt, borderWidth+1+spaceWidth/2, secondLineStart+spaceWidth/2);
		}

		//Helper text
		mmFontSmallGray->drawString(mainUnderDC, _T("(Press \"Space\" to type this word)"), borderWidth+1+spaceWidth/2, thirdLineStart-spaceWidth/2);
	} else if (mainWindowIsVisible) { //Crashes otherwise
		for (size_t i=0; i<words.size(); i++) {
			//If this is the currently-selected word, draw a box under it.
			//int x = words[i];
			int thisStrWidth = mmFont->getStringWidth(model->getWordString(words[i]));
			if (i!=model->getCurrSelectedID())
				mmFont = mmFontBlack;
			else {
				mmFont = mmFontGreen;

				SelectObject(mainUnderDC, g_YellowBkgrd);
				SelectObject(mainUnderDC, g_GreenPen);
				Rectangle(mainUnderDC, borderWidth+xOffset+1, secondLineStart, borderWidth+1+xOffset+thisStrWidth+spaceWidth, secondLineStart+mmFont->getHeight()+spaceWidth-1);
			}

			mmFont->drawString(mainUnderDC, model->getWordString(words[i]), borderWidth+1+spaceWidth/2 + xOffset, secondLineStart+spaceWidth/2);

			if (i<10) {
				swprintf(digit, _T("%i"), ((i+1)%10));
				int digitWidth = mmFont->getStringWidth(digit);

				mmFont->drawString(mainUnderDC, digit, borderWidth+1+spaceWidth/2 + xOffset + thisStrWidth/2 -digitWidth/2, thirdLineStart-spaceWidth/2-1);
			}

			xOffset += thisStrWidth + spaceWidth;
		}
	}

	if (!helpWindowIsVisible) {
		TCHAR* parenStr = model->getParenString();
		if (parenStr!=NULL && lstrlen(parenStr)>0) {
			swprintf(extendedWordString, _T("%s (%s)"), currStr, parenStr);
		} else {
			lstrcpy(extendedWordString, currStr);
		}
	}

	mmFontBlack->drawString(mainUnderDC, extendedWordString, borderWidth+1+spaceWidth/2, firstLineStart+spaceWidth/2+1);

	//Paint
	reBlit();
}



std::vector<unsigned short> getUserWordKeyStrokes(unsigned int id, unsigned int encoding)
{
	//Get the string
	wchar_t *typedStr;
	wchar_t destStr[200];
	unsigned int numSystemDefWords = strlen(systemDefinedWords);
	if (id<numSystemDefWords) {
		destStr[0] = systemDefinedWords[id];
		destStr[1] = 0x0000;
		typedStr = destStr;
	} else {
		id -= numSystemDefWords;
		if (encoding==ENCODING_UNICODE)
			typedStr = userDefinedWords[id];
		else if (encoding==ENCODING_ZAWGYI)
			typedStr = userDefinedWordsZg[id];
		else if (encoding==ENCODING_WININNWA) {
			wchar_t* srcStr = userDefinedWords[id];
			wcscpy(destStr, L"");
			convertFont(destStr, srcStr, Zawgyi_One, WinInnwa);
			typedStr = destStr;
		} else
			typedStr = L"";
	}

	//Convert
	size_t length = wcslen(typedStr);
	userKeystrokeVector.clear();
	for (size_t i=0; i<length; i++) 
		userKeystrokeVector.push_back((unsigned short) typedStr[i]);

	return userKeystrokeVector;
}



void typeCurrentPhrase()
{
	//Send key presses to the top-level program.
	HWND fore = GetForegroundWindow();
	SetActiveWindow(fore); //This probably won't do anything, since we're not attached to this window's message queue.


	//Use SendInput instead of SendMessage, since SendMessage requires the actual
	//  sub-window (component) to recieve the message, whereas SendInput only
	//  requires the top-level window. We could probably hack in SendMessage now that
	//  we're not becoming the active window, but for now I'd rather have a stable
	//  system than one that works on Windows 98.
	std::list<int>::iterator printIT = sentence->begin();
	std::vector<WORD> keyStrokes;
	int number_of_key_events = 0;
	for (;printIT!=sentence->end() || stopChar!=0;) {
		//We may or may not have a half/full stop at the end.
		if (printIT!=sentence->end()) {
			if (*printIT>=0)
				keyStrokes = model->getWordKeyStrokes(*printIT);
			else {
				keyStrokes = getUserWordKeyStrokes(-(*printIT)-1, model->getOutputEncoding());
			}
		} else {
			keyStrokes.clear();
			keyStrokes.push_back(stopChar);
		}

		//Buffer each key-stroke
		for (size_t i=0; i<keyStrokes.size(); i++) {
			//Send keydown
			keyInputPrototype.wScan = keyStrokes[i];
			keyInputPrototype.dwFlags = KEYEVENTF_UNICODE;
			inputItems[number_of_key_events++].ki = keyInputPrototype;

			keyInputPrototype.dwFlags = KEYEVENTF_UNICODE|KEYEVENTF_KEYUP;
			inputItems[number_of_key_events++].ki = keyInputPrototype;
		}

		//Increment
		if (printIT!=sentence->end())
			printIT++;
		else
			stopChar = 0;
	}


	//Send all the keystrokes at once to avoid a weird bug with single-letter repetitions.
	if(!SendInput(number_of_key_events,inputItems,sizeof(INPUT))) {
		MessageBox(NULL, _T("Couldn't send input"), _T("Error"), MB_OK|MB_ICONERROR);
	}



	//Now, reset...
	model->reset(true);
	sentence->clear();
	for (unsigned int i=0; i<userDefinedWords.size(); i++) {
		delete [] userDefinedWords[i];
		delete [] userDefinedWordsZg[i];
	}
	userDefinedWords.clear();
	userDefinedWordsZg.clear();


	//Technically, this can be called with JUST a stopChar, which implies
	//  that the window isn't visible. So check this.
	if (controlKeysOn) {
		//Turn off control keys
		turnOnControlkeys(FALSE);

		//Hide the window(s)
		ShowBothWindows(SW_HIDE);
	}
}



BOOL selectWord(int id, bool indexNegativeEntries)
{
	//Are there any words to use?
	int wordID = id;
	if (!indexNegativeEntries) {
		std::pair<BOOL, UINT32> typedVal = model->typeSpace(id);
		if (typedVal.first == FALSE)
			return FALSE;
		wordID = typedVal.second;
	}

	//Optionally turn off numerals
	//if (numberKeysOn==TRUE && typeBurmeseNumbers==FALSE)
	//	turnOnNumberkeys(FALSE);

	if (typePhrases==FALSE) {
		//Simple Case
		sentence->clear();
		sentence->insert(wordID);
		typeCurrentPhrase();
	} else {
		//Advanced Case - Insert
		sentence->insert(wordID);
	}

	return TRUE;
}




//Message handling for our help window
LRESULT CALLBACK HelpWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_CREATE:
		{
			//Resize our window?
			MoveWindow(hwnd, 400, 300, HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT, FALSE);

			//Now, create all our buffering objects
			RECT r;
			GetClientRect(hwnd, &r);
			HELP_CLIENT_SIZE.cx = r.right;
			HELP_CLIENT_SIZE.cy = r.bottom;
			helpDC = GetDC(hwnd);

			//Our expanding code is a bit fragile, so we'll have to initialize helpUnderDC and helpBitmap here.
			helpUnderDC = CreateCompatibleDC(helpDC);
			helpBitmap = CreateCompatibleBitmap(helpDC, HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT);
			SelectObject(helpUnderDC, helpBitmap);

			//Initialize our help window
			if (mainInitDone) {
				initCalculateHelp();
			}
			helpInitDone = true;
			//ShowWindow(hwnd, SW_SHOW);
			//helpWindowIsVisible = true;

			break;
		}
		/*case WM_NCHITTEST: //Allow dragging of the client area...
		{
			LRESULT uHitTest = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);
			if(uHitTest == HTCLIENT) {
				return HTCAPTION;
			} else
				return uHitTest;
			break;
		}*/
		case WM_LBUTTONDOWN:
		{
			//Thanks to dr. Carbon for suggesting this method.
			if (SetCapture(hwnd)!=NULL)
				break;

			//Drag the mosue
			isDragging = true;
			GetCursorPos(&dragFrom);
			break;
		}
		case WM_MOUSEMOVE:
		{ //Allow dragging of the mouse by its client area. Reportedly more accurate than NCHIT_TEST
			if (isDragging) {
				RECT rect;
				POINT dragTo;
				GetWindowRect(hwnd, &rect);
				GetCursorPos(&dragTo);
				
				//Constantly update its position
				MoveWindow(hwnd, (dragTo.x - dragFrom.x) + rect.left,
					(dragTo.y - dragFrom.y) + rect.top,
					rect.right - rect.left, rect.bottom - rect.top, FALSE);

				dragFrom = dragTo;
			}
			break;
		}
		case WM_LBUTTONUP:
		{
			if (isDragging) {
				isDragging = false;
				ReleaseCapture();
			}
			break;
		}
		case WM_MOVE:
		{
			//Move the main window?
			/*if (senWindowSkipMove==FALSE && (mainWindowIsVisible || subWindowIsVisible) && dragBothWindowsTogether==TRUE) {
				RECT r;
				GetWindowRect(hwnd, &r);
				RECT r2;
				GetWindowRect(GetDesktopWindow(), &r2);
				mainWindowSkipMove = TRUE;
				SetWindowPos(mainWindow, HWND_TOPMOST, min(max(r.left, 0), r2.right-C_WIDTH), max(r.top-C_HEIGHT, 0), 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			senWindowSkipMove = FALSE;*/
			break;
		}
		case WM_PAINT:
		{
			//Update only if there's an area which needs updating (e.g., a higher-level
			//  window has dragged over this one's client area... it can happen only with popups,
			//  but let's do it just to be safe.
			RECT updateRect;
			if (GetUpdateRect(hwnd, &updateRect, FALSE) != 0)
			{
				//Blitting every tick will slow us down... we should validate the
				//  rectangle after drawing it.
				reBlitHelp(updateRect);

				//Validate the client area
				ValidateRect(hwnd, NULL);
			}

			break;
		}
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}




//Message handling for our secondary window
LRESULT CALLBACK SubWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg) {
		case WM_CREATE:
		{
			//Resize our window?
			MoveWindow(hwnd, 100, 100+WINDOW_HEIGHT, SUB_WINDOW_WIDTH, SUB_WINDOW_HEIGHT, FALSE);

			//Now, create all our buffering objects
			RECT r;
			GetClientRect(hwnd, &r);
			SUB_C_WIDTH = r.right;
			SUB_C_HEIGHT = r.bottom;

			senDC = GetDC(hwnd);
			senUnderDC = CreateCompatibleDC(senDC);

			senBitmap = CreateCompatibleBitmap(senDC, SUB_WINDOW_WIDTH, SUB_WINDOW_HEIGHT);
			SelectObject(senUnderDC, senBitmap);
			break;
		}
		/*case WM_NCHITTEST: //Allow dragging of the client area...
		{
			LRESULT uHitTest = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);
			if(uHitTest == HTCLIENT) {
				return HTCAPTION;
			} else
				return uHitTest;
			break;
		}*/
		case WM_LBUTTONDOWN:
		{
			//Thanks to dr. Carbon for suggesting this method.
			if (SetCapture(hwnd)!=NULL)
				break;

			//Drag the mosue
			isDragging = true;
			GetCursorPos(&dragFrom);
			break;
		}
		case WM_MOUSEMOVE:
		{ //Allow dragging of the mouse by its client area. Reportedly more accurate than NCHIT_TEST
			if (isDragging) {
				RECT rect;
				POINT dragTo;
				GetWindowRect(hwnd, &rect);
				GetCursorPos(&dragTo);
				
				//Constantly update its position
				MoveWindow(hwnd, (dragTo.x - dragFrom.x) + rect.left,
					(dragTo.y - dragFrom.y) + rect.top,
					rect.right - rect.left, rect.bottom - rect.top, FALSE);

				dragFrom = dragTo;
			}
			break;
		}
		case WM_LBUTTONUP:
		{
			if (isDragging) {
				isDragging = false;
				ReleaseCapture();
			}
			break;
		}
		case WM_MOVE:
		{
			//Move the main window?
			if (senWindowSkipMove==FALSE && (mainWindowIsVisible || subWindowIsVisible) && dragBothWindowsTogether==TRUE) {
				RECT r;
				GetWindowRect(hwnd, &r);
				RECT r2;
				GetWindowRect(GetDesktopWindow(), &r2);
				mainWindowSkipMove = TRUE;
				SetWindowPos(mainWindow, HWND_TOPMOST, min(max(r.left, 0), r2.right-C_WIDTH), max(r.top-C_HEIGHT, 0), 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			senWindowSkipMove = FALSE;
			break;
		}
		case WM_PAINT:
		{
			//Update only if there's an area which needs updating (e.g., a higher-level
			//  window has dragged over this one's client area... it can happen only with popups,
			//  but let's do it just to be safe.
			RECT updateRect;
			if (GetUpdateRect(hwnd, &updateRect, FALSE) != 0)
			{
				//Blitting every tick will slow us down... we should validate the
				//  rectangle after drawing it.
				reBlit(updateRect);

				//Validate the client area
				ValidateRect(hwnd, NULL);
			}

			break;
		}
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}



void updateHelpWindow()
{
	if (!helpWindowIsVisible) {
		//Did we even initialize the help window?
		if (!helpIsCached) {
			//Time to re-size our help window. Might as well center it now, too
			HWND taskBar = FindWindowW(_T("Shell_TrayWnd"), _T(""));
			RECT r;
			GetClientRect(GetDesktopWindow(), &r);
			int newX = (r.right-r.left)/2-helpKeyboard->getWidth()/2;
			int newY = (r.bottom-r.top)-helpKeyboard->getHeight();
			if (taskBar != NULL) {
				GetClientRect(taskBar, &r);
				newY -= (r.bottom-r.top);
			}
			int newW = 0;
			int newH = 0;
			expandHWND(helpWindow, helpDC, helpUnderDC, helpBitmap, newX, newY, false, helpKeyboard->getWidth(), helpKeyboard->getHeight(), newW, newH);
			HELP_CLIENT_SIZE.cx = newW;
			HELP_CLIENT_SIZE.cy = newH;

			//Might as well build the reverse lookup
			model->reverseLookupWord(0);

			//...and now we can properly initialize its drawing surface
			helpKeyboard->init(helpDC, helpUnderDC, helpBitmap);

			helpIsCached = true;
		}


		//Register all hotkeys relevant for the help window
		bool res = true;
		if (controlKeysOn==FALSE) //We'll need these too.
			res = (turnOnControlkeys(TRUE)==TRUE);
		if (!turnOnHelpKeys(true) || !res)
			MessageBox(mainWindow, _T("Could not turn on the shift/control hotkeys."), _T("Error"), MB_ICONERROR | MB_OK);


		//Clear our current word (not the sentence, though, and keep the trigrams)
		lstrcpy(currStr, _T(""));
		model->reset(false);
		recalculate();

		//Show the help window
		ShowWindow(helpWindow, SW_SHOW);
		helpWindowIsVisible = true;
		reBlitHelp();
	} else {
		//Clear our word string
		lstrcpy(currStr, _T(""));

		turnOnHelpKeys(false);
		ShowWindow(helpWindow, SW_HIDE);
		helpWindowIsVisible = false;
		recalculate();
	}
}



/**
 * Message-handling code.
 */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//Handle callback
	switch(msg) {
		case WM_CREATE:
		{
			//Make our font?
			makeFont(hwnd);

			//Did it work? If so, init. If not, exit.
			if (mmFontBlack->isInError()) {
				DestroyWindow(hwnd);
				break;
			}
			initCalculate();

			//Resize our window?
			MoveWindow(hwnd, 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, FALSE);

			//Now, create all our buffering objects
			RECT r;
			GetClientRect(hwnd, &r);
			C_WIDTH = r.right;
			C_HEIGHT = r.bottom;

			mainDC = GetDC(hwnd);
			mainUnderDC = CreateCompatibleDC(mainDC);

			mainBitmap = CreateCompatibleBitmap(mainDC, WINDOW_WIDTH, WINDOW_HEIGHT);
			SelectObject(mainUnderDC, mainBitmap);

			//Init our helper window
			if (helpInitDone) {
				initCalculateHelp();
			}
			mainInitDone = true;

			break;
		}
		case WM_MOVE:
		{
			//Move the sentence window?
			if (typePhrases==TRUE && mainWindowSkipMove==FALSE && subWindowIsVisible && dragBothWindowsTogether==TRUE) {
				RECT r;
				GetWindowRect(hwnd, &r);
				RECT r2;
				GetWindowRect(GetDesktopWindow(), &r2);
				senWindowSkipMove = TRUE;
				SetWindowPos(senWindow, HWND_TOPMOST, min(max(r.left, 0), r2.right-SUB_C_WIDTH), min(r.top+C_HEIGHT, r2.bottom-SUB_C_HEIGHT), 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			mainWindowSkipMove = FALSE;
			break;
		}
		case UWM_HOTKEY_UP: //HOTKEY_UP is defined by us, it is just like HOTKEY_DOWN except it doesn't use the lparam
		{
			//Update our virtual keyboard
			if (helpKeyboard->highlightKey(wParam, false))
				reBlitHelp();

			break;
		}
		case WM_HOTKEY:
		{
			//Added to help us avoid multiple reactions, if necessary
			// (probably only necessary for the number/myanmar numbers distinction)
			bool keyWasUsed = false;

			//Handle our main language hotkey
			if(wParam == LANG_HOTKEY) {
				//Switch language
				if (mmOn==TRUE)
					switchToLanguage(FALSE);
				else
					switchToLanguage(TRUE);

				//Reset the model
				sentence->clear();
				model->reset(true);

				keyWasUsed = true;
			}


			//Should we update the virtual keyboard? This is done independently
			//  of actually handling the keypress itself
			if (helpWindowIsVisible && highlightKeys) {
				//If this is a shifted key, get which key is shifted: left or right
				unsigned int keyCode = wParam;
				if (keyCode==HOTKEY_SHIFT) {
					//Well, I like posting fake messages. :D
					// Note that (lParam>>16)&VK_LSHIFT doesn't work here
					if ((GetKeyState(VK_LSHIFT)&0x8000)!=0)
						PostMessage(mainWindow, WM_HOTKEY, HOTKEY_VIRT_LSHIFT, MOD_SHIFT);
					if ((GetKeyState(VK_RSHIFT)&0x8000)!=0)
						PostMessage(mainWindow, WM_HOTKEY, HOTKEY_VIRT_RSHIFT, MOD_SHIFT);
				} else {
					//Is this a valid key? If so, highlight it and repaint the help window
					if (helpKeyboard->highlightKey(keyCode, true)) {
						reBlitHelp();

						//CRITICAL SECTION
						{
							EnterCriticalSection(&threadCriticalSec);

							//Manage our thread's list of currently pressed hotkeys
							hotkeysDown.remove(keyCode);
							hotkeysDown.push_front(keyCode);

							//Do we need to start our thread?
							if (!threadIsActive) {
								threadIsActive = true;

								ResumeThread(keyTrackThread);

								//GetSystemTimeAsFileTime(&res);
								//int diff = (res.dwLowDateTime-rL)/10000L; //hectonanoseconds div 10,000 to get ms
							}

							LeaveCriticalSection(&threadCriticalSec);
						}
					}
				}

				//Doesn't consume a keypress
				//keyWasUsed = keyWasUsed;
			}


			//What to do if our user hits "F1".
			if (wParam == HOTKEY_HELP && allowNonBurmeseLetters==FALSE) {
				updateHelpWindow();

				keyWasUsed = true;
			}


			//Close the window?
			if (wParam == HOTKEY_ESC) {
				if (helpWindowIsVisible) {
					//Clear our word string
					lstrcpy(currStr, _T(""));

					turnOnHelpKeys(false);
					ShowWindow(helpWindow, SW_HIDE);
					helpWindowIsVisible = false;
					recalculate();
				} else {
					if (!mainWindowIsVisible) {
						//Kill the entire sentence.
						sentence->clear();
						model->reset(true);
						turnOnControlkeys(FALSE);
						ShowBothWindows(SW_HIDE);
					} else {
						model->reset(false);

						//No more numbers
						//if (typeBurmeseNumbers==FALSE)
						//	turnOnNumberkeys(FALSE);

						//Are we using advanced input?
						if (typePhrases==FALSE) {
							//Turn off control keys
							turnOnControlkeys(FALSE);
							ShowBothWindows(SW_HIDE);
						} else {
							//Just hide the typing window for now.
							ShowWindow(mainWindow, SW_HIDE);
							mainWindowIsVisible = false;

							if (sentence->size()==0) {
								//Kill the entire sentence.
								sentence->clear();
								ShowBothWindows(SW_HIDE);
								turnOnControlkeys(FALSE);
							}
						}
					}
				}

				keyWasUsed = true;
			}


			//Delete: Phrases only
			if (wParam == HOTKEY_DELETE) {
				if (helpWindowIsVisible) {
					//Delete the letter in front of you (encoding-wise, not visibly)
					//ADD LATER

				} else {
					if (!mainWindowIsVisible) {
						//Delete the next word
						if (sentence->deleteNext())
							recalculate();
						if (sentence->size()==0) {
							//Kill the entire sentence.
							sentence->clear();
							turnOnControlkeys(FALSE);
							ShowBothWindows(SW_HIDE);
						}
					}
				}

				keyWasUsed = true;
			}


			//Back up
			if (wParam == HOTKEY_BACK) {
				if (helpWindowIsVisible) {
					//Delete the letter in back of you (encoding-wise, not visibly)
					int len = lstrlen(currStr);
					if (len>0)
						currStr[len-1] = 0x0000;
					recalculate();
				} else {
					if (!mainWindowIsVisible) {
						//Delete the previous word
						if (sentence->deletePrev(model))
							recalculate();
						if (sentence->size()==0) {
							//Kill the entire sentence.
							sentence->clear();
							turnOnControlkeys(FALSE);
							ShowBothWindows(SW_HIDE);
						}
					} else {
						if (model->backspace()) {
							//Truncate...
							currStr[lstrlen(currStr)-1] = 0;
							recalculate();
						} else {
							//No more numerals.
							//if (typeBurmeseNumbers==FALSE)
							//	turnOnNumberkeys(FALSE);

							//Are we using advanced input?
							if (typePhrases==FALSE) {
								//Turn off control keys
								turnOnControlkeys(FALSE);

								ShowBothWindows(SW_HIDE);
							} else {
								//Just hide the typing window for now.
								ShowWindow(mainWindow, SW_HIDE);
								mainWindowIsVisible = false;

								if (sentence->size()==0) {
									//Kill the entire sentence.
									sentence->clear();
									turnOnControlkeys(FALSE);

									ShowWindow(senWindow, SW_HIDE);
									subWindowIsVisible = false;
								}
							}
						}
					}
				}

				keyWasUsed = true;
			}


			//Handle control hotkeys
			if (wParam == HOTKEY_RIGHT) {
				if (helpWindowIsVisible) {
					//Move the letter cursor one to the right
					//ADD LATER

				} else {
					if (mainWindowIsVisible) {
						//Move right/left within the current word.
						if (model->moveRight(1) == TRUE)
							recalculate();
					} else {
						//Move right/left within the current phrase.
						if (sentence->moveCursorRight(1, model))
							recalculate();
					}
				}

				keyWasUsed = true;
			} else if (wParam == HOTKEY_LEFT) {
				if (helpWindowIsVisible) {
					//Move the letter cursor one to the left
					//ADD LATER

				} else {
					if (mainWindowIsVisible) {
						if (model->moveRight(-1) == TRUE)
							recalculate();
					} else {
						//Move right/left within the current phrase.
						if (sentence->moveCursorRight(-1, model))
							recalculate();
					}
				}

				keyWasUsed = true;
			}


			//Determine what number, if any, was pressed
			int numCode = -1;
			if (wParam>=HOTKEY_0 && wParam<=HOTKEY_9)
				numCode = (int)wParam - HOTKEY_0;
			if (wParam>=HOTKEY_NUM0 && wParam<=HOTKEY_NUM9)
				numCode = (int)wParam - HOTKEY_NUM0;

			//Handle numbers
			if (!helpWindowIsVisible) {
				stopChar=0;
				if (numCode > -1) {
					if (mainWindowIsVisible) {
						//Convert 1..0 to 0..9
						if (--numCode<0)
							numCode = 9;

						//The model is visible: select that word
						BOOL typed = selectWord(numCode, helpWindowIsVisible);
						if (typed==TRUE && typePhrases==TRUE) {
							ShowWindow(mainWindow, SW_HIDE);
							mainWindowIsVisible = false;

							lstrcpy(currStr, _T(""));
							model->reset(false);
							recalculate();
						}

						keyWasUsed = true;
					} else if (typeBurmeseNumbers) {
						if (typePhrases==FALSE) {
							sentence->clear();
							sentence->insert(numCode);
							typeCurrentPhrase();
						} else {
							//Just type that number directly.
							sentence->insert(numCode);
							sentence->moveCursorRight(0, true, model);

							//Is our window even visible?
							if (!subWindowIsVisible) {
								turnOnControlkeys(TRUE);

								ShowWindow(senWindow, SW_SHOW);
								subWindowIsVisible = true;
							}

							recalculate();
						}

						keyWasUsed = true;
					}
				}
			}


			//Handle Half-stop/Full-stop
			if (wParam==HOTKEY_COMMA || wParam==HOTKEY_PERIOD) {
				stopChar = model->getStopCharacter((wParam==HOTKEY_PERIOD));
				if (helpWindowIsVisible) {
					//Possibly do nothing...
					//ADD LATER

				} else {
					if (!mainWindowIsVisible) {
						if (!subWindowIsVisible) {
							//This should be cleared already, but let's be safe...
							sentence->clear();
						}
						//Otherwise, we perform the normal "enter" routine.
						typeCurrentPhrase();
					}
				}

				keyWasUsed = true;
			}


			//Handle Enter
			if (wParam==HOTKEY_ENTER || wParam==HOTKEY_SHIFT_ENTER) {
				if (helpWindowIsVisible) {
					//Select our word, add it to the dictionary temporarily.
					// Flag the new entry so it can be cleared later when the sentence is selected
					if (currStrDictID==-1) {
						wchar_t *tempStr = new wchar_t[100];
						wchar_t *tempStrZg = new wchar_t[100];
						wcscpy(tempStr, currStr);
						wcscpy(tempStrZg, currStrZg);
						waitzar::sortMyanmarString(tempStr);
						userDefinedWords.push_back(tempStr);
						userDefinedWordsZg.push_back(tempStrZg);
						currStrDictID = -1*(strlen(systemDefinedWords)+userDefinedWords.size());
					}

					//Hide the help window
					helpWindowIsVisible = false;
					turnOnHelpKeys(false);
					ShowWindow(helpWindow, SW_HIDE);

					//Try to type this word
					BOOL typed = selectWord(currStrDictID, true);
					if (typed==TRUE && typePhrases==TRUE) {
						ShowWindow(mainWindow, SW_HIDE);
						mainWindowIsVisible = false;

						model->reset(false);
						lstrcpy(currStr, _T(""));
						recalculate();
					}

					//We need to reset the trigrams here...
					sentence->updateTrigrams(model);
				} else {
					stopChar = 0;
					if (mainWindowIsVisible) {
						//The model is visible: select that word
						BOOL typed = selectWord(-1, helpWindowIsVisible);
						if (typed==TRUE && typePhrases==TRUE) {
							ShowWindow(mainWindow, SW_HIDE);
							mainWindowIsVisible = false;

							lstrcpy(currStr, _T(""));
							model->reset(false);
							recalculate();
						}
					} else {
						//Type the entire sentence
						typeCurrentPhrase();
					}
				}

				keyWasUsed = true;
			}

			//Handle Space Bar
			if (wParam==HOTKEY_SPACE || wParam==HOTKEY_SHIFT_SPACE) {
				if (helpWindowIsVisible) {
					//Select our word, add it to the dictionary temporarily.
					// Flag the new entry so it can be cleared later when the sentence is selected
					if (currStrDictID==-1) {
						wchar_t *tempStr = new wchar_t[100];
						wchar_t *tempStrZg = new wchar_t[100];
						wcscpy(tempStr, currStr);
						wcscpy(tempStrZg, currStrZg);
						waitzar::sortMyanmarString(tempStr);
						userDefinedWords.push_back(tempStr);
						userDefinedWordsZg.push_back(tempStrZg);
						currStrDictID = -1*(strlen(systemDefinedWords)+userDefinedWords.size());
					}

					//Hide the help window
					helpWindowIsVisible = false;
					turnOnHelpKeys(false);
					ShowWindow(helpWindow, SW_HIDE);

					//Try to type this word
					BOOL typed = selectWord(currStrDictID, true);
					if (typed==TRUE && typePhrases==TRUE) {
						ShowWindow(mainWindow, SW_HIDE);
						mainWindowIsVisible = false;

						model->reset(false);
						lstrcpy(currStr, _T(""));
						recalculate();
					}

					//We need to reset the trigrams here...
					sentence->updateTrigrams(model);

					keyWasUsed = true;
				} else {
					stopChar = 0;
					if (mainWindowIsVisible) {
						//The model is visible: select that word
						BOOL typed = selectWord(-1, helpWindowIsVisible);
						if (typed==TRUE && typePhrases==TRUE) {
							ShowWindow(mainWindow, SW_HIDE);
							mainWindowIsVisible = false;

							model->reset(false);
							lstrcpy(currStr, _T(""));
							recalculate();
						}

						keyWasUsed = true;
					} else {
						//A bit tricky here. If the cursor's at the end, we'll
						//  do HOTKEY_ENTER. But if not, we'll just advance the cursor.
						//Hopefully this won't confuse users so much.
						if (wParam==HOTKEY_SPACE) {
							if (sentence->getCursorIndex()==-1 || sentence->getCursorIndex()<((int)sentence->size()-1)) {
								sentence->moveCursorRight(1, model);
								recalculate();
							} else {
								//Type the entire sentence
								typeCurrentPhrase();
							}

							keyWasUsed = true;
						}
					}
				}
			}

			//Handle our individual letter presses as hotkeys
			if (helpWindowIsVisible) {
				//Handle our help menu
				wchar_t* nextBit = helpKeyboard->typeLetter(wParam);
				if (nextBit != NULL) {
					//Valid letter
					lstrcat(currStr, nextBit);
					size_t len = wcslen(currStr);

					//Special cases
					if (wcslen(nextBit)==1 && nextBit[0]==L'\u1039') {
						//Combiner functions in reverse
						if (len>1 && canStack(currStr[len-2])) {
							currStr[len-1] = currStr[len-2];
							currStr[len-2] = nextBit[0];
						} else {
							currStr[len-1] = 0x0000; //Not standard behavior, but let's avoid bad combinations.
						}
					} else if (wcscmp(nextBit, L"\u1004\u103A\u1039")==0) {
						//Kinzi can be typed after the consonant instead of before it.
						//For now, we only cover the general case of typing "kinzi" directly after a consonant
						if (len>3 && canStack(currStr[len-4])) {
							currStr[len-1] = currStr[len-4];
							currStr[len-4] = nextBit[0];
							currStr[len-3] = nextBit[1];
							currStr[len-2] = nextBit[2];
						}
					}


					//Pre-sort unicode strings (should be helpful)
					recalculate();

					//Is the main window visible?
					if (!mainWindowIsVisible) {
						//Show it
						if (typePhrases==FALSE || !subWindowIsVisible) {
							ShowBothWindows(SW_SHOW);
						} else {
							ShowWindow(mainWindow, SW_SHOW);
							mainWindowIsVisible = true;
						}
					}

					keyWasUsed = true;
				}
			} else {
				//Handle regular letter-presses 
				int keyCode = (int)wParam;
				if (wParam >= HOTKEY_A && wParam <= HOTKEY_Z) //Seems like we should be doing with this Shift modifiers..
					keyCode += 32;
				if (keyCode >= HOTKEY_A_LOW && keyCode <= HOTKEY_Z_LOW)
				{
					//Run this keypress into the model. Accomplish anything?
					if (!model->typeLetter(keyCode))
						break;

					//Is this the first keypress of a romanized word? If so, the window is not visible...
					if (!mainWindowIsVisible)
					{
						//Reset it...
						lstrcpy(currStr, _T(""));
						//recalculate();

						//Optionally turn on numerals
						if (numberKeysOn==FALSE)
							turnOnNumberkeys(TRUE);

						//TEST: Re-position it
						//TEST: Use AttachThredInput? Yes!
						//Still a bit glitchy....
						//NOTE: We can probably use GetForegroundWindow() + AttachThreadInput() + GetFocus() to 
						//      avoid SendInput() and just use PostMessage(). This will help us support Windows 98, etc.
						if (experimentalTextCursorTracking==TRUE) {
							//Reset parameters for our thread
							//  (We set to a nice default, instead of 0,0, so that our window doesn't get "stuck" somewhere.)
							caretLatestPosition.x = 0;
							caretLatestPosition.y = 0;

							//Create and start our thread for tracking the caret
							caretTrackThread = CreateThread(
								NULL,                //Default security attributes
								0,                   //Default stack size
								UpdateCaretPosition, //Threaded function (name)
								NULL,                //Arguments to threaded function
								0,
								&caretTrackThreadID);//Pointer to return the thread's id into
							if (caretTrackThread==NULL) {
								MessageBox(NULL, _T("WaitZar could not create a helper thread. \nThis will not affect normal operation; however, it means that we can't track the caret."), _T("Warning"), MB_ICONWARNING | MB_OK);
								experimentalTextCursorTracking = FALSE;
							}

							//Wait for it.
							WaitForSingleObject(caretTrackThread, 1000);

							//Close it 
							CloseHandle(caretTrackThread);

							//Ready?
							if (caretLatestPosition.x!=0 && caretLatestPosition.y!=0) {
								//Line up our windows
								MoveWindow(mainWindow, caretLatestPosition.x, caretLatestPosition.y, WINDOW_WIDTH, WINDOW_HEIGHT, FALSE);
								MoveWindow(senWindow, caretLatestPosition.x, caretLatestPosition.y+WINDOW_HEIGHT, SUB_WINDOW_WIDTH, SUB_WINDOW_HEIGHT, FALSE);
							}
						}


						//Show it
						if (typePhrases==FALSE || !subWindowIsVisible) {
							//Turn on control keys
							turnOnControlkeys(TRUE);
							ShowBothWindows(SW_SHOW);
						} else {
							ShowWindow(mainWindow, SW_SHOW);
							mainWindowIsVisible = true;
						}
					}

					//Now, handle the keypress as per the usual...
					TCHAR keyStr[50];
					lstrcpy(keyStr, currStr);
					swprintf(currStr, _T("%s%c"), keyStr, keyCode);
					recalculate();

					keyWasUsed = true;
				}
			}



			//If this letter/number/etc. wasn't processed, see if we can type any of our
			// system-defined keys
			if (numCode==-1)
				numCode = wParam;
			else
				numCode = HOTKEY_0 + numCode;
			if (!helpWindowIsVisible && !mainWindowIsVisible && !keyWasUsed) {
				int newID = -1;
				for (size_t i=0; i<systemWordLookup.size(); i++) {
					if (systemWordLookup[i]->first==numCode) {
						newID = i;
						break;
					}
				}

				//Did we get anything?
				if (newID!=-1) {
					newID = -1-newID;
					
					//Try to type this word
					BOOL typed = selectWord(newID, true);
					if (typed==TRUE && typePhrases==TRUE) {
						//ShowWindow(mainWindow, SW_HIDE);
						//mainWindowIsVisible = false;
						//model->reset(false);
						//lstrcpy(currStr, _T(""));

						if (!subWindowIsVisible) {
							turnOnControlkeys(TRUE);

							ShowWindow(senWindow, SW_SHOW);
							subWindowIsVisible = true;
						}

						recalculate();
					}

					//We need to reset the trigrams here...
					sentence->updateTrigrams(model);
				}
			}

			break;
		}
		case WM_PAINT:
		{
			//Update only if there's an area which needs updating (e.g., a higher-level
			//  window has dragged over this one's client area... it can happen only with popups,
			//  but let's do it just to be safe.
			RECT updateRect;
			if (GetUpdateRect(hwnd, &updateRect, FALSE) != 0)
			{
				//Blitting every tick will slow us down... we should validate the
				//  rectangle after drawing it.
				reBlit(updateRect);

				//Validate the client area
				ValidateRect(hwnd, NULL);
			}

			break;
		}
		/*case WM_NCHITTEST: //Allow dragging of the client area...
		{
			LRESULT uHitTest = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);
			if(uHitTest == HTCLIENT) {
				return HTCAPTION;
			} else
				return uHitTest;
			break;
		}*/
		case WM_LBUTTONDOWN:
		{
			//Thanks to dr. Carbon for suggesting this method.
			if (SetCapture(hwnd)!=NULL)
				break;

			//Drag the mosue
			isDragging = true;
			GetCursorPos(&dragFrom);
			break;
		}
		case WM_MOUSEMOVE:
		{
			//Allow dragging of the mouse by its client area. Reportedly more accurate than NCHIT_TEST
			if (isDragging) {
				RECT rect;
				POINT dragTo;
				GetWindowRect(hwnd, &rect);
				GetCursorPos(&dragTo);
				
				//Constantly update its position
				MoveWindow(hwnd, (dragTo.x - dragFrom.x) + rect.left,
					(dragTo.y - dragFrom.y) + rect.top,
					rect.right - rect.left, rect.bottom - rect.top, FALSE);

				dragFrom = dragTo;
			}
			break;
		}
		case WM_LBUTTONUP:
		{
			if (isDragging) {
				isDragging = false;
				ReleaseCapture();
			}
			break;
		}
		case UWM_SYSTRAY: //Custom callback for our system tray icon
		{
			POINT pt;
			HMENU hmenu, hpopup;

			if (lParam==WM_RBUTTONUP || lParam==WM_LBUTTONUP) {
				//Make a popup menu
				GetCursorPos(&pt);
				hmenu = LoadMenu(hInst, MAKEINTRESOURCE(WZ_MENU));
				hpopup = GetSubMenu(hmenu, 0);


				//Note: set our text apropriately:
				TCHAR temp[200];
				UINT flagE = mmOn ? 0 : MF_CHECKED;
				UINT flagM = mmOn ? MF_CHECKED : 0;
				swprintf(temp, _T("English (%s)"), langHotkeyString);
				ModifyMenu(hmenu, IDM_ENGLISH, MF_BYCOMMAND|flagE, IDM_ENGLISH, temp);
				swprintf(temp, _T("Myanmar (%s)"), langHotkeyString);
				ModifyMenu(hmenu, IDM_MYANMAR, MF_BYCOMMAND|flagM, IDM_MYANMAR, temp);

				//Set checks for our sub-menus:
				UINT flagU = model->getOutputEncoding()==ENCODING_UNICODE ? MF_CHECKED : 0;
				UINT flagZ = model->getOutputEncoding()==ENCODING_ZAWGYI ? MF_CHECKED : 0;
				UINT flagW = model->getOutputEncoding()==ENCODING_WININNWA ? MF_CHECKED : 0;
				ModifyMenu(hmenu, ID_ENCODING_UNICODE5, MF_BYCOMMAND|flagU, ID_ENCODING_UNICODE5, POPUP_UNI);
				ModifyMenu(hmenu, ID_ENCODING_ZAWGYI, MF_BYCOMMAND|flagZ, ID_ENCODING_ZAWGYI, POPUP_ZG);
				ModifyMenu(hmenu, ID_ENCODING_WININNWA, MF_BYCOMMAND|flagW, ID_ENCODING_WININNWA, POPUP_WIN);

				//Set a check for the "Look Up Word" function
				UINT flagL = helpWindowIsVisible ? MF_CHECKED : 0;
				ModifyMenu(hmenu, IDM_LOOKUP, MF_BYCOMMAND|flagL, IDM_LOOKUP, POPUP_LOOKUP);


				//Cause our popup to appear in front of any other window.
				SetForegroundWindow(hwnd);

				//Force a track on this menu.
				int retVal = TrackPopupMenu(hpopup, //Which menu to track
                                 TPM_RETURNCMD |    //This code specifies that we return the ID of the selected menu item.
                                 TPM_RIGHTBUTTON,   //Track right mouse button
                                 pt.x, pt.y,        //Specifies the menu's location.
                                 0,                 //Reserved (MSDN: _Must_ be zero)
                                 hwnd,              //Owner
                                 NULL);            //MSDN: Ignored
				if (retVal == IDM_HELP) {
					//Properly handle hotkeys
					BOOL refreshControl = controlKeysOn;
					if  (refreshControl==TRUE)
						turnOnControlkeys(FALSE);

					//Show our box
					TCHAR temp[550];
					swprintf(temp, _T("WaitZar version %s - for more information, see: http://code.google.com/p/waitzar/\n\n%s - Switch between Myanmar and English\nType Burmese words like they sound, and press \"space\".\n\nWaitZar users should have the relevant fonts installed, if they want to see what they type after it's chosen.\nPlease see the User's Guide for more information."), WAIT_ZAR_VERSION, langHotkeyString);
					MessageBox(hwnd, temp, _T("About"), MB_ICONINFORMATION | MB_OK);

					//Hotkeys again
					if  (refreshControl==TRUE)
						turnOnControlkeys(TRUE);
				} else if (retVal == IDM_ENGLISH) {
					switchToLanguage(FALSE);

					//Reset the model
					sentence->clear();
					model->reset(true);
				} else if (retVal == IDM_MYANMAR) {
					switchToLanguage(TRUE);

					//Reset the model
					sentence->clear();
					model->reset(true);
				} else if (retVal == IDM_LOOKUP) {
					//Manage our help window
					if (mmOn==FALSE)
						switchToLanguage(TRUE);
					updateHelpWindow();
				} else if (retVal == IDM_EXIT) {
					DestroyWindow(hwnd);
				} else if (retVal == ID_ENCODING_UNICODE5) {
					setEncoding(ENCODING_UNICODE);
					recalculate();
				} else if (retVal == ID_ENCODING_ZAWGYI) {
					setEncoding(ENCODING_ZAWGYI);
					recalculate();
				} else if (retVal == ID_ENCODING_WININNWA) {
					setEncoding(ENCODING_WININNWA);
					recalculate();
				}

				//Fixes a bug re: MSKB article: Q135788
				PostMessage(hwnd, 0, 0, 0);

				//Reclaim resources
				DestroyMenu(hmenu);
			}

			break;
		}
		case WM_CTLCOLORDLG:
			//Our dialog has no background color.
			// (this might not actually have any effect)
			return NULL_BRUSH;
			break;
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			//Cleanup
			if (UnregisterHotKey(hwnd, LANG_HOTKEY) == FALSE)
				MessageBox(NULL, _T("Main Hotkey remains..."), _T("Warning"), MB_ICONERROR | MB_OK);
			if (mmOn==TRUE) {
				if (turnOnHotkeys(FALSE, true, true) == FALSE)
					MessageBox(NULL, _T("Some hotkeys remain..."), _T("Warning"), MB_ICONERROR | MB_OK);
			}

			//Remove systray icon
			NOTIFYICONDATA nid;
			nid.cbSize = sizeof(NOTIFYICONDATA);
			nid.hWnd = hwnd;
			nid.uID = STATUS_NID;
			nid.uFlags = NIF_TIP; //??? Needed ???
			Shell_NotifyIcon(NIM_DELETE, &nid);

			//Close our thread, delete our critical section
			if (highlightKeys) {
				DeleteCriticalSection(&threadCriticalSec);
				CloseHandle(keyTrackThread);
				CloseHandle(caretTrackThread);
			}

			//Log?
			if (isLogging) {
				fprintf(logFile, "WaitZar closed\n");
				fclose(logFile);
			}

			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}


BOOL turnOnHotkeys(BOOL on, bool affectLowercase, bool affectUppercase)
{
	int low_code;
	int high_code;
	BOOL retVal = TRUE;

	for (low_code=HOTKEY_A_LOW; low_code<=HOTKEY_Z_LOW; low_code++)
	{
		high_code = low_code - 32;
		if (on==TRUE)  {
			//Register this as an uppercase/lowercase letter
			if (affectUppercase) {
				if (RegisterHotKey(mainWindow, high_code, MOD_SHIFT, high_code)==FALSE)
					retVal = FALSE;
			}
			if (affectLowercase) {
				if (RegisterHotKey(mainWindow, low_code, NULL, high_code)==FALSE)
					retVal = FALSE;
			}
		} else {
			//De-register this as an uppercase/lowercase letter
			if (affectUppercase) {
				if (UnregisterHotKey(mainWindow, high_code)==FALSE)
					retVal = FALSE;
			}
			if (affectLowercase) {
				if (UnregisterHotKey(mainWindow, low_code)==FALSE)
					retVal = FALSE;
			}
		}
	}

	return retVal;
}



BOOL turnOnPunctuationkeys(BOOL on)
{
	BOOL retVal = true;

	if (on==TRUE) {
		//Punctuation keys
		if (RegisterHotKey(mainWindow, HOTKEY_COMMA, NULL, VK_OEM_COMMA)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_PERIOD, NULL, VK_OEM_PERIOD)==FALSE)
			retVal = FALSE;
	} else {
		//Additional punctuation keys
		if (UnregisterHotKey(mainWindow, HOTKEY_COMMA)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_PERIOD)==FALSE)
			retVal = FALSE;
	}

	//Return
	punctuationKeysOn = on;
	return retVal;
}


BOOL turnOnNumberkeys(BOOL on)
{
	BOOL retVal = true;

	//Register numbers
	if (on==TRUE) {
		//Numbers are no longer control keys.
		if (RegisterHotKey(mainWindow, HOTKEY_NUM0, NULL, VK_NUMPAD0)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_NUM1, NULL, VK_NUMPAD1)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_NUM2, NULL, VK_NUMPAD2)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_NUM3, NULL, VK_NUMPAD3)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_NUM4, NULL, VK_NUMPAD4)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_NUM5, NULL, VK_NUMPAD5)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_NUM6, NULL, VK_NUMPAD6)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_NUM7, NULL, VK_NUMPAD7)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_NUM8, NULL, VK_NUMPAD8)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_NUM9, NULL, VK_NUMPAD9)==FALSE)
			retVal = FALSE;
		for (int i=HOTKEY_0; i<=HOTKEY_9; i++) {
			if (RegisterHotKey(mainWindow, i, NULL, i)==FALSE)
				retVal = FALSE;
		}
	} else {
		//Numbers
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM0)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM1)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM2)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM3)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM4)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM5)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM6)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM7)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM8)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_NUM9)==FALSE)
			retVal = FALSE;
		for (int i=HOTKEY_0; i<=HOTKEY_9; i++) {
			if (UnregisterHotKey(mainWindow, i)==FALSE)
				retVal = FALSE;
		}
	}

	numberKeysOn = on;
	return retVal;
}


bool turnOnHelpKeys(bool on) 
{
	bool retVal = true;

	if (on) {
		//We'll keep our shifted hotkeys, but also add a hotkey for shift itself.
		//  We need to disambiguate the left and right shift keys later, since
		//  registering VK_LSHIFT and VK_RSHIFT doesn't seem to work
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT, MOD_SHIFT, VK_SHIFT)==FALSE)
			retVal = false;
	} else {
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT)==FALSE)
			retVal = false;
	}

	helpKeysOn = on;
	return retVal;
}


bool turnOnExtendedKeys(bool on) 
{
	bool retVal = true;

	//Register help keys
	if (on) {
		//Our combiner key (register shifted, too, to prevent errors)
		if (RegisterHotKey(mainWindow, HOTKEY_COMBINE, 0, VK_OEM_3)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_COMBINE, MOD_SHIFT, VK_OEM_3)==FALSE)
			retVal = false;

		//Various additional keyboard keys
		if (RegisterHotKey(mainWindow, HOTKEY_LEFT_BRACKET, 0, VK_OEM_4)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_LEFT_BRACKET, MOD_SHIFT, VK_OEM_4)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_RIGHT_BRACKET, 0, VK_OEM_6)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_RIGHT_BRACKET, MOD_SHIFT, VK_OEM_6)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_BACKSLASH, 0, VK_OEM_5)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_BACKSLASH, MOD_SHIFT, VK_OEM_5)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SEMICOLON, 0, VK_OEM_1)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_SEMICOLON, MOD_SHIFT, VK_OEM_1)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_APOSTROPHE, 0, VK_OEM_7)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_APOSTROPHE, MOD_SHIFT, VK_OEM_7)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_FORWARDSLASH, 0, VK_OEM_2)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_FORWARDSLASH, MOD_SHIFT, VK_OEM_2)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_COMMA, MOD_SHIFT, VK_OEM_COMMA)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_PERIOD, MOD_SHIFT, VK_OEM_PERIOD)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_SPACE, MOD_SHIFT, HOTKEY_SPACE)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_ENTER, MOD_SHIFT, VK_RETURN)==FALSE)
			retVal = FALSE;

		//Even though we won't use them, we should track them in our virtual keyboard
		if (RegisterHotKey(mainWindow, HOTKEY_MINUS, 0, VK_OEM_MINUS)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_MINUS, MOD_SHIFT, VK_OEM_MINUS)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_EQUALS, 0, VK_OEM_PLUS)==FALSE)
			retVal = false;
		if (RegisterHotKey(mainWindow, HOTKEY_SHIFT_EQUALS, MOD_SHIFT, VK_OEM_PLUS)==FALSE)
			retVal = false;


		//Number keys shifted
		for (int i=HOTKEY_SHIFT_0; i<=HOTKEY_SHIFT_9; i++) {
			if (RegisterHotKey(mainWindow, i, MOD_SHIFT, (i-HOTKEY_SHIFT_0)+HOTKEY_0)==FALSE)
				retVal = false;
		}
	} else {
		if (UnregisterHotKey(mainWindow, HOTKEY_COMBINE)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_COMBINE)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_LEFT_BRACKET)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_RIGHT_BRACKET)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_BACKSLASH)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_LEFT_BRACKET)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_RIGHT_BRACKET)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_BACKSLASH)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SEMICOLON)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_SEMICOLON)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_APOSTROPHE)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_APOSTROPHE)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_FORWARDSLASH)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_FORWARDSLASH)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_COMMA)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_PERIOD)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_MINUS)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_MINUS)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_EQUALS)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_EQUALS)==FALSE)
			retVal = false;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_SPACE)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_SHIFT_ENTER)==FALSE)
			retVal = FALSE;
		for (int i=HOTKEY_SHIFT_0; i<=HOTKEY_SHIFT_9; i++) {
			if (UnregisterHotKey(mainWindow, i))
				retVal = false;
		}
	}

	extendedKeysOn = on;
	return retVal;
}



BOOL turnOnControlkeys(BOOL on)
{
	BOOL retVal = true;

	//Register control keys
	if (on==TRUE) {
		if (RegisterHotKey(mainWindow, HOTKEY_SPACE, NULL, HOTKEY_SPACE)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_ENTER, NULL, VK_RETURN)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_LEFT, NULL, VK_LEFT)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_ESC, NULL, VK_ESCAPE)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_BACK, NULL, VK_BACK)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_DELETE, NULL, VK_DELETE)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_RIGHT, NULL, VK_RIGHT)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_UP, NULL, VK_UP)==FALSE)
			retVal = FALSE;
		if (RegisterHotKey(mainWindow, HOTKEY_DOWN, NULL, VK_DOWN)==FALSE)
			retVal = FALSE;
	} else {
		if (UnregisterHotKey(mainWindow, HOTKEY_SPACE)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_ENTER)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_LEFT)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_ESC)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_BACK)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_DELETE)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_RIGHT)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_UP)==FALSE)
			retVal = FALSE;
		if (UnregisterHotKey(mainWindow, HOTKEY_DOWN)==FALSE)
			retVal = FALSE;
	}

	controlKeysOn = on;
	return retVal;
}



void makeMainWindow(LPCWSTR windowClassName)
{
	//Set a window class's parameters
	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_SIZEALL);
	wc.hbrBackground = g_DarkGrayBkgrd;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = windowClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	if(!RegisterClassEx(&wc)) {
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	//Create a handle to the window
	// We have to use NOACTIVATE because, otherwise, typing text into a box that "selects all on refresh"
	// (like IE's address bar) is almost impossible. Unfortunately, this means our window will
	// receive very few actual events
	mainWindow = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_NOACTIVATE,
		windowClassName,
		_T("WaitZar"),
		WS_POPUP, //No border or title bar
		100, 100, WINDOW_WIDTH, WINDOW_HEIGHT,
		NULL, NULL, hInst, NULL
	);
}


void makeSubWindow(LPCWSTR windowClassName)
{
	if (typePhrases==FALSE)
		return;

	//Set a window class's parameters
	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = SubWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_SIZEALL);
	wc.hbrBackground = g_DarkGrayBkgrd;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = windowClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	if(!RegisterClassEx(&wc)) {
		MessageBox(NULL, _T("Sub-Window Registration Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	//Create a handle to the window
	// We have to use NOACTIVATE because, otherwise, typing text into a box that "selects all on refresh"
	// (like IE's address bar) is almost impossible. Unfortunately, this means our window will
	// receive very few actual events
	senWindow = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_NOACTIVATE,
		windowClassName,
		_T("WaitZar"),
		WS_POPUP, //No border or title bar
		100, 100+WINDOW_HEIGHT, SUB_WINDOW_WIDTH, SUB_WINDOW_HEIGHT,
		NULL, NULL, hInst, NULL
	);
}


void makeHelpWindow(LPCWSTR windowClassName)
{
	//Set a window class's parameters
	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = HelpWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_SIZEALL);
	wc.hbrBackground = g_GreenBkgrd;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = windowClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	if(!RegisterClassEx(&wc)) {
		MessageBox(NULL, _T("Help-Window Registration Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return;
	}

	//Create a handle to the window
	// We use LAYERED to allow for alpha blending on a per-pixel basis.
	//The MSDN docs say this might slow the program down, but I'll reserve
	// any optimizations until we have actual reported slowdown.
	helpWindow = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
		windowClassName,
		_T("WaitZar"),
		WS_POPUP, //No border or title bar
		400, 300, HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT,
		NULL, NULL, hInst, NULL
	);
}



/**
 * Borrowed from KeyMagic.
 */
BOOL IsAdmin()
{
   BOOL   fReturn         = FALSE;
   DWORD  dwStatus;
   DWORD  dwAccessMask;
   DWORD  dwAccessDesired;
   DWORD  dwACLSize;
   DWORD  dwStructureSize = sizeof(PRIVILEGE_SET);
   PACL   pACL            = NULL;
   PSID   psidAdmin       = NULL;

   HANDLE hToken              = NULL;
   HANDLE hImpersonationToken = NULL;

   PRIVILEGE_SET   ps;
   GENERIC_MAPPING GenericMapping;

   PSECURITY_DESCRIPTOR     psdAdmin           = NULL;
   SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;


   /*
      Determine if the current thread is running as a user that is a member of
      the local admins group.  To do this, create a security descriptor that
      has a DACL which has an ACE that allows only local aministrators access.
      Then, call AccessCheck with the current thread's token and the security
      descriptor.  It will say whether the user could access an object if it
      had that security descriptor.  Note: you do not need to actually create
      the object.  Just checking access against the security descriptor alone
      will be sufficient.
   */
   const DWORD ACCESS_READ  = 1;
   const DWORD ACCESS_WRITE = 2;


   __try
   {

      /*
         AccessCheck() requires an impersonation token.  We first get a primary
         token and then create a duplicate impersonation token.  The
         impersonation token is not actually assigned to the thread, but is
         used in the call to AccessCheck.  Thus, this function itself never
         impersonates, but does use the identity of the thread.  If the thread
         was impersonating already, this function uses that impersonation context.
      */
      if (!OpenThreadToken(GetCurrentThread(), TOKEN_DUPLICATE|TOKEN_QUERY,
		  TRUE, &hToken))
      {
         if (GetLastError() != ERROR_NO_TOKEN)
            __leave;

         if (!OpenProcessToken(GetCurrentProcess(),
			 TOKEN_DUPLICATE|TOKEN_QUERY, &hToken))
            __leave;
      }

      if (!DuplicateToken (hToken, SecurityImpersonation,
		  &hImpersonationToken))
		  __leave;


      /*
        Create the binary representation of the well-known SID that
        represents the local administrators group.  Then create the security
        descriptor and DACL with an ACE that allows only local admins access.
        After that, perform the access check.  This will determine whether
        the current user is a local admin.
      */
      if (!AllocateAndInitializeSid(&SystemSidAuthority, 2,
                                    SECURITY_BUILTIN_DOMAIN_RID,
                                    DOMAIN_ALIAS_RID_ADMINS,
                                    0, 0, 0, 0, 0, 0, &psidAdmin))
         __leave;

      psdAdmin = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
      if (psdAdmin == NULL)
         __leave;

      if (!InitializeSecurityDescriptor(psdAdmin,
		  SECURITY_DESCRIPTOR_REVISION))
         __leave;

      // Compute size needed for the ACL.
      dwACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) +
                  GetLengthSid(psidAdmin) - sizeof(DWORD);

      pACL = (PACL)LocalAlloc(LPTR, dwACLSize);
      if (pACL == NULL)
         __leave;

      if (!InitializeAcl(pACL, dwACLSize, ACL_REVISION2))
         __leave;

      dwAccessMask= ACCESS_READ | ACCESS_WRITE;

      if (!AddAccessAllowedAce(pACL, ACL_REVISION2, dwAccessMask,
		  psidAdmin))
         __leave;

      if (!SetSecurityDescriptorDacl(psdAdmin, TRUE, pACL, FALSE))
         __leave;

      /*
         AccessCheck validates a security descriptor somewhat; set the group
         and owner so that enough of the security descriptor is filled out to
         make AccessCheck happy.
      */
      SetSecurityDescriptorGroup(psdAdmin, psidAdmin, FALSE);
      SetSecurityDescriptorOwner(psdAdmin, psidAdmin, FALSE);

      if (!IsValidSecurityDescriptor(psdAdmin))
         __leave;

      dwAccessDesired = ACCESS_READ;

      /*
         Initialize GenericMapping structure even though you
         do not use generic rights.
      */
      GenericMapping.GenericRead    = ACCESS_READ;
      GenericMapping.GenericWrite   = ACCESS_WRITE;
      GenericMapping.GenericExecute = 0;
      GenericMapping.GenericAll     = ACCESS_READ | ACCESS_WRITE;

      if (!AccessCheck(psdAdmin, hImpersonationToken, dwAccessDesired,
                       &GenericMapping, &ps, &dwStructureSize, &dwStatus,
                       &fReturn))
      {
         fReturn = FALSE;
         __leave;
      }

   }

   __finally
   {
      // Clean up.
      if (pACL) LocalFree(pACL);
      if (psdAdmin) LocalFree(psdAdmin);
      if (psidAdmin) FreeSid(psidAdmin);
      if (hImpersonationToken) CloseHandle (hImpersonationToken);
      if (hToken) CloseHandle (hToken);
   }

   return fReturn;

}


bool IsVistaOrMore()
{
	OSVERSIONINFO OSversion;
	OSversion.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
	GetVersionEx(&OSversion);
	return (OSversion.dwMajorVersion>=6);
}


/**
 * Elevate and run a new instance of WaitZar
 */
void elevateWaitZar(LPCWSTR wzFileName)
{
	//Define our task
	SHELLEXECUTEINFO wzInfo;
    wzInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	wzInfo.fMask = 0;
	wzInfo.hwnd = NULL;
	wzInfo.lpVerb = _T("runas");
	wzInfo.lpFile = wzFileName;
	wzInfo.lpParameters = _T("runasadmin"); //Is this necessary?
    wzInfo.lpDirectory = NULL;
    wzInfo.nShow = SW_NORMAL;

	//Start the task
	if (ShellExecuteEx(&wzInfo) == FALSE) {
		MessageBox(NULL, _T("Could not elevate WaitZar. Program will now exit."), _T("Error!"), MB_ICONERROR | MB_OK);
	}
}



/**
 * Main method for Windows applications
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	//Save for later; if we try retrieving it, we'll just get a bunch of conversion
	//  warnings. Plus, the hInstance should never change.
	hInst = hInstance;
	mainWindowIsVisible = false;
	subWindowIsVisible = false;
	helpWindowIsVisible = false;
	mainInitDone = false;
	helpInitDone = false;

	//First and foremost
	PT_ORIGIN.x = 0;
	PT_ORIGIN.y = 0;
	helpIsCached = false;
	isDragging = false;

	//Also...
	buildSystemWordLookup();

	//Log?
	if (isLogging) {
		logFile = fopen("wz_log.txt", "w");
		if (logFile==NULL) {
			MessageBox(NULL, _T("Unable to open Log file"), _T("Warning"), MB_ICONWARNING | MB_OK);
			isLogging = false;
		} else {
			fprintf(logFile, "WaitZar was opened\n");
		}
	} else 
		logFile = NULL;
	waitzar::setLogFile(logFile);


	//Create a white/black brush
	g_WhiteBkgrd = CreateSolidBrush(RGB(255, 255, 255));
	g_DarkGrayBkgrd = CreateSolidBrush(RGB(128, 128, 128));
	g_YellowBkgrd = CreateSolidBrush(RGB(255, 255, 0));
	g_GreenBkgrd = CreateSolidBrush(RGB(0, 128, 0));
	g_GreenPen = CreatePen(PS_SOLID, 1, RGB(0, 128, 0));
	g_BlackPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
	g_EmptyPen = CreatePen(PS_NULL, 1, RGB(0, 0, 0));

	//Load our configuration file now; save some headaches later
	loadConfigOptions();

	//Should we run a UAC test on startup?
	if (alwaysRunElevated) {
		//Will elevating help?
		if (IsVistaOrMore() && !IsAdmin()) {
			TCHAR szCurFileName[1024];
            GetModuleFileName(GetModuleHandle(NULL), szCurFileName, 1023);
			elevateWaitZar(szCurFileName);
			return 0;
		}
	}

	//Give this process a low background priority
	//  NOTE: We need to balance this eventually.
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);

	//Create our windows
	makeMainWindow(_T("waitZarMainWindow"));
	makeSubWindow(_T("waitZarSentenceWindow"));
	makeHelpWindow(_T("waitZarHelpWindow"));

	//Our vector is used to store typed words for later...
	sentence = new SentenceList();

	//Load some icons...
	mmIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(ICON_WZ_MM), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR); //"Small Icons" are 16x16
	engIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(ICON_WZ_ENG), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR); //"Small Icons" are 16x16

	//Make our "notify icon" data structure
	NOTIFYICONDATA nid;
	nid.cbSize = sizeof(NOTIFYICONDATA); //natch
	nid.hWnd = mainWindow; //Cauess OUR window to receive notifications for this icon.
	nid.uID = STATUS_NID;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; //States that the callback message, icon, and size tip are used.
	nid.uCallbackMessage = UWM_SYSTRAY; //Message to send to our window
	nid.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(ICON_WZ_LOADING), IMAGE_ICON,
                        GetSystemMetrics(SM_CXSMICON),
                        GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR); //"Small Icons" are 16x16
	lstrcpy(nid.szTip, _T("WaitZar Myanmar Input System")); //Set tool tip text...

	//Error checking..
	if (mmIcon == NULL || engIcon==NULL)
		MessageBox(NULL, _T("Unable to load Icon!"), _T("Warning"), MB_ICONWARNING | MB_OK);

	//Set our hotkey
	if( registerInitialHotkey()==0 ) {
		//Check if we're running Wait Zar already
		if (waitzarAlreadyStarted()==TRUE) {
			MessageBox(NULL, _T("Wait Zar is already running. \n\nYou should see an \"ENG\" icon in your system tray; click on that to change the language. \n\nPlease see the Wait Zar User's Guide if you have any questions.  \n\n(If you are certain WaitZar is not actually running, please wait several minutes and then re-start the program.)"), _T("Wait Zar already running..."), MB_ICONINFORMATION | MB_OK);
			return 0;
		}
		MessageBox(NULL, _T("The main language shortcut could not be set up.\nWait Zar will not function properly."), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
	}
	mmOn = FALSE;

	//Edit: Add support for balloon tooltips
	if (showBalloonOnStart==TRUE) {
		nid.uFlags |= NIF_INFO;
		lstrcpy(nid.szInfoTitle, _T("Welcome to WaitZar"));
		swprintf(nid.szInfo, _T("Hit %ls to switch to Myanmar.\n\nClick here for more options."), langHotkeyString);
		nid.uTimeout = 20;
		nid.dwInfoFlags = NIIF_INFO; //Can we switch to NIIF_USER if supported?
	}

	//Add our icon to the tray
	Shell_NotifyIcon(NIM_ADD, &nid);


	//Initialize our keyboard input structures
	inputItems = new INPUT[1000];
	for (int i=0; i<1000; i++) {
		//We expect an input of type "keyboard"
		inputItems[i].type = INPUT_KEYBOARD;

		//Specify unicode (wVk MUST be zero)
		keyInputPrototype.dwFlags=KEYEVENTF_UNICODE;
		keyInputPrototype.wVk=0;

		//Have the system provide its own timestamp
		keyInputPrototype.time=0;

		//No extra info
		keyInputPrototype.dwExtraInfo=0;
	}

	//Initialize our romanisation string
	lstrcpy(currStr, _T(""));
	lstrcpy(currLetterSt, _T(""));

	//Success?
	if(mainWindow==NULL || (typePhrases==TRUE && senWindow==NULL) || helpWindow==NULL) {
		MessageBox(NULL, _T("Window Creation Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	//If we got this far, let's try to load our file.
	if (loadModel() == FALSE) {
		DestroyWindow(mainWindow);
		if (typePhrases==TRUE)
			DestroyWindow(senWindow);
		DestroyWindow(helpWindow);
		return 1;
	}

	//We might have a cached encoding level set...
	if (cachedEncoding!=-1) {
		model->setOutputEncoding(cachedEncoding);
		cachedEncoding = -1;
	}

	//Also load user-specific words
	readUserWords();

	//Did we get any?
	TCHAR noConfigWarningMsg[1500];
	lstrcpy(noConfigWarningMsg, _T(""));
	if (numConfigOptions==0) {
		lstrcpy(noConfigWarningMsg, _T("config.txt contained no valid configuration options."));
	}
	if (numCustomWords==0) {
		if (lstrlen(noConfigWarningMsg)==0)
			lstrcpy(noConfigWarningMsg, _T("mywords.txt contained no Burmese words."));
		else
			lstrcat(noConfigWarningMsg, _T(" Also, mywords.txt contained no Burmese words."));
	}
	if (lstrlen(noConfigWarningMsg)>0) {
		lstrcat(noConfigWarningMsg, _T("\nThis could be caused by a number of things:\n   + config.txt should be ASCII-encoded. mywords.txt should be UTF-8-encoded. Possibly you used another encoding?"));
		lstrcat(noConfigWarningMsg, _T("\n   + Perhaps you mis-spelled a configuration option. Check the Wait Zar manual to make sure you spelled each configuration option correctly.\n   + Maybe you commented out a line by mistake? The \"#\" key means to ignore a line."));
		lstrcat(noConfigWarningMsg, _T("\n   + Maybe your line-endings are wrong? Wait Zar can handle \\n OR \\r\\l (Windows or Linux) but that's all..."));
		lstrcat(noConfigWarningMsg, _T("\n\nIf you think this was caused by a bug in Wait Zar, please post an issue at http://code.google.com/p/waitzar/issues/list\n\nThis is just a warning --Wait Zar will still work fine without any config.txt or mywords.txt files."));
		MessageBox(NULL, noConfigWarningMsg, _T("Warning"), MB_ICONWARNING | MB_OK);
	}


	//Create (but don't start) our thread tracker
	if (highlightKeys) {
		//Initialize our critical section
		InitializeCriticalSection(&threadCriticalSec);
		threadIsActive = false;

		keyTrackThread = CreateThread(
			NULL,                //Default security attributes
			0,                   //Default stack size
			TrackHotkeyReleases, //Threaded function (name)
			NULL,                //Arguments to threaded function
			CREATE_SUSPENDED,    //Don't start this thread when it's created
			&keyTrackThreadID);  //Pointer to return the thread's id into
		if (keyTrackThread==NULL) {
			MessageBox(NULL, _T("WaitZar could not create a helper thread. \nThis will not affect normal operation; however, it means that WaitZar will not be able to highlight keys as you press them, which is a useful benefit for beginners."), _T("Warning"), MB_ICONWARNING | MB_OK);
			highlightKeys = FALSE;
		}
	}


	//TEMP_debug:
	/*if (isLogging) {
		model->setOutputEncoding(ENCODING_UNICODE);
		wchar_t* testStr = new wchar_t[100];
		wchar_t* testStrSort = new wchar_t[100];
		for (UINT i=0; i<500; i++) {
			std::vector<unsigned short> type = model->getWordKeyStrokes(i);
			for (size_t i=0; i<type.size(); i++) {
				testStr[i] = type[i];
				testStrSort[i] = type[i];
			}
			testStr[type.size()] = 0x0000;
			testStrSort[type.size()] = 0x0000;
			waitzar::sortMyanmarString(testStrSort);

			//Do the encodings compare?
			if (lstrcmp(testStr, testStrSort)!=0) {
				for (size_t x=0; x<wcslen(testStr); x++)
					fprintf(logFile, "%x", testStr[x]);
				fprintf(logFile, " !== ");
				for (size_t x=0; x<wcslen(testStrSort); x++)
					fprintf(logFile, "%x", testStrSort[x]);
				fprintf(logFile, "\n");
			}
		}
	}*/



	//Show it's ready by changing the shell icon
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = mainWindow;
	nid.uID = STATUS_NID;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; //States that the callback message, icon, and size tip are used.
	nid.uCallbackMessage = UWM_SYSTRAY; //Message to send to our window
	lstrcpy(nid.szTip, _T("WaitZar Myanmar Input System")); //Set tool tip text...
	nid.hIcon = engIcon;
	if (Shell_NotifyIcon(NIM_MODIFY, &nid) == FALSE) {
		TCHAR eTemp[200];
		swprintf(eTemp, _T("Can't load initial icon.\nError code: %x"), GetLastError());
		MessageBox(NULL, eTemp, _T("Warning"), MB_ICONERROR | MB_OK);
	}


	//Main message handling loop
	MSG Msg;
	while(GetMessage(&Msg, NULL, 0, 0) > 0) {
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	//Done
	return (int)Msg.wParam;
}


/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
