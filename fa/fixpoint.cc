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
#include <gmpxx.h>

// Forester headers
#include "abstraction.hh"
#include "executionmanager.hh"
#include "fixpoint.hh"
#include "folding.hh"
#include "regdef.hh"
#include "splitting.hh"
#include "virtualmachine.hh"

// anonymous namespace
namespace
{
using TreeAutVec = std::vector<std::shared_ptr<const TreeAut>>;

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
		const size_t                  root,
		const Box*                    box)
{
	std::vector<size_t> index;
	for (auto it = fae.getRoot(root)->accBegin(); it != fae.getRoot(root)->accEnd(); ++it)
	{
		const auto t = *it;
		for (const AbstractBox *aBox : TreeAut::GetSymbol(t)->getNode())
		{
			if (aBox->isBox() && aBox == box)
			{
				index.push_back(aBox->getOrder());
			}
		}
	}

	return index;
}

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
	FAE&              fae,
	Normalization::NormalizationInfo& normInfo)
{
	// TODO: maybe will need to revert this
	fae.unreachableFree();
	Normalization::normalizeWithoutMerging(fae, state, normInfo);

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


TreeAutVec getEmptyTrees(
        const FAE&            fwdFAE,
        const FAE&            bwdFAE)
{
    assert(fwdFAE.getRootCount() == bwdFAE.getRootCount());
    FA_DEBUG_AT(1, "empty input fwd " << fwdFAE);
    FA_DEBUG_AT(1, "empty input bwd " << bwdFAE);

    TreeAutVec res;

    for (size_t i = 0; i < fwdFAE.getRootCount(); ++i)
    {
        if (bwdFAE.getRoot(i) == nullptr && fwdFAE.getRoot(i) == nullptr)
        {
              continue;
        }
        else if (bwdFAE.getRoot(i) == nullptr && fwdFAE.getRoot(i) != nullptr)
        {
            // res.push_back(fwdFAE.getRoot(i));
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
            FA_DEBUG_AT(1, "new predicate " << *bwdFAE.getRoot(i));
            res.push_back(bwdFAE.getRoot(i));
        }
    }


    return res;
}


std::pair<bool, std::shared_ptr<FAE>> revertFolding(
		const SymState::BoxesAtRoot&      foldedRoots,
		const FAE&                        actFae,
		const SymState&                   state)
{
    std::shared_ptr<FAE> fae = std::shared_ptr<FAE>(new FAE(actFae));
    bool somethingUnfolded = false;
    bool skipped = false;

    std::vector<size_t> keys;
    for (const auto& item : foldedRoots)
    {
        keys.push_back(item.first);
    }
    std::sort(keys.begin(), keys.end());

    for (const auto& key : keys)
    {
        assert(foldedRoots.count(key));
        const auto &rootToBoxes = *foldedRoots.find(key);

        if (rootToBoxes.first >= fae->getRootCount() ||
                fae->getRoot(rootToBoxes.first) == nullptr)
        {
            skipped |= rootToBoxes.second.size() > 0;
            continue;
        }
        assert(rootToBoxes.first < fae->getRootCount());
        assert(fae->getRoot(rootToBoxes.first) != nullptr);

        for (const auto &boxes : rootToBoxes.second)
        {
            somethingUnfolded = true;
            if (fae->getRootCount() == 0)
            {
                return std::pair<bool, std::shared_ptr<FAE>>(somethingUnfolded, fae);
            }

            std::vector<FAE *> unfolded;

            Splitting splitting(*fae);
            splitting.isolateSet(
                    unfolded,
                    rootToBoxes.first,
                    0,
                    findSelectors(*fae, rootToBoxes.first, boxes.second));


			// assert(unfolded.size() == 1);
			assert(unfolded.size() <= 1);
			fae = std::shared_ptr<FAE>(unfolded.at(unfolded.size()-1));
			// break;
        }
		// break;
    }

    assert(!skipped || somethingUnfolded);

    return std::pair<bool, std::shared_ptr<FAE>>(somethingUnfolded, fae);
}


void revertAbstraction(
		SymState*   tmpState,
		const std::shared_ptr<FAE> faeAtIteration,
		const Normalization::NormalizationInfo* normInfo=nullptr)
{
	SymState fwd;
	fwd.init(*tmpState);
	fwd.SetFAE(faeAtIteration);

	SymState bwdBackup;
	bwdBackup.init(*tmpState);
	tmpState->SetFAE(tmpState->newNormalizedFAE());

	/* Top-down intersection
    tmpState->Intersect(fwd);
    if (tmpState->GetFAE()->Empty())
    {
        tmpState->AddPredicate(FI_abs::learnPredicates(&fwd, &bwdBackup));
		return;
    }
    */

	if (fwd.GetFAE()->getRootCount() != bwdBackup.GetFAE()->getRootCount())
	{
		fwd.SetFAE(fwd.newNormalizedFAE());
		bwdBackup.SetFAE(bwdBackup.newNormalizedFAE());
	}
	BUIntersection::BUProductResult buProductResult =
			BUIntersection::bottomUpIntersection(*fwd.GetFAE(), *bwdBackup.GetFAE());
	std::shared_ptr<FAE> newFae(new FAE(*bwdBackup.GetFAE()));

	for (const auto& ta : buProductResult.tas_)
	{
		if (ta == nullptr)
		{
			continue;
		}

		TreeAut tmp(*ta);
		if (tmp.areTransitionsEmpty())
		{
			tmpState->AddPredicate(FI_abs::learnPredicates(&fwd, &bwdBackup));
			tmpState->clearFAE();
			return;
		}
	}

	assert(buProductResult.tas_.size() == fwd.GetFAE()->getRootCount());

	TreeAutVec newRoots = normInfo == nullptr ?
						  buProductResult.tas_ :
						  Normalization::revertNormalization(buProductResult, *newFae, *normInfo);

	newFae->resizeRoots(newRoots.size());

	assert(newRoots.size() >= bwdBackup.GetFAE()->getRootCount());
	assert(newRoots.size() == newFae->getRootCount());

	for (size_t i = 0; i < newRoots.size(); ++i)
	{
		newFae->setRoot(i,
                        newRoots.at(i) != nullptr ?
                        std::shared_ptr<TreeAut>(new TreeAut(*newRoots.at(i))) :
                        nullptr);
	}

	for (const auto& ta : newFae->getRoots())
	{
		if (ta != nullptr && ta->areTransitionsEmpty())
		{
			tmpState->AddPredicate(FI_abs::learnPredicates(&fwd, &bwdBackup));
			tmpState->clearFAE();
			return;
		}
	}

	newFae->removeEmptyRoots();
	newFae->connectionGraph.reset(newRoots.size());
	newFae->updateConnectionGraph();


	tmpState->SetFAE(newFae);
}

void noFold(SymState::AbstractionInfo& ainfo)
{
	ainfo.iterationToFoldedRoots_[ainfo.abstrIteration_] =
			SymState::BoxesAtRoot();
	++ainfo.abstrIteration_;
}

std::vector<size_t> createChildren(
		const size_t cl,
		const size_t cr,
		const size_t cc)
{
	std::vector<size_t> res;
	res.push_back(cl);
	res.push_back(cr);
	res.push_back(cc);

	return res;
}

void addInitialPredicates(
		FAE&                                  fae,
		std::vector<std::shared_ptr<const TreeAut>>& predicates)
{
	auto initPred = fae.allocTA();
	assert(initPred->areTransitionsEmpty());
	assert(initPred->getFinalStates().size() == 0);

	label_type treeNodeLabel = nullptr;
    for (size_t i = 0; i < fae.getRootCount(); ++i)
	{
		for (const auto trans : *(fae.getRoot(i)))
		{
			if (VATAAdapter::GetSymbol(trans)->isNode() &&
					reinterpret_cast<const TypeBox *>(
							VATAAdapter::GetSymbol(trans)->getABox(0))->getName().find("TreeNode") !=
							std::string::npos)
			{
				treeNodeLabel = VATAAdapter::GetSymbol(trans);
				break;
			}
		}
		if (treeNodeLabel != nullptr)
		{
			break;
		}
	}
	assert(treeNodeLabel != nullptr);

	const size_t r0 = fae.addData(*initPred, Data::createInt(0));
	const size_t r1 = fae.addData(*initPred, Data::createInt(1));
	const size_t q0 = fae.freshState();
	const size_t q1 = fae.freshState();
	const size_t q2 = fae.freshState();
	const size_t q3 = fae.freshState();
	const size_t q4 = fae.freshState();

	initPred->addFinalState(q0);

	// Adding q0
	// initPred->addTransition(createChildren(q1,q3,r1), treeNodeLabel, q0);
    // initPred->addTransition(createChildren(q2,q3,r1), treeNodeLabel, q0);
    // initPred->addTransition(createChildren(q1,q4,r1), treeNodeLabel, q0);
    // initPred->addTransition(createChildren(q2,q4,r1), treeNodeLabel, q0);
	// initPred->addTransition(createChildren(r0,r0,r1), treeNodeLabel, q0);

	// // Adding q1
    // initPred->addTransition(createChildren(q2,q3,r1), treeNodeLabel, q1);
    // initPred->addTransition(createChildren(q2,r0,r1), treeNodeLabel, q1);
    // initPred->addTransition(createChildren(q1,q3,r1), treeNodeLabel, q1);
	// initPred->addTransition(createChildren(r0,r0,r1), treeNodeLabel, q1);

    // // Adding q2
    // initPred->addTransition(createChildren(q2,q3,r0), treeNodeLabel, q2);
    // initPred->addTransition(createChildren(q1,r0,r0), treeNodeLabel, q2);
    // initPred->addTransition(createChildren(r0,r0,r0), treeNodeLabel, q2);

	// // Adding q3
    // initPred->addTransition(createChildren(q1,q3,r1), treeNodeLabel, q3);
    // initPred->addTransition(createChildren(q1,r0,r1), treeNodeLabel, q3);
    // initPred->addTransition(createChildren(r0,q4,r1), treeNodeLabel, q3);
	// initPred->addTransition(createChildren(r0,r0,r1), treeNodeLabel, q3);

	// // Adding q4
    // initPred->addTransition(createChildren(q1,q4,r0), treeNodeLabel, q4);
	// initPred->addTransition(createChildren(r0,q4,r0), treeNodeLabel, q4);
    // initPred->addTransition(createChildren(r0,q3,r0), treeNodeLabel, q4);
    // initPred->addTransition(createChildren(r0,r0,r0), treeNodeLabel, q4);


	// Adding q0
	initPred->addTransition(createChildren(q0,q2,r0), treeNodeLabel, q0);
    initPred->addTransition(createChildren(q2,q0,r0), treeNodeLabel, q0);
	initPred->addTransition(createChildren(q0,q0,r0), treeNodeLabel, q0);
    // initPred->addTransition(createChildren(q1,q1,r0), treeNodeLabel, q0); // added
    initPred->addTransition(createChildren(q2,q1,r1), treeNodeLabel, q0);
	initPred->addTransition(createChildren(q1,q2,r1), treeNodeLabel, q0);

	// Adding q1
    initPred->addTransition(createChildren(q1,q2,r0), treeNodeLabel, q1);
    initPred->addTransition(createChildren(q2,q1,r0), treeNodeLabel, q1);
	initPred->addTransition(createChildren(q1,q1,r0), treeNodeLabel, q1); // commented
    initPred->addTransition(createChildren(q2,q2,r1), treeNodeLabel, q1);

    // Adding q2
    initPred->addTransition(createChildren(q2,q2,r0), treeNodeLabel, q2);
    initPred->addTransition(createChildren(q2,q2,r1), treeNodeLabel, q2);
	initPred->addTransition(createChildren(r0,r0,r1), treeNodeLabel, q2);
	initPred->addTransition(createChildren(r0,r0,r0), treeNodeLabel, q2);

	std::shared_ptr<const TreeAut> res = std::shared_ptr<TreeAut>(initPred);
	predicates.push_back(res);
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
	tmpState->ClearPredicates();
	const SymState::AbstractionInfo& ainfo = fwdPred.GetAbstractionInfo();

	assert(ainfo.finalFae_ != nullptr);
	revertAbstraction(tmpState, std::shared_ptr<FAE>(new FAE(*ainfo.finalFae_)));
	assert(!tmpState->GetFAE()->Empty());
	if (tmpState->GetFAE()->Empty())
	{
		return tmpState;
	}

	tmpState = execMan.copyState(bwdSucc);

	assert(ainfo.iterationToFoldedRoots_.size() == ainfo.faeAtIteration_.size());
	assert(ainfo.abstrIteration_ > 0);
	for (int i = (ainfo.abstrIteration_ - 1); i >= 0; --i)
	{
		assert(i < ainfo.iterationToFoldedRoots_.size());

		const auto& foldedRoots = (i != 0) ?
                     ainfo.iterationToFoldedRoots_.at(i) :
                     ainfo.learn1Boxes_;
		if (foldedRoots.size() == 0)
		{
			// continue;
		}
		std::pair<bool, std::shared_ptr<FAE>> reversionRes =
				revertFolding(foldedRoots, *tmpState->GetFAE(), fwdPred);
		if (!reversionRes.first && fwdPred.GetFAE()->getRootCount() > reversionRes.second->getRootCount())
		{
			// continue;
		}
        tmpState->SetFAE(reversionRes.second);

        if (tmpState->GetFAE()->getRootCount() == 0)
        {
            return tmpState;
        }

		assert(tmpState->GetFAE()->getRootCount());

		assert(ainfo.faeAtIteration_.count(i));

		revertAbstraction(tmpState, ainfo.faeAtIteration_.at(i),
						  &ainfo.normInfoAtIteration_.at(i));

        if (tmpState->GetFAE()->Empty())
		{ // check assertion

			return tmpState;
		}
	}

	tmpState->SetFAE(
			revertFolding(ainfo.learn2Boxes_, *tmpState->GetFAE(), fwdPred).second);
    tmpState->SetFAE(
			revertFolding(ainfo.iterationToFoldedRoots_.at(0), *tmpState->GetFAE(), fwdPred).second
    );

	assert(!tmpState->GetFAE()->Empty());
	revertAbstraction(tmpState, std::shared_ptr<FAE>(new FAE(*fwdPred.GetFAE())), &ainfo.reorderNormInfo_);
	std::shared_ptr<FAE> faeMin = std::shared_ptr<FAE>(new FAE(*tmpState->GetFAE()));
	assert(!tmpState->GetFAE()->Empty());
	assert(!ainfo.reorderNormInfo_.empty()
		   || ainfo.faeBeforeReorder_->getRootCount() != tmpState->GetFAE()->getRootCount());

	revertAbstraction(tmpState, ainfo.faeBeforeReorder_);

	faeMin->minimizeRoots();
	tmpState->SetFAE(faeMin);

	FA_DEBUG_AT(1, "Executing !!VERY!! suspicious reverse operation FixpointBase");
	return tmpState;
}


TreeAutVec FI_abs::learnPredicates(
		SymState *fwdState,
		SymState *bwdState)
{
	bool areSame = fwdState->GetFAE()->getRootCount() == bwdState->GetFAE()->getRootCount();
	std::shared_ptr<const FAE> normFAEBwd = areSame ? bwdState->GetFAE() : bwdState->newNormalizedFAE();
	areSame = fwdState->GetFAE()->getRootCount() == normFAEBwd->getRootCount();
	std::shared_ptr<const FAE> normFAEFwd = areSame ? fwdState->GetFAE() : fwdState->newNormalizedFAE();
	FA_DEBUG_AT(1, "FWD Learn predicates " << *fwdState);
	FA_DEBUG_AT(1, "BWD Learn predicates " << *bwdState);

	if (normFAEBwd->getRootCount() < FIXED_REG_COUNT ||
			(normFAEBwd->getRootCount() == FIXED_REG_COUNT &&
			normFAEBwd->getRoot(1) == nullptr))
	{
		return TreeAutVec(
			normFAEBwd->getRoots().begin(),
			normFAEBwd->getRoots().end());
	}

	if (normFAEFwd->getRootCount() != normFAEBwd->getRootCount())
	{
		FA_DEBUG_AT(1, "FWD " << *normFAEFwd << '\n' << "BWD " << *normFAEBwd << '\n');
	}
	assert(normFAEFwd->getRootCount() == normFAEBwd->getRootCount());

	TreeAutVec predicates = getEmptyTrees(*normFAEFwd, *normFAEBwd);

	if (predicates.empty())
	{ // no predicates found, brutal refinement
		assert(false);
		predicates.insert(predicates.end(),
				normFAEBwd->getRoots().begin(), normFAEBwd->getRoots().end());
	}

    // auto normalized = bwdState->newNormalizedFAE(true, true);
	// for (size_t i = 1; i < normalized->getRootCount(); ++i)
	// {
    //     if (normalized->getRoot(i) == nullptr)
    //     {
    //         continue;
    //     }
	// 	predicates.push_back(normalized->getRoot(i));
	// }

	assert(!predicates.empty());

	return predicates;
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

	if (FA_FUSION_ENABLED)
	{
		// strongFusion(fae);
		weakFusion(fae);
	}
	FA_DEBUG_AT(1, "After fusion " << fae << '\n');

	// abstract
	Abstraction abstraction(fae);

	if ((FA_START_WITH_PREDICATE_ABSTRACTION || !predicates_.empty()) && FA_USE_PREDICATE_ABSTRACTION)
	{	// for predicate abstraction
		for (size_t i = 0; i < fae.getRootCount(); ++i)
		{
			abstraction.predicateAbstraction(i, this->getPredicates());
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
//				abstraction.heightAbstraction(i, FA_ABS_HEIGHT, SmarterTMatchF(fae));
			}
		}
	}

	FA_DEBUG_AT(1, "after abstraction: " << std::endl << fae);
}


// FI_abs
void FI_abs::execute(ExecutionManager& execMan, SymState& state)
{
	auto& ainfo = state.GetAbstractionInfo();
	ainfo.clear();

	std::shared_ptr<FAE> fae = std::shared_ptr<FAE>(new FAE(*(state.GetFAE())));

	fae->updateConnectionGraph();

	if (firstRun_ && fae->getRootCount() > 2)
	{
		// addInitialPredicates(*fae, this->predicates_);
		firstRun_ = false;
	}

	std::set<size_t> forbidden;
#if FA_ALLOW_FOLDING

	// TODO faeBeforeReorder; after is remembered in state
	ainfo.faeBeforeReorder_ = std::shared_ptr<FAE>(new FAE(*fae));

	// reorder components into the canonical form (no merging!)
	reorder(&state, *fae, ainfo.reorderNormInfo_);
	state.SetFAE(std::shared_ptr<FAE>(new FAE(*fae)));

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
	else
	{
		noFold(ainfo);
	}

	ainfo.learn2Boxes_ = SymState::BoxesAtRoot(
			Folding::learn2(*fae,
							boxMan_,
							Normalization::computeForbiddenSet(*fae)));
#endif

	forbidden = Normalization::computeForbiddenSet(*fae);

	ainfo.normInfoAtIteration_[0] = Normalization::NormalizationInfo();
	Normalization::normalize(*fae, &state, ainfo.normInfoAtIteration_.at(0),
							 forbidden, true);

	assert(ainfo.abstrIteration_ == 1);
	ainfo.faeAtIteration_[0] = std::shared_ptr<FAE>(new FAE(*fae));
	assert(ainfo.faeAtIteration_.at(0)->getRootCount() <= state.GetFAE()->getRootCount());

	abstract(*fae);
#if FA_ALLOW_FOLDING
	ainfo.learn1Boxes_ = SymState::BoxesAtRoot(
			Folding::learn1(*fae, boxMan_, Normalization::computeForbiddenSet(*fae))
	);

	if (boxMan_.boxDatabase().size())
	{
		FAE old(*fae, boxMan_);

		try
		{
			do
			{
				forbidden = Normalization::computeForbiddenSet(*fae);

				ainfo.normInfoAtIteration_[ainfo.abstrIteration_] =
						Normalization::NormalizationInfo();
				Normalization::normalize(*fae, &state,
										 ainfo.normInfoAtIteration_.at(ainfo.abstrIteration_),
										 forbidden, true);

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
	assert(ainfo.iterationToFoldedRoots_.size() == ainfo.faeAtIteration_.size());
#endif
	FA_DEBUG_AT(1, "After abstraction " << *fae);
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
		// storeFaeToAccs(*fae, true);

		FA_DEBUG_AT(1, "Continue with " << *fae);
		SymState* tmpState = execMan.createChildState(state, next_);
		fae->minimizeRoots();
		tmpState->SetFAE(fae);
		execMan.enqueue(tmpState);
		ainfo.finalFae_ = tmpState->GetFAE();
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
	Normalization::NormalizationInfo tmp;
	reorder(&state, *fae, tmp);

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
