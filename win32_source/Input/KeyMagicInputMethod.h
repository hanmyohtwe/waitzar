/*
 * Copyright 2010 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#ifndef _KEYMAGIC_INPUT_METHOD
#define _KEYMAGIC_INPUT_METHOD

#include <stack>
#include <fstream>
#include <stdexcept>

#include "MyWin32Window.h"
#include "Input/LetterInputMethod.h"
#include "Input/keymagic_vkeys.h"
#include "NGram/Logger.h"


//NOTE TO SELF: KMRT_STRING values which appear in sequence COMBINE to form one string
// E.g.   $temp = 'abc' + 'def' 
// ...must be represented as  $temp = 'abcdef' 
// ...for array indexing to function properly.
enum RULE_TYPE {
	KMRT_UNKNOWN,          //Initializer
	KMRT_STRING,           //Anything which just matches a string (includes single characters, UXXXX)
	KMRT_WILDCARD,         //The wildcard character *
	KMRT_VARIABLE,         //Any "normal" variable like $test
	KMRT_MATCHVAR,         //Any backreference like $1
	KMRT_SWITCH,           //Any switch like ('my_switch')
	KMRT_VARARRAY,         //Variable matches like $temp[1]
	KMRT_VARARRAY_SPECIAL, //Variable matches like $temp[*], $temp[^] (we store the char itself in the val value)
	KMRT_VARARRAY_BACKREF, //Variable matches like $temp[$1]
	KMRT_KEYCOMBINATION,   //Something like <VK_SHIFT & VK_T> (val contains the key, combined with modifiers)
};

//Current version of our binary file format.
enum {
	KEYMAGIC_BINARY_VERSION = 1
};

struct Rule {
	RULE_TYPE type;
	std::wstring str;
	int val;
	int id; //Used for variables (and switches, later). 

	Rule(RULE_TYPE type1, const std::wstring& str1, int val1) {
		type = type1;
		str = str1;
		val = val1;
		id = -1;
	}
};



struct RuleSet {
	std::vector<Rule> match;
	std::vector<Rule> replace;
	std::vector<unsigned int> requiredSwitches;
	std::wstring debugRuleText;
	unsigned int tempOriginalSortID;

	//Helpers
	size_t getNumVkeys() const {
		size_t total = 0;
		for (std::vector<Rule>::const_iterator it=match.begin(); it!=match.end(); it++) {
			if (it->type==KMRT_KEYCOMBINATION) {
				total++;
				if ((it->val&KM_VKMOD_ALT)!=0)
					total++;
				if ((it->val&KM_VKMOD_CTRL)!=0)
					total++;
				if ((it->val&KM_VKMOD_SHIFT)!=0)
					total++;
				if ((it->val&KM_VKMOD_CAPS)!=0)
					total++;
			}
		}
		return total;
	}

	size_t getMatchStrExpectedLength(std::wstring(*GetVarString)(const std::vector< std::vector<Rule> >&, size_t), const std::vector< std::vector<Rule> >& variables) const {
		int estRuleSize = 0;
		for (size_t i=0; i<match.size(); i++) {
			//String: just add the string length; all escape 
			// sequences, etc., have already been folded in.
			if (match[i].type==KMRT_STRING)
				estRuleSize += match[i].str.length();

			//"Wild card" or "Vararray", or "Vararray Special" 
			// will only ever match one character.
			else if (match[i].type==KMRT_WILDCARD || match[i].type==KMRT_VARARRAY || match[i].type==KMRT_VARARRAY_SPECIAL)
				estRuleSize++;

			//"Key combination" has already been accounted for.
			// "Variable" only counts (for now) if it's a string
			//TODO: Extend and make fully recursive
			else if (match[i].type==KMRT_VARIABLE)
				estRuleSize += GetVarString(variables, match[i].id).size();
				
		}
		return estRuleSize;
	}
};



//String/int pairs
struct Group {
	std::wstring value;
	int group_match_id; //Groups count up from ONE
	Group() : value(L""), group_match_id(0) {}
	Group(const std::wstring& value1) : value(value1), group_match_id(0) {}
	Group(const std::wstring& value1, int id1) : value(value1), group_match_id(id1) {}
};


//A rule being matched and any relevant data (like the dot)
struct Matcher {
private:
	//NOTE: This is a pointer to constant data. 
	//      The pointer itself cannot be made constant, because that wouuld require us to
	//      write a custom operator= to deal with this (assignment is needed by the vector class), 
	//      and would most likely require a const-cast-away. So, we just allow the pointer to be 
	//      re-assigned, which isn't a problem, since this used to be a reference anyway.
	const std::vector<Rule>* rulestream; 

	int dot; //0 means "before the first rule"
	int strDot; //Special dot for the string.

public:
	//Constructor
	Matcher(const std::vector<Rule>* const rules = NULL) : rulestream(rules), dot(0), strDot(0) {}

	/*Matcher operator=(const Matcher& m2) {
		rulestream = m2.rulestream;
		dot = m2.dot;
		strDot = m2.strDot;
		return *this;
	}*/

	void moveDot() {
		if (rulestream==NULL)
			return;

		if ((*rulestream)[dot].type==KMRT_STRING && strDot<(int)(*rulestream)[dot].str.length()-1)
			strDot++;
		else {
			dot++;
			strDot = 0;
		}
	}
	int getDot() {
		return dot;
	}
	int getStrDot() {
		return strDot;
	}
	bool isDone() {
		return (rulestream==NULL) || (dot==rulestream->size());
	}
	const std::vector<Rule>* const getRulestream() {
		return rulestream;
	}
};


//Candidate matches.
struct Candidate {
private:
	//Matches we've already made
	std::vector<Group> matches;
	unsigned int currRootDot;

	//Matches put on hold while we recurse
	std::stack<Matcher> matchStack;

	//What switches will this candidate turn off?
	std::vector<unsigned int> switchesToOff;

	//What to replace after
	const std::vector<Rule>* replacementRules; //Note: See "Matcher" for why this can't be a constant pointer.

public:

	const std::vector<Rule>* const getReplacementRules() const { return replacementRules; }


	//Dot IDS
	int dotStartID;
	int dotEndID;

	//Default constructor
	Candidate(const std::vector<Rule>* const repRules = NULL) : replacementRules(repRules) {}

	//Init
	Candidate(const RuleSet& ruleSet, int dotStartID1) : replacementRules(&(ruleSet.replace)) {
		matchStack.push(Matcher(&(ruleSet.match)));
		switchesToOff = ruleSet.requiredSwitches;

		//Init array sizes
		for (unsigned int i=0; i<ruleSet.match.size(); i++)
			matches.push_back(Group(L"", -1));
		currRootDot = 0;

		dotStartID = dotStartID1;
	}

	//Copy constructor
	/*Candidate(const Candidate& c2) : replacementRules(c2.replacementRules) {
		matches = c2.matches;
		matchStack = c2.matchStack;
		switchesToOff = c2.switchesToOff;
		dotStartID = c2.dotStartID;
		dotEndID = c2.dotEndID;
		currRootDot = c2.currRootDot;
	}*/


	//For vector contexts 
	/*Candidate operator=(const Candidate& c2) {
		matches = c2.matches;
		matchStack = c2.matchStack;
		switchesToOff = c2.switchesToOff;
		replacementRules = c2.replacementRules;
		dotStartID = c2.dotStartID;
		dotEndID = c2.dotEndID;
		currRootDot = c2.currRootDot;
		return *this;
	}*/

	//Our "current match" is the top entry on the stack; this greatly simplifies memory management
	Matcher& currMatch() {
		if (matchStack.empty())
			throw std::runtime_error("Error: cannot match on a rule which is already done");
		return matchStack.top();
	}

	//Useful stuff
	const Rule& getCurrRule() {
		if (currMatch().getRulestream()==NULL)
			throw std::runtime_error("Bad programming: can't call \"getCurrRule\" on an emtpy rulestream.");

		if (currMatch().getDot() < (int)currMatch().getRulestream()->size())
			return (*(currMatch().getRulestream()))[currMatch().getDot()];
		throw std::runtime_error("Bad programming: the dot has exceeded the length of the current rulestream.");
	}
	wchar_t getCurrStringRuleChar() {
		if (getCurrRule().type!=KMRT_STRING)
			throw std::runtime_error("Can only call getCurrStringRuleChar() on a STRING type");
		return getCurrRule().str[currMatch().getStrDot()];
	}
	void newCurr(const std::vector<Rule>* const rule) {
		matchStack.push(Matcher(rule));
	}
	void advance(const std::wstring& foundStr, int foundID) { //Called if a match is allowed
		if (isDone())
			return;

		//Move the dot
		currMatch().moveDot();

		//Save our match data
		matches[currRootDot].value += foundStr;
		matches[currRootDot].group_match_id = foundID;

		//Pop the stack
		if (currMatch().isDone()) {
			matchStack.pop();
			advance(L"", foundID); //We need to move the current match's dot by one, signifying that we've "matched" this variable.
		}

		//Update root dot?
		if (matchStack.size()==1)
			currRootDot = currMatch().getDot();
	}
	void advance(wchar_t foundChar, int foundID) {
		std::wstringstream str;
		str <<foundChar;
		advance(str.str(), foundID);
	}
	bool isDone() { //Did we finish matching?
		return matchStack.empty();
	}

	//For switches
	/*void queueSwitchOff(unsigned int id) {
		switchesToOff.push_back(id);
	}*/
	const std::vector<unsigned int>& getPendingSwitches() const {
		return switchesToOff;
	}

	//Return
	std::wstring getMatch(unsigned int id) const {
		return matches[id].value;
	}
	int getMatchID(unsigned int id) const {
		return matches[id].group_match_id;
	}
};


class KeyMagicInputMethod : public LetterInputMethod {

public:
	KeyMagicInputMethod();
	virtual ~KeyMagicInputMethod();

	//Key functionality
	void loadTextRulesFile(const std::string& rulesFilePath);
	void loadBinaryRulesFile(const std::string& rulesFilePath);
	void saveBinaryRulesFile(const std::string& rulesFilePath, const std::string& checksum);
	void loadRulesFile(const std::string& rulesFilePath, const std::string& binaryFilePath, bool disableCache, std::string (*fileMD5Function)(const std::string&));
	std::wstring applyRules(const std::wstring& origInput, unsigned int vkeyCode);

	//Additional useful stuff
	const std::wstring& getOption(const std::wstring& optName);
	std::vector< std::pair<std::wstring, std::wstring> > convertToRulePairs();
	static std::wstring getVariableString(const std::vector< std::vector<Rule> >& variables, size_t variableID);

	//Overrides of LetterInputMethod
	std::pair<std::wstring, bool> appendTypedLetter(const std::wstring& prevStr, VirtKey& vkey);
	virtual void handleBackspace(VirtKey& vkey);
	virtual void handleStop(bool isFull, VirtKey& vkey);
	virtual void reset(bool resetCandidates, bool resetRoman, bool resetSentence, bool performFullReset);

	//Overridden just for "smart backspace"
	virtual void handleKeyPress(VirtKey& vkey);
	//virtual void appendToSentence(wchar_t letter, int id);


private:
	//Trace?
	//static bool LOG_KEYMAGIC_TRACE;
	//static std::string keyMagicLogFileName;
	static void clearLogFile();
	static void writeLogLine();
	static void writeLogLine(const std::wstring& logLine); //We'll escape MM outselves
	static void writeInt(std::vector<unsigned char>& stream, int intVal);
	static int readInt(unsigned char* buffer, size_t& currPos, size_t bufferSize);
	
	//Ugh
	static const std::wstring emptyStr;
	static bool useSmartBackspace; //Pull out into the header configs later

	//Data
	std::vector<bool> switches;
	std::vector< std::vector<Rule> > variables;
	static const std::vector< std::vector<Rule> >* lastConsideredVariables;
	std::vector< RuleSet > replacements;
	std::map<std::wstring, std::wstring> options; //Loaded from the first comment.

	//Used for smart backspace
	std::vector<std::wstring> typedStack;

	//Another index (helps search with switches quickly)
	std::map<unsigned int, std::vector<unsigned int> > switchLookup;
	static unsigned int getSwitchUniqueID(const std::vector<unsigned int>& reqSw);
	static unsigned int getSwitchUniqueID(const std::vector<bool>& switchVals);

	//Helpers
	int hexVal(wchar_t letter);
	Rule parseRule(const std::wstring& ruleStr);
	void addSingleRule(const std::wstring& fullRuleText, const std::vector<Rule>& rules, std::map< std::wstring, unsigned int>& varLookup, std::map< std::wstring, unsigned int>& switchLookup, size_t rhsStart, bool isVariable);
	std::vector<Rule> createRuleVector(const std::vector<Rule>& rules, const std::map< std::wstring, unsigned int>& varLookup, std::map< std::wstring, unsigned int>& switchLookup, std::vector<unsigned int>& switchesUsed, size_t iStart, size_t iEnd, bool condenseStrings);
	static Rule compressToSingleStringRule(const std::vector<Rule>& rules, const std::vector< std::vector<Rule> >& variables);
	std::pair<Candidate, bool> getCandidateMatch(RuleSet& rule, const std::wstring& input, unsigned int vkeyCode, bool& matchedOneVirtualKey);
	std::wstring applyMatch(const Candidate& result, bool& breakLoop, std::vector<int>& switchesToOn);

	//Again
	static bool ReplacementCompare(const RuleSet& first, const RuleSet& second);




};




#endif //_KEYMAGIC_INPUT_METHOD

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

