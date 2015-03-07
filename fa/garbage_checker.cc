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
	std::unordered_set<size_t>&       unvisited,
	const bool                        endCheck)
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
			unvisited.insert(i);
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

		std::string msg = endCheck ? ErrorMessages::GARBAGE_DETECTED_EXIT : ErrorMessages::GARBAGE_DETECTED;
		if (!FA_CONTINUE_WITH_GARBAGE)
		{
			throw ProgramError(msg, state, loc);
		}
		else
		{
			FA_ERROR_MSG(loc, msg);
		}
	}
}


void GarbageChecker::removeGarbage(
		FAE&                               fae,
		const std::unordered_set<size_t>&  unvisited)
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

	std::unordered_set<size_t> unvisited;
	// check garbage
	checkGarbage(fae, state, visited, unvisited, false);
}


void GarbageChecker::checkAndRemoveGarbage(
	FAE&                              fae,
	const SymState*                   state,
	const bool                        endCheck,
	std::unordered_set<size_t>&       unvisited)
{
	// compute reachable roots
	std::vector<bool> visited(fae.getRootCount(), false);
	traverse(fae, visited);

	// check garbage
	checkGarbage(fae, state, visited, unvisited, endCheck);
	removeGarbage(fae, unvisited);
}

void GarbageChecker::checkAndRemoveGarbage(
	FAE&                              fae,
	const SymState*                   state,
	const bool                        endCheck)
{
	std::unordered_set<size_t> unvisited;

	checkAndRemoveGarbage(fae, state, endCheck, unvisited);
}

void GarbageChecker::nontraverseCheckAndRemoveGarbage(
	FAE&                              fae,
	const SymState*                   state,
	const std::vector<bool>&          visited)
{
	std::unordered_set<size_t> unvisited;

	GarbageChecker::checkGarbage(fae, state, visited, unvisited);
	GarbageChecker::removeGarbage(fae, unvisited);
}
