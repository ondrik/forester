/*
 * Copyright (C) 2014 Martin Hruska
 *
 * This file is part of forester.
 *
 * forester is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * forester is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with forester.  If not, see <http://www.gnu.org/licenses/>.
 */

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
        std::unordered_map<size_t, size_t>&    result,
		const std::unordered_set<size_t>&      usedStates)
{
	for (const size_t state : usedStates)
    {
		if (!result.count(state))
		{
			result[state] = state;
		}
    }
}
