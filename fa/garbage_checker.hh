/*
 * Copyright (C) 2011 Jiri Simacek
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

#ifndef GARBAGE_CHECKER_H
#define GARBAGE_CHECKER_H

// forester headers
#include "forestautext.hh"
#include "symstate.hh"

class GarbageChecker
{
private:

	/**
	 * @brief  Traverse the forest automaton and mark visited components
	 *
	 * This method traverses the forest automaton and marks visited components
	 * in the passed bitmap.
	 *
	 * @param[out]  visited  Bitmap into which visited components are marked
	 */
	static void traverse(
		const FAE&                        fae,
		std::vector<bool>&                visited);


	static void checkGarbage(
		const FAE&                        fae,
		const SymState*                   state,
		const std::vector<bool>&          visited,
		std::unordered_set<size_t>&       unvisited,
		const bool                        endCheck=false);


	static void removeGarbage(
		FAE&                               fae,
		const std::unordered_set<size_t>&  unvisited);


	/**
	 * @brief  Checks for garbage
	 *
	 * Checks for garbage, i.e. components unreachable from program variables.
	 *
	 * @todo  This method fails for backward-only reachable components
	 */
	static void check(
		const FAE&                       fae,
		const SymState*                  state);

public:

	/**
	 * Methods iterates through forest automaton @p fae and finds
	 * all accessible and non-accessible roots in the current @p state
	 * (so whether it is possible to access the root by traversal from
	 * a TA pointed by some variable in the state).
	 * The non-accessible are then marked as a garbage and removed.
	 *
	 * @param[in,out] fae FA where the garbage is searched
	 * @param[in] state Current symbolic state
	 * @param[in] endCheck
	 * @param[out] List of garbage roots.
	 */
	static void checkAndRemoveGarbage(
		FAE&                             fae,
		const SymState*                  state,
		const bool                       endCheck,
		std::unordered_set<size_t>&      unvisited);

	/**
	 * Just an interface to checkAndRemoveGargabe with the forth parameter.
	 * This method creates an vector where the removed garbage nodes are stored
	 * and then calls the core method providing the vector.
	 */
	static void checkAndRemoveGarbage(
		FAE&                             fae,
		const SymState*                  state,
		const bool                       endCheck);

	/**
	 * This function checks and removes garbage but does not perform
	 * traversal over @p fae.
	 */
	static void nontraverseCheckAndRemoveGarbage(
		FAE&                                fae,
		const SymState*                     state,
		const std::vector<bool>&            visited);
};

#endif
