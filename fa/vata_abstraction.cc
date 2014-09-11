#include "vata_abstraction.hh"

bool VATAAbstraction::statesInRel(
			const size_t                             state1,
			const size_t                             state2,
			const std::unordered_map<size_t,size_t>& tmp)
{
		if (!tmp.count(state1) && !tmp.count(state2))
		{
			return true;
		}

		if (!tmp.count(state1) || !tmp.count(state2))
		{
			return false;
		}

		if (tmp.at(state1) == state2 ||
				tmp.at(state2) == state1 ||
				tmp.at(state1) == tmp.at(state2))
		{
			return true;
		}

		return false;
}

void VATAAbstraction::completeSymmetricIndex(
        std::unordered_map<size_t, size_t>& result,
				const StateToIndexMap&              stateIndex
)
{
		for (StateToIndexMap::iterator i = stateIndex.begin(); i != stateIndex.end(); ++i)
    {
				const size_t state = i->first;
	 			if (!result.count(state))
				{
					result[state] = state;
				}
    }
}
