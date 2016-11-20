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

//struct OperatorHashJoin : public OperatorBinary {
//	vector<Field_Unit> left_fields;
//	vector<Field_Unit> right_fields;
//	vector<Field_Unit> required;
//	vector<Field_Unit> produced;
//	vector<TID_Unit> TIDs;
//};

//void OperatorHashJoin::produce(){
//	computeRequired();
//	string delim ;
////	using type_wdc = tuple<Integer,Integer,Integer>;//w_id,d_id,c_id
////	unordered_map<type_wdc,Tid,hash_types::hash<type_wdc>> customer_wdc;
//	tuple_typename = name_generator.request_name("type_tuple");
//	tuple_tids = name_generator.request_name("type_tids");
//	hash_name = name_generator.request_name("hash");
//	// tuple_typename definition
//	out << "using " << tuple_typename << "=tuple<";
//	delim = "";
//	for (auto t : left_fields) {
//		out << delim << type(context->getAttr(t.tab,t.attr));
//		delim = ",";
//	}
//	out << ">;";
//	// tuple_tids definition
//	out << "using " << tuple_tids << "=tuple<";
//	delim = "";
//	for (auto t : *left->getTIDs()) {
//		out << delim << "Tid";
//		delim = ",";
//	}
//	out << ">;";
//	// unordered_multimap definition	
//	out << "unordered_multimap<" 
//	<< tuple_typename 
//	<< "," << tuple_tids 
//	<< "," << "hash_types::hash<" << tuple_typename << ">> " 
//	<< hash_name << ";";
//	
//	left->produce();
//	right->produce();
//}
//
//void OperatorHashJoin::computeRequired() {
//	required = *consumer->getRequired();
//	for (const Field_Unit& t : left_fields) {
//		auto it = find(required.cbegin(), required.cend(), t);
//		if (it == required.end()) required.push_back(t);
//	}
//	for (const Field_Unit& t : right_fields) {
//		auto it = find(required.cbegin(), required.cend(), t);
//		if (it == required.end()) required.push_back(t);
//	}
//	OperatorBinary::computeRequired();
//}
//
//void OperatorHashJoin::computeProduced() {
//	OperatorBinary::computeProduced();
//	produced = *left->getProduced();
//	
//	for (const Field_Unit& t : *right->getProduced()) {
//		auto it = find(produced.cbegin(), produced.cend(), t);
//		if (it == produced.end()) produced.push_back(t);
//	}
//}
//
//void OperatorHashJoin::computeTIDs() {
//	OperatorBinary::computeTIDs();
//	TIDs = *left->getTIDs();
//	for (auto t : *right->getTIDs()) {
//		TIDs.push_back(t);
//	}
//}
//
//void OperatorHashJoin::consume(const Operator* caller){
//	string delim = "";
//	if (caller == left) {
//		//auto t = make_tuple(customer.c_w_id[tid1], customer.c_d_id[tid1], customer.c_id[tid2]);
//		out << "auto t = make_tuple(";
//		delim = "";
//		auto TIDs_left = *left->getTIDs();
//		for (auto t : left_fields) {
//			auto it = find_if(TIDs_left.begin(), TIDs_left.end(), TabPredicate<TID_Unit>(t.tab));
//			assert(it != TIDs_left.end());
//			out << delim 
//				<< context->getTabName(t.tab) 
//				<< "." << context->getAttr(t.tab,t.attr).name
//				<< "[" << it->name << "]";
//			delim = ",";
//		}
//		out << ");";
//		//auto t_tids = make_tuple(tid1,tid2);
//		out << "auto t_tids = make_tuple(";
//		delim = "";
//		for (auto t : TIDs_left) {
//			out << delim << t.name;
//			delim = ",";
//		}
//		out << ");";
//		//customer_wdc.insert(make_pair(t,t_tids));
//		out << hash_name << ".insert(make_pair(t,t_tids));";
//	} else {
//		//auto t = make_tuple(order.o_w_id[tid], order.o_d_id[tid], order.o_c_id[tid]);
//		out << "auto t = make_tuple(";
//		delim = "";
//		auto TIDs_right = *right->getTIDs();
//		for (auto t : right_fields) {
//			auto it = find_if(TIDs_right.begin(), TIDs_right.end(), TabPredicate<TID_Unit>(t.tab));
//			assert(it != TIDs_right.end());
//			out << delim 
//				<< context->getTabName(t.tab) 
//				<< "." << context->getAttr(t.tab,t.attr).name
//				<< "[" << it->name << "]";
//			delim = ",";
//		}
//		out << ");";
//		//auto it = customer_wdc.find(t);
//		out << "for(auto it = " << hash_name << ".equal_range(t);"
//			<< "it.first != it.second;"
//			<< "++it.first) {";
//		auto TIDs_left = *left->getTIDs();
//		for (size_t i = 0; i < TIDs_left.size(); ++i) {
//			out << "auto " << TIDs_left[i].name 
//				<< "= get<" << i << ">(it.first->second);";
//		}
//		consumer->consume(this);
//		out << "}";
//	}
//}