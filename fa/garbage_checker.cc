#include "garbage_checker.hh"

// Code Listener headers
#include <cl/storage.hh>
#include <cl/code_listener.h>

// Forester headers
#include "streams.hh"
#include "error_messages.hh"
#include "programerror.hh"


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
	const std::vector<bool>&          visited)
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

		throw ProgramError(ErrorMessages::GARBAGE_DETECTED, state, loc);
	}
}


void GarbageChecker::check(
	const FAE&                        fae,
	const SymState*                   state) 
{
	// compute reachable roots
	std::vector<bool> visited(fae.getRootCount(), false);
	traverse(fae, visited);

	// check garbage
	checkGarbage(fae, state, visited);
}
