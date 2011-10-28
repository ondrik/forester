/*
 * Copyright (C) 2011 Kamil Dudka <kdudka@redhat.com>
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

#include "symtrace.hh"

#include <cl/cl_msg.hh>
#include <cl/cldebug.hh>
#include <cl/storage.hh>

#include "plotenum.hh"
#include "worklist.hh"

#include <algorithm>
#include <fstream>

#include <boost/foreach.hpp>

namespace Trace {

// /////////////////////////////////////////////////////////////////////////////
// implementation of Trace::NodeBase

NodeBase::~NodeBase() {
    BOOST_FOREACH(Node *parent, parents_)
        parent->notifyDeath(this);
}

Node* NodeBase::parent() const {
    CL_BREAK_IF(1 != parents_.size());
    return parents_.front();
}


// /////////////////////////////////////////////////////////////////////////////
// implementation of Trace::Node

void Node::notifyBirth(NodeBase *child) {
    children_.push_back(child);
}

void Node::notifyDeath(NodeBase *child) {
    // remove the dead child from the list
    children_.erase(
            std::remove(children_.begin(), children_.end(), child),
            children_.end());

    if (children_.empty())
        // FIXME: this may cause stack overflow on complex trace graphs
        delete this;
}


// /////////////////////////////////////////////////////////////////////////////
// implementation of Trace::NodeHandle

void NodeHandle::reset(Node *node) {
    // release the old node
    Node *&ref = parents_.front();
    ref->notifyDeath(this);

    // register the new node
    ref = node;
    ref->notifyBirth(this);
}


// /////////////////////////////////////////////////////////////////////////////
// implementation of Trace::plotTrace()

typedef const Node                     *TNode;
typedef std::pair<TNode, TNode>         TNodePair;

struct TracePlotter {
    std::ostream                        &out;

    TracePlotter(std::ostream &out_):
        out(out_)
    {
    }
};

// FIXME: copy-pasted from symplot.cc
#define SL_QUOTE(what) "\"" << what << "\""

void NullNode::plotNode(TracePlotter &tplot) const {
    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=box, color=red, fontcolor=red, label="
        << SL_QUOTE(origin_) << "];\n";
}

void RootNode::plotNode(TracePlotter &tplot) const {
    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=circle, color=black, fontcolor=black, label=\"start\"];\n";
}

void InsnNode::plotNode(TracePlotter &tplot) const {
    const char *color = (isBuiltin_)
        ? "blue"
        : "black";

    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=plaintext, fontname=monospace, fontcolor="
        << color << ", label="
        << SL_QUOTE((*insn_)) << "];\n";
}

void AbstractionNode::plotNode(TracePlotter &tplot) const {
    const char *label;
    switch (kind_) {
        case OK_SLS:
            label = "SLS abstraction";
            break;

        case OK_DLS:
            label = "DLS abstraction";
            break;

        default:
            label = "XXX unknown abstraction";
            CL_BREAK_IF("unknown abstraction");
    }

    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=ellipse, color=red, fontcolor=red, label="
        << SL_QUOTE(label) << "];\n";
}

void ConcretizationNode::plotNode(TracePlotter &tplot) const {
    // TODO: kind_
    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=ellipse, color=red, fontcolor=blue, label="
        << SL_QUOTE("concretizeObj()") << "];\n";
}

void SpliceOutNode::plotNode(TracePlotter &tplot) const {
    // TODO: kind_, successful_
    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=ellipse, color=red, fontcolor=blue, label="
        << SL_QUOTE("spliceOut*()") << "];\n";
}

void JoinNode::plotNode(TracePlotter &tplot) const {
    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=circle, color=red, fontcolor=red, label=\"join\"];\n";
}

void CloneNode::plotNode(TracePlotter &tplot) const {
    tplot.out << "\t" << SL_QUOTE(this) << " [shape=doubleoctagon, color=black"
        ", fontcolor=black, label=\"clone\"];\n";
}

void CallEntryNode::plotNode(TracePlotter &tplot) const {
    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=box, fontname=monospace, color=blue, fontcolor=blue"
        ", penwidth=3.0, label=\"--> call entry: " << (*insn_) << "\"];\n";
}

void CallSurroundNode::plotNode(TracePlotter &tplot) const {
    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=box, fontname=monospace, color=blue, fontcolor=blue"
        ", label=\"--- call frame: " << (*insn_) << "\"];\n";
}

void CallDoneNode::plotNode(TracePlotter &tplot) const {
    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=box, fontname=monospace, color=blue, fontcolor=blue"
        ", penwidth=3.0, label=\"<-- call done: "
        << (nameOf(*fnc_)) << "()\"];\n";
}

void CondNode::plotNode(TracePlotter &tplot) const {
    tplot.out << "\t" << SL_QUOTE(this) << " [shape=box, fontname=monospace";

    if (determ_)
        tplot.out << ", color=green";
    else
        tplot.out << ", color=red";

    tplot.out << ", fontcolor=black, label=\"" << (*inCmp_) << " ... ";

    if (determ_)
        tplot.out << "evaluated as ";
    else
        tplot.out << "assuming ";

    if (branch_)
        tplot.out << "TRUE";
    else
        tplot.out << "FALSE";

    tplot.out << "\"];\n";
}

void MsgNode::plotNode(TracePlotter &tplot) const {
    const char *color = "red";
    const char *label;
    switch (level_) {
        case ML_DEBUG:
            color = "black";
            label = "ML_DEBUG";
            break;

        case ML_WARN:
            color = "gold";
            label = "ML_WARN";
            break;

        case ML_ERROR:
            label = "ML_ERROR";
            break;

        default:
            label = "ML_XXX";
            CL_BREAK_IF("unhandled EMsgLevel in MsgNode");
    }

    tplot.out << "\t" << SL_QUOTE(this)
        << " [shape=tripleoctagon, fontcolor=monospace, color="
        << color << ", fontcolor=red, label="
        << SL_QUOTE((*loc_) << label) << "];\n";
}

void plotTraceCore(TracePlotter &tplot, Node *endPoint) {
    TNodePair item(/* from */ endPoint, /* to */ 0);

    WorkList<TNodePair> wl(item);
    while (wl.next(item)) {
        const TNode now = /* from */ item.first;
        const TNode to  = /* to   */ item.second;
        item.second = now;

        BOOST_FOREACH(TNode from, now->parents()) {
            item.first = from;
            wl.schedule(item);
        }

        now->plotNode(tplot);
        if (!to)
            continue;

        tplot.out << "\t" << SL_QUOTE(now)
            << " -> " << SL_QUOTE(to)
            << " [color=black, fontcolor=black];\n";
    }
}

// FIXME: copy-pasted from symplot.cc
bool plotTrace(Node *endPoint, TLoc loc) {
    PlotEnumerator *pe = PlotEnumerator::instance();
    std::string plotName(pe->decorate("symtrace"));
    std::string fileName(plotName + ".dot");

    // create a dot file
    std::fstream out(fileName.c_str(), std::ios::out);
    if (!out) {
        CL_ERROR("unable to create file '" << fileName << "'");
        return false;
    }

    // open graph
    out << "digraph " << SL_QUOTE(plotName)
        << " {\n\tlabel=<<FONT POINT-SIZE=\"18\">" << plotName
        << "</FONT>>;\n\tlabelloc=t;\n";

    // check whether we can write to stream
    if (!out.flush()) {
        CL_ERROR("unable to write file '" << fileName << "'");
        out.close();
        return false;
    }

    // initialize an instance of PlotData
    TracePlotter tplot(out);

    // do our stuff
    plotTraceCore(tplot, endPoint);

    // close graph
    out << "}\n";
    out.close();
    if (loc)
        CL_NOTE_MSG(loc, "trace graph dumped to '" << fileName << "'");
    else
        CL_DEBUG("symtrace: trace graph dumped to '" << fileName << "'");

    return !!out;
}

/// this runs in the debug build only
bool isRootNodeReachble(Node *const from) {
    Node *node = from;
    WorkList<Node *> wl(node);
    while (wl.next(node)) {
        if (dynamic_cast<RootNode *>(node))
            return true;

        BOOST_FOREACH(Node *pred, node->parents())
            wl.schedule(pred);
    }

    CL_ERROR("isRootNodeReachble() returns false");
    plotTrace(from);
    return false;
}


} // namespace Trace
