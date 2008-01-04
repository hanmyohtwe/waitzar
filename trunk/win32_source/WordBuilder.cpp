#include ".\wordbuilder.h"

WordBuilder::WordBuilder(WORD **dictionary, UINT32 **nexus, UINT32 **prefix) 
{
	//Store for later
	this->dictionary = dictionary;
	this->nexus = nexus;
	this->prefix = prefix;

	//Start off
	this->reset(true);
}

WordBuilder::~WordBuilder(void)
{
	//NOTE: This function probably doesn't ever need to be implemented properly.
	//      The way it stands, all files are scanned and data loaded when the
	//      program first starts. This all remains in memory until the program
	//      exits, at which point the entire process terminates anyways.
	//      There is never more than one WordBuilder, dictionary, nexus, prefix array, etc.
	//      So, we don't really worry too much about memory (except brushes and windows and 
	//      those kinds of things.)
}


/**
 * Types a letter and advances the nexus pointer. Returns true if the letter was a valid move,
 *   false otherwise. Updates the available word/letter list appropriately.
 */
bool WordBuilder::typeLetter(char letter) 
{
	//Is this letter meaningful?
	int nextNexus = jumpToNexus(this->currNexus, letter);
	if (nextNexus == -1)
		return false;

	//Save the path to this point and continue
	pastNexus[pastNexusID++] = currNexus;
	currNexus = nextNexus;

	//Update the external state of the WordBuilder
	this->resolveWords();

	//Success
	return true;
}


//Returns the selected ID and a boolean
std::pair<BOOL, UINT32> WordBuilder::typeSpace() 
{
	//Return val
	std::pair<BOOL, UINT32> result(FALSE, 0);

	//We're at a valid stopping point?
	if (this->getPossibleWords().size() == 0)
		return result;

	//Get the selected word, add it to the prefix array
	UINT32 newWord = this->getPossibleWords()[this->currSelectedID];
	addPrefix(newWord);

	//Reset the model, return this word
	this->reset(false);

	result.first = TRUE;
	result.second = newWord;
	return result;
}


/**
 * Reset the builder to its zero-state
 * @param fullReset If true, remove all prefixes in memory. (Called when Alt+Shift is detected.)
 */
void WordBuilder::reset(bool fullReset)
{
	//Partial reset
	this->currNexus = 0;
	this->pastNexusID = 0;
	this->currSelectedID = 0;
	this->possibleChars.clear();
	this->possibleWords.clear();

	//Full reset: remove all prefixes
	if (fullReset)
		this->trigramCount = 0;
}


/**
 * Given a "fromNexus", find the link leading out on character "jumpChar" and take it.
 *   Returns -1 if no such nexus could be found.
 */
int WordBuilder::jumpToNexus(int fromNexus, char jumpChar) 
{
	for (UINT32 i=0; i<this->nexus[fromNexus][0]; i++) {
		if ( (this->nexus[fromNexus][i+1]&0xFF) == jumpChar )
			return (this->nexus[fromNexus][i+1]>>8);
	}
	return -1;
}


int WordBuilder::jumpToPrefix(int fromPrefix, int jumpID)
{
	for (UINT32 i=0; i<this->prefix[fromPrefix][0]; i++) {
		if ( this->prefix[fromPrefix][i*2+2] == jumpID )
			return this->prefix[fromPrefix][i*2+3];
	}
	return -1;
}


/**
 * Based on the current nexus, what letters are valid moves, and what words
 *   should we present to the user?
 */
void WordBuilder::resolveWords(void) 
{
	//What possible characters are available after this point?
	int lowestPrefix = -1;
	this->possibleChars.clear();
	for (UINT32 i=0; i<this->nexus[currNexus][0]; i++) {
		char currChar = (this->nexus[currNexus][i+1]&0xFF);
		if (currChar == '~')
			lowestPrefix = (this->nexus[currNexus][i+1]>>8);
		else
			this->possibleChars.push_back(currChar);
	}

	//What words are possible given this point?
	possibleWords.clear();
	if (lowestPrefix == -1)
		return;

	//Hop to the most informative and least informative prefixes
	int highPrefix = lowestPrefix;
	for (UINT32 i=0; i<trigramCount; i++) {
		int newHigh = jumpToPrefix(highPrefix, trigram[i]);
		if (newHigh==-1)
			break;
		highPrefix = newHigh;
	}

	//First, put all high-informative entries into the resultant vector
	//  Then, add any remaining low-information entries
	for (UINT32 i=0; i<prefix[highPrefix][1]; i++) {
		possibleWords.push_back(prefix[highPrefix][2+prefix[highPrefix][0]*2+i]);
	}
	if (highPrefix != lowestPrefix) {
		for (UINT32 i=0; i<prefix[lowestPrefix][1]; i++) {
			int val = prefix[lowestPrefix][2+prefix[lowestPrefix][0]*2+i];
			if (!vectorContains(possibleWords, val))
				possibleWords.push_back(val);
		}
	}
}


bool WordBuilder::vectorContains(std::vector<UINT32> vec, UINT32 val)
{
	for (size_t i=0; i<vec.size(); i++) {
		if (vec[1] == val)
			return true;
	}
	return false;
}


void WordBuilder::addPrefix(UINT32 latestPrefix)
{
	//Latest prefixes go in the back
	if (trigramCount == 3) {
		trigram[0] = trigram[1];
		trigram[1] = trigram[2];
		trigram[2] = latestPrefix;
	} else
		trigram[trigramCount++] = latestPrefix;
}


std::vector<char> WordBuilder::getPossibleChars(void)
{
	return this->possibleChars;
}

std::vector<UINT32> WordBuilder::getPossibleWords(void)
{
	return this->possibleWords;
}

std::vector<WORD> WordBuilder::getWordKeyStrokes(UINT32 id) 
{
	this->keystrokeVector.clear();

	WORD size = this->dictionary[id][0];
	for (int i=0; i<size; i++) 
		this->keystrokeVector.push_back(this->dictionary[id][i+1]);

	return this->keystrokeVector;
}

//Note: returns a SINGLETON
TCHAR* WordBuilder::getWordString(UINT32 id)
{
	lstrcpy(currStr, _T(""));
	TCHAR temp[50];

	WORD size = this->dictionary[id][0];
	for (int i=0; i<size; i++)  {
		wsprintf(temp, _T("%c"), this->dictionary[id][i+1]);
		lstrcat(this->currStr, temp);
	}

	return this->currStr;
}
