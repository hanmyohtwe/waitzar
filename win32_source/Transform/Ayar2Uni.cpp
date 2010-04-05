/*
 * Copyright 2010 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#include "Ayar2Uni.h"

Ayar2Uni::Ayar2Uni()
{
}


//Very general
bool Ayar2Uni::IsConsonant(wchar_t letter) const
{
	if (letter>=L'\u1000' && letter<=L'\u102A')
		return true;
	if (letter==L'\u103F')
		return true;
	if (letter>=L'\u1040' && letter<=L'\u1049')
		return true;
	if (letter==L'\u104E')
		return true;
	return false;
}


//Convert
void Ayar2Uni::convertInPlace(std::wstring& src) const
{
	//Temporary algorithm: just split and move kinzi + tha-way-htoe + ya-yit
	std::wstringstream res;
	std::wstringstream currSyllable;
	std::wstringstream currSyllablePrefix;
	for (size_t i=0; i<src.length(); i++) {
		//The next syllable starts at the first non-stacked non-killed consonant, or at tha-way-htoe or ya-yit
		if (src[i]<L'\u1000' || src[i]>L'\u109F') {
			//Append all non-Myanmar letters and continue
			res <<currSyllablePrefix.str() <<currSyllable.str();
			currSyllablePrefix.str(L"");
			currSyllable.str(L"");
			while (i<src.length() && (src[i]<L'\u1000' || src[i]>L'\u109F')) {
				res <<src[i++];
			}
			i--;
			continue;
		}

		//Are we at the boundary of a new word?
		bool boundary = false;
		if (src[i]==L'\u1031' || src[i]==L'\u103C')
			boundary = true;
		else if (IsConsonant(src[i])) {
			boundary = true;
			if (src[i]==L'\u1004' && i+2<src.length() && src[i+1]==L'\u103A' && src[i+2]==L'\u1039')
				boundary = false; //Kinzi
			if (i>0 && src[i-1]==L'\u1039')
				boundary = false; //Stacked
			else if (i+1<src.length() && src[i+1]==L'\u103A')
				boundary = false; //Killed
		}
		if (boundary) {
			res <<currSyllablePrefix.str() <<currSyllable.str();
			currSyllablePrefix.str(L"");
			currSyllable.str(L"");
		}

		//Now, collect as usual.
		if (src[i]==L'\u1004' && i+2<src.length() && src[i+1]==L'\u103A' && src[i+2]==L'\u1039') {
			res <<L"\u1004\u103A\u1039";
			i+=2;
		} else if (IsConsonant(src[i]) && currSyllablePrefix.str().empty()) //Only the first consonant is the prefix.
			currSyllablePrefix <<src[i];
		else
			currSyllable <<src[i];

		if (i==src.length()-1) {
			res <<currSyllablePrefix.str() <<currSyllable.str();
			currSyllablePrefix.str(L"");
			currSyllable.str(L"");
		}
	}

	src = res.str();
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
