/*
 * Copyright 2010 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#include "LetterInputMethod.h"

using std::vector;
using std::pair;
using std::string;
using std::wstring;



LetterInputMethod::LetterInputMethod()
{
}

LetterInputMethod::~LetterInputMethod()
{
}




void LetterInputMethod::handleEsc()
{
	if (this->isHelpInput()) {
		//Flag for removal.
		this->providingHelpFor = NULL;
	} else {
		//Cancle the current sentence if not in help mode
		typedSentenceStr.str(L"");
	}
}



void LetterInputMethod::handleBackspace(VirtKey& vkey)
{
	//Delete a letter (performs differently in help mode)
	if (this->isHelpInput()) {
		//If help mode, delete a letter but don't hide the window
		wstring newStr = !typedCandidateStr.str().empty() ? typedCandidateStr.str().substr(0, typedCandidateStr.str().length()-1) : L"";
		typedCandidateStr.str(L"");
		typedCandidateStr <<newStr;
		viewChanged = true;
	} else {
		// Otherwise, delete a letter from the sentece, and hide if nothing left
		wstring newStr = !typedSentenceStr.str().empty() ? typedSentenceStr.str().substr(0, typedSentenceStr.str().length()-1) : L"";
		typedSentenceStr.str(L"");
		typedSentenceStr <<newStr;
		viewChanged = true;
	}
}

void LetterInputMethod::handleDelete()
{
	//Delete the letter in front of you (encoding-wise, not visibly)
	//TODO: ADD LATER
}


void LetterInputMethod::handleLeftRight(bool isRight, bool loopToZero)
{
	//Move the letter cursor one to the right/left
	//TODO: ADD LATER
}

void LetterInputMethod::handleTab()
{
	//Move right.
	handleLeftRight(true, false);
}

void LetterInputMethod::handleUpDown(bool isDown)
{
	//For now, nothing.
}

//Ignore numCode, typeBurmeseNumbers
void LetterInputMethod::handleNumber(VirtKey& vkey, bool typeBurmeseNumbers)
{
	this->handleKeyPress(wParam, lParam, isUpper);
}

void LetterInputMethod::handleStop(bool isFull, VirtKey& vkey)
{
	//Perform the normal "enter" routine.
	typedStopChar = isFull ? L'\u104B' : L'\u'; //TODO: Make this based on encoding...
	requestToTypeSentence = true;
}

std::pair<int, std::string> LetterInputMethod::lookupWord(std::wstring typedWord)
{
	throw std::exception("Error: Cannot perform \"lookup word\" in a letter-based model.");
}

void LetterInputMethod::typeHelpWord(std::string roman, std::wstring myanmar, int currStrDictID)
{
	throw std::exception("Error: Cannot perform \"type help word\" in a letter-based model.");
}

void LetterInputMethod::handleCommit(bool strongCommit)
{
	//Anything to commit?
	if (typedSentenceStr.str().empty()) 
		return;

	//If we are in help mode, add the word we chose to the dictionary, and flag 
	//   it so that it can be cleared later when the entire sentence is selected.
	//If in normal mode, commit the entire sentence. 
	//TODO: The "model" stuff only works on the "providingHelpFor" window... we need to move
	//      some functions around (like word lookup, etc.). 
	if (!this->isHelpInput()) {
		//Just commit the current sentence.
		requestToTypeSentence = true;
	} else {
		//Get its romanization, if it exists.
		std::pair<int, std::string> wordData = providingHelpFor->lookupWord(typedCandidateStr.str());
		int currStrDictID = wordData.first;
		string revWord = (wordData.first!=-1) ? wordData.second : "<no entry>";

		//Add it to the memory list, dictionary, and current sentence.
		providingHelpFor->typeHelpWord(revWord, typedSentenceStr.str(), currStrDictID);

		//Flag for removal
		this->providingHelpFor = NULL;
	}
}



//False means nothing else to type
pair<wstring, bool> LetterInputMethod::appendTypedLetter(const std::wstring& prevStr, VirtKey& vkey)
{
	//Nothing to do?
	wstring nextBit = helpKeyboard->typeLetter(nextKeycode);
	if (nextBit.empty())
		return pair<wstring, bool>(prevStr, false);

	//Temp var
	wstring currStr = prevStr + nextBit;
	size_t len = currStr.length();

	//Special cases
	if (nextBit.length()==1 && nextBit[0]==L'\u1039') {
		//Combiner functions in reverse
		if (len>1 && canStack(currStr[len-2])) {
			currStr[len-1] = currStr[len-2];
			currStr[len-2] = nextBit[0];
		} else {
			currStr.erase(currStr.length()-1); //Not standard behavior, but let's avoid bad combinations.
		}
	} else if (nextBit == wstring(L"\u1004\u103A\u1039")) {
		//Kinzi can be typed after the consonant instead of before it.
		//For now, we only cover the general case of typing "kinzi" directly after a consonant
		if (len>3 && canStack(currStr[len-4])) {
			currStr[len-1] = currStr[len-4];
			currStr[len-4] = nextBit[0];
			currStr[len-3] = nextBit[1];
			currStr[len-2] = nextBit[2];
		}
	}

	return pair<wstring, bool>(currStr, true);
}



void LetterInputMethod::handleKeyPress(VirtKey& vkey)
{
	//Delegate our combination algorithm
	wstring curr = typedSentenceStr.str();
	if (isHelpInput())
		curr = typedCandidateStr.str();
	pair<wstring, bool> next = appendTypedLetter(curr, (wchar_t)(wParam&0xFFFF), wParam, lParam);

	//Did we change anything?
	if (next.second) {
		//Update
		if (isHelpInput()) {
			typedCandidateStr.str(L"");
			typedCandidateStr <<next.first;
		} else {
			typedSentenceStr.str(L"");
			typedSentenceStr <<next.first;
		}

		//Save a romanized string if in help mode
		if (this->isHelpInput()) {
			//Check each romanisation
			typedRomanStr.str(L"");
			string roman = providingHelpFor->lookupWord(typedCandidateStr.str()).second;
			if (!roman.empty())
				typedRomanStr <<L'(' <<roman.c_str() <<L')';
		}

		//Trigger view change.
		viewChanged = true;
	} else {
		//Check for system keys
		InputMethod::handleKeyPress(wParam, lParam, isUpper);
	}
}



vector<wstring> LetterInputMethod::getTypedSentenceStrings()
{
	vector<wstring> res;
	res.push_back(typedSentenceStr.str());
	res.push_back(L"");
	res.push_back(L"");
	res.push_back(typedSentenceStr.str());
	return res;
}


vector< pair<wstring, unsigned int> > LetterInputMethod::getTypedCandidateStrings()
{
	vector< pair<wstring, unsigned int> > res;
	res.push_back(pair<wstring, unsigned int>(typedCandidateStr.str(), 0));
	return res;
}


void LetterInputMethod::appendToSentence(wchar_t letter, int id)
{
	typedSentenceStr <<letter;
}


void LetterInputMethod::reset(bool resetCandidates, bool resetRoman, bool resetSentence, bool performFullReset)
{
	//A "full" reset entails the others
	if (performFullReset) {
		resetCandidates = resetRoman = resetSentence = true;
		userDefinedWords.clear();
	}

	if (resetCandidates)
		typedCandidateStr.str(L"");
	if (resetRoman)
		typedRomanStr.str(L"");
	if (resetSentence)
		typedSentenceStr.str(L"");
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
