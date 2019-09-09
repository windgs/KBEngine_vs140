// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com

#include "cellapp.h"
#include "spacememory.h"	
#include "entity.h"
#include "witness.h"	
#include "navigation/navigation.h"
#include "loadnavmesh_threadtasks.h"
#include "entitydef/entities.h"
#include "client_lib/client_interface.h"
#include "network/network_stats.h"

#include "../../server/baseappmgr/baseappmgr_interface.h"
#include "../../server/cellappmgr/cellappmgr_interface.h"
#include "../../server/baseapp/baseapp_interface.h"
#include "../../server/cellapp/cellapp_interface.h"
#include "../../server/dbmgr/dbmgr_interface.h"

#include "navigation/navigation_tile_handle.h"
#include "navigation/AirWall.h"
#include "loadairwalls_threadtasks.h"
#include "loadmultipletiles_threadtasks.h"
#include "math/math.h"
#include "Python.h"

namespace KBEngine{	

//-------------------------------------------------------------------------------------
SpaceMemory::SpaceMemory(SPACE_ID spaceID, const std::string& scriptModuleName) :
id_(spaceID),
scriptModuleName_(scriptModuleName),
entities_(),
hasGeometry_(false),
pCell_(NULL),
coordinateSystem_(),
pNavHandle_(),
state_(STATE_NORMAL),
destroyTime_(0),
layerOne_()
{
	Network::Channel* pChannel = Components::getSingleton().getCellappmgrChannel();
	if (pChannel != NULL)
	{
		Network::Bundle* pBundle = Network::Bundle::createPoolObject(OBJECTPOOL_POINT);
		(*pBundle).newMessage(CellappmgrInterface::updateSpaceData);
		
		(*pBundle) << g_componentID;
		(*pBundle) << id_;
		(*pBundle) << scriptModuleName_;
		(*pBundle) << false;
		(*pBundle) << "";

		pChannel->send(pBundle);
	}
}

//-------------------------------------------------------------------------------------
SpaceMemory::~SpaceMemory()
{
	_clearGhosts();
	entities_.clear();
	
	this->coordinateSystem_.releaseNodes();
	
	pNavHandle_.clear();

	SAFE_RELEASE(pCell_);	

	Network::Channel* pChannel = Components::getSingleton().getCellappmgrChannel();
	if (pChannel != NULL)
	{
		Network::Bundle* pBundle = Network::Bundle::createPoolObject(OBJECTPOOL_POINT);
		(*pBundle).newMessage(CellappmgrInterface::updateSpaceData);

		(*pBundle) << g_componentID;
		(*pBundle) << id_;
		(*pBundle) << scriptModuleName_;
		(*pBundle) << true;
		(*pBundle) << "";

		pChannel->send(pBundle);
	}
}

//-------------------------------------------------------------------------------------
void SpaceMemory::_clearGhosts()
{
	// 因为space在destroy时做过一次清理，因此这里理论上剩下的是ghosts实体
	if(entities_.size() == 0)
		return;
	
	std::vector<ENTITY_ID> entitieslog;
	
	SPACE_ENTITIES::const_iterator log_iter = this->entities().begin();
	for(; log_iter != this->entities().end(); ++log_iter)
		entitieslog.push_back((*log_iter).get()->id());

	std::vector<ENTITY_ID>::iterator iter = entitieslog.begin();
	for(; iter != entitieslog.end(); ++iter)
	{
		Entity* entity = Cellapp::getSingleton().findEntity((*iter));
		if(entity != NULL && !entity->isDestroyed() && entity->spaceID() == this->id())
		{
			entity->destroyEntity();
		}
		else
		{
			entity = findEntity((*iter));
			if(entity != NULL && !entity->isDestroyed() && entity->spaceID() == this->id())
			{
				removeEntity(entity);
			}
		}
	}
	
	entities_.clear();	
}

//-------------------------------------------------------------------------------------
PyObject* SpaceMemory::__py_GetSpaceGeometryMapping(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;

	if(PyTuple_Size(args) != 1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::getSpaceGeometryMapping: (argssize != 1) error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if(PyArg_ParseTuple(args, "I", &spaceID) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::getSpaceGeometryMapping: args error!");
		PyErr_PrintEx(0);
		return 0;
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if(space == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::getSpaceGeometryMapping: (spaceID=%u) not found!", 
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	return PyUnicode_FromString(space->getGeometryPath().c_str());
}

//-------------------------------------------------------------------------------------
PyObject* SpaceMemory::__py_AddSpaceGeometryMapping(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;
	char* path = NULL;
	bool shouldLoadOnServer = true;
	PyObject* mapper = NULL;
	PyObject* py_params = NULL;
	std::map< int, std::string > params;
	
	int argCount = PyTuple_Size(args);
	if(argCount < 3 || argCount > 5)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceGeometryMapping: (argssize[spaceID, mapper, path, shouldLoadOnServer] < 3 || > 5) error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if(argCount == 4)
	{
		if(PyArg_ParseTuple(args, "I|O|s|b", &spaceID, &mapper, &path, &shouldLoadOnServer) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceGeometryMapping: args error!");
			PyErr_PrintEx(0);
			return 0;
		}
	}
	else if(argCount == 5)
	{
		if(PyArg_ParseTuple(args, "I|O|s|b|O", &spaceID, &mapper, &path, &shouldLoadOnServer, &py_params) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceGeometryMapping: args error!");
			PyErr_PrintEx(0);
			return 0;
		}
		
		if(py_params)
		{
			PyObject *key, *value;
			Py_ssize_t pos = 0;

			if(!PyDict_Check(py_params))
			{
				PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceGeometryMapping: args(params) not is PyDict!");
				PyErr_PrintEx(0);					
				return 0;
			}

			while(PyDict_Next(py_params, &pos, &key, &value)) 
			{
				if(!PyLong_Check(key) || !PyUnicode_Check(value))
				{
					PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceGeometryMapping: args(params) error!");
					PyErr_PrintEx(0);					
					return 0;
				}
				
				params[PyLong_AsLong(key)] = PyUnicode_AsUTF8AndSize(value, NULL);
			}
			
			SCRIPT_ERROR_CHECK();
		}
	}
	else
	{
		if(PyArg_ParseTuple(args, "I|O|s", &spaceID, &mapper, &path) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceGeometryMapping: args wrong!");
			PyErr_PrintEx(0);
			return 0;
		}
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if(space == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceGeometryMapping: spaceID error! spaceID=%u respath=%s", 
			spaceID, path);

		PyErr_PrintEx(0);
		return 0;
	}

	SCRIPT_ERROR_CHECK();

	if (Resmgr::getSingleton().matchPath(path).size() == 0)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceGeometryMapping: path error! spaceID=%u respath=%s",
			spaceID, path);

		PyErr_PrintEx(0);
		return 0;
	}

	if(!space->addSpaceGeometryMapping(path, shouldLoadOnServer, params))
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceGeometryMapping: (spaceID=%u respath=%s) error!", 
			spaceID, path);

		PyErr_PrintEx(0);
		return 0;
	}

	S_Return;
}

//-------------------------------------------------------------------------------------
bool SpaceMemory::addSpaceGeometryMapping(std::string respath, bool shouldLoadOnServer, const std::map< int, std::string >& params)
{
	INFO_MSG(fmt::format("KBEngine::addSpaceGeometryMapping: spaceID={}, respath={}, shouldLoadOnServer={}!\n",
		id(), respath, shouldLoadOnServer));

	hasGeometry_ = true;
	if(getGeometryPath() == respath)
	{
		WARNING_MSG(fmt::format("KBEngine::addSpaceGeometryMapping: spaceID={}, respath={} exist!\n",
			id(), respath));

		return true;
	}

	setGeometryPath(respath);

	if(shouldLoadOnServer)
		loadSpaceGeometry(params);

	return true;
}

//-------------------------------------------------------------------------------------
void SpaceMemory::loadSpaceGeometry(const std::map< int, std::string >& params)
{
	KBE_ASSERT(pNavHandle_ == NULL);
	Cellapp::getSingleton().threadPool().addTask(new LoadNavmeshTask(getGeometryPath(), this->id(), params));
}

//-------------------------------------------------------------------------------------
void SpaceMemory::unLoadSpaceGeometry()
{
	Network::Channel* pChannel = Components::getSingleton().getCellappmgrChannel();
	if (pChannel != NULL)
	{
		Network::Bundle* pBundle = Network::Bundle::createPoolObject(OBJECTPOOL_POINT);
		(*pBundle).newMessage(CellappmgrInterface::updateSpaceData);

		(*pBundle) << g_componentID;
		(*pBundle) << id_;
		(*pBundle) << scriptModuleName_;
		(*pBundle) << false;
		(*pBundle) << "";

		pChannel->send(pBundle);
	}
}

//-------------------------------------------------------------------------------------
void SpaceMemory::onLoadedSpaceGeometryMapping(NavigationHandlePtr pNavHandle)
{
	pNavHandle_ = pNavHandle;
	INFO_MSG(fmt::format("KBEngine::onLoadedSpaceGeometryMapping: spaceID={}, respath={}!\n",
			id(), getGeometryPath()));

	// 通知脚本
	{
		SCOPED_PROFILE(SCRIPTCALL_PROFILE);
		SCRIPT_OBJECT_CALL_ARGS2(Cellapp::getSingleton().getEntryScript().get(), const_cast<char*>("onSpaceGeometryLoaded"), 
			const_cast<char*>("Is"), this->id(), getGeometryPath().c_str(), false);
	}

	onAllSpaceGeometryLoaded();

	Network::Channel* pChannel = Components::getSingleton().getCellappmgrChannel();
	if (pChannel != NULL)
	{
		Network::Bundle* pBundle = Network::Bundle::createPoolObject(OBJECTPOOL_POINT);
		(*pBundle).newMessage(CellappmgrInterface::updateSpaceData);

		(*pBundle) << g_componentID;
		(*pBundle) << id_;
		(*pBundle) << scriptModuleName_;
		(*pBundle) << false;
		(*pBundle) << getGeometryPath();

		pChannel->send(pBundle);
	}
}

//-------------------------------------------------------------------------------------
void SpaceMemory::onAllSpaceGeometryLoaded()
{
	SCOPED_PROFILE(SCRIPTCALL_PROFILE);

	// 通知脚本
	SCRIPT_OBJECT_CALL_ARGS3(Cellapp::getSingleton().getEntryScript().get(), const_cast<char*>("onAllSpaceGeometryLoaded"), 
		const_cast<char*>("Iis"), this->id(), true, getGeometryPath().c_str(), false);
}

//-------------------------------------------------------------------------------------
bool SpaceMemory::update()
{
	if(destroyTime_ > 0 && timestamp() - destroyTime_ >= uint64( 3.f * stampsPerSecond() ))
	{
		if(entities_.size() == 0)
			return false;
	}

	this->coordinateSystem_.releaseNodes();

	if(destroyTime_ > 0 && timestamp() - destroyTime_ >= uint64( 30.f * stampsPerSecond() ))
	{
		_clearGhosts();
		KBE_ASSERT(entities_.size() == 0);
		this->coordinateSystem_.releaseNodes();
	}
		
	return true;
}

//-------------------------------------------------------------------------------------
void SpaceMemory::addEntityAndEnterWorld(Entity* pEntity, bool isRestore)
{
	addEntity(pEntity);
	addEntityToNode(pEntity);
	onEnterWorld(pEntity);
}

//-------------------------------------------------------------------------------------
void SpaceMemory::addEntity(Entity* pEntity)
{
	pEntity->spaceID(this->id_);
	pEntity->spaceEntityIdx(entities_.size());
	entities_.push_back(pEntity);
	pEntity->onEnterSpace(this);
}

//-------------------------------------------------------------------------------------
void SpaceMemory::addEntityToNode(Entity* pEntity)
{
	pEntity->installCoordinateNodes(&coordinateSystem_);
}

//-------------------------------------------------------------------------------------
void SpaceMemory::removeEntity(Entity* pEntity)
{
	KBE_ASSERT(pEntity->spaceID() == id());

	pEntity->spaceID(0);
	
	// 先获取到所在位置
	SPACE_ENTITIES::size_type idx = pEntity->spaceEntityIdx();

	KBE_ASSERT(idx < entities_.size());
	KBE_ASSERT(entities_[ idx ] == pEntity);

	// 如果有2个或以上的entity则将最后一个entity移至删除的这个entity所在位置
	Entity* pBack = entities_.back().get();
	pBack->spaceEntityIdx(idx);
	entities_[idx] = pBack;
	pEntity->spaceEntityIdx(SPACE_ENTITIES::size_type(-1));
	entities_.pop_back();

	onLeaveWorld(pEntity);

	// 这句必须在onLeaveWorld之后， 因为可能rangeTrigger需要参考pEntityCoordinateNode
	pEntity->uninstallCoordinateNodes(&coordinateSystem_);
	pEntity->onLeaveSpace(this);

	// 如果没有entity了则需要销毁space, 因为space最少存在一个entity
	if(entities_.empty() && state_ == STATE_NORMAL)
	{
		SpaceMemorys::destroySpace(this->id(), 0);
	}
}

//-------------------------------------------------------------------------------------
void SpaceMemory::_onEnterWorld(Entity* pEntity)
{
	if(!pEntity->isReal() || !pEntity->pScriptModule()->hasClient())
		return;

	if(pEntity->hasWitness())
	{
		_addSpaceDatasToEntityClient(pEntity);
		pEntity->pWitness()->onEnterSpace(this);
	}
}

//-------------------------------------------------------------------------------------
void SpaceMemory::onEnterWorld(Entity* pEntity)
{
	KBE_ASSERT(pEntity != NULL);
	
	// 如果是一个有Witness(通常是玩家)则需要将当前场景已经创建的有client部分的entity广播给他
	// 否则是一个普通的entity进入世界， 那么需要将这个entity广播给所有看见他的有Witness的entity。
	if(pEntity->hasWitness())
	{
		_onEnterWorld(pEntity);
	}
}

//-------------------------------------------------------------------------------------
void SpaceMemory::onEntityAttachWitness(Entity* pEntity)
{
	KBE_ASSERT(pEntity != NULL && pEntity->hasWitness());
	_onEnterWorld(pEntity);
}

//-------------------------------------------------------------------------------------
void SpaceMemory::onLeaveWorld(Entity* pEntity)
{
	if(!pEntity->isReal() || !pEntity->pScriptModule()->hasClient())
		return;
	
	// 向其他人客户端广播自己的离开
	// 向客户端发送onLeaveWorld消息
	if(pEntity->hasWitness())
	{
		pEntity->pWitness()->onLeaveSpace(this);
	}
}

int SpaceMemory::raycast(int layer, const Position3D& start, const Position3D& end, std::vector<Position3D>& hitPointVec)
{
	if (!pNavHandle_)
	{
		WARNING_MSG(fmt::format("Space::raycast(): space({}), not found navhandle!\n",
			id_));
		return -1;
	}

	if (layer == 1)
	{
		pNavHandle_->setLayerOne(&layerOne_);
		int ret = pNavHandle_->raycast(layer, start, end, hitPointVec);
		pNavHandle_->setLayerOne(NULL);
		return ret;
	}
	else
	{
		return pNavHandle_->raycast(layer, start, end, hitPointVec);
	}

}

int SpaceMemory::findStraightPath(CoordinateNode* pFromNode, int layer, const Position3D& start, const Position3D& end, std::vector<Position3D>& paths)
{
	if (!pNavHandle_)
	{
		WARNING_MSG(fmt::format("Space::findStraightPath(): space({}), not found navhandle!\n",
			id_));
		return -1;
	}

	if (layer == 1)
	{
		pNavHandle_->setLayerOne(&layerOne_);
		int ret = pNavHandle_->findStraightPath(pFromNode, layer, start, end, paths);
		pNavHandle_->setLayerOne(NULL);
		return ret;
	}
	else
	{
		return pNavHandle_->findStraightPath(pFromNode, layer, start, end, paths);
	}
}

int SpaceMemory::findRandomPointAroundCircle(CoordinateNode* pFromNode, int layer, const Position3D& centerPos,
	std::vector<Position3D>& points, uint32 max_points, float maxRadius)
{
	if (!pNavHandle_)
	{
		WARNING_MSG(fmt::format("Space::_findRandomPointAroundCircle(): space({}), not found navhandle!\n",
			id_));
		return 0;
	}

	if (layer == 1)
	{
		pNavHandle_->setLayerOne(&layerOne_);
		int ret = pNavHandle_->findRandomPointAroundCircle(pFromNode, layer, centerPos, points, max_points, maxRadius);
		pNavHandle_->setLayerOne(NULL);
		return ret;
	}
	else
	{
		return pNavHandle_->findRandomPointAroundCircle(pFromNode, layer, centerPos, points, max_points, maxRadius);
	}
}


//-------------------------------------------------------------------------------------
Entity* SpaceMemory::findEntity(ENTITY_ID entityID)
{
	SPACE_ENTITIES::const_iterator iter = this->entities().begin();
	for(; iter != this->entities().end(); ++iter)
	{
		const Entity* entity = (*iter).get();
			
		if(entity->id() == entityID)
			return const_cast<Entity*>(entity);
	}

	return NULL;
}

//-------------------------------------------------------------------------------------
bool SpaceMemory::destroy(ENTITY_ID entityID, bool ignoreGhost)
{
	if(state_ != STATE_NORMAL)
		return false;

	state_ = STATE_DESTROYING;
	destroyTime_ = timestamp();
	
	std::vector<ENTITY_ID> entitieslog;
	
	{
		SPACE_ENTITIES::const_iterator iter = this->entities().begin();
		for(; iter != this->entities().end(); ++iter)
		{
			const Entity* entity = (*iter).get();
			entitieslog.push_back(entity->id());
		}
	}

	{
		std::vector<ENTITY_ID>::iterator iter = entitieslog.begin();
		for(; iter != entitieslog.end(); ++iter)
		{
			Entity* entity = Cellapp::getSingleton().findEntity((*iter));
			if(entity != NULL && !entity->isDestroyed() && entity->spaceID() == this->id() && entity->isReal())
			{
				entity->onSpaceGone();
			}
		}
	}
	
	state_ = STATE_DESTROYED;
	
	if(this->entities().size() == 0)
		return true;

	std::vector<ENTITY_ID>::iterator iter = entitieslog.begin();
	for(; iter != entitieslog.end(); ++iter)
	{
		Entity* entity = Cellapp::getSingleton().findEntity((*iter));
		if(entity != NULL && entity->isReal() && !entity->isDestroyed() && entity->spaceID() == this->id())
		{
			entity->destroyEntity();
		}
		else
		{
			entity = findEntity((*iter));
			if(entity != NULL && entity->isReal() && !entity->isDestroyed() && entity->spaceID() == this->id())
			{
				removeEntity(entity);
			}
		}
	}

	if(!ignoreGhost)
		_clearGhosts();

	return true;
}

//-------------------------------------------------------------------------------------
void SpaceMemory::setGeometryPath(const std::string& path)
{ 
	return setSpaceData("_mapping", path); 
}

//-------------------------------------------------------------------------------------
const std::string& SpaceMemory::getGeometryPath()
{ 
	return getSpaceData("_mapping"); 
}

//-------------------------------------------------------------------------------------
void SpaceMemory::setSpaceData(const std::string& key, const std::string& value)
{
	SPACE_DATA::iterator iter = datas_.find(key);
	if(iter == datas_.end())
		datas_.insert(SPACE_DATA::value_type(key, value)); 
	else
		if(iter->second == value)
			return;
		else
			datas_[key] = value;

	onSpaceDataChanged(key, value, false);
}

//-------------------------------------------------------------------------------------
bool SpaceMemory::hasSpaceData(const std::string& key)
{
	SPACE_DATA::iterator iter = datas_.find(key);
	if(iter == datas_.end())
		return false;

	return true;
}

//-------------------------------------------------------------------------------------
const std::string& SpaceMemory::getSpaceData(const std::string& key)
{
	SPACE_DATA::iterator iter = datas_.find(key);
	if(iter == datas_.end())
	{
		static const std::string null = "";
		return null;
	}

	return iter->second;
}

//-------------------------------------------------------------------------------------
void SpaceMemory::delSpaceData(const std::string& key)
{
	SPACE_DATA::iterator iter = datas_.find(key);
	if(iter == datas_.end())
		return;

	datas_.erase(iter);

	onSpaceDataChanged(key, "", true);
}

//-------------------------------------------------------------------------------------
void SpaceMemory::onSpaceDataChanged(const std::string& key, const std::string& value, bool isdel)
{
	// 通知脚本
	if(!isdel)
	{
		SCOPED_PROFILE(SCRIPTCALL_PROFILE);
		SCRIPT_OBJECT_CALL_ARGS3(Cellapp::getSingleton().getEntryScript().get(), const_cast<char*>("onSpaceData"), 
			const_cast<char*>("Iss"), this->id(), key.c_str(), value.c_str(), false);
	}
	else
	{
		SCOPED_PROFILE(SCRIPTCALL_PROFILE);
		SCRIPT_OBJECT_CALL_ARGS3(Cellapp::getSingleton().getEntryScript().get(), const_cast<char*>("onSpaceData"), 
			const_cast<char*>("IsO"), this->id(), key.c_str(), Py_None, false);
	}

	SPACE_ENTITIES::const_iterator iter = this->entities().begin();
	for(; iter != this->entities().end(); ++iter)
	{
		const Entity* pEntity = (*iter).get();

		if(pEntity == NULL || pEntity->isDestroyed() || !pEntity->hasWitness())
			continue;

		Network::Bundle* pSendBundle = Network::Bundle::createPoolObject(OBJECTPOOL_POINT);
		NETWORK_ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pEntity->id(), (*pSendBundle));
		
		if(!isdel)
		{
			ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pSendBundle, ClientInterface::setSpaceData, set);
			(*pSendBundle) << this->id();
			(*pSendBundle) << key;
			(*pSendBundle) << value;
			ENTITY_MESSAGE_FORWARD_CLIENT_END(pSendBundle, ClientInterface::setSpaceData, set);
		}
		else
		{
			ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pSendBundle, ClientInterface::delSpaceData, del);
			(*pSendBundle) << this->id();
			(*pSendBundle) << key;
			ENTITY_MESSAGE_FORWARD_CLIENT_END(pSendBundle, ClientInterface::delSpaceData, del);
		}

		if(!isdel)
			pEntity->pWitness()->sendToClient(ClientInterface::setSpaceData, pSendBundle);
		else
			pEntity->pWitness()->sendToClient(ClientInterface::delSpaceData, pSendBundle);
	}
}

//-------------------------------------------------------------------------------------
void SpaceMemory::_addSpaceDatasToEntityClient(const Entity* pEntity)
{
	if(!pEntity)
	{
		return;
	}

	if(pEntity->isDestroyed())
	{
		return;
	}

	if(!pEntity->hasWitness())
	{
		WARNING_MSG(fmt::format("SpaceMemory::_addSpaceDatasToEntityClient: entity {} no client!\n", 
			pEntity->id()));

		return;
	}

	Network::Bundle* pSendBundle = Network::Bundle::createPoolObject(OBJECTPOOL_POINT);
	NETWORK_ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pEntity->id(), (*pSendBundle));

	ENTITY_MESSAGE_FORWARD_CLIENT_BEGIN(pSendBundle, ClientInterface::initSpaceData, init);
	(*pSendBundle) << this->id();

	SPACE_DATA::iterator iter = datas_.begin();
	for(; iter != datas_.end(); ++iter)
	{
		(*pSendBundle) << iter->first;
		(*pSendBundle) << iter->second;
	}

	ENTITY_MESSAGE_FORWARD_CLIENT_END(pSendBundle, ClientInterface::initSpaceData, init);

	pEntity->pWitness()->sendToClient(ClientInterface::initSpaceData, pSendBundle);
}

//-------------------------------------------------------------------------------------
PyObject* SpaceMemory::__py_SetSpaceData(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;

	if(PyTuple_Size(args) != 3)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::setSpaceData: (argssize != (spaceID, key, value)) error!");
		PyErr_PrintEx(0);
		return 0;
	}
	
	char* key = NULL, *value = NULL;
	if(PyArg_ParseTuple(args, "Iss", &spaceID, &key, &value) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::setSpaceData: args error!");
		PyErr_PrintEx(0);
		return 0;
	}
	
	if(key == NULL || value == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::setSpaceData: key or value error, not is string!");
		PyErr_PrintEx(0);
		return 0;
	}

	if(strlen(key) == 0)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::setSpaceData: key is empty!");
		PyErr_PrintEx(0);
		return 0;
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if(space == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::setSpaceData: (spaceID=%u) not found!", 
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}
	
	if(kbe_stricmp(key, "_mapping") == 0)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::setSpaceData: key{_mapping} is protected!", 
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	space->setSpaceData(key, value);
	S_Return;
}

//-------------------------------------------------------------------------------------
PyObject* SpaceMemory::__py_GetSpaceData(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;

	if(PyTuple_Size(args) != 2)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::getSpaceData: (argssize != (spaceID, key)) error!");
		PyErr_PrintEx(0);
		return 0;
	}
	
	char* key = NULL;
	if(PyArg_ParseTuple(args, "Is", &spaceID, &key) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::getSpaceData: args error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if(key == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::getSpaceData: key not is string!");
		PyErr_PrintEx(0);
		return 0;
	}

	if(strlen(key) == 0)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::getSpaceData: key is empty!");
		PyErr_PrintEx(0);
		return 0;
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if(space == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::getSpaceData: (spaceID=%u) not found!", 
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}
	
	if(!space->hasSpaceData(key))
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::getSpaceData: (spaceID=%u, key=%s) not found!", 
			spaceID, key);

		PyErr_PrintEx(0);
		return 0;
	}

	return PyUnicode_FromString(space->getSpaceData(key).c_str());
}

//-------------------------------------------------------------------------------------
PyObject* SpaceMemory::__py_DelSpaceData(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;

	if(PyTuple_Size(args) != 2)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::delSpaceData: (argssize != (spaceID, key)) error!");
		PyErr_PrintEx(0);
		return 0;
	}
	
	char* key = NULL;
	if(PyArg_ParseTuple(args, "Is", &spaceID, &key) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::delSpaceData: args error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if(key == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::delSpaceData: key not is string!");
		PyErr_PrintEx(0);
		return 0;
	}

	if(strlen(key) == 0)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::delSpaceData: key is empty!");
		PyErr_PrintEx(0);
		return 0;
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if(space == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::delSpaceData: (spaceID=%u) not found!", 
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	if(!space->hasSpaceData(key))
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::delSpaceData: (spaceID=%u, key=%s) not found!", 
			spaceID, key);

		PyErr_PrintEx(0);
		return 0;
	}
	
	if(kbe_stricmp(key, "_mapping") == 0)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::delSpaceData: key{_mapping} is protected!", 
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	space->delSpaceData(key);
	S_Return;
}

PyObject* SpaceMemory::__py_getMap(PyObject* self, PyObject* args)
{
	int argCount = PyTuple_Size(args);
	if (argCount < 3 || argCount > 4)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::__py_getMap: (argssize != 2) is error!");
		PyErr_PrintEx(0);
		return 0;
	}
	SPACE_ID spaceID = 0;
	int x, y;
	int layer = 1;
	if (argCount == 3)
	{
		if (PyArg_ParseTuple(args, "III", &spaceID, &x, &y) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "KBEngine::__py_getMap: args is error!");
			PyErr_PrintEx(0);
			return 0;
		}
	}
	else
	{
		if (PyArg_ParseTuple(args, "IIII", &spaceID, &x, &y, &layer) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "KBEngine::__py_getMap: args is error!");
			PyErr_PrintEx(0);
			return 0;
		}
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if (space == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::__py_getMap: (spaceID=%u) not found!",
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	int ret = space->_getMapNavCost(x, y, layer);
	return PyLong_FromLong(long(ret));
}

int SpaceMemory::_getMapNavCost(int x, int y, int layer)
{
	if (layer == 1)
	{
		pNavHandle_->setLayerOne(&layerOne_);
		int ret = pNavHandle_->getMapNavCost(x, y);
		pNavHandle_->setLayerOne(NULL);
		return ret;
	}
	else
	{
		return pNavHandle_->getMapNavCost(x, y);
	}
}


bool SpaceMemory::addMultipleTilesGeometryMapping(std::vector<std::string> resPaths, std::vector<int>& rotations, std::vector< std::pair<int, int> >& positions, bool shouldLoadOnServer)
{
	std::string joinedPath = "";
	for (std::vector<std::string>::iterator it = resPaths.begin();it != resPaths.end(); it++)
		joinedPath += (*it + ";");
	for (std::vector<int>::iterator it = rotations.begin();it != rotations.end(); it++)
		joinedPath += (" " + std::to_string(*it));
	for (std::vector<std::pair<int, int>>::iterator it = positions.begin();it != positions.end(); it++)
		joinedPath += (" " + std::to_string(it->first) + " " + std::to_string(it->second));

	INFO_MSG(fmt::format("KBEngine::addMultipleTilesGeometryMapping: spaceID={}, respath={}, shouldLoadOnServer={}!\n",
		id(), joinedPath, shouldLoadOnServer));
	hasGeometry_ = true;
	if (getGeometryPath() == joinedPath)
	{
		WARNING_MSG(fmt::format("KBEngine::addSpaceGeometryMapping: spaceID={}, respath={} exist!\n",
			id(), joinedPath));

		return true;
	}

	setGeometryPath(joinedPath);

	if (shouldLoadOnServer)
		loadMultipleTilesMap(resPaths, rotations, positions);

	return true;
}

void SpaceMemory::loadMultipleTilesMap(std::vector<std::string> resPaths, std::vector<int>& rotations, std::vector< std::pair<int, int> >& positions)
{
	KBE_ASSERT(pNavHandle_ == NULL);
	Cellapp::getSingleton().threadPool().addTask(new LoadMultiTilesTask(this->id(), resPaths, rotations, positions));
}

PyObject* SpaceMemory::__py_AddLayerOneTilesGeometryMapping(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;
	int x, y, width, height;
	PyObject* py_maps = NULL;

	int argCount = PyTuple_Size(args);
	if (argCount != 6)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesGeometryMapping: (argssize[spaceID, x, y, width, height, maps] != 6) is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if (PyArg_ParseTuple(args, "I|i|i|i|i|O", &spaceID, &x, &y, &width, &height, &py_maps) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesGeometryMapping: args is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if (space == NULL || !space->pNavHandle())
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesGeometryMapping: (spaceID=%u) not found!",
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	SCRIPT_ERROR_CHECK();

	int len = PyTuple_Size(py_maps);
	if (len != width * height)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesGeometryMapping: {} not equal {} * {}",
			len, width, height);

		PyErr_PrintEx(0);
		return 0;
	}

	std::vector<int> maps;
	for (int i = 0; i < len; i++)
	{
		maps.push_back((int)PyLong_AsLong(PyTuple_GetItem(py_maps, i)));
	}

	space->_addLayerOneTilesGeometryMapping(x, y, width, height, maps);
	S_Return;
}

void SpaceMemory::_addLayerOneTilesGeometryMapping(int x, int y, int width, int height, std::vector<int> &maps)
{
	for (int i = 0; i < width; i++)
	{
		for (int j = 0; j < height; j++)
		{
			int index = MatrixMap::MatrixMap::getIndex(i, j, width, height);
			if (maps[index] == MatrixMap::NAV_COST_PASS)
			{
				maps[index] = pNavHandle_->getMapNavCost(x + i, y + i);
			}
		}
	}

	layerOne_.addMatrixMap(x, y, width, height, maps);
}

PyObject* SpaceMemory::__py_AddLayerOneTilesFromBytes(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;
	int x, y, width, height;
	PyObject* py_byteMaps = NULL;

	int argCount = PyTuple_Size(args);
	if (argCount != 6)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesFromBytes: (argssize[spaceID, x, y, width, height, maps] != 6) is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if (PyArg_ParseTuple(args, "I|i|i|i|i|O", &spaceID, &x, &y, &width, &height, &py_byteMaps) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesFromBytes: args is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if (space == NULL || !space->pNavHandle())
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesFromBytes: (spaceID=%u) not found!",
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	SCRIPT_ERROR_CHECK();

	int totalBytes = (int)PyBytes_GET_SIZE(py_byteMaps);
	std::unique_ptr<uint8_t[]> tmpBuf(new uint8_t[totalBytes + 1]);
	memcpy(tmpBuf.get(), PyBytes_AS_STRING(py_byteMaps), totalBytes);

	if (totalBytes * 8 != width * height)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesGeometryMapping: {} not equal {} * {}",
			totalBytes, width, height);

		PyErr_PrintEx(0);
		return 0;
	}
	space->_addLayerOneTilesWithBytes(x, y, width, height, tmpBuf.get());
	S_Return;
}

void SpaceMemory::_mapOverlyLayer0(MatrixMap::MatrixMapPtr ptr)
{
	if (!ptr)
	{
		WARNING_MSG(fmt::format("SpaceMemory::_mapOverlyLayer0: ptr==nullptr.\n"));
		return;
	}

	int xMax = ptr->originPos.first + ptr->width;
	int yMax = ptr->originPos.second + ptr->height;
	for (int i = ptr->originPos.first; i < xMax; i++)
	{
		for (int j = ptr->originPos.second; j < yMax; j++)
		{
			int cost = ptr->getMapNavigationCost(i, j);
			if (cost == MatrixMap::NAV_COST_PASS)
			{
				int layer0cost = pNavHandle_->getMapNavCost(i, j);
				if (layer0cost == MatrixMap::NAV_COST_BLOCK || layer0cost == MatrixMap::NAV_COST_UNKNOW)
				{
					ptr->resetMapData(i, j, 1);
				}
				else
				{
					ptr->setMapData(i, j, 1);
				}
			}
		}
	}
}

void SpaceMemory::_addLayerOneTilesWithBytes(int x, int y, int width, int height, uint8_t *map)
{
	auto ptr = std::make_shared<MatrixMap::MatrixMap>(x, y, width, height, map);
	_mapOverlyLayer0(ptr);
	layerOne_.addMatrixMapWithMap(ptr);
}


PyObject * SpaceMemory::__py_AddLayerOneTilesById(PyObject * self, PyObject * args)
{
	SPACE_ID spaceID = 0;
	int x, y;
	char *ID = NULL;

	int argCount = PyTuple_Size(args);
	if (argCount != 4)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesById: (argssize[spaceID, x, y, ID] != 4) is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if (PyArg_ParseTuple(args, "I|i|i|s", &spaceID, &x, &y, &ID) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesById: args is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if (space == NULL || !space->pNavHandle())
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesById: (spaceID=%u) not found!",
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	SCRIPT_ERROR_CHECK();

	std::string IDStr(ID);
	MatrixMap::MatrixMapPtr mapPtr = AirWall::AirWall::getSingleton().getMapById(IDStr);
	if (mapPtr)
	{
		space->_addLayerOneTilesWithMap(x, y, mapPtr);
	}
	S_Return;
}

void SpaceMemory::_addLayerOneTilesWithMap(int x, int y, MatrixMap::MatrixMapPtr mapPtr)
{
	auto ptr = std::make_shared<MatrixMap::MatrixMap>(*mapPtr);
	ptr->setOriginPos(x, y);
	_mapOverlyLayer0(ptr);
	layerOne_.addMatrixMapWithMap(ptr);
}


PyObject * SpaceMemory::__py_LoadAirWalls(PyObject * self, PyObject * args)
{
	if (AirWall::AirWall::getSingleton().isLoad())
	{
		S_Return;
	}

	char *pathPtr;
	int argCount = PyTuple_Size(args);
	if (argCount != 1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::loadAirWalls: (argssize[path] != 1) is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if (PyArg_ParseTuple(args, "s", &pathPtr) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::loadAirWalls: args is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	SCRIPT_ERROR_CHECK();

	std::string path(pathPtr);
	Cellapp::getSingleton().threadPool().addTask(new LoadAirWallsTask(path));
	S_Return;
}

PyObject* SpaceMemory::__py_RemoveLayerOneTilesGeometryMapping(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;
	int x, y;
	int argCount = PyTuple_Size(args);
	if (argCount != 3)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::removeLayerOneTilesGeometryMapping: (argssize[spaceID, x, y] != 3) is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if (PyArg_ParseTuple(args, "I|i|i", &spaceID, &x, &y) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::removeLayerOneTilesGeometryMapping: args is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if (space == NULL || !space->pNavHandle())
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addLayerOneTilesGeometryMapping: (spaceID=%u) not found!",
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	SCRIPT_ERROR_CHECK();

	space->_removeLayerOneTilesGeometryMapping(x, y);
	S_Return;
}

void SpaceMemory::_removeLayerOneTilesGeometryMapping(int x, int y)
{
	layerOne_.removeMatrixMap(x, y);
}


PyObject* SpaceMemory::__py_AddSpaceMultipleTilesGeometryMapping(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;
	bool shouldLoadOnServer = true;
	PyObject* py_paths = NULL;
	PyObject* py_rotations = NULL;
	PyObject* py_positions = NULL;

	int argCount = PyTuple_Size(args);
	if (argCount < 4 || argCount > 5)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceMultipleTilesGeometryMapping: (argssize[spaceID, mapper, path, shouldLoadOnServer] < 4 || > 5) is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if (argCount == 4)
	{
		if (PyArg_ParseTuple(args, "I|O|O|O", &spaceID, &py_paths, &py_rotations, &py_positions) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceMultipleTilesGeometryMapping: args is error!");
			PyErr_PrintEx(0);
			return 0;
		}
	}
	else if (argCount == 5)
	{
		if (PyArg_ParseTuple(args, "I|O|O|O|b", &spaceID, &py_paths, &py_rotations, &py_positions, &shouldLoadOnServer) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceMultipleTilesGeometryMapping2: args is error!");
			PyErr_PrintEx(0);
			return 0;
		}
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if (space == NULL)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceMultipleTilesGeometryMapping: (spaceID=%u) not found!",
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	SCRIPT_ERROR_CHECK();

	int len = PyTuple_Size(py_paths);
	if (PyTuple_Size(py_rotations) != len || PyTuple_Size(py_positions) != len)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceMultipleTilesGeometryMapping: args length is error!");
		PyErr_PrintEx(0);
		return 0;
	}
	std::vector<std::string> subMapPaths;
	std::vector< std::pair<int, int> > subMapPositons;
	std::vector<int> subMapRotations;
	for (int i = 0; i < len; i++)
	{
		PyObject* asciiBytes = PyUnicode_AsASCIIString(PyTuple_GetItem(py_paths, i));
		subMapPaths.push_back(PyBytes_AsString((asciiBytes)));
		S_DECREF(asciiBytes);
		subMapRotations.push_back((int)PyLong_AsLong(PyTuple_GetItem(py_rotations, i)));

		PyObject* pos = PyTuple_GetItem(py_positions, i);
		int x = (int)PyLong_AsLong(PyTuple_GetItem(pos, 0));
		int y = (int)PyLong_AsLong(PyTuple_GetItem(pos, 1));
		subMapPositons.push_back(std::make_pair(x, y));
	}

	if (!space->addMultipleTilesGeometryMapping(subMapPaths, subMapRotations, subMapPositons, shouldLoadOnServer))
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::addSpaceMultipleTilesGeometryMapping2: (spaceID=%u) error!", spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	S_Return;
}

PyObject* SpaceMemory::__py_SetMapTileNavCost(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;
	int layerIndex, x, y, cost;

	int argCount = PyTuple_Size(args);
	if (argCount < 4 || argCount > 5)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::setMapTileNavCost: (argssize[spaceID, mapper, path, shouldLoadOnServer] < 4 || > 5) is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if (argCount == 4)
	{
		if (PyArg_ParseTuple(args, "I|I|I|I", &spaceID, &x, &y, &cost) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "KBEngine::setMapTileNavCost: args is error!");
			PyErr_PrintEx(0);
			return 0;
		}
		layerIndex = 0;
	}
	else if (argCount == 5)
	{
		if (PyArg_ParseTuple(args, "I|I|I|I|I", &spaceID, &x, &y, &cost, &layerIndex) == -1)
		{
			PyErr_Format(PyExc_AssertionError, "KBEngine::setMapTileNavCost: args is error!");
			PyErr_PrintEx(0);
			return 0;
		}
	}
	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if (space == NULL || !space->pNavHandle())
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::setMapTileNavCost: (spaceID=%u) not found!",
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	SCRIPT_ERROR_CHECK();

	bool ret = space->pNavHandle()->setMapTileNavCost(layerIndex, x, y, cost);
	return PyLong_FromLong(long(ret));
}

PyObject* SpaceMemory::__py_SetTileMapOrigin(PyObject* self, PyObject* args)
{
	SPACE_ID spaceID = 0;
	int x, y;

	int argCount = PyTuple_Size(args);
	if (argCount != 3)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::SetTileMapOrigin: (argssize[spaceId,x,y] != 3) is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	if (PyArg_ParseTuple(args, "I|I|I", &spaceID, &x, &y) == -1)
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::SetTileMapOrigin: args is error!");
		PyErr_PrintEx(0);
		return 0;
	}

	SpaceMemory* space = SpaceMemorys::findSpace(spaceID);
	if (space == NULL || !space->pNavHandle())
	{
		PyErr_Format(PyExc_AssertionError, "KBEngine::SetTileMapOrigin: (spaceID=%u) not found!",
			spaceID);

		PyErr_PrintEx(0);
		return 0;
	}

	SCRIPT_ERROR_CHECK();

	//add by huyf 2019.08.21
	DEBUG_MSG(fmt::format("SpaceMemory::__py_SetTileMapOrigin: spaceID=[{}] x=[{}] y=[{}]\n", spaceID, x, y));
	//add end

	bool ret = space->pNavHandle()->setMapTileOrigin(x, y);
	return PyLong_FromLong(long(ret));
}

//-------------------------------------------------------------------------------------
}
