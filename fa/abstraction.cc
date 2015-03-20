#include "abstraction.hh"

#include "streams.hh"

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

	if (!predicates.empty())
	{
		FA_NOTE("Predicate: " << *predicates.back());

		std::set<std::pair<size_t, size_t>> product;
		FAE::makeProduct(fae_, *predicates.back(), product);

		// create a map of states of 'fae_' on sets of states of 'predicate'
		std::vector<std::set<size_t>> matchWith(numStates, std::set<size_t>());
		for (const std::pair<size_t, size_t>& statePair : product)
		{
			matchWith[faeStateIndex[statePair.first]].insert(statePair.second);
		}

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
	else
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
		for (auto j = faeStateIndex.begin(); j != faeStateIndex.end(); ++j)
		{	// go through the matrix
			for (auto k = faeStateIndex.begin(); k != faeStateIndex.end(); ++k)
			{
				if (k == j)
					continue;

				if (fae_.getRoot(i)->isFinalState(j->first)
					|| fae_.getRoot(i)->isFinalState(k->first))
				{
					rel[j->second][k->second] = false;
					continue;
				}

				// load data if present
				const Data* jData;
				bool jIsData = fae_.isData(j->first, jData);
				assert((!jIsData || (nullptr != jData)) && (!(nullptr == jData) || !jIsData));
				const Data* kData;
				bool kIsData = fae_.isData(k->first, kData);
				assert((!kIsData || (nullptr != kData)) && (!(nullptr == kData) || !kIsData));

				if (jIsData || kIsData)
				{
					rel[j->second][k->second] = false;
					continue;
				}

				if (stateMap[j->first] % stateMap[k->first])
					continue;

				rel[j->second][k->second] = false;
			}
		}
	}

	std::ostringstream oss;
	utils::relPrint(oss, rel);
	FA_NOTE("Relation: \n" << oss.str());

	std::ostringstream ossInd;
	ossInd << '[';
	for (auto it = faeStateIndex.begin(); it != faeStateIndex.end(); ++it)
	{
		ossInd << '(' << FA::writeState(it->first) << ',' << it->second << ')';
	}

	ossInd << ']';
	FA_NOTE("Index: " << ossInd.str());

	// TODO: label states of fae_ by states of predicate

	for (size_t i = 0; i < fae_.getRootCount(); ++i)
	{
		TreeAut ta = fae_.createTAWithSameBackend();
		//fae_.getRoot(i)->collapsed(ta, rel, faeStateIndex);
		fae_.setRoot(i, std::shared_ptr<TreeAut>(fae_.allocTA()));
		ta.uselessAndUnreachableFree(*fae_.getRoot(i));
	}

	FA_NOTE("Predicate abstraction output: " << fae_);
}
