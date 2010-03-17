/*
    Copyright (c) 2008, Interactive Pulp, LLC
    All rights reserved. (see the NOTICE file in the top directory for more information)

	NOTE: This source file is licensed under the New BSD License, unlike the remainder
	of the WaitZar project, which is licensed under the Apache License 2.0. This file is an
	amalgamation of several Java source files in the Pulp Core project, and is used primarily
	for reading an annotated version of the PNG format.

	Please see:
	http://code.google.com/p/pulpcore/
	...for the latest licensing information regarding the Pulp Core project.
*/

#include ".\pulpcorefont.h"


using std::wstring;
using std::string;


/**
 * Empty constructor
 */
PulpCoreFont::PulpCoreFont()
{
}


/**
 * Copy initializers.
 */
void PulpCoreFont::init(PulpCoreFont *copyFrom, HDC currDC) 
{
	PulpCoreImage::init(copyFrom, currDC);

	//Copy all relevant font fields
	this->num_char_pos = copyFrom->num_char_pos;
	this->uppercaseOnly = copyFrom->uppercaseOnly;
	this->firstChar = copyFrom->firstChar;
	this->lastChar = copyFrom->lastChar;
	this->tracking = copyFrom->tracking;

	//We're probably safe passing a reference
	this->charPositions = copyFrom->charPositions;
	this->bearingLeft = copyFrom->bearingLeft;
	this->bearingRight = copyFrom->bearingRight;
}


/**
 * Create a PulpCoreFont.
 */
void PulpCoreFont::init(HRSRC resource, HGLOBAL dataHandle, HDC currDC, int devLogPixelsY, unsigned int defaultColor) 
{
	PulpCoreImage::init(resource, dataHandle, currDC);

	//Tint the default color
	this->currColor = defaultColor;
	this->tintSelf(defaultColor);
}


void PulpCoreFont::init(char *data, DWORD size, HDC currDC, unsigned int defaultColor)
{
	PulpCoreImage::init(data, size, currDC);

	//Tint the default color
	this->currColor = defaultColor;
	this->tintSelf(defaultColor);
}


void PulpCoreFont::tintSelf(UINT rgbColor)
{
	//Same functionality
	PulpCoreImage::tintSelf(rgbColor);

	//Now set the color cache entries properly
	for (int i=0; i<num_char_pos-1; i++) 
		cachedColor[i] = rgbColor;
}

void PulpCoreFont::tintLetter(int letterID, unsigned int color)
{
	//Get the bounds of the letter to tint
	PulpCoreImage::tintSelf(color, charPositions[letterID], 0, charPositions[letterID+1]-charPositions[letterID], height);

	//Finally, save
	cachedColor[letterID] = color;
}


void PulpCoreFont::readChunk(int chunkType, int length, HDC currDC)
{
	if (chunkType == CHUNK_FONT)
        fontSet();
	else {
		//Delegate
		PulpCoreImage::readChunk(chunkType, length, currDC);
	}
}

void PulpCoreFont::fontSet()
{
	//Read PulpCore's magic number
	for (int i=0; i<8; i++) {
		if (res_data[currPos++] != PULP_MAGICNUM[i]) {
			std::stringstream msg;
			msg <<"PULP_MAGICNUM[" <<i <<"] is " <<res_data[currPos-1] <<" not " <<PULP_MAGICNUM[i];
			throw std::exception(msg.str().c_str());
		}
	}

	//"shorts" are always BIG_ENDIAN
	firstChar = readUnsignedShort();
    lastChar = readUnsignedShort();
    tracking = readUnsignedShort();
	bool hasBearing = (readByte()!=0);

	//Initialize arrays
    int numChars = lastChar - firstChar + 1;
	num_char_pos = numChars + 1;
    charPositions = new int[num_char_pos];
    bearingLeft = new int[numChars];
    bearingRight = new int[numChars];
	cachedColor = new unsigned int[numChars];

	//Java inits...
	for (int i=0; i<num_char_pos; i++) 
		charPositions[i] = 0;

	//Read character positions
    for (int i=0; i<num_char_pos; i++) {
		charPositions[i] = readShort();
    }

	//Read bearings
    for (int i = 0; i < numChars; i++) {
		if (hasBearing) {
			bearingLeft[i] = readShort();
			bearingRight[i] = readShort();
		} else {
			bearingLeft[i] = 0;
			bearingRight[i] = 0;
        }
	}
	uppercaseOnly = (lastChar < 'a');
}



void PulpCoreFont::drawChar(HDC bufferDC, char letter, int xPos, int yPos)
{
	//Avoid errors
	if (directPixels==NULL)
		return;

	//Get metrics
	int index = getCharIndex(letter);
	int pos = charPositions[index];
    int charWidth = charPositions[index+1] - pos;

	//Re-tint?
	if (cachedColor[index]!=currColor)
		tintLetter(index, currColor);

	//Draw this letter
	AlphaBlend(
	   bufferDC, xPos, yPos, charWidth, height,   //Destination
	   directDC, pos, 0, charWidth, height,    //Source
	   blendFunc				   //Method
	);
}



void PulpCoreFont::drawString(HDC bufferDC, const wstring &str, int xPos, int yPos)
{
	//Don't loop through null or zero-lengthed strings
	int numChars = str.length();
	if (str.empty() || numChars==0 || directPixels==NULL)
		return;

	//Loop through all letters...
	int nextIndex = getCharIndex(str[0]);
	int startX = xPos;
    for (int i=0; i<numChars; i++) {
		int index = nextIndex;
        int pos = charPositions[index];
        int charWidth = charPositions[index+1] - pos;

		//Re-tint?
		if (cachedColor[index]!=currColor)
			tintLetter(index, currColor);

		//Draw this letter
		AlphaBlend(
			   bufferDC, startX, yPos, charWidth, height,   //Destination
			   directDC, pos, 0, charWidth, height,    //Source
			   blendFunc				   //Method
		);

		//Prepare next character.... if any
        if (i < numChars-1) {
            nextIndex = getCharIndex(str[i + 1]);
            int dx = charWidth + getKerning(index, nextIndex);
			startX += dx;
        }
    }
}


int PulpCoreFont::getCharIndex(wchar_t ch)
{
	//Special-case (not in WZ)
	if (uppercaseOnly && ch>='a' &&ch<= 'z')
		ch += 'A' - 'a';

	//Bound (default to last character)
	if (ch<firstChar || ch>lastChar)
		ch = lastChar;

	return ch - firstChar;
}



void PulpCoreFont::drawString(HDC bufferDC, const string &str, int xPos, int yPos)
{
	//Don't loop through null or zero-lengthed strings
	if (str.empty() || directPixels==NULL)
		return;

	//Loop through all letters...
	int nextIndex = getCharIndex(str[0]);
	int startX = xPos;
	for (size_t i=0; i<str.length(); i++) {
		int index = nextIndex;
        int pos = charPositions[index];
        int charWidth = charPositions[index+1] - pos;

		//Re-tint?
		if (cachedColor[index]!=currColor)
			tintLetter(index, currColor);

		//Draw this letter
		AlphaBlend(
			   bufferDC, startX, yPos, charWidth, height,   //Destination
			   directDC, pos, 0, charWidth, height,    //Source
			   blendFunc				   //Method
		);

		//Prepare next character.... if any
		if (i < str.length()-1) {
            nextIndex = getCharIndex(str[i + 1]);
            int dx = charWidth + getKerning(index, nextIndex);
			startX += dx;
        }
    }
}

int PulpCoreFont::getCharIndex(char ch)
{
	//Special-case (not in WZ)
	if (uppercaseOnly && ch>='a' &&ch<= 'z')
		ch += 'A' - 'a';

	//Bound (default to last character)
	if (ch<firstChar || ch>lastChar)
		ch = lastChar;

	return ch - firstChar;
}



int PulpCoreFont::getKerning(int leftIndex, int rightIndex)
{
	// Future versions of this method might handle kerning pairs, like "WA" and "Yo"
	if (tracking!=0 && (shouldIgnoreTracking(rightIndex) || shouldIgnoreTracking(leftIndex)))
        return bearingRight[leftIndex] + bearingLeft[rightIndex];
	else
		return bearingRight[leftIndex] + tracking + bearingLeft[rightIndex];
}



bool PulpCoreFont::shouldIgnoreTracking(int index)
{
	int width = (charPositions[index+1] - charPositions[index]);
	int lsb = bearingLeft[index];
    int rsb = bearingRight[index];
    int advance = width + lsb + rsb;
    return advance < width/2;
}


int PulpCoreFont::getCharWidth(char letter)
{
	int index = getCharIndex(letter);
	int pos = charPositions[index];
	return charPositions[index+1] - pos;
}

int PulpCoreFont::getHeight(HDC currDC)
{
	return PulpCoreImage::getHeight();
}


int PulpCoreFont::getStringWidth(const wstring &str, HDC currDC)
{
	return getStringWidth(str, 0, str.length());
}


int PulpCoreFont::getStringWidth(const wstring &str, int start, int end)
{
        if (end <= start) {
            return 0;
        }
        int stringWidth = 0;
        
        int lastIndex = -1;
        for (int i=start; i<end; i++) {  //FIXME: Something's causing an error here, in this loop...
            int index = getCharIndex(str[i]);
            int charWidth = charPositions[index+1] - charPositions[index];
            
            if (lastIndex!=-1)
                stringWidth += getKerning(lastIndex, index);
            stringWidth += charWidth;
            lastIndex = index;
        }
        return stringWidth;
}


