/*
 * Copyright 2010 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#ifndef _BURGLISH_BUILDER
#define _BURGLISH_BUILDER

#include <vector>
#include <string>

namespace waitzar
{

/* 
 * This class is for templatized replacement of WordBuilder.
 */

class BurglishBuilder
{
public:
	//Basic
	BurglishBuilder();
	~BurglishBuilder();

	///////////////////////////////////////////////
	//Functionality expected in RomanInputMethod()
	///////////////////////////////////////////////
	
	//Key elements of BurglishBuilder
	bool typeLetter(char letter);
	std::vector<unsigned int> getPossibleWords() const;
	void reset(bool fullReset);

	//Requires hacking (mostly b/c WordBuilder assumes word IDs)
	std::wstring getWordString(unsigned int id) const;
	std::pair<int, std::string> reverseLookupWord(std::wstring word);
	int getCurrSelectedID() const;

	//Requires copying of WordBuilder code. (Unfortunate, but unavoidable).
	std::pair<bool, unsigned int> typeSpace(int quickJumpID, bool useQuickJump);
	bool backspace();
	bool moveRight(int amt);
	bool pageUp(bool up);
	int getCurrPage() const;
	int getNumberOfPages() const;
	unsigned short getStopCharacter(bool isFull) const;

	//Vestigial (performs no relevant work here)
	bool hasPatSintWord() const { return false; }
	unsigned int getFirstWordIndex() const { return 0; }
	std::wstring getParenString() const { return L""; }



private:
	std::vector<std::wstring> generatedWords;






}

#endif //_BURGLISH_BUILDER



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

