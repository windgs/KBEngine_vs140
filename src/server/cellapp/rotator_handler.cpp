// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com

#include "cellapp.h"
#include "entity.h"
#include "rotator_handler.h"
#include "turn_controller.h"

namespace KBEngine{	


//-------------------------------------------------------------------------------------
RotatorHandler::RotatorHandler(KBEShared_ptr<Controller> pController, const Direction3D& destDir, float velocity, PyObject* userarg):
destDir_(destDir),
velocity_(fabs(velocity)),
pyuserarg_(userarg),
pController_(pController)
{
	updatableName = "RotatorHandler";

	Py_INCREF(userarg);

	static_cast<TurnController*>(pController.get())->pRotatorHandler(this);
	Cellapp::getSingleton().addUpdatable(this);
}

//-------------------------------------------------------------------------------------
RotatorHandler::RotatorHandler() :
destDir_(0.f,0.f,0.f),
velocity_(0.f),
pyuserarg_(NULL),
pController_(KBEShared_ptr<Controller>())
{
	updatableName = "RotatorHandler";
}

//-------------------------------------------------------------------------------------
RotatorHandler::~RotatorHandler()
{
	if (pyuserarg_ != NULL)
	{
		Py_DECREF(pyuserarg_);
	}
}

//-------------------------------------------------------------------------------------
void RotatorHandler::addToStream(KBEngine::MemoryStream& s)
{
	s << destDir_.dir.x << destDir_.dir.y << destDir_.dir.z << velocity_;
	s.appendBlob(script::Pickler::pickle(pyuserarg_));
}

//-------------------------------------------------------------------------------------
void RotatorHandler::createFromStream(KBEngine::MemoryStream& s)
{
	s >> destDir_.dir.x >> destDir_.dir.y >> destDir_.dir.z >> velocity_;
	
	std::string val = "";
	s.readBlob(val);
	pyuserarg_ = script::Pickler::unpickle(val);
}

//-------------------------------------------------------------------------------------
bool RotatorHandler::requestTurnOver()
{
	if (pController_)
	{
		if (pController_->pEntity())
			pController_->pEntity()->onTurn(pController_->id(), pyuserarg_);

		// �����onTurn�е���cancelController��id���ᵼ��Controller��������pController_ΪNULL
		if (pController_)
			pController_->destroy();
	}

	return true;
}

//-------------------------------------------------------------------------------------
const Direction3D& RotatorHandler::destDir()
{
	return destDir_;
}

//-------------------------------------------------------------------------------------
bool RotatorHandler::update()
{
	if(pController_ == NULL)
	{
		delete this;
		return false;
	}
		
	Entity* pEntity = pController_->pEntity();
	Py_INCREF(pEntity);
	if ((((PyObject *)pEntity)->ob_type->tp_name, "CNpc") == 0)
	{
		DEBUG_MSG(fmt::format("huyf-yh: MoveToPointHandler::update-Py_INCREF p={:p} scriptName={} id={} ob_refcnt = {}\n", (void*)pEntity, pEntity->scriptName(), pEntity->id(), ((PyObject *)pEntity)->ob_refcnt));
	}

	const Direction3D& dstDir = destDir();
	Direction3D currDir = pEntity->direction();

	// �õ���ֵ
	float deltaYaw = dstDir.yaw() - currDir.yaw();

	if (deltaYaw > KBE_PI)
		deltaYaw = (float)((double)deltaYaw - KBE_2PI/* �������ǵĻ��ȱ�ʾ��Χ��-PI ~ PI���˴���ֹ��� */);
	else if (deltaYaw < -KBE_PI)
		deltaYaw = (float)((double)deltaYaw + KBE_2PI);

	if (fabs(deltaYaw) < velocity_)
	{
		deltaYaw = 0.f;
		currDir.yaw(dstDir.yaw());
	}
	else if (fabs(deltaYaw) > velocity_)
	{
		deltaYaw = KBEClamp(deltaYaw, -velocity_, velocity_);
		currDir.yaw(currDir.yaw() + deltaYaw);
	}

	if (currDir.yaw() > KBE_PI)
		currDir.yaw((float((double)currDir.yaw() - KBE_2PI)));
	else if (currDir.yaw() < -KBE_PI)
		currDir.yaw((float((double)currDir.yaw() + KBE_2PI)));

	// ����entity����λ�ú�����
	if (pController_)
		pEntity->setPositionAndDirection(pEntity->position(), currDir);

	// ����ﵽĿ�ĵ��򷵻�true
	if (fabs(deltaYaw) < 0.0001f && requestTurnOver())
	{
		Py_DECREF(pEntity);
		if ((((PyObject *)pEntity)->ob_type->tp_name, "CNpc") == 0)
		{
			DEBUG_MSG(fmt::format("huyf-yh: MoveToPointHandler::update-Py_DECREF p={:p} scriptName={} id={} ob_refcnt = {}\n", (void*)pEntity, pEntity->scriptName(), pEntity->id(), ((PyObject *)pEntity)->ob_refcnt));
		}
		delete this;
		return false;
	}

	Py_DECREF(pEntity);
	if ((((PyObject *)pEntity)->ob_type->tp_name, "CNpc") == 0)
	{
		DEBUG_MSG(fmt::format("huyf-yh: MoveToPointHandler::update-Py_DECREF p={:p} scriptName={} id={} ob_refcnt = {}\n", (void*)pEntity, pEntity->scriptName(), pEntity->id(), ((PyObject *)pEntity)->ob_refcnt));
	}
	return true;
}

//-------------------------------------------------------------------------------------
}
