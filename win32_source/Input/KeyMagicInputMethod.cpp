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
	map< wstring, unsigned int> tempVarLookup;
	map< wstring, unsigned int> tempSwitchLookup;
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
				if (i+1<line.size() && line[i+1] == L'>') {
					separator = 2;
					i++;
				}
				sepIndex = allRules.size()+1;

				//Interpret
				//TODO: Put this in a central place.
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
			//NOTE: This won't add the rule if there are ignorable characters (e.g., whitespace) at the end of the line
			if ((line[i]==L'+' || i==line.size()-1) && !rule.str().empty() && currQuoteChar==L'\0') {
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


		//Any leftover rules? (To-do: centralize)
		if (!rule.str().empty()) {
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


		//Interpret and add it
		if (separator==0)
			throw std::exception(ConfigManager::glue(L"Error: Rule does not contain = or =>: \n", line).c_str());
		try {
			addSingleRule(allRules, tempVarLookup, tempSwitchLookup, sepIndex, separator==1);
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

	try {
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
					for (size_t id=bracesStartIndex+2; id<ruleStr.length()-1; id++) {
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
			//Escaped strings should already have been taken care of (but not translated)
			std::wstringstream buff;
			for (size_t i=1; i<ruleStr.length()-1; i++) {
				//Normal letters
				if (ruleStr[i]!=L'\\') {
					buff <<ruleStr[i];
					continue;
				}

				//Escape sequences
				// Can be: \\, \", \uXXXX
				if (i+1<ruleStr.length()-1 && ruleStr[i+1]==L'\\') {
					buff <<L'\\';
					i++;
				} else if (i+1<ruleStr.length()-1 && ruleStr[i+1]==L'"') {
					buff <<L'"';
					i++;
				} else if (i+5<ruleStr.length()-1 && (ruleStr[i+1]==L'u'||ruleStr[i+1]==L'U') && hexVal(ruleStr[i+2])!=-1 && hexVal(ruleStr[i+3])!=-1 && hexVal(ruleStr[i+4])!=-1 && hexVal(ruleStr[i+5])!=-1) {
					int num = hexVal(ruleStr[i+2])*0x1000 + hexVal(ruleStr[i+3])*0x100 + hexVal(ruleStr[i+4])*0x10 + hexVal(ruleStr[i+5]);
					buff <<(wchar_t)num;
					i += 5;
				} else
					throw std::exception(ConfigManager::glue(L"Invalid escape sequence in: ", ruleStr).c_str());
			}
			result.type = KMRT_STRING;
			result.str = buff.str();
		}


		//KMRT_STRING: We translate NULL/null to empty strings
		else if (ruleLowercase == L"null") {
			result.type = KMRT_STRING;
			result.str = L"";
		}


		//KMRT_STRING: The VK_* keys can be handled as single-character strings
		else if (ruleStr.size()>3 && ruleStr[0]==L'V' && ruleStr[1]==L'K' && ruleStr[2]==L'_') {
			result.type = KMRT_STRING;
			result.val = -1;
			result.str = L"X";
			for (size_t id=0; !KeyMagicVKeys[id].keyName.empty(); id++) {
				if (KeyMagicVKeys[id].keyName == ruleStr) {
					result.val = KeyMagicVKeys[id].keyValue;
					result.str[0] = (wchar_t)result.val;
					break;
				}
			}
			if (result.val == -1)
				throw std::exception("Unknown VKEY specified");
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
		else if (ruleStr.size()>2 && ruleStr[0]==L'<' && ruleStr[ruleStr.length()-1]==L'>') {
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
				throw std::exception(ConfigManager::glue(L"Unknown VKEY specified \"", vkeys[vkeys.size()-1], L"\"").c_str());

			//Now, handle all modifiers
			for (size_t id=0; id<vkeys.size()-1; id++) {
				if (vkeys[id]==L"VK_SHIFT" || vkeys[id] == L"VK_LSHIFT" || vkeys[id] == L"VK_RSHIFT") {
					result.val |= KM_VKMOD_SHIFT;
				} else if (vkeys[id]==L"VK_CONTROL" || vkeys[id]==L"VK_CTRL" || vkeys[id] == L"VK_LCONTROL" || vkeys[id] == L"VK_RCONTROL" || vkeys[id] == L"VK_LCTRL" || vkeys[id] == L"VK_RCTRL") {
					result.val |= KM_VKMOD_CTRL;
				} else if (vkeys[id]==L"VK_ALT" || vkeys[id]==L"VK_MENU" || vkeys[id] == L"VK_LALT" || vkeys[id] == L"VK_RALT" || vkeys[id] == L"VK_LMENU" || vkeys[id] == L"VK_RMENU") {
					result.val |= KM_VKMOD_ALT;
				} else if (vkeys[id]==L"VK_CAPSLOCK" || vkeys[id]==L"VK_CAPITAL") {
					result.val |= KM_VKMOD_CAPS;
				} else {
					throw std::exception(ConfigManager::glue(L"Unknown VKEY specified as modifier \"", vkeys[id], L"\"").c_str());
				}
			}
		}

		//Done?
		if (result.type==KMRT_UNKNOWN)
			throw std::exception("Error: Unknown rule type");
	} catch (std::exception ex) {
		throw std::exception(ConfigManager::glue(ex.what(), "  \"", waitzar::escape_wstr(ruleStr, false), "\"").c_str());
	}

	return result;
}



//This function will likely be replaced by something more powerful later
Rule KeyMagicInputMethod::compressToSingleStringRule(const std::vector<Rule>& rules)
{
	//Init
	Rule res(KMRT_STRING, L"", 0);

	//Combine all strings, and all variables which point ONLY to strings
	for (std::vector<Rule>::const_iterator topRule = rules.begin(); topRule!=rules.end(); topRule++) {
		if (topRule->type==KMRT_STRING) {
			res.str += topRule->str;
		} else if (topRule->type==KMRT_VARARRAY) {
			if (variables[topRule->id].size()!=1 || variables[topRule->id][0].type!=KMRT_STRING)
				throw std::exception("Error: Variable depth too great for '^' or '*' reference as \"VARARRAY\"");
			res.str += variables[topRule->id][0].str[topRule->val];
		} else if (topRule->type==KMRT_VARIABLE) {
			if (variables[topRule->id].size()!=1 || variables[topRule->id][0].type!=KMRT_STRING)
				throw std::exception("Error: Variable depth too great for '^' or '*' reference as \"VARIABLE\"");
			res.str += variables[topRule->id][0].str;
		} else
			throw std::exception("Error: Variable accessed with '*' or '^', but points to non-obvious data structure.");
	}

	return res;
}




//Validate (and slightly transform) a set of rules given a start and an end index
//We assume that this is never called on the LHS of an assigment statement.
vector<Rule> KeyMagicInputMethod::createRuleVector(const vector<Rule>& rules, const map< wstring, unsigned int>& varLookup, map< wstring, unsigned int>& switchLookup, size_t iStart, size_t iEnd, bool condenseStrings)
{
	//Behave differently depending on the rule type
	vector<Rule> res;
	for (size_t i=iStart; i<iEnd; i++) {
		Rule currRule = rules[i];
		switch (currRule.type) {
			case KMRT_UNKNOWN:
				throw std::exception("Error: A rule was discovered with type \"KMRT_UNKNOWN\"");

			case KMRT_STRING:
				//If a string is followed by another string, combine them.
				if (condenseStrings) {
					while (i+1<iEnd && rules[i+1].type==KMRT_STRING) {
						currRule.str += rules[i+1].str;
						i++;
					}
				}
				break;

			case KMRT_VARIABLE:
			case KMRT_VARARRAY:
			case KMRT_VARARRAY_SPECIAL:
			case KMRT_VARARRAY_BACKREF:
				//Make sure all variables exist; fill in their implicit ID
				//NOTE: We cannot assume variables before they are declared; this would allow for circular references.
				if (varLookup.count(currRule.str)==0)
					throw std::exception(ConfigManager::glue(L"Error: Previously undefined variable \"", currRule.str, L"\"").c_str());
				currRule.id = varLookup.find(currRule.str)->second;

				//Special check
				if (currRule.type==KMRT_VARARRAY_SPECIAL || currRule.type==KMRT_VARARRAY_BACKREF) {
					//For now, we need to treat these as arrays of strings. To avoid crashing at runtime, we
					//   attempt to conver them here. (We let the result fizzle)
					compressToSingleStringRule(variables[currRule.id]);
				}
				break;

			case KMRT_SWITCH:
				//Make sure the switch exists, and fill in its implicit ID
				if (switchLookup.count(currRule.str)==0) {
					switchLookup[currRule.str] = switches.size();
					switches.push_back(false);
				}
				currRule.id = switchLookup.find(currRule.str)->second;
				break;
		}

		res.push_back(currRule);
	}

	return res;
}




//Add a rule to our replacements/variables
void KeyMagicInputMethod::addSingleRule(const vector<Rule>& rules, map< wstring, unsigned int>& varLookup, map< wstring, unsigned int>& switchLookup, size_t rhsStart, bool isVariable)
{
	//
	// Step 1: Validate
	//

	//Simple checks 1,2: Variables are correct
	if (isVariable && rhsStart!=1)
		throw std::exception("Rule error: Variables cannot have multiple assignments left of the parentheses");
	if (isVariable && rules[0].type!=KMRT_VARIABLE)
		throw std::exception("Rule error: Assignment ($x = y) can only assign into a variable.");

	//LHS checks: no backreferences
	for (size_t i=0; i<rhsStart; i++) {
		if (rules[i].type==KMRT_MATCHVAR || rules[i].type==KMRT_VARARRAY_BACKREF)
			throw std::exception("Cannot have backreference ($1, $2, or $test[$1]) on the left of an expression");
	}

	//RHS checks: no ambiguous wildcards
	for (size_t i=rhsStart; i<rules.size(); i++) {
		if (rules[i].type==KMRT_WILDCARD || rules[i].type==KMRT_VARARRAY_SPECIAL)
			throw std::exception("Cannot have wildcards (*, $test[*], $test[^]) on the right of an expression");
	}

	//TODO: We might consider checking the number of backreferences against the $1...${n} values, but I don't 
	//      think it's necessary. We can also check this elsewhere.


	//
	// Step 2: Modify
	//

	//Compute the RHS string first (this also avoids circular variable references)
	std::vector<Rule> rhsVector = createRuleVector(rules, varLookup, switchLookup, rhsStart, rules.size(), true);

	//LHS storage depends on if this is a variable or not
	if (isVariable) {
		//Make sure it's in the array ONLY once. Then add it.
		const wstring& candidate = rules[0].str;
		if (varLookup.count(candidate) != 0)
			throw std::exception(ConfigManager::glue(L"Error: Duplicate variable definition: \"", candidate, L"\"").c_str());
		varLookup[candidate] = variables.size();
		variables.push_back(rhsVector);
	} else {
		//Get a similar LHS vector, add it to our replacements list
		std::vector<Rule> lhsVector = createRuleVector(rules, varLookup, switchLookup, 0, rhsStart, false); //We need to preserve groupings
		replacements.push_back(std::pair< std::vector<Rule>, std::vector<Rule> >(lhsVector, rhsVector));
	}
}



//NOTE: This function will update matchedOneVirtualKey
pair<Candidate, bool> KeyMagicInputMethod::getCandidateMatch(pair< vector<Rule>, vector<Rule> >& rule, const wstring& input, unsigned int vkeyCode, bool& matchedOneVirtualKey)
{
	vector<Candidate> candidates;
	for (size_t dot=0; dot<input.length(); dot++) {
		//Add a new empty candidate
		candidates.push_back(Candidate(rule, dot));

		//Continue matching.
		//Note: For now, the candidates list won't expand while we move the dot. (It might shrink, however)
		//      Later, we will have to dynamically update i, but this should be fairly simple.
		for (vector<Candidate>::iterator curr=candidates.begin(); curr!=candidates.end();) {
			//Before attemping to "move the dot", we must expand any variables by adding new entries to the stack.
			//NOTE: We also need to handle switches, which shouldn't move the dot anyway
			bool allow = true;
			while (curr->getCurrRule().type==KMRT_VARIABLE || curr->getCurrRule().type==KMRT_SWITCH) {
				if (curr->getCurrRule().type==KMRT_VARIABLE) {
					//Variable: Push to stack
					curr->newCurr(variables[curr->getCurrRule().id]);
				} else {
					//Switch: only if that switch is ON, and matching will turn that switch OFF later.
					if (switches[curr->getCurrRule().id]) {
						curr->advance(L'', -1);
						curr->queueSwitchOff(curr->getCurrRule().id);
					} else {
						allow = false;
						break;
					}
				}
			}

			//"Moving the dot" is allowed under different circumstances, depending on the type of rule we're dealing with.
			if (allow) {
				switch(curr->getCurrRule().type) {
					//Wildcard: always move
					case KMRT_WILDCARD:
						curr->advance(input[dot], -1);
						break;

					//String: only if the current char matches
					case KMRT_STRING:
						if (curr->getCurrStringRuleChar() == input[dot])
							curr->advance(input[dot], -1);
						else
							allow = false;
						break;

					//Vararray: Only if that particular character matches
					//Silently fail to match if an invalid index is given.
					case KMRT_VARARRAY:
					{
						//TODO: Right now, this fails for anything except a string array (and anything simple)
						Rule toCheck  = compressToSingleStringRule(variables[curr->getCurrRule().id]);
						wstring str = toCheck.str;
						if (curr->getCurrRule().val>0 && curr->getCurrRule().val<=(int)str.length() && (str[curr->getCurrRule().val-1] == input[dot]))
							curr->advance(input[dot], -1);
						else
							allow = false;
						break;
					}

					//Match "any" or "not any"; silently fail on anything else.
					case KMRT_VARARRAY_SPECIAL:
						if (curr->getCurrRule().val!='*' && curr->getCurrRule().val!='^')
							allow = false;
						else {
							//TODO: Right now, this fails silently for anything except strings and simple variables.
							Rule toCheck  = compressToSingleStringRule(variables[curr->getCurrRule().id]);
							int foundID = -1;
							for (size_t i=0; i<toCheck.str.length(); i++) {
								//Does it match?
								if (toCheck.str[i] == input[dot]) {
									foundID = i;
									break;	
								}
							}

							//We allow if '*' and found, or '^' and not
							if (curr->getCurrRule().val=='*' && foundID==-1)
								allow = false;
							else if (curr->getCurrRule().val=='^' && foundID!=-1)
								allow = false;
							else
								curr->advance(input[dot], foundID);
						}
						break;

					//Key combinations only match in certain conditions, which are checked before this.
					case KMRT_KEYCOMBINATION:
						allow = false;
						break;

					//"Error" and "quasi-error" cases.
					case KMRT_VARIABLE:
						throw std::exception("Bad match: variables should be dealt with outside the main loop.");
					case KMRT_SWITCH:
						throw std::exception("Bad match: switches should be dealt with outside the main loop.");
					default:
						throw std::exception("Bad match: inavlid rule type.");
				}
			}

			//Did this rule pass or fail?
			bool eraseFlag = false;
			if (!allow) {
				//Remove the current entry; decrement the pointer
				curr = candidates.erase(curr);
				eraseFlag = true;
				continue;
			}

			//Did this match finish?
			if (curr->isDone()) {
				curr->dotEndID = dot+1;
				return pair<Candidate, bool>(*curr, true);
			}

			//Did we just erase an element? If so, the pointer doesn't increment
			if (eraseFlag) 
				eraseFlag = false;
			else
				curr++;
		}
	}

	//There is one case where the dot can be at the end of the string and a rule is still matched: if 
	//   a VK_* matches and no VK_* has previously been matched.
	if (!matchedOneVirtualKey) {
		//Match the <VK_*> values on all candidates
		for (vector<Candidate>::iterator curr=candidates.begin(); curr!=candidates.end(); curr++) {
			//Move on the VKEY?
			if (curr->getCurrRule().type==KMRT_KEYCOMBINATION && curr->getCurrRule().val==vkeyCode) {
				curr->advance(L"", -1);
				if (curr->isDone()) {
					matchedOneVirtualKey = true;
					curr->dotEndID = input.length();
					return pair<Candidate, bool>(*curr, true);
				}
			}
		}
	}

	//No matches: past the end of the input string.
	vector<Rule> temp;
	return pair<Candidate, bool>(Candidate(temp), false);
}


//NOTE: resetLoop and breakLoop are changed by this function.
//NOTE: breakLoop takes precedence
wstring KeyMagicInputMethod::applyMatch(const Candidate& result, bool& resetLoop, bool& breakLoop)
{
	//Reset flags
	resetLoop = false;
	breakLoop = false;

	//Turn "off" all switches that were matched
	const vector<unsigned int>& switchesToOff = result.getPendingSwitches();
	for (vector<unsigned int>::const_iterator it=switchesToOff.begin(); it!=switchesToOff.end(); it++)
		switches[*it] = false;

	//We've got a match! Apply our replacements algorithm
	std::wstringstream replacementStr;
	for (std::vector<Rule>::iterator repRule=result.replacementRules.begin(); repRule!=result.replacementRules.end(); repRule++) {
		switch(repRule->type) {
			//String: Just append it.
			case KMRT_STRING:
				replacementStr <<repRule->str;
				break;

			//Variable: try to append that variable.
			case KMRT_VARIABLE:
			{
				//To-do: Right now, this only applies for simple & semi-complex rules.
				Rule toAdd  = compressToSingleStringRule(variables[repRule->id]);
				replacementStr <<toAdd.str;
				break;
			}

			//Turn switches back on.
			case KMRT_SWITCH:
				switches[repRule->id] = true;
				break;

			//Vararray: just add that character
			case KMRT_VARARRAY:
			{
				Rule toAdd  = compressToSingleStringRule(variables[repRule->id]);
				replacementStr <<toAdd.str[repRule->val-1];
				break;
			}

			//Variable: Just add the saved backref
			case KMRT_MATCHVAR:
				replacementStr <<result.getMatch(repRule->val-1);
				break;

			//Vararray backref: Requires a little more indexing
			case KMRT_VARARRAY_BACKREF:
			{
				Rule toAdd  = compressToSingleStringRule(variables[repRule->id]);
				replacementStr <<toAdd.str[result.getMatchID(repRule->val-1)];
 				break;
			}

			//Anything else (WILDCARD, VARARRAY_SPECIAL , KEYCOMBINATION) is an error.
			default:
				throw std::exception("Bad match: inavlid replacement type.");
		}
	}

	//Apply the "single ASCII replacement" rule
	if (result.replacementRules.size()==1 && result.replacementRules[0].type==KMRT_STRING && result.replacementRules[0].str.length()==1) {
		wchar_t ch = result.replacementRules[0].str[0];
		if (ch>L'\x020' && ch<L'\x07F') {
			breakLoop = true;
		}
	}

	//Save new string
	wstring newStr = replacementStr.str();

	//Reset for the next rule.
	resetLoop = true;

	return newStr;
}



pair<wstring, bool> KeyMagicInputMethod::appendTypedLetter(const wstring& prevStr, wchar_t nextASCII, WPARAM nextKeycode)
{
	//Append, apply rules
	return pair<wstring, bool>(applyRules(prevStr+wstring(1,nextASCII), nextKeycode), true);
}



wstring KeyMagicInputMethod::applyRules(const wstring& origInput, unsigned int vkeyCode)
{
	//Volatile version of our input
	wstring input = origInput;

	//First, turn all switches off
	for (size_t i=0; i<switches.size(); i++)
		switches[i] = false;

	//For each rule, generate and match a series of candidates
	//  As we do this, move the "dot" from position 0 to position len(input)
	bool matchedOneVirtualKey = false;
	bool resetLoop = false;
	bool breakLoop = false;
	unsigned int totalMatchesOverall = 0;
	for (int rpmID=0; rpmID<(int)replacements.size(); rpmID++) {
		//Somewhat of a hack...
		if (resetLoop) {
			rpmID = 0;
			resetLoop = false;
		}

		//Match this rule
		pair<Candidate, bool> result = getCandidateMatch(replacements[rpmID], input, vkeyCode, matchedOneVirtualKey);

		//Did we match anything?
		if (result.second) {
			//Before we apply the rule, check if we've looped "forever"
			if (++totalMatchesOverall >= 500) {
				//To do: We might also consider logging this, later.
				throw std::exception(ConfigManager::glue(L"Error on keymagic regex; infinite loop on input: \n   ", input).c_str());
			}

			//Apply, update input.
			input = input.substr(0, result.first.dotStartID) + applyMatch(result.first, resetLoop, breakLoop) + input.substr(result.first.dotEndID, input.size());
			if (breakLoop)
				break;
		}
	}

	return input;
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
