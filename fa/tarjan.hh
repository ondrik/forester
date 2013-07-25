/*
 * Copyright (C) 2013 Ondrej Lengal
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

#ifndef _TARJAN_HH_
#define _TARJAN_HH_

#include <unordered_set>

/**
 * @brief  Implementation of Tarjan's algorithm for SCCs
 *
 * Implementation of the Tarjan's algorithm for finding strongly connected
 * components in a directed graph. This modification finds only non-trivial
 * components, i.e. it discards SCCs that contain a single node without
 * a self-loop.
 *
 * Taken from
 * http://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
 *
 * \code{cpp}
 * algorithm tarjan is
 *   input: graph G = (V, E)
 *   output: set of strongly connected components (sets of vertices)
 * 
 *   index := 0
 *   S := empty
 *   for each v in V do
 *     if (v.index is undefined) then
 *       strongconnect(v)
 *     end if
 *   repeat
 * 
 *   function strongconnect(v)
 *     // Set the depth index for v to the smallest unused index
 *     v.index := index
 *     v.lowlink := index
 *     index := index + 1
 *     S.push(v)
 * 
 *     // Consider successors of v
 *     for each (v, w) in E do
 *       if (w.index is undefined) then
 *         // Successor w has not yet been visited; recurse on it
 *         strongconnect(w)
 *         v.lowlink := min(v.lowlink, w.lowlink)
 *       else if (w is in S) then
 *         // Successor w is in stack S and hence in the current SCC
 *         v.lowlink := min(v.lowlink, w.index)
 *       end if
 *     repeat
 * 
 *     // If v is a root node, pop the stack and generate an SCC
 *     if (v.lowlink = v.index) then
 *       start a new strongly connected component
 *       repeat
 *         w := S.pop()
 *         add w to current strongly connected component
 *       until (w = v)
 *       output the current strongly connected component
 *     end if
 *   end function
 *  \endcode
 */
template <class TreeAut>
class Tarjan
{
private:  // data types

	struct TarjanInfo
	{
		size_t index;
		size_t lowlink;
	};

public:   // data types

	/// Used to store an SCC
	typedef std::unordered_set<size_t> TStronglyCC;

	/// Used to store a partition
	typedef std::vector<TStronglyCC> TPartition;

private:  // data members

	/// Information about processed nodes
	std::unordered_map<size_t, TarjanInfo> tarjanStore_;

	/// The stack used in the algorithm
	std::vector<size_t> tarjanStack_;

	/// Counter for indexing nodes
	size_t glIndex_;

	/// The tree automaton
	const TreeAut& ta_;

	/// The partition
	TPartition& partition_;

private:  // methods

// REMOVE THE FOLLOWING MACROS!!!!!
#define _MSBM         ((~static_cast<size_t>(0)) >> 1)
#define _MSB          (~_MSBM)
#define _MSB_TEST(x)  (x & _MSB)
#define _MSB_GET(x)   (x & _MSBM)
#define _MSB_ADD(x)   (x | _MSB)

	/**
	 * @brief  Retrieves non-trivial strongly connected components
	 *
	 * Given a tree automaton @p ta_, this method adds all strongly connected
	 * components reachable from @p v into the @p parititon_ member field.
	 *
	 * @param[in]  v  The root of the search tree
	 *
	 * @returns  @p v.lowlink
	 */
	size_t strongconnect(size_t v)
	{
		TarjanInfo tmpInfo = {
			/* index */   glIndex_,
			/* lowlink */ glIndex_
		};
		++glIndex_;

		// set the 'index' and 'lowlink' of 'v'
		auto itBoolPair =	tarjanStore_.insert(std::make_pair(v, tmpInfo));
		assert(itBoolPair.second);      // make sure there was nothing before
		TarjanInfo& vInfo = itBoolPair.first->second;  // reference to the info field

		// push into the stack
		tarjanStack_.push_back(v);

		bool isSingleNodeNonTrivialSCC = false;

		for (typename TreeAut::Iterator it = ta_.begin(v); it != ta_.end(v, it); ++it)
		{	// traverse transitions going from 'v'
			const typename TreeAut::Transition& trans = *it;
			assert(trans.rhs() == v);

			for (size_t w : trans.lhs())
			{	// for all successors of 'v'
				if (v == w)
				{	// if there is a loop on 'v'
					isSingleNodeNonTrivialSCC = true;
				}
				else if (tarjanStore_.end() == tarjanStore_.find(w))
				{	// w.index is undefined
					size_t wLowlink = this->strongconnect(w);

					vInfo.lowlink = std::min(vInfo.lowlink, wLowlink);
				}
				else if (tarjanStack_.end() !=
					std::find(tarjanStack_.begin(), tarjanStack_.end(), w))
				{	// w.index is defined and w is in tarjanStack -> end of loop

					// TODO: the previous test might be done using a set containing
					// accessed states
					// TODO: try the previous test using the other way (rbegin to rend)

					auto it = tarjanStore_.find(w);
					assert(tarjanStore_.end() != it);
					const TarjanInfo& wInfo = it->second;

					vInfo.lowlink = std::min(vInfo.lowlink, wInfo.index);
				}
				else
				{	// otherwise w is a part of a disjoint SCC, then do nothing
				}
			}
		}

		if ((vInfo.index == vInfo.lowlink))
		{	// in the case 'v' is the root of an SCC
			if ((tarjanStack_.back()) == v && !isSingleNodeNonTrivialSCC)
			{	// if the SCC is trivial
				tarjanStack_.pop_back();
			}
			else
			{	// if the SCC is non-trivial, just skip it
				std::unordered_set<size_t> component;

				size_t w;
				do
				{	// until we pop all nodes of the SCC
					assert(!tarjanStack_.empty());

					w = tarjanStack_.back();
					tarjanStack_.pop_back();

					component.insert(w);
				} while (v != w);

				partition_.push_back(std::move(component));
			}
		}

		return vInfo.lowlink;
	}

public:   // methods

	/**
	 * @brief  The constructor
	 *
	 * @param[in]  ta         The tree automaton for which the algorithm will operate
	 * @param[out] partition  The partition into which the SCCs will be loaded
	 */
	Tarjan(
		const TreeAut&  ta,
		TPartition&     partition) :
		tarjanStore_(),
		tarjanStack_(),
		glIndex_(0),
		ta_(ta),
		partition_(partition)
	{ }

	/**
	 * @brief  Executes the algorithm
	 *
	 * Executes Tarjan's SCC algorithm on @p ta_ and fills @p partition_ with
	 * non-trivial strongly connected components.
	 */
	void operator()()
	{
		this->strongconnect(ta_.getFinalState());

		assert(tarjanStack_.empty());
	}
};

#endif

