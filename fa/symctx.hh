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

#ifndef SYM_CTX_H
#define SYM_CTX_H

// Standard library headers
#include <vector>
#include <unordered_map>

// Code Listener headers
#include <cl/storage.hh>
#include <cl/clutil.hh>

// Forester headers
#include "nodebuilder.hh"
#include "notimpl_except.hh"
#include "streams.hh"
#include "types.hh"
#include "varinfo.hh"

#define ABP_OFFSET		0
#define ABP_SIZE		SymCtx::getSizeOfDataPtr()
#define RET_OFFSET		(ABP_OFFSET + ABP_SIZE)
#define RET_SIZE		SymCtx::getSizeOfCodePtr()

#define GLOBAL_VARS_BLOCK_STR   "__global_vars_block"

/**
 * @brief  Symbolic context of a function
 *
 * This class represents a symbolic context of a function.
 *
 * @todo
 */
class SymCtx
{
public:   // data types

	/// Stack frame layout
	typedef std::vector<SelData> StackFrameLayout;


	/**
	 * @brief  The type that maps identifiers of variables to @p VarInfo
	 *
	 * This type serves as a map between identifiers of variables and @p VarInfo
	 * structures.
	 */
	typedef std::unordered_map<int, VarInfo> var_map_type;


private:  // static data

	/// @todo is @p static really the best option?

	// must be initialised externally!

	/// The size of a data pointer in the analysed program
	static size_t size_of_data_ptr;

	/// The size of a code pointer in the analysed program
	static size_t size_of_code_ptr;


public:   // static methods

	/**
	 * @brief  Initialise the symbolic context
	 *
	 * This static method needs to be called before the @p SymCtx class is
	 * used for the first time. It properly initialises static members of the
	 * class from the passed @p CodeStorage.
	 *
	 * @param[in]  stor  The @p CodeStorage from which the context is to be
	 *                   initialised
	 */
	static void initCtx(const CodeStorage::Storage& stor);

	static size_t getSizeOfCodePtr();

	static size_t getSizeOfDataPtr();

	static bool isStacked(const CodeStorage::Var& var);

private:  // data members

	/// Reference to the function in the @p CodeStorage
	const CodeStorage::Fnc& fnc_;

	/**
	 * @brief  The layout of stack frames
	 *
	 * The layout of stack frames (one stack frame corresponds to one structure
	 * with selectors.
	 */
	StackFrameLayout sfLayout_;

	/// The map of identifiers of variables to @p VarInfo
	var_map_type varMap_;

	/// The number of variables in registers
	size_t regCount_;

	/// The number of arguments of the function
	size_t argCount_;


public:   // methods


	/**
	 * @brief  A constructor of a symbolic context for given function
	 *
	 * This is a constructor that creates a new symbolic context for given
	 * function.
	 *
	 * @param[in]  fnc           The function for which the symbolic context is to
	 *                           be created
	 * @param[in]  globalVarMap  Map of global variables (in case the function is
	 *                           to be compiled, otherwise @p nullptr)
	 */
	SymCtx(
		const CodeStorage::Fnc& fnc,
		const var_map_type* globalVarMap = nullptr);

	/**
	 * @brief  Constructor for @e global context
	 *
	 * The constructor that creates the context for global variables. The function
	 * reference is set to @p NULL reference (which is not very nice).
	 *
	 * @todo: revise the whole * concept
	 */
	SymCtx(const var_map_type* globalVarMap);

	const VarInfo& getVarInfo(size_t id) const;

	const StackFrameLayout& GetStackFrameLayout() const;

	const var_map_type& GetVarMap() const;

	size_t GetRegCount() const;

	size_t GetArgCount() const;

	const CodeStorage::Fnc& GetFnc() const;

};

#endif
