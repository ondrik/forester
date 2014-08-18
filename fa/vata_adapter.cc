#include "vata_adapter.hh"

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <utility>

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
    FA_DEBUG_AT(1,"TA get final states\n");
    return vataAut_.GetFinalStates();
}

size_t VATAAdapter::getFinalState() const
{
	FA_DEBUG_AT(1,"TA get final state\n");

	const std::unordered_set<size_t>& finalStates = this->getFinalStates();
	assert(1 == finalStates.size());
    return *finalStates.begin();
}

const VATAAdapter::Transition VATAAdapter::getAcceptingTransition() const
{
    assert(++(vataAut_.GetAcceptTrans().begin()) == vataAut_.GetAcceptTrans().end());
    FA_DEBUG_AT(1,"TA get accepting transitions\n");
    return *(vataAut_.GetAcceptTrans().begin());
}

VATAAdapter& VATAAdapter::unreachableFree(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA unreachable\n");
    dst.vataAut_ = std::move(vataAut_.RemoveUnreachableStates());
    return dst;
}

VATAAdapter& VATAAdapter::uselessAndUnreachableFree(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA useless\n");
    dst.vataAut_ = std::move(vataAut_.RemoveUselessStates());
    dst.vataAut_ = std::move(dst.vataAut_.RemoveUnreachableStates());
    return dst;
}

VATAAdapter& VATAAdapter::disjointUnion(
		VATAAdapter&                      dst,
		const VATAAdapter&                src,
		bool                              addFinalStates)
{
    FA_DEBUG_AT(1,"TA disjoint\n");
    dst.vataAut_ = std::move(TreeAut::UnionDisjointStates(
           dst.vataAut_, src.vataAut_));
    return dst;
}

VATAAdapter& VATAAdapter::minimized(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA minimized\n");
    dst.vataAut_ = std::move(vataAut_.Reduce());
    return dst;
}

bool VATAAdapter::areTransitionsEmpty()
{
    FA_DEBUG_AT(1,"TA are transitions empty\n");
    return vataAut_.AreTransitionsEmpty();
}

// TODO: Rewrite to std::move
VATAAdapter& VATAAdapter::copyTransitions(VATAAdapter& dst) const
{
    FA_DEBUG_AT(1,"TA copy transitions\n");
    CopyAllFunctor copyAllFunctor;
    dst.vataAut_.CopyTransitionsFrom(vataAut_, copyAllFunctor);
	return dst;
}

// TODO: Rewrite to std::move
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

// TODO: Rewrite to std::move
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


// TODO: Rewrite to std::move
VATAAdapter& VATAAdapter::unfoldAtRoot(
    VATAAdapter&                                  dst,
    const std::unordered_map<size_t, size_t>&     statesTranslator,
    bool                                          registerFinalState) const
{
    FA_DEBUG_AT(1,"TA unfoldAtRoot1\n");
    this->copyTransitions(dst);
    for (auto state : this->getFinalStates())
    {
        std::unordered_map<size_t, size_t>::const_iterator j = statesTranslator.find(state);
        assert(j != statesTranslator.end());

        for (auto trans : vataAut_[state])
        {
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

    for (auto state : vataAut_.GetUsedStates())
    {
        index.add(state);
    }
}

// TODO: check this if there will be problems (and they will)
VATAAdapter::TreeAut::AcceptTrans VATAAdapter::getEmptyRootTransitions() const
{
    FA_DEBUG_AT(1,"TA get empty root\n");
    assert(vataAut_.IsStateFinal(cEmptyRootTransIndex)
            && vataAut_.GetFinalStates().size() == 1);
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
    std::vector<const Transition*> stack;
    std::unordered_set<size_t> visited;

    for (const Transition& t : src.vataAut_[rootState])
    {
        stack.push_back(&t);
    }

    while(!stack.empty())
    {
        const Transition& t = *(stack.back());
        stack.pop_back();
        this->addTransition(t);
        visited.insert(t.GetParent());
        for (size_t child : t.GetChildren())
        {
            if (visited.count(child) == 0)
            {
                for (const Transition& k : src.vataAut_[child])
                {
                    stack.push_back(&k);
                }
            }
        }
    }
}

// collapses states according to a given relation
// OL: should finish the function in VATA
VATAAdapter& VATAAdapter::collapsed(
    VATAAdapter&                             dst,
    const std::vector<std::vector<bool>>&    rel,
    const Index<size_t>&                     stateIndex) const
{
    std::unordered_map<size_t, size_t> vataRel; // relation compatible with the one in VATA
    int i = 0;
    for(std::vector<bool> row  : rel)
    {
        int j = 0;
        for(bool rel : row)
        {
            if (rel)
            {
                vataRel[i] = j;
            }
            ++j;
        }
        ++i;
    }
    FA_DEBUG_AT(1,"TA collapsed\n");
    dst.vataAut_ = std::move(vataAut_.CollapseStates(vataRel));

    return dst;
}
