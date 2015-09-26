#include "nodebuilder.hh"

#include "notimpl_except.hh"
#include "config.h"

namespace {
	const std::string zeroExceptionMssg =
		"Zero size node is not supported. A type is not probably well defined";

	template <class F, class G>
	void internalBuildNode(
		const cl_type*                          type,
		F                                       structFunct,
		G                                       defaultFunct)
	{
		// Assertions
		assert(type != nullptr);


		if (!FA_ALLOW_ZERO_STRUCT && type->size <= 0)
		{
			throw NotImplementedException(zeroExceptionMssg);
		}

		// according to the type
		switch (type->code)
		{
			case cl_type_e::CL_TYPE_STRUCT:  // a structure is flattened
				for (int i = 0; i < type->item_cnt; ++i)
				{
					structFunct(i);
				}
				break;
			case cl_type_e::CL_TYPE_PTR:  // falls through
			case cl_type_e::CL_TYPE_INT:	// falls through
			case cl_type_e::CL_TYPE_BOOL: // falls through
			default:
				defaultFunct();
				break;
		}
	}
}

void NodeBuilder::buildNode(
	std::vector<SelData>&                   nodeInfo,
	const cl_type*                          type,
	int                                     offset,
	const std::string&                      name,
	const alloc_type_e                      allocType)
{
	auto structFunct = [&nodeInfo, &type, &offset, &name, &allocType](int i) -> void {
		const cl_type_item& item = type->items[i];

		std::string ndName = name + '.';
		if (nullptr != item.name)
		{
			ndName += item.name;
		}
		else
		{
			// anonymous names for selectors are permitted only for unions
			assert(nullptr != item.type);
			assert(cl_type_e::CL_TYPE_UNION == item.type->code);

			ndName += "(anon union)";
		}

		NodeBuilder::buildNode(nodeInfo, item.type, offset + item.offset, ndName, allocType);
	};

	auto defaultFunct = [&nodeInfo, &type, &offset, &name, &allocType] () -> void {
			nodeInfo.push_back(SelData(offset, type->size, 0, name, allocType));
	};

	internalBuildNode(type, structFunct, defaultFunct);
}


void NodeBuilder::buildNode(
	std::vector<size_t>&                      nodeInfo,
	const cl_type*                            type,
	int                                       offset,
	const alloc_type_e                        allocType)
{
	auto structFunct = [&nodeInfo, &type, &offset, &allocType](int i) -> void {
			NodeBuilder::buildNode(nodeInfo, type->items[i].type,
				offset + type->items[i].offset, allocType);
	};

	auto defaultFunct = [&nodeInfo, &offset] () -> void {
		nodeInfo.push_back(offset);
	};
	
	internalBuildNode(type, structFunct, defaultFunct);
}


void NodeBuilder::buildNodes(
	std::vector<size_t>&                      nodeInfo,
	const std::vector<const cl_type*>&        components,
	int                                       offset)
{
	for (const cl_type* elem : components)
	{
		assert(nullptr != elem);

		buildNode(nodeInfo, elem, offset);
		offset += elem->size;
	}
}
