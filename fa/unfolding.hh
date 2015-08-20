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
#include <stdexcept>
#include <algorithm>

// Forester headers
#include "forestautext.hh"
#include "abstractbox.hh"
#include "utils.hh"

class Unfolding
{
	FAE& fae;

protected:
	void boxMerge(
		TreeAut&                       dst,
		const TreeAut&                 src,
		const TreeAut&                 boxRoot,
		const Box*                     box,
		const std::vector<size_t>&     rootIndex);

private:
    void getChildrenAndLabelFromBox(
		const Box*                         box,
        const TreeAut::Transition&         transition,
        std::vector<size_t>&               children,
        std::vector<const AbstractBox*>&   label);

	void initRootRefIndex(
		std::vector<size_t>&                   index,
		const Box*                             box,
		const TreeAut::Transition&             t);

	void substituteOutputPorts(
		const std::vector<size_t>&    index,
		const size_t                  root,
		const Box*                    box);

	void substituteInputPorts(
		const std::vector<size_t>&    index,
		const Box*                    box);

public:
	Unfolding(FAE& fae) : fae(fae) {}

	void unfoldBox(const size_t root, const Box* box);
	void unfoldBoxes(const size_t root, const std::set<const Box*>& boxes);
};

#endif
