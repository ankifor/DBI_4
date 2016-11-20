#pragma once

#include <vector>
#include <string>
#include "Types.hpp"

using namespace std;

struct Schema {
	struct Relation {
		struct Attribute {
			string name;
			Types::Tag type;
			unsigned len;
			unsigned len2;
			bool notNull;
			bool primaryFlag;
			Attribute() : len(~0), len2(~0), notNull(true), primaryFlag(false) {}
		};
		struct Index {
			vector<unsigned> fields;
			string name;
			bool unique;
			bool tree;
			bool primary;
			Index() : unique(false), tree(false), primary(false) {}
		};
		
		string name;
		vector<Schema::Relation::Attribute> attributes;
		size_t primaryKey;
		bool primaryKeySet;
		vector<Schema::Relation::Index> indices;
		Relation(const string& name) : name(name), primaryKey(0), primaryKeySet(false) {}
		string hppTableDeclaration() const;
		string cppTableImplementation() const;
	};
	vector<Schema::Relation> relations;
	string toString() const;
	string hppFilePrint() const;
	string cppFilePrint(const string& hpp_file) const;

};