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
	stringstream out;
	
	out << "#include \"Types.hpp\""   << endl;
	out << "#include \"schema_1.hpp\""   << endl;
	out << "#include <iostream>"      << endl;
	out << "#include <unordered_map>" << endl;
	out << "using namespace std;"     << endl;
	out << "bool pred(const Varchar<16>& s) {return s.len > 0 && s.value[0]=='B';}";
	//out << "bool pred1(const Integer& a0, const Varchar<10>& a1) {return a0>=3 || (a1.len>0 && a1.value[0]=='2');}";
	out << "void run_query() {" << endl;
	
	OperatorScan scanCust(&context, out, "customer");
//and c_id = 322
//and c_w_id = 1
//and c_d_id = 1
	OperatorSelect select(&context, out, {"pred",{
		{"c_last"}
	}});
	
	OperatorScan scanOrder(&context, out, "order");
	OperatorHashJoin join1(&context, out, 
		 {{"c_w_id"},{"c_d_id"},{"c_id"  }}
		,{{"o_w_id"},{"o_d_id"},{"o_c_id"}}
	);
	
	OperatorScan scanOrdeline(&context, out, "orderline");
	OperatorHashJoin join2(&context, out, 
		 {{"o_w_id" },{"o_d_id" },{"o_id"   }}
		,{{"ol_w_id"},{"ol_d_id"},{"ol_o_id"}}
	);
	
	OperatorProjection proj(&context, out, {
		 {"c_first"}
		,{"c_last"}
		,{"o_all_local"}
		,{"ol_amount"}
	});

	OperatorPrint print(&context, out);

	print.setInput(&proj);
	proj.setInput(&join2);
	join2.setInput(&join1, &scanOrdeline);
	join1.setInput(&select, &scanOrder);
	select.setInput(&scanCust);

	print.produce();

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
