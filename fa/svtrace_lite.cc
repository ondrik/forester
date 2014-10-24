#include "svtrace_lite.hh"

// code listener headers
#include <cl/storage.hh>

// std headers
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <exception>
#include <assert.h>

namespace
{

	// XML initialization
	const std::string START       = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n\
	<graphml xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n\
		<key attr.name=\"lineNumberInOrigin\" attr.type=\"int\" for=\"edge\" id=\"originline\"/>\n\
		<key attr.name=\"originFileName\" attr.type=\"string\" for=\"edge\" id=\"originfile\">\n\
			<default>\"&lt;command-line&gt;\"</default>\n\
		</key>\n\
		<key attr.name=\"nodeType\" attr.type=\"string\" for=\"node\" id=\"nodetype\">\n\
			<default>path</default>\n\
		</key>\n\
		<key attr.name=\"isFrontierNode\" attr.type=\"boolean\" for=\"node\" id=\"frontier\">\n\
			<default>false</default>\n\
		</key>\n\
		<key attr.name=\"isViolationNode\" attr.type=\"boolean\" for=\"node\" id=\"violation\">\n\
			<default>false</default>\n\
		</key>\n\
		<key attr.name=\"isEntryNode\" attr.type=\"boolean\" for=\"node\" id=\"entry\">\n\
			<default>false</default>\n\
		</key>\n\
		<key attr.name=\"isSinkNode\" attr.type=\"boolean\" for=\"node\" id=\"sink\">\n\
			<default>false</default>\n\
		</key>\n\
		<key attr.name=\"enterFunction\" attr.type=\"string\" for=\"edge\" id=\"enterFunction\"/>\n\
		<key attr.name=\"returnFromFunction\" attr.type=\"string\" for=\"edge\" id=\"returnFrom\"/>\n\
		<graph edgedefault=\"directed\">\n\
			<data key=\"sourcecodelang\">C</data>\n";


	const std::string END         = "</graph>\n</graphml>";
	const std::string DATA_START  = "<data key=";
	const std::string DATA_END    = "</data>\n";
	const std::string NODE_START  = "<node id=";
	const std::string NODE_END    = "</node>";
	const std::string NODE_ENTRY  = "<data key=\"entry\">true</data>";
	const std::string NODE_NAME   = "A";
	const std::string VIOLATION   = "<data key=\"violation\">true";
	const std::string EDGE_START  = "<edge source=";
	const std::string EDGE_END    = "</edge>";
	const std::string EDGE_TARGET = " target=";


	bool instrsEq(
			const CodeStorage::Insn*                       instr1,
			const CodeStorage::Insn*                       instr2)
	{
		return instr1->loc.line == instr2->loc.line &&
			instr1->loc.column >= instr2->loc.column &&
			instr1->code == instr2->code;
	}


	void printStart(
			std::ostream&                                  out)
	{
		out << START;
	}


	void printEnd(
			std::ostream&                                  out,
			int                                            endNode)
	{
		out << "\t" << NODE_START << "\"" << NODE_NAME << endNode << "\">\n";
		out << "\t" << VIOLATION << DATA_END;
		out << "\t" << NODE_END << "\n";
		out << "\t" << END;
	}
}


void SVTraceLite::printTrace(
			const std::vector<const CodeStorage::Insn*>&   instrs,
			std::ostream&                                  out)
{
	printStart(out);

	for (size_t i = 0; i < instrs.size(); ++i)
	{
		const auto instr = instrs.at(i);
		const int lineNumber = instr->loc.line;
		
		if (i+1 < instrs.size() && instrsEq(instr, instrs.at(i+1)))
		{
			continue;
		}

		if (nodeNumber_ == 1)
		{
			out << "\t" << NODE_START << "\"" << NODE_NAME << nodeNumber_ << "\">\n\t"
				<< NODE_ENTRY << "\n\t" << NODE_END <<"\n";
		}
		else
		{
			out << "\t" << NODE_START << "\"" << NODE_NAME << nodeNumber_ << "\"/>\n";
		}
	
		const std::string indent("\t\t\t");
		out << "\t\t" << EDGE_START << "\"" << NODE_NAME << nodeNumber_ << "\"" <<
			EDGE_TARGET << "\""<<  NODE_NAME << nodeNumber_+1 << "\">\n";

		out << indent << DATA_START << "\"originfile\">\""<< instr->loc.file << "\"" << DATA_END;
		out << indent << DATA_START << "\"originline\">"<< lineNumber << DATA_END;
		out << "\t" << EDGE_END << "\n";

		++nodeNumber_;
	}
	
	printEnd(out, nodeNumber_);
}
