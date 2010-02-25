/*
 * Copyright 2010 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#ifndef _SETTINGS_ENCODING
#define _SETTINGS_ENCODING

struct Encoding {
	std::wstring id;
	bool canUseAsOutput;
	std::wstring displayName;
	std::wstring initial;
	std::wstring imagePath;

	//Allow map comparison 
	bool operator<(const Encoding& other) const {
		return id < other.id;
	}

	//Allow logical equals and not equals
	bool operator==(const Encoding &other) const {
		return id == other.id;
	}
	bool operator!=(const Encoding &other) const {
		return id != other.id;
	}

	//Allow eq/neq on strings, too
	bool operator==(const std::wstring& other) const {
		return id == other;
	}
	bool operator!=(const std::wstring& other) const {
		return id != other;
	}
};



#endif //_SETTINGS_ENCODING

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

