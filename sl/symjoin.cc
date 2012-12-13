/*
 * Copyright (C) 2010-2012 Kamil Dudka <kdudka@redhat.com>
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
#include "symjoin.hh"

#include <cl/cl_msg.hh>
#include <cl/cldebug.hh>
#include <cl/clutil.hh>

#include "prototype.hh"
#include "symcmp.hh"
#include "symdiscover.hh"
#include "symgc.hh"
#include "symplot.hh"
#include "symseg.hh"
#include "symutil.hh"
#include "symtrace.hh"
#include "worklist.hh"
#include "util.hh"

#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>

static bool debuggingSymJoin = static_cast<bool>(DEBUG_SYMJOIN);

#define SJ_DEBUG(...) do {                                                  \
    if (::debuggingSymJoin)                                                 \
        CL_DEBUG("SymJoin: " << __VA_ARGS__);                               \
} while (0)

void debugSymJoin(const bool enable)
{
    if (enable == ::debuggingSymJoin)
        return;

    CL_DEBUG("symjoin: debugSymJoin(" << enable << ") takes effect");
    ::debuggingSymJoin = enable;
}

#define SJ_FLDT(fldDst, fld1, fld2)     \
    "(fldDst = #" << fldDst.fieldId()   \
    << ", fld1 = #" << fld1.fieldId()   \
    << ", fld2 = #" << fld2.fieldId()   \
    << ")"

#define SJ_ITEM(item) SJ_FLDT(item.fldDst, item.fld1, item.fld2)
#define SJ_OBJP(o1, o2) "(o1 = #" << o1 << ", o2 = #" << o2 << ")"
#define SJ_VALP(v1, v2) "(v1 = #" << v1 << ", v2 = #" << v2 << ")"

// mapping direction
enum {
    /// left-to-right
    DIR_LTR = 0,

    /// right-to-left
    DIR_RTL = 1
};

template <class T>
class WorkListWithUndo: public WorkList<T> {
    private:
        typedef WorkList<T> TBase;

    public:
        /// push an @b already @b processed item back to WorkList
        void undo(const T &item) {
            CL_BREAK_IF(!hasKey(TBase::seen_, item));
            TBase::todo_.push(item);
        }
};

struct SchedItem {
    FldHandle           fldDst;
    FldHandle           fld1;
    FldHandle           fld2;
    TProtoLevel         ldiff;

    SchedItem():
        ldiff(0)
    {
    }

    SchedItem(
            const FldHandle        &fldDst_,
            const FldHandle        &fld1_,
            const FldHandle        &fld2_,
            const TProtoLevel       ldiff_):
        fldDst(fldDst_),
        fld1(fld1_),
        fld2(fld2_),
        ldiff(ldiff_)
    {
    }
};

// needed by std::set
inline bool operator<(const SchedItem &a, const SchedItem &b)
{
    if (a.fld1 < b.fld1)
        return true;
    if (b.fld1 < a.fld1)
        return false;

    if (a.fld2 < b.fld2)
        return true;
    if (b.fld2 < a.fld2)
        return false;

    return (a.ldiff < b.ldiff);
}

typedef WorkListWithUndo<SchedItem>                             TWorkList;

typedef TObjMap                                                 TObjMapBidir[2];

typedef std::map<TValPair /* (v1, v2) */, TValId /* dst */>     TJoinCache;

/// current state, common for joinSymHeaps() and joinData()
struct SymJoinCtx {
    SymHeap                    &dst;
    SymHeap                    &sh1;
    SymHeap                    &sh2;

    const TProtoLevel           l1Drift;
    const TProtoLevel           l2Drift;

    TValMapBidir                valMap1;
    TValMapBidir                valMap2;

    TObjMapBidir                objMap1;
    TObjMapBidir                objMap2;

    TWorkList                   wl;
    EJoinStatus                 status;
    bool                        forceThreeWay;
    bool                        allowThreeWay;

    std::set<TObjId /* dst */>  protos;

    TJoinCache                  joinCache;

    void initValMaps() {
        // VAL_NULL should be always mapped to VAL_NULL
        valMap1[0][VAL_NULL] = VAL_NULL;
        valMap1[1][VAL_NULL] = VAL_NULL;
        valMap2[0][VAL_NULL] = VAL_NULL;
        valMap2[1][VAL_NULL] = VAL_NULL;
        const TValPair vp(VAL_NULL, VAL_NULL);
        joinCache[vp] = VAL_NULL;

        // OBJ_NULL should be always mapped to OBJ_NULL
        objMap1[0][OBJ_NULL] = OBJ_NULL;
        objMap1[1][OBJ_NULL] = OBJ_NULL;
        objMap2[0][OBJ_NULL] = OBJ_NULL;
        objMap2[1][OBJ_NULL] = OBJ_NULL;
    }

    /// constructor used by joinSymHeaps()
    SymJoinCtx(SymHeap &dst_, SymHeap &sh1_, SymHeap &sh2_,
            const bool allowThreeWay_):
        dst(dst_),
        sh1(sh1_),
        sh2(sh2_),
        l1Drift(0),
        l2Drift(0),
        status(JS_USE_ANY),
        forceThreeWay(false),
        allowThreeWay((1 < (SE_ALLOW_THREE_WAY_JOIN)) && allowThreeWay_)
    {
        initValMaps();
    }

    /// constructor used by joinData()
    SymJoinCtx(SymHeap &sh_, TProtoLevel l1Drift_, TProtoLevel l2Drift_):
        dst(sh_),
        sh1(sh_),
        sh2(sh_),
        l1Drift(l1Drift_),
        l2Drift(l2Drift_),
        status(JS_USE_ANY),
        forceThreeWay(false),
        allowThreeWay(0 < (SE_ALLOW_THREE_WAY_JOIN))
    {
        initValMaps();
    }

    bool joiningData() const {
        return (&dst == &sh1)
            && (&dst == &sh2);
    }
};

/// handy when debugging
void dump_ctx(const SymJoinCtx &ctx)
{
    using std::cout;

    // plot heaps
    if (!ctx.joiningData()) {
        cout << "    plotting ctx.sh1...\n";
        plotHeap(ctx.sh1, "dump_ctx");
        cout << "    plotting ctx.sh2...\n";
        plotHeap(ctx.sh2, "dump_ctx");
    }
    cout << "    plotting ctx.dst...\n";
    plotHeap(ctx.dst, "dump_ctx");

    // print entry-point
    cout << "\ndump_ctx: ";
    if (ctx.joiningData())
        cout << "joinData()\n";
    else
        cout << "joinSymHeaps()\n";

    // summarize mapping
    cout << "    ctx.objMap1[0]     .size() = " << (ctx.objMap1[0].size() - 1)
        << "\n";
    cout << "    ctx.objMap2[0]     .size() = " << (ctx.objMap2[0].size() - 1)
        << "\n\n";
    cout << "    ctx.valMap1[0]     .size() = " << (ctx.valMap1[0].size() - 1)
        << "\n";
    cout << "    ctx.valMap2[0]     .size() = " << (ctx.valMap2[0].size() - 1)
        << "\n\n";

    // sumarize aux containers
    cout << "    ctx.joinCache      .size() = " << ctx.joinCache.size()
        << "\n";
    cout << "    ctx.protos         .size() = " << ctx.protos.size()
        << "\n\n";

    // print queue stats
    cout << "    ctx.wl          .cntSeen() = " << ctx.wl.cntSeen() << "\n";
    cout << "    ctx.wl          .cntTodo() = " << ctx.wl.cntTodo() << "\n\n";

    // print the current status
    cout << "    ctx.status = " << ctx.status << "\n";
}

/// update ctx.status according to action
bool updateJoinStatus(SymJoinCtx &ctx, const EJoinStatus action)
{
    if (JS_USE_ANY == action)
        return true;

    EJoinStatus &status = ctx.status;
    switch (status) {
        case JS_THREE_WAY:
            break;

        case JS_USE_ANY:
            status = action;
            break;

        default:
            if (action != status)
                status = JS_THREE_WAY;
    }

    return (JS_THREE_WAY != status)
        || ctx.forceThreeWay
        || ctx.allowThreeWay;
}

bool defineObjectMapping(
        SymJoinCtx              &ctx,
        const TObjId            objDst,
        const TObjId            obj1,
        const TObjId            obj2)
{
    const bool hasObj1 = (OBJ_INVALID != obj1);
    const bool hasObj2 = (OBJ_INVALID != obj2);
    CL_BREAK_IF(!hasObj1 && !hasObj2);

    const bool ok1 = !hasObj1 || mapBidir(ctx.objMap1, obj1, objDst);
    const bool ok2 = !hasObj2 || mapBidir(ctx.objMap2, obj2, objDst);

    if (ok1 && ok2)
        return true;

    SJ_DEBUG("<-- object mapping mismatch " << SJ_OBJP(obj1, obj2));
    return false;
}

/// preserve shared Neq predicates
void preserveSharedNeqs(
        SymJoinCtx             &ctx,
        const TValId            vDst,
        const TValId            v1,
        const TValId            v2)
{
    // look for shared Neq predicates
    TValList rVals1;
    ctx.sh1.gatherRelatedValues(rVals1, v1);
    BOOST_FOREACH(const TValId rel1, rVals1) {
        if (!ctx.sh1.chkNeq(v1, rel1))
            // not a Neq in sh1
            continue;

        const TValMap &vMap1 = ctx.valMap1[DIR_LTR];
        const TValMap::const_iterator it1 = vMap1.find(rel1);
        if (vMap1.end() == it1)
            // related value has not (yet?) any mapping to dst
            continue;

        const TValId relDst = it1->second;
        const TValMap &vMap2r = ctx.valMap2[DIR_RTL];
        const TValMap::const_iterator it2r = vMap2r.find(relDst);
        if (vMap2r.end() == it2r)
            // related value has not (yet?) any mapping back to sh2
            continue;

        const TValId rel2 = it2r->second;
        if (!ctx.sh2.chkNeq(v2, rel2))
            // not a Neq in sh2
            continue;

        // establish the Neq in ctx.dst
        ctx.dst.addNeq(vDst, relDst);
    }
}

/// define value mapping for the given value triple (v1, v2, vDst)
bool defineValueMapping(
        SymJoinCtx             &ctx,
        const TValId            vDst,
        const TValId            v1,
        const TValId            v2)
{
    const bool hasValue1 = (VAL_INVALID != v1);
    const bool hasValue2 = (VAL_INVALID != v2);
    CL_BREAK_IF(!hasValue1 && !hasValue2);

    const bool ok1 = !hasValue1 || mapBidir(ctx.valMap1, v1, vDst);
    const bool ok2 = !hasValue2 || mapBidir(ctx.valMap2, v2, vDst);
    if (ok1 && ok2)
        return true;

    SJ_DEBUG("<-- value mapping mismatch " << SJ_VALP(v1, v2));
    return false;
}

bool writeJoinedValue(
        SymJoinCtx             &ctx,
        const FldHandle        &dst,
        const TValId            vDst,
        const TValId            v1,
        const TValId            v2)
{
    CL_BREAK_IF(!dst.isValidHandle());
    CL_BREAK_IF(VAL_INVALID == vDst);
    CL_BREAK_IF(VAL_INVALID == v1 && VAL_INVALID == v2);

    if (VAL_INVALID != v1 && VAL_INVALID != v2) {
        // update join cache
        const TValPair vp(v1, v2);
        CL_BREAK_IF(hasKey(ctx.joinCache, vp) && vDst != ctx.joinCache[vp]);
        ctx.joinCache[vp] = vDst;

        // collect shared Neq relations
        preserveSharedNeqs(ctx, vDst, v1, v2);
    }

    // write the value
    dst.setValue(vDst);

    // the current implementation cannot fail (reserved for future extensions)
    return true;
}

bool joinCacheLookup(
        TValId                 *pDst,
        SymJoinCtx             &ctx,
        const TValId            v1,
        const TValId            v2)
{
    const TValPair vp(v1, v2);
    const TJoinCache::const_iterator it = ctx.joinCache.find(vp);
    if (ctx.joinCache.end() == it)
        return false;

    *pDst = it->second;
    return true;
}

bool joinTargetSpec(
        ETargetSpecifier       *pDst,
        SymJoinCtx             &ctx,
        const TValId            v1,
        const TValId            v2)
{
    const ETargetSpecifier ts1 = ctx.sh1.targetSpec(v1);
    const ETargetSpecifier ts2 = ctx.sh2.targetSpec(v2);

    ETargetSpecifier tsDst = TS_INVALID;
    if (ts1 == ts2)
        tsDst = ts1 /* = ts2 */;
    else if (TS_INVALID == ts1)
        tsDst = ts2;
    else if (TS_INVALID == ts2)
        tsDst = ts1;
    else if (TS_REGION == ts1)
        tsDst = ts2;
    else if (TS_REGION == ts2)
        tsDst = ts1;

    *pDst = tsDst;
    return (TS_INVALID != tsDst);
}

bool joinRangeValues(
        SymJoinCtx             &ctx,
        const SchedItem        &item)
{
    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();

    ETargetSpecifier ts;
    if (!joinTargetSpec(&ts, ctx, v1, v2))
        // target specifier mismatch
        return false;

    const IR::Range rng1 = ctx.sh1.valOffsetRange(v1);
    const IR::Range rng2 = ctx.sh2.valOffsetRange(v2);

    // compute the join of ranges
    IR::Range rng = join(rng1, rng2);

    // [experimental] widening on offset ranges
    if (!isSingular(rng1) && !isSingular(rng2)) {
#if (SE_ALLOW_OFF_RANGES & 0x2)
        if (rng.lo == rng1.lo || rng.lo == rng2.lo)
            rng.hi = IR::IntMax;
#endif
#if (SE_ALLOW_OFF_RANGES & 0x4)
        if (rng.hi == rng1.hi || rng.hi == rng2.hi)
            rng.lo = IR::IntMin;
#endif
    }

    if (!isCovered(rng, rng1) && !updateJoinStatus(ctx, JS_USE_SH2))
        return false;
    if (!isCovered(rng, rng2) && !updateJoinStatus(ctx, JS_USE_SH1))
        return false;

    // resolve root in ctx.dst
    const TObjId objDst = roMapLookup(ctx.objMap1[0], ctx.sh1.objByAddr(v1));
    CL_BREAK_IF(objDst != roMapLookup(ctx.objMap2[0], ctx.sh2.objByAddr(v2)));
    const TValId rootDst = ctx.dst.addrOfTarget(objDst, ts);

    // create a VT_RANGE value in ctx.dst
    const TValId vDst = ctx.dst.valByRange(rootDst, rng);

    // create the corresponding has-value edge
    return writeJoinedValue(ctx, item.fldDst, vDst, v1, v2);
}

bool checkObjectMapping(
        const SymJoinCtx       &ctx,
        const TObjId            obj1,
        const TObjId            obj2,
        const bool              allowUnknownMapping,
        TObjId                 *pDst = 0)
{
    if (!checkNonPosValues(obj1, obj2))
        return false;

    // read-only object lookup
    const TObjMap &oMap1 = ctx.objMap1[DIR_LTR];
    const TObjMap &oMap2 = ctx.objMap2[DIR_LTR];
    TObjMap::const_iterator i1 = oMap1.find(obj1);
    TObjMap::const_iterator i2 = oMap2.find(obj2);

    const bool hasMapping1 = (oMap1.end() != i1);
    const bool hasMapping2 = (oMap2.end() != i2);
    if (!hasMapping1 && !hasMapping2)
        // we have not enough info yet
        return allowUnknownMapping;

    const TObjId objDst1 = (hasMapping1) ? i1->second : OBJ_INVALID;
    const TObjId objDst2 = (hasMapping2) ? i2->second : OBJ_INVALID;

    if (hasMapping1 && hasMapping2 && (objDst1 == objDst2)) {
        // mapping already known and known to be consistent
        if (pDst)
            *pDst = objDst1 /* = objDst2 */;

        return true;
    }

    if (allowUnknownMapping) {
        SJ_DEBUG("<-- object mapping mismatch: " << SJ_OBJP(obj1, obj2)
                 " -> " << SJ_OBJP(objDst1, objDst2));
    }

    return false;
}

/// read-only (in)consistency check among value pair (v1, v2)
bool checkValueMapping(
        const SymJoinCtx       &ctx,
        const TValId            v1,
        const TValId            v2,
        const bool              allowUnknownMapping,
        TValId                 *pDst = 0)
{
    if (!checkNonPosValues(v1, v2))
        return false;

    const EValueTarget code1 = ctx.sh1.valTarget(v1);
    const EValueTarget code2 = ctx.sh2.valTarget(v2);
    if (VT_OBJECT == code1 && VT_OBJECT == code2
            && !matchOffsets(ctx.sh1, ctx.sh2, v1, v2))
        return false;

    const TObjId obj1 = ctx.sh1.objByAddr(v1);
    const TObjId obj2 = ctx.sh2.objByAddr(v2);
    if (!checkObjectMapping(ctx, obj1, obj2, allowUnknownMapping))
        return false;

    // read-only value lookup
    const TValMap &vMap1 = ctx.valMap1[DIR_LTR];
    const TValMap &vMap2 = ctx.valMap2[DIR_LTR];
    TValMap::const_iterator i1 = vMap1.find(v1);
    TValMap::const_iterator i2 = vMap2.find(v2);

    const bool hasMapping1 = (vMap1.end() != i1);
    const bool hasMapping2 = (vMap2.end() != i2);
    if (!hasMapping1 || !hasMapping2)
        // we have not enough info yet
        return allowUnknownMapping;

    const TValId vDst1 = (hasMapping1) ? i1->second : VAL_INVALID;
    const TValId vDst2 = (hasMapping2) ? i2->second : VAL_INVALID;

    if (hasMapping1 && hasMapping2 && (vDst1 == vDst2)) {
        // mapping already known and known to be consistent
        if (pDst) {
            const IR::Range off = ctx.sh1.valOffsetRange(v1);
            CL_BREAK_IF(off != ctx.sh2.valOffsetRange(v2));
            *pDst = ctx.dst.valByRange(vDst1 /* == vDst2 */, off);
        }
        return true;
    }

    if (allowUnknownMapping) {
        SJ_DEBUG("<-- value mapping mismatch: " << SJ_VALP(v1, v2)
                 " -> " << SJ_VALP(vDst1, vDst2));
    }

    return false;
}

bool handleUnknownValues(
        SymJoinCtx             &ctx,
        const SchedItem        &item,
        const TValId            vDst)
{
    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();

    const bool isNull1 = (VAL_NULL == v1);
    const bool isNull2 = (VAL_NULL == v2);
    if (isNull1 != isNull2) {
        const TValId valGt = (isNull2) ? v1 : v2;
        TValMapBidir &vMap = (isNull2) ? ctx.valMap1 : ctx.valMap2;
        if (!mapBidir(vMap, valGt, vDst))
            return false;
    }
    else {
        if (!defineValueMapping(ctx, vDst, v1, v2))
            return false;
    }

    return writeJoinedValue(ctx, item.fldDst, vDst, v1, v2);
}

bool joinCustomValues(
        SymJoinCtx              &ctx,
        const SchedItem         &item)
{
    SymHeap &sh1 = ctx.sh1;
    SymHeap &sh2 = ctx.sh2;

    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();

    const CustomValue cVal1 = sh1.valUnwrapCustom(v1);
    const CustomValue cVal2 = sh2.valUnwrapCustom(v2);
    if (cVal1 == cVal2) {
        // full match
        const TValId vDst = ctx.dst.valWrapCustom(cVal1);
        if (!defineValueMapping(ctx, vDst, v1, v2))
            return false;

        return writeJoinedValue(ctx, item.fldDst, vDst, v1, v2);
    }

    IR::Range rng1, rng2;
    if (!rngFromVal(&rng1, sh1, v1) || !rngFromVal(&rng2, sh2, v2)) {
        // throw custom values away and abstract them by a fresh unknown value
        SJ_DEBUG("throwing away unmatched custom values " << SJ_VALP(v1, v2));
        if (!updateJoinStatus(ctx, JS_THREE_WAY))
            return false;

        const TValId vDst = ctx.dst.valCreate(VT_UNKNOWN, VO_UNKNOWN);
        if (!defineValueMapping(ctx, vDst, v1, v2))
            return false;

        return writeJoinedValue(ctx, item.fldDst, vDst, v1, v2);
    }

    // compute the resulting range that covers both
    IR::Range rng = join(rng1, rng2);

#if SE_INT_ARITHMETIC_LIMIT
    const IR::TInt max = std::max(std::abs(rng.lo), std::abs(rng.hi));
    if (max <= (SE_INT_ARITHMETIC_LIMIT)) {
        SJ_DEBUG("<-- integral values preserved by SE_INT_ARITHMETIC_LIMIT "
                << SJ_VALP(v1, v2));

        return false;
    }
#endif

#if !(SE_ALLOW_INT_RANGES & 0x1)
    // avoid creation of a CV_INT_RANGE value from two CV_INT values
    if (isSingular(rng1) && isSingular(rng2)) {
        // force three-way join in order not to loop forever!
        ctx.forceThreeWay = true;
        if (!updateJoinStatus(ctx, JS_THREE_WAY))
            return false;

        const TValId vDst = ctx.dst.valCreate(VT_UNKNOWN, VO_UNKNOWN);
        if (!defineValueMapping(ctx, vDst, v1, v2))
            return false;

        return writeJoinedValue(ctx, item.fldDst, vDst, v1, v2);
    }
#endif

    // [experimental] widening on intervals
    if (!isSingular(rng1) && !isSingular(rng2)) {
#if (SE_ALLOW_INT_RANGES & 0x2)
        if (rng.lo == rng1.lo || rng.lo == rng2.lo)
            rng.hi = IR::IntMax;
#endif
#if (SE_ALLOW_INT_RANGES & 0x4)
        if (rng.hi == rng1.hi || rng.hi == rng2.hi)
            rng.lo = IR::IntMin;
#endif
    }

    if (!isCovered(rng, rng1) && !updateJoinStatus(ctx, JS_USE_SH2))
        return false;

    if (!isCovered(rng, rng2) && !updateJoinStatus(ctx, JS_USE_SH1))
        return false;

    const CustomValue cv(rng);
    const TValId vDst = ctx.dst.valWrapCustom(cv);
    if (!defineValueMapping(ctx, vDst, v1, v2))
        return false;

    return writeJoinedValue(ctx, item.fldDst, vDst, v1, v2);
}

EValueOrigin joinOrigin(const EValueOrigin vo1, const EValueOrigin vo2)
{
    if (vo1 == vo2)
        // use any
        return vo2;

    if (VO_DEREF_FAILED == vo1 || VO_DEREF_FAILED == vo2)
        // keep the error recovery as cheap as possible
        return VO_DEREF_FAILED;

    // FIXME: we should prioritize isUninitialized(...) to be sound in this
    return VO_UNKNOWN;
}

bool joinValuesByCode(
        bool                   *pResult,
        SymJoinCtx             &ctx,
        const SchedItem        &item)
{
    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();

    const TObjId obj1 = ctx.sh1.objByAddr(v1);
    const TObjId obj2 = ctx.sh2.objByAddr(v2);

    // check target's validity
    if (VAL_NULL != v1 && VAL_NULL != v2) {
        const bool haveTarget1 = ctx.sh1.isValid(obj1);
        const bool haveTarget2 = ctx.sh2.isValid(obj2);
        if (haveTarget1 != haveTarget2) {
            SJ_DEBUG("<-- target validity mismatch " << SJ_VALP(v1, v2));
            *pResult = false;
            return true;
        }
    }

    const EValueTarget code1 = ctx.sh1.valTarget(v1);
    const EValueTarget code2 = ctx.sh2.valTarget(v2);
    if (VT_RANGE == code1 || VT_RANGE == code2)
        // these have to be handled in followValuePair()
        return false;

    // check for VT_UNKNOWN
    const bool isUnknown1 = (VT_UNKNOWN == code1);
    const bool isUnknown2 = (VT_UNKNOWN == code2);

    if (!isUnknown1 && !isUnknown2) {
        // handle VT_CUSTOM values
        const bool isCustom1 = (VT_CUSTOM == code1);
        const bool isCustom2 = (VT_CUSTOM == code2);
        *pResult = isCustom1 && isCustom2 && joinCustomValues(ctx, item);
        return     isCustom1 || isCustom2;
    }

    // join the origin
    const EValueOrigin vo1 = ctx.sh1.valOrigin(v1);
    const EValueOrigin vo2 = ctx.sh2.valOrigin(v2);
    const EValueOrigin origin = joinOrigin(vo1, vo2);

    // do not join VT_UNKNOWN with a valid pointer
    if (VO_DEREF_FAILED != origin) {
        const bool haveTarget1 = isPossibleToDeref(ctx.sh1, v1);
        const bool haveTarget2 = isPossibleToDeref(ctx.sh2, v2);
        if (haveTarget1 || haveTarget2) {
            *pResult = false;
            return true;
        }
    }

    // create a new unknown value in ctx.dst
    const TValId vDst = ctx.dst.valCreate(VT_UNKNOWN, origin);
    *pResult = handleUnknownValues(ctx, item, vDst);
    if (!*pResult)
        // we have failed
        return true;

    item.fldDst.setValue(vDst);

    // we have to use the heap where the unknown value occurs
    if (!isUnknown2)
        return updateJoinStatus(ctx, JS_USE_SH1);
    else if (!isUnknown1)
        return updateJoinStatus(ctx, JS_USE_SH2);
    else
        // use any
        return true;
}


bool bumpNestingLevel(const FldHandle &fld)
{
    if (!fld.isValidHandle())
        return false;

    // resolve root (the owning object of this field)
    SymHeap &sh = *static_cast<SymHeap *>(fld.sh());
    const TObjId obj = fld.obj();

    if (OK_REGION == sh.objKind(obj))
        // do not bump nesting level on concrete objects
        return false;

    TFldSet ignoreList;
    buildIgnoreList(ignoreList, sh, obj);
    return !hasKey(ignoreList, fld);
}

/// (FLD_INVALID == fldDst) means read-only!!!
bool joinFreshItem(
        SymJoinCtx             &ctx,
        const SchedItem        &item,
        const bool              readOnly = false)
{
    const FldHandle &fld1 = item.fld1;
    const FldHandle &fld2 = item.fld2;

    const bool segClone = (!fld1.isValidHandle() || !fld2.isValidHandle());
    CL_BREAK_IF(segClone && readOnly);
    CL_BREAK_IF(!fld1.isValidHandle() && !fld2.isValidHandle());

    const TValId v1 = fld1.value();
    const TValId v2 = fld2.value();

    TValId vDst;
    if (joinCacheLookup(&vDst, ctx, v1, v2)) {
        if (!readOnly)
            item.fldDst.setValue(vDst);
        return true;
    }

    if (VAL_NULL == v1 || VAL_NULL == v2) {
        if (segClone) {
            // we got only one value and the value is VAL_NULL
            item.fldDst.setValue(VAL_NULL);
            return true;
        }

        if (readOnly)
            // act optimistically for now
            return true;
    }

    if (readOnly)
        return checkValueMapping(ctx, v1, v2, /* allowUnknownMapping */ true);

    if (segClone) {
        const bool isGt1 = !fld2.isValidHandle();
        const TObjMap &om = (isGt1) ? ctx.objMap1[0] : ctx.objMap2[0];
        const SymHeap &shGt = (isGt1) ? ctx.sh1 : ctx.sh2;
        const TValId valGt = (isGt1) ? v1 : v2;
        const EValueTarget code = shGt.valTarget(valGt);
        if (VT_RANGE != code) {
            const TObjId objGt = shGt.objByAddr(valGt);
            const TObjMap::const_iterator it = om.find(objGt);
            if (om.end() != it) {
                const TObjId objDst = it->second;
                const TOffset off = shGt.valOffset(valGt);
                const ETargetSpecifier ts = shGt.targetSpec(valGt);
                const TValId valDst = ctx.dst.addrOfTarget(objDst, ts, off);
                item.fldDst.setValue(valDst);
                return true;
            }
        }

#if SE_ALLOW_THREE_WAY_JOIN < 3
        if (!ctx.joiningData())
            return false;
#endif
    }
    else {
        // special values have to match (NULL not treated as special here)
        if (v1 < 0 || v2 < 0) {
            if (v1 == v2) {
                if (!readOnly)
                    item.fldDst.setValue(v1);
                return true;
            }

            SJ_DEBUG("<-- special value mismatch " << SJ_VALP(v1, v2));
            return false;
        }

        if (checkValueMapping(ctx, v1, v2, /* allowUnknown */ false, &vDst))
            // already joined
            return writeJoinedValue(ctx, item.fldDst, vDst, v1, v2);

        bool result;
        if (joinValuesByCode(&result, ctx, item))
            return result;
    }

    if (ctx.wl.schedule(item))
        SJ_DEBUG("+++ " << SJ_VALP(v1, v2)
                << " <- " << SJ_FLDT(item.fldDst, fld1, fld2)
                << ", ldiff = " << item.ldiff);

    return true;
}

struct ObjJoinVisitor {
    SymJoinCtx              &ctx;
    const TProtoLevel       ldiffOrig;
    TFldSet                 blackList1;
    TFldSet                 blackList2;
    bool                    noFollow;

    ObjJoinVisitor(SymJoinCtx &ctx_, const TProtoLevel ldiff_):
        ctx(ctx_),
        ldiffOrig(ldiff_),
        noFollow(false)
    {
    }

    bool operator()(const FldHandle item[3]) {
        const FldHandle &fld1   = item[0];
        const FldHandle &fld2   = item[1];
        const FldHandle &fldDst = item[2];

        // check black-list
        if (hasKey(blackList1, fld1) || hasKey(blackList2, fld2))
            return /* continue */ true;

        int ldiff = ldiffOrig;
        if (bumpNestingLevel(fld1))
            ++ldiff;
        if (bumpNestingLevel(fld2))
            --ldiff;

        const SchedItem sItem(fldDst, fld1, fld2, ldiff);
        return joinFreshItem(ctx, sItem);
    }
};

struct ObjMatchVisitor {
    SymJoinCtx              &ctx;

    public:
        ObjMatchVisitor(SymJoinCtx &ctx_):
            ctx(ctx_)
        {
        }

        bool operator()(const FldHandle item[2]) {
            const FldHandle &fld1 = item[0];
            const FldHandle &fld2 = item[1];

            const FldHandle fldInvalid;
            const SchedItem sItem(fldInvalid, fld1, fld2, /* ldiff */ 0);
            return joinFreshItem(ctx, sItem, /* readOnly */ true);
        }
};

bool joinFields(
        SymJoinCtx             &ctx,
        const TObjId            objDst,
        const TObjId            obj1,
        const TObjId            obj2,
        const TProtoLevel       ldiff,
        const BindingOff       *offBlackList = 0)
{
    if (!defineObjectMapping(ctx, objDst, obj1, obj2))
        return false;

    // initialize visitor
    ObjJoinVisitor objVisitor(ctx, ldiff);

    if (offBlackList) {
        buildIgnoreList(objVisitor.blackList1, ctx.sh1, obj1, *offBlackList);
        buildIgnoreList(objVisitor.blackList2, ctx.sh2, obj2, *offBlackList);
    }
    else if (ctx.joiningData()) {
        if (obj1 == obj2)
            // do not follow shared data
            return true;
        else
            ctx.protos.insert(objDst);
    }

    const TObjId objs[] = {
        obj1,
        obj2,
        objDst
    };

    SymHeap *const heaps[] = {
        &ctx.sh1,
        &ctx.sh2,
        &ctx.dst
    };

    // guide the visitors through them
    return traverseLiveFieldsGeneric<3>(heaps, objs, objVisitor);
}

bool joinObjType(
        TObjType               *pDst,
        SymJoinCtx             &ctx,
        const TObjId            obj1,
        const TObjId            obj2)
{
    if (OBJ_INVALID == obj2) {
        *pDst = ctx.sh1.objEstimatedType(obj1);
        return true;
    }

    if (OBJ_INVALID == obj1) {
        *pDst = ctx.sh2.objEstimatedType(obj2);
        return true;
    }

    const TObjType clt1 = ctx.sh1.objEstimatedType(obj1);
    const TObjType clt2 = ctx.sh2.objEstimatedType(obj2);

    const bool typeFree1 = !clt1;
    const bool typeFree2 = !clt2;
    if (typeFree1 && typeFree2) {
        // full match
        *pDst = 0;
        return true;
    }

    if (typeFree1 || typeFree2) {
        *pDst = 0;

        if (typeFree1)
            return updateJoinStatus(ctx, JS_USE_SH1);

        if (typeFree2)
            return updateJoinStatus(ctx, JS_USE_SH2);

        CL_BREAK_IF("joinClt() malfunction");
    }

    if (*clt1 == *clt2) {
        // full match
        *pDst = clt1;
        return true;
    }

    // the type-info will be abstracted out
    *pDst = 0;
    return updateJoinStatus(ctx, JS_THREE_WAY);
}

bool joinObjKind(
        EObjKind               *pDst,
        SymJoinCtx             &ctx,
        const TObjId            obj1,
        const TObjId            obj2)
{
    CL_BREAK_IF(OBJ_INVALID == obj1 && OBJ_INVALID == obj2);

    const EObjKind kind1 = ctx.sh1.objKind(obj1);
    const EObjKind kind2 = ctx.sh2.objKind(obj2);
    if (kind1 == kind2) {
        // pick any
        *pDst = kind1 /* = kind2 */;
        return true;
    }

    const EObjKind prioTable[] = {
        OK_REGION,
        OK_OBJ_OR_NULL,
        OK_SEE_THROUGH,
        OK_SEE_THROUGH_2N
    };

    for (unsigned i = 0; i < (sizeof prioTable) / (sizeof *prioTable); ++i) {
        const EObjKind dominated = prioTable[i];

        if (dominated == kind1) {
            *pDst = kind2;
            return updateJoinStatus(ctx, JS_USE_SH2);
        }

        if (dominated == kind2) {
            *pDst = kind1;
            return updateJoinStatus(ctx, JS_USE_SH1);
        }
    }
            
    SJ_DEBUG("<-- object kind mismatch " << SJ_OBJP(obj1, obj2));
    return false;
}

bool matchBindingFieldsByValue(
        SymHeap                 &sh,
        const TObjId             obj,
        const BindingOff        &off1,
        const BindingOff        &off2)
{
        const TValId valNextBy1 = valOfPtr(sh, obj, off1.next);
        const TValId valNextBy2 = valOfPtr(sh, obj, off2.next);
        if (valNextBy1 != valNextBy2)
            return false;

        const TValId valPrevBy1 = valOfPtr(sh, obj, off1.prev);
        const TValId valPrevBy2 = valOfPtr(sh, obj, off2.prev);
        return (valPrevBy1 == valPrevBy2);
}

bool joinSegBindingOfMayExist(
        bool                   *pResult,
        BindingOff             *pOff,
        const SymJoinCtx       &ctx,
        const TObjId            obj1,
        const TObjId            obj2)
{
    const EObjKind kind1 = ctx.sh1.objKind(obj1);
    const EObjKind kind2 = ctx.sh2.objKind(obj2);

    const bool isMayExist1 = isMayExistObj(kind1);
    const bool isMayExist2 = isMayExistObj(kind2);
    if (!isMayExist1 && !isMayExist2)
        // no OK_SEE_THROUGH involved
        return false;

    const BindingOff off1 = ctx.sh1.segBinding(obj1);
    const BindingOff off2 = ctx.sh2.segBinding(obj2);
    *pOff = (isMayExist2) ? off1 : off2;

    if (off1 == off2) {
        // the 'next' offset matches trivially
        *pResult = true;
        return true;
    }

    if (OK_OBJ_OR_NULL == kind2) {
        *pResult = (VAL_NULL == valOfPtr(ctx.sh1, obj1, off1.next));
        return true;
    }

    if (OK_OBJ_OR_NULL == kind1) {
        *pResult = (VAL_NULL == valOfPtr(ctx.sh2, obj2, off2.next));
        return true;
    }

    // NOTE: test-0504 utilizes this code path

    if (isMayExist1 && matchBindingFieldsByValue(ctx.sh1, obj1, off1, off2))
            goto match;

    if (isMayExist2 && matchBindingFieldsByValue(ctx.sh2, obj2, off1, off2))
            goto match;

    // giving up
    *pResult = false;
    return true;

match:
    SJ_DEBUG("non-trivial match of 'next' offset of OK_SEE_THROUGH");
    *pResult = true;
    return true;
}

bool joinSegBinding(
        BindingOff             *pOff,
        const SymJoinCtx       &ctx,
        const TObjId            obj1,
        const TObjId            obj2)
{
    const EObjKind kind1 = ctx.sh1.objKind(obj1);
    const EObjKind kind2 = ctx.sh2.objKind(obj2);

    const bool isSeg1 = isObjWithBinding(kind1);
    const bool isSeg2 = isObjWithBinding(kind2);
    if (!isSeg1 && !isSeg2)
        // nothing to join here
        return true;

    if (isSeg1 && isSeg2) {
        bool result;
        if (!joinSegBindingOfMayExist(&result, pOff, ctx, obj1, obj2)) {
            // just compare the binding offsets
            const BindingOff off1 = ctx.sh1.segBinding(obj1);
            const BindingOff off2 = ctx.sh2.segBinding(obj2);
            if ((result = (off1 == off2)))
                *pOff = off1;
        }

        if (result)
            return true;

        SJ_DEBUG("<-- segment binding mismatch " << SJ_OBJP(obj1, obj2));
        return false;
    }

    if (isSeg1) {
        *pOff = ctx.sh1.segBinding(obj1);
        return true;
    }

    if (isSeg2) {
        *pOff = ctx.sh2.segBinding(obj2);
        return true;
    }

    // not reachable
    CL_TRAP;
    return false;
}

bool joinNestingLevel(
        TProtoLevel            *pDst,
        const SymJoinCtx       &ctx,
        const TObjId            obj1,
        const TObjId            obj2,
        const TProtoLevel       ldiffExpected)
{
    const TProtoLevel level1 = ctx.l1Drift + ctx.sh1.objProtoLevel(obj1);
    const TProtoLevel level2 = ctx.l2Drift + ctx.sh2.objProtoLevel(obj2);

    *pDst = std::max(level1, level2);

    if (ctx.joiningData() && obj1 == obj2)
        // shared data
        return true;

    if (OBJ_INVALID == obj1 || OBJ_INVALID == obj2)
        // we got only one object, just take its level as it is
        return true;

    // check that the computed ldiff matches the actual one
    const TProtoLevel ldiffComputed = level1 - level2;
    return (ldiffComputed == ldiffExpected);
}

bool joinMinLength(
        TMinLen                *pDst,
        SymJoinCtx             &ctx,
        const TObjId            obj1,
        const TObjId            obj2,
        const EObjKind          kind)
{
    if (OBJ_INVALID == obj1 || OBJ_INVALID == obj2) {
        if (objMinLength(ctx.sh1, obj1) || objMinLength(ctx.sh2, obj2))
            // insertion of non-empty object does not cover both variants
            updateJoinStatus(ctx, JS_THREE_WAY);

        *pDst = 0;
        return true;
    }

    const TMinLen len1 = objMinLength(ctx.sh1, obj1);
    const TMinLen len2 = objMinLength(ctx.sh2, obj2);
    *pDst = std::min(len1, len2);

#if SE_PRESERVE_DLS_MINLEN
    if (len1 != len2 && OK_DLS == kind && !ctx.joiningData()) {
        const TMinLen maxLen = std::max(len1, len2);
        if (maxLen < (SE_PRESERVE_DLS_MINLEN))
            return false;
    }
#else
    (void) kind;
#endif

    if (len1 < len2)
        return updateJoinStatus(ctx, JS_USE_SH1);

    if (len2 < len1)
        return updateJoinStatus(ctx, JS_USE_SH2);

    // the lengths are equal, pick any
    CL_BREAK_IF(len1 != len2);
    return true;
}

bool joinObjValidity(
        bool                    *pDst,
        SymJoinCtx              &ctx,
        const TObjId             obj1,
        const TObjId             obj2)
{
    if (OBJ_INVALID == obj1) {
        *pDst = ctx.sh2.isValid(obj2);
        return true;
    }

    if (OBJ_INVALID == obj2) {
        *pDst = ctx.sh1.isValid(obj1);
        return true;
    }

    const bool valid1 = ctx.sh1.isValid(obj1);
    const bool valid2 = ctx.sh2.isValid(obj2);
    if (valid1 != valid2) {
        SJ_DEBUG("<-- object validity mismatch " << SJ_OBJP(obj1, obj2));
        return false;
    }

    *pDst = valid1;
    return true;
}

bool joinObjSize(
        TSizeRange              *pDst,
        SymJoinCtx              &ctx,
        const TObjId             obj1,
        const TObjId             obj2)
{
    if (OBJ_INVALID == obj1) {
        *pDst = ctx.sh2.objSize(obj2);
        return true;
    }

    if (OBJ_INVALID == obj2) {
        *pDst = ctx.sh1.objSize(obj1);
        return true;
    }

    const TSizeRange size1 = ctx.sh1.objSize(obj1);
    const TSizeRange size2 = ctx.sh2.objSize(obj2);
    if (size1 != size2) {
        SJ_DEBUG("<-- object size mismatch " << SJ_OBJP(obj1, obj2));
        return false;
    }

    *pDst = size1;
    return true;
}

void importBlockMap(
        TUniBlockMap            *pMap,
        SymHeap                 &dst,
        const SymHeap           &src)
{
    BOOST_FOREACH(TUniBlockMap::reference item, *pMap) {
        UniformBlock &bl = item.second;
        bl.tplValue = translateValProto(dst, src, bl.tplValue);
    }
}

void joinUniBlocksCore(
        TUniBlockMap            *pMap,
        bool                    *pExtra1,
        bool                    *pExtra2,
        SymJoinCtx              &ctx,
        const TObjId            root1,
        const TObjId            root2)
{
    SymHeap &sh1 = ctx.sh1;
    SymHeap &sh2 = ctx.sh2;

    TUniBlockMap bMap1, bMap2;
    sh1.gatherUniformBlocks(bMap1, root1);
    sh2.gatherUniformBlocks(bMap2, root2);

    BOOST_FOREACH(TUniBlockMap::const_reference item, bMap1) {
        UniformBlock bl2(item.second);
        bl2.tplValue = translateValProto(sh2, sh1, bl2.tplValue);

        TUniBlockMap cov2;
        if (!sh2.findCoveringUniBlocks(&cov2, root2, bl2))
            *pExtra1 = true;

        BOOST_FOREACH(TUniBlockMap::const_reference cItem, cov2) {
            UniformBlock blDst(cItem.second);
            blDst.tplValue = translateValProto(ctx.dst, sh2, blDst.tplValue);
            (*pMap)[blDst.off] = blDst;
        }
    }

    BOOST_FOREACH(TUniBlockMap::const_reference item, bMap2) {
        UniformBlock bl1(item.second);
        bl1.tplValue = translateValProto(sh1, sh2, bl1.tplValue);

        TUniBlockMap cov1;
        if (!sh1.findCoveringUniBlocks(&cov1, root1, bl1))
            *pExtra2 = true;

        BOOST_FOREACH(TUniBlockMap::const_reference cItem, cov1) {
            UniformBlock blDst(cItem.second);
            const TOffset off = blDst.off;
            if (hasKey(*pMap, off)) {
                // we already have a uniform block at this offset
                const UniformBlock &old = (*pMap)[off];
                CL_BREAK_IF(!areValProtosEqual(ctx.dst, sh1,
                            old.tplValue,
                            bl1.tplValue));

                if (bl1.size < old.size)
                    // the current block is smaller than the original one, skip it!
                    continue;
            }

            blDst.tplValue = translateValProto(ctx.dst, sh1, blDst.tplValue);
            (*pMap)[off] = blDst;
        }
    }
}

bool joinUniBlocks(
        SymJoinCtx              &ctx,
        const TObjId            objDst,
        const TObjId            obj1,
        const TObjId            obj2)
{
    TUniBlockMap bMapDst;
    bool hasExtra1 = false;
    bool hasExtra2 = false;

    if (OBJ_INVALID == obj2) {
        hasExtra2 = true;
        ctx.sh1.gatherUniformBlocks(bMapDst, obj1);
        importBlockMap(&bMapDst, ctx.dst, /* src */ ctx.sh1);
    }
    else if (OBJ_INVALID == obj1) {
        hasExtra1 = true;
        ctx.sh2.gatherUniformBlocks(bMapDst, obj2);
        importBlockMap(&bMapDst, ctx.dst, /* src */ ctx.sh2);
    }
    else
        joinUniBlocksCore(&bMapDst, &hasExtra1, &hasExtra2, ctx, obj1, obj2);

    // update join status accordingly
    if (hasExtra1 && !updateJoinStatus(ctx, JS_USE_SH2))
        return false;
    if (hasExtra2 && !updateJoinStatus(ctx, JS_USE_SH1))
        return false;

    BOOST_FOREACH(TUniBlockMap::const_reference item, bMapDst) {
        const UniformBlock &bl = item.second;
        ctx.dst.writeUniformBlock(objDst, bl);
    }

    return true;
}

static const BindingOff ObjOrNull(OK_OBJ_OR_NULL);

/// (NULL == pObjDst) means dry-run, (NULL != off) means 0..1 abstract object
bool createObject(
        TObjId                 *pObjDst,
        SymJoinCtx             &ctx,
        const TObjId            obj1,
        const TObjId            obj2,
        const TProtoLevel       ldiff,
        const BindingOff       *offMayExist = 0)
{
    bool valid;
    if (!joinObjValidity(&valid, ctx, obj1, obj2))
        return false;

    EObjKind kind;
    if (!joinObjKind(&kind, ctx, obj1, obj2))
        return false;

    BindingOff off;
    if (!joinSegBinding(&off, ctx, obj1, obj2))
        return false;

    TProtoLevel protoLevel;
    if (!joinNestingLevel(&protoLevel, ctx, obj1, obj2, ldiff))
        return false;

    TSizeRange size;
    if (!joinObjSize(&size, ctx, obj1, obj2))
        return false;

    TMinLen len;
    if (!joinMinLength(&len, ctx, obj1, obj2, kind))
        return false;

    TObjType clt;
    if (!joinObjType(&clt, ctx, obj1, obj2))
        return false;

    if (!pObjDst)
        // do not create the object
        return true;;

    if (offMayExist) {
        // we are asked to introduce OK_SEE_THROUGH/OK_OBJ_OR_NULL
        if (OK_REGION != kind && !isMayExistObj(kind))
            CL_BREAK_IF("invalid call of createObject()");

        if (ObjOrNull == *offMayExist)
            kind = OK_OBJ_OR_NULL;
        else {
            off = *offMayExist;
            kind = (off.next == off.prev)
                ? OK_SEE_THROUGH
                : OK_SEE_THROUGH_2N;
        }
    }

    // create an image in ctx.dst
    const TObjId objDst = ctx.dst.heapAlloc(size);

    if (!joinUniBlocks(ctx, objDst, obj1, obj2))
        // failed to complement uniform blocks
        return false;

    if (clt)
        // preserve estimated type-info of the root
        ctx.dst.objSetEstimatedType(objDst, clt);

    if (!valid)
        // mark the object as invalid
        ctx.dst.objInvalidate(objDst);

    // preserve 'prototype' flag
    ctx.dst.objSetProtoLevel(objDst, protoLevel);

    if (OK_REGION != kind) {
        // abstract object
        ctx.dst.objSetAbstract(objDst, kind, off);

        // intialize the minimum segment length
        ctx.dst.segSetMinLength(objDst, len);
    }

    if (!joinFields(ctx, objDst, obj1, obj2, ldiff))
        return false;

    *pObjDst = objDst;
    return true;
}

bool objMatchLookAhead(
        SymJoinCtx             &ctx,
        const TObjId            obj1,
        const TObjId            obj2,
        const TProtoLevel       ldiff)
{
    if (checkObjectMapping(ctx, obj1, obj2, /* allowUnknownMapping */ false))
        return true;

    if (!checkObjectMapping(ctx, obj1, obj2, /* allowUnknownMapping */ true))
        return false;

    if (!createObject(/* dry-run */ 0, ctx, obj1, obj2, ldiff))
        // dry-run creation failed
        return false;

    // set up a visitor
    SymHeap *const heaps[] = { &ctx.sh1, &ctx.sh2 };
    TObjId objs[] = { obj1, obj2 };
    ObjMatchVisitor visitor(ctx);

    return traverseLiveFieldsGeneric<2>(heaps, objs, visitor);
}

bool offRangeFallback(
        SymJoinCtx             &ctx,
        const SchedItem        &item);

bool mapTargetAddress(
        SymJoinCtx              &ctx,
        const SchedItem         &item,
        const TObjId             objDst,
        const bool               preserveValMap = false)
{
    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();

    const EValueTarget code1 = ctx.sh1.valTarget(v1);
    const EValueTarget code2 = ctx.sh2.valTarget(v2);

    if (VT_RANGE == code1 || VT_RANGE == code2)
        return joinRangeValues(ctx, item);

    const TOffset off1 = ctx.sh1.valOffset(v1);
    const TOffset off2 = ctx.sh2.valOffset(v2);
    if (off1 != off2)
        return offRangeFallback(ctx, item);

    ETargetSpecifier tsDst;
    if (!joinTargetSpec(&tsDst, ctx, v1, v2))
        return false;

    const TValId vDst = ctx.dst.addrOfTarget(objDst, tsDst, off1 /* = off2 */);
    if (/* XXX */ !preserveValMap && !defineValueMapping(ctx, vDst, v1, v2))
        return false;

    // write the resulting value to item.fldDst
    return writeJoinedValue(ctx, item.fldDst, vDst, v1, v2);
}

bool mapAsymTarget(
        SymJoinCtx             &ctx,
        const FldHandle        &fldDst,
        const TValId            v1,
        const TValId            v2,
        const EJoinStatus       action)
{
    if (!updateJoinStatus(ctx, action))
        return false;

    TObjId objDst;
    TOffset offDst;
    ETargetSpecifier tsDst;

    switch (action) {
        case JS_USE_SH1:
            objDst = roMapLookup(ctx.objMap1[DIR_LTR], ctx.sh1.objByAddr(v1));
            offDst = ctx.sh1.valOffset(v1);
            tsDst = ctx.sh1.targetSpec(v1);
            break;

        case JS_USE_SH2:
            objDst = roMapLookup(ctx.objMap2[DIR_LTR], ctx.sh2.objByAddr(v2));
            offDst = ctx.sh2.valOffset(v2);
            tsDst = ctx.sh2.targetSpec(v2);
            break;

        default:
            CL_BREAK_IF("invalid call of mapAsymTarget()");
            return false;
    }

    // write the resulting value to item.fldDst
    const TValId vDst = ctx.dst.addrOfTarget(objDst, tsDst, offDst);
    return writeJoinedValue(ctx, fldDst, vDst, v1, v2);
}

bool joinObjects(
        bool                   *pResult,
        TObjId                 *pObjDst,
        SymJoinCtx             &ctx,
        const TObjId            obj1,
        const TObjId            obj2,
        const TProtoLevel       ldiff,
        bool                    firstTryReadOnly = true)
{
    if (firstTryReadOnly && !objMatchLookAhead(ctx, obj1, obj2, ldiff))
        return false;

    if (ctx.joiningData() && obj1 == obj2) {
        // we are on the way from joinData() and hit shared data
        *pObjDst = obj1 /* = obj2 */;
        *pResult = defineObjectMapping(ctx, obj1, obj2, obj1 /* = obj2 */);
        return true;
    }

    if (!checkObjectMapping(ctx, obj1, obj2, /* allowUnknownMapping */ true)) {
        *pResult = false;
        return true;
    }

    EObjKind kind;
    if (!joinObjKind(&kind, ctx, obj1, obj2))
        return false;

    // go ahead, try it read-write!
    SJ_DEBUG(">>> joinObjects" << SJ_OBJP(obj1, obj2));
    *pResult = createObject(pObjDst, ctx, obj1, obj2, ldiff);
    if (!*pResult || !isObjWithBinding(kind))
        return true;

    const TObjId objDst = *pObjDst;
    const BindingOff &off = ctx.dst.segBinding(objDst);

    if (OK_DLS == kind) {
        const SchedItem prevItem(
                PtrHandle(ctx.dst, objDst, off.prev),
                PtrHandle(ctx.sh1, obj1  , off.prev),
                PtrHandle(ctx.sh2, obj2  , off.prev),
                ldiff);
        if (ctx.wl.schedule(prevItem))
            SJ_DEBUG("+++ " << SJ_ITEM(prevItem) << " by joinObjects()");
    }

    const SchedItem nextItem(
            PtrHandle(ctx.dst, objDst, off.next),
            PtrHandle(ctx.sh1, obj1  , off.next),
            PtrHandle(ctx.sh2, obj2  , off.next),
            ldiff);
    if (ctx.wl.schedule(nextItem))
        SJ_DEBUG("+++ " << SJ_ITEM(nextItem) << " by joinObjects()");

    return true;
}

/// (NULL != off) means 'introduce OK_{OBJ_OR_NULL,SEE_THROUGH,SEE_THROUGH_2N}'
bool segmentCloneCore(
        TObjId                     *pObjDst,
        SymJoinCtx                 &ctx,
        SymHeap                    &shGt,
        const TValId                valGt,
        const TObjMapBidir         &objMapGt,
        const TProtoLevel           ldiff,
        const EJoinStatus           action,
        const BindingOff           *off)
{
    const TObjMapBidir &objMapLt = (JS_USE_SH2 == action)
        ? ctx.objMap1
        : ctx.objMap2;

    const TObjMap &objMapGtLtr = objMapGt[DIR_LTR];
    const TObjMap &objMapLtRtl = objMapLt[DIR_RTL];

    if (!isAnyDataArea(shGt.valTarget(valGt)))
        // not valid target
        return false;

    const TObjId objGt = shGt.objByAddr(valGt);
    const TObjMap::const_iterator it = objMapGtLtr.find(objGt);
    if (objMapGtLtr.end() != it) {
        const TObjId objDst = it->second;
        if (OK_OBJ_OR_NULL == ctx.dst.objKind(objDst) && (ObjOrNull != *off))
            // FIXME: we need to fix this in a more generic way
            return false;

        if (!hasKey(objMapLtRtl, objDst)) {
            // mapping already available for objGt
            *pObjDst = objDst;
            return true;
        }
    }

    SJ_DEBUG("+i+ insertSegmentClone: cloning object #" << objGt <<
             ", action = " << action);

    const TObjId obj1 = (JS_USE_SH1 == action) ? objGt : OBJ_INVALID;
    const TObjId obj2 = (JS_USE_SH2 == action) ? objGt : OBJ_INVALID;

    if (!updateJoinStatus(ctx, action))
        return false;

    // clone the object
    if (createObject(pObjDst, ctx, obj1, obj2, ldiff, off))
        return true;

    SJ_DEBUG("<-- insertSegmentClone: failed to create object "
             << SJ_OBJP(obj1, obj2));
    return false;
}

void scheduleSegAddr(
        TWorkList              &wl,
        const EJoinStatus       action,
        const SchedItem         item)
{
    CL_BREAK_IF(JS_USE_SH1 != action && JS_USE_SH2 != action);

    const FldHandle fldInvalid;

    const SchedItem segItem(
            item.fldDst,
            (JS_USE_SH1 == action) ? item.fld1 : fldInvalid,
            (JS_USE_SH2 == action) ? item.fld2 : fldInvalid,
            item.ldiff);
    wl.schedule(segItem);
}

bool cloneSpecialValue(
        SymJoinCtx              &ctx,
        SymHeap                 &shGt,
        const TValId            valGt,
        const TValMapBidir      &valMapGt,
        const SchedItem         &itemToClone,
        EValueTarget            code)
{
    const TValId rootGt = shGt.valRoot(valGt);
    EValueOrigin vo = shGt.valOrigin(valGt);
    TValId vDst;

    switch (code) {
        case VT_RANGE:
            if (hasKey(valMapGt[DIR_LTR], rootGt))
                break;

            CL_BREAK_IF("unable to transfer a VT_RANGE value");
            // fall through!

        case VT_CUSTOM:
            code = VT_UNKNOWN;
            vo = VO_UNKNOWN;
            // fall through!

        default:
            vDst = ctx.dst.valCreate(code, vo);
            return handleUnknownValues(ctx, itemToClone, vDst);
    }

    // VT_RANGE
    const TValId rootDst = roMapLookup(valMapGt[DIR_LTR], rootGt);
    const IR::Range range = shGt.valOffsetRange(valGt);
    vDst = ctx.dst.valByRange(rootDst, range);
    return handleUnknownValues(ctx, itemToClone, vDst);
}

/// (NULL != off) means 'introduce OK_{OBJ_OR_NULL,SEE_THROUGH,SEE_THROUGH_2N}'
bool insertSegmentClone(
        bool                    *pResult,
        SymJoinCtx              &ctx,
        const SchedItem         &item,
        const EJoinStatus       action,
        const BindingOff        *off = 0)
{
    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();
    SJ_DEBUG(">>> insertSegmentClone" << SJ_VALP(v1, v2));

    const bool isGt1 = (JS_USE_SH1 == action);
    const bool isGt2 = (JS_USE_SH2 == action);
    CL_BREAK_IF(isGt1 == isGt2);

    // resolve the existing segment in shGt
    SymHeap &shGt = ((isGt1) ? ctx.sh1 : ctx.sh2);
    const TObjId seg = shGt.objByAddr((isGt1) ? v1 : v2);
    const EObjKind kind = shGt.objKind(seg);

    const bool isObjOrNull = (OK_OBJ_OR_NULL == kind)
        || (off && (ObjOrNull == *off));

    // resolve the 'next' pointer and check its validity
    FldHandle nextPtr;
    TValId nextGt;
    if (isObjOrNull)
        nextGt = VAL_NULL;
    else {
        nextPtr = (off)
            ? PtrHandle(shGt, seg, off->next)
            : nextPtrFromSeg(shGt, seg);

        nextGt = nextPtr.value();
    }

    const TValId nextLt = (isGt2) ? v1 : v2;
    if (!off && !checkValueMapping(ctx,
                (isGt1) ? nextGt : nextLt,
                (isGt2) ? nextGt : nextLt,
                /* allowUnknownMapping */ true))
    {
        SJ_DEBUG("<-- insertSegmentClone: value mismatch "
                 "(nextLt = #" << nextLt << ", nextGt = #" << nextGt << ")");
        return false;
    }

#if SE_ALLOW_THREE_WAY_JOIN < 3
    if (!ctx.joiningData() && objMinLength(shGt, seg))
        // on the way from joinSymHeaps(), some three way joins are destructive
        ctx.allowThreeWay = false;
#endif

    const TObjMapBidir &objMapGt = (isGt1)
        ? ctx.objMap1
        : ctx.objMap2;

    const TValMapBidir &valMapGt = (isGt1)
        ? ctx.valMap1
        : ctx.valMap2;

    const bool isMayExist = !!off;

    scheduleSegAddr(ctx.wl, action, item);

    SchedItem cloneItem;
    while (ctx.wl.next(cloneItem)) {
        const TValId v1 = cloneItem.fld1.value();
        const TValId v2 = cloneItem.fld2.value();

        const TValId valGt = (isGt1) ? v1 : v2;
        const TValId valLt = (isGt2) ? v1 : v2;
        if (VAL_INVALID != valLt) {
            // process the rest of ctx.wl rather in joinPendingValues()
            ctx.wl.undo(cloneItem);
            break;
        }

        if (nextGt == valGt)
            // do not go beyond the segment, just follow its data
            continue;

        const TObjId objGt = shGt.objByAddr(valGt);
        if (seg != objGt)
            // OK_SEE_THROUGH/OK_OBJ_OR_NULL is applicable only on the first obj
            off = 0;

        const EValueTarget code = shGt.valTarget(valGt);
        if (isAnyDataArea(code)) {
            if (isMayExist && OK_DLS == shGt.objKind(objGt))
                // FIXME: temporarily not supported
                goto fail;

            TObjId objDst;
            if (segmentCloneCore(&objDst, ctx, shGt, valGt, objMapGt,
                        cloneItem.ldiff, action, off))
            {
                const IR::Range off = shGt.valOffsetRange(valGt);
                const ETargetSpecifier ts = shGt.targetSpec(valGt);
                const TValId vDstBase = ctx.dst.addrOfTarget(objDst, ts);
                const TValId vDst = ctx.dst.valByRange(vDstBase, off);
                if (cloneItem.fldDst.isValidHandle())
                    cloneItem.fldDst.setValue(vDst);
                continue;
            }
        }
        else {
            if (cloneSpecialValue(ctx, shGt, valGt, valMapGt, cloneItem, code))
                continue;
        }

fail:
        // clone failed
        *pResult = false;
        return true;
    }

    *pResult = true;
    if (isObjOrNull)
        // nothing to follow
        return true;

    const TObjId segDst = roMapLookup(objMapGt[DIR_LTR], seg);

    const FldHandle fldNextDst = nextPtrFromSeg(ctx.dst, segDst);

    // schedule the successor fields
    const FldHandle fldNext1 = (isGt1) ? nextPtr : item.fld1;
    const FldHandle fldNext2 = (isGt2) ? nextPtr : item.fld2;
    const SchedItem nextItem(fldNextDst, fldNext1, fldNext2, item.ldiff);
    if (ctx.wl.schedule(nextItem))
        SJ_DEBUG("+++ " << SJ_FLDT(fldNextDst, fldNext1, fldNext2)
                << " by insertSegmentClone()");

    return true;
}

void resolveMayExist(
        SymJoinCtx             &ctx,
        bool                   *isAbs1,
        bool                   *isAbs2,
        const TObjId            obj1,
        const TObjId            obj2)
{
    if (!*isAbs1 || !*isAbs2)
        // at most one abstract object
        return;

    const EObjKind kind1 = ctx.sh1.objKind(obj1);
    const EObjKind kind2 = ctx.sh2.objKind(obj2);
    if (kind1 == kind2)
        // kind of abstract object matches in both cases
        return;

    if (isMayExistObj(kind1))
        *isAbs1 = false;

    if (isMayExistObj(kind2))
        *isAbs2 = false;
}

bool segInsertionFallback(
        bool                    *pResult,
        SymJoinCtx              &ctx,
        const SchedItem         &item)
{
    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();

    const EValueTarget code1 = ctx.sh1.valTarget(v1);
    const EValueTarget code2 = ctx.sh2.valTarget(v2);
    if (VT_RANGE == code1 || VT_RANGE == code2)
        // segInsertionFallback() does not support VT_RANGE values for now
        return false;

    const TObjId obj1 = ctx.sh1.objByAddr(v1);
    const TObjId obj2 = ctx.sh2.objByAddr(v2);

    bool isAbs1 = isAbstractObject(ctx.sh1, obj1);
    bool isAbs2 = isAbstractObject(ctx.sh2, obj2);
    resolveMayExist(ctx, &isAbs1, &isAbs2, obj1, obj2);

    if (isAbs1 && insertSegmentClone(pResult, ctx, item, JS_USE_SH1)) {
        if (*pResult)
            *pResult = mapAsymTarget(ctx, item.fldDst, v1, v2, JS_USE_SH1);

        return true;
    }

    if (isAbs2 && insertSegmentClone(pResult, ctx, item, JS_USE_SH2)) {
        if (*pResult)
            *pResult = mapAsymTarget(ctx, item.fldDst, v1, v2, JS_USE_SH2);

        return true;
    }

    // segInsertionFallback() not applicable
    return false;
}

bool offRangeFallback(
        SymJoinCtx             &ctx,
        const SchedItem        &item)
{
#if !(SE_ALLOW_OFF_RANGES & 0x1)
    return false;
#endif
    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();

    ETargetSpecifier ts;
    if (!joinTargetSpec(&ts, ctx, v1, v2))
        // target specifier mismatch
        return false;

    const TObjId obj1 = ctx.sh1.objByAddr(v1);
    const TObjId obj2 = ctx.sh2.objByAddr(v2);

    const TObjMap &m1 = ctx.objMap1[DIR_LTR];
    const TObjMap &m2 = ctx.objMap2[DIR_LTR];

    const TObjMap::const_iterator it1 = m1.find(obj1);
    const TObjMap::const_iterator it2 = m2.find(obj2);
    if (it1 == m1.end() || it2 == m2.end() || it1->second != it2->second)
        // not really a suitable candidate for offRangeFallback()
        return false;

    // check we got different offsets
    const IR::Range off1 = ctx.sh1.valOffsetRange(v1);
    const IR::Range off2 = ctx.sh2.valOffsetRange(v2);
    CL_BREAK_IF(off1 == off2);

    if (!updateJoinStatus(ctx, /* intentionally! */ JS_THREE_WAY))
        return false;

    // resolve root in ctx.dst
    const TObjId objDst = roMapLookup(ctx.objMap1[0], ctx.sh1.objByAddr(v1));
    CL_BREAK_IF(objDst != roMapLookup(ctx.objMap2[0], ctx.sh2.objByAddr(v2)));
    const TValId rootDst = ctx.dst.addrOfTarget(objDst, ts);

    // compute the resulting range
    IR::Range rng;
    rng.lo = std::min(off1.lo, off2.lo);
    rng.hi = std::max(off1.hi, off2.hi);
    rng.alignment = IR::Int1;

    // create a VT_RANGE value in ctx.dst
    const TValId vDst = ctx.dst.valByRange(rootDst, rng);
    return writeJoinedValue(ctx, item.fldDst, vDst, v1, v2);
}

typedef std::vector<TOffset>                        TOffList;

class MayExistVisitor {
    private:
        SymJoinCtx              ctx_;
        const TProtoLevel       ldiff_;
        const EJoinStatus       action_;
        const FldHandle         fldRef_;
        const TObjId            obj_;
        bool                    lookThrough_;
        TOffList                foundOffsets_;

    public:
        MayExistVisitor(
                SymJoinCtx          &ctx,
                const TProtoLevel   ldiff,
                const EJoinStatus   action,
                const FldHandle     fldRef,
                const TObjId        obj):
            ctx_(ctx),
            ldiff_(ldiff),
            action_(action),
            fldRef_(fldRef),
            obj_(obj),
            lookThrough_(false)
        {
            CL_BREAK_IF(JS_USE_SH1 != action && JS_USE_SH2 != action);
        }

        const TOffList& foundOffsets() const {
            return foundOffsets_;
        }

        bool found() const {
            return !foundOffsets_.empty();
        }

        void enableLookThroughMode(bool enable = true) {
            lookThrough_ = enable;
        }

        bool operator()(const FldHandle &fld) {
            if (!isDataPtr(fld.type()))
                // not a pointer
                return /* continue */ true;

            SymHeap &sh = *static_cast<SymHeap *>(fld.sh());
            TValId val = fld.value();

            for (;;) {
                const TObjId seg = sh.objByAddr(val);
                if (seg == obj_)
                    // refuse referencing the MayExist candidate itself
                    return /* continue */ true;

                const FldHandle fld1 = (JS_USE_SH1 == action_) ? fld : fldRef_;
                const FldHandle fld2 = (JS_USE_SH2 == action_) ? fld : fldRef_;
                const FldHandle fldInvalid;
                const SchedItem item(fldInvalid, fld1, fld2, ldiff_);
                if (checkValueMapping(ctx_,
                            fld1.value(),
                            fld2.value(),
                            /* allowUnknownMapping */ true))
                    // looks like we have a candidate
                    break;

                if (!lookThrough_ || !isAbstractObject(sh, seg))
                    return /* continue */ true;

                const TOffset off = sh.valOffset(val);
                if (sh.segMinLength(seg) || off != headOffset(sh, seg))
                    return /* continue */ true;

                val = nextValFromSegAddr(sh, val);
            }

            foundOffsets_.push_back(fld.offset());
            return /* continue */ true;
        }
};

bool mayExistDigOffsets(
        BindingOff              *pOff,
        SymHeap                 &sh,
        const TValId             valBy,
        TOffList                 offList)
{
    if (offList.empty())
        // no match
        return false;

    const TObjId obj = sh.objByAddr(valBy);

    typedef std::map<TValId, TOffList>              TOffsByVal;
    TOffsByVal offsByVal;
    BOOST_FOREACH(const TOffset off, offList) {
        const TValId val = valOfPtr(sh, obj, off);
        offsByVal[val].push_back(off);
    }

    int maxCnt = 0;
    TValId maxVal = VAL_INVALID;

    BOOST_FOREACH(TOffsByVal::const_reference item, offsByVal) {
        const int cnt = item.second.size();
        if (cnt < maxCnt)
            continue;

        maxCnt = cnt;
        maxVal = /* val */ item.first;
    }

    // pick the list of offsets with the best sharing
    CL_BREAK_IF(VAL_INVALID == maxVal);
    offList = offsByVal[maxVal];

    switch (maxCnt) {
        case 1:
            // introduce OK_SEE_THROUGH
            pOff->next = pOff->prev = offList.front();
            break;

        case 2:
            // introduce OK_SEE_THROUGH_2N
            pOff->next = offList[0];
            pOff->prev = offList[1];
            break;

        default:
            if (debuggingSymJoin)
                CL_BREAK_IF("please implement");
            return false;
    }

    pOff->head = sh.valOffset(valBy);
    return true;
}

bool mayExistFallback(
        bool                    *pResult,
        SymJoinCtx              &ctx,
        const SchedItem         &item,
        const EJoinStatus       action)
{
    const bool use1 = (JS_USE_SH1 == action);
    const bool use2 = (JS_USE_SH2 == action);
    CL_BREAK_IF(use1 == use2);

    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();

    const TObjId obj1 = ctx.sh1.objByAddr(v1);
    const TObjId obj2 = ctx.sh2.objByAddr(v2);

    const bool hasMapping1 = hasKey(ctx.objMap1[DIR_LTR], obj1);
    const bool hasMapping2 = hasKey(ctx.objMap2[DIR_LTR], obj2);
    if ((hasMapping1 != hasMapping2) && (hasMapping1 == use1))
        // try it the other way around
        return false;

    SymHeap &sh = (use1) ? ctx.sh1 : ctx.sh2;
    const TValId val = (use1) ? v1 : v2;
    if (!isPossibleToDeref(sh, val))
        // no valid target
        return false;

    const TObjId obj = (use1) ? obj1 : obj2;
    if (OK_REGION != sh.objKind(obj))
        // only concrete objects/prototypes are candidates for OK_SEE_THROUGH
        return false;

    BindingOff off;

    const FldHandle ref = (use2) ? item.fld1 : item.fld2;
    if (VAL_NULL == ref.value()) {
        // introduce OK_OBJ_OR_NULL
        off = ObjOrNull;
    }
    else {
        // look for next pointer(s) of OK_SEE_THROUGH/OK_SEE_THROUGH_2N
        MayExistVisitor visitor(ctx, item.ldiff, action, ref, obj);
        traverseLivePtrs(sh, obj, visitor);
        if (!visitor.found()) {
            // reference value not matched directly, try to look through in
            // order to allow insert chains of possibly empty abstract objects
            visitor.enableLookThroughMode();
            traverseLivePtrs(sh, obj, visitor);
            if (visitor.found())
                // e.g. test-0124 and test-167 use this code path
                SJ_DEBUG("MayExistVisitor::enableLookThroughMode() in use!");
        }

        if (!mayExistDigOffsets(&off, sh, val, visitor.foundOffsets()))
            // no match
            return false;
    }

    if (!insertSegmentClone(pResult, ctx, item, action, &off))
        return false;

    if (*pResult)
        // mayExistFallback() always implies JS_THREE_WAY
        *pResult = updateJoinStatus(ctx, JS_THREE_WAY);

    return true;
}

bool joinValuePair(SymJoinCtx &ctx, const SchedItem &item)
{
    const TValId v1 = item.fld1.value();
    const TValId v2 = item.fld2.value();
    SJ_DEBUG("--- " << SJ_FLDT(item.fldDst, item.fld1, item.fld2)
            << " -> " << SJ_VALP(v1, v2));

    TValId vDst;
    if (joinCacheLookup(&vDst, ctx, v1, v2)) {
        item.fldDst.setValue(vDst);
        return true;
    }

    bool result;
    if (joinValuesByCode(&result, ctx, item))
        return result;

    const TObjId obj1 = ctx.sh1.objByAddr(v1);
    const TObjId obj2 = ctx.sh2.objByAddr(v2);

    TObjId objDst;
    if (checkObjectMapping(ctx, obj1, obj2, /* allowUnkn */ false, &objDst))
        return mapTargetAddress(ctx, item, objDst);

    const bool isAbs1 = isAbstractObject(ctx.sh1, obj1);
    const bool isAbs2 = isAbstractObject(ctx.sh2, obj2);
    const bool haveAbs = isAbs1 || isAbs2;

    if (haveAbs) {
        if (joinObjects(&result, &objDst, ctx, obj1, obj2, item.ldiff))
            return result && mapTargetAddress(ctx, item, objDst, /* XXX */ true);

        if (segInsertionFallback(&result, ctx, item))
            return result;

        // failed to join abstract targets
        return false;
    }

    if (!checkValueMapping(ctx, v1, v2, /* allowUnknownMapping */ true)) {
        if (mayExistFallback(&result, ctx, item, JS_USE_SH1))
            return result;

        if (mayExistFallback(&result, ctx, item, JS_USE_SH2))
            return result;
    }

    if (joinObjects(&result, &objDst, ctx, obj1, obj2, item.ldiff,
                /* firstTryReadOnly */ false)
            && result)
        return mapTargetAddress(ctx, item, objDst);

    return false;
}

bool joinPendingValues(SymJoinCtx &ctx)
{
    SchedItem item;
    while (ctx.wl.next(item)) {
        if (!joinValuePair(ctx, item))
            return false;
    }

    return true;
}

class JoinVarVisitor {
    public:
        enum EMode {
            JVM_LIVE_OBJS,
            JVM_UNI_BLOCKS
        };

    private:
        SymJoinCtx              &ctx_;
        const EMode             mode_;

    public:
        JoinVarVisitor(SymJoinCtx &ctx, const EMode mode):
            ctx_(ctx),
            mode_(mode)
        {
        }

        bool operator()(const TObjId objs[3]) {
            const TObjId objDst = objs[/* dst */ 0];
            const TObjId obj1   = objs[/* sh1 */ 1];
            const TObjId obj2   = objs[/* sh2 */ 2];

            switch (mode_) {
                case JVM_LIVE_OBJS:
                    return joinFields(ctx_, objDst, obj1, obj2,
                        /* ldiff */ 0);

                case JVM_UNI_BLOCKS:
                    return joinUniBlocks(ctx_, objDst, obj1, obj2);
            }

            CL_BREAK_IF("stack smashing detected");
            return false;
        }
};

bool joinCVars(SymJoinCtx &ctx, const JoinVarVisitor::EMode mode)
{
    SymHeap *const heaps[] = {
        &ctx.dst,
        &ctx.sh1,
        &ctx.sh2
    };

    // go through all program variables
    JoinVarVisitor visitor(ctx, mode);
    return traverseProgramVarsGeneric<
        /* N_DST */ 1,
        /* N_SRC */ 2>
            (heaps, visitor,
             /* allowRecovery */ JoinVarVisitor::JVM_LIVE_OBJS == mode);
}

// FIXME: the implementation is not going to work well in certain cases
bool isFreshProto(SymJoinCtx &ctx, const TObjId objDst, bool *wasMayExist = 0)
{
    const TObjId obj1 = roMapLookup(ctx.objMap1[DIR_RTL], objDst);
    const TObjId obj2 = roMapLookup(ctx.objMap2[DIR_RTL], objDst);

    const bool isValid1 = (OBJ_INVALID != obj1);
    const bool isValid2 = (OBJ_INVALID != obj2);
    if (isValid1 == isValid2)
        return false;

    if (wasMayExist) {
        const EObjKind kind = (isValid1)
            ? ctx.sh1.objKind(obj1)
            : ctx.sh2.objKind(obj2);

        *wasMayExist = isMayExistObj(kind);
    }

    return true;
}

struct MayExistLevelUpdater {
    SymJoinCtx         &ctx;
    const TObjId        objDst;
    TFldSet             ignoreList;

    MayExistLevelUpdater(SymJoinCtx &ctx_, const TObjId objDst_):
        ctx(ctx_),
        objDst(objDst_)
    {
        buildIgnoreList(ignoreList, ctx.dst, objDst);
    }

    bool operator()(const FldHandle &sub) const {
        if (hasKey(this->ignoreList, sub))
            return /* continue */ true;

        const TObjId proto = ctx.dst.objByAddr(sub.value());
        if (OBJ_INVALID == proto)
            return /* continue */ true;

        if (objDst == proto)
            // self loop
            return /* continue */ true;

        if (!isFreshProto(ctx, proto))
            // not a newly introduced one
            return /* continue */ true;

        // this object became a prototype, increment its level
        objIncrementProtoLevel(ctx.dst, proto);
        return /* continue */ true;
    }
};

bool updateMayExistLevels(SymJoinCtx &ctx)
{
    TObjList dstRoots;
    ctx.dst.gatherObjects(dstRoots, isOnHeap);
    BOOST_FOREACH(const TObjId objDst, dstRoots) {
        const EObjKind kind = ctx.dst.objKind(objDst);
        if (!isMayExistObj(kind))
            // we are interested only in 0..1 objects here
            continue;

        bool wasMayExist;
        if (!isFreshProto(ctx, objDst, &wasMayExist) || wasMayExist)
            // not a newly introduced one
            continue;

        const MayExistLevelUpdater visitor(ctx, objDst);
        if (!traverseLivePtrs(ctx.dst, objDst, visitor))
            return false;
    }

    return true;
}

bool handleDstPreds(SymJoinCtx &ctx)
{
    if (!ctx.joiningData()) {
        // cross-over check of Neq predicates

        if (!ctx.sh1.matchPreds(ctx.dst, ctx.valMap1[DIR_LTR])) {
            if (ctx.sh1.matchPreds(ctx.dst, ctx.valMap1[0], /* nonzero */ true))
                ctx.allowThreeWay = false;

            if (!updateJoinStatus(ctx, JS_USE_SH2))
                return false;
        }

        if (!ctx.sh2.matchPreds(ctx.dst, ctx.valMap2[DIR_LTR])) {
            if (ctx.sh2.matchPreds(ctx.dst, ctx.valMap2[0], /* nonzero */ true))
                ctx.allowThreeWay = false;

            if (!updateJoinStatus(ctx, JS_USE_SH1))
                return false;
        }
    }

    // TODO: match generic Neq predicates also in prototypes;  for now we
    // consider only minimal segment lengths
    BOOST_FOREACH(const TObjId protoDst, ctx.protos) {
        const TObjId proto1 = roMapLookup(ctx.objMap1[DIR_RTL], protoDst);
        const TObjId proto2 = roMapLookup(ctx.objMap2[DIR_RTL], protoDst);

        const TMinLen len1   = objMinLength(ctx.sh1, proto1);
        const TMinLen len2   = objMinLength(ctx.sh2, proto2);
        const TMinLen lenDst = objMinLength(ctx.dst, protoDst);

        if ((lenDst < len1) && !updateJoinStatus(ctx, JS_USE_SH2))
            return false;

        if ((lenDst < len2) && !updateJoinStatus(ctx, JS_USE_SH1))
            return false;
    }

    // all OK
    return true;
}

bool validateStatus(const SymJoinCtx &ctx)
{
    if (!ctx.allowThreeWay && ctx.forceThreeWay) {
        CL_DEBUG("!J! forcing three-way join");
        return true;
    }

    if (ctx.allowThreeWay || (JS_THREE_WAY != ctx.status))
        return true;

    CL_DEBUG(">J< cancelling three-way join");
    return false;
}

bool joinSymHeaps(
        EJoinStatus             *pStatus,
        SymHeap                 *pDst,
        SymHeap                  sh1,
        SymHeap                  sh2,
        const bool               allowThreeWay)
{
    SJ_DEBUG("--> joinSymHeaps()");
    TStorRef stor = sh1.stor();
    CL_BREAK_IF(&stor != &sh2.stor());

    // update trace
    Trace::waiveCloneOperation(sh1);
    Trace::waiveCloneOperation(sh2);
    *pDst = SymHeap(stor, new Trace::TransientNode("joinSymHeaps()"));

    // initialize symbolic join ctx
    SymJoinCtx ctx(*pDst, sh1, sh2, allowThreeWay);

    CL_BREAK_IF(!protoCheckConsistency(ctx.sh1));
    CL_BREAK_IF(!protoCheckConsistency(ctx.sh2));

    // try to join the objects that hold the return values
    if (!joinFields(ctx, OBJ_RETURN, OBJ_RETURN, OBJ_RETURN, /* ldiff */ 0))
        goto fail;

    // start with program variables
    if (!joinCVars(ctx, JoinVarVisitor::JVM_LIVE_OBJS))
        goto fail;

    // go through all values in them
    if (!joinPendingValues(ctx))
        goto fail;

    // join uniform blocks
    if (!joinCVars(ctx, JoinVarVisitor::JVM_UNI_BLOCKS))
        goto fail;

    if (!updateMayExistLevels(ctx))
        goto fail;

    // go through shared Neq predicates and set minimal segment lengths
    if (!handleDstPreds(ctx))
        goto fail;

    // if the result is three-way join, check if it is a good idea
    if (!validateStatus(ctx))
        goto fail;

    // catch possible regression at this point
    CL_BREAK_IF((JS_USE_ANY == ctx.status) != areEqual(sh1, sh2));
    CL_BREAK_IF((JS_THREE_WAY == ctx.status) && areEqual(sh1, ctx.dst));
    CL_BREAK_IF((JS_THREE_WAY == ctx.status) && areEqual(sh2, ctx.dst));

    if (JS_THREE_WAY == ctx.status) {
        // create a new trace graph node for JS_THREE_WAY
        Trace::Node *tr1 = sh1.traceNode();
        Trace::Node *tr2 = sh2.traceNode();
        pDst->traceUpdate(new Trace::JoinNode(tr1, tr2));
    }

    // all OK
    *pStatus = ctx.status;
    SJ_DEBUG("<-- joinSymHeaps() says " << ctx.status);
    CL_BREAK_IF(!segCheckConsistency(ctx.dst));
    CL_BREAK_IF(!protoCheckConsistency(ctx.dst));
    return true;

fail:
    // if the join failed on heaps that were isomorphic, something went wrong
    CL_BREAK_IF(areEqual(sh1, sh2));
    return false;
}

// FIXME: this works only for nullified blocks anyway
void killUniBlocksUnderBindingPtrs(
        SymHeap                &sh,
        const BindingOff       &bf,
        const TObjId            obj)
{
    // go through next/prev pointers
    TFldSet blackList;
    buildIgnoreList(blackList, sh, obj, bf);
    BOOST_FOREACH(const FldHandle &fld, blackList) {
        if (VAL_NULL != fld.value())
            continue;

        // if there is a nullified block under next/prev pointer, kill it now
        fld.setValue(VAL_TRUE);
        fld.setValue(VAL_NULL);
    }
}

void recoverPointersToDst(
        SymJoinCtx             &ctx,
        const TObjId            dst)
{
    // redirect pointers from prototypes to their parents
    BOOST_FOREACH(const TObjId protoGhost, ctx.protos) {
        redirectRefs(ctx.dst,
                /* pointingFrom */  protoGhost,
                /* pointingTo   */  dst,
                /* pointingWith */  TS_INVALID,
                /* redirectTo   */  dst,
                /* redirectWith */  TS_ALL);
    }

    // redirect pointers to self
    redirectRefs(ctx.dst,
            /* pointingFrom */  dst,
            /* pointingTo   */  dst,
            /* pointingWith */  TS_INVALID,
            /* redirectTo   */  dst,
            /* redirectWith */  TS_ALL);
}

bool joinData(
        SymHeap                 &sh,
        const BindingOff        &bf,
        const TObjId             obj1,
        const TObjId             obj2,
        TObjId                  *pDst,
        TObjSet                  protoObjs[1][2],
        EJoinStatus             *pStatus)
{
    SJ_DEBUG("--> joinData" << SJ_OBJP(obj1, obj2));

    SymJoinCtx ctx(sh,
            /* l1Drift */ !isAbstractObject(sh, obj1),
            /* l2Drift */ !isAbstractObject(sh, obj2));

    TSizeRange size;
    if (!joinObjSize(&size, ctx, obj1, obj2))
        return false;

    TObjType clt;
    if (!joinObjType(&clt, ctx, obj1, obj2))
        return false;

    const EObjKind kind = (isDlsBinding(bf))
        ? OK_DLS
        : OK_SLS;

    // create an image in ctx.dst
    const TObjId objDst = ctx.dst.heapAlloc(size);
    ctx.dst.objSetAbstract(objDst, kind, bf);

    // intialize the minimum segment length
    const TMinLen lenDst = objMinLength(sh, obj1) + objMinLength(sh, obj2);
    ctx.dst.segSetMinLength(objDst, lenDst);

    if (clt)
        // preserve estimated type-info of the root
        ctx.dst.objSetEstimatedType(objDst, clt);

    // compute the initial ldiff (nesting level difference)
    TProtoLevel ldiff = 0;
    if (isAbstractObject(sh, obj1))
        --ldiff;
    if (isAbstractObject(sh, obj2))
        ++ldiff;

    if (!joinFields(ctx, objDst, obj1, obj2, ldiff, &bf))
        return false;

    // perform main loop
    if (!joinPendingValues(ctx))
        return false;

    killUniBlocksUnderBindingPtrs(sh, bf, obj1);
    killUniBlocksUnderBindingPtrs(sh, bf, obj2);
    if (!joinUniBlocks(ctx, objDst, obj1, obj2))
        // failed to complement uniform blocks
        return false;

    // go through Neq predicates
    if (!handleDstPreds(ctx))
        return false;

    // if the result is three-way join, check if it is a good idea
    if (!validateStatus(ctx))
        return false;

    if (!updateMayExistLevels(ctx)) {
        CL_BREAK_IF("updateMayExistLevels() has failed in joinData()");
        return false;
    }

    if (pDst) {
        recoverPointersToDst(ctx, objDst);
        *pDst = objDst;
    }

    unsigned cntProto1 = 0;
    unsigned cntProto2 = 0;

    // go through prototypes
    BOOST_FOREACH(const TObjId protoDst, ctx.protos) {
        const TObjId proto1 = roMapLookup(ctx.objMap1[DIR_RTL], protoDst);
        const TObjId proto2 = roMapLookup(ctx.objMap2[DIR_RTL], protoDst);

        if (OBJ_INVALID != proto1) {
            ++cntProto1;
            if (protoObjs)
                (*protoObjs)[0].insert(proto1);
        }

        if (OBJ_INVALID != proto2) {
            ++cntProto2;
            if (protoObjs)
                (*protoObjs)[1].insert(proto2);
        }
    }

    if (pStatus)
        *pStatus = ctx.status;

    SJ_DEBUG("<-- joinData() says " << ctx.status << " (found " << cntProto1
            << " | " << cntProto2 << " prototype objects)");

    return true;
}
