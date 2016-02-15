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

#ifndef LABEL_H
#define LABEL_H

// Standard library headers
#include <vector>
#include <stdexcept>
#include <unordered_map>

// Forester headers
#include "types.hh"
#include "abstractbox.hh"
#include "programerror.hh"

/**
 * @brief  A memory node
 */
struct NodeLabel
{
public:   // data types

	enum class node_type
	{
		n_unknown,
		n_node, // general node -- selectors, boxes, etc.
		n_data, // data
		n_vData // data vector
	};

	/**
	 * @brief  An item in a memory node
	 */
	struct NodeItem
	{
		const AbstractBox* aBox;
		size_t index;
		size_t offset;

		NodeItem(const AbstractBox* aBox, const size_t index, const size_t offset)
			: aBox(aBox), index(index), offset(offset)
		{ }
	};

private:  // data members

	node_type type_;

public:   // data members

	union
	{
		struct
		{
			const Data* data;
			size_t id;
		} data;

		struct
		{
			const std::vector<const AbstractBox*>* v;
			std::unordered_map<size_t, NodeItem>* m;
			const std::vector<SelData>* sels;
			void* tag;
		} node;

		const DataArray* vData;
	};

public:   // methods

	NodeLabel() :
		type_(node_type::n_unknown)
	{ }

	NodeLabel(
		const Data*                                 data,
		size_t                                      id) :
		type_(node_type::n_data)
	{
		this->data.data = data;
		this->data.id = id;
	}

	NodeLabel(
		const std::vector<const AbstractBox*>*      v,
		const std::vector<SelData>*                 sels) :
		type_(node_type::n_node)
	{
		this->node.v = v;
		this->node.m = new std::unordered_map<size_t, NodeItem>();
		this->node.sels = sels;
	}

	NodeLabel(
		const DataArray*                            vData) :
		type_(node_type::n_vData),
		vData(vData)
	{ }

	~NodeLabel()
	{
		if (node_type::n_node == type_)
			delete this->node.m;
	}

	void addMapItem(size_t key, const AbstractBox* aBox, const size_t index,
					const size_t offset)
	{
		if (this->node.m->find(key) != this->node.m->end())
		{
			throw std::runtime_error("Cannot find a given selector");
		}
		assert(this->node.m->find(key) == this->node.m->end());


		this->node.m->insert(std::make_pair(key, NodeItem(aBox, index, offset)));
	}

	bool isData() const
	{
		return node_type::n_data == type_;
	}

	bool isData(const Data*& data) const
	{
		if (node_type::n_data != type_)
			return false;
		data = this->data.data;
		return true;
	}

	const Data& getData() const
	{
		assert(node_type::n_data == type_);
		return *this->data.data;
	}

	size_t getDataId() const
	{
		assert(node_type::n_data == type_);
		return this->data.id;
	}

	const std::vector<SelData>* getSels() const
	{
		assert(node_type::n_node == type_);

		return this->node.sels;
	}

	const AbstractBox* boxLookup(size_t offset, const AbstractBox* def) const
	{
		assert(node_type::n_node == type_);
		auto i = this->node.m->find(offset);
		if (i == this->node.m->end())
			return def;
		return i->second.aBox;
	}

	const NodeItem& nodeLookup(size_t offset) const
	{
		assert(node_type::n_node == type_);
		auto i = this->node.m->find(offset);
		assert(i != this->node.m->end());
		return i->second;
	}

    const AbstractBox* getBoxFromNode(size_t offset) const
    {
        const NodeItem& node = nodeLookup(offset);

        return node.aBox;
    }

	node_type GetType() const
	{
		return type_;
	}

	const DataArray& getVData() const
	{
		assert(node_type::n_vData == type_);
		return *this->vData;
	}

	bool isNode() const
	{
		return node_type::n_node == type_;
	}

	const std::vector<const AbstractBox*>& getNode() const
	{
		assert(node_type::n_node == type_);
		return *this->node.v;
	}

	void* getTag() const
	{
		assert(node_type::n_node == type_);
		return this->node.tag;
	}

	void setTag(void* tag)
	{
		assert(node_type::n_node == type_);
		this->node.tag = tag;
	}

	template <class F>
	bool iterate(F f) const
	{
		assert(node_type::n_node == type_);

		for (size_t i = 0, offset = 0; i < this->node.v->size(); ++i)
		{
			const AbstractBox* aBox = (*this->node.v)[i];

			if (!f(aBox, i, offset))
				return false;

			offset += aBox->getArity();
		}

		return true;
	}

	bool operator<(const NodeLabel& rhs) const
	{
		return this->data.data < rhs.data.data;
	}

	bool operator==(const NodeLabel& rhs) const
	{
		return this->data.data == rhs.data.data;
	}

	bool operator!=(const NodeLabel& rhs) const
	{
		return this->data.data != rhs.data.data;
	}

	friend std::ostream& operator<<(std::ostream& os, const NodeLabel& label);

	friend size_t hash_value(const NodeLabel& label)
	{
		return boost::hash_value(label.data.data);
	}
};


struct label_type
{
	const NodeLabel* obj_;

	label_type() : obj_(nullptr) {}
	label_type(const label_type& label) : obj_(label.obj_) {}
	label_type(const NodeLabel* obj) : obj_(obj) {}
	label_type(uintptr_t obj) : label_type(reinterpret_cast<NodeLabel *>(obj)) {}

	const NodeLabel& operator*() const {
		assert(this->obj_);
		return *this->obj_;
	}

	const NodeLabel* operator->() const {
		assert(this->obj_);
		return this->obj_;
	}

    operator uintptr_t() const {
        assert(this->obj_);
        return reinterpret_cast<uintptr_t>(obj_);
    }

	bool operator<(const label_type& rhs) const {
		return this->obj_ < rhs.obj_;
	}

	bool operator==(const label_type& rhs) const {
		return this->obj_ == rhs.obj_;
	}

	bool operator!=(const label_type& rhs) const {
		return this->obj_ != rhs.obj_;
	}

	friend size_t hash_value(const label_type& label) {
		return boost::hash_value(label.obj_);
	}

	friend std::ostream& operator<<(std::ostream& os, const label_type& label) {
		return os << *label.obj_;
	}
};


namespace std
{
template <>
struct hash<label_type>
{
	size_t operator()(const label_type& label) const
	{
		return boost::hash_value(label.obj_);
	}
};

} // namespace

#endif
