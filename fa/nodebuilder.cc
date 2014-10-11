#include "nodebuilder.hh"

void NodeBuilder::buildNode(
	std::vector<SelData>& nodeInfo,
	const cl_type* type,
	int offset,
	const std::string& name)
{
	// Assertions
	assert(type != nullptr);
	assert(type->size > 0);

	// according to the type
	switch (type->code)
	{
		case cl_type_e::CL_TYPE_STRUCT:  // a structure is flattened
			for (int i = 0; i < type->item_cnt; ++i)
			{
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

				NodeBuilder::buildNode(nodeInfo, item.type, offset + item.offset, ndName);
			}
			break;

		case cl_type_e::CL_TYPE_PTR:  // falls through
		case cl_type_e::CL_TYPE_INT:	// falls through
		case cl_type_e::CL_TYPE_BOOL: // falls through
		default:
			nodeInfo.push_back(SelData(offset, type->size, 0, name));
			break;
	}
}


void NodeBuilder::buildNode(
	std::vector<size_t>& nodeInfo,
	const cl_type* type,
	int offset)
{
	// Assertions
	assert(type != nullptr);
	assert(type->size > 0);

	// according to the type
	switch (type->code)
	{
		case cl_type_e::CL_TYPE_STRUCT:  // a structure is flattened
			for (int i = 0; i < type->item_cnt; ++i)
			{
				NodeBuilder::buildNode(nodeInfo, type->items[i].type,
					offset + type->items[i].offset);
			}
			break;

		case cl_type_e::CL_TYPE_PTR:  // falls through
		case cl_type_e::CL_TYPE_INT:  // falls through
		case cl_type_e::CL_TYPE_BOOL: // falls through
		default:
			nodeInfo.push_back(offset);
			break;
	}
}

void NodeBuilder::buildNode(
	std::vector<size_t>& nodeInfo,
	const std::vector<const cl_type*>& components,
	int offset)
{
	for (const cl_type* elem : components)
	{
		assert(nullptr != elem);

		buildNode(nodeInfo, elem, offset);
		offset += elem->size;
	}
}
