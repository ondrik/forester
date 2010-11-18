/*
 * Copyright (C) 2009-2010 Kamil Dudka <kdudka@redhat.com>
 *
 * This file is part of predator.
 *
 * predator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * predator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with predator.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "symutil.hh"

#include <cl/cl_msg.hh>
#include <cl/storage.hh>

#include "symbt.hh"
#include "symheap.hh"
#include "symproc.hh"

#include <stack>

#include <boost/foreach.hpp>

TObjId subObjByChain(const SymHeap &sh, TObjId obj, TFieldIdxChain ic) {
    BOOST_FOREACH(const int nth, ic) {
        obj = sh.subObj(obj, nth);
        if (OBJ_INVALID == obj)
            break;
    }

    return obj;
}

TObjId subObjByInvChain(const SymHeap &sh, TObjId obj, TFieldIdxChain ic) {
    std::stack<int> chkStack;

    // just slowly go to the root
    for (unsigned i = 0; i < ic.size(); ++i) {
        int nth;
        const TObjId parent = sh.objParent(obj, &nth);
        if (OBJ_INVALID == parent)
            // count mismatch
            return OBJ_INVALID;

        chkStack.push(nth);
        obj = parent;
    }

    // now check if the captured selector sequence matches the given one
    for (unsigned i = 0; i < ic.size(); ++i) {
        CL_BREAK_IF(chkStack.empty());
        if (chkStack.top() != ic[i])
            // field mismatch
            return OBJ_INVALID;

        chkStack.pop();
    }
    CL_BREAK_IF(!chkStack.empty());

    return obj;
}

bool isHeapObject(const SymHeap &heap, TObjId obj) {
    if (obj <= 0)
        return false;

    for (; OBJ_INVALID != obj; obj = heap.objParent(obj))
        if (heap.cVar(0, obj))
            return false;

    return true;
}

void digRootObject(const SymHeap &heap, TValueId *pValue) {
    TObjId obj = heap.pointsTo(*pValue);
    CL_BREAK_IF(obj < 0);

    TObjId parent;
    while (OBJ_INVALID != (parent = heap.objParent(obj)))
        obj = parent;

    TValueId val = heap.placedAt(obj);
    CL_BREAK_IF(val <= 0);

    *pValue = val;
}

void getPtrValues(SymHeapCore::TContValue &dst, const SymHeap &heap,
                  TObjId obj)
{
    std::stack<TObjId> todo;
    todo.push(obj);
    while (!todo.empty()) {
        const TObjId obj = todo.top();
        todo.pop();

        const struct cl_type *clt = heap.objType(obj);
        const enum cl_type_e code = (clt)
            ? clt->code
            : /* anonymous object of known size */ CL_TYPE_PTR;

        switch (code) {
            case CL_TYPE_PTR: {
                const TValueId val = heap.valueOf(obj);
                if (0 < val)
                    dst.push_back(val);

                break;
            }

            case CL_TYPE_STRUCT:
            case CL_TYPE_UNION:
                for (int i = 0; i < clt->item_cnt; ++i) {
                    const TObjId subObj = heap.subObj(obj, i);
                    CL_BREAK_IF(subObj < 0);

                    todo.push(subObj);
                }
                break;

            case CL_TYPE_ENUM:
            case CL_TYPE_ARRAY:
            case CL_TYPE_CHAR:
            case CL_TYPE_BOOL:
            case CL_TYPE_INT:
                break;

            default:
                // other types of value should be safe to ignore here
                // but worth to check by a debugger at least once anyway
#ifndef NDEBUG
                CL_TRAP;
#endif
                break;
        }
    }
}

void objReplace(SymHeap &sh, TObjId oldObj, TObjId newObj) {
    // check for possible replacement of sub-object
    CL_BREAK_IF(OBJ_INVALID != sh.objParent(oldObj));
    CL_BREAK_IF(OBJ_INVALID != sh.objParent(newObj));

    // resolve object addresses
    const TValueId oldAddr = sh.placedAt(oldObj);
    const TValueId newAddr = sh.placedAt(newObj);
    CL_BREAK_IF(oldAddr <= 0 || newAddr <= 0);

    // update all references
    sh.valReplace(oldAddr, newAddr);

    // now destroy the old object
    sh.objDestroy(oldObj);
}

void skipObj(const SymHeap &sh, TObjId *pObj, TFieldIdxChain icHead,
             TFieldIdxChain icNext)
{
    const TObjId objPtrNext = subObjByChain(sh, *pObj, icNext);
    const TObjId headNext = sh.pointsTo(sh.valueOf(objPtrNext));
    const TObjId objNext = subObjByInvChain(sh, headNext, icHead);

    // move to the next object
    *pObj = objNext;
}

typedef std::pair<TObjId, const cl_initializer *> TInitialItem;

// specialization of TraverseSubObjsHelper suitable for gl initializers
template <> struct TraverseSubObjsHelper<TInitialItem> {
    static const struct cl_type* getItemClt(const SymHeap           &sh,
                                            const TInitialItem      &item)
    {
        const struct cl_type *clt = sh.objType(item.first);
        CL_BREAK_IF(item.second && (!clt || *clt != *item.second->type));
        return clt;
    }

    static TInitialItem getNextItem(const SymHeap                   &sh,
                                    TInitialItem                    item,
                                    int                             nth)
    {
        item.first = sh.subObj(item.first, nth);

        const struct cl_initializer *&initial = item.second;
        if (initial)
            initial = initial->data.nested_initials[nth];

        return item;
    }
};

bool initSingleVariable(SymHeap &sh, const TInitialItem &item) {
    const TObjId obj = item.first;
    const struct cl_type *clt = sh.objType(obj);
    CL_BREAK_IF(!clt);

    const enum cl_type_e code = clt->code;
    switch (code) {
        case CL_TYPE_ARRAY:
            CL_DEBUG("CL_TYPE_ARRAY is not supported by VarInitializer");
            return /* continue */ true;

        case CL_TYPE_UNION:
        case CL_TYPE_STRUCT:
            CL_TRAP;

        default:
            break;
    }

    const struct cl_initializer *initial = item.second;
    if (!initial) {
        // no initializer given, nullify the variable
        sh.objSetValue(obj, /* also equal to VAL_FALSE */ VAL_NULL);
        return /* continue */ true;
    }

    // FIXME: we're asking for troubles this way
    const CodeStorage::Storage *null = 0;
    SymBackTrace dummyBt(*null);
    SymProc proc(sh, &dummyBt);

    // resolve initial value
    const struct cl_operand *op = initial->data.value;
    const TValueId val = proc.heapValFromOperand(*op);
    CL_DEBUG("using explicit initializer: obj #"
            << static_cast<int>(obj) << " <-- val #"
            << static_cast<int>(val));

    // set the initial value
    CL_BREAK_IF(VAL_INVALID == val);
    sh.objSetValue(obj, val);

    return /* continue */ true;
}

void initVariable(SymHeap                       &sh,
                  TObjId                        obj,
                  const CodeStorage::Var        &var)
{
    const TInitialItem item(obj, var.initial);

    if (isComposite(var.clt))
        traverseSubObjs(sh, item, initSingleVariable, /* leavesOnly */ true);
    else
        initSingleVariable(sh, item);
}

class PointingObjectsFinder {
    SymHeap::TContObj &dst_;

    public:
        PointingObjectsFinder(SymHeap::TContObj &dst): dst_(dst) { }

        bool operator()(const SymHeap &sh, TObjId obj) const {
            const TValueId addr = sh.placedAt(obj);
            CL_BREAK_IF(addr <= 0);

            sh.usedBy(dst_, addr);
            return /* continue */ true;
        }
};

void gatherPointingObjects(const SymHeap            &sh,
                           SymHeap::TContObj        &dst,
                           const TObjId             root,
                           bool                     toInsideOnly)
{
    const PointingObjectsFinder visitor(dst);

    if (!toInsideOnly)
        visitor(sh, root);

    traverseSubObjs(sh, root, visitor, /* leavesOnly */ false);
}

void redirectInboundEdges(
        SymHeap                 &sh,
        const TObjId            pointingFrom,
        const TObjId            pointingTo,
        const TObjId            redirectTo)
{
    // go through all objects pointing at/inside pointingTo
    SymHeap::TContObj refs;
    gatherPointingObjects(sh, refs, pointingTo, /* toInsideOnly */ false);
    BOOST_FOREACH(const TObjId obj, refs) {
        if (pointingFrom != objRoot(sh, obj))
            // pointed from elsewhere, keep going
            continue;

        TObjId parent = sh.pointsTo(sh.valueOf(obj));
        CL_BREAK_IF(parent <= 0);

        // seek obj's root
        int nth;
        TFieldIdxChain invIc;
        while (OBJ_INVALID != (parent = sh.objParent(parent, &nth)))
            invIc.push_back(nth);

        // now take the selector chain reversely
        TObjId target = redirectTo;
        BOOST_REVERSE_FOREACH(int nth, invIc) {
            target = sh.subObj(target, nth);
            CL_BREAK_IF(OBJ_INVALID == target);
        }

        // redirect!
        sh.objSetValue(obj, sh.placedAt(target));
    }
}

