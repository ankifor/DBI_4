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


void remove_not_required_fields(vector<Field_Unit>& produced, const vector<Field_Unit>& required) {
	for (auto it = produced.begin(); it != produced.end(); ) {
		auto it_req = find(required.begin(), required.end(), *it);
		if (it_req == required.end()) {
			// not found => not required
			it = produced.erase(it);
		} else {
			++it;
		}
	}
}

void check_produced_for_required_fields(const vector<Field_Unit>& produced, const vector<Field_Unit>& required) {
	for (auto it = required.begin(); it != required.end(); ++it) {
		auto it_prod = find(produced.begin(), produced.end(), *it);
		assert(it_prod != produced.end());
	}
}


//==============================================================================
// Context
//==============================================================================

const Schema::Relation* Context::getTabDef(string tabname) const {
	auto it = tab_instances.find(tabname);
	if (it == tab_instances.end()) {
		return nullptr;
	}
	return &schema.relations[it->second];
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
	produced = getRequired();
	auto def = *context->getTabDef(tabname);
	
	for (auto it = produced.begin(); it != produced.end(); ) {
		if (it->tab_name.empty()) {
			it->tab_name = tabname;
		}
		assert(it->tab_name == tabname);
		auto it_tab = find_if(def.attributes.begin(), def.attributes.end(), AttrPredicate(it->field_name));
		if (it_tab == def.attributes.end()) {
			// not present in table
			it = produced.erase(it);
		} else {
			it->type_name = type(*it_tab);
			it->token = it->tab_name + "." + it->field_name + "[" + tid_name + "]";
			++it;
		}
	}
	assert(produced.size() > 0);
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

void OperatorPrint::consume(const Operator*) {
	auto produced = input->getProduced();	
	out << "cout<<";
	string delim = "";
	for (const Field_Unit& fu : produced) {
		out << delim << fu.token << "<<";
		delim = "\",\"<<";
	}
	out << "endl;";
}

//==============================================================================
// OperatorProjection
//==============================================================================
void OperatorProjection::computeProduced() {
	produced = input->getProduced();
	remove_not_required_fields(produced, getRequired());
	check_produced_for_required_fields(produced, fields);
	assert(produced.size() == fields.size());
}


//==============================================================================
// OperatorSelect
//==============================================================================
void OperatorSelect::computeRequired() {
	required = consumer->getRequired();
	//add fields from conditions, if needed
	for (auto f : condition.fields) {
		auto it = find(required.begin(), required.end(), f);
		if (it == required.end()) {
			required.push_back(f);
		}
	}
}

void OperatorSelect::consume(const Operator* caller) {
	auto pr = getProduced();
	out << "if (" << condition.predicat << "(";
	string delim = "";
	for (auto f : condition.fields) {
		auto it = find(pr.begin(), pr.end(), f);
		assert(it != pr.end());
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
	required = consumer->getRequired();
	//add fields from left if needed
	for (auto f : left_fields) {
		auto it = find(required.begin(), required.end(), f);
		if (it == required.end()) {
			required.push_back(f);
		}
	}
	//add fields from right if needed
	for (auto f : right_fields) {
		auto it = find(required.begin(), required.end(), f);
		if (it == required.end()) {
			required.push_back(f);
		}
	}
}

void OperatorHashJoin::computeProduced() {
	size_t ind = 0;	
	auto produced_left = left->getProduced();
	hash_insert = hash_name + ".insert(make_pair(";
	// process key of hash_table
	{
		string type_key = name_generator.request_name("type_key", true);
		string it_key = iterator_name + ".first->first";
		hash_definition += "using " + type_key + "=tuple<";
		hash_insert += "make_tuple(";
		
		ind = 0;
		delim = "";
		for (auto f : left_fields) {
			auto it = find(produced_left.begin(),produced_left.end(),f);
			produced.push_back(*it);
			produced.back().token = "get<" + (ind++) + ">(" + it_key + ");"
			
			hash_definition += delim + it->type_name;
			hash_insert += delim + it->token;
			delim = ",";
		}
		hash_definition += ">;";//close type_key definition
		hash_insert += ")";//close make_tuple()
	}
	hash_insert += ",";
	// process value of hash_table
	{
		string it_val = iterator_name + ".first->second";
		string type_val = name_generator.request_name("type_val", true);
		
		hash_definition += "using " + type_val << "=tuple<";
		hash_insert += "make_tuple(";
		
		ind = 0;
		delim = "";
		for (auto f : produced_left) {
			auto it = find(left_fields.begin(),left_fields.end(),f);
			if (it != left_fields.end()) continue;
			
			produced.push_back(f);
			produced.back().token = "get<" + (ind++) + ">(" + it_val + ");"
			
			hash_definition += delim + f.type_name;
			hash_insert += delim + f.token;
			delim = ",";
		}
		hash_definition += ">;";//close type_val definition
		hash_insert += ")";//close make_tuple()
	}
	hash_insert += "));";//close insert
	//define multimap
	hash_definition += 
		"unordered_multimap<" 
		+ type_key 
		+ "," + type_val 
		+ "," + "hash_types::hash<" + type_key + ">> "
		+ hash_name + ";";
	// add fields from right without change
	for (auto f : right->getProduced()) {
		auto it = find(produced.begin(), produced.end(), f);
		assert(it == produced.end());
		produced.push_back(f);
	}
}

void OperatorHashJoin::produce(){
	out << hash_definition;
	// produce
	left->produce();
	right->produce();
}


void OperatorHashJoin::consume(const Operator* caller){
	string delim = "";
	if (caller == left) {
		out << hash_insert;
	} else {//TODO
		//auto t = make_tuple(order.o_w_id[tid], order.o_d_id[tid], order.o_c_id[tid]);
		out << "auto t = make_tuple(";
		delim = "";
		auto TIDs_right = *right->getTIDs();
		for (auto t : right_fields) {
			auto it = find_if(TIDs_right.begin(), TIDs_right.end(), TabPredicate<TID_Unit>(t.tab));
			assert(it != TIDs_right.end());
			out << delim 
				<< context->getTabName(t.tab) 
				<< "." << context->getAttr(t.tab,t.attr).name
				<< "[" << it->name << "]";
			delim = ",";
		}
		out << ");";
		//auto it = customer_wdc.find(t);
		out << "for(auto it = " << hash_name << ".equal_range(t);"
			<< "it.first != it.second;"
			<< "++it.first) {";
		auto TIDs_left = *left->getTIDs();
		for (size_t i = 0; i < TIDs_left.size(); ++i) {
			out << "auto " << TIDs_left[i].name 
				<< "= get<" << i << ">(it.first->second);";
		}
		consumer->consume(this);
		out << "}";
	}
}