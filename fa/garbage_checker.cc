#include "garbage_checker.hh"

// Code Listener headers
#include <cl/storage.hh>
#include <cl/code_listener.h>

// Forester headers
#include "config.h"
#include "error_messages.hh"
#include "programerror.hh"
#include "streams.hh"


void GarbageChecker::traverse(
	const FAE&                        fae,
	std::vector<bool>&                visited)
{
	visited = std::vector<bool>(fae.getRootCount(), false);

	for (const Data& var : fae.GetVariables())
	{
		// skip everything what is not a root reference
		if (!var.isRef())
			continue;

		size_t root = var.d_ref.root;

		// check whether we traversed this one before
		if (visited[root])
			continue;

		fae.connectionGraph.visit(root, visited);
	}
}


void GarbageChecker::checkGarbage(
	const FAE&                        fae,
	const SymState*                   state,
	const std::vector<bool>&          visited,
	std::vector<size_t>&              unvisited)
{
	bool garbage = false;

	for (size_t i = 0; i < fae.getRootCount(); ++i)
	{
		if (!fae.getRoot(i))
			continue;

		if (!visited[i])
		{
			FA_DEBUG_AT(1, "the root " << i << " is not referenced anymore ... "
				<< fae.connectionGraph.data[i]);

			garbage = true;
			unvisited.push_back(i);
		}
	}

	if (garbage)
	{
		const cl_loc* loc = nullptr;
		if (nullptr != state &&
			nullptr != state->GetInstr() &&
			nullptr != state->GetInstr()->insn())
		{
			loc = &state->GetInstr()->insn()->loc;
		}

		if (!FA_CONTINUE_WITH_GARBAGE)
		{
			throw ProgramError(ErrorMessages::GARBAGE_DETECTED, state, loc);
		}
		else
		{
			FA_ERROR_MSG(loc, ErrorMessages::GARBAGE_DETECTED);
		}
	}
}


void GarbageChecker::removeGarbage(
		FAE&                             fae,
		const std::vector<size_t>&       unvisited)
{
	for (size_t i : unvisited)
	{
		fae.setRoot(i, nullptr);
	}
}


void GarbageChecker::check(
	const FAE&                        fae,
	const SymState*                   state) 
{
	// compute reachable roots
	std::vector<bool> visited(fae.getRootCount(), false);
	traverse(fae, visited);

	std::vector<size_t> unvisited;
	// check garbage
	checkGarbage(fae, state, visited, unvisited);
}


void GarbageChecker::checkAndRemoveGarbage(
	FAE&                              fae,
	const SymState*                   state)
{
	// compute reachable roots
	std::vector<bool> visited(fae.getRootCount(), false);
	traverse(fae, visited);

	std::vector<size_t> unvisited;
	// check garbage
	checkGarbage(fae, state, visited, unvisited);
	removeGarbage(fae, unvisited);
}
