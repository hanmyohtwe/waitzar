/*
 * Copyright 2009 by Seth N. Hetu
 *
 * Please refer to the end of the file for licensing information
 */

#ifndef _CONFIGMANAGER
#define _CONFIGMANAGER

#include <vector>
#include "wz_utilities.h"
#include "json_spirit.h"
#include "Interfaces.h"
using json_spirit::wValue;
using json_spirit::read;


//Simple class to help us load json files easier
class JsonFile {
public:
	JsonFile(const std::string& path="")
	{
		this->path = path;
	}
	wValue json() 
	{
		if (!hasParsed) {
			const std::wstring text = waitzar::readUTF8File(path);
			read(text, parsed);
			hasParsed = true;
		}
		return parsed;
	}
	//For map indexing:
	bool operator<(const JsonFile& j) const
	{
		return this->path < j.path;
	}
private:
	std::string path;
	wValue parsed;
	bool hasParsed;
};







//Options for our ConfigManager class
struct Settings {
	Option<std::wstring> hotkey;
	Option<bool> silenceMywordsErrors;
	Option<bool> balloonStart;
	Option<bool> alwaysElevate;
	Option<bool> trackCaret;
	Option<bool> lockWindows;
};
struct Language {
	Option<std::wstring> displayName;
	Option<std::wstring> defaultOutputEncoding;
	Option<std::wstring> defaultDisplayMethod;
	Option<std::wstring> defaultInputMethod;
	std::map<std::wstring, InputMethod*> inputMethods;
	std::map<std::wstring, Encoding> encodings;
	std::map<std::wstring, Transformation*> transformations;
	std::map<std::wstring, DisplayMethod*> displayMethods;
};
struct OptionTree {
	Settings settings;
	std::map<std::wstring, Language> languages;
};



/**
 * This class exists for managing configuration options automatically. It is also
 *  the main access point for WaitZar into the SpiritJSON library, without requiring 
 *  us to link against Boost.
 */
class ConfigManager
{
public:
	ConfigManager(void);
	~ConfigManager(void);

	//Build our config. manager up slowly
	void initMainConfig(const std::string& configFile);
	void initAddLanguage(const std::string& configFile, const std::vector<std::string>& subConfigFiles);
	void initLocalConfig(const std::string& configFile);
	void initUserConfig(const std::string& configFile);

	//Accessible by our outside class
	std::vector< std::pair<std::wstring, std::wstring> > getSettings() const;
	std::vector<std::wstring> getLanguages() const;
	std::vector<std::wstring> getInputManagers() const;
	std::vector<std::wstring> getEncodings() const;

	//Control
	std::wstring getActiveLanguage() const;
	void changeActiveLanguage(const std::wstring& newLanguage);

private:
	//Our many config files.
	JsonFile mainConfig;
	std::map<JsonFile , std::vector<JsonFile> > langConfigs;
	JsonFile localConfig;
	JsonFile userConfig;

	//The actual representation
	OptionTree options;
};




#endif //_CONFIGMANAGER

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

