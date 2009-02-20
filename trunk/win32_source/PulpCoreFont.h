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

#pragma once

#define _UNICODE
#define UNICODE

//Don't let Visual Studio warn us to use the _s functions
#define _CRT_SECURE_NO_WARNINGS

//Ironic that adding compliance now causes headaches compiling under VS2003
#define _CRT_NON_CONFORMING_SWPRINTFS


#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include "PulpCoreImage.h"

//Magic number for Pulp Core font header.
const char PULP_MAGICNUM[] = "pulpfnt\x0B";              //0x70756c70666e740b

//Pulp Core chunk IDs
#define CHUNK_FONT 0x666f4e74


/**
 * PulpCore is a Java project; see the link in the license file. 
 *  Fonts used in PulpCore are accomplished by creating a PNG of 
 *   the Font's letters and then embedding additional "chunks" 
 *   specifying the advance width, kerning, and other specifics. 
 *  This creates a valid PNG file (the additional chunks are just
 *   ignored by any other image processing software) that can
 *   handle complex fonts like Zawgyi-One (used internally by WaitZar). 
 *  There are three good reasons to do it this way:
 *   1) PulpCore automatically anti-aliases a font's characters when
 *      converting them to PNG. So, Myanmar text in WaitZar looks good
 *      even if the user doesn't have Clear Type installed. (Developers
 *      could even manually tweak the font if required).
 *   2) Processing is slightly faster, and memory usage barely increases.
 *      Instead of rendering a true-type font, we're just blitting  pixels
 *      from one buffer to another (this is always fast.)
 *   3) If Zawgyi-One ever becomes obsolete, or changes its encoding order 
 *      (which is likely given U+104E's new usage) then we can still display 
 *      our Myanmar text without forcing the user to install the "old" version 
 *      on his system. We could embed the Zawgyi-One.ttf file itself, but that's 10 times
 *      larger than our PNG (which uses internal compression, like all PNGs do.)
 */
class PulpCoreFont : public PulpCoreImage
{
public:
	//Empty constructor
	PulpCoreFont();

	//Initializers
	void init(HRSRC resource, HGLOBAL dataHandle, HDC currDC);
	void init(PulpCoreFont* copyFrom, HDC currDC);

	//Drawing functionality
	void drawString(HDC bufferDC, TCHAR* str, int xPos, int yPos);
	void drawChar(HDC bufferDC, char letter, int xPos, int yPos);
	
	//Other useful metrics
    int getStringWidth(TCHAR* str);
	int getStringWidth(TCHAR* str, int start, int length);
	int getCharWidth(char letter);

private:
	//PulpCoreFont-specific properties
	int firstChar;
	int lastChar;
	int tracking;
	int* charPositions;
	int num_char_pos;
	int* bearingLeft;
	int* bearingRight;
	bool uppercaseOnly;

	//Internal Methods
	void readChunk(int chunkType, int length, HDC currDC);
	void fontSet();
	int getCharIndex(TCHAR ch);
	int getKerning(TCHAR left, TCHAR right);
	int getKerning(int leftIndex, int rightIndex);
	bool shouldIgnoreTracking(int index);
};
