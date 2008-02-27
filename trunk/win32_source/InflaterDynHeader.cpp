/* 
   InflaterDynHeader.cpp v0.0.7
   Copyright (C) 2001 Free Software Foundation, Inc.
   Written by John Leuner for the jazzlib project.

   All rights reserved. (see the NOTICE file in the top directory for more information)

   NOTE: For those unfamiliar with the GNU Classpath project's license, this code
   falls under the "special exception" to the GNU GPL which allows one 
         "to link this library... to produce an executable,... and to copy and distribute 
		  [the .exe] under terms of your choice..."
   The WaitZar project chooses to license the resultant EXE under the general licensing
   terms for the WaitZar project (currently the Apache License 2.0) and to distribute the 
   relevant jazzlib source (as required) under the terms of the GNU Classpath license.

   Please see:
   http://jazzlib.sourceforge.net/
   ...for the latest licensing information regarding the Jazzlib
*/


#include ".\inflaterdynheader.h"


/**
 * 
 */
InflaterDynHeader::InflaterDynHeader()
{
}


/**
 * Avoid memory leaks, if possible.
 */
InflaterDynHeader::~InflaterDynHeader()
{
	if (blLens != NULL)
		delete [] blLens;

	if (litdistLens != NULL)
		delete [] litdistLens;
	
	if (blTree != NULL)
		delete blTree;
}


bool InflaterDynHeader::decode(StreamManipulator &input)
{
//    decode_loop:
    for (;;) {
		switch (mode) {
			case LNUM:
				lnum = input.peekBits(5);
				if (lnum < 0)
					return false;
				lnum += 257;
				input.dropBits(5);
				mode = DNUM;
				//Fall through
			case DNUM:
				dnum = input.peekBits(5);
				if (dnum < 0)
					return false;
				dnum++;
				input.dropBits(5);
				num = lnum+dnum;
				litdistLens = new char[num];
				mode = BLNUM;
				//Fall through
			case BLNUM:
				blnum = input.peekBits(4);
				if (blnum < 0)
					return false;
				blnum += 4;
				input.dropBits(4);
				blLens = new char[19];
				ptr = 0;
				mode = BLLENS;
				//Fall through
			case BLLENS:
			{
				while (ptr < blnum) {
					int len = input.peekBits(3);
					if (len < 0)
						return false;
					input.dropBits(3);
					blLens[BL_ORDER[ptr]] = (byte) len;
					ptr++;
				}
				blTree = new InflaterHuffmanTree(blLens, 19);
				blLens = NULL;
				ptr = 0;
				mode = LENS;
				//Fall through
			}
			case LENS:
			{
				int symbol;
				while (((symbol = blTree->getSymbol(input)) & ~15) == 0) {
					//Normal case: symbol in [0..15]
					litdistLens[ptr++] = (char)( lastLen = (byte) symbol);
					if (ptr == num) {
						//Finished
						return true;
					}
				}
				//need more input?
				if (symbol < 0)
					return false;
	
				//otherwise, repeat code
				if (symbol >= 17) {
					//repeat zero
					lastLen = 0;
				} else {
					/*if (ptr == 0)
						throw new DataFormatException();*/
				}
				repSymbol = symbol-16;
				mode = REPS;
			} //fall through
			case REPS:
			{
				int bits = repBits[repSymbol];
				int count = input.peekBits(bits);
				if (count < 0)
					return false;
				input.dropBits(bits);
				count += repMin[repSymbol];
				/*if (ptr + count > num)
					throw new DataFormatException();*/
				while (count-- > 0)
					litdistLens[ptr++] = (char)lastLen;

				if (ptr == num) {
					//Finished
					return true;
				}
			}
			mode = LENS;
			//goto decode_loop; //Does this actually do anything...?
		}
    }
}


InflaterHuffmanTree* InflaterDynHeader::buildLitLenTree()
{
    char* litlenLens = new char[lnum];
	copyArray(litdistLens, 0, litlenLens, 0, lnum);

	InflaterHuffmanTree* res = new InflaterHuffmanTree(litlenLens, lnum);
	delete [] litlenLens;
	return res;
}


InflaterHuffmanTree* InflaterDynHeader::buildDistTree()
{
    char* distLens = new char[dnum];
    copyArray(litdistLens, lnum, distLens, 0, dnum);

	InflaterHuffmanTree* res = new InflaterHuffmanTree(distLens, dnum);
	delete [] distLens;
	return res;
}

