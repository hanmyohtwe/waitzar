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

PulpCoreFont::PulpCoreFont(HRSRC resource, HGLOBAL dataHandle)
{
	//Init
	this->error = FALSE;
	lstrcpy(this->errorMsg, _T(""));

	//Get raw data
    res_data = (char*)LockResource(dataHandle);
    res_size = SizeofResource(NULL, resource);

	//Loop through all bytes...
	currPos = 0;
	
	//Read the png signature
	for (int i=0; i<8; i++) {
		if (res_data[currPos++] != PNG_SIGNATURE[i] && error==FALSE) {
			swprintf(errorMsg, _T("PNG_SIG[%i] is %02X not %02X"), i, res_data[currPos-1], PNG_SIGNATURE[i]);
			error = TRUE;
		}
	}

	//Read remaining "chunks"
    int length;
    int chunkType;
	//int crc;
	while (error==FALSE) {
		length = readInt(); //Error; access violation
		chunkType = readInt();

		if (chunkType == CHUNK_IHDR)
			readHeader();
		else if (chunkType == CHUNK_HOTS) {
			hotspotX = readInt();
			hotspotY = readInt();
		} else if (chunkType == CHUNK_PLTE)
            readPalette(length);
        else if (chunkType == CHUNK_TRNS)
            readTransparency(length);
        else if (chunkType == CHUNK_IDAT)
            readData();
        else if (chunkType == CHUNK_ANIM)
            readAnimation();
        else if (chunkType == CHUNK_FONT)
            fontSet();
		else if (chunkType != CHUNK_IEND)
			currPos += length;  //Ignore this chunk

		//Ignore CRC
        readInt();
            
		//Continue?
        if (chunkType == CHUNK_IEND)
            break;
	}

	//Remaining data signifies an error
	if (currPos != res_size && error==FALSE) {
		swprintf(errorMsg, _T("Extraneous bytes: %l"), (res_size-currPos));
		error = TRUE;
	}

}



void PulpCoreFont::readHeader() 
{
	//Read basic image information
	width = readInt();
    height = readInt();
	bitDepth = readByte();
    colorType = readByte();
    int compressionMethod = readByte();
    int filterMethod = readByte();
    int interlaceMethod = readByte();
    bool supportedBitDepth = (bitDepth == 8 || (bitDepth == 4 && colorType == COLOR_TYPE_PALETTE));

	//Simple error check
    if (compressionMethod != 0 || filterMethod != 0 || interlaceMethod != 0 || !supportedBitDepth 
		|| ( colorType != COLOR_TYPE_GRAYSCALE && colorType != COLOR_TYPE_RGB &&  colorType != COLOR_TYPE_PALETTE && 
		     colorType != COLOR_TYPE_GRAYSCALE_WITH_ALPHA && colorType != COLOR_TYPE_RGB_WITH_ALPHA)) 
	{
		//It's an error...
		if (error==FALSE) {
			lstrcpy(errorMsg, _T("PNG header requires unsupported options"));
			error = TRUE;
		}
	}

	//Create a new image
    isOpaque = true;
    if (colorType == COLOR_TYPE_GRAYSCALE_WITH_ALPHA || colorType == COLOR_TYPE_RGB_WITH_ALPHA)
		isOpaque = false;

	imgData = new int[width*height];
}


void PulpCoreFont::readPalette(int length) 
{
	//Palettes must be %3
	if (length%3!=0 && error==FALSE) {
		swprintf(errorMsg, _T("Palette length %i is not mod 3"), length);
		error = TRUE;
		return;
	}

	//Init palette array
	pal_length = length/3;
	palette = new int[pal_length];

	//Fill palette array
    for (int i=0; i<pal_length; i++) {
		int a = 0xff;
        int r = readByte();
        int g = readByte();
        int b = readByte();
            
        palette[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}


void PulpCoreFont::readTransparency(int length) 
{
	//Was the palette valid
	if ((palette == NULL || length > pal_length) && error==FALSE) {
		swprintf(errorMsg, _T("Pallete is null or too short."));
		error = TRUE;
		return;
	}

	//Read data
    for (int i=0; i<length; i++) {
		int a = readByte();
        if (a<0xFF)
			isOpaque = false;
        
		//CoreGraphics.PREMULTIPLIED_ALPHA is always true...
        palette[i] = premultiply((a << 24) | (palette[i] & 0xffffff));
    }
}


void PulpCoreFont::readAnimation() 
{
	//Init
	int numFramesAcross = readShort();
    int numFramesDown = readShort();
    bool loop = (readByte()!=0);
    int frames = readShort();
    int* frameSequence = new int[frames];
    int* frameDuration = new int[frames];
    
	//Read sequences & durations
    for (int i=0; i<frames; i++)
		frameSequence[i] = readShort();
    for (int i=0; i<frames; i++)
        frameDuration[i] = readShort();

	//That's all... we don't support animated fonts for now...
    //AnimatedImage animImage = new AnimatedImage(image, numFramesAcross, numFramesDown);
    //animImage.setSequence(frameSequence, frameDuration, loop);
	delete [] frameSequence;
	delete [] frameDuration;
}


void PulpCoreFont::readData() 
{
	lstrcpy(errorMsg, _T("Read Data not implemented! (Yikes...)"));
	error = TRUE;
}


void PulpCoreFont::fontSet() 
{
	//Read PulpCore's magic number
	for (int i=0; i<8; i++) {
		if (res_data[currPos++] != PULP_MAGICNUM[i] && error==FALSE) {
			swprintf(errorMsg, _T("PULP_MAGICNUM[%i] is %02X not %02X"), i, res_data[currPos-1], PULP_MAGICNUM[i]);
			error = TRUE;
		}
	}

	//"shorts" are always BIG_ENDIAN
	firstChar = readShort();
    lastChar = readShort();
    tracking = readShort();
	bool hasBearing = (readByte()!=0);
    
	//Initialize arrays
    int numChars = lastChar - firstChar + 1;
	num_char_pos = numChars + 1;
    charPositions = new int[num_char_pos];
    bearingLeft = new int[numChars];
    bearingRight = new int[numChars];
     
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
        uppercaseOnly = (lastChar < 'a');
	}
}


int PulpCoreFont::premultiply(int arbg) 
{
	int a = (arbg >> 24) & 0xFF;
	int r = (arbg >> 16) & 0xFF;
	int g = (arbg >> 8) & 0xFF;
    int b = arbg & 0xFF;
    
	r = (a * r + 127) / 255;
    g = (a * g + 127) / 255;
    b = (a * b + 127) / 255;
        
    return (a << 24) | (r << 16) | (g << 8) | b;
}


int PulpCoreFont::readInt() 
{
	int retVal = (((0xFF&res_data[currPos])<<24)  | ((0xFF&res_data[currPos+1])<<16) | ((0xFF&res_data[currPos+2])<<8) | ((0xFF&res_data[currPos+3]))) ;
	currPos += 4;
	return retVal;
}

int PulpCoreFont::readShort() 
{
	int retVal = (((0xFF&res_data[currPos])<<8)  | ((0xFF&res_data[currPos+1])));
	currPos += 2;
	return retVal;
}

int PulpCoreFont::readByte() 
{
	return (0xFF&res_data[currPos++]);
}



/////////////////////
//Properties
/////////////////////
BOOL PulpCoreFont::isInError() 
{
	return error;
}

TCHAR* PulpCoreFont::getErrorMsg() 
{
	return errorMsg;
}

