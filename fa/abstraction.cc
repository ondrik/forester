#include "abstraction.hh"
#include "forestaut.hh"
#include "streams.hh"
#include "vata_adapter.hh"

#include <vata/aut_base.hh>

#include <vector>

namespace
{
	struct SmartTMatchF
	{
		bool operator()(
				const TreeAut::Transition &t1,
				const TreeAut::Transition &t2)
		{
			label_type l1 = TreeAut::GetSymbol(t1);
			label_type l2 = TreeAut::GetSymbol(t2);
			if (l1->isNode() && l2->isNode())
			{
				if (!FA_ALLOW_STACK_FRAME_ABSTRACTION)
				{
					if ((static_cast<const TypeBox *>(l1->nodeLookup(-1).aBox))->getName().find("__@") == 0)
					{
						return l1 == l2;
					}
				}
				return l1->getTag() == l2->getTag();
			}

			return l1 == l2;
		}
	};

	class SmarterTMatchF
{
private:  // data members

	const FAE& fae_;

public:   // methods

	SmarterTMatchF(const FAE& fae) :
		fae_(fae)
	{ }

	bool operator()(
		const TreeAut::Transition&  t1,
		const TreeAut::Transition&  t2)
	{
        label_type l1 = TreeAut::GetSymbol(t1);
        label_type l2 = TreeAut::GetSymbol(t2);
		if (!l1->isNode() || !l2->isNode())
			return l1 == l2;

		if (l1->getTag() != l2->getTag())
			return false;

		if (&t1.GetChildren() == &t2.GetChildren())
			return true;

		if (t1.GetChildrenSize() != t2.GetChildrenSize())
			return false;

		for (size_t i = 0; i < t1.GetChildrenSize(); ++i)
		{
			size_t s1 = t1.GetNthChildren(i), s2 = t2.GetNthChildren(i), ref;

			if (s1 == s2)
				continue;

			if (FA::isData(s1))
			{
				if (!this->fae_.getRef(s1, ref))
					return false;

				if (FA::isData(s2))
					return false;
			} else
			{
				if (FA::isData(s2) && !fae_.getRef(s2, ref))
					return false;
			}
		}

		return true;
	}
};
}

void Abstraction::predicateAbstraction(
		const size_t                                          abstrRoot,
		const std::vector<std::shared_ptr<const TreeAut>>&    predicates)
{
	FA_DEBUG_AT(1,"Predicate abstraction input: predicates " << predicates.size() << " FAE: " << fae_);

	assert(fae_.getRoot(abstrRoot) != nullptr);
	const TreeAut& abstrTa = *fae_.getRoot(abstrRoot);

	Index<size_t> abstrTaStateIndex;
	abstrTa.buildStateIndex(abstrTaStateIndex);

	const size_t numStates = abstrTaStateIndex.size();
	assert(abstrTa.getUsedStates().size() == numStates);

	FA_DEBUG_AT(1,"Index: " << abstrTaStateIndex);

	// create the initial relation
	// TODO: use boost::dynamic_bitset
	std::vector<std::vector<bool>> rel;

	VATA::AutBase::ProductTranslMap translMap;
	for (const auto& predicate : predicates)
	{
		FA_DEBUG_AT(1,"Predicate: " << *predicate);
        FA_DEBUG_AT(1,"ISECT1: " << abstrTa);
        FA_DEBUG_AT(1,"ISECT2: " << predicate /* *(predicate->getRoot(root)) */);

        if (predicate != nullptr)
        {
            const auto res = VATAAdapter::intersectionBU(
                    abstrTa,
                    /* *(predicate->getRoot(root)) */ *predicate, &translMap);
            FA_DEBUG_AT(1, "RES: " << res);
        }
	}

    std::vector<std::set<size_t>> matchWith(numStates, std::set<size_t>());
    for (const auto& matchPair : translMap)
    {
        matchWith[abstrTaStateIndex[matchPair.first.first]].insert(matchPair.first.second);
    }

    std::ostringstream oss;
    for (size_t i = 0; i < matchWith.size(); ++i)
    {
        oss << ", " << i << " -> ";
        utils::printCont(oss, matchWith[i]);
    }
    FA_DEBUG_AT(1,"matchWith: " << oss.str());

    // create the relation
    rel.assign(numStates, std::vector<bool>(numStates, false));
    for (size_t i = 0; i < numStates; ++i)
    {
        rel[i][i] = true;

        for (size_t j = 0 ; j < i; ++j)
        {
            if (matchWith[i] == matchWith[j])
            {
                rel[i][j] = true;
                rel[j][i] = true;
            }
        }
    }
	if (predicates.empty()) //else
	{
		// create universal relation
		rel.assign(numStates, std::vector<bool>(numStates, true));
	}

    // refine the relation according to cutpoints etc.
    ConnectionGraph::StateToCutpointSignatureMap stateMap;
    ConnectionGraph::computeSignatures(stateMap, abstrTa);
    for (const auto j : abstrTaStateIndex)
    { // go through the matrix
        for (const auto k : abstrTaStateIndex)
        {
            if (k == j)
            {
                continue;
            }

            if (abstrTa.isFinalState(j.first)
                || abstrTa.isFinalState(k.first))
            {
                // rel[j.second][k.second] = false;
                // continue;
            }

            // load data if present
            const Data* jData;
            const bool jIsData = fae_.isData(j.first, jData);
            assert((!jIsData || (nullptr != jData)) && (!(nullptr == jData) || !jIsData));
            const Data* kData;
            const bool kIsData = fae_.isData(k.first, kData);
            assert((!kIsData || (nullptr != kData)) && (!(nullptr == kData) || !kIsData));

            if (jIsData || kIsData)
            {
                rel[j.second][k.second] = false;
            }
            else if (!(stateMap[j.first] % stateMap[k.first]))
            {
                rel[j.second][k.second] = false;
            }

        }
    }

    struct SmartTMatchF matcher;
    const auto usedStates = abstrTa.getUsedStates();
    for (const auto j : usedStates)
    {
        for (const auto k : usedStates)
        {
            if (j == k ||
                rel.at(abstrTaStateIndex.translate(j)).at(
                        abstrTaStateIndex.translate(k)) == false)
            {
                continue;
            }

            bool matchedAll = true;
            for (auto jIt = abstrTa.begin(j); jIt != abstrTa.end(j); ++jIt)
            {
                bool matchedOne = true;
                for (auto kIt = abstrTa.begin(k); kIt != abstrTa.end(k); ++kIt)
                {
                    if (!matcher(*jIt, *kIt))
                    {
                        matchedOne = false;
                        break;
                    }
                }
                matchedAll &= matchedOne;
            }

            if (!matchedAll)
            {
                rel.at(abstrTaStateIndex.translate(j)).at(abstrTaStateIndex.translate(k)) = false;
            }
        }
    }

	std::ostringstream ossRel;
	utils::relPrint(ossRel, rel);
	FA_DEBUG_AT(1,"Relation: \n" << ossRel.str());

	std::ostringstream ossInd;
	ossInd << '[';
	for (const auto it : abstrTaStateIndex)
	{
		ossInd << '(' << FA::writeState(it.first) << ',' << it.second << ')';
	}

	ossInd << ']';
	FA_DEBUG_AT(1,"Index: " << ossInd.str());

	// TODO: label states of fae_ by states of predicate
	std::unordered_map<size_t, size_t> relCom;
	for (size_t state = 0; state < numStates; ++state)
	{
		relCom[state] = state;
	}

	for (const auto& item1 : abstrTaStateIndex)
	{
		for (const auto& item2 : abstrTaStateIndex)
		{
			if (rel.at(item1.second).at(item2.second))
			{
				relCom[item2.first] = item1.first;
			}
		}
	}

	for (const auto& item : abstrTaStateIndex)
	{
		if (relCom.count(item.second) == 0)
		{
			relCom[item.second] = item.second;
		}
	}

    TreeAut ta = fae_.createTAWithSameBackend();
    if (abstrRoot >= 2)
    {
        abstrTa.collapsed(ta, relCom);
    }
    else
    {
        ta = abstrTa;
    }
    FA_DEBUG_AT(1,"NEW TA " << ta);
    fae_.setRoot(abstrRoot, std::shared_ptr<TreeAut>(fae_.allocTA()));
    ta.uselessAndUnreachableFree(*fae_.getRoot(abstrRoot));

	FA_DEBUG_AT(1,"Predicate abstraction output: " << fae_);
}
