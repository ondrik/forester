#include "abstraction.hh"

#include "streams.hh"
#include "vata_adapter.hh"

#include <vata/aut_base.hh>

#include <vector>

void Abstraction::predicateAbstraction(
		const std::vector<std::shared_ptr<const FAE>>&    predicates)
{
	FA_NOTE("Predicate abstraction input: " << fae_);

	Index<size_t> faeStateIndex;
	for (size_t i = 0; i < fae_.getRootCount(); ++i)
	{
		assert(nullptr != fae_.getRoot(i));
		fae_.getRoot(i)->buildStateIndex(faeStateIndex);
	}

	const size_t numStates = faeStateIndex.size();

	FA_NOTE("Index: " << faeStateIndex);

	// create the initial relation
	// TODO: use boost::dynamic_bitset
	std::vector<std::vector<bool>> rel;

	for (const auto& predicate : predicates)
	{
		FA_NOTE("Predicate: " << *predicates.back());
		assert(predicates.back()->getRootCount() == this->fae_.getRootCount());

		VATA::AutBase::ProductTranslMap translMap;
		for (size_t root = 0; root < this->fae_.getRootCount(); ++root)
		{
			VATAAdapter::intersectionBU(*this->fae_.getRoot(root), *(predicate->getRoot(root)), &translMap);

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
		FA_NOTE("matchWith: " << oss.str());
		
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
					rel[j.second][k.second] = false;
					continue;
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
					continue;
				}

				if (stateMap[j.first] % stateMap[k.first])
				{
					continue;
				}

				rel[j.second][k.second] = false;
			}
		}
	}

	std::ostringstream oss;
	utils::relPrint(oss, rel);
	FA_NOTE("Relation: \n" << oss.str());

	std::ostringstream ossInd;
	ossInd << '[';
	for (const auto it : faeStateIndex)
	{
		ossInd << '(' << FA::writeState(it.first) << ',' << it.second << ')';
	}

	ossInd << ']';
	FA_NOTE("Index: " << ossInd.str());

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
			if (rel[item1.second][item2.second])
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
		TreeAut* ta = fae_.allocTA();
		fae_.getRoot(i)->collapsed(*ta, relCom);
		FA_NOTE("NEW TA " << *ta);
		fae_.setRoot(i, std::shared_ptr<TreeAut>(ta));
		//ta.uselessAndUnreachableFree(*fae_.getRoot(i));
	}

	FA_NOTE("Predicate abstraction output: " << fae_);
}
