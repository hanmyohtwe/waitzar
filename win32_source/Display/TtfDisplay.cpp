/*
 * Copyright 2009 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#include "TtfDisplay.h"


using std::wstring;
using std::string;


TtfDisplay::TtfDisplay()
{
	this->font = NULL;
	this->fontHandle = NULL;
	this->lastKnownGoodHDC = NULL;
	this->fontHeight = -1;
	this->fileToDelete = L"";
}

TtfDisplay::~TtfDisplay()
{
	//Delete our font(s) from memory
	if (fontHandle!=NULL)
		RemoveFontMemResourceEx(fontHandle);
	if (!fileToDelete.empty())
		RemoveFontResourceEx(fileToDelete.c_str(), FR_PRIVATE, 0);
}


void TtfDisplay::init(HFONT existingFont)
{
	this->font = existingFont;
}


void TtfDisplay::init(char *data, unsigned long size, HDC currDC, unsigned int defaultColor)
{
	//No use for now
}

void TtfDisplay::init(const wstring& fileName,  HDC currDC, unsigned int defaultColor, int devLogPixelsY)
{
	//Save
	if (currDC != NULL)
		lastKnownGoodHDC = currDC;

	//Create it
	if (!fileName.empty()) {
		fileToDelete = fileName;
		if (AddFontResourceEx(fileName.c_str(), FR_PRIVATE, 0)==0)
			throw std::exception("Couldn't load internal font file");
	}

	//Now, add the logical font
	this->initLogicalFont(devLogPixelsY);
}


void TtfDisplay::init(HRSRC resource, HGLOBAL dataHandle, HDC currDC, int devLogPixelsY, unsigned int defaultColor)
{
	//Save
	if (currDC != NULL)
		lastKnownGoodHDC = currDC;

	//Get raw data
	void* data = LockResource(dataHandle);
	size_t len = SizeofResource(NULL, resource);

	//Add the font resource
	DWORD nFonts;
	fontHandle = AddFontMemResourceEx(data, len, 0, &nFonts);
	if(!fontHandle)
		throw std::exception("Embedded font could not be loaded.");

	//Unlock this resource for later use.
	UnlockResource(dataHandle);

	//Now, add the logical font
	this->initLogicalFont(devLogPixelsY);
}



void TtfDisplay::initLogicalFont(int devLogPixelsY)
{
	//Create the Font
	LOGFONT lf;
	memset(&lf, 0, sizeof(lf));
	lf.lfHeight = -MulDiv(pointSize, devLogPixelsY, 72);
	lf.lfWeight = FW_DONTCARE;
	lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
	lf.lfQuality = DEFAULT_QUALITY;//PROOF_QUALITY;
	wcscpy_s(lf.lfFaceName, fontFaceName.c_str());
	font = CreateFontIndirect(&lf);
	if (!font)
		throw std::exception("Could not create font");
}


HFONT TtfDisplay::getInternalHFont()
{
	return font;
}



void TtfDisplay::drawString(HDC bufferDC, const std::wstring &str, int xPos, int yPos)
{
	//Save
	if (bufferDC!=NULL)
		lastKnownGoodHDC = bufferDC;

	//Set the font, set the color
	HFONT oldFont = NULL;
	if (font!=NULL)
		oldFont = (HFONT)SelectObject(lastKnownGoodHDC, font);
	COLORREF oldTextColor = SetTextColor(lastKnownGoodHDC, currColorRGB);

	//Actual drawing code
	int prevBkgMode = SetBkMode(lastKnownGoodHDC, TRANSPARENT);
	//int oldSmooth;
	//SystemParametersInfo(SPI_GETFONTSMOOTHINGCONTRAST, 0, &oldSmooth, FALSE);
	//int newSmooth = 2200; //Maximum smoothing. BAD: Messes up the entire window!
	//SystemParametersInfo(SPI_SETFONTSMOOTHINGCONTRAST, 0, &newSmooth, FALSE);
	ExtTextOut(lastKnownGoodHDC, xPos, yPos, 0, NULL, str.c_str(), str.length(), NULL);
	//SystemParametersInfo(SPI_SETFONTSMOOTHINGCONTRAST, 0, &oldSmooth, FALSE);
	SetBkMode(lastKnownGoodHDC, prevBkgMode);
	
	//Restore
	SetTextColor(lastKnownGoodHDC, oldTextColor);
	if (oldFont!=NULL)
		SelectObject(lastKnownGoodHDC, oldFont);
}


void TtfDisplay::drawString(HDC bufferDC, const std::string &str, int xPos, int yPos)
{
	//Funnel into a single function call
	std::wstringstream buff;
	buff <<str.c_str();
	this->drawString(bufferDC, buff.str(), xPos, yPos);
}


void TtfDisplay::drawChar(HDC bufferDC, char letter, int xPos, int yPos)
{
	//Funnel into a single function call
	std::wstringstream buff;
	buff <<letter;
	this->drawString(bufferDC, buff.str(), xPos, yPos);
}


int TtfDisplay::getStringWidth(const std::wstring &str, HDC currDC)
{
	//Save
	if (currDC!=NULL)
		lastKnownGoodHDC = currDC;

	//Measure this item by its string.
	SIZE textSize;
	HFONT oldFont = NULL;
	if (font!=NULL)
		oldFont = (HFONT)SelectObject(lastKnownGoodHDC, font);
	GetTextExtentPoint32(lastKnownGoodHDC, str.c_str(), str.length(), &textSize);

	//Restore the DC, return
	if (oldFont!=NULL)
		SelectObject(lastKnownGoodHDC, oldFont);
	return textSize.cx;
}

int TtfDisplay::getHeight(HDC currDC)
{
	//Save
	if (currDC!=NULL)
		lastKnownGoodHDC = currDC;

	//Cache our value after setting it once
	if (fontHeight==-1) {
		//Measure the font
		SIZE sz;
		wstring testStr = L"Testing: \u1000\u103C\u1000\u103B";
		HFONT oldFont = NULL;
		if (font!=NULL)
			oldFont = (HFONT)SelectObject(lastKnownGoodHDC, font);
		GetTextExtentPoint32(lastKnownGoodHDC, testStr.c_str(), testStr.length(), &sz);
		fontHeight = sz.cy;

		//Restore the DC
		if (oldFont!=NULL)
			SelectObject(lastKnownGoodHDC, oldFont);
	}

	return fontHeight;
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
