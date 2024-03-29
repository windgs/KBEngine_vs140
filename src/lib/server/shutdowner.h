// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com

#ifndef KBE_SHUTDOWNER_HANDLER_H
#define KBE_SHUTDOWNER_HANDLER_H

#include "common/common.h"
#include "common/timer.h"
#include "helper/debug_helper.h"
#include "server/shutdown_handler.h"

namespace KBEngine { 
namespace Network{
	class EventDispatcher;
}

class Shutdowner: public TimerHandler
{
public:
	enum TimeOutType
	{
		TIMEOUT_SHUTDOWN_TICK,		
		//add by huyf 2019.07.17:关服时base销毁实体可能会未全部安全销毁
		TIMEOUT_SHUTDOWN_READY_END_TICK,
		//add end:关服时base销毁实体可能会未全部安全销毁
		TIMEOUT_SHUTDOWN_END_TICK,
		TIMEOUT_MAX
	};
	
	Shutdowner(ShutdownHandler* pShutdownHandler);
	virtual ~Shutdowner();
	
	void shutdown(float period, float tickPeriod, Network::EventDispatcher& dispatcher);
	void cancel();

protected:
	virtual void handleTimeout(TimerHandle handle, void * arg);

	ShutdownHandler* pShutdownHandler_;
	TimerHandle pTimerHandle_;
	Network::EventDispatcher* pDispatcher_;
	float tickPeriod_;	
	//add by huyf 2019.07.17:关服时base销毁实体可能会未全部安全销毁
	int	shutDownCount_;
	//add end:关服时base销毁实体可能会未全部安全销毁
};

}

#endif // KBE_SHUTDOWNER_HANDLER_H
