#include<iostream>
#include<cstring>
#include<cstdio>
#include<fstream>
#include<map>
#include<vector>
#include<algorithm>
using namespace std;
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

vector<string> grad_to_name; //add: 需要求导的变量
vector<string>grad_result;
string grad_kernel;
void parse_json()
{
	var_map.clear();
	in_name.clear();
	out_name.clear();
	grad_to_name.clear();
	grad_result.clear();
	grad_kernel="";
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
		if(line.find("\"grad_to\"") != string::npos)
		{
			string::size_type left_idx = line.find("["), right_idx;
			left_idx = line.find("\"", left_idx);
			while(left_idx != string::npos)
			{
				right_idx = line.find("\"", left_idx+1);
				string tmp_grad_to_name = line.substr(left_idx+1, right_idx-left_idx-1);
				grad_to_name.push_back(tmp_grad_to_name);
				left_idx = line.find("\"", right_idx+1);
			}
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
				id.upbound = min(id.upbound, upbound);
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
				id.upbound = min(id.upbound, upbound);
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
			string factor_string = RHS.substr(last_idx, i);
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

string remove_space_and_brackets(string st)
{
	int len = 0, inbracket = 0;
	static char ret[10005];
	for(int i=0; i<st.length(); i++)
	{
		if(st[i]=='<'||st[i]=='[')inbracket++;
		if(st[i]==' '||st[i]==';'||inbracket!=0);
		else ret[len++]=st[i];
		if(st[i]=='>'||st[i]==']')inbracket--;
	}
	ret[len++]='\0';
	return string(ret);
}

void clear_buffer(char buffer[], vector<string> &expr_ele, int &len)
{
	if(len>0)
	{
		buffer[len++]='\0';
		expr_ele.push_back(string(buffer));
		len=0;
	}
}

void grad(string expr, vector<string> &grad_to_name)
{
	vector<string>expr_ele;
	static char buffer[10005];
	int len = 0, flag = 0;
	for(int i=0; i<expr.length(); i++)//非常naive的词法分析
	{
		if(flag==1)
		{
			clear_buffer(buffer, expr_ele, len);
			flag=0;
		}
		if(expr[i]=='+'||expr[i]=='-'||expr[i]=='*'||expr[i]=='/'||expr[i]=='='||expr[i]=='('||expr[i]==')')
		{
			clear_buffer(buffer, expr_ele, len);
			flag=1;
		}
		buffer[len++]=expr[i];
	}
	clear_buffer(buffer, expr_ele, len);

	cout<<"grad_expr:"<<expr<<endl;

	for(auto grad_to_var: grad_to_name)
	{
		string dLValue = "d"+expr_ele[0];
		string ans = "";
		//cout<<"grad_to_var="<<"\""<<grad_to_var<<"\"\n";
		for(int i=2; i<expr_ele.size(); i++)
		{
			//cout<<"expr_ele[i]="<<"\""<<expr_ele[i]<<"\"\n";
			if(expr_ele[i]==grad_to_var)
			{
				string core = dLValue;
				int core_L = i, core_R = i;
				while(1)
				{
					int L, R;
					string now = "";
					int inbracket = 0;
					for(L=core_L-1; L>=2; L--)
					{
						if(expr_ele[L]==")")inbracket++;
						if(expr_ele[L]=="(")inbracket--;
						if(inbracket==0 && (expr_ele[L]=="+" || expr_ele[L]=="-"))break;
						if(inbracket==-1)break;
					}
					L++;
					inbracket = 0;
					for(R=core_R+1; R<expr_ele.size(); R++)
					{
						if(expr_ele[R]=="(")inbracket++;
						if(expr_ele[R]==")")inbracket--;
						if(inbracket==0 && (expr_ele[R]=="+" || expr_ele[R]=="-"))break;
						if(inbracket==-1)break;
					}
					R--;
					for(int j=L; j<core_L; j++)now += expr_ele[j];
					now += core;
					for(int j=core_R+1;j<=R;j++) now += expr_ele[j];
					core = now;

					inbracket = 0;
					for(L=core_L-1; L>=2; L--)
					{
						if(expr_ele[L]==")")inbracket++;
						if(expr_ele[L]=="(")inbracket--;
						if(inbracket==-1)break;
					}
					inbracket = 0;
					for(R=core_R+1; R<expr_ele.size(); R++)
					{
						if(expr_ele[R]=="(")inbracket++;
						if(expr_ele[R]==")")inbracket--;
						if(inbracket==-1)break;
					}
					if(L>1 && R<expr_ele.size() && expr_ele[L]=="(" && expr_ele[R]==")")
					{
						core_L = L; core_R = R;
					}
					else break;
				}
				if(ans!="")ans+="+";
				ans+=core;
			}
		}
		cout<<"grad_to_var:"<<grad_to_var<<" grad_result:"<<ans<<endl;
		grad_result.push_back(ans);
	}
}
class MyVar2
{
	public :
	string name;
	int dim;
	int dim_list[100];
	string id_expr_list[100];
	MyVar2()
	{
		memset(dim_list, 0, sizeof(dim_list));
	}
};
vector<MyVar2>LHS_var_list;
map<string, MyVar2>RHS_var_map;
bool is_first_grad_to;
int used_id;
const string big_alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
int LHS_var_cnt;
bool is_in_grad_to_name(string var_name)
{
	for(auto grad_name:grad_to_name)
	{
		if(grad_name == var_name)return true;
	}
	return false;
}
void parse_grad_id_expr_list(MyVar2 &var2, string id_expr_list_string)
{
	int dim_idx=1;
	string::size_type left_idx = 0, right_idx = id_expr_list_string.find(",");
	while(right_idx != string::npos)
	{
		string id_expr = id_expr_list_string.substr(left_idx, right_idx-left_idx);
		var2.id_expr_list[dim_idx] = id_expr;
		left_idx = right_idx+1, right_idx = id_expr_list_string.find(",", left_idx);
		dim_idx++;
	}
	string id_expr = id_expr_list_string.substr(left_idx);
	var2.id_expr_list[dim_idx] = id_expr;

}
void parse_grad_LHS(string LHS)
{
	string::size_type left_idx = 0, right_idx = LHS.find("<");
	string LHS_var_string = LHS.substr(left_idx, right_idx-left_idx);
	MyVar2 LHS_var = MyVar2();
	MyVar tmp_var = var_map[LHS_var_string];
	LHS_var.name = LHS_var_string;
	LHS_var.dim = tmp_var.dim;
	for(int i=1;i<=LHS_var.dim;i++)
	{
	LHS_var.dim_list[i]=tmp_var.dim_list[i];
	}
	//cout<<"    LHS_var "<<LHS_var.name<<" "<<LHS_var.dim<<endl;
	left_idx = LHS.find("[");right_idx = LHS.find("]");
	string id_expr_list_string = LHS.substr(left_idx+1, right_idx-left_idx-1);
	parse_grad_id_expr_list(LHS_var, id_expr_list_string);
	LHS_var_list.push_back(LHS_var);
}
void change_id_expr(MyVar2 &var2)
{
	MyVar2 tmp_var = LHS_var_list[0];
	for(int i=1;i<=var2.dim;i++)
	{
		string id_expr = var2.id_expr_list[i];
		string::size_type idx=id_expr.find("+");
		if(idx != string::npos)
		{
			char new_id_char;
			if(tmp_var.id_expr_list[i][1] == id_expr.substr(0,1)[1])
			new_id_char = tmp_var.id_expr_list[i][0];		
			else new_id_char = 'z'-used_id;
			string new_id = "";
			new_id.append(1, new_id_char);
			used_id++;
			var2.id_expr_list[i] = new_id;
			string minus_id = id_expr.substr(idx+1);
			tmp_var.id_expr_list[i] = new_id + "-" + minus_id;
		}
		idx=id_expr.find("//");
		if(idx != string::npos)
		{
			string new_id = id_expr.substr(0, idx);
			string times_id = id_expr.substr(idx+2);
			var2.id_expr_list[i] = new_id;
			tmp_var.id_expr_list[i] = new_id + "*" + times_id;
		}
		idx=id_expr.find("%");
		if(idx != string::npos)
		{
			char new_id_char = 'z'-used_id;
			string new_id = "";
			new_id.append(1, new_id_char);
			used_id++;
			var2.id_expr_list[i] = new_id;
			tmp_var.id_expr_list[i-1] = tmp_var.id_expr_list[i-1] + "+" + new_id;
			
		}
	}
	if(is_first_grad_to == true){is_first_grad_to = false;LHS_var_list[0] = tmp_var;}
	else LHS_var_list.push_back(tmp_var);
}
void parse_grad_RHS(string RHS)
{
	string::size_type right_idx = RHS.find("<"), left_idx = right_idx-1;
	while(right_idx != string::npos)
	{
		string var_name = RHS.substr(left_idx, right_idx-left_idx);
		MyVar2 var2 = MyVar2();
		var2.name = var_name;
		MyVar tmp_var = var_map[var_name];
		var2.dim = tmp_var.dim;
		for(int i=1;i<=var2.dim;i++)
		{
			var2.dim_list[i] = tmp_var.dim_list[i];
		}
		left_idx = RHS.find("[",left_idx);
		right_idx = RHS.find("]", left_idx);
		string id_expr_list_string = RHS.substr(left_idx+1, right_idx-left_idx-1);
		parse_grad_id_expr_list(var2, id_expr_list_string);
		if(is_in_grad_to_name(var_name) && grad_to_name.size() == 1)change_id_expr(var2);
		if(RHS_var_map.find(var_name) == RHS_var_map.end())RHS_var_map[var_name] = var2;
		right_idx = RHS.find("<",right_idx);
		left_idx = right_idx-1;
	}
}
void grad_debug()
{
	for(auto LHS_var : LHS_var_list)
	{
		cout<<LHS_var.name<<" : ";
		for(int i=1;i<=LHS_var.dim;i++)
		{
			cout<<LHS_var.id_expr_list[i]<<" "<<LHS_var.dim_list[i]<<" ,";
		}
		cout<<endl;
	}
	for(auto RHS_var : RHS_var_map)
	{
		cout<<RHS_var.second.name<<" : ";
		for(int i=1;i<=RHS_var.second.dim;i++)
		{
			cout<<RHS_var.second.id_expr_list[i]<<" "<<RHS_var.second.dim_list[i]<<" ,";
		}
		cout<<endl;
	}
}
void parse_grad_kernel()
{
	string::size_type left_idx = 0, right_idx = kernel.find("=");
	string LHS = kernel.substr(left_idx, right_idx-left_idx);
	parse_grad_LHS(LHS);

	left_idx = kernel.find("=");
	string RHS = kernel.substr(left_idx+1);
	parse_grad_RHS(RHS);
	grad_debug();
}
void parse_grad_result(string grad_name,string grad_result)
{
	cout<<grad_name<<" "<<grad_result<<endl;
	grad_kernel += "d" + grad_name + "<";
	MyVar2 var2 = RHS_var_map[grad_name];
	for(int i=1;i<var2.dim;i++)
	{
		grad_kernel+=to_string(var2.dim_list[i])+", ";
	}
	grad_kernel+=to_string(var2.dim_list[var2.dim])+">[";
	for(int i=1;i<var2.dim;i++)
	{
		grad_kernel+=var2.id_expr_list[i]+", ";
	}
	grad_kernel+=var2.id_expr_list[var2.dim]+"] = ";
	string::size_type idx=grad_result.find_first_of(big_alpha, 0), last_idx = 0;
	while(idx != string::npos)
	{
		grad_kernel+=grad_result.substr(last_idx, idx-last_idx+1);
		string var_name = grad_result.substr(idx,1);
		MyVar2 var2;
		if(var_name == LHS_var_list[0].name)
		{
			var2 = LHS_var_list[LHS_var_cnt];
			LHS_var_cnt++;
		}
		else 
		{
			var2 = RHS_var_map[var_name];
		}
		grad_kernel+="<";
		for(int i=1;i<var2.dim;i++)
		{
			grad_kernel+=to_string(var2.dim_list[i])+", ";
		}
		grad_kernel+=to_string(var2.dim_list[var2.dim])+">[";
		for(int i=1;i<var2.dim;i++)
		{
			grad_kernel+=var2.id_expr_list[i]+", ";
		}
		grad_kernel+=var2.id_expr_list[var2.dim]+"] ";
		last_idx = idx+1;
		idx=grad_result.find_first_of(big_alpha, last_idx);
	}
	grad_kernel+=grad_result.substr(last_idx, idx-last_idx+1);
	grad_kernel+=";";
	cout<<"!!"<<grad_kernel<<endl;
}
void build_grad_kernel()
{
	LHS_var_list.clear();
	RHS_var_map.clear();
	is_first_grad_to = true;
	used_id = 0;
	cout<<"kernel : "<<kernel<<endl;
	parse_grad_kernel();
	for(int i=0;i<grad_to_name.size();i++)
	{
		LHS_var_cnt=0;
		parse_grad_result(grad_to_name[i], grad_result[i]);
	}
	
}
bool grad_to_is_not(string name)
{
	for(auto grad_name : grad_to_name)
	{
		if(grad_name != name)return true;
	}
    for(auto result : grad_result)
	{
		if(count(result.begin(),result.end(), name[0]) >1)return true;
	} 
	return false;
}
void output_json(int idx)
{
	MyVar tmp_var;
	bool is_first = true;
	output_file<<"{"<<endl;
	output_file<<"    \"name\": \"grad_case" + to_string(idx)+ "\","<<endl;
	output_file<<"    \"ins\": [";
	for(auto name : in_name)
	if(grad_to_is_not(name))
	{
		if(is_first)is_first = false;
		else output_file<<", ";
		output_file<<"\""<<name<<"\"";
	}
	for(auto name : out_name)
	{
		if(is_first)is_first = false;
		else output_file<<", ";
		output_file<<"\"d"<<name<<"\"";
	}
	output_file<<"],"<<endl;
	output_file<<"    \"outs\": [";
	is_first = true;
	for(auto name : grad_to_name)
	{
		if(is_first)is_first = false;
		else output_file<<", ";
		output_file<<"\"d"<<name<<"\"";
	}
	output_file<<"],"<<endl;
	output_file<<"    \"data_type\": \""<<global_type<<"\","<<endl;
	output_file<<"    \"kernel\": \""<<grad_kernel<<"\","<<endl;
	output_file<<"}";
}
int main()
{
	string input_file_name;
	string output_file_name;

	//求导部分使用示例
	/*grad_to_name.push_back("A");
	grad_to_name.push_back("B");
	grad_to_name.push_back("C");
	grad("D=A*(B+C)/3.0",grad_to_name);
	return 0;*/

	for(int i=1; i<=10; i++)//1-10
	{
		input_file_name = "../project2/cases/case"+to_string(i)+".json";
		output_file_name = "../project2/cases/grad_case"+to_string(i)+".json";
		cout << output_file_name << endl;
		input_file.open(input_file_name, ios::in);
		output_file.open(output_file_name, ios::out | ios::trunc);
		
		cout<<endl<<"dealing with case "<<to_string(i)<<endl;
		parse_json();
		build_map();
		parse_into_stmt();
		//debug();
		tmp_list.clear();
		
		//add
		string expr = remove_space_and_brackets(kernel);
		grad(expr, grad_to_name);
		build_grad_kernel();
		output_json(i);
		input_file.close();
		output_file.close();
	}
	return 0;
} 
