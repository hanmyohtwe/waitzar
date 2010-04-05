/*
 * Copyright 2010 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#ifndef _TRANSFORM_UNI2AYAR
#define _TRANSFORM_UNI2AYAR

#include <sstream>
#include "Transform/Transformation.h"

/**
 * Enable the "Ayar" encoding
 */
class Uni2Ayar : public Transformation
{
public:
	Uni2Ayar();

	//Convert
	void convertInPlace(std::wstring& src) const;

private:
	bool IsConsonant(wchar_t letter) const;
};


#endif //_TRANSFORM_UNI2AYAR

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

