#include "vata_adapter.hh"
VATAAdapter::VATAAdapter(TreeAut aut) : vataAut_(aut)
{}

VATAAdapter::VATAAdapter() : vataAut_()
{}

VATAAdapter::VATAAdapter(const VATAAdapter& adapter) : vataAut_(adapter.vataAut_)
{}

VATAAdapter::~VATAAdapter()
{}

VATAAdapter VATAAdapter::createVATAAdapterWithSameTransitions(
    const VATAAdapter&         ta)
{
    return VATAAdapter(TreeAut(ta.vataAut_, true, false));
}

VATAAdapter* VATAAdapter::allocateVATAAdapterWithSameTransitions(
    const VATAAdapter&         ta)
{
    return new VATAAdapter(TreeAut(ta.vataAut_, true, false));
}
    
VATAAdapter VATAAdapter::createVATAAdapterWithSameFinalStates(
    const VATAAdapter&         ta,
    bool                 copyFinalStates)
{
    return VATAAdapter(TreeAut(ta.vataAut_, true, copyFinalStates));
}

VATAAdapter* VATAAdapter::allocateVATAAdapterWithSameFinalStates(
    const VATAAdapter&         ta,
    bool                 copyFinalStates)
{
    return new VATAAdapter(TreeAut(ta.vataAut_, true, copyFinalStates));
}

VATAAdapter::iterator VATAAdapter::begin() const
{
    return vataAut_.begin();
}

VATAAdapter::iterator VATAAdapter::end() const
{
	return vataAut_.end();
}

// TODO CHECK semantic against original implementation
typename VATAAdapter::DownAccessor::Iterator VATAAdapter::begin(
        size_t rhs) const
{
    return vataAut_[rhs].begin();
}

// TODO CHECK semantic against original implementation
typename VATAAdapter::DownAccessor::Iterator VATAAdapter::end(
        size_t rhs) const
{
    return vataAut_[rhs].end();
}

// TODO CHECK semantic against original implementation
typename VATAAdapter::DownAccessor::Iterator VATAAdapter::end(
        size_t rhs,
        DownAccessor::Iterator i) const
{
    return vataAut_[rhs].end();
}

typename VATAAdapter::AcceptTrans::Iterator VATAAdapter::accBegin() const
{
   return vataAut_.GetAcceptTrans().begin();
}

typename VATAAdapter::AcceptTrans::Iterator VATAAdapter::accEnd() const
{
   return vataAut_.GetAcceptTrans().end();
}

// TODO CHECK semantic against original implementation
typename VATAAdapter::AcceptTrans::Iterator VATAAdapter::accEnd(
        VATAAdapter::AcceptTrans::Iterator i) const
{
   return vataAut_.GetAcceptTrans().end();
}

VATAAdapter& VATAAdapter::operator=(const VATAAdapter& rhs)
{
    this->vataAut_ = rhs.vataAut_;
    return *this;
}

void VATAAdapter::addTransition(
		const std::vector<size_t>&          children,
		const SymbolType&                   symbol,
		size_t                              parent)
{
    this->vataAut_.AddTransition(children, symbol, parent);
}

void VATAAdapter::addTransition(const Transition& transition)
{
    this->vataAut_.AddTransition(transition);
}

const VATAAdapter::Transition VATAAdapter::getTransition(
        const std::vector<size_t>&          children,
		const SymbolType&                   symbol,
		size_t                              parent)

{
    if (vataAut_.ContainsTransition(children, symbol, parent))
    {
        return Transition(symbol, parent, children);
    }

    assert(false);
    return Transition();
}

void VATAAdapter::addFinalState(size_t state)
{
    vataAut_.SetStateFinal(state);
}

void VATAAdapter::addFinalStates(const std::set<size_t>& states)
{
    vataAut_.SetStatesFinal(states);
}

bool VATAAdapter::isFinalState(size_t state) const
{
    return vataAut_.IsStateFinal(state);
}

const std::unordered_set<size_t>& VATAAdapter::getFinalStates() const
{
    return vataAut_.GetFinalStates();
}

size_t VATAAdapter::getFinalState() const
{
    return vataAut_.GetFinalState();
}
    
const VATAAdapter::Transition& VATAAdapter::getAcceptingTransition() const
{
    return *(vataAut_.GetAcceptTrans().begin());
}

// TODO: Is this correct?
VATAAdapter& VATAAdapter::unreachableFree(VATAAdapter& dst) const
{
    vataAut_.RemoveUnreachableStates();
    dst = *this;
    return dst;
}

// TODO: Is this correct?
VATAAdapter& VATAAdapter::uselessAndUnreachableFree(VATAAdapter& dst) const
{
    vataAut_.RemoveUselessStates();
    vataAut_.RemoveUnreachableStates();
    dst = *this;
    return dst;
}

VATAAdapter& VATAAdapter::disjointUnion(
		VATAAdapter&                      dst,
		const VATAAdapter&                src,
		bool                              addFinalStates)
{
   dst.vataAut_ = TreeAut::UnionDisjointStates(
           dst.vataAut_, src.vataAut_, addFinalStates);

   return dst;
}

// TODO: Is this correct?
VATAAdapter& VATAAdapter::minimized(VATAAdapter& dst) const
{
    dst.vataAut_ = vataAut_.Reduce();
    return dst;
}

// TODO is this correct
bool VATAAdapter::areTransitionsEmpty()
{
    return vataAut_.AreTransitionsEmpty();
}

VATAAdapter& VATAAdapter::copyTransitions(VATAAdapter& dst) const
{
    CopyAllFunctor copyAllFunctor;
    dst.vataAut_.CopyTransitionsFrom(vataAut_, copyAllFunctor);
	return dst;
}
	
VATAAdapter& VATAAdapter::copyNotAcceptingTransitions(
        VATAAdapter&                       dst,
        const VATAAdapter&                 ta) const
{
    CopyNonAcceptingFunctor copyFunctor(ta);
    dst.vataAut_.CopyTransitionsFrom(vataAut_, copyFunctor);
	return dst;
}

void VATAAdapter::clear()
{
    vataAut_.Clear();
}

