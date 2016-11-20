#include "Schema.hpp"

#include <sstream>
#include <iostream>

using namespace std;

extern string ReplaceString(string subject, const string& search, const string& replace);
extern void ReplaceStringInPlace(string& subject, const string& search, const string& replace);

string type(const Schema::Relation::Attribute& attr) {
	Types::Tag type = attr.type;
	switch(type) {
		case Types::Tag::Integer:
			return "Integer";
		case Types::Tag::Timestamp:
			return "Timestamp";			
		case Types::Tag::Numeric: {
			stringstream ss;
			ss << "Numeric<" << attr.len << "," << attr.len2 << ">";
			return ss.str();
		}
		case Types::Tag::Char: {
			stringstream ss;
			ss << "Char<" << attr.len << ">";
			return ss.str();
		}
		case Types::Tag::Varchar: {
			stringstream ss;
			ss << "Varchar<" << attr.len << ">";
			return ss.str();
		}
	}
	throw;
}

string Schema::toString() const {
	stringstream out;
	string delim;
	for (const Schema::Relation& rel : relations) {
		out << rel.name << endl;
		// fields
		for (const auto& attr : rel.attributes) {
			out << '\t' << attr.name << ' ' << type(attr) << ' ' << (attr.notNull ? "not null" : "") << endl;
		}
		// indices
		for (const auto& ind : rel.indices) {
			out << "\tIndex " << ind.name << "(" << (ind.unique? "unique,": "non-unique,") << (ind.tree? "tree" : "hash")  << "):";
			delim = "";
			for (unsigned keyId : ind.fields) {
				out << delim << rel.attributes[keyId].name;
				delim = ",";
			}
			out << endl;
		}
	}
	return out.str();
}

string Schema::Relation::hppTableDeclaration() const {
	stringstream out;
	string delim;
	if (attributes.size() == 0) return out.str();
	
	out << "struct Table_" << name << " {" << endl;
	for (const auto& attr : attributes) {
		out << "\tvector<" << type(attr) << "> " <<  attr.name << ";" << (attr.primaryFlag? " //primary": "") << endl;
	}
	out << endl;
	// print indices
	string type_index;
	for (const auto& ind : indices) {
		//print fields as a comment
		out << "\t// " << ind.name << ": "; 
		delim = "";
		for (unsigned keyId : ind.fields) {
			out << delim << attributes[keyId].name;
			delim = ", ";
		}
		out << endl;
		//declare tuple type for index
		type_index = "type_" + ind.name;
		out << "\tusing " << type_index << " = tuple<";
		string delim = "";
		for (unsigned keyId : ind.fields) {
			out << delim << type(attributes[keyId]);
			delim = ",";
		}
		out << ">;" << endl;
		//declare index
		if (ind.unique) {
			if (ind.tree) {
				out << "\tmap<" << type_index << ",Tid> " << ind.name << ";" << endl;
			} else {
				out << "\tunordered_map<" << type_index << ",Tid,hash_types::hash<" << type_index <<  ">> " << ind.name << ";" << endl;
			}
		} else {
			if (ind.tree) {
				out << "\tmultimap<" << type_index << ",Tid> " << ind.name << ";" << endl;
			} else {
				out << "\tunordered_multimap<" << type_index << ",Tid,hash_types::hash<" << type_index <<  ">> " << ind.name << ";" << endl;
			}
		}
	}
	out << endl;
	//size()
	out << "\tsize_t size() {return " << attributes[0].name << ".size();}" << endl;
	//read_from_file()
	out << "\tvoid read_from_file(ifstream& in);" << endl;
	//insert()
	out << "\tTid insert(";
	delim = "";
	for (auto& attr : attributes) {
		out << delim << type(attr) << " in_" << attr.name; 
		delim = ",";
	}
	out << ");" << endl;
	
	out << "\tvoid remove(Tid tid);" << endl;
	out << "};" << endl;
	return out.str();
}

string Schema::Relation::cppTableImplementation() const {
	stringstream out;
	string delim;
	string indent;
	string tmplt;
	
	if (attributes.size() == 0) return out.str();
	// insert method
	{
		// signature begin
		out << indent << "Tid Table_" << name << "::" << "insert(";
		delim = "";
		for (auto& attr : attributes) {
			out << delim << type(attr) << " in_" << attr.name; 
			delim = ",";
		}
		out << ")" << endl;
		// signature end
		out << indent << "{" << endl;
		indent.push_back('\t');
		out << indent << "Tid new_tid = size();" << endl;
		// indices check
		for (auto& ind : indices) {
			if (ind.unique) {
				out << indent << "check_key(" << ind.name;
				for (unsigned keyId : ind.fields) {
					out << ", in_" << attributes[keyId].name;
				}
				out << ");" << endl;
			}
		}
		// indices insert
		for (auto& ind : indices) {
			out << indent << "insert_key<false>(" << ind.name << ", new_tid";
			for (unsigned keyId : ind.fields) {
				out << ", in_" << attributes[keyId].name;
			}
			out << ");" << endl;
		}
		// push back fields
		tmplt = "&name;.push_back(in_&name;);";
		for (auto& attr : attributes) {
			out << indent << ReplaceString(tmplt, "&name;", attr.name) << endl;
		}
		//return
		out << indent << "return new_tid;" << endl;
		indent.pop_back();
		out << indent << "}" << endl;
	}
	out << endl;
	// remove method
	{
		// signature begin
		out << indent << "void Table_" << name << "::" << "remove(Tid tid)" << endl;
		// signature end
		out << indent << "{" << endl;
		indent.push_back('\t');
		out << indent << "Tid last_tid = size() - 1;" << endl;
		out << indent << "assert(tid <= last_tid);" << endl;
		// remove tid from indices
		for (const auto& ind : indices) {
			out << indent << "remove_key(" << ind.name << ",tid";
			for (unsigned keyId : ind.fields) {
				out << "," << attributes[keyId].name << "[tid]";
			}
			out << ");" << endl;
		}
		//swap with the last element
		{
			out << indent << "if (tid != last_tid) {" << endl;
			indent.push_back('\t');
			// update indices for last_tid
			for (const auto& ind : indices) {
				out << indent << "replace_key(" << ind.name << ",last_tid,tid";
				for (unsigned keyId : ind.fields) {
					out << "," << attributes[keyId].name << "[last_tid]";
				}
				out << ");" << endl;
			}
			// move fields
			tmplt = "&name;[tid] = &name;[last_tid];";
			for (const auto& attr : attributes) {
				out << indent << ReplaceString(tmplt, "&name;", attr.name) << endl;
			}
			indent.pop_back();
			out << indent << "}" << endl;
		}
		// pop back fields
		for (const auto& attr : attributes) {
			out << indent << attr.name << ".pop_back();" << endl;
		}
		
		indent.pop_back();
		out << indent << "}" << endl;
	}
	out << endl;
	// read_from_file method
	{
		// signature begin
		out << indent << "void Table_" << name << "::" << "read_from_file(ifstream& in)" << endl;
		// signature end
		out << indent << "{" << endl;
		indent.push_back('\t');
		out << indent << "assert(in.is_open());" << endl;
		// buffers
		{
			out << indent << "string buf_field; buf_field.reserve(500);" << endl;			
			for (const auto& attr : attributes) {
				out << indent << type(attr) << " in_" << attr.name << ";" << endl;
			}
		}
		//reading loop
		{
			out << indent << "char first_symbol = 0;" << endl;
			out << indent << "while (!in.eof()) {" << endl;
			indent.push_back('\t');
			out << indent << "first_symbol = in.get();" << endl;
			out << indent << "if (first_symbol == ROW_DLM || first_symbol == EOF) continue;" << endl;
			out << indent << "in.unget();" << endl;
			out << indent << "assert(!in.fail());" << endl;
			out << endl;
			// read row from file
			tmplt = "getline(in, buf_field, &delim;); in_&name; = &type;::castString(buf_field.data(), buf_field.length());";
			string buf;
			for (size_t i = 0; i < attributes.size(); ++i) {
				const auto& attr = attributes[i];
				buf = ReplaceString(tmplt, "&name;", attr.name);
				ReplaceStringInPlace(buf, "&type;", type(attr));
				ReplaceStringInPlace(buf, "&delim;", (i == attributes.size()-1? "ROW_DLM": "FLD_DLM"));
				out << indent << buf << endl;
			}
			out << endl;
			// make insertion
			out << indent << "insert(";
			delim = "";
			for (auto& attr : attributes) {
				out << delim << "in_" << attr.name;
				delim = ",";
			}
			out << ");" << endl;
			indent.pop_back();
			out << indent << "}" << endl;
		}
		indent.pop_back();
		out << indent << "}" << endl;
	}
	out << endl;
	
	return out.str();
}

string Schema::hppFilePrint() const {
	stringstream out;
	
	out << "#pragma once"             << endl;
	out << "#include \"Types.hpp\""   << endl;
	out << "#include <tuple>"         << endl;
	out << "#include <vector>"        << endl;
	out << "#include <unordered_map>" << endl;
	out << "#include <map>"           << endl;
	out << "#include <fstream>"       << endl;
	out                               << endl; 
	out << "using namespace std;"     << endl;
	out                               << endl; 
	
	for (const Schema::Relation& rel : relations) {
		out << rel.hppTableDeclaration() << endl;
	}
	out << endl;
	
	for (const Schema::Relation& rel : relations) {
		out << "extern Table_" << rel.name << " " << rel.name << ";" << endl;
	}
	
	return out.str();
}

string Schema::cppFilePrint(const string& hpp_file) const {
	stringstream out;

	out << "#include \"" << hpp_file << "\"" << endl;
	
	out << "#include \"Types.hpp\""   << endl;
	out << "#include \"Help.hpp\""    << endl;
	out << "#include <cassert>"       << endl;
	out << "#include <string>"        << endl;
	out << "#include <utility>"       << endl;
	out << endl; 
 
	out << "using namespace std;"     << endl;
	out << endl; 
	
	out << "static const char FLD_DLM = '|';" << endl;
	out << "static const char ROW_DLM = '\\n';" << endl;
	out << endl;
	
	for (const Schema::Relation& rel : relations) {
		out << rel.cppTableImplementation() << endl;
	}
	
	return out.str();
}