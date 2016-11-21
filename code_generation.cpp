#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <assert.h>
#include "code_generation.h"


using namespace std;


//==============================================================================
// Helpers
//==============================================================================
extern string type(const Schema::Relation::Attribute& attr);
extern string ReplaceString(string subject, const string& search, const string& replace);
extern void ReplaceStringInPlace(string& subject, const string& search, const string& replace);

struct AttrPredicate {
	string name;
	AttrPredicate(string name) : name(name) {}
	bool operator()(const Schema::Relation::Attribute& t) const { return t.name == name; }
};

struct RelPredicate {
	string name;
	RelPredicate(string name) : name(name) {}
	bool operator()(const Schema::Relation& r) const { return r.name == name; }
};

class Name_Generator {
	unordered_map<string,size_t> used_names;
public:
	string request_name(const string& suggested, bool with_number) {
		auto it = used_names.find(suggested);
		size_t num = 0;
		if (it == used_names.end()) {
			num = 0;
			used_names.insert(make_pair(suggested,num));
		} else {
			num = ++it->second;
			with_number = true;
		}
		return suggested + (with_number? to_string(num) : "");
	}
} name_generator;


//void remove_not_required_fields(vector<Field_Unit>& produced, const vector<Field_Unit>& required) {
//	for (auto it = produced.begin(); it != produced.end(); ) {
//		auto it_req = find(required.begin(), required.end(), *it);
//		if (it_req == required.end()) {
//			// not found => not required
//			it = produced.erase(it);
//		} else {
//			++it;
//		}
//	}
//}
//
//void check_produced_for_required_fields(const vector<Field_Unit>& produced, const vector<Field_Unit>& required) {
//	for (auto it = required.begin(); it != required.end(); ++it) {
//		auto it_prod = find(produced.begin(), produced.end(), *it);
//		assert(it_prod != produced.end());
//	}
//}


//==============================================================================
// Context
//==============================================================================

const Schema::Relation* Context::getTabDef(string tabname) const {
	auto it = find_if(schema.relations.begin(), schema.relations.end(), RelPredicate(tabname));
	if (it == schema.relations.end()) {
		return nullptr;
	}
	return &(*it);
}

const Schema::Relation::Attribute* Context::getAttr(const Field_Unit& fu) const {
	auto rel = getTabDef(fu.tab_name);
	if (rel == nullptr) {
		return nullptr;
	}
	auto it = find_if(rel->attributes.begin(), rel->attributes.end(), AttrPredicate(fu.field_name));
	if (it == rel->attributes.end()) {
		return nullptr;
	}
	return &(*it);
}

//==============================================================================
// OperatorScan
//==============================================================================
OperatorScan::OperatorScan(const Context* context, stringstream& out, string tabname) 
	: OperatorUnary(context,out)
	, tabname(tabname) 
{
	tid_name = name_generator.request_name("tid", true);
}
		
void OperatorScan::computeProduced() {
	
	auto def = *context->getTabDef(tabname);
	
	for (auto it = fields.begin(); it != fields.end(); ) {
		auto it_tab = find_if(def.attributes.begin(), def.attributes.end(), AttrPredicate(it->field_name));
		if (it_tab == def.attributes.end() || !it->tab_name.empty() && it->tab_name != tabname) {
			// not present in table
			it = fields.erase(it);
		} else {
			it->tab_name = tabname;
			it->type_name = type(*it_tab);
			it->token = it->tab_name + "." + it->field_name + "[" + tid_name + "]";
			++it;
		}
	}
	assert(fields.size() > 0);
}

void OperatorScan::produce() {
	const string tmplt = "for (Tid &tid; = 0;&tid; < &tab;.size(); ++&tid;)";
	string tmp = ReplaceString(tmplt,"&tid;",tid_name);
	ReplaceStringInPlace(tmp, "&tab;", tabname);
	out << tmp << "{";
	consumer->consume(this);
	out << "}";
}

//==============================================================================
// OperatorPrint
//==============================================================================

void OperatorPrint::consume(const Operator* caller) {
	auto produced = input->getProduced();	
	out << "cout<<";
	string delim = "";
	for (auto& f : produced) {
		out << delim << f.token << "<<";
		delim = "\",\"<<";
	}
	out << "endl;";
}

//==============================================================================
// OperatorProjection
//==============================================================================
void OperatorProjection::computeProduced() {
	auto produced = input->getProduced();
	for (auto it = fields.begin(); it != fields.end(); ) {
		auto it_prod = find(produced.begin(), produced.end(), *it);
		if (it_prod == produced.end()) {
			assert(false);//should not happen
		} else {
			*it = *it_prod;
			++it;
		}
	}
}


//==============================================================================
// OperatorSelect
//==============================================================================
void OperatorSelect::computeRequired() {
	fields = consumer->getRequired();
	//add fields from conditions, if needed
	for (auto& f : condition.fields) {
		auto it = find(fields.begin(), fields.end(), f);
		if (it == fields.end()) {
			fields.push_back(f);
		}
	}
}

void OperatorSelect::computeProduced() {
	auto produced = input->getProduced();
	for (auto it = fields.begin(); it != fields.end(); ) {
		auto it_prod = find(produced.begin(), produced.end(), *it);
		if (it_prod == produced.end()) {
			it = fields.erase(it);//not produced
		} else {
			*it = *it_prod;
			++it;
		}
	}
}

void OperatorSelect::consume(const Operator* caller) {
	auto produced = getProduced();
	out << "if (" << condition.predicat << "(";
	string delim = "";
	for (auto& f : condition.fields) {
		auto it = find(produced.begin(), produced.end(), f);
		assert(it != produced.end());
		out << delim << it->token;
		delim = ",";
	}
	out << ")){";
	consumer->consume(this);
	out << "}";
}



//==============================================================================
// OperatorHashJoin
//==============================================================================

OperatorHashJoin::OperatorHashJoin(
	const Context* context, stringstream& out
	, const vector<Field_Unit>& left_fields
	, const vector<Field_Unit>& right_fields
) : OperatorBinary(context, out), left_fields(left_fields), right_fields(right_fields)
{
	hash_name = name_generator.request_name("hash", true);
	iterator_name = name_generator.request_name("it", true);
}

void OperatorHashJoin::computeRequired() {
	fields = consumer->getRequired();
	//add fields from left if needed
	for (const auto& f : left_fields) {
		auto it = find(fields.begin(), fields.end(), f);
		if (it == fields.end()) {
			fields.push_back(f);
		}
	}
	//add fields from right if needed
	for (const auto& f : right_fields) {
		auto it = find(fields.begin(), fields.end(), f);
		if (it == fields.end()) {
			fields.push_back(f);
		}
	}
}

void OperatorHashJoin::computeProduced() {
	size_t ind_val = 0;
	string it_key_name = iterator_name + ".first->first";
	string it_val_name = iterator_name + ".first->second";
	// refresh fields from left without change
	auto& produced_left = left->getProduced();
	for (auto& f : fields) {
		auto it = find(produced_left.begin(), produced_left.end(), f);
		if (it != produced_left.end()) {
			f = *it;
			auto it_key = find(left_fields.begin(), left_fields.end(), f);
			if (it_key != left_fields.end()) {
				f.token = "get<" + to_string(distance(left_fields.begin(), it_key)) + ">(" + it_key_name + ")";
			} else {
				f.token = "get<" + to_string(ind_val++) + ">(" + it_val_name + ")";
			}
		}
	}
	// refresh fields from right without change
	auto& produced_right = right->getProduced();
	for (auto& f : fields) {
		auto it = find(produced_right.begin(), produced_right.end(), f);
		if (it != produced_right.end()) {
			f = *it;
		}
	}
	// delete fileds without token
	for (auto it = fields.begin(); it != fields.end(); ) {
		if (it->token.empty()) {
			it = fields.erase(it);//not produced
		} else {
			++it;
		}
	}	
}

void OperatorHashJoin::produce(){
	string delim;
	auto& produced = left->getProduced();
	//key definition
	string type_key = name_generator.request_name("type_key", true);
	{
		out << "using " << type_key << "=tuple<";
		delim = "";
		for (const auto& f : left_fields) {
			auto it = find(produced.begin(),produced.end(),f);
			out << delim << it->type_name;
			delim = ",";
		}
		out << ">;";
	}
	//value definition
	string type_val = name_generator.request_name("type_val", true);
	{
		out << "using " << type_val << "=tuple<";
		delim = "";
		for (const auto& f : produced) {
			auto it = find(left_fields.begin(),left_fields.end(),f);
			if (it == left_fields.end()) {
				out << delim << f.type_name;
				delim = ",";
			}
		}
		out << ">;";
	}
	//hash definition
	out << "unordered_multimap<" 
		<< type_key 
		<< "," << type_val 
		<< "," << "hash_types::hash<" + type_key + ">> "
		<< hash_name << ";";
	//call produce of children
	left->produce();
	right->produce();
}

void OperatorHashJoin::consumeLeft() {
	string delim = "";
	auto& produced = left->getProduced();
	
	out << hash_name << ".insert(make_pair(";
	{
		out << "make_tuple(";
		delim = "";
		for (const auto& f : left_fields) {
			auto it = find(produced.begin(),produced.end(),f);
			out << delim << it->token;
			delim = ",";
		}
		out << ")";
	}
	out << ",";
	{
		out << "make_tuple(";
		delim = "";
		for (const auto& f : produced) {
			auto it = find(left_fields.begin(),left_fields.end(),f);
			if (it == left_fields.end()) {
				out << delim << f.token;
				delim = ",";
			}
		}		
		out << ")";
	}
	out << "));";
}

void OperatorHashJoin::consumeRight() {
	string delim = "";
	auto& produced = right->getProduced();
	out << "auto " << iterator_name << "="
		<< hash_name << ".equal_range(make_tuple(";
	{
		delim = "";
		for (const auto& f : right_fields) {
			auto it = find(produced.begin(),produced.end(),f);
			out << delim << it->token;
			delim = ",";
		}
	}
	out << "));";
	out << "while (" << iterator_name << ".first != " << iterator_name << ".second){";
	consumer->consume(this);
	out << "++" << iterator_name << ".first;}";
}