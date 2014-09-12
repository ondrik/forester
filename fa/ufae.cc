/*
 * Copyright (C) 2010 Jiri Simacek
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

#include "ufae.hh"

    /**
	 * @brief  Clears the structure to the initial state
	 */
	void UFAE::clear()
	{
		backend_.addFinalState(0);
		stateOffset_ = 1;
	}

	/**
	 * @brief  Retrievs the offset of states
	 *
	 * @returns  Offset of states from the original forest automaton
	 */
	size_t UFAE::getStateOffset() const
	{
		return stateOffset_;
	}

	/**
	 * @brief  Sets the offset of states
	 *
	 * @param[in]  offset  The new offset of states
	 */
	void UFAE::setStateOffset(size_t offset)
	{
		stateOffset_ = offset;
	}

	TreeAut& UFAE::fae2ta(
		TreeAut&                   dst,
		Index<size_t>&             index,
		const FAE&                 src) const
	{
		dst.addFinalState(0);
		std::vector<Cursor<std::unordered_set<size_t>::const_iterator>> tmp;
		for (auto root : src.getRoots())
		{
			TreeAut::rename(
				dst,
				*root,
				FAE::RenameNonleafF(index, stateOffset_),
				false);

			assert(root->getFinalStates().size());
			tmp.push_back(Cursor<std::unordered_set<size_t>::const_iterator>(
				root->getFinalStates().begin(),
				root->getFinalStates().end()));
		}

		std::vector<size_t> lhs(tmp.size());
		label_type label = boxMan_.lookupLabel(tmp.size(), src.GetVariables());

		bool valid = true;
		while (valid)
		{
			for (size_t i = 0; i < lhs.size(); ++i)
			{
				lhs[i] = index[*tmp[i].curr] + stateOffset_;
			}
			dst.addTransition(lhs, label, 0);
			valid = false;

			for (auto it = tmp.begin(); !valid && it != tmp.end(); ++it)
			{
				valid = it->inc();
			}
		}
		return dst;
	}

	void UFAE::join(const TreeAut& src, const Index<size_t>& index)
	{
		TreeAut::disjointUnion(backend_, src, false);
		stateOffset_ += index.size();
	}

	void UFAE::adjust(const Index<size_t>& index)
	{
		stateOffset_ += index.size();
	}

