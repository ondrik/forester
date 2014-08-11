/*
 * Copyright (C) 2010 Jiri Simacek
 *
 * This file is part of predator.
 *
 * predator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * predator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with predator.  If not, see <http://www.gnu.org/licenses/>.
 */

// Standard library headers
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <ostream>

// Forester headers
#include "treeaut.hh"
#include "simalg.hh"
#include "antichainext.hh"

struct LhsEnv
{
	size_t index;
	std::vector<size_t> data;

	LhsEnv(
		const std::vector<size_t>&   lhs,
		size_t                       index) :
		index(index),
		data{}
	{
		this->data.insert(this->data.end(), lhs.begin(), lhs.begin() + index);
		this->data.insert(this->data.end(), lhs.begin() + index + 1, lhs.end());
	}

	static std::set<LhsEnv>::iterator get(
		const std::vector<size_t>&    lhs,
		size_t                        index,
		std::set<LhsEnv>&             s)
	{
		return s.insert(LhsEnv(lhs, index)).first;
	}

	static std::set<LhsEnv>::iterator find(
		const std::vector<size_t>&     lhs,
		size_t                         index,
		std::set<LhsEnv>&              s)
	{
		std::set<LhsEnv>::iterator x = s.find(LhsEnv(lhs, index));
		if (s.end() == x)
			throw std::runtime_error("LhsEnv::find - lookup failed");

		return x;
	}

	static bool sim(
		const LhsEnv&                              e1,
		const LhsEnv&                              e2,
		const std::vector<std::vector<bool>>&      sim)
	{
		if ((e1.index != e2.index) || (e1.data.size() != e2.data.size()))
			return false;

		for (size_t i = 0; i < e1.data.size(); ++i)
		{
			if (!sim[e1.data[i]][e2.data[i]])
				return false;
		}

		return true;
	}

	static bool eq(
		const LhsEnv&                           e1,
		const LhsEnv&                           e2,
		const std::vector<std::vector<bool>>&   sim)
	{
		if ((e1.index != e2.index) || (e1.data.size() != e2.data.size()))
			return false;

		for (size_t i = 0; i < e1.data.size(); ++i)
		{
			if (!sim[e1.data[i]][e2.data[i]] || !sim[e2.data[i]][e1.data[i]])
				return false;
		}

		return true;
	}

	bool operator<(const LhsEnv& rhs) const
	{
		return (this->index < rhs.index) || (
			(this->index == rhs.index) && (this->data < rhs.data));
	}

	bool operator==(const LhsEnv& rhs) const
	{
		return (this->index == rhs.index) && (this->data == rhs.data);
	}

	friend std::ostream& operator<<(
		std::ostream&        os,
		const LhsEnv&        env)
	{
		os << "[" << env.data.size() << "]";
		os << env.index << "|";
		for (size_t i : env.data)
			os << "|" << i;
		return os;
	}
};

struct Env
{
	std::set<LhsEnv>::iterator lhs;
	size_t label;
	size_t rhs;

	Env(
		std::set<LhsEnv>::iterator    lhs,
		size_t                        label,
		size_t                        rhs) :
		lhs(lhs),
		label(label),
		rhs(rhs)
	{ }

	static std::map<Env, size_t>::iterator get(
		std::set<LhsEnv>::const_iterator   lhs,
		size_t                             label,
		size_t                             rhs,
		std::map<Env, size_t>&             m)
	{
		return m.insert(std::pair<Env, size_t>(Env(lhs, label, rhs), m.size())).first;
	}

	static std::map<Env, size_t>::iterator get(
		std::set<LhsEnv>::const_iterator    lhs,
		size_t                              label,
		size_t                              rhs,
		std::map<Env, size_t>&              m,
		bool&                               inserted)
	{
		std::pair<std::map<Env, size_t>::iterator, bool> x = m.insert(std::pair<Env, size_t>(Env(lhs, label, rhs), m.size()));
		inserted = x.second;
		return x.first;
	}

	static std::map<Env, size_t>::iterator find(
		std::set<LhsEnv>::const_iterator     lhs,
		size_t                               label,
		size_t                               rhs,
		std::map<Env, size_t>&               m)
	{
		std::map<Env, size_t>::iterator x = m.find(Env(lhs, label, rhs));
		if (x == m.end())
			throw std::runtime_error("Env::find - lookup failed");
		return x;
	}

	static bool sim(
		const Env&                              e1,
		const Env&                              e2,
		const std::vector<std::vector<bool>>&   sim)
	{
		return (e1.label == e2.label) && LhsEnv::sim(*e1.lhs, *e2.lhs, sim);
	}

	static bool eq(
		const Env&                              e1,
		const Env&                              e2,
		const std::vector<std::vector<bool>>&   sim)
	{
		return (e1.label == e2.label) && LhsEnv::eq(*e1.lhs, *e2.lhs, sim);
	}

	bool operator<(const Env& rhs) const
	{
		return (this->label < rhs.label) || (
			(this->label == rhs.label) && (
				(*this->lhs < *rhs.lhs) || (
					(*this->lhs == *rhs.lhs) && (
						this->rhs < rhs.rhs
					)
				)
			)
		);
	}
};

void TA::downwardTranslation(
	LTS&                    lts,
	const Index<size_t>&    stateIndex,
	const Index<T>&         labelIndex) const
{
	// build an index of non-translated left-hand sides
	Index<const std::vector<size_t>*> lhs;
	this->buildLhsIndex(lhs);
	lts = LTS(labelIndex.size() + this->maxRank_, stateIndex.size() + lhs.size());
	for (Index<const std::vector<size_t>*>::iterator i = lhs.begin();
		i != lhs.end(); ++i)
	{
		for (size_t j = 0; j < i->first->size(); ++j)
		{
			lts.addTransition(stateIndex.size() + i->second,
				labelIndex.size() + j, stateIndex[(*i->first)[j]]);
		}
	}

	for (const TransIDPair* ptrTransIDPair : this->transitions_)
	{
		lts.addTransition(
			stateIndex[ptrTransIDPair->first.rhs()],
			labelIndex[ptrTransIDPair->first.label()],
			stateIndex.size() + lhs[&(ptrTransIDPair->first.lhs())]);
	}
}


void TA::downwardSimulation(
	std::vector<std::vector<bool>>&   rel,
	const Index<size_t>&              stateIndex) const
{
	LTS lts;
	Index<T> labelIndex;
	this->buildLabelIndex(labelIndex);
	this->downwardTranslation(lts, stateIndex, labelIndex);
	OLRTAlgorithm alg(lts);
	alg.init();
	alg.run();
	alg.buildRel(stateIndex.size(), rel);
}

void TA::upwardTranslation(
	LTS&                                    lts,
	std::vector<std::vector<size_t>>&       part,
	std::vector<std::vector<bool>>&         rel,
	const Index<size_t>&                    stateIndex,
	const Index<T>&                         labelIndex,
	const std::vector<std::vector<bool>>&   sim) const
{
	std::set<LhsEnv> lhsEnvSet;
	std::map<Env, size_t> envMap;
	std::vector<const Env*> head;
	part.clear();
	for (const TransIDPair* ptrTransIDPair : this->transitions_)
	{
		std::vector<size_t> lhs;
		stateIndex.translate(lhs, ptrTransIDPair->first.lhs());
		size_t label = labelIndex[ptrTransIDPair->first.label()];
		size_t rhs = stateIndex[ptrTransIDPair->first.rhs()];
		for (size_t j = 0; j < lhs.size(); ++j)
		{ // insert required items into lhsEnv and lhsMap and build equivalence
			// classes
			bool inserted;
			std::map<Env, size_t>::const_iterator env =
				Env::get(LhsEnv::get(lhs, j, lhsEnvSet), label, rhs, envMap, inserted);

			if (inserted)
			{
				inserted = false;
				for (size_t k = 0; k < head.size(); ++k)
				{
					if (Env::eq(*head[k], env->first, sim))
					{
						part[k].push_back(env->second + stateIndex.size());
						inserted = true;
						break;
					}
				}
				if (!inserted)
				{
					head.push_back(&env->first);
					part.push_back(std::vector<size_t>(
						1, env->second + stateIndex.size()));
				}
			}
		}
	}

	lts = LTS(labelIndex.size() + 1, stateIndex.size() + envMap.size());
	for (const TransIDPair* ptrTransIDPair : this->transitions_)
	{
		std::vector<size_t> lhs;
		stateIndex.translate(lhs, ptrTransIDPair->first.lhs());
		size_t label = labelIndex[ptrTransIDPair->first.label()];
		size_t rhs = stateIndex[ptrTransIDPair->first.rhs()];
		for (size_t j = 0; j < lhs.size(); ++j)
		{
			// find particular env
			std::map<Env, size_t>::iterator env =
				Env::find(LhsEnv::find(lhs, j, lhsEnvSet), label, rhs, envMap);
			lts.addTransition(lhs[j], labelIndex.size(), env->second);
			lts.addTransition(env->second, label, rhs);
		}
	}

	rel = std::vector<std::vector<bool>>(
		part.size() + 2, std::vector<bool>(part.size() + 2, false));

	// 0 non-accepting, 1 accepting, 2 .. environments
	rel[0][0] = true;
	rel[0][1] = true;
	rel[1][1] = true;
	for (size_t i = 0; i < head.size(); ++i)
	{
		for (size_t j = 0; j < head.size(); ++j)
		{
			if (Env::sim(*head[i], *head[j], sim))
				rel[i + 2][j + 2] = true;
		}
	}
}

void TA::upwardSimulation(
	std::vector<std::vector<bool>>&         rel,
	const Index<size_t>&                    stateIndex,
	const std::vector<std::vector<bool>>&   param) const
{
	LTS lts;
	Index<T> labelIndex;
	this->buildLabelIndex(labelIndex);
	std::vector<std::vector<size_t>> part;
	std::vector<std::vector<bool>> initRel;
	this->upwardTranslation(lts, part, initRel, stateIndex, labelIndex, param);
	OLRTAlgorithm alg(lts);
	// accepting states to block 1
	std::vector<size_t> finalStates;
	stateIndex.translate(finalStates, std::vector<size_t>(finalStates_.begin(), finalStates_.end()));
	alg.fakeSplit(finalStates);
	// environments to blocks 2, 3, ...
	for (size_t i = 0; i < part.size(); ++i)
		alg.fakeSplit(part[i]);
	alg.getRelation().load(initRel);
	alg.init();
	alg.run();
	alg.buildRel(stateIndex.size(), rel);
}

void TA::combinedSimulation(
	std::vector<std::vector<bool>>&           dst,
	const std::vector<std::vector<bool>>&     dwn,
	const std::vector<std::vector<bool>>&     up)
{
	size_t size = dwn.size();
	std::vector<std::vector<bool> > dut(size, std::vector<bool>(size, false));
	for (size_t i = 0; i < size; ++i)
	{
		for (size_t j = 0; j < size; ++j)
		{
			for (size_t k = 0; k < size; ++k)
			{
				if (dwn[i][k] && up[j][k])
				{
					dut[i][j] = true;
					break;
				}
			}
		}
	}
	dst = dut;
	for (size_t i = 0; i < size; ++i)
	{
		for (size_t j = 0; j < size; ++j)
		{
			if (!dst[i][j])
				continue;

			for (size_t k = 0; k < size; ++k)
			{
				if (dwn[j][k] && !dut[i][k])
				{
					dst[i][j] = false;
					break;
				}
			}
		}
	}
}

bool TA::subseteq(const TA& a, const TA& b)
{
	return AntichainExt<T>::subseteq(a, b);
}


void TA::buildStateIndex(Index<size_t>& index) const
{
    for (const TransIDPair* trans : this->transitions_)
    {
        for (size_t state : trans->first.lhs())
        {
            index.add(state);
        }
        index.add(trans->first.rhs());
    }

    for (size_t state : finalStates_)
    {
        index.add(state);
    }
}

void TA::buildSortedStateIndex(Index<size_t>& index) const
{
    std::unordered_set<size_t> s;
    for (const TransIDPair* trans : this->transitions_)
    {
        for (size_t state : trans->first.lhs())
        {
            s.insert(state);
        }
        s.insert(trans->first.rhs());
    }
    s.insert(finalStates_.begin(), finalStates_.end());
    for (size_t state : s)
        index.add(state);
}

void TA::buildLabelIndex(Index<T>& index) const
{
    for (const TransIDPair* trans : this->transitions_)
    {
        index.add(trans->first.label());
    }
}

void TA::buildLhsIndex(Index<const std::vector<size_t>*>& index) const
{
    for (const TransIDPair* trans : this->transitions_)
    {
        index.add(&trans->first.lhs());
    }
}

typename TA::td_cache_type TA::buildTDCache() const
{
    td_cache_type cache;
    for (const TransIDPair* trans : this->transitions_)
    {	// insert all transitions_
        std::vector<const Transition*>& vec = cache.insert(std::make_pair(
            trans->first.rhs(),
            std::vector<const Transition*>())).first->second;
        vec.push_back(&trans->first);
    }

    return cache;
}

typename TA::td_cache_type TA::buildTDCacheWithEmptyRoot() const
{
    td_cache_type cache = this->buildTDCache();
    cache.insert(
        std::make_pair(cEmptyRootTransIndex, std::vector<const Transition*>()));

    return cache;
}

std::vector<const typename TA::Transition*> TA::getEmptyRootTransitions() const
{
    TA::td_cache_type cache = buildTDCacheWithEmptyRoot();

    return cache.at(cEmptyRootTransIndex);
}

void TA::buildBUCache(bu_cache_type& cache) const
{
    std::unordered_set<size_t> s;
    for (const TransIDPair* trans : this->transitions_)
    {
        s.clear();
        for (size_t state : trans->first.lhs())
        {
            if (s.insert(state).second)
            {
                cache.insert(std::make_pair(
                            state, std::vector<const Transition*>())).
                                first->second.push_back(&trans->first);
            }
        }
    }
}

void TA::buildLTCache(lt_cache_type& cache) const
{
    for (const TransIDPair* trans : this->transitions_)
    {
        cache.insert(std::make_pair(trans->first.label(),
                    std::vector<const Transition*>())).
            first->second.push_back(&trans->first);
    }
}

void TA::buildLTCacheExt(
	const TA&                 ta,
	TA::lt_cache_type&        cache,
    T                         lUndef)
{
	for (TA::iterator i = ta.begin(); i != ta.end(); ++i)
	{
		if (i->label()->isData())
		{
			cache.insert(
				make_pair(lUndef, std::vector<const TT*>())
			).first->second.push_back(&*i);
		} else {
			cache.insert(
				make_pair(i->label(), std::vector<const TT*>())
			).first->second.push_back(&*i);
		}
	}
}

void TA::addTransition(
		const std::vector<size_t>&          lhs,
		const T&                            label,
		size_t                              rhs)
{
    //vataAut_.AddTransition(lhs, reinterpret_cast<uintptr_t> (&label), rhs);
    this->internalAdd(Transition(lhs, label, rhs, this->lhsCache()));
}

void TA::addTransition(const Transition& transition)
{
    this->internalAdd(Transition(transition, this->lhsCache()));
}

const typename TA::TransIDPair* TA::addTransition(
		const TransIDPair*       transition)
{
    return this->internalAdd(Transition(transition->first, this->lhsCache()));
}

const typename TA::TransIDPair* TA::addTransition(
    const TransIDPair*               transition,
    const std::vector<size_t>&       index)
{
    return this->internalAdd(Transition(transition->first, index, this->lhsCache()));
}

const typename TA::TransIDPair* TA::addTransition(
    const Transition&                 transition,
    const std::vector<size_t>&        index)
{
    return this->internalAdd(Transition(transition, index, this->lhsCache()));
}

const typename TA::Transition& TA::getTransition(
        const std::vector<size_t>&          lhs,
		const T&                            label,
		size_t                              rhs)
{
    TransIDPair* pair = this->transCache().find(Transition(
                    lhs, label, rhs, this->lhsCache()));
    assert(pair != NULL);
    return getTransitionFromPair(pair);
}

const label_type TA::GetSymbol(const Transition& trans)
{
    return trans.GetSymbol();
}

void TA::copyReachableTransitionsFromRoot(const TA& src,
        const size_t& rootState)
{
    td_cache_type cache = src.buildTDCacheWithEmptyRoot();
    copyReachableTransitionsFromRoot(src, cache, rootState);
}

void TA::copyReachableTransitionsFromRoot(const TA& src,
        td_cache_type cache, const size_t& rootState)
{
    for (td_iterator k = src.tdStart(cache, {rootState});
        k.isValid();
        k.next())
    { // copy reachable transitions
        addTransition(*k);
    }
}

TA::TA(
    Backend&             backend_) :
    nextState_(0),
    finalStates_{},
    backend_(&backend_),
    maxRank_(0),
    transitions_{}
{ }

TA::TA() :
		nextState_(0),
		finalStates_{},
		backend_(),
		maxRank_(0),
		transitions_{}
{
    backend_ = new Backend();
}

TA::TA(
    const TA&         ta) :
    nextState_(ta.nextState_),
    finalStates_(),
    backend_(ta.backend_),
    maxRank_(ta.maxRank_),
    transitions_(ta.transitions_)
{
    for (TransIDPair* trans : this->transitions_)
    {	// copy transitions_
        this->transCache().addRef(trans);
    }
}

/*
template <class T>
template <class F>
TA::TA(
    const TA&         ta,
    F                    f) :
    nextState_(ta.nextState_),
    finalStates_(),
    //vataAut_(),
    backend_(ta.backend_),
    maxRank_(ta.maxRank_),
    transitions_()
{
    for (TransIDPair* trans : ta.transitions_)
    {	// copy transitions_ (only those requested)
        if (f(&trans->first))
        {
            this->addTransition(trans);
        }
    }
}
*/

TA TA::createTAWithSameTransitions(
		const TA&         ta)
{
        return TA(*ta.backend_);
}

TA* TA::allocateTAWithSameTransitions(
		const TA&         ta)
{
        return new TA(*ta.backend_);
}

TA TA::createTAWithSameFinalStates(
        const TA&         ta,
        bool                 copyFinalStates)
{
    TA taNew(ta);
    if (copyFinalStates)
    {
        taNew.finalStates_ = ta.finalStates_;
    }
    return taNew;
}

TA* TA::allocateTAWithSameFinalStates(
        const TA&         ta,
        bool                 copyFinalStates)
{
    TA* taNew = new TA(ta);
    if (copyFinalStates)
    {
        taNew->finalStates_ = ta.finalStates_;
    }
    return taNew;
}

// this is really sad :-(
#include "forestaut.hh"
class TA;
