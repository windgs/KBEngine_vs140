#include <string>
#include <map>
#include <Python.h>
#include <functional>

#include "game_common.h"
#include "kbedispatcher.h"
#include "scriptobject.h"

#if KBE_PLATFORM == PLATFORM_WIN32
#ifdef _DEBUG
#pragma comment (lib, "hiredis_d.lib")
#else
#pragma comment (lib, "hiredis.lib")
#endif
#endif

using namespace std::placeholders;

namespace KBEngine {
namespace script {
namespace game_common {
	
	static ConnectionMap connectionMap = ConnectionMap();
	static uint32 connectionId = 0;
	Network::EventDispatcher* mainLoopDispatcher = NULL;

	static void onRedisConnectedCallback(const struct redisAsyncContext* c, int status)
	{
		if (!c->data)
			return;

		RedisClient* rc = static_cast<RedisClient*>(c->data);
		rc->onRedisConnected(c, status);
	}

	static void onRedisDisconnectedCallback(const struct redisAsyncContext* c, int status)
	{
		if (!c->data)
			return;

		RedisClient* rc = static_cast<RedisClient*>(c->data);
		rc->onRedisDisconnected(c, status);
	}

	static void onRedisCommandResCallback(struct redisAsyncContext* c, void* replyData, void* cmdCallback)
	{
		if (!c->data)
			return;

		RedisClient* rc = static_cast<RedisClient*>(c->data);
		rc->onRedisCommandResult(c, replyData, cmdCallback);
	}

	static PyObject* getRedisReplyResult(redisReply* reply)
	{
		PyObject* result = NULL;
		std::string res;
		switch (reply->type)
		{
		case REDIS_REPLY_STRING:
			result = PyBytes_FromStringAndSize(reply->str, reply->len);
			break;
		case REDIS_REPLY_ARRAY:
			result = PyList_New(reply->elements);
			for (size_t i = 0;i < reply->elements;++i)
			{
				PyList_SetItem(result, i, getRedisReplyResult(reply->element[i]));
			}
			break;
		case REDIS_REPLY_INTEGER:
			result = PyLong_FromLongLong(reply->integer);
			break;
		case REDIS_REPLY_STATUS:
			res.assign(reply->str, reply->len);
			result = PyUnicode_FromString(res.c_str());
			break;
		case REDIS_REPLY_NIL:
			result = PyUnicode_FromString("");
			break;
		default:
			break;
		};
		return result;
	};


	void RedisClient::onRedisConnected(const struct redisAsyncContext* c, int status)
	{	
		if (!this->connectCallback)
			return;
		long connected = 0;
		long lCid = 0;
		if (status == REDIS_OK)
		{
			connected = 1;
			lCid = connId;
		}

		PyObject* cid = PyLong_FromLong(lCid);
		PyObject* isConnected = PyLong_FromLong(connected);
		PyObject* pyArgs = PyTuple_New(2);
		PyTuple_SetItem(pyArgs, 0, cid);
		PyTuple_SetItem(pyArgs, 1, isConnected);
		PyObject* pyRet = PyObject_CallObject(this->connectCallback, pyArgs);
		if (pyRet == NULL)
		{
			SCRIPT_ERROR_CHECK();
		}
		else
		{
			Py_DECREF(pyRet);
		}

		Py_DECREF(pyArgs);
	}

	void RedisClient::onRedisDisconnected(const struct redisAsyncContext* c, int status)
	{
		ConnectionMap::iterator it = connectionMap.find(this->connId);
		if (it != connectionMap.end())
		{
			connectionMap.erase(it);
		}

		if (!this->disconnectCallback)
			return;

		PyObject* cid = PyLong_FromLong(connId);
		PyObject* pyArgs = PyTuple_New(1);
		PyTuple_SetItem(pyArgs, 0, cid);
		PyObject* pyRet = PyObject_CallObject(this->disconnectCallback, pyArgs);

		if (pyRet == NULL)
		{
			SCRIPT_ERROR_CHECK();
		}
		else
		{
			Py_DECREF(pyRet);
		}

		Py_DECREF(pyArgs);
		Py_DECREF(this->connectCallback);
		Py_DECREF(this->disconnectCallback);
		delete(this);
	}

	bool RedisClient::connectRedis()
	{
		DEBUG_MSG(fmt::format("connect redis::{}:{} ...\n", this->address, this->port));
		redisAsyncContext* c = redisAsyncConnect(address.c_str(), port);
		if (c->err)
		{
			ERROR_MSG(fmt::format("RedisClient::connectRedis: errno={}, error={}\n",
				c->err, c->errstr));

			redisAsyncFree(c);
			return false;
		}
		c->data = this;
		this->redisCtx = c;
		redisKBEAttach(c, mainLoopDispatcher);
		redisAsyncSetConnectCallback(c, onRedisConnectedCallback);
		redisAsyncSetDisconnectCallback(c, onRedisDisconnectedCallback);

		redisAsyncHandleWrite(c);
		return true;
	}

	void RedisClient::disconnectRedis()
	{
		this->redisCtx->data = NULL;
		redisAsyncDisconnect(this->redisCtx);
		onRedisDisconnected(NULL, REDIS_OK);
	}

	void RedisClient::doRedisCommand(const char* cmd, PyObject* cmdCallback)
	{
		redisAsyncCommand(this->redisCtx, onRedisCommandResCallback, cmdCallback, cmd);
	}

	void RedisClient::onRedisCommandResult(struct redisAsyncContext*, void* replyData, void* cmdCallback)
	{
		if (!cmdCallback)
			return;

		redisReply* reply = static_cast<redisReply*>(replyData);
		std::string error = "";
		
		PyObject* result=NULL;
		if (reply->type == REDIS_REPLY_ERROR)
		{
			error.assign(reply->str, reply->len);
			result = Py_None;
			Py_INCREF(result);
			ERROR_MSG(fmt::format("RedisClient::onRedisCommandResult: err={}\n", error.c_str()));
		}
		else
		{
			result = getRedisReplyResult(reply);
		}
		
		PyObject* pyCallback = static_cast<PyObject*>(cmdCallback);

		if (pyCallback && pyCallback != Py_None)
		{
			PyObject* cid = PyLong_FromLong(connId);
			PyObject* pyError = PyUnicode_FromString(error.c_str());

			PyObject* pyArgs = PyTuple_New(3);
			PyTuple_SetItem(pyArgs, 0, cid);
			PyTuple_SetItem(pyArgs, 1, pyError);
			PyTuple_SetItem(pyArgs, 2, result);

			PyObject* pyRet = PyObject_CallObject(pyCallback, pyArgs);
			if (pyRet == NULL)
			{
				SCRIPT_ERROR_CHECK();
			}
			else
			{
				Py_DECREF(pyRet);
			}

			Py_DECREF(pyArgs);
		}
		else
			Py_XDECREF(result);

		Py_XDECREF(pyCallback);
	}

	//-------------------------------------------------------------------------------------------
	
	static PyObject* pyConnectRedis(PyObject* self, PyObject* args)
	{
		char* redisAddr=NULL;
		uint16 redisPort = 0;
		PyObject* connectCallback=NULL;
		PyObject* disconnectCallback=NULL;

		int argCount = PyTuple_Size(args);
		if (argCount != 4)
		{
			PyErr_Format(PyExc_AssertionError, "GameCommon::connectRedis: (argssize[address, port, connectCallback, disconnectCallback] is error!");
			PyErr_PrintEx(0);
			return NULL;
		}

		if (PyArg_ParseTuple(args, "s|H|O|O", &redisAddr, &redisPort, &connectCallback, &disconnectCallback) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "GameCommon::connectRedis: (parse args error. [address, port, connectCallback, disconnectCallback]!");
			PyErr_PrintEx(0);
			return NULL;
		}
		if (redisPort == 0)
			redisPort = 6379;

		if (!PyCallable_Check(connectCallback))
			connectCallback = NULL;

		if (!PyCallable_Check(disconnectCallback))
			disconnectCallback = NULL;

		++connectionId;
		uint32 cid = connectionId;
		RedisClient* c = new RedisClient(redisAddr, redisPort, cid, connectCallback, disconnectCallback);
		connectionMap.insert(std::pair<uint32, RedisClient&>(cid, *c));
		
		if (c->connectRedis())
		{	
		}
		else
		{
			c->onRedisConnected(NULL, REDIS_ERR);
		}
		
		return PyLong_FromLong(cid);
	}
	static PyObject* pyDisconnectRedis(PyObject* self, PyObject* args)
	{
		int argCount = PyTuple_Size(args);
		if (argCount != 1)
		{
			PyErr_Format(PyExc_AssertionError, "GameCommon::connectRedis: (argssize[cid] is error!");
			PyErr_PrintEx(0);
			return NULL;
		}
		
		uint32 cid = 0;
		if (PyArg_ParseTuple(args, "I", &cid) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "GameCommon::connectRedis: (parse args error. [cid]!");
			PyErr_PrintEx(0);
			return NULL;
		}

		ConnectionMap::iterator it = connectionMap.find(cid);
		if (it != connectionMap.end())
		{
			it->second.disconnectRedis();
		}
		
		S_Return;
	}
	static PyObject* pyExecuteRawRedis(PyObject* self, PyObject* args)
	{
		size_t argCnt = PyTuple_Size(args);
		if (argCnt != 3 && argCnt != 2)
		{
			PyErr_Format(PyExc_AssertionError, "GameCommon::pyExecuteRawRedis: (argssize[cid, cmd, callback] is error!");
			PyErr_PrintEx(0);
			return NULL;
		}
		uint32 cid = 0;
		const char *cmd = NULL;
		PyObject* resultCallback = NULL;

		if (argCnt == 3)
		{
			PyArg_ParseTuple(args, "I|s|O", &cid, &cmd, &resultCallback);
			Py_INCREF(resultCallback);
		}
		else if (argCnt == 2)
		{
			PyArg_ParseTuple(args, "I|s", &cid, &cmd);
		}
	
		ConnectionMap::iterator it = connectionMap.find(cid);
		if (it != connectionMap.end())
		{
			it->second.doRedisCommand(cmd, resultCallback);
		}
		S_Return;
	}

	void installModule(Network::EventDispatcher* dispatcher)
	{
		PyObject* mod = PyImport_AddModule("GameCommon");
		APPEND_SCRIPT_MODULE_METHOD(mod, connectRedis, pyConnectRedis, METH_VARARGS, 0);
		APPEND_SCRIPT_MODULE_METHOD(mod, disconnectRedis, pyDisconnectRedis, METH_VARARGS, 0);
		APPEND_SCRIPT_MODULE_METHOD(mod, executeRawRedis, pyExecuteRawRedis, METH_VARARGS, 0);

		mainLoopDispatcher = dispatcher;
	}
}
}
}
