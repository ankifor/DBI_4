#pragma once

#include <exception>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>
#include "Schema.hpp"

using namespace std;

class ParserError : exception {
   string msg;
   unsigned line;
public:
   ParserError(unsigned line, const string& m) : msg(m), line(line) {}
   ~ParserError() throw() {}
   const char* what() const throw() {
      return msg.c_str();
   }
};

struct Parser {
	string fileName;
	ifstream in;
	enum class State : unsigned { 
		Init, 
		Semicolon,
		Create, 
			//TABLE
			Table, CreateTableBegin, CreateTableEnd, TableName, 
			Primary, Key, KeyListBegin, KeyName, KeyListEnd, PrimaryTree,
			AttributeName, 
				AttributeTypeInt, 
				AttributeTypeChar, CharBegin, CharValue, CharEnd, 
				AttributeTypeNumeric, NumericBegin, NumericValue1, NumericSeparator, NumericValue2, NumericEnd,
				Not, Null, 
			Separator,
			//INDEX
			Index, CreateIndexBegin, CreateIndexEnd, IndexName, IndexOn, IndexTable, IndexField, IndexUnique, IndexTree
	};
	State state;
	Schema::Relation* rel;
	string index_name;
	Parser(const string& fileName) : fileName(fileName), state(State::Init), rel(nullptr) {}
	~Parser() {};
	unique_ptr<Schema> parse();
private:
	void nextToken(unsigned line, const string& token, Schema& s);
};
