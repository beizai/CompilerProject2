#include<iostream>
#include<cstring>
#include<cstdio>
#include<fstream>
#include<map>
#include<vector>
#include "IR.h"
#include "IRMutator.h"
#include "IRVisitor.h"
#include "IRPrinter.h"
#include "type.h"
using namespace std;
using namespace Boost::Internal;
class MyVar//“变量”，包括数组和数 
{
	public :
		string name;
		bool type;// 0表示数组，1表示数
		int dim;// 数组维数，如果是数则为1 
		int dim_list[100];//数组每一维大小，如果是数则无意义
		bool is_in;//是否是输入变量 
		bool is_out;//是否是输出变量 
	MyVar()
	{
		 name = "";
		 type = false;
		 dim = 0;
		 memset(dim_list, 0, sizeof(dim_list));
		 is_in = is_out = false;
	}
	MyVar(string _name)
	{
		 name = _name;
		 type = false;
		 dim = 0;
		 memset(dim_list, 0, sizeof(dim_list));	
		 is_in = is_out = false;
	}
};
class IdExpr
{
	public :
		string name;//IdExpr的粗表达式，形如" (j + k) / 2 " 
		int upbound;//IdExpr 上届 
		IdExpr()
		{
			name = "";
			upbound = 10000;
		}
		IdExpr(string _name)
		{
			name = _name;
			upbound = 10000;
		}
};
class Id
{
	public :
		string name;//Id名字 
		int upbound;//Id上界(会更新) 
		bool is_LHS;//是否在LHS中出现过 
		Id()
		{
			name = "";
			upbound = 10000;
			is_LHS = false;	
		}
		Id(string _name)
		{
			name = _name;
			upbound = 10000;
			is_LHS = false;
		} 
};
class Factor
{
	public:
		vector<string>undef_id_list;//项中未出现在LHS中的id 
		vector<IdExpr>id_expr_list;//项中所有的IdExpr 
		string final_code;//项的粗final_code 
		Factor()
		{
			undef_id_list.clear();
			id_expr_list.clear();
			final_code = "";
		}
};
class MyStmt
{
	public :
		vector<IdExpr>LHS_idexpr_list; //LHS中所有IdExpr
		vector<string>LHS_id_list; //LHS中所有的id(id.name)， 
		MyVar LHS_var;//LHS中的var 
		vector<int>factor_ops;//顺序保存“项（factor）”之间是什么运算符，1表示+，2表示- 
		vector<Factor>factor_list;//语句的项的列表
		map<string,Id>id_map;//stmt中所有的id 注意区别  
		MyStmt()
		{
			LHS_idexpr_list.clear();
			LHS_id_list.clear();
			factor_ops.clear();
			factor_list.clear();
			id_map.clear();
		} 
};  
const string op_chars = " +-*/%()";
MyStmt tmp_stmt;

//以下是所有可能用到的全局变量 
string global_name;//函数名字
string global_type;//所有变量类型 可能是"int"或"float"
Type index_type = Type::int_scalar(32);
Type data_type;
string kernel;//kernel字符串 
map<string, MyVar>var_map;//kernel中所有var 
map<string,Id>id_map;//kernel中所有的id 注意区别 
vector<MyStmt>stmt_list;//所有语句列表 
ifstream input_file;// 输入文件流 
ofstream output_file;// 输出文件流

vector<string> in_name;	//输入参数名字
vector<string> out_name;	//输出参数名字
vector<string> tmp_list;	//储存相应的LHS name
MyStmt this_stmt;	//Current use stmt
//以上是所有可能用到的全局变量 

void parse_json()
{
	var_map.clear();
	in_name.clear();
	out_name.clear();
	string line;
	while(getline(input_file,line))
	{
		if(line.find("\"name\"") != string::npos)
		{
			string::size_type left_idx = line.find(":"), right_idx;
			left_idx = line.find("\"", left_idx);
			right_idx = line.find("\"", left_idx+1);
			global_name = line.substr(left_idx+1, right_idx-left_idx-1);
		}
		if(line.find("\"ins\"") != string::npos)
		{
			string::size_type left_idx = line.find("["), right_idx;
			left_idx = line.find("\"", left_idx);
			while(left_idx != string::npos)
			{
				right_idx = line.find("\"", left_idx+1);
				string tmp_var_name = line.substr(left_idx+1, right_idx-left_idx-1);
				if(var_map.find(tmp_var_name) == var_map.end())
					var_map[tmp_var_name]  = MyVar(tmp_var_name);
				in_name.push_back(tmp_var_name);
				var_map[tmp_var_name].is_in = true;
				left_idx = line.find("\"", right_idx+1);
			}
		}
		if(line.find("\"outs\"") != string::npos)
		{
			string::size_type left_idx = line.find("["), right_idx;
			left_idx = line.find("\"", left_idx);
			while(left_idx != string::npos)
			{
				right_idx = line.find("\"", left_idx+1);
				string tmp_var_name = line.substr(left_idx+1, right_idx-left_idx-1);
				if(var_map.find(tmp_var_name) == var_map.end())var_map[tmp_var_name] = MyVar(tmp_var_name);
				var_map[tmp_var_name].is_out = true;
				//cout<<tmp_var_name<<endl;
				out_name.push_back(tmp_var_name);
				left_idx = line.find("\"", right_idx+1);
			}
		}
		if(line.find("\"data_type\"") != string::npos)
		{
			string::size_type left_idx = line.find(":"), right_idx;
			left_idx = line.find("\"", left_idx);
			right_idx = line.find("\"", left_idx+1);
			global_type = line.substr(left_idx+1, right_idx-left_idx-1);
			//cout<<global_type<<" "<<left_idx<<" "<<right_idx<<endl;
		}
		if(line.find("\"kernel\"") != string::npos)
		{
			string::size_type left_idx = line.find(":"), right_idx;
			left_idx = line.find("\"", left_idx);
			right_idx = line.find("\"", left_idx+1);
			kernel = line.substr(left_idx+1, right_idx-left_idx-1);
			//cout<<kernel<<" "<<left_idx<<" "<<right_idx<<endl;
		}
	}
}
void build_dim_list(string var_name, string dim_list)
{
	MyVar var= var_map[var_name];
	
	string::size_type left_idx=0, right_idx=dim_list.find(",");
	while(right_idx != string::npos)
	{
		string dim = dim_list.substr(left_idx, right_idx-left_idx);
		var.dim++; var.dim_list[var.dim] = atoi(dim.c_str());
		left_idx = dim_list.find_first_not_of(" ", right_idx+1);
		right_idx = dim_list.find(",", left_idx);
	}
	string dim = dim_list.substr(left_idx);
	var.dim++; var.dim_list[var.dim] = atoi(dim.c_str());
	if(var.dim == 1)var.type = 1;
	else var.type = 0;
	
	var_map[var_name] = var;
}
void build_map()
{
	for(auto it : var_map)
	{
		string var_name = it.first;
		MyVar var = it.second;
		string::size_type left_idx = kernel.find(var_name),right_idx;
		left_idx = kernel.find("<", left_idx);
		right_idx = kernel.find(">", left_idx);
		string dim_list = kernel.substr(left_idx+1, right_idx-left_idx-1);
		//cout<<dim_list<<endl;
		build_dim_list( var_name, dim_list);
		
		/*cout<<var_name<<": "<<var_map[var_name].is_in<<" "<<var_map[var_name].is_out<<" ";
		for(int i=1;i<=var_map[var_name].dim;i++)
		cout<<var_map[var_name].dim_list[i]<<" ";
		cout<<endl;*/
	}
}
bool is_int(string id_string)
{
	int len=id_string.length();
	for(int i=0;i<len;i++)if(id_string[i]<'0' || id_string[i]>'9')return false;
	return true;
}
bool check_in_vector(vector<string>id_list, string s)
{
	for(auto i:id_list)if(i == s)return true;
	return false;
}
bool is_in_LHS_id_list(string id) 
{
	for(auto LHS_id:tmp_stmt.LHS_id_list)
	{
		if(id == LHS_id)return true;
	}
	return false;
}
void parse_id_expr(bool is_LHS, string id_expr, int upbound, vector<string>&id_vector)
{
	//cout<<id_expr<<endl;
	string::size_type left_idx = id_expr.find_first_not_of(op_chars);
	string::size_type right_idx = id_expr.find_first_of(op_chars, left_idx);
	while(left_idx != string::npos)
	{
		string id_string = id_expr.substr(left_idx, right_idx-left_idx);
		//cout<<"id "<<id_string<<" "<<upbound<<endl;
		if(is_int(id_string) == false)
		{
			if(check_in_vector(tmp_stmt.LHS_id_list, id_string) == false)
			if(is_LHS == false)
			if(check_in_vector(id_vector, id_string) == false)id_vector.push_back(id_string);
			if(tmp_stmt.id_map.find(id_string) == tmp_stmt.id_map.end())
			{
				Id id(id_string);
				id.upbound = upbound;
				id.is_LHS = is_LHS;
				tmp_stmt.id_map[id_string] = id;
			}
			else
			{
				Id id = tmp_stmt.id_map[id_string];
				if(is_in_LHS_id_list(id_string) == false)id.upbound = min(id.upbound, upbound);
				tmp_stmt.id_map[id_string] = id;
			}
			if(is_LHS)id_vector.push_back(id_string);
			
			if(id_map.find(id_string) == id_map.end())
			{
				Id id(id_string);
				id.upbound = upbound;
				id.is_LHS = is_LHS;
				id_map[id_string] = id;
			}
			else
			{
				Id id = id_map[id_string];
				if(is_in_LHS_id_list(id_string) == false)id.upbound = min(id.upbound, upbound);
				id_map[id_string] = id;
			}
		}
		left_idx = id_expr.find_first_not_of(op_chars, right_idx);
		right_idx = id_expr.find_first_of(op_chars, left_idx);
	}
}
void parse_id_list_into_id(bool is_LHS, string id_list, MyVar var, vector<string> &id_vector)
{
	//cout<<"id_list "<<id_list<<endl;
	string::size_type left_idx = 0, right_idx = id_list.find(",");
	int dim_idx = 1;
	while(right_idx != string::npos)
	{
		
		string id_expr = id_list.substr(left_idx, right_idx-left_idx);
		parse_id_expr(is_LHS, id_expr, var.dim_list[dim_idx], id_vector);
		left_idx = right_idx+1, right_idx = id_list.find(",", left_idx);
		dim_idx++;
	}
	string id_expr = id_list.substr(left_idx);
	parse_id_expr(is_LHS, id_expr, var.dim_list[dim_idx], id_vector);
}
void parse_id_list_into_idexpr(string id_list, MyVar var, vector<IdExpr> &idexpr_vector)
{
	string::size_type left_idx = 0, right_idx = id_list.find(",");
	int dim_idx = 1;
	while(right_idx != string::npos)
	{
		string id_expr_string = id_list.substr(left_idx, right_idx-left_idx);
		IdExpr idexpr(id_expr_string);
		idexpr.upbound = var.dim_list[dim_idx];
		idexpr_vector.push_back(idexpr);
		left_idx = right_idx+1, right_idx = id_list.find(",", left_idx);
		dim_idx++;
	}
	string id_expr_string = id_list.substr(left_idx);
	IdExpr idexpr(id_expr_string);
	idexpr.upbound = var.dim_list[dim_idx];
	idexpr_vector.push_back(idexpr);
}
void parse_factor(string factor_string, Factor &factor)
{
	//cout<<"factor "<<factor_string<<endl;

	factor.undef_id_list.clear();
	factor.id_expr_list.clear();
	string::size_type left_idx = factor_string.find("["),right_idx = factor_string.find("]");
	while(left_idx != string::npos)
	{
		string id_list = factor_string.substr(left_idx+1, right_idx-left_idx-1);
		string::size_type right_idx_2 = factor_string.find_last_of("<", left_idx);
		string::size_type left_idx_2 = factor_string.find_last_of(op_chars, right_idx_2-1);
		string var_string = factor_string.substr(left_idx_2+1, right_idx_2-left_idx_2-1);
		MyVar var = var_map[var_string];
		parse_id_list_into_id(false, id_list, var, factor.undef_id_list);
		parse_id_list_into_idexpr(id_list, var, factor.id_expr_list);
		left_idx = factor_string.find("[", right_idx);
		right_idx = factor_string.find("]", left_idx);
	}
	int len=factor_string.length();
	bool in_brac = false;
	for(int i=0;i<len;i++)
	{
		if(factor_string[i] == '<')in_brac = true;
		if(factor_string[i] == '>')in_brac =false;
		if(in_brac == true || factor_string[i] == '>')continue;
		if(factor_string[i] == ',')factor.final_code += "][";
		else factor.final_code += factor_string[i];
	}
	/*cout<<"    undef_id_list ";
	for(auto i:factor.undef_id_list)cout<<i<<" ";
	cout<<endl;
	cout<<"    id_expr_list ";
	for(auto i:factor.id_expr_list)cout<<i.name<<" "<<i.upbound<<" ";
	cout<<endl;
	cout<<"    final_code "<<factor.final_code<<endl;*/
} 
void parse_RHS(string RHS)
{
	tmp_stmt.factor_ops.clear(); 
	tmp_stmt.factor_list.clear();
	int in_bracket = 0, in_bracket_2 = 0, len =RHS.length(), last_idx = 0;
	for(int i=0;i<len;i++)
	{
		if(RHS[i] == '(')in_bracket++;
		else if(RHS[i] == ')')in_bracket--;
		else if(RHS[i] == '[')in_bracket_2++;
		else if(RHS[i] == ']')in_bracket_2--;
		else if((RHS[i] == '+' || RHS[i] == '-') && in_bracket == 0 && in_bracket_2 == 0)
		{
			if(RHS[i] == '+')tmp_stmt.factor_ops.push_back(1);
			else tmp_stmt.factor_ops.push_back(2);
			string factor_string = RHS.substr(last_idx, i-last_idx);
			Factor factor;
			parse_factor(factor_string, factor);
			tmp_stmt.factor_list.push_back(factor);
			last_idx = i+1;
		}
	}
	string factor_string = RHS.substr(last_idx);
	Factor factor;
	parse_factor(factor_string, factor);
	tmp_stmt.factor_list.push_back(factor);
}
void parse_LHS(string LHS)
{	
	//cout<<"LHS "<<LHS<<endl;
	tmp_stmt.LHS_idexpr_list.clear();
	tmp_stmt.LHS_id_list.clear();
	string::size_type left_idx = 0, right_idx = LHS.find("<");
	string LHS_var_string = LHS.substr(left_idx, right_idx-left_idx);
	tmp_stmt.LHS_var = var_map[LHS_var_string];
	//cout<<"    LHS_var "<<LHS_var.name<<" "<<LHS_var.dim<<endl;
	left_idx = LHS.find("[");right_idx = LHS.find("]");
	if(left_idx != string::npos)
	{
		string id_list = LHS.substr(left_idx+1, right_idx - left_idx -1);
		//cout<<id_list<<endl;
		parse_id_list_into_id(true, id_list, tmp_stmt.LHS_var, tmp_stmt.LHS_id_list);
		parse_id_list_into_idexpr(id_list, tmp_stmt.LHS_var, tmp_stmt.LHS_idexpr_list);
	}
	/*cout<<"    LHS_id_list ";
	for(auto i : LHS_id_list)cout<<i<<" "; 
	cout<<endl;
	cout<<"    LHS_idexpr_list ";
	for(auto i : LHS_idexpr_list)cout<<i.name<<" "<<i.upbound<<" "; 
	cout<<endl;*/
}
void parse_stmt(string stmt)
{
	tmp_stmt = MyStmt();
	//cout<<endl<<"stmt : "<<stmt<<endl;
	string::size_type left_idx = 0, right_idx = stmt.find("=");
	string LHS = stmt.substr(left_idx, right_idx-left_idx);
	parse_LHS(LHS);

	left_idx = stmt.find("=");
	string RHS = stmt.substr(left_idx+1);
	parse_RHS(RHS);
	stmt_list.push_back(tmp_stmt);
	
}
void parse_into_stmt()
{
	cout<<"kernel : "<<kernel<<endl;
	id_map.clear();
	stmt_list.clear();
	string::size_type left_idx = 0,right_idx = kernel.find(";");
	while(right_idx != string::npos)
	{
		string stmt = kernel.substr(left_idx, right_idx - left_idx);
		parse_stmt(stmt);
		left_idx = kernel.find_first_not_of(" ", right_idx + 1);
		right_idx = kernel.find(";",left_idx);
	}
	 
}
void debug()
{
	int idx=0;
	for(auto k: stmt_list)
	{
	cout<<"stmt "<<++idx<<endl;
	cout<<"    LHS : ";
	cout<<"    LHS_var : "<<k.LHS_var.name<<" dim : "<<k.LHS_var.dim<<endl;
	cout<<"        LHS_id_list : ";
	for(auto i: k.LHS_id_list)cout<<i<<" ";
	cout<<endl;
		cout<<"        LHS_idexpr_list : ";
	for(auto i: k.LHS_idexpr_list)cout<<i.name<<" upbound : "<<i.upbound<<" ";
	cout<<endl;
	
	for(int i=0 ;i!=k.factor_list.size();i++)
	{
		cout<<"    factor "<<i<<endl;
		cout<<"        undef_id_list : ";
		for(auto j: k.factor_list[i].undef_id_list)cout<<j<<" ";
		cout<<endl;
		cout<<"        id_expr_list : ";
		for(auto j: k.factor_list[i].id_expr_list)cout<<j.name<<" upbound "<<j.upbound<<" ";
		cout<<endl;
		cout<<"        final_code : "<<k.factor_list[i].final_code<<endl;
	}
	cout<<"    factor_ops : ";
	for(auto i: k.factor_ops)cout<<i<<" ";
	cout<<endl;
	cout<<"    stmt_id_map ";
	for(auto i:k.id_map)cout<<i.second.name<<" upbound : "<<i.second.upbound<<" ";
	cout<<endl; 
	}
	cout<<"var_map ";
	for(auto i:var_map)cout<<i.second.name<<" dim : "<<i.second.dim<<" ";
	cout<<endl;
	cout<<"id_map ";
	for(auto i:id_map)cout<<i.second.name<<" upbound : "<<i.second.upbound<<" ";
	cout<<endl;
}

Expr make_add_expr(Expr A, Expr B){ return Binary::make(data_type, BinaryOpType::Add, A, B);}
Expr make_sub_expr(Expr A, Expr B){ return Binary::make(data_type, BinaryOpType::Sub, A, B);}
Expr make_mul_expr(Expr A, Expr B){ return Binary::make(data_type, BinaryOpType::Mul, A, B);}
Expr make_div_expr(Expr A, Expr B){ return Binary::make(data_type, BinaryOpType::Div, A, B);}
Expr make_mod_expr(Expr A, Expr B){ return Binary::make(data_type, BinaryOpType::Mod, A, B);}
Expr make_idx(Id id)
{
	Expr dom = Dom::make(index_type, 0, id.upbound);
	Expr idx = Index::make(index_type, id.name, dom, IndexType::Spatial);				
	return idx;
}

Expr parse_idx(string st, int L, int R)
{
	//cout<<"in parse_idx st="<<st<<" L="<<L<<" R="<<R<<endl;
	int i, inbracket=0, low_level_pos=-1, high_level_pos=-1;
	for(i=L+1; i<=R-1; i++)
	{
		if(st[i]=='(')inbracket++;
		if(st[i]==')')inbracket--;
		if(inbracket<0)break;
	}
	if(inbracket==0&&st[L]=='('&&st[R]==')')return parse_idx(st, L+1, R-1);

	for(inbracket=0, i=L; i<=R; i++)
	{
		if(st[i]=='(')inbracket++;
		if(st[i]==')')inbracket--;
		if(inbracket==0 && (st[i]=='+' || st[i]=='-'))low_level_pos=i;
		if(inbracket==0 && (st[i]=='*' || st[i]=='/' || st[i]=='%'))high_level_pos=i;
	}
	if(low_level_pos!=-1)
	{
		Expr A = parse_idx(st, L, low_level_pos-1);
		Expr B = parse_idx(st, low_level_pos+1, R);
		if(st[low_level_pos]=='+')return make_add_expr(A,B);
		else return make_sub_expr(A,B);
	}
	else if(high_level_pos!=-1)
	{
		Expr A = parse_idx(st, L, high_level_pos-1);
		Expr B = parse_idx(st, high_level_pos+1, R);
		if(st[high_level_pos]=='*')return make_mul_expr(A,B);
		else if(st[high_level_pos]=='/')return make_div_expr(A,B);
		else return make_mod_expr(A,B);
	}
	else if('0'<=st[L]&&st[L]<='9')
	{
		return Expr(atoi(st.substr(L,R-L+1).c_str()));
	}
	else
	{
		string name = st.substr(L,R-L+1);
		return make_idx(this_stmt.id_map[name]);
	}
}

vector<Expr> parse_idx_list(string st, int L, int R)
{
	int i,pre=L;
	vector<Expr>idx_list;
	for(i=L;i<=R+1;i++)
	{
		if(st[i]==']')
		{
			idx_list.push_back(parse_idx(st,pre+1,i-1));
			pre=i+1;
		}
	}
	return idx_list;
}

vector<size_t> get_var_shape(string var_name)
{
	MyVar var = var_map[var_name];
	vector<size_t>shape_list;
	for(int i=1;i<=var.dim;i++)
		shape_list.push_back(var.dim_list[i]);
	return shape_list;
}

Expr parse_final_code(string st, int L, int R)
{
	//cout<<"in parse_final_code st="<<st<<" L="<<L<<" R="<<R<<endl;
	int i, inbracket=0, low_level_pos=-1, high_level_pos=-1;
	for(i=L+1; i<=R-1; i++)
	{
		if(st[i]=='(')inbracket++;
		if(st[i]==')')inbracket--;
		if(inbracket<0)break;
	}
	if(inbracket==0&&st[L]=='('&&st[R]==')')return parse_final_code(st, L+1, R-1);

	for(inbracket=0, i=L; i<=R; i++)
	{
		if(st[i]=='('||st[i]=='[')inbracket++;
		if(st[i]==')'||st[i]==']')inbracket--;
		if(inbracket==0 && (st[i]=='+' || st[i]=='-'))low_level_pos=i;
		if(inbracket==0 && (st[i]=='*' || st[i]=='/' || st[i]=='%'))high_level_pos=i;
	}
	//cout<<"low_pos="<<low_level_pos<<" high_pos="<<high_level_pos<<endl;
	if(low_level_pos!=-1)
	{
		Expr A = parse_final_code(st, L, low_level_pos-1);
		Expr B = parse_final_code(st, low_level_pos+1, R);
		if(st[low_level_pos]=='+')return make_add_expr(A,B);
		else return make_sub_expr(A,B);
	}
	else if(high_level_pos!=-1)
	{
		Expr A = parse_final_code(st, L, high_level_pos-1);
		Expr B = parse_final_code(st, high_level_pos+1, R);
		if(st[high_level_pos]=='*')return make_mul_expr(A,B);
		else if(st[high_level_pos]=='/')return make_div_expr(A,B);
		else return make_mod_expr(A,B);
	}
	else if('0'<=st[L]&&st[L]<='9')
	{
		return Expr(atoi(st.substr(L,R-L+1).c_str()));
	}
	else//find [ ]
	{
		int la=-1;
		for(int i=L; i<=R; i++)
		{
			if(st[i]=='[')
			{
				la=i;
				break;
			}
		}
		if(la==-1)//scalar
		{
			string name=st.substr(L, R-L+1);
			return Var::make(data_type, name, {}, {1});
		} 
		else 
		{
			string name=st.substr(L, la-L);
			return Var::make(data_type, name, parse_idx_list(st,la,R), get_var_shape(name));
		}
	}
}
string remove_space(string st)
{
	static char ret[10005];
	int len=0;
	for(int i=0; i<st.length(); i++)
	{
		if(st[i]==' ' || (st[i]=='/' && st[i-1]=='/'));
		else ret[len++]=st[i];
	}
	ret[len]='\0';
	return string(ret);
}

void print_shape(string name) {
	vector<size_t> shape;
	shape = get_var_shape(name);
	if (shape.size() == 1 && shape[0] == 1) return;
	for (int j = 0; j < shape.size(); ++j) {
		output_file << "[" << shape[j] << "]";
	}
}

void create_head() {
	output_file << "#include \"../run2.h\"\n\n";
	output_file << "void " + global_name + "(";
	int num = 0;
	for (int i = 0; i < in_name.size(); ++i) {
		if (num)
			output_file << ",";
		++num;
		output_file << global_type << " " << "(&" << in_name[i] << ")";
		print_shape(in_name[i]);
	}
	for (int i = 0; i < out_name.size(); ++i) {
		if (var_map[out_name[i]].is_in == 1)
			continue;
		if (num)
			output_file << ",";
		++num;
		output_file << global_type << " " << "(&" << out_name[i] << ")";
		print_shape(out_name[i]);
	}
	output_file << ")" << endl << "{" << endl;
        int blankspace = 2;	
	for (int i = 0; i < blankspace; ++i)
		output_file << " ";
	output_file << global_type << " ";
	
	cout << tmp_list.size() << endl;
	for (int i = 0; i < tmp_list.size(); ++i) {
		if (i > 0)
			output_file << ", ";
		output_file << "tmp" << i;
		print_shape(tmp_list[i]);
	}
	output_file << ";" << endl;
	
}


void make_stmt()
{
	data_type = (global_type == "int")?(Type::int_scalar(32)):(Type::float_scalar(32));
	IRPrinter printer;
	vector<Stmt>all_stmt_list;
	all_stmt_list.clear();
	int tmp_num = 0;
	string tmp_name;
	for(auto stmt: stmt_list)
	{
		this_stmt = stmt;
		vector<Stmt>stmts_in_loop;
		vector<Expr>LHS_idx_list;
		for(auto LHS_id_name: stmt.LHS_id_list)
		{
			Id id = stmt.id_map[LHS_id_name];
			LHS_idx_list.push_back(make_idx(id));
		}
		tmp_name = "tmp" + to_string(tmp_num++);
		Expr expr_tmp = Var::make(data_type, tmp_name, LHS_idx_list, get_var_shape(stmt.LHS_var.name));
		tmp_list.push_back(stmt.LHS_var.name);
		Stmt tmp_clear_stmt = Move::make(expr_tmp, Expr(0), MoveType::LocalToMem);
		stmts_in_loop.push_back(tmp_clear_stmt);
		for(int i=0; i!=stmt.factor_list.size(); i++)
		{
			string final_code = remove_space(stmt.factor_list[i].final_code);
			
			Expr factor_expr = parse_final_code(final_code, 0, final_code.length()-1);
			Stmt factor_stmt = Move::make(
				expr_tmp,Binary::make(data_type, (i > 0 && stmt.factor_ops[i-1]==2?(BinaryOpType::Sub):(BinaryOpType::Add)), expr_tmp, factor_expr),MoveType::MemToMem
				
			);
			
			Stmt if_stmt = factor_stmt;
			for(auto id_expr: stmt.factor_list[i].id_expr_list)
			{
				string st = remove_space(id_expr.name);
				Expr judge = Compare::make(index_type, CompareOpType::LT, parse_idx(st, 0, st.length()-1), Expr(id_expr.upbound));
				if_stmt = IfThenElse::make(judge, if_stmt, Stmt());
                Expr judge2 = Compare::make(index_type, CompareOpType::LE, Expr(0), parse_idx(st, 0, st.length()-1));    
                if_stmt = IfThenElse::make(judge2, if_stmt, Stmt());
			}
			vector<Expr>idx_list;
			for(auto id_name: stmt.factor_list[i].undef_id_list)
			{
				Id id = stmt.id_map[id_name];
				idx_list.push_back(make_idx(id));
			}
			Stmt loop_nest = LoopNest::make(idx_list, {if_stmt});
			stmts_in_loop.push_back(loop_nest);
		}
		Stmt loop_nest = LoopNest::make(LHS_idx_list, stmts_in_loop);
		string LHS_name = stmt.LHS_var.name;
		Expr expr_LHS = Var::make(data_type, LHS_name, LHS_idx_list, get_var_shape(LHS_name));
		Stmt move_from_tmp_to_LHS = LoopNest::make(LHS_idx_list, {Move::make(expr_LHS, expr_tmp, MoveType::MemToMem)});
		all_stmt_list.push_back(loop_nest);
		all_stmt_list.push_back(move_from_tmp_to_LHS);
	}
	cout<<"----------------ans-------------------\n";
	if (global_name == "") return;
	create_head();
	output_file<<printer.print(Kernel::make({}, {}, {}, all_stmt_list, KernelType::CPU));
	output_file << "}" << endl;

}



int main()
{
	string input_file_name;
	string output_file_name;

	for(int i=1; i<=10; i++)//1-10
	{
		input_file_name = "../project2/cases/grad_case"+to_string(i)+".json";
		output_file_name = "../project2/kernels/grad_case"+to_string(i)+".cc";
		cout << output_file_name << endl;
		input_file.open(input_file_name, ios::in);
		output_file.open(output_file_name, ios::out | ios::trunc);
		
		cout<<endl<<"dealing with case "<<to_string(i)<<endl;
		global_name = "";
		parse_json();
		build_map();
		parse_into_stmt();
		debug();
		tmp_list.clear();
		make_stmt();
		input_file.close();
		output_file.close();
	}
	return 0;
} 
