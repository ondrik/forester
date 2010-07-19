/*
 * Copyright (C) 2009 Kamil Dudka <kdudka@redhat.com>
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
#include "symproc.hh"

#include <cl/cl_msg.hh>

#include "symabstract.hh"
#include "symbt.hh"
#include "symgc.hh"
#include "symheap.hh"
#include "symplot.hh"
#include "symstate.hh"
#include "symutil.hh"
#include "util.hh"

#include <stack>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>

// /////////////////////////////////////////////////////////////////////////////
// SymProc implementation
TValueId SymProc::heapValFromCst(const struct cl_operand &op) {
    bool isBool = false;
    enum cl_type_e code = op.type->code;
    switch (code) {
        case CL_TYPE_BOOL:
            isBool = true;
            break;

        case CL_TYPE_INT:
        case CL_TYPE_PTR:
            break;

        default:
            TRAP;
    }

    const struct cl_cst &cst = op.data.cst;
    code = cst.code;
    switch (code) {
        case CL_TYPE_INT:
            if (isBool) {
                return (cst.data.cst_int.value)
                    ? VAL_TRUE
                    : VAL_FALSE;
            } else {
                if (!cst.data.cst_int.value)
                    return VAL_NULL;

                // create a new unknown non-NULL value
                TValueId val = heap_.valCreateUnknown(UV_UNKNOWN, op.type);
                heap_.neqOp(SymHeap::NEQ_ADD, val, VAL_NULL);
                return val;
            }

        case CL_TYPE_FNC: {
            // wrap fnc uid as SymHeap value
            const int uid = cst.data.cst_fnc.uid;
            return heap_.valCreateCustom(op.type, uid);
        }

        case CL_TYPE_STRING: {
            // FIXME: this temporary workaround is highly suboptimal, subtle
            // and error-prone !!!
            CL_WARN_MSG(lw_,
                    "CL_TYPE_STRING not supported by heapValFromCst()");
            const int uid = reinterpret_cast<long>(cst.data.cst_string.value);
            return heap_.valCreateCustom(op.type, uid);
        }

        default:
            TRAP;
            return VAL_INVALID;
    }
}

void SymProc::heapObjHandleAccessorDeref(TObjId *pObj) {
    EUnknownValue code;

    // TODO: check --- it should be pointer variable, => NON-ABSTRACT ?

    // attempt to dereference
    const TValueId val = heap_.valueOf(*pObj);
    switch (val) {
        case VAL_NULL:
            CL_ERROR_MSG(lw_, "dereference of NULL value");
            goto fail_with_bt;

        case VAL_INVALID:
            TRAP;
            goto fail;

        case VAL_DEREF_FAILED:
            goto fail;

        default:
            break;
    }

    // do we really know the value?
    code = heap_.valGetUnknown(val);
    switch (code) {
        case UV_KNOWN:
        case UV_ABSTRACT:
            break;

        case UV_UNKNOWN:
            *pObj = OBJ_UNKNOWN;
            return;

        case UV_UNINITIALIZED:
            CL_ERROR_MSG(lw_, "dereference of uninitialized value");
            goto fail_with_bt;
    }

    // value lookup
    *pObj = heap_.pointsTo(val);
    switch (*pObj) {
        case OBJ_LOST:
            CL_ERROR_MSG(lw_, "dereference of non-existing non-heap object");
            goto fail_with_bt;

        case OBJ_DELETED:
            CL_ERROR_MSG(lw_, "dereference of already deleted heap object");
            goto fail_with_bt;

        case OBJ_UNKNOWN:
        case OBJ_INVALID:
            TRAP;

        default:
            // valid object
            return;
    }

fail_with_bt:
    bt_->printBackTrace();

fail:
    *pObj = OBJ_DEREF_FAILED;
}

void SymProc::heapObjHandleAccessorItem(TObjId *pObj,
                                        const struct cl_accessor *ac)
{
    // access subObj
    const int id = ac->data.item.id;
    *pObj = heap_.subObj(*pObj, id);

    // check result of the SymHeap operation
    if (OBJ_INVALID == *pObj)
        *pObj = /* FIXME: misleading */ OBJ_DEREF_FAILED;
}

void SymProc::heapObjHandleAccessor(TObjId *pObj,
                                    const struct cl_accessor *ac)
{
    const enum cl_accessor_e code = ac->code;
    switch (code) {
        case CL_ACCESSOR_DEREF:
            this->heapObjHandleAccessorDeref(pObj);
            return;

        case CL_ACCESSOR_ITEM:
            this->heapObjHandleAccessorItem(pObj, ac);
            return;

        case CL_ACCESSOR_REF:
            // CL_ACCESSOR_REF will be processed wihtin heapValFromOperand()
            // on the way out from here ... otherwise we are encountering
            // a bug!
            return;

        case CL_ACCESSOR_DEREF_ARRAY:
            CL_WARN_MSG(lw_, "CL_ACCESSOR_DEREF_ARRAY not implemented yet");
            *pObj = OBJ_DEREF_FAILED;
            return;
    }
}

namespace {

TObjId varFromOperand(const struct cl_operand &op, const SymHeap &sh,
                      const SymBackTrace *bt)
{
    int uid;

    const enum cl_operand_e code = op.code;
    switch (code) {
        case CL_OPERAND_VAR:
            uid = op.data.var.id;
            break;

        case CL_OPERAND_REG:
            uid = op.data.reg.id;
            break;

        default:
            TRAP;
            return OBJ_INVALID;
    }

    const int nestLevel = bt->countOccurrencesOfTopFnc();
    const CVar cVar(uid, nestLevel);
    return sh.objByCVar(cVar);
}

} // namespace

TObjId SymProc::heapObjFromOperand(const struct cl_operand &op) {
    TObjId var = varFromOperand(op, heap_, bt_);
    if (OBJ_INVALID == var)
        // unable to resolve static variable
        TRAP;

    // process all accessors
    const struct cl_accessor *ac = op.accessor;
    while (ac) {
        this->heapObjHandleAccessor(&var, ac);
        ac = ac->next;
    }

    return var;
}

bool SymProc::lhsFromOperand(TObjId *pObj, const struct cl_operand &op) {
    *pObj = this->heapObjFromOperand(op);
    switch (*pObj) {
        case OBJ_UNKNOWN:
            CL_DEBUG_MSG(lw_,
                    "ignoring OBJ_UNKNOWN as lhs, this is definitely a bug "
                    "if there is no error reported above...");
            // fall through!
        case OBJ_DEREF_FAILED:
            return false;

        case OBJ_LOST:
        case OBJ_DELETED:
        case OBJ_INVALID:
            TRAP;

        default:
            return true;
    }
}

namespace {
    template <class THeap>
    TValueId valueFromVar(THeap &heap, TObjId var, const struct cl_type *clt,
                          const struct cl_accessor *ac)
    {
        switch (var) {
            case OBJ_INVALID:
                TRAP;
                return VAL_INVALID;

            case OBJ_UNKNOWN:
                return heap.valCreateUnknown(UV_UNKNOWN, clt);

            case OBJ_DELETED:
            case OBJ_DEREF_FAILED:
            case OBJ_LOST:
                return VAL_DEREF_FAILED;

            case OBJ_RETURN:
            default:
                break;
        }

        // seek the last accessor
        while (ac && ac->next)
            ac = ac->next;

        // handle CL_ACCESSOR_REF if any
        return (ac && ac->code == CL_ACCESSOR_REF)
            ? heap.placedAt(var)
            : heap.valueOf(var);
    }
}

TValueId SymProc::heapValFromOperand(const struct cl_operand &op) {
    const enum cl_operand_e code = op.code;
    switch (code) {
        case CL_OPERAND_VAR:
        case CL_OPERAND_REG:
            return valueFromVar(heap_,
                    this->heapObjFromOperand(op),
                    op.type, op.accessor);

        case CL_OPERAND_CST:
            return this->heapValFromCst(op);

        default:
            TRAP;
            return VAL_INVALID;
    }
}

int /* uid */ SymProc::fncFromOperand(const struct cl_operand &op) {
    if (CL_OPERAND_CST == op.code) {
        // direct call
        const struct cl_cst &cst = op.data.cst;
        if (CL_TYPE_FNC != cst.code)
            TRAP;

        return cst.data.cst_fnc.uid;

    } else {
        // indirect call
        const TValueId val = this->heapValFromOperand(op);
        if (VAL_INVALID == val)
            // Oops, it does not look as indirect call actually
            TRAP;

        return heap_.valGetCustom(/* TODO: check type */ 0, val);
    }
}

void SymProc::heapObjDefineType(TObjId lhs, TValueId rhs) {
    const TObjId var = heap_.pointsTo(rhs);
    if (OBJ_INVALID == var)
        TRAP;

    const struct cl_type *clt = heap_.objType(lhs);
    if (!clt)
        return;

    if (clt->code != CL_TYPE_PTR)
        TRAP;

    // move to next clt
    // --> what are we pointing to actually?
    clt = clt->items[0].type;
    if (!clt)
        TRAP;

    if (CL_TYPE_VOID == clt->code)
        return;

    const int cbGot = heap_.objSizeOfAnon(var);
    if (!cbGot)
        // anonymous object of zero size
        TRAP;

    const int cbNeed = clt->size;
    if (cbGot != cbNeed) {
        static const char szMsg[] =
            "amount of allocated memory not accurate";
        if (cbGot < cbNeed)
            CL_ERROR_MSG(lw_, szMsg);
        else
            CL_WARN_MSG(lw_, szMsg);

        CL_NOTE_MSG(lw_, "allocated: " << cbGot  << " bytes");
        CL_NOTE_MSG(lw_, " expected: " << cbNeed << " bytes");
    }

    heap_.objDefineType(var, clt);
}

void SymProc::heapSetSingleVal(TObjId lhs, TValueId rhs) {
    // save the old value, which is going to be overwritten
    const TValueId oldValue = heap_.valueOf(lhs);
    if (VAL_INVALID == oldValue)
        TRAP;

    if (0 < rhs) {
        const TObjId target = heap_.pointsTo(rhs);
        if (0 < target && !heap_.objType(target))
            // anonymous object is going to be specified by a type
            this->heapObjDefineType(lhs, rhs);
    }

    heap_.objSetValue(lhs, rhs);
    if (collectJunk(heap_, oldValue, lw_))
        bt_->printBackTrace();
}

void SymProc::objSetValue(TObjId lhs, TValueId rhs) {
    if (VAL_DEREF_FAILED == rhs) {
        // we're already on an error path
        heap_.objSetValue(lhs, rhs);
        return;
    }

    // DFS for composite types
    typedef std::pair<TObjId, TValueId> TItem;
    std::stack<TItem> todo;
    push(todo, lhs, rhs);
    while (!todo.empty()) {
        TObjId lhs;
        TValueId rhs;
        boost::tie(lhs, rhs) = todo.top();
        todo.pop();

        const TObjId rObj = heap_.valGetCompositeObj(rhs);
        if (OBJ_INVALID == rObj) {
            // non-composite value
            this->heapSetSingleVal(lhs, rhs);
            continue;
        }

        const struct cl_type *clt = heap_.objType(rObj);
        if (!clt || clt->code != CL_TYPE_STRUCT || clt != heap_.objType(lhs))
            // type-info problem
            TRAP;

        // iterate through all fields
        for (int i = 0; i < clt->item_cnt; ++i) {
            const TObjId lSub = heap_.subObj(lhs, i);
            const TObjId rSub = heap_.subObj(rObj, i);
            if (lSub <= 0 || rSub <= 0)
                // composition problem
                TRAP;

            // schedule sub for next wheel
            const TValueId rSubVal = heap_.valueOf(rSub);
            push(todo, lSub, rSubVal);
        }
    }
}

void SymProc::objDestroy(TObjId obj) {
    // gather potentialy destroyed pointer sub-values
    std::vector<TValueId> ptrs;
    getPtrValues(ptrs, heap_, obj);

    // destroy object recursively
    heap_.objDestroy(obj);

    // now check for JUNK
    bool junk = false;
    BOOST_FOREACH(TValueId val, ptrs) {
        if (collectJunk(heap_, val, lw_))
            junk = true;
    }

    if (junk)
        // print backtrace at most once per one call of objDestroy()
        bt_->printBackTrace();
}

void SymProc::execFree(const CodeStorage::TOperandList &opList) {
    if (/* dst + fnc + ptr */ 3 != opList.size())
        TRAP;

    if (CL_OPERAND_VOID != opList[0].code)
        // Oops, free() does not usually return a value
        TRAP;

    const TValueId val = heapValFromOperand(opList[/* ptr given to free() */2]);
    if (VAL_INVALID == val)
        // could not resolve value to be freed
        TRAP;

    switch (val) {
        case VAL_NULL:
            CL_DEBUG_MSG(lw_, "ignoring free() called with NULL value");
            return;

        case VAL_DEREF_FAILED:
            return;

        default:
            break;
    }

    const EUnknownValue code = heap_.valGetUnknown(val);
    switch (code) {
        case UV_ABSTRACT:
            TRAP;
            // fall through!

        case UV_KNOWN:
            break;

        case UV_UNKNOWN:
            CL_DEBUG_MSG(lw_, "ignoring free() called on unknown value");
            return;

        case UV_UNINITIALIZED:
            CL_ERROR_MSG(lw_, "free() called on uninitialized value");
            bt_->printBackTrace();
            return;
    }

    const TObjId obj = heap_.pointsTo(val);
    switch (obj) {
        case OBJ_DELETED:
            CL_ERROR_MSG(lw_, "double free() detected");
            bt_->printBackTrace();
            return;

        case OBJ_LOST:
            // this is a double error in the analyzed program :-)
            CL_ERROR_MSG(lw_, "attempt to free a non-heap object"
                              ", which does not exist anyhow");
            bt_->printBackTrace();
            return;

        case OBJ_UNKNOWN:
        case OBJ_INVALID:
            TRAP;

        default:
            break;
    }

    // FIXME: should check ifAllocated (is there possibility for error?)
    //        aliasing problem for  free(first item pointer)
    CVar cVar;
    if (heap_.cVar(&cVar, obj)) {
        CL_DEBUG("about to free var #" << cVar.uid);
        CL_ERROR_MSG(lw_, "attempt to free a non-heap object");
        bt_->printBackTrace();
        return;
    }

    if (OBJ_INVALID != heap_.objParent(obj)) {
        CL_ERROR_MSG(lw_, "attempt to free a non-root object");
        bt_->printBackTrace();
        return;
    }

    CL_DEBUG_MSG(lw_, "executing free()");
    this->objDestroy(obj);
}

void SymProc::execMalloc(TState &state, const CodeStorage::TOperandList &opList,
                         bool fastMode)
{
    if (/* dst + fnc + size */ 3 != opList.size())
        TRAP;

    const struct cl_operand &dst = opList[0];
    const TObjId varLhs = this->heapObjFromOperand(dst);
    if (OBJ_INVALID == varLhs)
        // could not resolve lhs
        TRAP;

    const struct cl_operand &amount = opList[2];
    if (CL_OPERAND_CST != amount.code)
        // amount of allocated memory not constant
        TRAP;

    const struct cl_cst &cst = amount.data.cst;
    if (CL_TYPE_INT != cst.code)
        // amount of allocated memory not a number
        TRAP;

    const int cbAmount = cst.data.cst_int.value;
    CL_DEBUG_MSG(lw_, "executing malloc(" << cbAmount << ")");
    const TObjId obj = heap_.objCreateAnon(cbAmount);
    if (OBJ_INVALID == obj)
        // unable to create a heap object
        TRAP;

    const TValueId val = heap_.placedAt(obj);
    if (val <= 0)
        TRAP;

    if (!fastMode) {
        // OOM state simulation
        this->objSetValue(varLhs, VAL_NULL);
        state.insert(heap_);
    }

    // store the result of malloc
    this->objSetValue(varLhs, val);
    state.insert(heap_);
}

namespace {
    template <int N_ARGS, class TOpList>
    bool chkVoidCall(const TOpList &opList)
    {
        if (/* dst + fnc */ 2 != opList.size() - N_ARGS)
            return false;
        else
            return (CL_OPERAND_VOID == opList[0].code);
    }

    template <int NTH, class TOpList>
    bool readPlotName(std::string *dst, const TOpList opList)
    {
        const cl_operand &op = opList[NTH + /* dst + fnc */ 2];
        if (CL_OPERAND_CST != op.code)
            return false;

        const cl_cst &cst = op.data.cst;
        if (CL_TYPE_STRING != cst.code)
            return false;

        *dst = cst.data.cst_string.value;
        return true;
    }

    template <int NTH, class TOpList, class THeap, class TBt>
    bool readHeapVal(TValueId *dst, const TOpList opList, const THeap &heap,
                     TBt *bt)
    {
        // FIXME: we might use the already existing instance instead
        SymProc proc(const_cast<THeap &>(heap), bt);

        const cl_operand &op = opList[NTH + /* dst + fnc */ 2];
        const TValueId value = proc.heapValFromOperand(op);
        if (value < 0)
            return false;

        *dst = value;
        return true;
    }

    template <class TInsn, class THeap, class TBt>
    bool readNameAndValue(std::string *pName, TValueId *pValue,
                          const TInsn &insn, const THeap &heap, TBt bt)
    {
        const CodeStorage::TOperandList &opList = insn.operands;
        const LocationWriter lw(&insn.loc);

        if (!chkVoidCall<2>(opList))
            return false;

        if (!readHeapVal<0>(pValue, opList, heap, bt))
            return false;

        if (!readPlotName<1>(pName, opList))
            return false;

        return true;
    }

    template <class TStor, class TFnc, class THeap>
    bool fncFromHeapVal(const TStor &stor, const TFnc **dst, TValueId value,
                        const THeap &heap)
    {
        const int uid = heap.valGetCustom(/* pClt */ 0, value);
        if (-1 == uid)
            return false;

        // FIXME: get rid of the const_cast
        const TFnc *fnc = const_cast<TStor &>(stor).fncs[uid];
        if (!fnc)
            return false;

        *dst = fnc;
        return true;
    }

    void emitPrototypeError(const LocationWriter &lw, const std::string &fnc) {
        CL_WARN_MSG(lw, "incorrectly called "
                << fnc << "() not recognized as built-in");
    }

    void emitPlotError(const LocationWriter &lw, const std::string &plotName) {
        CL_WARN_MSG(lw, "error while plotting '" << plotName << "'");
    }

    template <class TInsn, class THeap>
    bool callPlot(const TInsn &insn, const THeap &heap) {
        const CodeStorage::TOperandList &opList = insn.operands;
        const LocationWriter lw(&insn.loc);

        std::string plotName;
        if (!chkVoidCall<1>(opList) || !readPlotName<0>(&plotName, opList)) {
            emitPrototypeError(lw, "___sl_plot");
            return false;
        }

        const CodeStorage::Storage &stor = *insn.stor;
        SymHeapPlotter plotter(stor, heap);
        if (!plotter.plot(plotName))
            emitPlotError(lw, plotName);

        return true;
    }

    template <class TInsn, class THeap, class TBt>
    bool callPlotByPtr(const TInsn &insn, const THeap &heap, TBt *bt) {
        const LocationWriter lw(&insn.loc);

        std::string plotName;
        TValueId value;
        if (!readNameAndValue(&plotName, &value, insn, heap, bt)) {
            emitPrototypeError(lw, "___sl_plot_by_ptr");
            return false;
        }

        const CodeStorage::Storage &stor = *insn.stor;
        SymHeapPlotter plotter(stor, heap);
        if (!plotter.plotHeapValue(plotName, value))
            emitPlotError(lw, plotName);

        return true;
    }

    template <class TInsn, class THeap, class TBt>
    bool callPlotStackFrame(const TInsn &insn, const THeap &heap, TBt *bt) {
        const CodeStorage::Storage &stor = *insn.stor;
        const LocationWriter lw(&insn.loc);

        std::string plotName;
        TValueId value;
        const CodeStorage::Fnc *fnc;

        if (!readNameAndValue(&plotName, &value, insn, heap, bt)
                || !fncFromHeapVal(stor, &fnc, value, heap))
        {
            emitPrototypeError(lw, "___sl_plot_stack_frame");
            return false;
        }

        SymHeapPlotter plotter(stor, heap);
        if (!plotter.plotStackFrame(plotName, *fnc, bt))
            emitPlotError(lw, plotName);

        return true;
    }
}

bool SymProc::execCall(TState &dst, const CodeStorage::Insn &insn,
                       SymProcExecParams ep)
{
    const CodeStorage::TOperandList &opList = insn.operands;
    const struct cl_operand &fnc = opList[1];
    if (CL_OPERAND_CST != fnc.code)
        return false;

    const struct cl_cst &cst = fnc.data.cst;
    if (CL_TYPE_FNC != cst.code)
        return false;

    if (CL_SCOPE_GLOBAL != fnc.scope || !cst.data.cst_fnc.is_extern)
        return false;

    const char *fncName = cst.data.cst_fnc.name;
    if (!fncName)
        return false;

    if (STREQ(fncName, "malloc")) {
        this->execMalloc(dst, opList, ep.fastMode);
        return true;
    }

    if (STREQ(fncName, "free")) {
        this->execFree(opList);
        goto call_done;
    }

    if (STREQ(fncName, "abort")) {
        if (opList.size() != 2 || opList[0].code != CL_OPERAND_VOID)
            TRAP;

        // do nothing for abort()
        goto call_done;
    }

    if (STREQ(fncName, "___sl_plot")) {
        if (ep.skipPlot)
            CL_DEBUG_MSG(lw_, "___sl_plot skipped per user's request");

        else if (!callPlot(insn, heap_))
            // invalid prototype etc.
            return false;

        goto call_done;
    }

    if (STREQ(fncName, "___sl_plot_stack_frame")) {
        if (ep.skipPlot)
            CL_DEBUG_MSG(lw_,
                    "___sl_plot_stack_frame skipped per user's request");

        else if (!callPlotStackFrame(insn, heap_, bt_))
            // invalid prototype etc.
            return false;

        goto call_done;
    }

    if (STREQ(fncName, "___sl_plot_by_ptr")) {
        if (ep.skipPlot)
            CL_DEBUG_MSG(lw_, "___sl_plot_by_ptr skipped per user's request");

        else if (!callPlotByPtr(insn, heap_, bt_))
            // invalid prototype etc.
            return false;

        goto call_done;
    }

    // no built-in has been matched
    return false;

call_done:
    dst.insert(heap_);
    return true;
}

namespace {
    bool handleUnopTruthNotTrivial(TValueId &val) {
        switch (val) {
            case VAL_FALSE:
                val = VAL_TRUE;
                return true;

            case VAL_TRUE:
                val = VAL_FALSE;
                return true;

            case VAL_INVALID:
                return true;

            default:
                return false;
        }
    }
}

template <class THeap>
void handleUnopTruthNot(THeap &heap, TValueId &val, const struct cl_type *clt) {
    if (!clt || clt->code != CL_TYPE_BOOL)
        // inappropriate type for CL_UNOP_TRUTH_NOT
        TRAP;

    if (handleUnopTruthNotTrivial(val))
        // we are done
        return;

    const EUnknownValue code = heap.valGetUnknown(val);
    if (UV_KNOWN == code || UV_ABSTRACT == code)
        // the value we got is not VAL_TRUE, VAL_FALSE, nor an unknown value
        TRAP;

    const TValueId origValue = val;
    val = heap.valDuplicateUnknown(origValue);
    // FIXME: not tested
    TRAP;
    heap.addEqIf(origValue, val, VAL_TRUE, /* neg */ true);
}

template <class THeap>
TValueId handleOpCmpBool(THeap &heap, enum cl_binop_e code,
                         const struct cl_type *dstClt, TValueId v1, TValueId v2)
{
    // TODO: describe the following magic somehow
    TValueId valElim = VAL_FALSE;
    switch (code) {
        case CL_BINOP_EQ:
            valElim = VAL_TRUE;
            // fall through!

        case CL_BINOP_NE:
            break;

        default:
            // crazy comparison of bool values
            TRAP;
            return VAL_INVALID;
    }
    if (v1 == valElim)
        return v2;
    if (v2 == valElim)
        return v1;

    if (v1 < 0 || v2 < 0)
        TRAP;

    // FIXME: not tested
    TRAP;
    bool result;
    if (!heap.proveEq(&result, v1, v2))
        return heap.valCreateUnknown(UV_UNKNOWN, dstClt);

    // invert if needed
    if (CL_BINOP_NE == code)
        result = !result;

    return (result)
        ? VAL_TRUE
        : VAL_FALSE;
}

template <class THeap>
TValueId handleOpCmpInt(THeap &heap, enum cl_binop_e code,
                        const struct cl_type *dstClt, TValueId v1, TValueId v2)
{
    if (VAL_INVALID == v1 || VAL_INVALID == v2)
        TRAP;

    // check if the values are equal
    bool eq;
    if (!heap.proveEq(&eq, v1, v2))
        // we don't know if the values are equal or not
        goto who_knows;

    switch (code) {
        case CL_BINOP_LT:
        case CL_BINOP_GT:
            if (eq)
                // we got either (x < x), or (x > x)
                return VAL_FALSE;
            else
                // bad luck, hard to compare unknown values for < >
                goto who_knows;

        case CL_BINOP_LE:
        case CL_BINOP_GE:
            if (eq)
                // we got either (x <= x), or (x >= x)
                return VAL_TRUE;
            else
                // bad luck, hard to compare unknown values for <= >=
                goto who_knows;

        case CL_BINOP_NE:
            eq = !eq;
            // fall through!

        case CL_BINOP_EQ:
            return (eq)
                ? VAL_TRUE
                : VAL_FALSE;

        default:
            TRAP;
    }

who_knows:
    // unknown result of int comparison
    TValueId val = heap.valCreateUnknown(UV_UNKNOWN, dstClt);
    switch (code) {
        case CL_BINOP_EQ:
            heap.addEqIf(val, v1, v2, /* neg */ false);
            return val;

        case CL_BINOP_NE:
            heap.addEqIf(val, v1, v2, /* neg */ true);
            return val;

        default:
            // EqIf predicate is not suitable for <, <=, >, >=
            return val;
    }
}

template <class THeap>
TValueId handleOpCmpPtr(THeap &heap, enum cl_binop_e code,
                        const struct cl_type *dstClt, TValueId v1, TValueId v2)
{
    if (VAL_DEREF_FAILED == v1 || VAL_DEREF_FAILED == v2)
        return VAL_DEREF_FAILED;

    if (v1 < 0 || v2 < 0)
        TRAP;

    switch (code) {
        case CL_BINOP_EQ:
        case CL_BINOP_NE:
            break;

        default:
            // crazy comparison of pointer values
            TRAP;
            return VAL_INVALID;
    }

    // check if the values are equal
    bool result;
    if (!heap.proveEq(&result, v1, v2)) {
        // we don't know if the values are equal or not
        const TValueId val = heap.valCreateUnknown(UV_UNKNOWN, dstClt);

        // store the relation over the triple (val, v1, v2) for posteriors
        heap.addEqIf(val, v1, v2, /* neg */ CL_BINOP_NE == code);
        return val;
    }

    // invert if needed
    if (CL_BINOP_NE == code)
        result = !result;

    return (result)
        ? VAL_TRUE
        : VAL_FALSE;
}

template <class THeap>
TValueId handleOpCmp(THeap &heap, enum cl_binop_e code,
                     const struct cl_type *dstClt, const struct cl_type *clt,
                     TValueId v1, TValueId v2)
{
    // clt is assumed to be valid at this point
    switch (clt->code) {
        case CL_TYPE_PTR:  return handleOpCmpPtr (heap, code, dstClt, v1, v2);
        case CL_TYPE_BOOL: return handleOpCmpBool(heap, code, dstClt, v1, v2);
        case CL_TYPE_INT:  return handleOpCmpInt (heap, code, dstClt, v1, v2);
        default:
            // unexpected clt->code
            TRAP;
            return VAL_INVALID;
    }
}

TValueId handlePointerPlus(const SymHeap &sh, const struct cl_type *clt,
                           TValueId ptr, const struct cl_operand &op)
{
    // jump to _target_ type
    if (!clt || clt->code != CL_TYPE_PTR)
        TRAP;
    clt = clt->items[0].type;

    // read integral offset
    if (CL_OPERAND_CST != op.code)
        TRAP;
    const struct cl_cst &cst = op.data.cst;
    if (CL_TYPE_INT != op.type->code)
        TRAP;
    int off = cst.data.cst_int.value;
    CL_DEBUG("handlePointerPlus(): " << off << "b offset requested");

    // seek root object while cumulating the offset
    TObjId obj = sh.pointsTo(ptr);
    TObjId parent;
    int nth;
    while (OBJ_INVALID != (parent = sh.objParent(obj, &nth))) {
        const struct cl_type *cltParent = sh.objType(parent);
        if (cltParent->item_cnt <= nth)
            TRAP;

        off += cltParent->items[nth].offset;
        obj = parent;
    }

    if (off)
        // TODO: handle general moves within a single object
        TRAP;

    // get the final address and check type compatibility
    const TValueId addr = sh.placedAt(obj);
    const struct cl_type *cltDst = sh.valType(addr);
    if (!cltDst || *cltDst != *clt)
        // type problem
        TRAP;

    return addr;
}

// template for generic (unary, binary, ...) operator handlers
template <int ARITY, class TProc>
struct OpHandler {
    static TValueId handleOp(TProc &proc, int code, const TValueId rhs[ARITY],
                             const struct cl_type *clt[ARITY +/* dst */1]);
};

// unary operator handler
template <class TProc>
struct OpHandler</* unary */ 1, TProc> {
    static TValueId handleOp(TProc &proc, int iCode, const TValueId rhs[1],
                             const struct cl_type *clt[1 + /* dst type */ 1])
    {
        TValueId val = rhs[0];

        const enum cl_unop_e code = static_cast<enum cl_unop_e>(iCode);
        switch (code) {
            case CL_UNOP_TRUTH_NOT:
                handleUnopTruthNot(proc.heap_, val, clt[0]);
                // fall through!

            case CL_UNOP_ASSIGN:
                break;

            default:
                TRAP;
        }

        return val;
    }
};

// binary operator handler
template <class TProc>
struct OpHandler</* binary */ 2, TProc> {
    static TValueId handleOp(TProc &proc, int iCode, const TValueId rhs[2],
                             const struct cl_type *clt[2 + /* dst type */ 1])
    {
        const struct cl_type *const cltA = clt[0];
        const struct cl_type *const cltB = clt[1];
        if (!cltA || !cltB)
            // type-info is missing
            TRAP;

        SymHeap &heap = proc.heap_;
        if (*cltA != *cltB) {
            // we don't support arrays, pointer arithmetic and the like,
            // the types therefor have to match with each other for a binary
            // operator
            CL_ERROR_MSG(proc.lw_,
                    "mixing of types for a binary operator not supported yet");
            CL_NOTE_MSG(proc.lw_,
                    "the analysis may crash because of the error above");
            return heap.valCreateUnknown(UV_UNKNOWN, cltA);
        }

        const enum cl_binop_e code = static_cast<enum cl_binop_e>(iCode);
        switch (code) {
            case CL_BINOP_EQ:
            case CL_BINOP_NE:
            case CL_BINOP_LT:
            case CL_BINOP_GT:
            case CL_BINOP_LE:
            case CL_BINOP_GE:
                return handleOpCmp(heap, code, clt[2], cltA, rhs[0], rhs[1]);

            case CL_BINOP_PLUS:
            case CL_BINOP_MINUS:
                CL_WARN_MSG(proc.lw_, "binary operator not implemented yet");
                return heap.valCreateUnknown(UV_UNKNOWN, cltA);

            default:
                TRAP;
                return VAL_INVALID;
        }
    }
};

// C++ does not support partial specialisation of function templates, this helps
template <int ARITY, class TProc>
TValueId handleOp(TProc &proc, int code, const TValueId rhs[ARITY],
                  const struct cl_type *clt[ARITY + /* dst type */ 1])
{
    return OpHandler<ARITY, TProc>::handleOp(proc, code, rhs, clt);
}


template <int ARITY>
void SymProc::execOp(const CodeStorage::Insn &insn) {
    // resolve lhs
    TObjId varLhs = OBJ_INVALID;
    const struct cl_operand &dst = insn.operands[/* dst */ 0];
    if (!this->lhsFromOperand(&varLhs, dst))
        return;

    // store cl_type of dst operand
    const struct cl_type *clt[ARITY + /* dst type */ 1];
    clt[/* dst type */ ARITY] = dst.type;

    // gather rhs values (and type-info)
    const CodeStorage::TOperandList &opList = insn.operands;
    TValueId rhs[ARITY];
    for (int i = 0; i < ARITY; ++i) {
        const struct cl_operand &op = opList[i + /* [+dst] */ 1];
        clt[i] = op.type;
        rhs[i] = this->heapValFromOperand(op);
        if (VAL_INVALID == rhs[i])
            TRAP;
    }

    TValueId valResult = VAL_INVALID;
    if (2 == ARITY && CL_BINOP_POINTER_PLUS
            == static_cast<enum cl_binop_e>(insn.subCode))
    {
        valResult = handlePointerPlus(heap_, clt[/* dst type */ ARITY],
                                      rhs[0], opList[/* src2 */ 2]);
        goto rhs_ready;
    }

    // handle generic operator and store result
    valResult = handleOp<ARITY>(*this, insn.subCode, rhs, clt);

rhs_ready:
    this->objSetValue(varLhs, valResult);
}

bool SymProc::concretizeLoop(TState &dst, const CodeStorage::Insn &insn,
                             const struct cl_operand &op)
{
    bool hit = false;

    TSymHeapList todo;
    todo.push_back(heap_);
    while (!todo.empty()) {
        SymHeap &sh = todo.front();
        SymProc proc(sh, bt_);
        proc.setLocation(lw_);

        // we expect a pointer at this point
        const TObjId ptr = varFromOperand(op, sh, bt_);
        const TValueId val = sh.valueOf(ptr);
        if (0 < val && UV_ABSTRACT == sh.valGetUnknown(val)) {
            hit = true;
            concretizeObj(sh, val, todo);
        }

        // process the current heap and move to the next one (if any)
        proc.execCore(dst, insn, SymProcExecParams());
        todo.pop_front();
    }

    return hit;
}

namespace {
bool checkForDeref(const struct cl_operand &op, const CodeStorage::Insn &insn) {
    const enum cl_insn_e code = insn.code;
    const struct cl_accessor *ac = op.accessor;
    if (ac && CL_ACCESSOR_DEREF == ac->code) {
        // we expect the dereference only as the first accessor
        if (CL_INSN_UNOP != code ||
                CL_UNOP_ASSIGN != static_cast<enum cl_unop_e>(insn.subCode))
            TRAP;

        // we should go through concretization
        return true;
    }

    return false;
}
} // namespace

bool SymProc::concretizeIfNeeded(TState &results, const CodeStorage::Insn &insn)
{
    const size_t opCnt = insn.operands.size();
    if (opCnt != /* deref */ 2 && opCnt != /* free() */ 3)
        // neither dereference, nor free()
        return false;

    bool hitDeref = false;
    bool hitConcretize = false;
    BOOST_FOREACH(const struct cl_operand &op, insn.operands) {
        if (!checkForDeref(op, insn))
            continue;

        hitDeref = true;

        if (hitConcretize)
            // FIXME: are we ready for two dereferences within one insn?
            TRAP;

        hitConcretize = this->concretizeLoop(results, insn, op);
    }
    if (hitDeref)
        return true;

    const enum cl_insn_e code = insn.code;
    const struct cl_operand &src = insn.operands[/* src */ 1];
    if (CL_INSN_CALL != code || CL_OPERAND_CST != src.code)
        return false;

    const struct cl_cst &cst = src.data.cst;
    if (CL_TYPE_FNC != cst.code || !STREQ(cst.data.cst_fnc.name, "free"))
        return false;

    // assume call of free()
    this->concretizeLoop(results, insn, insn.operands[/* addr */ 2]);
    return true;
}

bool SymProc::execCore(TState &dst, const CodeStorage::Insn &insn,
                       SymProcExecParams ep)
{
    const enum cl_insn_e code = insn.code;
    switch (code) {
        case CL_INSN_UNOP:
            this->execOp<1>(insn);
            break;

        case CL_INSN_BINOP:
            this->execOp<2>(insn);
            break;

        case CL_INSN_CALL:
            return this->execCall(dst, insn, ep);

        default:
            TRAP;
            return false;
    }

    dst.insert(heap_);
    return true;
}

bool SymProc::exec(TState &dst, const CodeStorage::Insn &insn,
                   SymProcExecParams ep)
{
    lw_ = &insn.loc;
    if (this->concretizeIfNeeded(dst, insn))
        // concretization loop done
        return true;

    return this->execCore(dst, insn, ep);
}
