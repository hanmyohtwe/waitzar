/*
 * Copyright 2010 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#ifndef _BURGLISH_BUILDER
#define _BURGLISH_BUILDER

#include <vector>
#include <set>
#include <string>
#include <sstream>

#include "Input/burglish_data.h"
#include "Json Spirit/json_spirit_value.h"
#include "Json Spirit/json_spirit_reader.h"

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
	static void InitStatic();

	///////////////////////////////////////////////
	//Functionality expected in RomanInputMethod()
	///////////////////////////////////////////////
	
	//Key elements of BurglishBuilder
	bool typeLetter(char letter);
	void reset(bool fullReset);

	//Requires hacking (mostly b/c WordBuilder assumes word IDs)
	std::vector<unsigned int> getPossibleWords() const;
	std::wstring getWordString(unsigned int id) const;
	std::pair<int, std::string> reverseLookupWord(std::wstring word);

	//Requires copying of WordBuilder code. (Unfortunate, but unavoidable).
	bool backspace();
	bool moveRight(int amt);
	bool pageUp(bool up);
	std::pair<bool, unsigned int> typeSpace(int quickJumpID, bool useQuickJump);
	int getCurrPage() const;
	int getCurrSelectedID() const;
	int getNumberOfPages() const;
	unsigned short getStopCharacter(bool isFull) const;

	//Required by SentenceList
	void insertTrigram(const std::vector<unsigned int> &trigrams) { } //Burglish doesn't use trigrams.

	//Vestigial (performs no relevant work here)
	bool hasPatSintWord() const { return false; }
	unsigned int getFirstWordIndex() const { return 0; }
	std::wstring getParenString() const { return L""; }



private:
	static bool IsVowel(wchar_t letter);
	static bool IsValid(const std::wstring& word);
	static void addStandardWords(std::wstring roman, std::set<std::wstring>& resultsList);

	void reGenerateWordlist();

	//Borrowed from WordBuilder
	void setCurrSelected(int id);

private:
	static json_spirit::wmObject onsetPairs;
	static json_spirit::wmObject rhymePairs;

	std::wstringstream typedRomanStr;
	std::vector<std::wstring> savedWordIDs;
	std::vector<std::wstring> generatedWords;
	int currSelectedID;
	int currSelectedPage;


};

}//End of waitzar namespace


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

