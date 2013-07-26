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

// Forester headers
#include "unfolding.hh"


void Unfolding::boxMerge(
	TreeAut&                       dst,
	const TreeAut&                 src,
	const TreeAut&                 boxRoot,
	const Box*                     box,
	const std::vector<size_t>&     rootIndex)
{
	TreeAut tmp(*fae_.backend);
	TreeAut tmp2(*fae_.backend);

	// copy 'src' and 'boxRoot' (with relabelled references) into 'dst'
	fae_.relabelReferences(tmp, boxRoot, rootIndex);
	fae_.unique(tmp2, tmp);
	src.copyTransitions(dst, TreeAut::NonAcceptingF(src));
	tmp2.copyTransitions(dst, TreeAut::NonAcceptingF(tmp2));
	dst.addFinalStates(tmp2.getFinalStates());

	for (size_t state : src.getFinalStates())
	{
		for (TreeAut::iterator i = src.begin(state); i != src.end(state, i); ++i)
		{
			std::vector<size_t> lhs;
			std::vector<const AbstractBox*> label;
			size_t lhsOffset = 0;
			if (nullptr != box)
			{	// in the case a box is to be substituted
				bool found = false;
				for (const AbstractBox* aBox : i->label()->getNode())
				{
					if (!aBox->isStructural())
					{	// in the case the box is not structural
						label.push_back(aBox);
						continue;
					}

					const StructuralBox* b = static_cast<const StructuralBox*>(aBox);
					if (b != static_cast<const StructuralBox*>(box))
					{	// this box is not interesting
						for (size_t k = 0; k < b->getArity(); ++k, ++lhsOffset)
						{
							lhs.push_back(i->lhs()[lhsOffset]);
						}

						label.push_back(b);
					}
					else
					{	// in case the box is to be substituted
						lhsOffset += box->getArity();

						if (found)
						{
							assert(false);
						}

						found = true;
					}
				}

				if (!found)
				{
					assert(false);
				}
			}
			else
			{	// in the case no box is to be substituted
				lhs = i->lhs();
				label = i->label()->getNode();
			}

			for (TreeAut::iterator j = tmp2.accBegin(); j != tmp2.accEnd(j); ++j)
			{	// for all accepting transitions of the 'boxRoot' TA, combine them with
				// the transition of the 'src' TA and insert into 'dst'
				std::vector<size_t> lhs2 = lhs;
				std::vector<const AbstractBox*> label2 = label;
				lhs2.insert(lhs2.end(), j->lhs().begin(), j->lhs().end());
				label2.insert(label2.end(), j->label()->getNode().begin(), j->label()->getNode().end());
				FA::reorderBoxes(label2, lhs2);
				dst.addTransition(lhs2, fae_.boxMan->lookupLabel(label2), j->rhs());
			}
		}
	}
}


void Unfolding::unfoldBox(
	size_t              root,
	const Box*          box)
{
	// Preconditions
	assert(root < fae_.getRootCount());
	assert(nullptr != fae_.getRoot(root));
	assert(nullptr != box);

	const TT<label_type>& t = fae_.getRoot(root)->getAcceptingTransition();

	size_t lhsOffset = 0;
	std::vector<size_t> index = { root };

	// construct 'index'
	for (const AbstractBox* aBox : t.label()->getNode())
	{	// for all boxes in the transition
		if (static_cast<const AbstractBox*>(box) != aBox)
		{	// in the case the box is not interesting, just continue
			lhsOffset += aBox->getArity();

			continue;
		}

		// if the box is interesting
		for (size_t j = 0; j < box->getArity(); ++j)
		{
			const Data& data = fae_.getData(t.lhs()[lhsOffset + j]);

			if (data.isUndef())
			{
				index.push_back(static_cast<size_t>(-1));
			}
			else
			{
				assert(data.isRef());
				index.push_back(data.d_ref.root);
			}
		}

		break;
	}

	// merge the box into 'ta'
	std::shared_ptr<TreeAut> ta = std::shared_ptr<TreeAut>(fae_.allocTA());
	this->boxMerge(*ta, *fae_.getRoot(root), *box->getOutput(), box, index);

	fae_.setRoot(root, ta);
	fae_.connectionGraph.invalidate(root);

	if (!box->getInput())
	{	// in the case there is only a single TA in the box
		return;
	}

	assert(box->getInputIndex() < index.size());

	size_t aux = index[box->getInputIndex() + 1];

	assert(static_cast<size_t>(-1) != aux);
	assert(aux < fae_.getRootCount());

	TreeAut tmp(*fae_.backend);

	fae_.getRoot(aux)->unfoldAtRoot(tmp, fae_.freshState());
	fae_.setRoot(aux, std::shared_ptr<TreeAut>(fae_.allocTA()));

	this->boxMerge(*fae_.getRoot(aux), tmp, *box->getInput(), nullptr, index);

	fae_.connectionGraph.invalidate(aux);
}


void Unfolding::unfoldStraightBoxes()
{
	typedef typename TreeAut::Transition Transition;

	for (size_t i = 0; i < fae_.getRootCount(); ++i)
	{
		if (nullptr != fae_.getRoot(i))
		{
			const TreeAut& ta = *fae_.getRoot(i);

			std::unordered_set<const Transition*> unboundedOccur =
				ta.getUnboundedOccurTrans();

			for (const Transition& trans : ta)
			{	// traverse all transitions of 'ta'
				if (unboundedOccur.cend() == unboundedOccur.find(&trans))
				{	// if the transition does not appear unboundedly many times
					if (trans.label()->isNode())
					{	// if the transition represents a memory node
						for (const AbstractBox* aBox : trans.label()->getNode())
						{	// check whether the transition contains a box
							assert(nullptr != aBox);
							if (aBox->isType(box_type_e::bBox))
							{
								FA_NOTE("We wish to perform unfolding of " << trans);
								break;
							}
						}
					}
				}
			}
		}
	}
}
