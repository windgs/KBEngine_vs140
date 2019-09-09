/*
This source file is part of KBEngine
For the latest info, see http://www.kbengine.org/

Copyright (c) 2008-2018 KBEngine.

KBEngine is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

KBEngine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public License
along with KBEngine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "kbcmd.h"
#include "client_sdk.h"
#include "client_bot_py.h"	
#include "entitydef/entitydef.h"
#include "entitydef/scriptdef_module.h"
#include "entitydef/property.h"
#include "entitydef/method.h"
#include "entitydef/datatype.h"
#include "network/fixed_messages.h"

#ifdef _WIN32  
#include <direct.h>  
#include <io.h>  
#elif _LINUX  
#include <stdarg.h>  
#include <sys/stat.h>  
#endif  

namespace KBEngine {	

static std::string headerBody = "";
static std::string moduleSuffix = "Base";
static std::string indent = "    ";

//-------------------------------------------------------------------------------------
BotClient::BotClient():
	ClientSDK(),
	initBody_()
{
}

//-------------------------------------------------------------------------------------
BotClient::~BotClient()
{

}

bool BotClient::create(const std::string& path)
{
	basepath_ = path;

	if (basepath_[basepath_.size() - 1] != '\\' && basepath_[basepath_.size() - 1] != '/')
		basepath_ += "/";

	currHeaderPath_ = currSourcePath_ = basepath_;


	const EntityDef::SCRIPT_MODULES& scriptModules = EntityDef::getScriptModules();
	generateCommonMap(scriptModules);
	//writeEntityCommonClass();

	EntityDef::SCRIPT_MODULES::const_iterator moduleIter = scriptModules.begin();
	for (; moduleIter != scriptModules.end(); ++moduleIter)
	{
		ScriptDefModule* pScriptDefModule = (*moduleIter).get();
		if (!pScriptDefModule->hasClient())
			continue;

		if (!writeEntityModule(pScriptDefModule))
			return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------
void BotClient::onCreateEntityModuleFileName(const std::string& moduleName)
{
	sourcefileName_ = moduleName + moduleSuffix+ ".py";
}


//-------------------------------------------------------------------------------------
bool BotClient::writeEntityModuleBegin(ScriptDefModule* pEntityScriptDefModule)
{
	std::string newModuleName = fmt::format("{}{}", pEntityScriptDefModule->getName(), moduleSuffix);

	sourcefileBody_ = headerBody;
	sourcefileBody_ += "# -*- encoding:utf-8 -*-\n";
	sourcefileBody_ += std::string("# defined in */scripts/entity_defs/") + pEntityScriptDefModule->getName() + ".def\n";
	sourcefileBody_ += std::string("# It is generated automatically, please do not modify manually\n");
	sourcefileBody_ += "import KBEngine\n\n";

	sourcefileBody_ += fmt::format("class {}(KBEngine.Entity):", newModuleName);

	return true;
}

bool BotClient::writeEntityPropertys(ScriptDefModule* pEntityScriptDefModule,
	ScriptDefModule* pCurrScriptDefModule)
{
	return true;
}

//-------------------------------------------------------------------------------------
bool BotClient::writeEntityModuleEnd(ScriptDefModule* pEntityScriptDefModule)
{
	return true;
}

bool BotClient::writeEntityMethods(ScriptDefModule* pEntityScriptDefModule,
	ScriptDefModule* pCurrScriptDefModule)
{
	sourcefileBody_ += "\n";
	headerfileBody_ += "\n";

	ScriptDefModule::METHODDESCRIPTION_MAP& clientMethods = pCurrScriptDefModule->getClientMethodDescriptions();
	ScriptDefModule::METHODDESCRIPTION_MAP::iterator methodIter = clientMethods.begin();
	if (methodIter == clientMethods.end())
		sourcefileBody_ += fmt::format("{}pass\n", indent);

	for (; methodIter != clientMethods.end(); ++methodIter)
	{
		MethodDescription* pMethodDescription = methodIter->second;

		sourcefileBody_ += fmt::format("{}def {}({}): pass\n", indent, pMethodDescription->getName(), "#REPLACE#");

		std::string argsBody = "self";

		std::vector<DataType*>& argTypes = pMethodDescription->getArgTypes();

		size_t argCnt = argTypes.size();
		for (size_t i=0; i<argCnt; i++)
		{
			argsBody += fmt::format(", arg{}", i);
		}

		strutil::kbe_replace(sourcefileBody_, "#REPLACE#", argsBody);
	}

	return true;
}

bool BotClient::writeEntityProcessMessagesMethod(ScriptDefModule* pEntityScriptDefModule)
{
	return true;
}

//-------------------------------------------------------------------------------------
}
