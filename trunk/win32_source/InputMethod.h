/*
 * Copyright 2010 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#ifndef _INPUT_METHOD
#define _INPUT_METHOD

#include <string>
#include "MyWin32Window.h"
#include "RomanInputMethod.h"


//Expected interface: "Input Method"
class InputMethod {
public:
	InputMethod(MyWin32Window* mainWindow, MyWin32Window* sentenceWindow, MyWin32Window* helpWindow, MyWin32Window* memoryWindow, const vector< pair <int, unsigned short> > &systemWordLookup);
	virtual ~InputMethod();

	//Struct-like properties
	Option<std::wstring> displayName;
	Option<std::wstring> encoding;
	Option<TYPES> type;

	//Useful functionality
	void treatAsHelpKeyboard(RomanInputMethod* providingHelpFor);
	bool isHelpInput();
	void forceViewChanged();
	bool getAndClearViewChanged();
	bool getAndClearRequestToTypeSentence();

	//Keypress handlers (abstract virtual)
	virtual void handleEsc() = 0;
	virtual void handleBackspace() = 0;
	virtual void handleDelete() = 0;
	virtual void handleLeftRight(bool isRight) = 0;
	virtual void handleCommit(bool strongCommit) = 0;
	virtual void handleNumber(int numCode, WPARAM wParam) = 0;
	virtual void handleStop(bool isFull) = 0;

	virtual void handleKeyPress(WPARAM wParam);


private:
	const vector< pair <int, unsigned short> > &systemWordLookup;


protected:
	//Window control
	MyWin32Window* mainWindow;
	MyWin32Window* sentenceWindow;
	MyWin32Window* helpWindow;
	MyWin32Window* memoryWindow;

	//Helper typing control
	RomanInputMethod* providingHelpFor;

	//Repaint after this?
	bool viewChanged;
	bool requestToTypeSentence;


public:  //Abstract methods

	//Is this class a placeholder, or is it a real IM?
	//   There are other ways to do this, but it's nice to 
	//   have a way of double-checking.
	virtual bool isPlaceholder() = 0; 

	//Get strings to print, always in unicode
	//The current typed string (sentence string). 
	//This will be typed to the output program if "Enter" is pressed.
	//All sub-classes must ensure that this remains valid outside of any function call, and
	//  before any calls to the base class's methods.
	//A "typed" string is in an arbitrary encoding; converting to/from unicode is done as needed.
	//The "Sentence pre-cursor string" is the current string that's been typed so far, before the cursor.
	virtual std::wstring getTypedSentenceString() = 0;
	virtual std::wstring getSentencePreCursorString() = 0;
	virtual void appendToSentence(wchar_t letter, int id) = 0; //Used for system letters only, for perf. reasons. (id is optional)

	//The current "candidate" string, which will be displayed in the top
	//  window. It will not be entered until it also appears in the sentence string.
	//The same warnings apply as to the typedSentenceString.
	//NOTE: for now, the "int" part is just the highlight level: 1 for red and 2 for green (3 for both, which means green)
	//      TODO: Make this cleaner.
	virtual std::vector< std::pair<std::wstring, unsigned int> > getTypedCandidateStrings() = 0;


	//Get the typed romanized string. This consists ONLY of all typed valid letters
	virtual std::wstring getTypedRomanString() = 0;

	//Called periodically
	virtual void reset(bool resetCandidates, bool resetRoman, bool resetSentence, bool performFullReset) = 0;

private:
	//Must be maintained by the subclass
	std::wstringstream typedRomanStr;
};


#endif //_INPUT_METHOD

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

