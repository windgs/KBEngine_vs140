#ifndef _GAME_COMMON_
#define _GAME_COMMON_

#include <string>
#include <Python.h>

#include "async.h"
#include "event_dispatcher.h"

namespace KBEngine {
	namespace script {
		namespace game_common {

			class RedisClient
			{
			protected:
				~RedisClient(){	};
	
			public:
				redisAsyncContext * redisCtx;
				std::string address;
				uint16 port;
				uint32 connId;
				PyObject* connectCallback;
				PyObject* disconnectCallback;

				RedisClient(const char* addr, uint16 port, uint32 connId, PyObject* ccb, PyObject* dcb):
					redisCtx(NULL),
					address(addr),
					port(port),
					connId(connId)
				{
					if(ccb)
						Py_INCREF(ccb);
					if(dcb)
						Py_INCREF(dcb);
					connectCallback = ccb;
					disconnectCallback = dcb;
				};

				void onRedisConnected(const struct redisAsyncContext*, int status);
				void onRedisDisconnected(const struct redisAsyncContext*, int status);
				void doRedisCommand(const char* cmd, PyObject* cmdCallback);
				void onRedisCommandResult(struct redisAsyncContext*, void*, void*);
				bool connectRedis();
				void disconnectRedis();
			};

			typedef std::map<uint32, RedisClient&> ConnectionMap;
			void installModule(Network::EventDispatcher* dispatcher);
		}
	}
}

#endif // !_GAME_COMMON_

