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

#ifndef KBE_CLIENT_SDK_BOT_H
#define KBE_CLIENT_SDK_BOT_H

#include "client_sdk.h"
#include "common/common.h"
#include "helper/debug_helper.h"

namespace KBEngine{

class BotClient : public ClientSDK
{
public:
	BotClient();
	virtual ~BotClient();

	virtual std::string name() const {
		return "botClient";
	}

	virtual bool create(const std::string& path);
	virtual void onCreateEntityModuleFileName(const std::string& moduleName);
	virtual bool writeEntityModuleBegin(ScriptDefModule* pEntityScriptDefModule);
	virtual bool writeEntityPropertys(ScriptDefModule* pEntityScriptDefModule,ScriptDefModule* pCurrScriptDefModule);
	virtual bool writeEntityModuleEnd(ScriptDefModule* pEntityScriptDefModule);
	virtual bool writeEntityMethods(ScriptDefModule* pEntityScriptDefModule,ScriptDefModule* pCurrScriptDefModule);
	virtual bool writeEntityProcessMessagesMethod(ScriptDefModule* pEntityScriptDefModule);

protected:
	std::string initBody_;
};

}
#endif
