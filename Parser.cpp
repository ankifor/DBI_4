#include "Parser.hpp"

#include <iostream>
#include <iterator>
#include <algorithm>
#include <cstdlib>
#include <cctype>

using namespace std;

namespace keyword {
	const string Primary  = "primary"  ;
	const string Key      = "key"      ;
	const string Create   = "create"   ;
	const string Table    = "table"    ;
	const string Index    = "index"    ;
	const string Integer  = "integer"  ;
	const string Numeric  = "numeric"  ;
	const string Char     = "char"     ;
	const string Varchar  = "varchar"  ;
	const string Timestamp= "timestamp";
	const string Not      = "not"      ;
	const string Null     = "null"     ;
	const string On       = "on"       ;
	const string Tree     = "tree"     ;
	const string Unique   = "unique"   ;
}

namespace literal {
	const char ParenthesisLeft = '(';
	const char ParenthesisRight= ')';
	const char Comma           = ',';
	const char Semicolon       = ';';
}

template<typename T>
struct NamePredicate {
	const string& name;
	NamePredicate(const string& name) : name(name) {}
	bool operator()(const T& t) const { return t.name == name; }
};

unique_ptr<Schema> Parser::parse() {
	string token;
	unsigned line = 1;
	unique_ptr<Schema> s(new Schema());
	in.open(fileName.c_str());
	if (!in.is_open()) {
		throw ParserError(line, "cannot open file '"+fileName+"'");
	}
	while (in >> token) {
		string::size_type pos;
		string::size_type prevPos = 0;

		while (( pos = token.find_first_of(",)(;", prevPos) ) != string::npos) {
			nextToken(line, token.substr(prevPos, pos-prevPos), *s);
			nextToken(line, token.substr(pos,1), *s);
			prevPos=pos+1;
		}
		nextToken(line, token.substr(prevPos), *s);
		if (token.find("\n")!=string::npos) {
			++line;
		}
	}
	in.close();
	return move(s);
}

static bool isIdentifier(const string& str) {
	if (
		   str==keyword::Primary   
		|| str==keyword::Key       
		|| str==keyword::Create    
		|| str==keyword::Table     
		|| str==keyword::Index     
		|| str==keyword::Integer   
		|| str==keyword::Numeric   
		|| str==keyword::Char      
		|| str==keyword::Varchar   
		|| str==keyword::Timestamp 
		|| str==keyword::Not       
		|| str==keyword::Null      
		|| str==keyword::On        
	) 
	{
		return false;
	}

	return str.find_first_not_of("abcdefghijklmnopqrstuvwxyz_1234567890") == string::npos;
}

static bool isInt(const string& str) {
   return str.find_first_not_of("0123456789") == string::npos;
}

void Parser::nextToken(unsigned line, const string& token, Schema& schema) {
	if (getenv("DEBUG"))
		cerr << line << ": " << token << endl;
	if (token.empty())
		return;
	string tok;
	
	transform(token.begin(), token.end(), back_inserter(tok), ::tolower);
	
	
	switch(state) {
		case State::Semicolon: /* fallthrough */
		case State::Init:
			rel = nullptr;
			if (tok==keyword::Create)
				state=State::Create;
			else
				throw ParserError(line, "Expected 'CREATE', found '"+token+"'");
			break;
		case State::Create:
			if (tok==keyword::Table)
				state=State::Table;
			else if (tok==keyword::Index)
				state=State::Index;
			else
				throw ParserError(line, "Expected 'TABLE', found '"+token+"'");
			break;
		case State::Table:
			if (isIdentifier(tok)) {
				state=State::TableName;
				schema.relations.push_back(Schema::Relation(token));
				rel = &schema.relations.back();
			} else {
				throw ParserError(line, "Expected TableName, found '"+token+"'");
			}
			break;
		case State::TableName:
			if (tok.size()==1 && tok[0]==literal::ParenthesisLeft)
				state=State::CreateTableBegin;
			else
				throw ParserError(line, "Expected '(', found '"+token+"'");
			break;
		case State::Separator: /* fallthrough */
		case State::CreateTableBegin:
			if (tok.size()==1 && tok[0]==literal::ParenthesisRight) {
				state=State::CreateTableEnd;
			} else if (tok==keyword::Primary) {
				state=State::Primary;
				rel->indices.push_back(Schema::Relation::Index());
				rel->primaryKey = rel->indices.size() - 1;
				rel->primaryKeySet = true;
				rel->indices.back().name = "primary_key";
				rel->indices.back().primary = true;
				rel->indices.back().unique = true;
			} else if (isIdentifier(tok)) {
				rel->attributes.push_back(Schema::Relation::Attribute());
				rel->attributes.back().name = token;
				state=State::AttributeName;
			} else {
				throw ParserError(line, "Expected attribute definition, primary key definition or ')', found '"+token+"'");
			}
			break;
		case State::CreateTableEnd:
			if (tok.size()==1 && tok[0]==literal::Semicolon)
				state=State::Semicolon;
			else
				throw ParserError(line, "Expected ';', found '"+token+"'");
			break;
		case State::Primary:
			if (tok==keyword::Key)
				state=State::Key;
			else
				throw ParserError(line, "Expected 'KEY', found '"+token+"'");
			break;
		case State::Key:
			if (tok.size()==1 && tok[0]==literal::ParenthesisLeft)
				state=State::KeyListBegin;
			else if (tok==keyword::Tree) {
				state=State::PrimaryTree;
				rel->indices.back().tree = true;
			} else
				throw ParserError(line, "Expected list of key attributes or 'tree', found '"+token+"'");
			break;
		case State::PrimaryTree:
			if (tok.size()==1 && tok[0]==literal::ParenthesisLeft)
				state=State::KeyListBegin;
			else
				throw ParserError(line, "Expected list of key attributes or 'tree', found '"+token+"'");
			break;			
		case State::KeyListBegin:
			if (isIdentifier(tok)) {
				auto& attributes = rel->attributes;
				auto it = find_if(attributes.begin(), attributes.end(), NamePredicate<Schema::Relation::Attribute>(token));
				if (it == attributes.end())
					throw ParserError(line, "'"+token+"' is not an attribute of '"+rel->name+"'");
				it->primaryFlag = true;
				rel->indices.back().fields.push_back(distance(attributes.begin(), it));
				state=State::KeyName;
			} else {
				throw ParserError(line, "Expected key attribute, found '"+token+"'");
			}
			break;
		case State::KeyName:
			if (tok.size()==1 && tok[0] == literal::Comma)
				state=State::KeyListBegin;
			else if (tok.size()==1 && tok[0] == literal::ParenthesisRight)
				state=State::KeyListEnd;
			else
				throw ParserError(line, "Expected ',' or ')', found '"+token+"'");
			break;
		case State::KeyListEnd:
			if (tok.size()==1 && tok[0] == literal::Comma)
				state=State::Separator;
			else if (tok.size()==1 && tok[0] == literal::ParenthesisRight)
				state=State::CreateTableEnd;
			else
				throw ParserError(line, "Expected ',' or ')', found '"+token+"'");
			break;
		case State::AttributeName:
			if (tok==keyword::Integer) {
				rel->attributes.back().type=Types::Tag::Integer;
				state=State::AttributeTypeInt;
			} else if (tok==keyword::Char) {
				rel->attributes.back().type=Types::Tag::Char;
				state=State::AttributeTypeChar;
			} else if (tok==keyword::Numeric) {
				rel->attributes.back().type=Types::Tag::Numeric;
				state=State::AttributeTypeNumeric;
			} else if (tok==keyword::Varchar) {
				rel->attributes.back().type=Types::Tag::Varchar;
				state=State::AttributeTypeChar;
			} else if (tok==keyword::Timestamp) {
				rel->attributes.back().type=Types::Tag::Timestamp;
				state=State::AttributeTypeInt;
			}
			else {
				throw ParserError(line, "Expected type after attribute name, found '"+token+"'");
			}
			break;
		case State::AttributeTypeChar:
			if (tok.size()==1 && tok[0]==literal::ParenthesisLeft)
				state=State::CharBegin;
			else
				throw ParserError(line, "Expected '(' after 'CHAR', found'"+token+"'");
			break;
		case State::CharBegin:
			if (isInt(tok)) {
				rel->attributes.back().len=atoi(tok.c_str());
				state=State::CharValue;
			} else {
				throw ParserError(line, "Expected integer after 'CHAR(', found'"+token+"'");
			}
			break;
		case State::CharValue:
			if (tok.size()==1 && tok[0]==literal::ParenthesisRight)
				state=State::CharEnd;
			else
				throw ParserError(line, "Expected ')' after length of CHAR, found'"+token+"'");
			break;
		case State::AttributeTypeNumeric:
			if (tok.size()==1 && tok[0]==literal::ParenthesisLeft)
				state=State::NumericBegin;
			else
				throw ParserError(line, "Expected '(' after 'NUMERIC', found'"+token+"'");
			break;
		case State::NumericBegin:
			if (isInt(tok)) {
				rel->attributes.back().len=atoi(tok.c_str());
				state=State::NumericValue1;
			} else {
				throw ParserError(line, "Expected integer after 'NUMERIC(', found'"+token+"'");
			}
			break;
		case State::NumericValue1:
			if (tok.size()==1 && tok[0]==literal::Comma)
				state=State::NumericSeparator;
			else
				throw ParserError(line, "Expected ',' after first length of NUMERIC, found'"+token+"'");
			break;
		case State::NumericValue2:
			if (tok.size()==1 && tok[0]==literal::ParenthesisRight)
				state=State::NumericEnd;
			else
				throw ParserError(line, "Expected ')' after second length of NUMERIC, found'"+token+"'");
			break;
		case State::NumericSeparator:
			if (isInt(tok)) {
				rel->attributes.back().len2=atoi(tok.c_str());
				state=State::NumericValue2;
			} else {
				throw ParserError(line, "Expected second length for NUMERIC type, found'"+token+"'");
			}
			break;
		case State::CharEnd: /* fallthrough */
		case State::NumericEnd: /* fallthrough */
		case State::AttributeTypeInt:
			if (tok.size()==1 && tok[0]==literal::Comma)
				state=State::Separator;
			else if (tok==keyword::Not)
				state=State::Not;
			else if (tok.size()==1 && tok[0]==literal::ParenthesisRight)
				state=State::CreateTableEnd;
			else 
				throw ParserError(line, "Expected ',' or 'NOT NULL' after attribute type, found '"+token+"'");
			break;
		case State::Not:
			if (tok==keyword::Null) {
				rel->attributes.back().notNull=true;
				state=State::Null;
			}
			else 
				throw ParserError(line, "Expected 'NULL' after 'NOT' name, found '"+token+"'");
			break;
		case State::Null:
			if (tok.size()==1 && tok[0]==literal::Comma)
				state=State::Separator;
			else if (tok.size()==1 && tok[0]==literal::ParenthesisRight)
				state=State::CreateTableEnd;
			else 
				throw ParserError(line, "Expected ',' or ')' after attribute definition, found '"+token+"'");
			break;
		case State::Index:
			if (isIdentifier(tok)) {
				state=State::IndexName;
				index_name = token;
			} else {
				throw ParserError(line, "Expected IndexName, found '"+token+"'");
			}
			break;
		case State::IndexName:
			if (tok==keyword::On)
				state=State::IndexOn;
			else
				throw ParserError(line, "Expected 'ON' after index name, found '"+token+"'");
			break;
		case State::IndexOn:
			if (isIdentifier(tok)) {
				// obtain relation
				auto& relations = schema.relations;
				auto it = find_if(relations.begin(), relations.end(), NamePredicate<Schema::Relation>(token));
				
				if (it == schema.relations.end())
					throw ParserError(line, "'"+token+"' is not a table");
				rel = &(*it);
				// add index
				rel->indices.push_back(Schema::Relation::Index());
				rel->indices.back().name = index_name;
				state=State::IndexTable;
			} else {
				throw ParserError(line, "Expected IndexTableName, found '"+token+"'");
			}
			break;
		case State::IndexTable:
			if (tok.size()==1 && tok[0]==literal::ParenthesisLeft)
				state=State::CreateIndexBegin;
			else if (tok==keyword::Unique) {
				state=State::IndexUnique;
				rel->indices.back().unique = true;
			} else if (tok==keyword::Tree) {
				state=State::IndexTree;
				rel->indices.back().tree = true;
			} else
				throw ParserError(line, "Expected '(' or 'unique' or 'tree', found '"+token+"'");
			break;
		case State::IndexUnique:
			if (tok.size()==1 && tok[0]==literal::ParenthesisLeft)
				state=State::CreateIndexBegin;
			else if (tok==keyword::Tree) {
				state=State::IndexTree;
				rel->indices.back().tree = true;
			} else
				throw ParserError(line, "Expected '(' or 'tree', found '"+token+"'");
			break;
		case State::IndexTree:
			if (tok.size()==1 && tok[0]==literal::ParenthesisLeft)
				state=State::CreateIndexBegin;
			else
				throw ParserError(line, "Expected '(', found '"+token+"'");
			break;
		case State::CreateIndexBegin:
			if (isIdentifier(tok)) {
				const auto& attributes = rel->attributes;
				auto it = find_if(attributes.begin(), attributes.end(), NamePredicate<Schema::Relation::Attribute>(token));
				if (it == attributes.end())
					throw ParserError(line, "'"+token+"' is not an attribute of '"+rel->name+"'");

				rel->indices.back().fields.push_back(distance(attributes.begin(), it));
				state=State::IndexField;
			} else {
				throw ParserError(line, "Expected index attribute, found '"+token+"'");
			}			
			break;
		case State::IndexField:
			if (tok.size()==1 && tok[0] == literal::Comma)
				state=State::CreateIndexBegin;
			else if (tok.size()==1 && tok[0] == literal::ParenthesisRight)
				state=State::CreateIndexEnd;
			else
				throw ParserError(line, "Expected ',' or ')', found '"+token+"'");
			break;
		case State::CreateIndexEnd:
			if (tok.size()==1 && tok[0]==literal::Semicolon)
				state=State::Semicolon;
			else
				throw ParserError(line, "Expected ';', found '"+token+"'");
			break;
		default:
			throw;
			break;
  	}
}
