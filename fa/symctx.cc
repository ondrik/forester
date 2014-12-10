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

#include "symctx.hh"

size_t SymCtx::size_of_data_ptr = 0;
size_t SymCtx::size_of_code_ptr = 0;

SymCtx::SymCtx(
	const CodeStorage::Fnc& fnc,
	const var_map_type* globalVarMap
) :
	fnc_(fnc),
	sfLayout_{},
	varMap_{},
	regCount_(2),
	argCount_(0)
{
	// pointer to previous stack frame
	sfLayout_.push_back(SelData(ABP_OFFSET, ABP_SIZE, 0, "_pABP"));

	// pointer to context info
	sfLayout_.push_back(SelData(RET_OFFSET, RET_SIZE, 0, "_retaddr"));

	size_t offset = ABP_SIZE + RET_SIZE;

	for (auto& funcVar : fnc_.vars)
	{	// for each variable in the function
		const CodeStorage::Var& var = fnc_.stor->vars[funcVar];

		switch (var.code) {
			case CodeStorage::EVar::VAR_LC:
				if (!SymCtx::isStacked(var)) {
					varMap_.insert(
						std::make_pair(var.uid, VarInfo::createInReg(regCount_++))
					);
					break;
				}
				// no break
			case CodeStorage::EVar::VAR_FNC_ARG:
				NodeBuilder::buildNode(sfLayout_, var.type, offset, var.name);
				varMap_.insert(std::make_pair(var.uid, VarInfo::createOnStack(offset)));
				offset += var.type->size;
				if (var.code == CodeStorage::EVar::VAR_FNC_ARG)
					++argCount_;
				break;
			case CodeStorage::EVar::VAR_GL:
			{ // global variables do not occur at the stack, but we need to track
				// them as they can be used
				if (nullptr != globalVarMap)
				{	// in case we are compiling the function
					FA_NOTE("Compiling global variable " << var.name << " in function "
						<< nameOf(fnc_));

					auto itGlobalVar = globalVarMap->find(var.uid);
					if (globalVarMap->end() == itGlobalVar)
					{ // the variable must be in the global map
						assert(false);
					}

					/// @todo: instead of inserting, why not just initialise varMap_ to
					///        globalVarMap
					varMap_.insert(std::make_pair(var.uid, itGlobalVar->second));
				}

				break;
			}
			default:
				assert(false);
		}
	}
}

SymCtx::SymCtx(const var_map_type* globalVarMap) :
		fnc_(*static_cast<CodeStorage::Fnc*>(nullptr)),
		sfLayout_{},
		varMap_{*globalVarMap},
		regCount_(0),
		argCount_(0)
{
	// Assertions
	assert(nullptr != globalVarMap);
}


void SymCtx::initCtx(const CodeStorage::Storage& stor)
{
	if (stor.types.codePtrSizeof() >= 0)
	{
		size_of_code_ptr = static_cast<size_t>(stor.types.codePtrSizeof());
	}
	else
	{
		size_of_code_ptr = sizeof(void(*)());
	}

	if (stor.types.dataPtrSizeof() >= 0)
	{
		size_of_data_ptr = static_cast<size_t>(stor.types.dataPtrSizeof());
	}
	else
	{
		size_of_data_ptr = sizeof(void*);
	}

	// Post-condition
	assert(size_of_data_ptr > 0);
	assert(size_of_code_ptr > 0);
}

size_t SymCtx::getSizeOfCodePtr()
{
	// Assertions
	assert(size_of_code_ptr > 0);

	return size_of_code_ptr;
}

size_t SymCtx::getSizeOfDataPtr()
{
	// Assertions
	assert(size_of_data_ptr > 0);

	return size_of_data_ptr;
}

bool SymCtx::isStacked(const CodeStorage::Var& var) {
	switch (var.code) {
		case CodeStorage::EVar::VAR_FNC_ARG: return true;
		case CodeStorage::EVar::VAR_LC: return !var.name.empty();
		case CodeStorage::EVar::VAR_GL: return false;
		default: return false;
	}
}

const VarInfo& SymCtx::getVarInfo(size_t id) const
{
	var_map_type::const_iterator i = varMap_.find(id);
	assert(i != varMap_.end());
	return i->second;
}

const SymCtx::StackFrameLayout& SymCtx::GetStackFrameLayout() const
{
	return sfLayout_;
}

const SymCtx::var_map_type& SymCtx::GetVarMap() const
{
	return varMap_;
}

size_t SymCtx::GetRegCount() const
{
	return regCount_;
}

size_t SymCtx::GetArgCount() const
{
	return argCount_;
}

const CodeStorage::Fnc& SymCtx::GetFnc() const
{
	return fnc_;
}
