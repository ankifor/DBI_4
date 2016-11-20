#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <assert.h>
#include "code_generation.h"


using namespace std;

extern string ReplaceString(string subject, const string& search, const string& replace);
extern void ReplaceStringInPlace(string& subject, const string& search, const string& replace);
extern string type(const Schema::Relation::Attribute& attr);

template<class T>
struct TabPredicate {
	size_t tab;
	TabPredicate(size_t tab) : tab(tab) {}
	bool operator()(const T& t) const { return t.tab == tab; }
};

class Name_Generator {
	unordered_map<string,size_t> used_names;
public:
	string request_name(const string& suggested) {
		auto it = used_names.find(suggested);
		if (it == used_names.end()) {
			used_names.insert(make_pair(suggested,0));
			return suggested;
		} else {
			size_t num = ++it->second;
			return suggested + to_string(num);
		}
	}
} name_generator;

void OperatorScan::computeProduced() {
	auto def = context->getTabDef(tab);
	for (size_t i = 0; i < def.attributes.size(); ++i) {
		produced.push_back({tab, i});
	}
}

void OperatorScan::computeTIDs() {
	string tid = name_generator.request_name("tid");
	TIDs.push_back({tid, tab});
}

void OperatorScan::produce() {
	string tid = TIDs[0].name;
	const string tmplt = "for (Tid &tid; = 0;&tid; < &tab;.size(); ++&tid;)";
	string tmp = ReplaceString(tmplt,"&tid;",tid);
	ReplaceStringInPlace(tmp, "&tab;", context->getTabName(tab));
	out << tmp << "{";
	consumer->consume(this);
	out << "}";
}

void OperatorPrint::consume(const Operator* caller) {
	auto TIDs = *input->getTIDs();
	auto produced = *input->getProduced();
	
	out << "cout<<";
	for (size_t i = 0; i < produced.size(); ++i) {
		// get tid.name
		auto it = find_if(TIDs.begin(), TIDs.end(), TabPredicate<TID_Unit>(produced[i].tab));
		assert(it != TIDs.end());
		
		out << (i > 0? "\",\"<<": "")
			<< context->getTabName(produced[i].tab) << "."
			<< context->getAttr(produced[i].tab, produced[i].attr).name
			<< "[" << it->name << "]" << "<<";
	}
	out << "endl;";
}

void OperatorSelect::computeRequired() {
	required = *consumer->getRequired();
	auto it = find(required.begin(), required.end(), fc.field);
	if (it == required.end()) {
		required.push_back(fc.field);
	}
	OperatorUnary::computeRequired();
}

void OperatorSelect::consume(const Operator* caller) {
	auto TIDs = *input->getTIDs();
	
	{
		auto produced = *input->getProduced();
		auto it = find_if(produced.begin(), produced.end(), TabPredicate<Field_Unit>(fc.field.tab));
		assert(it != produced.end());
	}
	
	
	auto it = find_if(TIDs.begin(), TIDs.end(), TabPredicate<TID_Unit>(fc.field.tab));
	assert(it != TIDs.end());
	out << "if (" << fc.predicat << "("
		<< context->getTabName(fc.field.tab) << "."
		<< context->getAttr(fc.field.tab,fc.field.attr).name
		<< "[" << it->name << "]"
		<< ")){";
	consumer->consume(this);
	out << "}";
}

void OperatorProjection::setFields(const vector<Field_Unit>& fields_new) {
	fields = fields_new;
}

void OperatorProjection::check() {
	// check: fields >= required
	for (const Field_Unit& t : *consumer->getRequired()) {
		auto it = find(fields.cbegin(), fields.cend(), t);
		assert(it != fields.end());
	}
	// check: fields <= produced by child
	auto available = *input->getProduced();
	for (const Field_Unit& t : fields) {
		auto it = find(available.cbegin(), available.cend(), t);
		assert(it != available.end());
	}
}

//struct OperatorHashJoin : public OperatorBinary {
//	vector<Field_Unit> left_fields;
//	vector<Field_Unit> right_fields;
//	vector<Field_Unit> required;
//	vector<Field_Unit> produced;
//	vector<TID_Unit> TIDs;
//};

void OperatorHashJoin::produce(){
	computeRequired();
	string delim ;
//	using type_wdc = tuple<Integer,Integer,Integer>;//w_id,d_id,c_id
//	unordered_map<type_wdc,Tid,hash_types::hash<type_wdc>> customer_wdc;
	tuple_typename = name_generator.request_name("type_tuple");
	tuple_tids = name_generator.request_name("type_tids");
	hash_name = name_generator.request_name("hash");
	// tuple_typename definition
	out << "using " << tuple_typename << "=tuple<";
	delim = "";
	for (auto t : left_fields) {
		out << delim << type(context->getAttr(t.tab,t.attr));
		delim = ",";
	}
	out << ">;";
	// tuple_tids definition
	out << "using " << tuple_tids << "=tuple<";
	delim = "";
	for (auto t : *left->getTIDs()) {
		out << delim << "Tid";
		delim = ",";
	}
	out << ">;";
	// unordered_multimap definition	
	out << "unordered_multimap<" 
	<< tuple_typename 
	<< "," << tuple_tids 
	<< "," << "hash_types::hash<" << tuple_typename << ">> " 
	<< hash_name << ";";
	
	left->produce();
	right->produce();
}

void OperatorHashJoin::computeRequired() {
	required = *consumer->getRequired();
	for (const Field_Unit& t : left_fields) {
		auto it = find(required.cbegin(), required.cend(), t);
		if (it == required.end()) required.push_back(t);
	}
	for (const Field_Unit& t : right_fields) {
		auto it = find(required.cbegin(), required.cend(), t);
		if (it == required.end()) required.push_back(t);
	}
	OperatorBinary::computeRequired();
}

void OperatorHashJoin::computeProduced() {
	OperatorBinary::computeProduced();
	produced = *left->getProduced();
	
	for (const Field_Unit& t : *right->getProduced()) {
		auto it = find(produced.cbegin(), produced.cend(), t);
		if (it == produced.end()) produced.push_back(t);
	}
}

void OperatorHashJoin::computeTIDs() {
	OperatorBinary::computeTIDs();
	TIDs = *left->getTIDs();
	for (auto t : *right->getTIDs()) {
		TIDs.push_back(t);
	}
}

void OperatorHashJoin::consume(const Operator* caller){
	string delim = "";
	if (caller == left) {
		//auto t = make_tuple(customer.c_w_id[tid1], customer.c_d_id[tid1], customer.c_id[tid2]);
		out << "auto t = make_tuple(";
		delim = "";
		auto TIDs_left = *left->getTIDs();
		for (auto t : left_fields) {
			auto it = find_if(TIDs_left.begin(), TIDs_left.end(), TabPredicate<TID_Unit>(t.tab));
			assert(it != TIDs_left.end());
			out << delim 
				<< context->getTabName(t.tab) 
				<< "." << context->getAttr(t.tab,t.attr).name
				<< "[" << it->name << "]";
			delim = ",";
		}
		out << ");";
		//auto t_tids = make_tuple(tid1,tid2);
		out << "auto t_tids = make_tuple(";
		delim = "";
		for (auto t : TIDs_left) {
			out << delim << t.name;
			delim = ",";
		}
		out << ");";
		//customer_wdc.insert(make_pair(t,t_tids));
		out << hash_name << ".insert(make_pair(t,t_tids));";
	} else {
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