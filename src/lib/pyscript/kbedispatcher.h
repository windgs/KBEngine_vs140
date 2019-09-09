#ifndef __KBE_EVENT_DISPATCHER_H__
#define __KBE_EVENT_DISPATCHER_H__
#include "hiredis.h"
#include "async.h"
#include "interfaces.h"
#include "event_dispatcher.h"

namespace KBEngine {

	typedef struct redisKBEEvents {
		redisAsyncContext *context;
		Network::EventDispatcher *dispatcher;
		void* readHandler;
		void* writeHandler;
	} redisKBEEvents;

	static void redisKBEReadEvent(int fd, redisKBEEvents *arg) {
		((void)fd);
		redisKBEEvents *e = arg;
		redisAsyncHandleRead(e->context);
	}

	static void redisKBEWriteEvent(int fd, redisKBEEvents *arg) {
		((void)fd);
		redisKBEEvents *e = arg;
		redisAsyncHandleWrite(e->context);
	}

	class RedisReadHandler :public Network::InputNotificationHandler
	{
	public:
		redisKBEEvents * redisEvent;
		RedisReadHandler(redisKBEEvents* e) :redisEvent(e) {};
		virtual int handleInputNotification(int fd)
		{
			redisKBEReadEvent(fd, redisEvent);
			return 0;
		};
	};

	class RedisWriteHandler :public Network::OutputNotificationHandler
	{
	public:
		redisKBEEvents * redisEvent;
		RedisWriteHandler(redisKBEEvents* e) :redisEvent(e) {};
		virtual int handleOutputNotification(int fd)
		{
			redisKBEWriteEvent(fd, redisEvent);
			return 0;
		};
	};

	static void redisKBEAddRead(void *privdata) {
		redisKBEEvents *e = (redisKBEEvents*)privdata;
		redisContext *c = &(e->context->c);
		e->dispatcher->registerReadFileDescriptor(c->fd, (RedisReadHandler*)e->readHandler);
	}

	static void redisKBEDelRead(void *privdata) {
		redisKBEEvents *e = (redisKBEEvents*)privdata;
		redisContext *c = &(e->context->c);
		e->dispatcher->deregisterReadFileDescriptor(c->fd);
	}

	static void redisKBEAddWrite(void *privdata) {
		redisKBEEvents *e = (redisKBEEvents*)privdata;
		redisContext *c = &(e->context->c);
		e->dispatcher->registerWriteFileDescriptor(c->fd, (RedisWriteHandler*)e->writeHandler);
	}

	static void redisKBEDelWrite(void *privdata) {
		redisKBEEvents *e = (redisKBEEvents*)privdata;
		redisContext *c = &(e->context->c);
		e->dispatcher->deregisterWriteFileDescriptor(c->fd);
	}

	static void redisKBECleanup(void *privdata) {
		redisKBEEvents *e = (redisKBEEvents*)privdata;
		delete (RedisReadHandler*)e->readHandler;
		delete (RedisWriteHandler*)e->writeHandler;

		redisContext *c = &(e->context->c);
		e->dispatcher->deregisterReadFileDescriptor(c->fd);
		e->dispatcher->deregisterWriteFileDescriptor(c->fd);

		free(e);
	}

	static int redisKBEAttach(redisAsyncContext *ac, Network::EventDispatcher *dispatcher) {
		redisKBEEvents *e;

		/* Nothing should be attached when something is already attached */
		if (ac->ev.data != NULL)
			return REDIS_ERR;

		/* Create container for context and r/w events */

		e = (redisKBEEvents*)malloc(sizeof(*e));
		e->dispatcher = dispatcher;
		e->readHandler = new RedisReadHandler(e);
		e->writeHandler = new RedisWriteHandler(e);
		e->context = ac;

		/* Register functions to start/stop listening for events*/
		ac->ev.addRead = redisKBEAddRead;
		ac->ev.delRead = redisKBEDelRead;
		ac->ev.addWrite = redisKBEAddWrite;;
		ac->ev.delWrite = redisKBEDelWrite;
		ac->ev.cleanup = redisKBECleanup;
		ac->ev.data = e;

		return REDIS_OK;
	}
}
#endif
