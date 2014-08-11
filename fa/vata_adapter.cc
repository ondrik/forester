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
    FA_DEBUG_AT(1,"TA =\n");

		if (this != &rhs)
		{
			this->vataAut_ = rhs.vataAut_;
		}

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

// OL: I don't know whether it is or is not necessary to return the reference
// (as in the original). There may be some guys depending on the returning
// value to be a reference? Let's take a look at this.
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

// OL: maybe use one templated function for the two methods?
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

	const std::unordered_set<size_t>& finalStates = this->getFinalStates();
	assert(1 == finalStates.size());
  return *finalStates.begin();
}

const VATAAdapter::Transition& VATAAdapter::getAcceptingTransition() const
{
    FA_DEBUG_AT(1,"TA get accepting transitions\n");
		assert(1 == vataAut_.GetAcceptTrans().size());
    return *(vataAut_.GetAcceptTrans().begin());
}

// TODO: Is this correct?
// OL: Let's simplify it to use the C++11 r-value reference. But this can be
// done only if dst is empty.
VATAAdapter& VATAAdapter::unreachableFree(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA unreachable\n");
    dst.vataAut_ = vataAut_.RemoveUnreachableStates();
    return dst;
}

// TODO: Is this correct?
// OL: Let's simplify it to use the C++11 r-value reference. But this can be
// done only if dst is empty.
// Further: in which other should we do it? Let's discuss this.
VATAAdapter& VATAAdapter::uselessAndUnreachableFree(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA useless\n");
    dst.vataAut_ = vataAut_.RemoveUselessStates();
    dst.vataAut_ = dst.vataAut_.RemoveUnreachableStates();
    return dst;
}

// OL: Let's simplify it to use the C++11 r-value reference. But this can be
// done only if dst is empty.
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
// OL: Should be, though the signature of Reduce is about to change in VATA to
// have as the parameter settings of the minimization.
// OL: Let's simplify it to use the C++11 r-value reference. But this can be
// done only if dst is empty.
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

// OL: Let's simplify it to use the C++11 r-value reference. But this can be
// done only if dst is empty.
VATAAdapter& VATAAdapter::copyTransitions(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA copy transitions\n");
    CopyAllFunctor copyAllFunctor;
    dst.vataAut_.CopyTransitionsFrom(vataAut_, copyAllFunctor);
	return dst;
}

// OL: Let's simplify it to use the C++11 r-value reference. But this can be
// done only if dst is empty.
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

// OL: Let's simplify it to use the C++11 r-value reference. But this can be
// done only if dst is empty.
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


// OL: Let's simplify it to use the C++11 r-value reference. But this can be
// done only if dst is empty.
// We might also change the name of the function, or at least of the 'states'
// parameter
VATAAdapter& VATAAdapter::unfoldAtRoot(
    VATAAdapter&                                  dst,
    const std::unordered_map<size_t, size_t>&     states,
    bool                                          registerFinalState) const
{
    FA_DEBUG_AT(1,"TA unfoldAtRoot1\n");
    this->copyTransitions(dst);
    for (auto state : this->getFinalStates())
    {
        std::unordered_map<size_t, size_t>::const_iterator j = states.find(state);
        assert(j != states.end());

        for (auto trans : vataAut_[state])
        { // TODO: Check: is this semantic same as the original?
					// OL: I'd say it is
            dst.addTransition(trans.GetChildren(), trans.GetSymbol(), j->second);
        }

        if (registerFinalState)
        {
            dst.addFinalState(j->second);
        }
    }

    return dst;
}

// OL: Make this a VATA function? E.g. GetUsedStates()?.
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
// OL: This is AFAIK used only once in code (forestautext.hh)
// OL: maybe return vataAut_[cEmptyRootIndex]?
// OL: maybe log the transitions that are returned in the original and this version?
VATAAdapter::TreeAut::AcceptTrans VATAAdapter::getEmptyRootTransitions() const
{
    FA_DEBUG_AT(1,"TA get empty root\n");
    return vataAut_.GetAcceptTrans();
}

// OL: this should return ALL transitions reachable from rootState, not only
// where rootState is the parent, but also those under them. So this method
// should do reachability analysis of the graph of the automaton (remembering
// already visited states etc.). See treeaut.hh, around the line 360.
void VATAAdapter::copyReachableTransitionsFromRoot(
    const VATAAdapter&        src,
    const size_t&             rootState)
{
    FA_DEBUG_AT(1,"TA copy reachable transitions from root\n");
    for (const Transition& k : src.vataAut_[rootState])
    {
        this->addTransition(k);
    }
}

// collapses states according to a given relation
// OL: should finish the function in VATA
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
