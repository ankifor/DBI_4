#include <iostream>
#include <memory>
#include <cstdio>
#include <sstream>
#include "Schema.hpp"
#include "Parser.hpp"
#include "code_generation.h"

using namespace std;
//extern Table_warehouse warehouse;
//extern Table_district district;
//extern Table_customer customer;
//extern Table_history history;
//extern Table_neworder neworder;
//extern Table_order order;
//extern Table_orderline orderline;
//extern Table_item item;
//extern Table_stock stock;
string create_query(Schema* schema) {
	Context context(*schema);
	context.tab_instances = {
		 {"warehouse", 0}
		,{"district",1}
		,{"customer",2}
		,{"history",3}
		,{"neworder",4}
		,{"order",5}
		,{"orderline",6}
		,{"item",7}
		,{"stock",8}
	};
	stringstream out;
	
	out << "#include \"Types.hpp\""   << endl;
	out << "#include \"schema_1.hpp\""   << endl;
	out << "#include <iostream>"      << endl;
	out << "#include <unordered_map>" << endl;
	out << "using namespace std;"     << endl;
	out << "bool pred(const Varchar<16>& s) {return s.len > 0 && s.value[0]=='B';}";
	out << "void run_query() {" << endl;
	
	OperatorScan scanCust(&context, out);
	OperatorScan scanOrder(&context, out);
	OperatorScan scanOl(&context, out);
	OperatorSelect selectCust(&context, out);
	OperatorPrint printData(&context, out);
	OperatorProjection projectFields(&context, out);
	OperatorHashJoin hjCustOrder(&context, out);
	OperatorHashJoin hjCustOrderOl(&context, out);
	
	printData.setInput(&projectFields);
	projectFields.setInput(&hjCustOrderOl);
	hjCustOrderOl.setInput(&hjCustOrder,&scanOl);
	hjCustOrder.setInput(&selectCust,&scanOrder);
	selectCust.setInput(&scanCust);
	
	scanCust.assignTable(2);
	scanOrder.assignTable(5);
	scanOl.assignTable(6);
	selectCust.setFieldComparison({2,5},"pred");
	hjCustOrderOl.setFields(
		 {{5,2},{5,1},{5,0}}
		,{{6,2},{6,1},{6,0}});
	hjCustOrder.setFields(
		 {{2,2},{2,1},{2,0}}
		,{{5,2},{5,1},{5,3}});
	//c_first, c_last, o_all_local, ol_amount 
	projectFields.setFields({{2,3},{2,5},{5,7},{6,8}});
	
	printData.computeProduced();
	printData.computeRequired();
	printData.computeTIDs();
	
	projectFields.check();
	
	printData.produce();
	
	out << "}" << endl;
	return out.str();
}



int main(int argc, char* argv[]) {
	if (argc != 4) {
		cerr << "usage: " << argv[0] 
		     << " <schema file>" 
		     << " <output dir>"
		     << " <filename without extension>"
		     << endl
		     << argc << endl;
		return -1;
	}

	Parser p(argv[1]);
	try {
		unique_ptr<Schema> schema = p.parse();
		
		ofstream out;

		string path = string(argv[2],strlen(argv[2])) + "/";
		string name = string(argv[3],strlen(argv[3]));
		
		
		out.open(path  + name + ".cpp");
		out << create_query(schema.get());
		out.close();		
		
	} catch (ParserError& e) {
		cerr << e.what() << endl;
	}
	return 0;
}
