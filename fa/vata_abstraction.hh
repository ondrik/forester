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

#ifndef VATA_ABSTRACTION_H
#define VATA_ABSTRACTION_H

// std headers
#include <unordered_map>
#include <unordered_set>
#include <iostream>

class VATAAbstraction
{
public:
    // currently erases '1' from the relation
	template <class A, class F, class G>
	static void heightAbstraction(
		const A&                                   aut,
		std::unordered_map<size_t, size_t>&        result,
		size_t                                     height,
		F                                          f,
		G                                          cutpointMatch)
	{
			std::unordered_map<size_t, size_t> tmp;
			const std::unordered_set<size_t> usedStates(aut.GetUsedStates());

			while (height--)
			{
					tmp = result;

					for (const size_t state1 : usedStates)
					{
							for (const size_t state2 : usedStates)
							{
									if (VATAAbstraction::areStatesEquivalent(
												aut, state1, state2, f, cutpointMatch, tmp))
									{
											result[state2] = state1;
									}
							}
					}
				}

				VATAAbstraction::completeSymmetricIndex(result, usedStates);
	}

private:
    /**
     * @brief Function check whether two states are in a relation in a given automaton
     *
     * Checks whether two states are in a relation that means checking if all of 
     * their transitions match.
     *
     * @return true When states are in the relation, otherwise false
     */
	template <class A, class F, class G>
	static bool areStatesEquivalent(
		const A&                                 aut,
		const size_t                             state1,
		const size_t                             state2,
		F                                        f,
		G                                        cutpointMatch,
		const std::unordered_map<size_t,size_t>& tmp)
	{
			if (state1 == state2)
			{
					return true;
			}
			if(!cutpointMatch(state1, state2)
				|| !statesInRel(state1, state2, tmp))
			{
					return false;
			}

			for (const typename A::Transition trans1 : aut[state1])
			{
					for (const typename A::Transition trans2 : aut[state2])
					{
							if (!VATAAbstraction::transMatch(
										trans1, trans2, f, tmp))
							{ // if two transitions does not match, states are not equivalent
									return false;
							}
					}
			}

			return true;
	}

   /**
	 * @brief  Determines whether two transitions match
	 *
	 * This function determines whether two transitions_ match (and can therefore
	 * e.g. be merged during abstraction). First, the @p funcMatch functor is used
	 * to determine whether the transitions are to be checked at all.
	 */
	template <class T, class F>
	static bool transMatch(
		const T&                                  trans1,
		const T&                                  trans2,
		F                                         funcMatch,
		const std::unordered_map<size_t, size_t>& rel)
	{
			if (trans1.GetChildrenSize() != trans2.GetChildrenSize())
			{
					return false;
			}

			if (!funcMatch(trans1, trans2))
			{ // check symbol
					return false;
			}
		
			for (size_t m = 0; m < trans1.GetChildrenSize(); ++m)
			{
				const size_t ch1 = trans1.GetNthChildren(m);
				const size_t ch2 = trans2.GetNthChildren(m);
				
				if (!statesInRel(ch1, ch2, rel))
				{ // NOTE A signature of children is not checked here
						return false;
				}
			}

			return true;
	}

	static bool statesInRel(
		const size_t                             state1,
		const size_t                             state2,
		const std::unordered_map<size_t,size_t>& tmp);


	static void completeSymmetricIndex(
		std::unordered_map<size_t,size_t>&         result,
		const std::unordered_set<size_t>&          usedStates);
};

#endif
