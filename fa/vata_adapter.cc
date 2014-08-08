#include "vata_adapter.hh"

VATAAdapter::VATAAdapter(TreeAut aut) : vataAut_(aut)
{}

VATAAdapter::VATAAdapter() : vataAut_()
{}

VATAAdapter::VATAAdapter(const VATAAdapter& adapter) : vataAut_(adapter.vataAut_)
{}

VATAAdapter::~VATAAdapter()
{}

VATAAdapter VATAAdapter::createTAWithSameTransitions(
    const VATAAdapter&         ta)
{
    FA_DEBUG_AT(1,"Create TA with same transitions\n");
    return VATAAdapter(TreeAut(ta.vataAut_, true, false));
}

VATAAdapter* VATAAdapter::allocateTAWithSameTransitions(
    const VATAAdapter&         ta)
{
    FA_DEBUG_AT(1,"Allocate TA with same transitions\n");
    return new VATAAdapter(TreeAut(ta.vataAut_, true, false));
}
    
VATAAdapter VATAAdapter::createTAWithSameFinalStates(
    const VATAAdapter&         ta,
    bool                 copyFinalStates)
{
    FA_DEBUG_AT(1,"Create TA with same final states\n");
    return VATAAdapter(TreeAut(ta.vataAut_, true, copyFinalStates));
}

VATAAdapter* VATAAdapter::allocateTAWithSameFinalStates(
    const VATAAdapter&         ta,
    bool                 copyFinalStates)
{
    FA_DEBUG_AT(1,"Allocate TA with same final states\n");
    return new VATAAdapter(TreeAut(ta.vataAut_, true, copyFinalStates));
}

VATAAdapter::iterator VATAAdapter::begin() const
{
    FA_DEBUG_AT(1,"TA begin\n");
    return vataAut_.begin();
}

VATAAdapter::iterator VATAAdapter::end() const
{
    FA_DEBUG_AT(1,"TA end\n");
	return vataAut_.end();
}

// TODO CHECK semantic against original implementation
typename VATAAdapter::DownAccessor::Iterator VATAAdapter::begin(
        size_t rhs) const
{
    FA_DEBUG_AT(1,"TA Down begin\n");
    return vataAut_[rhs].begin();
}

// TODO CHECK semantic against original implementation
typename VATAAdapter::DownAccessor::Iterator VATAAdapter::end(
        size_t rhs) const
{
    FA_DEBUG_AT(1,"TA Down end\n");
    return vataAut_[rhs].end();
}

// TODO CHECK semantic against original implementation
typename VATAAdapter::DownAccessor::Iterator VATAAdapter::end(
        size_t rhs,
        DownAccessor::Iterator i) const
{
    FA_DEBUG_AT(1,"TA Down end 1\n");
    return vataAut_[rhs].end();
}

typename VATAAdapter::AcceptTrans::Iterator VATAAdapter::accBegin() const
{
    FA_DEBUG_AT(1,"TA Acc begin\n");
   return vataAut_.GetAcceptTrans().begin();
}

typename VATAAdapter::AcceptTrans::Iterator VATAAdapter::accEnd() const
{
    FA_DEBUG_AT(1,"TA Acc end\n");
   return vataAut_.GetAcceptTrans().end();
}

// TODO CHECK semantic against original implementation
typename VATAAdapter::AcceptTrans::Iterator VATAAdapter::accEnd(
        VATAAdapter::AcceptTrans::Iterator i) const
{
    FA_DEBUG_AT(1,"TA Acc end 1\n");
   return vataAut_.GetAcceptTrans().end();
}

VATAAdapter& VATAAdapter::operator=(const VATAAdapter& rhs)
{
    FA_DEBUG_AT(1,"TA ==\n");
    this->vataAut_ = rhs.vataAut_;
    return *this;
}

void VATAAdapter::addTransition(
		const std::vector<size_t>&          children,
		const SymbolType&                   symbol,
		size_t                              parent)
{
    FA_DEBUG_AT(1,"TA add transition\n");
    this->vataAut_.AddTransition(children, symbol, parent);
}

void VATAAdapter::addTransition(const Transition& transition)
{
    FA_DEBUG_AT(1,"TA add transition 1\n");
    this->vataAut_.AddTransition(transition);
}

const VATAAdapter::Transition VATAAdapter::getTransition(
        const std::vector<size_t>&          children,
		const SymbolType&                   symbol,
		size_t                              parent)

{
    FA_DEBUG_AT(1,"TA get transition\n");
    if (vataAut_.ContainsTransition(children, symbol, parent))
    {
        return Transition(symbol, parent, children);
    }

    assert(false);
    return Transition();
}
	
const label_type VATAAdapter::GetSymbol(const Transition& t)
{
    FA_DEBUG_AT(1,"TA get symbol\n");
    return label_type(t.GetSymbol()); 
}

void VATAAdapter::addFinalState(size_t state)
{
    FA_DEBUG_AT(1,"TA add final state\n");
    vataAut_.SetStateFinal(state);
}

void VATAAdapter::addFinalStates(const std::set<size_t>& states)
{
    FA_DEBUG_AT(1,"TA add final states\n");
    vataAut_.SetStatesFinal(states);
}

void VATAAdapter::addFinalStates(const std::unordered_set<size_t>& states)
{
    FA_DEBUG_AT(1,"TA add final states 1\n");
    for (size_t state : states)
    {
        vataAut_.SetStateFinal(state);
    }
}

bool VATAAdapter::isFinalState(size_t state) const
{
    FA_DEBUG_AT(1,"TA is final state\n");
    return vataAut_.IsStateFinal(state);
}

const std::unordered_set<size_t>& VATAAdapter::getFinalStates() const
{
    FA_DEBUG_AT(1,"TA is final states\n");
    return vataAut_.GetFinalStates();
}

size_t VATAAdapter::getFinalState() const
{
    FA_DEBUG_AT(1,"TA get final states\n");
    return vataAut_.GetFinalState();
}
    
const VATAAdapter::Transition& VATAAdapter::getAcceptingTransition() const
{
    FA_DEBUG_AT(1,"TA get accepting transitions\n");
    return *(vataAut_.GetAcceptTrans().begin());
}

// TODO: Is this correct?
VATAAdapter& VATAAdapter::unreachableFree(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA unreachable\n");
    dst.vataAut_ = vataAut_.RemoveUnreachableStates();
    return dst;
}

// TODO: Is this correct?
VATAAdapter& VATAAdapter::uselessAndUnreachableFree(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA useless\n");
    dst.vataAut_ = vataAut_.RemoveUselessStates();
    dst.vataAut_ = dst.vataAut_.RemoveUnreachableStates();
    return dst;
}

VATAAdapter& VATAAdapter::disjointUnion(
		VATAAdapter&                      dst,
		const VATAAdapter&                src,
		bool                              addFinalStates)
{
    FA_DEBUG_AT(1,"TA disjoint\n");
   dst.vataAut_ = TreeAut::UnionDisjointStates(
           dst.vataAut_, src.vataAut_, addFinalStates);

   return dst;
}

// TODO: Is this correct?
VATAAdapter& VATAAdapter::minimized(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA minimized\n");
    dst.vataAut_ = vataAut_.Reduce();
    return dst;
}

// TODO is this correct
bool VATAAdapter::areTransitionsEmpty()
{
    FA_DEBUG_AT(1,"TA are transitions empty\n");
    return vataAut_.AreTransitionsEmpty();
}

VATAAdapter& VATAAdapter::copyTransitions(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA copy transitions\n");
    CopyAllFunctor copyAllFunctor;
    dst.vataAut_.CopyTransitionsFrom(vataAut_, copyAllFunctor);
	return dst;
}
	
VATAAdapter& VATAAdapter::copyNotAcceptingTransitions(
        VATAAdapter&                       dst,
        const VATAAdapter&                 ta) const
{
    FA_DEBUG_AT(1,"TA copy not accepting transitions\n");
    CopyNonAcceptingFunctor copyFunctor(ta);
    dst.vataAut_.CopyTransitionsFrom(vataAut_, copyFunctor);
	return dst;
}

void VATAAdapter::clear()
{
    FA_DEBUG_AT(1,"TA clear\n");
    vataAut_.Clear();
}

bool VATAAdapter::subseteq(const VATAAdapter& a, const VATAAdapter& b)
{
    FA_DEBUG_AT(1,"TA subseteq\n");
   return TreeAut::CheckInclusion(a.vataAut_, b.vataAut_); 
}

VATAAdapter& VATAAdapter::unfoldAtRoot(
    VATAAdapter&                   dst,
	size_t                         newState,
	bool                           registerFinalState) const
{
    FA_DEBUG_AT(1,"TA unfoldAtRoot\n");
    if (registerFinalState)
    {
        dst.addFinalState(newState);
    }

    for (auto trans : vataAut_)
    {
        dst.addTransition(trans);
        if (isFinalState(trans.GetParent()))
            dst.addTransition(trans.GetChildren(), trans.GetSymbol(), newState);
    }

    return dst;
}


VATAAdapter& VATAAdapter::unfoldAtRoot(
    VATAAdapter&                                  dst,
    const std::unordered_map<size_t, size_t>&     states,
    bool                                          registerFinalState) const
{
    FA_DEBUG_AT(1,"TA unfoldAtRoot1\n");
    copyTransitions(dst);
    for (auto state : getFinalStates())
    {
        std::unordered_map<size_t, size_t>::const_iterator j = states.find(state);
        assert(j != states.end());

        for (auto trans : vataAut_[state])
        { // TODO: Check: is this semantic same as the original?
            dst.addTransition(trans.GetChildren(), trans.GetSymbol(), j->second);
        }

        if (registerFinalState)
        {
            dst.addFinalState(j->second);
        }
    }

    return dst;
}

void VATAAdapter::buildStateIndex(Index<size_t>& index) const
{
    FA_DEBUG_AT(1,"TA buildStateIndex\n");
    for (auto trans : vataAut_)
    {
        for (auto state : trans.GetChildren())
        {
            index.add(state);
        }
        index.add(trans.GetParent());
    }

    for (size_t state : getFinalStates())
    {
        index.add(state);
    }
}

// TODO: check this if there will be problems (and they will)
VATAAdapter::TreeAut::AcceptTrans VATAAdapter::getEmptyRootTransitions() const
{
    FA_DEBUG_AT(1,"TA get empty root\n");
    return vataAut_.GetAcceptTrans();
}

void VATAAdapter::copyReachableTransitionsFromRoot(
    const VATAAdapter&        src,
    const size_t&             rootState)
{
    FA_DEBUG_AT(1,"TA copy reachable transitions from root\n");
    for (const Transition& k : src.vataAut_[rootState])
    {
        addTransition(k);
    }
}

// collapses states according to a given relation
VATAAdapter& VATAAdapter::collapsed(
    VATAAdapter&                             dst,
    const std::vector<std::vector<bool>>&    rel,
    const Index<size_t>&                     stateIndex) const
{
    FA_DEBUG_AT(1,"TA collapsed\n");
    return dst;

    /*
    std::vector<size_t> headIndex;
    utils::relBuildClasses(rel, headIndex);

    std::ostringstream os;
    utils::printCont(os, headIndex);

    // TODO: perhaps improve indexing
    std::vector<size_t> invStateIndex(stateIndex.size());
    for (Index<size_t>::iterator i = stateIndex.begin(); i != stateIndex.end(); ++i)
    {
        invStateIndex[i->second] = i->first;
    }

    for (std::vector<size_t>::iterator i = headIndex.begin(); i != headIndex.end(); ++i)
    {
        *i = invStateIndex[*i];
    }

    for (const size_t& state : finalStates_)
    {
        dst.addFinalState(headIndex[stateIndex[state]]);
    }

    for (const TransIDPair* trans : this->transitions_)
    {
        std::vector<size_t> lhs;
        stateIndex.translate(lhs, trans->first.lhs());
        for (size_t j = 0; j < lhs.size(); ++j)
            lhs[j] = headIndex[lhs[j]];
        dst.addTransition(lhs, trans->first.label(), headIndex[stateIndex[trans->first.rhs()]]);
        std::ostringstream os;
        utils::printCont(os, lhs);
    }
    return dst;
    */
}
