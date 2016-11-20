#pragma once
#include <sstream>
#include "Schema.hpp"


using namespace std;

struct Field_Unit {
	size_t tab;
	size_t attr;
	bool operator==(const Field_Unit& t) const {return t.tab == tab && t.attr == attr;}
};

struct TID_Unit {
	string name;
	size_t tab;
};

struct Context {
	struct Tab_Instance {
		string name;
		size_t def_pos;
	};
	Schema schema;
	vector<Tab_Instance> tab_instances;
	Context(Schema& schema) {this->schema = schema;}
	const string& getTabName(size_t tab) const {return tab_instances[tab].name;}
	const Schema::Relation& getTabDef(size_t tab) const {return schema.relations[tab_instances[tab].def_pos];}
	const Schema::Relation::Attribute& getAttr(size_t tab, size_t attr) const {return getTabDef(tab).attributes[attr];}
};

struct Operator {
	const Context* context = nullptr;
	Operator* consumer;
	stringstream& out;
	
	Operator(const Context* context, stringstream& out): context(context), out(out) {}
	void setConsumer(Operator* consumer) {this->consumer = consumer;}
	
	virtual const vector<Field_Unit>* getProduced() const = 0;
	virtual const vector<Field_Unit>* getRequired() const = 0;
	virtual const vector<TID_Unit>* getTIDs() const =0 ;
	virtual void computeTIDs() = 0;
	virtual void computeProduced() = 0;
	virtual void computeRequired() = 0;
	
	virtual void consume(const Operator* caller) = 0;
	virtual void produce() = 0;
};

struct OperatorUnary : public Operator {
	Operator* input = nullptr;	
	OperatorUnary(const Context* context, stringstream& out) : Operator(context,out) {}
	void setInput(Operator* input) {this->input = input;input->setConsumer(this);}
	void computeTIDs() {input->computeTIDs();}
	void computeProduced() {input->computeProduced();}
	void computeRequired() {input->computeRequired();}
};

struct OperatorBinary : public Operator {
	Operator* left = nullptr;
	Operator* right = nullptr;
	OperatorBinary(const Context* context, stringstream& out) : Operator(context,out) {}
	void setInput(Operator* left, Operator* right) {
		this->left = left; left->setConsumer(this);
		this->right=right; right->setConsumer(this);
	}
	void computeTIDs() {left->computeTIDs();right->computeTIDs();}
	void computeProduced() {left->computeProduced();right->computeProduced();}
	void computeRequired() {left->computeRequired();right->computeRequired();}
};

struct OperatorScan : public Operator {
	vector<Field_Unit> produced;
	size_t tab;
	vector<TID_Unit> TIDs;
	//-------------
	OperatorScan(const Context* context, stringstream& out) : Operator(context,out) {}
	void assignTable(size_t tab) {this->tab = tab;}
	
	const vector<Field_Unit>* getRequired() const {return consumer->getRequired();}
	const vector<Field_Unit>* getProduced() const {return &produced;}
	const vector<TID_Unit>* getTIDs() const {return &TIDs;}
	void computeTIDs();
	void computeProduced();
	void computeRequired() {}
	
	void consume(const Operator* caller) {}
	void produce();
};

struct OperatorPrint : public OperatorUnary {
	vector<Field_Unit> required;//nothing
	//-------------
	OperatorPrint(const Context* context, stringstream& out) : OperatorUnary(context,out) {}

	const vector<Field_Unit>* getRequired() const {return &required;}
	const vector<Field_Unit>* getProduced() const {return input->getProduced();}
	const vector<TID_Unit>* getTIDs() const {return input->getTIDs();}
	
	void consume(const Operator* caller);
	void produce() {input->produce();}
};

struct OperatorSelect : public OperatorUnary {
	struct Field_Comparison {
		Field_Unit field;
		string predicat;
	};
	Field_Comparison fc;
	vector<Field_Unit> required;
	//-------------
	OperatorSelect(const Context* context, stringstream& out) : OperatorUnary(context,out) {}
	void setFieldComparison(const Field_Unit& field, const string& predicat) {fc.field = field; fc.predicat = predicat;}

	const vector<Field_Unit>* getRequired() const {return &required;}
	const vector<Field_Unit>* getProduced() const {return input->getProduced();}
	const vector<TID_Unit>* getTIDs() const {return input->getTIDs();}
	void computeRequired();
	
	void consume(const Operator* caller);
	void produce() {input->produce();}
};

struct OperatorProjection : public OperatorUnary {
	vector<Field_Unit> fields;
	//-------------
	OperatorProjection(const Context* context, stringstream& out) : OperatorUnary(context,out) {}
	void setFields(const vector<Field_Unit>& fields);
	
	const vector<Field_Unit>* getRequired() const {return &fields;}
	const vector<Field_Unit>* getProduced() const {return &fields;}
	const vector<TID_Unit>* getTIDs() const {return input->getTIDs();}
	
	void consume(const Operator* caller) {consumer->consume(this);}
	void produce() {check();input->produce();}
	void check();
};

struct OperatorHashJoin : public OperatorBinary {
	vector<Field_Unit> left_fields;
	vector<Field_Unit> right_fields;
	vector<Field_Unit> required;
	vector<Field_Unit> produced;
	vector<TID_Unit> TIDs;
	string tuple_typename;
	string tuple_tids;
	string hash_name;
	//-------------
	OperatorHashJoin(const Context* context, stringstream& out) : OperatorBinary(context,out) {}
	void setFields(const vector<Field_Unit>& left_fields, const vector<Field_Unit>& right_fields) {
		this->left_fields = left_fields;
		this->right_fields = right_fields;
	}
	void computeRequired();
	void computeProduced();
	void computeTIDs();
	
	const vector<Field_Unit>* getRequired() const {return &required;}
	const vector<Field_Unit>* getProduced() const {return &produced;}
	const vector<TID_Unit>* getTIDs() const {return &TIDs;}
	
	void consume(const Operator* caller);
	void produce();
};