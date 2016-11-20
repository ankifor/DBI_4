#pragma once
#include <sstream>
#include <assert.h>
#include <unordered_map>
#include "Schema.hpp"


using namespace std;

struct Field_Unit {
	string tab_name;
	string field_name;
	string token;
	string type_name;
	bool operator==(const Field_Unit& t) const {
		return t.field_name == field_name && t.tab_name == tab_name;
	}
};

struct Field_Comparison {
	vector<Field_Unit> fields;
	string predicat;//predicat(fields(0),fields(1),...,fields(n))
};

struct Context {
	unordered_map<string, size_t> tab_instances;//should be set from outside
	Schema schema;
	Context(Schema& schema) {this->schema = schema;}
	const Schema::Relation* getTabDef(string tabname) const;
	const Schema::Relation::Attribute* getAttr(const Field_Unit& fu) const;
};
//==============================================================================
// Operator
//==============================================================================
struct Operator {
public:	
	Operator(const Context* context, stringstream& out): context(context), out(out) {}
	
	
	const vector<Field_Unit>& getRequired() {
		if (!computeRequiredFinished) {
			computeRequired();
			computeRequiredFinished = true;
		}
		return required;
	}
	
	const vector<Field_Unit>& getProduced() {
		if (!computeProducedFinished) {
			computeProduced();
			computeProducedFinished = true;
		}
		return produced;
	}
	
	virtual void consume(const Operator* caller) = 0;
	virtual void produce() = 0;
	
	void setConsumer(Operator* consumer) {this->consumer = consumer;}
protected: 
	
	const Context* context = nullptr;
	Operator* consumer;
	stringstream& out;
	
	vector<Field_Unit> required;
	bool computeRequiredFinished = false;
	virtual void computeRequired() = 0;
	
	vector<Field_Unit> produced;
	bool computeProducedFinished = false;
	virtual void computeProduced() = 0;
	
	
};
//==============================================================================
// Unary Operators
//==============================================================================
struct OperatorUnary : public Operator {
	Operator* input = nullptr;	
	OperatorUnary(const Context* context, stringstream& out) : Operator(context,out) {}
	void setInput(Operator* input) {this->input = input;input->setConsumer(this);}
};

struct OperatorScan : public OperatorUnary {
	OperatorScan(const Context* context, stringstream& out, string tabname);
	void consume(const Operator* caller) {assert(false);/*should never be called*/}
	void produce();
	
protected:
	string tabname;
	string tid_name;
	
	void computeRequired() {required = consumer->getRequired();}
	void computeProduced();
};

struct OperatorPrint : public OperatorUnary {
	OperatorPrint(const Context* context, stringstream& out) : OperatorUnary(context,out) {}

	void consume(const Operator* caller);
	void produce() {input->produce();}
	
protected:
	void computeRequired() {/*nothing to do here*/}
	void computeProduced() {produced = input->getProduced();}
};


struct OperatorProjection : public OperatorUnary {
	vector<Field_Unit> fields;
	//-------------
	OperatorProjection(const Context* context, stringstream& out, const vector<Field_Unit>& fields) 
		: OperatorUnary(context,out)
		, fields(fields) {}

	void consume(const Operator* caller) {consumer->consume(this);}
	void produce() {input->produce();}
	
protected:
	void computeRequired() {required = fields;}
	void computeProduced();
};

struct OperatorSelect : public OperatorUnary {
	OperatorSelect(const Context* context, stringstream& out, const Field_Comparison& condition) 
		: OperatorUnary(context,out)
		, condition(condition) {}

	void consume(const Operator* caller);
	void produce() {input->produce();}
	
protected:
	Field_Comparison condition;
	
	void computeRequired();
	void computeProduced() {produced = input->getProduced();}
};


//==============================================================================
// Binary Operators
//==============================================================================
struct OperatorBinary : public Operator {
	Operator* left = nullptr;
	Operator* right = nullptr;
	OperatorBinary(const Context* context, stringstream& out) : Operator(context,out) {}
	virtual void setInput(Operator* left, Operator* right) {
		this->left = left; left->setConsumer(this);
		this->right=right; right->setConsumer(this);
	}
};



//
//struct OperatorHashJoin : public OperatorBinary {
//	vector<Field_Unit> left_fields;
//	vector<Field_Unit> right_fields;
//	vector<Field_Unit> required;
//	vector<Field_Unit> produced;
//	vector<TID_Unit> TIDs;
//	string tuple_typename;
//	string tuple_tids;
//	string hash_name;
//	//-------------
//	OperatorHashJoin(const Context* context, stringstream& out) : OperatorBinary(context,out) {}
//	void setFields(const vector<Field_Unit>& left_fields, const vector<Field_Unit>& right_fields) {
//		this->left_fields = left_fields;
//		this->right_fields = right_fields;
//	}
//	void computeRequired();
//	void computeProduced();
//	void computeTIDs();
//	
//	const vector<Field_Unit>* getRequired() const {return &required;}
//	const vector<Field_Unit>* getProduced() const {return &produced;}
//	const vector<TID_Unit>* getTIDs() const {return &TIDs;}
//	
//	void consume(const Operator* caller);
//	void produce();
//};