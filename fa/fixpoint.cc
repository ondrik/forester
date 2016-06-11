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

// Standard library headers
#include <ostream>

// Code Listener headers
#include <cl/storage.hh>

// Forester headers
#include "abstraction.hh"
#include "config.h"
#include "executionmanager.hh"
#include "fixpoint.hh"
#include "folding.hh"
#include "forestautext.hh"
#include "normalization.hh"
#include "regdef.hh"
#include "splitting.hh"
#include "streams.hh"
#include "ufae.hh"
#include "utils.hh"
#include "virtualmachine.hh"

// anonymous namespace
namespace
{
struct ExactTMatchF
{
	bool operator()(
		const TreeAut::Transition&  t1,
		const TreeAut::Transition&  t2)
	{
		return TreeAut::GetSymbol(t1) == TreeAut::GetSymbol(t2);
	}
};

std::vector<size_t> findSelectors(
		const FAE&                    fae,
		const TreeAut::Transition&    t)
{
	std::vector<size_t> index;

    for (const AbstractBox* aBox : TreeAut::GetSymbol(t)->getNode())
    {
        if (aBox->isBox())
        {
            index.push_back(aBox->getOrder());
        }
	}

	return index;
}

struct SmartTMatchF
{
	bool operator()(
		const TreeAut::Transition&  t1,
		const TreeAut::Transition&  t2)
	{
        label_type l1 = TreeAut::GetSymbol(t1);
        label_type l2 = TreeAut::GetSymbol(t2);
		if (l1->isNode() && l2->isNode())
		{
			if (!FA_ALLOW_STACK_FRAME_ABSTRACTION)
			{
				if ((static_cast<const TypeBox *>(l1->nodeLookup(-1).aBox))->getName().find("__@") == 0)
				{
					return l1 == l2;
				}
			}
			return l1->getTag() == l2->getTag();
		}

		return l1 == l2;
	}
};

class SmarterTMatchF
{
private:  // data members

	const FAE& fae_;

public:   // methods

	SmarterTMatchF(const FAE& fae) :
		fae_(fae)
	{ }

	bool operator()(
		const TreeAut::Transition&  t1,
		const TreeAut::Transition&  t2)
	{
        label_type l1 = TreeAut::GetSymbol(t1);
        label_type l2 = TreeAut::GetSymbol(t2);
		if (!l1->isNode() || !l2->isNode())
			return l1 == l2;

		if (l1->getTag() != l2->getTag())
			return false;

		if (&t1.GetChildren() == &t2.GetChildren())
			return true;

		if (t1.GetChildrenSize() != t2.GetChildrenSize())
			return false;

		for (size_t i = 0; i < t1.GetChildrenSize(); ++i)
		{
			size_t s1 = t1.GetNthChildren(i), s2 = t2.GetNthChildren(i), ref;

			if (s1 == s2)
				continue;

			if (FA::isData(s1))
			{
				if (!this->fae_.getRef(s1, ref))
					return false;

				if (FA::isData(s2))
					return false;
			} else
			{
				if (FA::isData(s2) && !fae_.getRef(s2, ref))
					return false;
			}
		}

		return true;
	}
};

struct CompareVariablesF
{
	bool operator()(
		const FAE&            fae,
		size_t                i,
		const TreeAut&        ta1,
		const TreeAut&        ta2)
	{
		VirtualMachine vm(fae);

		bool isFixedComp = true;
		for (size_t j = 0; j < FIXED_REG_COUNT; ++j)
		{
			if (i == vm.varGet(j).d_ref.root)
			{
				isFixedComp = false;
				break;
			}
		}

		if (isFixedComp)
		{
			return true;
		}

		const TreeAut::Transition& t1 = ta1.getAcceptingTransition();
		const TreeAut::Transition& t2 = ta2.getAcceptingTransition();
		return (TreeAut::GetSymbol(t1) == TreeAut::GetSymbol(t2)) &&
                (t1.GetChildren() == t2.GetChildren());
	}
};

struct FuseNonFixedF
{
	bool operator()(size_t root, const FAE* fae)
	{
		// Preconditions
		assert(nullptr != fae);

		VirtualMachine vm(*fae);

		for (size_t i = 0; i < FIXED_REG_COUNT; ++i)
		{
			if (root == vm.varGet(i).d_ref.root)
			{
				return false;
			}
		}

		return true;
	}
};


/**
 * @brief  Reorders components into the canonical order
 *
 * This function reorders the components of the FA @p fae @e without @e merging
 * them together.
 *
 * @param[in]      state  The state of the symbolic execution
 * @param[in,out]  fae    The forest automaton to be reordered
 */
void reorder(
	const SymState*   state,
	FAE&              fae)
{
	fae.unreachableFree();
	Normalization::normalizeWithoutMerging(fae, state);

	FA_DEBUG_AT(3, "after reordering: " << std::endl << fae);
}

bool testInclusion(
	FAE&                           fae,
	TreeAut&                       fwdConf,
	UFAE&                          fwdConfWrapper)
{
	TreeAut ta = TreeAut::createTAWithSameTransitions(fwdConf);

	Index<size_t> index;

	fae.unreachableFree();

	fwdConfWrapper.fae2ta(ta, index, fae);

	if (TreeAut::subseteq(ta, fwdConf))
	{
		return true;
	}

	fwdConfWrapper.join(ta, index);

	ta.clear();

	fwdConf.minimized(ta);
	fwdConf = ta;

	return false;
}

struct CopyNonZeroRhsF
{
	bool operator()(const TreeAut::Transition* transition) const
	{
		return transition->GetParent() != 0;
	}
};


void getCandidates(
	std::set<size_t>&               candidates,
	const FAE&                      fae)
{
	std::unordered_map<
		std::vector<std::pair<int, size_t>>,
		std::set<size_t>,
		boost::hash<std::vector<std::pair<int, size_t>>>
	> partition;

	for (size_t i = 0; i < fae.getRootCount(); ++i)
	{
		std::vector<std::pair<int, size_t>> tmp;

		fae.connectionGraph.getRelativeSignature(tmp, i);

		partition.insert(std::make_pair(tmp, std::set<size_t>())).first->second.insert(i);
	}

	candidates.clear();

	for (auto& tmp : partition)
	{
		if (tmp.second.size() > 1)
			candidates.insert(tmp.second.begin(), tmp.second.end());
	}
}

	std::vector<std::shared_ptr<const TreeAut>> getEmptyTrees(
			const FAE&            fwdFAE,
			const FAE&            bwdFAE)
	{
		assert(fwdFAE.getRootCount() == bwdFAE.getRootCount());
		FA_DEBUG_AT(1, "empty input fwd " << fwdFAE);
		FA_DEBUG_AT(1, "empty input bwd " << bwdFAE);

		std::vector<std::shared_ptr<const TreeAut>> res;

		for (size_t i = 0; i < fwdFAE.getRootCount(); ++i)
		{
			if (bwdFAE.getRoot(i) == nullptr && fwdFAE.getRoot(i) == nullptr)
			{
				  continue;
			}
			else if (bwdFAE.getRoot(i) == nullptr && fwdFAE.getRoot(i) != nullptr)
			{
				res.push_back(fwdFAE.getRoot(i));
				continue;
			}
			else if (bwdFAE.getRoot(i) != nullptr && fwdFAE.getRoot(i) == nullptr)
			{
				res.push_back(bwdFAE.getRoot(i));
				continue;
			}

			TreeAut isectTA = TreeAut::intersectionBU(
					*(fwdFAE.getRoot(i)),*(bwdFAE.getRoot(i)));
			std::shared_ptr<TreeAut> finalIsectTA = std::shared_ptr<TreeAut>(new TreeAut());

			isectTA.uselessAndUnreachableFree(*finalIsectTA);
			FA_DEBUG_AT(1, "empty " << isectTA);

			if (finalIsectTA->areTransitionsEmpty())
			{
				res.push_back(bwdFAE.getRoot(i));
			}
		}


		return res;
	}
} // namespace


size_t FixpointBase::fold(
		const std::shared_ptr<FAE>&       fae,
		std::set<size_t>&                 forbidden,
		SymState::AbstractionInfo&        ainfo)
{
	ainfo.iterationToFoldedRoots_[ainfo.abstrIteration_] =
			SymState::BoxesAtRoot(Folding::fold(*fae, boxMan_, forbidden));
	++ainfo.abstrIteration_;

	return ainfo.iterationToFoldedRoots_.at(ainfo.abstrIteration_ - 1).size();
}



SymState* FixpointBase::reverseAndIsect(
	ExecutionManager&                      execMan,
	const SymState&                        fwdPred,
	const SymState&                        bwdSucc) const
{
	(void)fwdPred;
	SymState* tmpState = execMan.copyState(bwdSucc);
	const SymState::AbstractionInfo& ainfo = fwdPred.GetAbstractionInfo();

	assert(ainfo.iterationToFoldedRoots_.size() - ainfo.faeAtIteration_.size() <= 1 || boxMan_.size() == 0);
	for (int i = (ainfo.abstrIteration_ - 1); i >= 0; --i)
	{
		assert(i < ainfo.iterationToFoldedRoots_.size() || boxMan_.size() == 0);

		if (i < ainfo.iterationToFoldedRoots_.size())
		{
			for (const auto &rootToBoxes : ainfo.iterationToFoldedRoots_.at(i))
			{
				for (const auto &boxes : rootToBoxes.second)
				{
					std::shared_ptr<FAE> fae = std::shared_ptr<FAE>(new FAE(*(tmpState->GetFAE())));
					if (fae->getRootCount() == 0)
					{
						return tmpState;
					}

					std::vector<FAE *> unfolded;
					Splitting splitting(*fae);
					splitting.isolateSet(
							unfolded,
							rootToBoxes.first,
							0,
							findSelectors(*fae, fae->getRoot(rootToBoxes.first)->getAcceptingTransition()));

		//			int i = rootToBoxes.first;
		//			for (; i >= 0; --i)
		//			{
		//				if (i >= fae->getRootCount() || fae->getRoot(i) == nullptr)
		//				{
		//					continue;
		//				}
		//				else if (fae->getRoot(i) != nullptr)
		//				{
		//					break;
		//				}
		//			}

		//			if (rootToBoxes.first < fae->getRootCount())
		//			{
		//				for (const auto fs : fae->getRoot(i)->getFinalStates())
		//				{
		//					const auto &ta = fae->getRoot(i);
		//					for (auto it = ta->begin(fs); it != ta->end(fs); ++it)
		//					{
		//						splitting.isolateSet(
		//								unfolded,
		//								rootToBoxes.first,
		//								0,
		//								findSelectors(*fae, *it /*fae->getRoot(i)->getAcceptingTransition()*/));
		//					}
		//				}
		//			}
					//fae->getType(rootToBoxes.first)->getSelectors());
					assert(unfolded.size() == 1);
					fae = std::shared_ptr<FAE>(unfolded.at(0));
					try
					{
						assert(fae->getRoot(rootToBoxes.first)->getAcceptingTransition().GetParent() == boxes.first);
						Unfolding(*fae).unfoldBox(rootToBoxes.first, boxes.second);
					}
					catch (std::runtime_error e)
					{
						;
					}
					tmpState->SetFAE(fae);
				}
			}
		}

		if (ainfo.faeAtIteration_.count(i))
		{
			SymState fwd;
			fwd.init(*tmpState);
			fwd.SetFAE(ainfo.faeAtIteration_.at(i));

			tmpState->SetFAE(tmpState->newNormalizedFAE());
			SymState bwdBackup;
			bwdBackup.init(*tmpState);

			fwd.SetFAE(fwd.newNormalizedFAE());
			tmpState->Intersect(fwd);

            if (tmpState->GetFAE()->Empty())
            {
				SymState fwdp;
				fwdp.init(fwdPred);
				SymState bwdp;
				bwdp.init(bwdSucc);
				//tmpState->SetPredicates(FI_abs::learnPredicates(&fwdp, &bwdp));
				if (!(ainfo.abstrIteration_ > 0 || (FAE::subseteq(*fwd.GetFAE(), *fwdp.GetFAE())
					  && FAE::subseteq(*fwdp.GetFAE(), *fwd.GetFAE()))))
				{
					assert(false);
				}
				tmpState->SetPredicates(FI_abs::learnPredicates(&fwd, &bwdBackup));
                return tmpState;
            }
		}
		else
		{
			// This is possible in the last iteration of reversion
			// of folding and abstraction. Because just one fold can
			// be done before the abstraction loop.
			assert(i <= 1);
		}
	}

	// perform intersection
	SymState bwdBackup;
	bwdBackup.init(*tmpState);
	tmpState->Intersect(fwdPred);
	if (tmpState->GetFAE()->Empty())
    {
		SymState fwdState;
		fwdState.init(fwdPred);
        tmpState->SetPredicates(FI_abs::learnPredicates(&fwdState, &bwdBackup));
    }

	FA_DEBUG_AT(1, "Executing !!VERY!! suspicious reverse operation FixpointBase");
	return tmpState;
}


std::vector<std::shared_ptr<const TreeAut>> FI_abs::learnPredicates(
		SymState *fwdState,
		SymState *bwdState)
{
	FA_DEBUG_AT(1, "BWD " << *bwdState);
	std::shared_ptr<FAE> normFAEBwd = bwdState->newNormalizedFAE();
	std::shared_ptr<FAE> normFAEFwd = fwdState->newNormalizedFAE();

	if (normFAEBwd->getRootCount() < FIXED_REG_COUNT ||
			(normFAEBwd->getRootCount() == FIXED_REG_COUNT &&
			normFAEBwd->getRoot(1) == nullptr))
	{
		return std::vector<std::shared_ptr<const TreeAut>>(
			normFAEBwd->getRoots().begin(),
			normFAEBwd->getRoots().end());
	}

	if (normFAEFwd->getRootCount() != normFAEBwd->getRootCount())
		FA_DEBUG_AT(1, "FWD " << *normFAEFwd << '\n' << "BWD " << *normFAEBwd << '\n');
	assert(normFAEFwd->getRootCount() == normFAEBwd->getRootCount());

	std::vector<std::shared_ptr<const TreeAut>> predicate = getEmptyTrees(*normFAEFwd, *normFAEBwd);

	if (predicate.empty())
	{ // no predicates found, brutal refinement
		assert(false);
		predicate.insert(predicate.end(),
				normFAEBwd->getRoots().begin(), normFAEBwd->getRoots().end());
	}

	assert(!predicate.empty());

	return predicate;
}


void FI_abs::storeFaeToAccs(
		FAE &fae,
		const bool startFromZero)
{
	for (size_t i = 0; i < fae.getRootCount(); ++i)
    {
        if (i <= 1)
        {
            continue;
        }

        std::vector<size_t> index(fae.getRootCount(), static_cast<size_t>(-1));
        size_t start = startFromZero ? 0 : i;
        index[i] = start++;

        for (auto& cutpointInfo : fae.connectionGraph.data.at(i).signature)
        {
            if (cutpointInfo.root == i)
            {
                continue;
            }
            else
            {
                index[cutpointInfo.root] = start++;
            }
        }

        const auto ta = fae.getRoot(i);

        auto tmp = std::unique_ptr<TreeAut>(&fae.unique(*accs_.allocTA(), *ta));
        std::unique_ptr<TreeAut> finalTa = std::unique_ptr<TreeAut>(accs_.allocTA());
        accs_.relabelReferences(*finalTa, *tmp, index);
        TreeAut::disjointUnion(*accs_.getRoot(0), *finalTa);
    }
}


void FI_abs::strongFusion(
		FAE&            fae)
{
	auto renamedAccs = std::unique_ptr<TreeAut>(&fae.unique(*accs_.allocTA(), *accs_.getRoot(0), false));
    for (size_t i = 0; i < fae.getRootCount(); ++i)
    {
        //faeTemp.appendRoot(fae.allocTA());
        std::vector<size_t> index;
        index.push_back(i);

        for (auto &cutpointInfo : fae.connectionGraph.data.at(i).signature)
        {
            if (cutpointInfo.root == i)
            {
                continue;
            }
            else
            {
                index.push_back(cutpointInfo.root);
            }
        }

        std::unique_ptr<TreeAut> finalTa = std::unique_ptr<TreeAut>(fae.allocTA());
        accs_.relabelReferences(*finalTa, *renamedAccs, index);

        std::shared_ptr<TreeAut> tempTa = std::shared_ptr<TreeAut>(
                TreeAut::allocateTAWithSameFinalStates(*fae.getRoot(i)));
        TreeAut::disjointUnion(*tempTa, *finalTa, false);
        fae.setRoot(i, tempTa);
     }
}


void FI_abs::weakFusion(
	FAE&                 fae)
{
    // merge fixpoint
    std::vector<FAE*> tmp;

    ContainerGuard<std::vector<FAE*>> g(tmp);

    FAE::loadCompatibleFAs(
        tmp, // result
        fwdConf_,
        ta_,
        boxMan_,
        fae,
        0,
        CompareVariablesF()
    );

    for (size_t i = 0; i < tmp.size(); ++i)
    {
        FA_DEBUG_AT(3, "accelerator " << std::endl << *tmp[i]);
    }

    fae.fuse(tmp, FuseNonFixedF());
    FA_DEBUG_AT(1, "fused " << std::endl << fae);
}


void FI_abs::abstract(
	FAE&                 fae)
{
	fae.unreachableFree();

	FA_DEBUG_AT(3, "before abstraction: " << std::endl << fae);

	// FAE faeTemp = FAE(ta_, boxMan_);
	if (FA_FUSION_ENABLED)
	{
		strongFusion(fae);
		/*
		faeTemp.connectionGraph = fae.connectionGraph;
		FA_DEBUG_AT(1, "Zumped " << faeTemp << '\n');
		 */

		//weakFusion(fae);
	}

	// abstract
	Abstraction abstraction(fae);
	//Abstraction abstractionTemp(faeTemp);

	if ((FA_START_WITH_PREDICATE_ABSTRACTION || !predicates_.empty()) && FA_USE_PREDICATE_ABSTRACTION)
	{	// for predicate abstraction
		for (size_t i = 0; i < fae.getRootCount(); ++i)
		{
			abstraction.predicateAbstraction(i, this->getPredicates());
			//abstractionTemp.predicateAbstraction(i, this->getPredicates());
		}
	}
	else
	{	// for finite height abstraction

		// the roots that will be excluded from abstraction
		std::vector<bool> excludedRoots(fae.getRootCount(), false);
		for (size_t i = 0; i < FIXED_REG_COUNT; ++i)
		{
			excludedRoots[VirtualMachine(fae).varGet(i).d_ref.root] = true;
		}

		for (size_t i = 0; i < fae.getRootCount(); ++i)
		{
			if (!excludedRoots[i])
			{
				abstraction.heightAbstraction(i, FA_ABS_HEIGHT, SmartTMatchF());
//				abstractionTemp.heightAbstraction(i, FA_ABS_HEIGHT, SmartTMatchF());
//				abstraction.heightAbstraction(i, FA_ABS_HEIGHT, SmarterTMatchF(fae));
			}
		}
	}

	/*
	if (!FAE::subseteq(fae, faeTemp))
	{
		std::cerr << "SMALL " << fae;
		std::cerr << "BIG " << faeTemp;
		assert(false);
	}
	 */
	/*
	if (FAE::subseteq(faeTemp, fae) && FAE::subseteq(fae, faeTemp))
	{
		//std::cerr << "FAIL\n";
	}
	else
	{
		//std::cerr << "HIT\n";
	}
	*/

	FA_DEBUG_AT(1, "after abstraction: " << std::endl << fae);
}


// FI_abs
void FI_abs::execute(ExecutionManager& execMan, SymState& state)
{
	auto& ainfo = state.GetAbstractionInfo();
	ainfo.clear();

	std::shared_ptr<FAE> fae = std::shared_ptr<FAE>(new FAE(*(state.GetFAE())));

	fae->updateConnectionGraph();

	std::set<size_t> forbidden;
#if FA_ALLOW_FOLDING

	// reorder components into the canonical form (no merging!)
	reorder(&state, *fae);
	if (!boxMan_.boxDatabase().empty())
	{	// in the case there are some boxes, try to fold immediately before
		// normalization
		for (size_t i = 0; i < FIXED_REG_COUNT; ++i)
		{
			forbidden.insert(VirtualMachine(*fae).varGet(i).d_ref.root);
		}

		// fold already discovered boxes
		try
		{
			this->fold(fae, forbidden, ainfo);
		}
		catch (std::runtime_error&)
		{
			throw ProgramError("Abstraction leads to inconsisten selector map", &state);
		}
	}

	Folding::learn2(*fae, boxMan_, Normalization::computeForbiddenSet(*fae));
#endif
	forbidden = Normalization::computeForbiddenSet(*fae);

	Normalization::normalize(*fae, &state, forbidden, true);

	ainfo.faeAtIteration_[(ainfo.abstrIteration_ == 0) ? ainfo.abstrIteration_++ : ainfo.abstrIteration_-1] =
			std::shared_ptr<FAE>(new FAE(*fae));
	abstract(*fae);
#if FA_ALLOW_FOLDING
	Folding::learn1(*fae, boxMan_, Normalization::computeForbiddenSet(*fae));

	if (boxMan_.boxDatabase().size())
	{
		FAE old(*fae, boxMan_);

		try
		{
			do
			{
				forbidden = Normalization::computeForbiddenSet(*fae);

				Normalization::normalize(*fae, &state, forbidden, true);

				ainfo.faeAtIteration_[ainfo.abstrIteration_] = std::shared_ptr<FAE>(
						new FAE(*fae));

				abstract(*fae);

				forbidden.clear();
				for (size_t i = 0; i < FIXED_REG_COUNT; ++i)
				{
					forbidden.insert(VirtualMachine(*fae).varGet(i).d_ref.root);
				}

				old = *fae;
			} while (this->fold(fae, forbidden, ainfo) && !FAE::subseteq(*fae, old));
		}
		catch (std::runtime_error&)
		{
			throw ProgramError("Abstraction leads to inconsistent selector map", &state);
		}

	}
#endif
	// test inclusion
	if (testInclusion(*fae, fwdConf_, fwdConfWrapper_))
	{
		FA_DEBUG_AT(1, "hit " << fwdConf_);
		FA_DEBUG_AT(1, "hit " << *fae);

		execMan.pathFinished(&state);
	} else
	{
		FA_DEBUG_AT_MSG(1, &this->insn()->loc, "extending fixpoint\n" << *fae << "\n");
		storeFaeToAccs(*fae);
		storeFaeToAccs(*fae, true);

		SymState* tmpState = execMan.createChildState(state, next_);
		tmpState->SetFAE(fae);
		execMan.enqueue(tmpState);
	}
	FA_DEBUG_AT_MSG(1, &this->insn()->loc, "AbsInt end " << *fae);
}

// FI_fix
void FI_fix::execute(ExecutionManager& execMan, SymState& state)
{

	std::shared_ptr<FAE> fae = std::shared_ptr<FAE>(new FAE(*(state.GetFAE())));

	fae->updateConnectionGraph();

	std::set<size_t> forbidden;
#if FA_ALLOW_FOLDING
	reorder(&state, *fae);

	if (!boxMan_.boxDatabase().size())
	{
		for (size_t i = 0; i < FIXED_REG_COUNT; ++i)
		{
			forbidden.insert(VirtualMachine(*fae).varGet(i).d_ref.root);
		}

		this->fold(fae, forbidden, state.GetAbstractionInfo());
	}
#endif
	forbidden = Normalization::computeForbiddenSet(*fae);

	Normalization::normalize(*fae, &state, forbidden, true);
#if FA_ALLOW_FOLDING
	if (boxMan_.boxDatabase().size())
	{
		forbidden.clear();

		for (size_t i = 0; i < FIXED_REG_COUNT; ++i)
		{
			forbidden.insert(VirtualMachine(*fae).varGet(i).d_ref.root);
		}

		while (this->fold(fae, forbidden, state.GetAbstractionInfo()))
		{
			forbidden = Normalization::computeForbiddenSet(*fae);

			Normalization::normalize(*fae, &state, forbidden, true);

			forbidden.clear();

			for (size_t i = 0; i < FIXED_REG_COUNT; ++i)
			{
				forbidden.insert(VirtualMachine(*fae).varGet(i).d_ref.root);
			}
		}
	}
#endif
	// test inclusion
	if (testInclusion(*fae, fwdConf_, fwdConfWrapper_))
	{
		FA_DEBUG_AT(3, "hit");

		execMan.pathFinished(&state);
	} else
	{
		FA_DEBUG_AT_MSG(1, &this->insn()->loc, "extending fixpoint\n" << *fae);

		SymState* tmpState = execMan.createChildState(state, next_);
		tmpState->SetFAE(fae);

		execMan.enqueue(tmpState);
	}
}

void FI_abs::printDebugInfoAboutPredicates() const
{
    FA_DEBUG_AT(1, "Number of predicates " <<
            this->getPredicates().size() << " of " << this->insn());
    std::ostringstream os;
    this->printPredicates(os);
    FA_DEBUG_AT(1, os.str());
}

void FI_abs::printPredicates(std::ostringstream& os) const
{
	for (const std::shared_ptr<const TreeAut>& pred : this->getPredicates())
	{
		os << "\n---------------------------------------------------\n"
			<< *this << this->insn();

		const CodeStorage::Insn* clInsn = this->insn();
		assert(nullptr != clInsn);
		assert(nullptr != clInsn->bb);
		if (clInsn->bb->front() == clInsn)
		{
			os << " (" << clInsn->bb->name() << ")";
		}

		if (pred != nullptr)
		{
			os << ": " << *this << "\n" << *pred << '\n';
		}
		else
		{
			os << ": " << *this << "\n" << "nullptr" << '\n';
		}
	}
}
