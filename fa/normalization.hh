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

#ifndef NORMALIZATION_H
#define NORMALIZATION_H

// Standard library headers
#include <vector>
#include <map>

// Forester headers
#include "forestautext.hh"
#include "abstractbox.hh"
#include "streams.hh"
#include "utils.hh"
#include "bu_intersection.hh"

class SymState;

class Normalization
{
private:
	using TreeAutVec = std::vector<std::shared_ptr<const TreeAut>>;

public:
	struct NormalizationInfo {
	private:
		using RootStatePair = std::pair<size_t, size_t>;
		struct RootNormalizationInfo {
			std::set<RootStatePair> joinStates_;
			std::unordered_set<size_t> rootsMerging_;

			RootNormalizationInfo() :
					joinStates_(),
					rootsMerging_()
			{}
		};
	public:
		std::unordered_map<size_t, struct RootNormalizationInfo> rootsNormalizationInfo_;
		std::unordered_map<size_t, size_t> rootMapping_;
		size_t lastAddedRoot_;
		size_t backupLast_;

		NormalizationInfo() :
				rootsNormalizationInfo_(),
				rootMapping_(),
				lastAddedRoot_(0),
				backupLast_(0)
		{}

		void addNewRoot(const size_t root)
		{
			rootsNormalizationInfo_[root] = RootNormalizationInfo();
			backupLast_ = lastAddedRoot_;
			lastAddedRoot_ = root;
		}

		void addRootIfNotExists(const size_t root)
		{
			if (rootsNormalizationInfo_.count(root) == 0)
			{
				addNewRoot(root);
			}
		}

		void addMapping(const size_t from, const size_t to)
		{
			assert(!rootMapping_.count(from));

			rootMapping_[from] = to;
		}

		void addMergedRoot(const size_t root, const size_t mergedRoot)
		{
			assert(rootsNormalizationInfo_.count(root));

			rootsNormalizationInfo_.at(root).rootsMerging_.insert(mergedRoot);
		}

		void addMergedRootToLastRoot(const size_t mergedRoot)
		{
			assert(rootsNormalizationInfo_.count(lastAddedRoot_));

			rootsNormalizationInfo_.at(lastAddedRoot_).rootsMerging_.insert(mergedRoot);
		}

		void addJoinState(const size_t root, const size_t mergedRoot, const size_t state)
		{
			assert(rootsNormalizationInfo_.count(root));

			rootsNormalizationInfo_.at(root).joinStates_.insert(
					RootStatePair(mergedRoot, state));
		}

		void addJoinStateToLastRoot(const size_t root, const size_t state)
		{
			assert(rootsNormalizationInfo_.count(lastAddedRoot_));

			rootsNormalizationInfo_.at(lastAddedRoot_).joinStates_.insert(
					RootStatePair(root, state));
		}

		void mergeRoots(const size_t root, const size_t mergedRoot)
		{
			assert(rootsNormalizationInfo_.count(root));
			if (rootsNormalizationInfo_.count(mergedRoot) == 0)
			{
				return;
			}

			rootsNormalizationInfo_.at(root).rootsMerging_.insert(
					rootsNormalizationInfo_.at(mergedRoot).rootsMerging_.begin(),
					rootsNormalizationInfo_.at(mergedRoot).rootsMerging_.end()
			);
			assert(rootsNormalizationInfo_.at(root).rootsMerging_.size() >=
				   rootsNormalizationInfo_.at(mergedRoot).rootsMerging_.size());

			rootsNormalizationInfo_.at(root).joinStates_.insert(
					rootsNormalizationInfo_.at(mergedRoot).joinStates_.begin(),
					rootsNormalizationInfo_.at(mergedRoot).joinStates_.end()
			);
			assert(rootsNormalizationInfo_.at(root).joinStates_.size() >=
					rootsNormalizationInfo_.at(mergedRoot).joinStates_.size());
		}

		void removeLastRoot()
		{
			assert(rootsNormalizationInfo_.count(lastAddedRoot_));

			rootsNormalizationInfo_.erase(lastAddedRoot_);
			lastAddedRoot_ = backupLast_;
		}

		bool containsMergedRoot(const size_t root) const
		{
			return rootsNormalizationInfo_.count(root) > 0;
		}

		std::unordered_set<size_t> collectMergedRoots(const size_t root, const size_t merged)
		{
			assert(root != merged);
			if (rootsNormalizationInfo_.count(merged) == 0)
			{
				return std::unordered_set<size_t>();
			}

			std::unordered_set<size_t> mergedSet;
			for (const size_t nextMerged : rootsNormalizationInfo_.at(merged).rootsMerging_)
			{
				const auto tmp = collectMergedRoots(merged, nextMerged);
				mergedSet.insert(tmp.begin(), tmp.end());
			}

			for (const size_t m : mergedSet)
			{
				mergeRoots(root, m);
			}

			return mergedSet;
		}

		void finalize()
		{
			std::unordered_set<size_t> tobeRemoved;
			for (const auto& rootInfo : rootsNormalizationInfo_)
			{
				if (rootInfo.second.rootsMerging_.size() == 0 &&
						rootInfo.second.joinStates_.size() == 0)
				{
					tobeRemoved.insert(rootInfo.first);
					continue;
				}
			}

			for (const size_t state : tobeRemoved)
			{
				if (rootsNormalizationInfo_.count(state))
				{
					remove(state);
				}
			}
		}

		void clear()
		{
			rootsNormalizationInfo_.clear();
			rootMapping_.clear();
		}

		bool empty() const
		{
			return rootsNormalizationInfo_.size() == 0 &&
				   rootMapping_.size() == 0;
		}

		void createIdentityMapping(const size_t roots)
		{
			for (size_t i=0; i < roots; ++i)
			{
				rootMapping_[i] = i;
			}
		}

		size_t getSize() const
		{
			return rootsNormalizationInfo_.size();
		}

		void remove(const size_t root)
		{
			assert(rootsNormalizationInfo_.count(root) != 0);
			rootsNormalizationInfo_.erase(root);
		}

		friend std::ostream& operator<<(std::ostream& os, const NormalizationInfo& info)
		{
			for (const auto& rootInfo : info.rootsNormalizationInfo_)
			{
				os << "Root " << rootInfo.first << " contains: [";
				for (const auto& merged : rootInfo.second.rootsMerging_)
					os << merged << ", ";
				os << "], with join states: [";
				for (const auto& joinStatePair : rootInfo.second.joinStates_)
					os << joinStatePair.first << ":" << joinStatePair.second << ", ";
				os << "]\n";
			}

			os << "Mapping is ["; for (const auto& p : info.rootMapping_)
					os << p.first << " -> " << p.second << ", ";
			os << "]\n";

			return os;
		}
	};

private:  // data members

	FAE& fae;

	/// the corresponding symbolic state
	const SymState* state_;

protected:

	TreeAut* mergeRoot(
		TreeAut&                          dst,
		size_t                            ref,
		std::shared_ptr<const TreeAut>    srcPtr,
		std::vector<size_t>&              joinStates);


	void traverse(
		std::vector<bool>&                visited,
		std::vector<size_t>&              order,
		std::vector<bool>&                marked) const;


	/**
	 * @brief  Normalizes given root recursively
	 *
	 * This method performs recursively normalization of all components reachable
	 * from given root. This only removes redundant root points (and preserves
	 * only root points which are real cutpoints), reordering of roots is
	 * performed on a higher level.
	 *
	 * @param[in,out]  normalized  Vector marking root points which are normalized
	 * @param[in]      root        The root to be normalized
	 * @param[in]      marked      Bitmap telling which root points are referenced
	 *                             more than once
	 */
	void normalizeRoot(
		std::vector<bool>&                normalized,
		const size_t                      root,
		const std::vector<bool>&          marked,
		NormalizationInfo&                normalizationInfo,
		bool                              ignoreVars = false);


	bool selfReachable(
		size_t                            root,
		size_t                            self,
		const std::vector<bool>&          marked);


	/**
	 * @brief  Transforms the forest automaton into a canonicity-respecting form
	 *
	 * This method transforms the corresponding forest automaton into
	 * a canonicity-respecting form. This means that root points correspond to
	 * real cutpoints (other are merged) and the order of the root points is
	 * according to the depth-first traversal.
	 *
	 * @param[in]  marked  Vector marking roots which are referred more than once
	 * @param[in]  order   Vector with root indices in the right order
	 *
	 * @returns  @p true in case some components were merged, @p false otherwise
	 */
	bool normalizeInternal(
		const std::vector<bool>&          marked,
		const std::vector<size_t>&        order,
		NormalizationInfo&                normalizationInfo,
		bool                              ignoreVars = false);

public:

	void scan(
		std::vector<bool>&                marked,
		std::vector<size_t>&              order,
		const std::set<size_t>&           forbidden = std::set<size_t>(),
		const bool                        extended = false);


	static bool normalize(
		FAE&                              fae,
		const SymState*                   state,
		NormalizationInfo&                normalizationInfo,
		const std::set<size_t>&           forbidden = std::set<size_t>(),
		bool                              extended = false,
		bool                              ignoreVars = false);

	static bool normalize(
		FAE&                              fae,
		const SymState*                   state,
		const std::set<size_t>&           forbidden = std::set<size_t>(),
		bool                              extended = false,
        bool                              ignoreVars = false);

	static bool normalizeWithoutMerging(
		FAE&                              fae,
		const SymState*                   state,
		NormalizationInfo&                normInfo,
		const std::set<size_t>&           forbidden = std::set<size_t>(),
		bool                              extended = false);

    static TreeAutVec revertNormalization(
        const BUIntersection::BUProductResult& buProductResult,
		FAE& newFAE,
        const Normalization::NormalizationInfo& info);

	/**
	 * @brief  Computes the indices of components which are not to be merged
	 *
	 * This function computes the set of indices of components of the forest
	 * automaton @p fae which are not to be merged or folded
	 *
	 * @param[in]  fae  The forest automaton
	 *
	 * @returns  The set with indices of components not to be merged or folded
	 */
	static std::set<size_t> computeForbiddenSet(
			const FAE& fae,
			const bool ignoreNearby = false,
			const bool ignoreVars = false);

public:   // methods

	Normalization(FAE& fae, const SymState* state) :
		fae(fae), state_{state}
	{ }
};

#endif
