#include "svtrace_printer.hh"

// code listener headers
#include <cl/storage.hh>

// std headers
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

extern "C"
{
	#include "sparse/token.h"
}


// XML initialization
const std::string SVTracePrinter::START = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n\
<graphml xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"http://graphml.graphdrawing.org/xmlns\">\n\
    <key attr.name=\"assumption\" attr.type=\"string\" for=\"edge\" id=\"assumption\"/>\n\
    <key attr.name=\"sourcecode\" attr.type=\"string\" for=\"edge\" id=\"sourcecode\"/>\n\
    <key attr.name=\"sourcecodeLanguage\" attr.type=\"string\" for=\"graph\" id=\"sourcecodelang\"/>\n\
    <key attr.name=\"tokenSet\" attr.type=\"string\" for=\"edge\" id=\"tokens\"/>\n\
    <key attr.name=\"originTokenSet\" attr.type=\"string\" for=\"edge\" id=\"origintokens\"/>\n\
    <key attr.name=\"negativeCase\" attr.type=\"string\" for=\"edge\" id=\"negated\">\n\
        <default>false</default>\n\
    </key>\n\
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



const std::string SVTracePrinter::END = "\t</graph>\n</graphml>";
const std::string SVTracePrinter::DATA_START = "\t\t\t<data key=";
const std::string SVTracePrinter::DATA_END = "</data>\n";
const std::string SVTracePrinter::NODE_START = "<node id=";
const std::string SVTracePrinter::NODE_END = "</node>";
const std::string SVTracePrinter::ENTRY = "<data key=\"entry\">true</data>";
const std::string SVTracePrinter::VIOLATION = "<data key=\"violation\">true";
const std::string SVTracePrinter::EDGE_START = "edge source=";
const std::string SVTracePrinter::EDGE_END = "</edge>";
const std::string SVTracePrinter::EDGE_TARGET = " target=";


void SVTracePrinter::printTrace(
			const std::vector<const CodeStorage::Insn*>&   instrs,
			std::ostream&                                  out,
			const char*                                    filename)
{
	out << START;
		
	std::ifstream in;
	in.open(filename, std::ifstream::in);
	struct token end;
	struct token* begin = letTokenize(filename, &end);
	struct token* next = begin;
	int lastLine = 0;

	for (const auto instr : instrs)
	{
		const int lineNumber = instr->loc.line;
		if (lineNumber == lastLine)
		{
			continue;
		}
		next = getNext(begin, next, lineNumber);
		std::string line;
		jumpToLine(in, lastLine, lineNumber, line);
		lastLine = lineNumber;
		if (line.length() == 0)
		{
			continue;
		}

		if (nodeNumber_ == 1)
		{
			out << "\t" << NODE_START << "A" << nodeNumber_ << "\">\n\t" << ENTRY << "\n\t" << NODE_END <<"\n";
		}
		else
		{
			out << "\t" << NODE_START << "A" << nodeNumber_ << "/>\n";
		}
		printTokensOfLine(
				out,
				filename,
				line,
				findToken(next, &end, lineNumber+1),
				&end);
		++nodeNumber_;
	}

	out << "\t" << NODE_START << "A" << nodeNumber_ << "\">\n";
    out << "\t" << VIOLATION << DATA_END;
    out << "\t" << NODE_END << "\n";

	out << END;
	return;
}



void SVTracePrinter::printTokensOfLine(
			std::ostream&                                 out,
			const char*                                   filename,
			const std::string&                            line,
			struct token*                                 act,
			const struct token*                           end)
{
	if (act == end || line.length() == 0)
	{
		return;
	}

	const int lineNumber = act->pos.line;
	const int startTokenNumber = tokenNumber_;
	std::string code = "";

	for (struct token* i = act; i->pos.line == lineNumber && i != end; i = i->next)
	{
		++tokenNumber_;
		code += getToken(i, line);
		code += "\n";
	}

	out << "\t\t" << EDGE_START << "A" << nodeNumber_ << EDGE_TARGET << "A" << nodeNumber_+1 << "\">\n";
	out << DATA_START << "\"sourcecode\">" << code << "\t\t\t" << DATA_END;
	if (startTokenNumber == tokenNumber_)
	{
		out << DATA_START <<"\"tokens\">" << tokenNumber_ << DATA_END;
	}
	else
	{
		out << DATA_START << "\"tokens\">" << startTokenNumber << "," << tokenNumber_ << DATA_END;
	}
	out << DATA_START << "\"originfile\">\""<< filename << "\"" << DATA_END;
	if (startTokenNumber == tokenNumber_)
	{
		out << DATA_START << "\"origintokens\">" << tokenNumber_ << DATA_END;
	}
	else
	{
		out << DATA_START << "\"tokens\">" << startTokenNumber << "," << tokenNumber_ << DATA_END;
	}
    out << DATA_START << "\"originline\">"<< lineNumber << DATA_END;
	out << "\t" << EDGE_END << "\n";

	return;
}


struct token* SVTracePrinter::getNext(
			struct token*                               begin,
			struct token*                               next,
			const int                                   line)
{
	return ((line < next->pos.line) ? begin : next);
}


struct token* SVTracePrinter::findToken(
			struct token*                               next,
			const struct token*                         end,
			const int                                   line)
{
	struct token* i = next;
	for (; i != end && i->pos.line != line; i = i->next);

	return i;
}


struct token* SVTracePrinter::letTokenize(
		const char*                                     filename,
		struct token*                                   end)
{
	const char *next_path[2] = {
		"",
		NULL
	};
	int fd = open(filename, O_RDONLY);
	struct token* begin = tokenize(filename, fd, end, next_path);
	close(fd);

	return begin;
}


std::string SVTracePrinter::getToken(
		const struct token*                            token,
		const std::string&                             line)
{
	const int end = (token->pos.line != token->next->pos.line) ? line.length() : token->next->pos.pos;
	std::string res = "";

	for (int i = token->pos.pos+1; i < end && line[i-1] != '\n'; ++i)
	{
		res += line[i-1];
	}
	res += "\0";

	return res;
}

void SVTracePrinter::jumpToLine(
			std::ifstream&                                 in,
			const int                                      actLine,
			const int                                      nextLine,
			std::string&                                   line)
{
	for (int i = actLine; i < nextLine; ++i)
	{
		std::getline(in, line);
	}
}

