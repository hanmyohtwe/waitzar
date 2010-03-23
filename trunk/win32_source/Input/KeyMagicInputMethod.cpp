/*
 * Copyright 2010 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#include "KeyMagicInputMethod.h"

using std::vector;
using std::pair;
using std::string;
using std::wstring;


KeyMagicInputMethod::KeyMagicInputMethod()
{
}

KeyMagicInputMethod::~KeyMagicInputMethod()
{
}


void KeyMagicInputMethod::loadRulesFile(const string& rulesFilePath)
{
	vector<wstring> lines;

	{
		//Step 1: Load the file, convert to wchar_t*, skip the BOM
		size_t i = 0;
		wstring datastream = waitzar::readUTF8File(rulesFilePath);
		if (datastream[i] == L'\uFEFF')
			i++;
		else if (datastream[i] == L'\uFFFE')
			throw std::exception("KeyMagicInputMethod rules file  appears to be encoded backwards.");


		//First, parse into an array of single "rules" (or variables), removing 
		//  comments and combining multiple lines, etc.
		size_t sz = datastream.size();
		std::wstringstream line;
		wchar_t lastChar = L'\0';
		for (; i<sz; i++) {
			//Skip comments: C-style
			if (i+1<sz && datastream[i]==L'/' && datastream[i+1]==L'*') {
				i += 2;
				while (i<sz && (datastream[i]!=L'/' || datastream[i-1]!=L'*'))
					i++;
				continue;
			}
			//Skip comments: Cpp-style
			if (i+1<sz && datastream[i]==L'/' && datastream[i+1]==L'/') {
				i += 2;
				while (i<sz && datastream[i]!=L'\n')
					i++;
				continue;
			}

			//Skip \r
			if (datastream[i]==L'\r')
				continue;

			//Newline causes a line break 
			if (datastream[i]==L'\n') {
				//...unless preceeded by "\"
				if (lastChar=='\\') {
					lastChar = L'\0';
					continue;
				}

				if (!line.str().empty())
					lines.push_back(line.str());
				line.str(L"");
				lastChar = L'\0';
				continue;
			}

			//Anything else is just appended and saved (except \\ in some cases, and ' ' and '\t' in others)
			lastChar = datastream[i];
			if (lastChar==L'\t') //Replace tab with space
				lastChar = L' ';
			if (lastChar==L'\\' && ((i+1<sz && datastream[i+1]==L'\n')||(i+2<sz && datastream[i+1]==L'\r' && datastream[i+2]==L'\n')))
				continue;
			if (lastChar==L' ' && i>0 && (datastream[i-1]==L' '||datastream[i-1]==L'\t')) //Remove multiple spaces
				continue;
			line <<lastChar;

			//Last line?
			if (i==sz-1 && !line.str().empty())
				lines.push_back(line.str());
		}
	}

	//Now, turn the rules into an understandable set of data structures
	//We have:
	//   $variable = rule+
	//   rule+ => rule+
	std::wstringstream rule;
	for (size_t i=0; i<lines.size(); i++) {
		wstring line = lines[i];
		rule.str(L"");

		//Break the current line into a series of:
		// item + ...+ item (=|=>) item +...+ item
		//Then apply logic to each individual item
		int separator = 0; //1 for =, 2 for =>
		size_t sepIndex = 0;
		vector<Rule> allRules;
		wchar_t currQuoteChar = L'\0';
		for (size_t i=0; i<line.size(); i++) {
			//Separator?
			if (line[i] == L'=') {
				//= or =>
				separator = 1;
				if (i+i<line.size() && line[i+1] == L'>') {
					separator = 2;
					i++;
				}
				sepIndex = allRules.size()+1;
				continue;
			}

			//Whitespace?
			if (iswspace(line[i]))
				continue;

			//String?
			if (line[i]==L'"' || line[i]==L'\'') {
				if (currQuoteChar==L'\0')
					currQuoteChar = line[i];
				else if (line[i]==currQuoteChar && (i==0 || line[i-1]!=L'\\'))
					currQuoteChar = L'\0';
			}

			//Append the letter?
			if (line[i]!=L'+' || currQuoteChar!=L'\0')
				rule <<line[i];

			//New rule?
			if ((line[i]==L'+' || i==line.size()-1) && !rule.str().empty()) {
				//Interpret
				try {
					allRules.push_back(parseRule(rule.str()));
				} catch (std::exception ex) {
					std::wstringstream err;
					err <<ex.what();
					err <<"\nRule:\n";
					err <<line;
					throw std::exception(waitzar::escape_wstr(err.str(), false).c_str());
				}

				//Reset
				rule.str(L"");
			}
		}

		//Interpret and add it
		if (separator==0)
			throw std::exception(ConfigManager::glue(L"Error: Rule does not contain = or =>: \n", line).c_str());
		try {
			addSingleRule(allRules, sepIndex, separator==1);
		} catch (std::exception ex) {
			std::wstringstream err;
			err <<ex.what();
			err <<"\nRule:\n";
			err <<line;
			throw std::exception(waitzar::escape_wstr(err.str(), false).c_str());
		}
	}
}


int KeyMagicInputMethod::hexVal(wchar_t letter)
{
	switch(letter) {
		case L'0':  case L'1':  case L'2':  case L'3':  case L'4':  case L'5':  case L'6':  case L'7':  case L'8':  case L'9':
			return letter-L'0';
		case L'a':  case L'b':  case L'c':  case L'd':  case L'e':  case L'f':
			return letter-L'a' + 10;
		default:
			return -1;
	}
}


Rule KeyMagicInputMethod::parseRule(const std::wstring& ruleStr)
{
	//Initialize a suitable structure for our results.
	Rule result = Rule(KMRT_UNKNOWN, L"", 0);

	//Detection of type is a bit ad-hoc for now. At least, we need SOME length of string
	if (ruleStr.empty())
		throw std::exception("Error: Cannot create a rule from an empty string");
	wstring ruleLowercase = ruleStr;
	waitzar::loc_to_lower(ruleLowercase);

	//KMRT_VARIABLE: Begins with a $, must be followed by [a-zA-Z0-9_]
	//KMRT_MATCHVAR: Same, but ONLY numerals
	//KMRT_VARARRAY: Ends with "[" + ([0-9]+) + "]"
	//KMRT_VARARRAY_SPECIAL: Ends with "[" + [\*\^] + "]"
	//KMRT_VARARRAY_BACKREF: Ends with "[" + "$" + ([0-9]+) + "]"
	if (ruleStr[0] == L'$') {
		//Validate
		int numberVal = 0;
		int bracesStartIndex = -1;
		for (size_t i=1; i<ruleStr.length(); i++) {
			//First part: letters or numbers only
			wchar_t c = ruleStr[i];
			if (c>=L'0'&&c<=L'9') {
				if (numberVal != -1) {
					numberVal *= 10;
					numberVal += (c - L'0');
				}
				continue;
			}
			if ((c>=L'a'&&c<=L'z') || (c>=L'A'&&c<=L'Z') || (c==L'_')) {
				numberVal = -1;
				continue;
			}

			//Switch?
			if (c==L'[') {
				if (numberVal!=-1)
					throw std::exception("Invalid variable: cannot subscript number constants");

				//Flag for further parsing
				bracesStartIndex = i;
				break;
			}

			//Else, error
			throw std::exception("Invalid variable letter: should be alphanumeric.");
		}

		//No further parsing?
		if (bracesStartIndex==-1) {
			//Save
			if (numberVal != -1) {
				result.str = ruleStr;
				result.type = KMRT_MATCHVAR;
				result.val = numberVal;
			} else {
				result.str = ruleStr;
				result.type = KMRT_VARIABLE;
			}
		} else {
			//It's a complex variable.
			if (bracesStartIndex==ruleStr.length()-1 || ruleStr[ruleStr.length()-1]!=L']')
				throw std::exception("Invalid variable: bracket isn't closed.");
			result.str = ruleStr.substr(0, bracesStartIndex);
			
			//Could be [*] or [^]; check these first
			if (bracesStartIndex+1==ruleStr.length()-2 && (ruleStr[bracesStartIndex+1]==L'^' || ruleStr[bracesStartIndex+1]==L'*')) {
				result.type = KMRT_VARARRAY_SPECIAL;
				result.val = ruleStr[bracesStartIndex+1];
			} else if (ruleStr[bracesStartIndex+1]==L'$') {
				//It's a variable reference
				int total = 0;
				for (size_t id=ruleStr[bracesStartIndex+2]; id<ruleStr.length()-1; id++) {
					if (ruleStr[id]>=L'0' && ruleStr[id]<=L'9') {
						total *= 10;
						total += (ruleStr[id]-L'0');
					} else 
						throw std::exception("Invalid variable: bracket variable contains non-numeric ID characters.");
				}
				if (total==0)
					throw std::exception("Invalid variable: bracket variable has no ID.");

				//Save
				result.type = KMRT_VARARRAY_BACKREF;
				result.val = total;
			} else {
				//It's a simple ID reference
				int total = 0;
				for (size_t id=ruleStr[bracesStartIndex+1]; id<ruleStr.length()-1; id++) {
					if (ruleStr[id]>=L'0' && ruleStr[id]<=L'9') {
						total *= 10;
						total += (ruleStr[id]-L'0');
					} else 
						throw std::exception("Invalid variable: bracket id contains non-numeric ID characters.");
				}
				if (total==0)
					throw std::exception("Invalid variable: bracket id has no ID.");

				//Save
				result.type = KMRT_VARARRAY;
				result.val = total;
			}
		}
	}


	//KMRT_STRING: Enclosed with ' or "
	else if (ruleStr.size()>1 && ((ruleStr[0]==L'\''&&ruleStr[ruleStr.size()-1]==L'\'') || ((ruleStr[0]==L'"'&&ruleStr[ruleStr.size()-1]==L'"')))) {
		//Escaped strings should already have been taken care of.
		result.type = KMRT_STRING;
		result.str = ruleStr.substr(1, ruleStr.length()-2);
	}


	//KMRT_STRING: We translate NULL/null to empty strings
	else if (ruleLowercase == L"null") {
		result.type = KMRT_STRING;
		result.str = L"";
	}


	//KMRT_STRING: Unicode letters are converted here
	else if (ruleStr.length()==5 && ruleLowercase[0]==L'u' && hexVal(ruleLowercase[1])!=-1 && hexVal(ruleLowercase[2])!=-1 && hexVal(ruleLowercase[3])!=-1 && hexVal(ruleLowercase[4])!=-1) {
		result.type = KMRT_STRING;
		result.str = L"X";

		//Convert
		result.val = hexVal(ruleLowercase[1])*0x1000 + hexVal(ruleLowercase[2])*0x100 + hexVal(ruleLowercase[3])*0x10 + hexVal(ruleLowercase[4]);
		result.str[0] = (wchar_t)result.val;
	}

	
	//KMRT_WILDCARD: The * wildcard
	else if (ruleStr == L"*") {
		result.type = KMRT_WILDCARD;
		result.str = L"*";
	}


	//KMRT_SWITCH: Enclosed in (....); switches are also named with quotes (single or double)
	else if (ruleStr.size()>1 && ruleStr[0]==L'(' && ruleStr[ruleStr.size()-1]==L')') {
		//Ensure proper quotation (and something inside
		if (ruleStr.size()>4 && ((ruleStr[1]==L'\''&&ruleStr[ruleStr.size()-2]==L'\'') || (ruleStr[1]==L'"'&&ruleStr[ruleStr.size()-2]==L'"'))) {
			result.type = KMRT_SWITCH;
			result.str = ruleStr.substr(2, ruleStr.length()-4);
		} else 
			throw std::exception("Bad 'switch' type rule");
	}
	

	//KMRT_KEYCOMBINATION: <VK_SHIFT & VK_T>
	else if (ruleStr.size()>2 && ruleStr[0]==L'<' && ruleStr[1]==L'>') {
		//Read each value into an array
		vector<wstring> vkeys;
		std::wstringstream currKey;
		for (size_t id=0; id<ruleStr.length(); id++) {
			//Append?
			if ((ruleStr[id]>='A'&&ruleStr[id]<='Z') || (ruleStr[id]>='0'&&ruleStr[id]<='9') || ruleStr[id]==L'_')
				currKey <<ruleStr[id];

			//New key?
			if ((ruleStr[id]==L'&' || ruleStr[id]==L'>') && !ruleStr.empty()) {
				vkeys.push_back(currKey.str());
				currKey.str(L"");
			}
		}

		//Handle the final key
		result.type = KMRT_KEYCOMBINATION;
		result.val = -1;
		if (vkeys.empty())
			throw std::exception("Invalid VKEY: nothing specified");
		for (size_t id=0; !KeyMagicVKeys[id].keyName.empty(); id++) {
			if (KeyMagicVKeys[id].keyName == vkeys[vkeys.size()-1]) {
				result.val = KeyMagicVKeys[id].keyValue;
				break;
			}
		}
		if (result.val == -1)
			throw std::exception("Unknown VKEY specified");

		//Now, handle all modifiers
		for (size_t id=0; id<vkeys.size()-1; id++) {
			if (vkeys[id] == L"VK_SHIFT") {
				//TODO
				//TODO: specify all combined values, append it to result.val
				//TODO
			} else {
				throw std::exception("Unknown VKEY specified as modifier");
			}
		}
	}

	//Done?
	if (result.type==KMRT_UNKNOWN)
		throw std::exception("Error: Unknown rule type");
	return result;
}



//Add a rule to our replacements/variables
void KeyMagicInputMethod::addSingleRule(const vector<Rule>& rules, size_t rhsStart, bool isVariable)
{
	//Simple checks 1,2: Variables are correct
	if (isVariable && rhsStart!=1)
		throw std::exception("Rule error: Variables cannot have multiple assignments left of the parentheses");
	if (isVariable && rules[0].type!=KMRT_VARIABLE)
		throw std::exception("Rule error: Assignment ($x = y) can only assign into a variable.");



}




wstring KeyMagicInputMethod::applyRules(const wstring& input)
{
	return NULL;
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
