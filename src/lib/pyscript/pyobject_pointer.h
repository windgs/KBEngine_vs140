// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com

#ifndef KBE_PYOBJECT_POINTER_H
#define KBE_PYOBJECT_POINTER_H

#include "common/smartpointer.h"
//add by huyf 2019.07.10
#include "common/common.h"
//add end

namespace KBEngine { 

typedef SmartPointer<PyObject> PyObjectPtr;

template<>
inline void incrementReferenceCount(const PyObject& obj)
{
	Py_INCREF( const_cast<PyObject*>( &obj ) );
// 	if (strcmp(obj.ob_type->tp_name, "CNpc") == 0)
// 	{
// 		ERROR_MSG(fmt::format("huyf-yh: incrementReferenceCount obj={:p} tp_name = CNpc ob_refcnt = {}.\n", (void*)&obj, obj.ob_refcnt));
// 	}
	
};

template<>
inline void decrementReferenceCount(const PyObject& obj)
{
	Py_DECREF( const_cast<PyObject*>( &obj ) );
// 	if (strcmp(obj.ob_type->tp_name, "CNpc") == 0)
// 	{
// 		ERROR_MSG(fmt::format("huyf-yh: decrementReferenceCount obj={:p} tp_name = CNpc ob_refcnt = {}.\n", (void*)&obj, obj.ob_refcnt));
// 	}
};

}
#endif // KBE_PYOBJECT_POINTER_H
