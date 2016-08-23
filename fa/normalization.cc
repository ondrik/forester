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

// Code Listener headers
#include <cl/storage.hh>

// Forester headers
#include "abstractinstruction.hh"
#include "error_messages.hh"
#include "garbage_checker.hh"
#include "normalization.hh"
#include "programerror.hh"
#include "regdef.hh"
#include "symstate.hh"
#include "virtualmachine.hh"
#include "bu_intersection.hh"

namespace {
	
	void substituteRootRefByTA(
		TreeAut&                        resTA,
		const TreeAut::Transition&      trans,
		const size_t                    rootRefState,
		const std::vector<size_t>&      joinStates)
	{
		std::vector<size_t> transChildren = trans.GetChildren();
		std::vector<size_t>::iterator rootRefIterator = 
			std::find(transChildren.begin(), transChildren.end(), rootRefState);
		assert(rootRefIterator != transChildren.end());

		for (const auto& k : joinStates)
		{
			*rootRefIterator = k;
			resTA.addTransition(
				transChildren,
				TreeAut::GetSymbol(trans),
				trans.GetParent());
		}
	}

	// StateTuple has to be iterable and has to have defined equivalence relation
	template<class StateTuple>
	static bool containsRootRef(
		const StateTuple& stateTuple,
		const size_t rootRefState)
	{
		return std::find(stateTuple.begin(), stateTuple.end(), rootRefState) != stateTuple.end();
	}
}

TreeAut* Normalization::mergeRoot(
	TreeAut&                          dst,
	const size_t                      rootRef,
	std::shared_ptr<const TreeAut>    srcPtr,
	std::vector<size_t>&              joinStates)
{
	// Assertions
	assert(rootRef < this->fae.getRootCount());

	const TreeAut& src = *srcPtr;
	TreeAut* resTA = this->fae.allocTA();

	std::unordered_map<size_t, size_t> joinStatesMap;
	if (srcPtr != nullptr)
	{
		for (const size_t finState : src.getFinalStates())
		{
			joinStates.push_back(this->fae.nextState());
			joinStatesMap.insert(std::make_pair(finState, this->fae.freshState()));
		}
	}
	else
	{
		joinStates.push_back(this->fae.addData(*resTA, Data::createUndef()));
	}

	resTA->addFinalStates(dst.getFinalStates());
	size_t rootRefState = _MSB_ADD(this->fae.boxMan->getDataId(Data::createRef(rootRef)));
	
	bool hit = false;
	for (const auto& trans : dst)
	{
		if (containsRootRef(trans.GetChildren(), rootRefState))
		{
			substituteRootRefByTA(*resTA, trans, rootRefState, joinStates);
			hit = true;
		}
		else
		{
			resTA->addTransition(trans);
		}
	}
	if (!hit) {assert(false);}
	if (srcPtr != nullptr)
	{ // avoid screwing up things
		src.unfoldAtRoot(*resTA, joinStatesMap, false);
	}

	// postconditions
	
	// the new TA should have the same final states as dst TA
	// so at least number of the states is checked. 
	assert(resTA->getFinalStates().size() == dst.getFinalStates().size());
	return resTA;
}


void Normalization::traverse(
	std::vector<bool>&                visited,
	std::vector<size_t>&              order,
	std::vector<bool>&                marked) const
{
	visited = std::vector<bool>(this->fae.getRootCount(), false);
	marked = std::vector<bool>(this->fae.getRootCount(), false);

	order.clear();

	for (const Data& var : this->fae.GetVariables())
	{	// start from program variables and perform DFT
		// skip everything what is not a root reference
		if (!var.isRef())
			continue;

		size_t root = var.d_ref.root;

		// mark rootpoint pointed by a variable
		marked[root] = true;

		// check whether we traversed this one before
		if (visited[root])
			continue;

		this->fae.connectionGraph.visit(root, visited, order, marked);
	}
}


void Normalization::normalizeRoot(
	std::vector<bool>&                normalized,
	const size_t                      root,
	const std::vector<bool>&          marked,
	NormalizationInfo&                normalizationInfo,
	bool                              ignoreVars)
{
	normalizationInfo.addRootIfNotExists(root);
	if (normalized[root])
		return;

	normalized[root] = true;

	// we need a copy here!
	ConnectionGraph::CutpointSignature signature =
		this->fae.connectionGraph.data[root].signature;

	for (auto& cutpoint : signature)
	{
		this->normalizeRoot(normalized, cutpoint.root, marked, normalizationInfo, ignoreVars);

		if (marked[cutpoint.root] && !(ignoreVars && cutpoint.root > 1))
			continue;

		if (root == cutpoint.root)
		{
			continue;
		}
		assert(root != cutpoint.root);

		std::vector<size_t> refStates;

		assert(this->fae.getRoot(root) != nullptr);
		TreeAut* ta = this->mergeRoot(
			*this->fae.getRoot(root),
			cutpoint.root,
			this->fae.getRoot(cutpoint.root),
			refStates
		);
		normalizationInfo.mergeRoots(root, cutpoint.root);
		normalizationInfo.remove(cutpoint.root);

		normalizationInfo.addMergedRoot(root, cutpoint.root);
		assert(refStates.size() > 0);
		for (const auto& state : refStates)
		{
			normalizationInfo.addJoinState(root, cutpoint.root, state);
		}

		this->fae.setRoot(root, std::shared_ptr<TreeAut>(ta));
		this->fae.setRoot(cutpoint.root, nullptr);

		this->fae.connectionGraph.mergeCutpoint(root, cutpoint.root);
	}
}


bool Normalization::selfReachable(
	size_t                            root,
	size_t                            self,
	const std::vector<bool>&          marked)
{
	for (auto& cutpoint : this->fae.connectionGraph.data[root].signature)
	{
		if (cutpoint.root == self)
			return true;

		if (marked[cutpoint.root])
			continue;

		if (this->selfReachable(cutpoint.root, self, marked))
			return true;
	}

	return false;
}


void Normalization::scan(
	std::vector<bool>&                marked,
	std::vector<size_t>&              order,
	const std::set<size_t>&           forbidden,
	const bool                        extended)
{
	assert(this->fae.connectionGraph.isValid());

	std::vector<bool> visited(this->fae.getRootCount(), false);

	marked = std::vector<bool>(this->fae.getRootCount(), false);

	order.clear();

	// compute canonical root ordering
	this->traverse(visited, order, marked);

	GarbageChecker::nontraverseCheckAndRemoveGarbage(this->fae, this->state_, visited);

	if (!extended)
	{
		for (auto& x : forbidden)
		{
			marked[x] = true;
		}

		return;
	}

	for (auto& x : forbidden)
	{
		marked[x] = true;

		for (auto& cutpoint : this->fae.connectionGraph.data[x].signature)
		{
			if ((cutpoint.root != x) && !this->selfReachable(cutpoint.root, x, marked))
				continue;

			marked[cutpoint.root] = true;

			break;
		}
	}
}

namespace {
	static bool isInCanonicalForm(
		const std::vector<bool>&          marked,
		const std::vector<size_t>&        order)
	{
		size_t i;

		for (i = 0; i < order.size(); ++i)
		{
			if (!marked[i] || (order[i] != i))
				return false;
		}

		assert(i == order.size());
		return true;
	}
}

bool Normalization::normalizeInternal(
	const std::vector<bool>&          marked,
	const std::vector<size_t>&        order,
	NormalizationInfo&                normInfo,
	bool                              ignoreVars)
{
	if (isInCanonicalForm(marked, order))
	{	// in case the FA is in the canonical form
		this->fae.resizeRoots(order.size());
		this->fae.connectionGraph.data.resize(order.size());
		normInfo.createIdentityMapping(this->fae.getRootCount());
		return false;
	}

	// reindex roots
	std::vector<size_t> index(this->fae.getRootCount(), static_cast<size_t>(-1));
	std::vector<bool> normalized(this->fae.getRootCount(), false);
	std::vector<std::shared_ptr<TreeAut>> newRoots;
	size_t offset = 0;
	bool merged = false;

	for (auto& i : order)
	{	// push tree automata into a new tuple in the right order
		normInfo.addRootIfNotExists(newRoots.size());

		this->normalizeRoot(normalized, i, marked, normInfo, ignoreVars);

		if (!marked[i])
		{	// if a root was merged, do not put it in the new tuple!
			merged = true;
			normInfo.removeLastRoot();

			continue;
		}
		normInfo.addMapping(i, newRoots.size());
		newRoots.push_back(this->fae.getRoot(i));

		index[i] = offset++;
	}
	assert(order.size() >= newRoots.size());

	normInfo.finalize();

	// update representation
	this->fae.swapRoots(newRoots);

	assert(normInfo.rootsNormalizationInfo_.size() == 0 ||
		   normInfo.rootsNormalizationInfo_.size() <= newRoots.size());
	assert(normInfo.rootMapping_.size() == newRoots.size());

	for (size_t i = 0; i < this->fae.getRootCount(); ++i)
	{
		if (this->fae.getRoot(i) == nullptr)
			continue;
		assert(this->fae.getRoot(i) != nullptr);
		this->fae.setRoot(i, std::shared_ptr<TreeAut>(
			this->fae.relabelReferences(this->fae.getRoot(i).get(), index)
		));
	}

	this->fae.connectionGraph.finishNormalization(this->fae.getRootCount(), index);

	// update variables
	this->fae.UpdateVarsRootRefs(index);

	return merged;
}


bool Normalization::normalize(
		FAE&                              fae,
		const SymState*                   state,
		NormalizationInfo&                normalizationInfo,
		const std::set<size_t>&           forbidden,
		bool                              extended,
		bool                              ignoreVars)
{
	Normalization norm(fae, state);

	std::vector<size_t> order;
	std::vector<bool> marked;

	norm.scan(marked, order, forbidden, extended);

	bool result = norm.normalizeInternal(marked, order, normalizationInfo, ignoreVars);

	assert(!normalizationInfo.empty());
	FA_DEBUG_AT(3, "after normalization: " << std::endl << fae);

	return result;
}


bool Normalization::normalize(
		FAE&                              fae,
		const SymState*                   state,
		const std::set<size_t>&           forbidden,
		bool                              extended,
		bool                              ignoreVars)
{
	NormalizationInfo temp;
	return Normalization::normalize(fae, state, temp, forbidden, extended, ignoreVars);
}


bool Normalization::normalizeWithoutMerging(
		FAE&                              fae,
		const SymState*                   state,
		NormalizationInfo&                normInfo,
		const std::set<size_t>&           forbidden,
		bool                              extended)
{
	Normalization norm(fae, state);

	std::vector<size_t> order;
	std::vector<bool> marked;

	norm.scan(marked, order, forbidden, extended);

	// normalize without merging (we say that all components are referred more
	// than once), i.e. only reorder
	std::fill(marked.begin(), marked.end(), true);

	bool result = norm.normalizeInternal(marked, order, normInfo);

	FA_DEBUG_AT(3, "after normalization: " << std::endl << fae);

	return result;
}

std::set<size_t> Normalization::computeForbiddenSet(const FAE& fae, const bool ignoreNearby, const bool ignoreVars)
{
	// Assertions
	assert(fae.getRootCount() == fae.connectionGraph.data.size());
	assert(fae.getRootCount() >= FIXED_REG_COUNT);

	std::set<size_t> forbidden;

	VirtualMachine vm(fae);

	if (!ignoreVars)
	{
		for (size_t i = 0; i < FIXED_REG_COUNT; ++i)
		{
			assert(fae.getRoot(vm.varGet(i).d_ref.root));
			forbidden.insert(vm.varGet(i).d_ref.root);
		}
	}

	if (ignoreNearby)
	{
		return forbidden;
	}

	for (size_t i = 0; i < FIXED_REG_COUNT; ++i)
	{
		vm.getNearbyReferences(vm.varGet(i).d_ref.root, forbidden);
	}

	return forbidden;
}


Normalization::TreeAutVec Normalization::revertNormalization(
        const BUIntersection::BUProductResult& buProductResult,
		FAE& newFAE,
        const Normalization::NormalizationInfo& info)
{
	assert(!info.empty());

	const auto& tas = buProductResult.tas_;
	assert(info.rootMapping_.size() == tas.size());
	TreeAutVec res = TreeAutVec(tas.size());

	std::unordered_map<size_t, size_t> stateToRef;

	std::vector<size_t> reverseMap(info.rootMapping_.size());

	for (const auto& mappingPair : info.rootMapping_)
	{
		reverseMap[mappingPair.second] = mappingPair.first;
	}
	assert(reverseMap.size() == info.rootMapping_.size());

	for (const auto& mappingPair : info.rootMapping_)
	{
		// First of all we move TA to its position before normalization
		if (mappingPair.first >= res.size())
		{
			res.resize(mappingPair.first + 1);
		}

		assert(tas.size() > mappingPair.second);
		// make deep copy of TA
		auto tmp = std::shared_ptr<TreeAut>(new TreeAut());
		newFAE.relabelReferences(*tmp,
								 *tas.at(mappingPair.second), reverseMap);
		res[mappingPair.first] = tmp;
		assert(res[mappingPair.first] != nullptr);

		if (!info.containsMergedRoot(mappingPair.first))
		{
			continue;
		}

		// Copy automaton corresponding where other TAs were merged to
		// the positions where these TAs were before normalization.
		assert(info.rootsNormalizationInfo_.count(mappingPair.first));
		const auto& rootInfo = *(info.rootsNormalizationInfo_.find(mappingPair.first));
		assert(mappingPair.second < tas.size());
		for (const auto& mergedRoot : rootInfo.second.rootsMerging_)
		{
			if (mergedRoot >= res.size())
			{
				res.resize(mergedRoot + 1);
			}

			// make deep copy of TA
			auto ta = std::shared_ptr<TreeAut>(new TreeAut(*res.at(mappingPair.first)));
			ta->eraseFinalStates();

			for (const auto& joinStatePair : rootInfo.second.joinStates_)
			{ // find all join states for the given mergedRoot
				if (joinStatePair.first != mergedRoot)
				{
					continue;
				}

				for (const auto& productMapPair : buProductResult.productMap_)
				{ // for each join state find a related product state in new automaton
					if (productMapPair.first.first == joinStatePair.second)
					{ // it is join state so set the related product states final in ta.
						if (ta->getUsedStates().count(productMapPair.second) == 0)
						{
							continue;
						}
						assert(ta->getUsedStates().count(productMapPair.second));
						ta->addFinalState(productMapPair.second);

						assert(stateToRef.count(productMapPair.second) == 0);
						stateToRef[productMapPair.second] = joinStatePair.first;
					}
				}
			}

			if (rootInfo.first == mergedRoot)
			{ // finally if ta is not moved anywhere copy the original final states
				ta->addFinalStates(tas.at(rootInfo.first)->getFinalStates());
			}
			assert(ta->getFinalStates().size() > 0);

			res[mergedRoot] = ta;
			assert(res[mergedRoot] != nullptr);
		}
		// + 1 is for identity, when i-th TA is moved to the i-th position
		assert(tas.size() + rootInfo.second.rootsMerging_.size() <= res.size() + 1);
    }

	auto f = [&stateToRef, &newFAE] (TreeAut& dst, const size_t state) {
        return (stateToRef.count(state)) ?
               newFAE.addData(dst, Data::createRef(stateToRef.at(state), 0)) :
               state;
	};

	for (size_t i=0; i < res.size(); ++i)
	{
		// assert(res.at(i) != nullptr);
		if (res.at(i) != nullptr)
		{
			std::shared_ptr<TreeAut> newTa(new TreeAut());
			assert(newTa != nullptr);
			newFAE.modifyTransitions(*newTa, *res.at(i), f);
			res[i] = newTa;

		}
	}

    return res;
}

