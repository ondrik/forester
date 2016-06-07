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
		const std::vector<std::shared_ptr<const TreeAut>>&    predicates)
{
	FA_DEBUG_AT(1,"Predicate abstraction input: predicates " << predicates.size() << " FAE: " << fae_);

	Index<size_t> faeStateIndex;
	for (size_t i = 0; i < fae_.getRootCount(); ++i)
	{
		assert(nullptr != fae_.getRoot(i));
		fae_.getRoot(i)->buildStateIndex(faeStateIndex);
	}

	const size_t numStates = faeStateIndex.size();

	FA_DEBUG_AT(1,"Index: " << faeStateIndex);

	// create the initial relation
	// TODO: use boost::dynamic_bitset
	std::vector<std::vector<bool>> rel;

	VATA::AutBase::ProductTranslMap translMap;
	for (const auto& predicate : predicates)
	{
		FA_DEBUG_AT(1,"Predicate: " << *predicate);
		//assert(predicate->getRootCount() >= this->fae_.getRootCount());
		//const size_t roots = predicate->getRootCount() >= this->fae_.getRootCount()
		//	? this->fae_.getRootCount() : predicate->getRootCount();

		for (size_t root = 0; root < this->fae_.getRootCount(); ++root)
		{
			FA_DEBUG_AT(1,"ISECT1: " << *this->fae_.getRoot(root));
			FA_DEBUG_AT(1,"ISECT2: " << predicate /* *(predicate->getRoot(root)) */);
			if (this->fae_.getRoot(root) != nullptr && predicate != nullptr)
			{
				const auto res = VATAAdapter::intersectionBU(
						*this->fae_.getRoot(root),
						/* *(predicate->getRoot(root)) */ *predicate, &translMap);
				FA_DEBUG_AT(1, "RES: " << res);
			}
		}
	}

    std::vector<std::set<size_t>> matchWith(numStates, std::set<size_t>());
    for (const auto& matchPair : translMap)
    {
        matchWith[faeStateIndex[matchPair.first.first]].insert(matchPair.first.second);
    }

    /*
    std::set<std::pair<size_t, size_t>> product;
    FAE::makeProduct(fae_, *predicates.back(), product);

    // create a map of states of 'fae_' on sets of states of 'predicate'
    std::vector<std::set<size_t>> matchWith(numStates, std::set<size_t>());
    for (const std::pair<size_t, size_t>& statePair : product)
    {
        matchWith[faeStateIndex[statePair.first]].insert(statePair.second);
    }
    */

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

	for (size_t i = 0; i < fae_.getRootCount(); ++i)
	{
		assert(nullptr != fae_.getRoot(i));

		// refine the relation according to cutpoints etc.
		ConnectionGraph::StateToCutpointSignatureMap stateMap;
		ConnectionGraph::computeSignatures(stateMap, *fae_.getRoot(i));
		for (const auto j : faeStateIndex)
		{ // go through the matrix
			for (const auto k : faeStateIndex)
			{
				if (k == j)
				{
					continue;
				}

				if (fae_.getRoot(i)->isFinalState(j.first)
					|| fae_.getRoot(i)->isFinalState(k.first))
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
		const auto usedStates = fae_.getRoot(i)->getUsedStates();
		for (const auto j : usedStates)
		{
			for (const auto k : usedStates)
			{
				if (j == k ||
					rel.at(faeStateIndex.translate(j)).at(
							faeStateIndex.translate(k)) == false)
				{
					continue;
				}

				bool matchedAll = true;
				for (auto jIt = fae_.getRoot(i)->begin(j); jIt != fae_.getRoot(i)->end(j); ++jIt)
				{
					bool matchedOne = true;
					for (auto kIt = fae_.getRoot(i)->begin(k); kIt != fae_.getRoot(i)->end(k); ++kIt)
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
					rel.at(faeStateIndex.translate(j)).at(faeStateIndex.translate(k)) = false;
				}
			}

			for (const auto& item : faeStateIndex)
			{
				if (!usedStates.count(item.first) && usedStates.count(faeStateIndex.translate(j)))
				{ // if state is not in the same automaton, refine
					rel.at(faeStateIndex.translate(j)).at(item.second) = false;
					rel.at(item.second).at(faeStateIndex.translate(j)) = false;
				}
			}
		}
	}

	std::ostringstream ossRel;
	utils::relPrint(ossRel, rel);
	FA_DEBUG_AT(1,"Relation: \n" << ossRel.str());

	std::ostringstream ossInd;
	ossInd << '[';
	for (const auto it : faeStateIndex)
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

	for (const auto& item1 : faeStateIndex)
	{
		for (const auto& item2 : faeStateIndex)
		{
			if (rel.at(item1.second).at(item2.second))
			{
				relCom[item2.first] = item1.first;
			}
		}
	}

	for (const auto& item : faeStateIndex)
	{
		if (relCom.count(item.second) == 0)
		{
			relCom[item.second] = item.second;
		}
	}

	for (size_t i = 0; i < fae_.getRootCount(); ++i)
	{
		TreeAut ta = fae_.createTAWithSameBackend();
		if (i >= 2)
		{
			fae_.getRoot(i)->collapsed(ta, relCom);
		}
		else
		{
			ta = *fae_.getRoot(i);
		}
		FA_DEBUG_AT(1,"NEW TA " << ta);
		//fae_.setRoot(i, std::shared_ptr<TreeAut>(ta));
		fae_.setRoot(i, std::shared_ptr<TreeAut>(fae_.allocTA()));
		ta.uselessAndUnreachableFree(*fae_.getRoot(i));
	}

	FA_DEBUG_AT(1,"Predicate abstraction output: " << fae_);
}
