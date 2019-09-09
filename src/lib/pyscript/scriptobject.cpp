// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com


#include "scriptobject.h"

#ifndef CODE_INLINE
#include "scriptobject.inl"
#endif

namespace KBEngine{ namespace script{

ScriptObject::SCRIPTOBJECT_TYPES ScriptObject::scriptObjectTypes;

SCRIPT_METHOD_DECLARE_BEGIN(ScriptObject)
SCRIPT_METHOD_DECLARE_END()

SCRIPT_MEMBER_DECLARE_BEGIN(ScriptObject)
SCRIPT_MEMBER_DECLARE_END()

SCRIPT_GETSET_DECLARE_BEGIN(ScriptObject)
SCRIPT_GETSET_DECLARE_END()
SCRIPT_INIT(ScriptObject, 0, 0, 0, 0, 0)									

//-------------------------------------------------------------------------------------
ScriptObject::ScriptObject(PyTypeObject* pyType, bool isInitialised)
{
	if (PyType_Ready(pyType) < 0)
	{
		ERROR_MSG(fmt::format("ScriptObject: Type {} is not ready\n", pyType->tp_name));
	}

	if (!isInitialised)
	{
		PyObject_INIT(static_cast<PyObject*>(this), pyType);
	}
}

//-------------------------------------------------------------------------------------
ScriptObject::~ScriptObject()
{
	assert(this->ob_refcnt == 0);
}

//-------------------------------------------------------------------------------------
PyTypeObject* ScriptObject::getScriptObjectType(const std::string& name)
{
	ScriptObject::SCRIPTOBJECT_TYPES::iterator iter = scriptObjectTypes.find(name);
	if(iter != scriptObjectTypes.end())
		return iter->second;

	return NULL;
}

//-------------------------------------------------------------------------------------
PyObject* ScriptObject::py__module__()
{ 
	return PyUnicode_FromString(scriptName()); 
}

//-------------------------------------------------------------------------------------
PyObject* ScriptObject::py__qualname__()
{ 
	return PyUnicode_FromString(scriptName()); 
}

//-------------------------------------------------------------------------------------
PyObject* ScriptObject::py__name__()
{ 
	return PyUnicode_FromString(scriptName()); 
}

//-------------------------------------------------------------------------------------
PyObject* ScriptObject::tp_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
	return type->tp_alloc(type, 0); 
};

//-------------------------------------------------------------------------------------
PyObject* ScriptObject::onScriptGetAttribute(PyObject* attr)
{	
	PyObject* p= PyObject_GenericGetAttr(this, attr);
	//add by huyf 2019.09.05
	//char* ccattr = PyUnicode_AsUTF8AndSize(attr, NULL);
	//DEBUG_MSG(fmt::format("ScriptObject::onScriptGetAttribute scriptName={} name={} tp_name={}\n", scriptName(), ccattr, p->ob_type->tp_name));
	//add end
	return p;
}

//-------------------------------------------------------------------------------------
int ScriptObject::onScriptSetAttribute(PyObject* attr, PyObject* value)
{
	//add by huyf 2019.09.05
	//char* ccattr = PyUnicode_AsUTF8AndSize(attr, NULL);
	//DEBUG_MSG(fmt::format("ScriptObject::onScriptSetAttribute scriptName={} name={} tp_name={}\n", scriptName(), ccattr, value->ob_type->tp_name));
	//add end
	return PyObject_GenericSetAttr(static_cast<PyObject*>(this), attr, value);
}

//-------------------------------------------------------------------------------------
int ScriptObject::onScriptDelAttribute(PyObject* attr)
{
	//add by huyf 2019.09.05
	char* ccattr = PyUnicode_AsUTF8AndSize(attr, NULL);
	DEBUG_MSG(fmt::format("ScriptObject::onScriptDelAttribute scriptName={} name={} tp_name={}\n", scriptName(), ccattr, attr->ob_type->tp_name));
	//add end
	return this->onScriptSetAttribute(attr, NULL);
}

//-------------------------------------------------------------------------------------
int ScriptObject::onScriptInit(PyObject* self, PyObject* args, PyObject* kwds)
{
	return 0;
}

//-------------------------------------------------------------------------------------
PyObject* ScriptObject::tp_repr()
{
	if(g_debugEntity)
		return PyUnicode_FromFormat("%s object at %p, refc=%u.", 
			this->scriptName(), this, (uint32)static_cast<PyObject*>(this)->ob_refcnt);
	
	return PyUnicode_FromFormat("%s object at %p.", this->scriptName(), this);
}

//-------------------------------------------------------------------------------------
PyObject* ScriptObject::tp_str()
{
	if(g_debugEntity)
		return PyUnicode_FromFormat("%s object at %p, refc=%u.", 
				this->scriptName(), this, (uint32)static_cast<PyObject*>(this)->ob_refcnt);
	
	return PyUnicode_FromFormat("%s object at %p.", this->scriptName(), this);
}

//-------------------------------------------------------------------------------------

}
}
