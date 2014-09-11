#include "vata_abstraction.hh"

bool VATAAbstraction::statesInRel(
			const size_t                             state1,
			const size_t                             state2,
			const std::unordered_map<size_t,size_t>& tmp)
{
		if (tmp.size() == 0)
		{ // all states in a relation
			return true;
		}

		const size_t count1 = tmp.count(state1);
		const size_t count2 = tmp.count(state2);
		if (!count1 && !count2)
		{
			return true;
		}

		if (!count1 || !count2)
		{
			return false;
		}

		const size_t relState1 = tmp.at(state1);
		const size_t relState2 = tmp.at(state2);
		if (relState1 == state2 ||
				relState2 == state1 ||
				relState1 == relState2)
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
