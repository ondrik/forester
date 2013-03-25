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

#ifndef UNFOLDING_H
#define UNFOLDING_H

// Standard library headers
#include <vector>
#include <set>

// Forester headers
#include "forestautext.hh"
#include "abstractbox.hh"

class Unfolding
{
private:  // data members

	FAE& fae_;

protected:

	/**
	 * @brief  Inserts a tree automaton from a box into a higher level automaton
	 *
	 * This method merges the tree automaton @p boxRoot into the tree automaton @p
	 * src instead of the box @p box producing the tree automaton @p dst. The
	 * references in @p boxRoot are relabelled according to @p rootIndex. Note
	 * that the substitution only takes place under the @e root state of 'src'.
	 *
	 * @param[out]  dst        The result tree automaton
	 * @param[in]   src        The source tree automaton
	 * @param[in]   boxRoot    The tree automaton to be inserted into @p src
	 * @param[in]   box        The box that is to be substituted by @p boxRoot (or
	 *                         @p nullptr in case @p boxRoot is just added)
	 * @param[in]   rootIndex  The index renaming cutpoints in @p boxRoot
	 */
	void boxMerge(
		TreeAut&                       dst,
		const TreeAut&                 src,
		const TreeAut&                 boxRoot,
		const Box*                     box,
		const std::vector<size_t>&     rootIndex);

public:   // methods

	/**
	 * @brief  Unfolds an occurrence of a box in the accepting transition
	 *
	 * This method unfolds an occurrence of the box @p box in the accepting
	 * transition of the TA at index @p root.
	 *
	 * @param[in]  root  The index of the TA where @p box is to be unfolded
	 * @param[in]  box   The box to be unfolded
	 */
	void unfoldBox(
		size_t                          root,
		const Box*                      box);


	/**
	 * @brief  Unfolds occurrences of all boxes from a set in the root transition
	 *
	 * This method unfolds all occurrences of boxes from @p boxes in the accepting
	 * transition of the TA at index @p root.
	 *
	 * @param[in]  root   The index of the TA where @p boxes are to be unfolded
	 * @param[in]  boxes  The boxes to be unfolded
	 */
	void unfoldBoxes(
		size_t                          root,
		const std::set<const Box*>&     boxes)
	{
		for (const Box* box : boxes)
		{
			this->unfoldBox(root, box);
		}
	}


	/**
	 * @brief  Unfolds boxes which are not in a loop
	 *
	 * This method unfolds all boxes in the forest automaton which are not in
	 * a loop.
	 */
	void unfoldStraightBoxes();

public:

	Unfolding(FAE& fae) : fae_(fae) {}

};

#endif
