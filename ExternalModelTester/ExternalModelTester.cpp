#include <stdio.h>

//#include <wchar.h>
//#include <locale.h>

#include "../win32_source/fontconv.h"
#include "../win32_source/WordBuilder.h"


//Function prototype:
wchar_t* makeStringFromKeystrokes(std::vector<unsigned short> keystrokes);

//Useful global...
wchar_t returnVal[500];


/**
 * This file demonstrates how to use Wait Zar technology in your own projects. 
 * In particular, we show how the WordBuilder class can be used to disambiguate words.
 * You will need the following to run this sample:
 *   1) Compile or link in fontmap.[cpp|h], fonconv.[cpp|h], lib.[cpp|h], regex.[cpp|h], and WordBuilder.[cpp|h]
 *   2) In WordBuilder.h, uncomment the line "#define __STDC_ISO_10646__  200104L"
 *   3) Have Myanmar.model and mywords.txt on hand (if you plan to use Wait Zar's current model). 
 *   4) Note that the WordBuilder constructor can also be called with detailed information about the internals
 *      of the model. This is a very advanced usage of WordBuilder; you should probably avoid it.
 */
int main(int argc, const char* argv[])
{
	//Switch to wide-character mode
	if (fwide(stdout, 0) == 0) {
		if (fwide(stdout, 1) <= 0) {
			printf("Could not switch to wide char mode!\n");
			return 0;
		} else {
			wprintf(L"Switched to wide mode.\n");
		}
	}

	//Set the locale??
	/*char* localeInfo = setlocale(0, "japanese");
    wprintf(L"Locale information set to %s\n", localeInfo);*/

	
	//Create your model as an object.
	//NOTE: You should not mix printf() and wprintf() (it might behave unexpectedly). So, for unicode programs, always use wprintf().
	WordBuilder *model = new WordBuilder("../win32_source/Myanmar.model", "../win32_source/mywords.txt");
	wprintf(L"Model loaded correctly.\n\n");


	//////////////////////////////////////////////////////////////////////
	//Use case 1: List all Burmese words for "kote"
	//////////////////////////////////////////////////////////////////////
	
	//First, we reset the model; this should occur before the first keypress of every NEW word.
	//  Note that "true" will reset the trigrams as well, "false" just resets everything else.
	model->reset(true);
	
	//Now, we must type each letter individually. Since we know that "kote" is in our dictionary, 
	//  we just call typeLetter() four times. If we are not sure if the word exists, we should check
	//  the return value of typeLetter() each time we call it. 
	model->typeLetter('k'); //Transition -->'k'
	model->typeLetter('o'); //Transition (k) --> 'o'
	model->typeLetter('t'); //Transition (k-->o) --> 't'
	model->typeLetter('e'); //Transition (k-->o-->t) --> 'e'
	
	//We have arrived at "kote", so we now just list all possible values.
	std::vector<unsigned short> currWord;
	wchar_t* printWord;
	std::vector<unsigned int> possWords = model->getPossibleWords();
	wprintf(L"\"kote\" can be one of the following %i words \n", possWords.size());
	for (unsigned int i=0; i<possWords.size(); i++) {
		//Note that here, we use getWordKeyStrokes(), NOT getWordString(). 
		//  Calling getWordString() will ALWAYS return the Zawgyi-One encoding.
		//  If you wish to actually print out the Myanmar text, you can use a converter function, like
		//  we do here.
		currWord = model->getWordKeyStrokes(possWords[i]);
		printWord = makeStringFromKeystrokes(currWord);
		
		wprintf(L"  %ls  (", printWord);
		for (unsigned int x=0; x<currWord.size(); x++) {
			wprintf(L" %x", currWord[x]);
		}
		
		
		wprintf(L" )  size:%i\n",currWord.size());
	}
	wprintf(L"\n");



	//////////////////////////////////////////////////////////////////////
	//Use case 2: Check custom words & shortcut words. Output in Win Innwa.
	//////////////////////////////////////////////////////////////////////

	//Reset, and change the encoding.
	model->reset(true);
	model->setOutputEncoding(ENCODING_WININNWA);
	
	//Type each letter
	const char* hello = "minggalarpar";
	wprintf(L"Typing \"minggalarpar\" as: ");
	for (unsigned int i=0; i<strlen(hello); i++) {
		//Is it in the model?
		if (!model->typeLetter(hello[i])) {
			wprintf(L"\nUh-oh! Couldn't type: %i\n", i);
		}

		//Have we typed enough?
		wprintf(L"%c", hello[i]);
		if (wcslen(model->getParenString())>0) {
			wprintf(L"-");
		}
	}
	wprintf(L"\n");
	

	wprintf(L"\nDone\n" );
	return 0;
}


wchar_t* makeStringFromKeystrokes(std::vector<unsigned short> keystrokes)
{
	for (unsigned int i=0; i<keystrokes.size(); i++) {
		returnVal[i] = keystrokes[i];
	}
	returnVal[keystrokes.size()] = 0x0000; //Note: we need to terminate with a FULL-width zero (not just '\0') to ensure that we actually end the string.
	
	return returnVal;
}
