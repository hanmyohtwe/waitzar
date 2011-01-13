/*
 * Copyright 2009 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#ifndef _OUTPUT_TTFDISPLAY
#define _OUTPUT_TTFDISPLAY

#include <stdexcept>

#include "MyWin32Window.h"
#include "Display/DisplayMethod.h"

/**
 * Placeholder class: to be used for TtfDisplay Output method
 */
class TtfDisplay : public DisplayMethod
{
public:
	TtfDisplay();
	virtual ~TtfDisplay();

	//Additional properties
	int pointSize;
	std::wstring fontFaceName;

	//Initialization: If we manage to load from another source? (E.g., downloading from a network?)
	void init(char *data, unsigned long size, HDC currDC, unsigned int defaultColor);

	//Initialization: Load an embedded font resource
	void init(HRSRC resource, HGLOBAL dataHandle, HDC currDC, int devLogPixelsY, unsigned int defaultColor);

	//Custom: load a local TTF file
	void init(const std::wstring& fileName, HDC currDC, unsigned int defaultColor, int devLogPixelsY);

	//Special init case: load an existing font
	void init(HFONT existingFont);

	//Functionality
	void drawString(HDC bufferDC, const std::wstring &str, int xPos, int yPos, const std::wstring& filterStr=L"", size_t filterLetterWidth=0);
	void drawString(HDC bufferDC, const std::string &str, int xPos, int yPos);
	void drawChar(HDC bufferDC, char letter, int xPos, int yPos);
	int getStringWidth(const std::wstring &str, HDC currDC, const std::wstring& filterStr=L"", size_t filterLetterWidth=0);
	int getHeight(HDC currDC);

	//Hackish, but needed in one place...
	HFONT getInternalHFont();

private:
	HFONT font;
	HANDLE fontHandle;
	HDC lastKnownGoodHDC;
	HPEN greenPen;
	std::wstring fileToDelete;
	int fontHeight;

	void initLogicalFont(int devLogPixelsY);

};


#endif //_OUTPUT_PNGFONT

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

