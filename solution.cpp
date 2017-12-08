#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <thread>

using namespace std;


#define NUM_OF_REGISTERS 8

int split(string s, string delimiter, vector<string> &result){
	size_t pos = 0;
	string token;
	while ((pos = s.find(delimiter)) != std::string::npos) {
	    token = s.substr(0, pos);
	    result.push_back(token);
	    s.erase(0, pos + delimiter.length());
	}
	result.push_back(s);
	return 0;
}

bool startsWith(string s, string s2){
	string substring = string(s.c_str(), s2.size());
	return substring == s2;
}

typedef enum {
	ADD,
	ADDI,
	SUB,
	MUL,
	DIV ,
	BRANCH,
	BRANCH_EQ,
	BRANCH_N_EQ,
	END,
	NO_OP
} OPERATOR_TYPE;

static unordered_map<string, OPERATOR_TYPE> operator_map;

void static setupEnums(){
	operator_map["add"] =  OPERATOR_TYPE::ADD;
	operator_map["add"] = OPERATOR_TYPE::ADDI;
	operator_map["sub"]	= OPERATOR_TYPE::SUB;
	operator_map["mul"] = OPERATOR_TYPE::MUL;
	operator_map["div"] = OPERATOR_TYPE::DIV;
	operator_map["b"] = OPERATOR_TYPE::BRANCH;
	operator_map["beq"] = OPERATOR_TYPE::BRANCH_EQ;
	operator_map["bnq"] = OPERATOR_TYPE::BRANCH_N_EQ;
	operator_map["end"] = OPERATOR_TYPE::END;
	operator_map[""] = OPERATOR_TYPE::NO_OP;
}

class InstructionParser;

class Unit {
public:
	InstructionParser* currentInstruction;
	vector<int>* t_vars;
	Unit* previousUnit;
	unordered_map<string, int> *labelToInstructionNumber;
	Unit(vector<int>* t_vars, Unit* previousUnit, unordered_map<string, int> *labelToInstructionNumber);

	virtual int getNextInstruction(InstructionParser* &nextInstruction) = 0;
	virtual int doWork() = 0;
	int work(){
		int rc = 0;
		if(this->currentInstruction != 0){
			rc = doWork();
		}
		if(rc == 0) {
			rc = this->getNextInstruction(this->currentInstruction);
		}
		return rc;
	}

};

class Operator {
public:
	vector<string> opcodesVector;
	vector<int>* register_values;
	int computedValue;
	int* pc;

	Operator(string opcodes, vector<int>* register_values, int &programCounter){
		string delimiter = ",";
		split(opcodes, delimiter, opcodesVector);
		this->register_values = register_values;
		this->pc = &programCounter;
		this->computedValue = NULL;
	}

	int getResolvedOperand(int index, void* data){
		string operand = this->opcodesVector[index];
		if(startsWith(operand, "label")){
			memcpy(data, operand.c_str(), sizeof(char)*operand.size()+1);
		} else if(startsWith(operand, "$")){
			int registerIndex = stoi(operand.substr(1, operand.size()));
			memcpy(data, &(this->register_values->at(registerIndex)), sizeof(int));
		} else {
			int immediateValue = stoi(operand.substr(0, operand.size()));
			memcpy(data, &immediateValue, sizeof(int));
		}
	}

	int setRegister(int operandIndex, int value){
		string operand = this->opcodesVector[operandIndex];
		int registerIndex = stoi(operand.substr(1, operand.size()));
		this->register_values->at(registerIndex) = value;
		return 0;
	}

	virtual void writeBack() = 0;

	virtual int execute(Unit* unit) = 0;
};

class AddOpeartor: public Operator {
public:
	AddOpeartor(string opcodes, vector<int>* register_values, int &programCounter):Operator(opcodes, register_values, programCounter){
	}

	int execute(Unit* unit){
		int a;
		int b;
		this->getResolvedOperand(1, &a);
		this->getResolvedOperand(2, &b);
		this->computedValue = a+b;
	}

	void writeBack(){
			this->setRegister(0, this->computedValue);
	}
};

class MulOperator: public Operator{
public:
	MulOperator(string opcodes, vector<int>* register_values, int &programCounter):Operator(opcodes, register_values, programCounter){
	}
	int execute(Unit* unit){
		int a;
		int b;
		this->getResolvedOperand(1, &a);
		this->getResolvedOperand(2, &b);
		this->computedValue = a*b;
	}

	void writeBack(){
		this->setRegister(0, this->computedValue);
	}
};


class DivOperator: public Operator{
public:
	DivOperator(string opcodes, vector<int>* register_values, int &programCounter):Operator(opcodes, register_values, programCounter){
	}
	int execute(Unit* unit){
		int a;
		int b;
		this->getResolvedOperand(1, &a);
		this->getResolvedOperand(2, &b);
		this->computedValue = a/b;
	}

	void writeBack(){
		this->setRegister(0, this->computedValue);
	}
};

class SubtractOperator: public Operator{
public:
	SubtractOperator(string opcodes, vector<int>* register_values, int &programCounter):Operator(opcodes, register_values, programCounter){
	}
	int execute(Unit* unit){
		int a;
		int b;
		this->getResolvedOperand(1, &a);
		this->getResolvedOperand(2, &b);
		this->computedValue = a-b;
	}

	void writeBack(){
		this->setRegister(0, this->computedValue);
	}
};

class BranchOperator: public Operator {
public:
	BranchOperator(string opcodes, vector<int>* register_values, int &programCounter):Operator(opcodes, register_values, programCounter){
	}
	int execute(Unit* unit){
	//	#TODO
		char* jumpLabel = (char*)malloc(1000);
		this->getResolvedOperand(0, jumpLabel);
		string label = string(jumpLabel);
		int newProgramCounter = unit->labelToInstructionNumber->at(label);
		memcpy(this->pc, &newProgramCounter, sizeof(int));
		free(jumpLabel);
	}
	void writeBack(){
	//		do nothing
	}
};

class BranchEqOperator: public Operator{
public:
	BranchEqOperator(string opcodes, vector<int>* register_values, int &programCounter):Operator(opcodes, register_values, programCounter){
	}
	int execute(Unit* unit){
		int value1;
		int value2;
		this->getResolvedOperand(0, &value1);
		this->getResolvedOperand(1, &value2);
		if(value1 == value2){
			char* jumpLabel = (char*)malloc(1000);
			this->getResolvedOperand(2, jumpLabel);
			string label = string(jumpLabel);
			int newProgramCounter = unit->labelToInstructionNumber->at(label);
			memcpy(this->pc, &newProgramCounter, sizeof(int));
			free(jumpLabel);
		}
	}
	void writeBack(){
		// do nothing
	}
};

class BranchNeqOperator: public Operator {
public:
	BranchNeqOperator(string opcodes, vector<int>* register_values, int &programCounter):Operator(opcodes, register_values, programCounter){
	}
	int execute(Unit* unit){
		int value1;
		int value2;
		this->getResolvedOperand(0, &value1);
		this->getResolvedOperand(1, &value2);
		if(value1 != value2){
			char* jumpLabel = (char*)malloc(1000);
			this->getResolvedOperand(2, jumpLabel);
			string label = string(jumpLabel);
			int newProgramCounter = unit->labelToInstructionNumber->at(label);
			memcpy(this->pc, &newProgramCounter, sizeof(int));
			free(jumpLabel);
		}
	}
	void writeBack(){
	//	do nothing
	}
};

class InstructionParser {
public:
	string data;
	string label;
	OPERATOR_TYPE operatorType;
	Operator* oper;
	InstructionParser();
	static InstructionParser* parse(string instuction, string instructionNumber, vector<int>* register_values, int&programCounter);
	string getLabel();
	OPERATOR_TYPE getOperatorType();

};

OPERATOR_TYPE InstructionParser::getOperatorType(){
	return this->operatorType;
}

InstructionParser::InstructionParser(){
}

InstructionParser* InstructionParser::parse(string instruction,string instructionNumber, vector<int>* register_values, int& programCounter){
	InstructionParser* ip = new InstructionParser();
	ip->data = instruction;
	vector<string> words;
	string delimter = " ";
	OPERATOR_TYPE opeartorType;
	string opcodes;
	split(ip->data, delimter, words);
	if(startsWith(words[0], "label")){
		ip->label = words[0];
		opeartorType = operator_map[words[1]];
		opcodes = words[2];
	} else {
		ip->label = instructionNumber;
		opeartorType = operator_map[words[0]];
		opcodes = words[1];
	}

	switch(opeartorType){

		case OPERATOR_TYPE::ADD:
			ip->operatorType = OPERATOR_TYPE::ADD;
			ip->oper = new AddOpeartor(opcodes, register_values, programCounter);
			break;

		case OPERATOR_TYPE::ADDI:
			ip->operatorType = OPERATOR_TYPE::ADDI;
			ip->oper = new AddOpeartor(opcodes, register_values, programCounter);
			break;

		case OPERATOR_TYPE::BRANCH:
			ip->operatorType = OPERATOR_TYPE::BRANCH;
			ip->oper = new BranchOperator(opcodes, register_values, programCounter);
			break;

		case OPERATOR_TYPE::BRANCH_EQ:
			ip->operatorType = OPERATOR_TYPE::BRANCH_EQ;
			ip->oper = new BranchEqOperator(opcodes, register_values, programCounter);
			break;

		case OPERATOR_TYPE::BRANCH_N_EQ:
			ip->operatorType = OPERATOR_TYPE::BRANCH_N_EQ;
			ip->oper = new BranchNeqOperator(opcodes, register_values, programCounter);
			break;

		case OPERATOR_TYPE::DIV:
			ip->operatorType = OPERATOR_TYPE::DIV;
			ip->oper = new DivOperator(opcodes, register_values, programCounter);
			break;

		case OPERATOR_TYPE::MUL:
			ip->operatorType = OPERATOR_TYPE::MUL;
			ip->oper = new MulOperator(opcodes, register_values, programCounter);
			break;

		case OPERATOR_TYPE::SUB:
			ip->operatorType = OPERATOR_TYPE::SUB;
			ip->oper = new SubtractOperator(opcodes, register_values, programCounter);
			break;

		case OPERATOR_TYPE::END:
			ip->operatorType = OPERATOR_TYPE::END;
			break;
	}

	return ip;
}

string InstructionParser::getLabel(){
	return this->label;
}

class IFUnit: public Unit {
public:
	vector<int>* t_vars;
	vector<InstructionParser*> *instructionParserVector;
	int *programCounter;
	IFUnit(vector<int>* t_vars, unordered_map<string, int> *labelToInstructionNumber, vector<InstructionParser*> *instructionParserVector, int *programCounter);
	int getNextInstruction(InstructionParser* &nextInstruction);
	int doWork();
};


Unit::Unit(vector<int>* t_vars, Unit* previousUnit, unordered_map<string, int>* labelToInstructionNumber){
	this->t_vars = t_vars;
	this->previousUnit = previousUnit;
	this->currentInstruction = NULL;
	this->labelToInstructionNumber = labelToInstructionNumber;
};

IFUnit::IFUnit(vector<int>* t_vars, unordered_map<string, int> *labelToInstructionNumber, vector<InstructionParser*> *instructionParserVector, int *programCounter):Unit(t_vars, NULL, labelToInstructionNumber){
	this->t_vars = t_vars;
	this->instructionParserVector = instructionParserVector;
	this->programCounter = programCounter;
}

int IFUnit::getNextInstruction(InstructionParser* &nextInstruction){
	int pc = *(this->programCounter) % this->instructionParserVector->size();
	InstructionParser* ip = this->instructionParserVector->at(pc);
	nextInstruction = ip;
	int newProgramCounter = (*(this->programCounter) + 1) % this->instructionParserVector->size();
	memcpy(this->programCounter, &newProgramCounter, sizeof(int));
	if(this->currentInstruction->getOperatorType() == OPERATOR_TYPE::END){
		return -1;
	}
	return 0;
}

int IFUnit::doWork(){
	cout << this->currentInstruction->data << endl;
	this->currentInstruction->oper->execute(this);
	this->currentInstruction->oper->writeBack();
	for(int i=0; i<this->t_vars->size(); i++){
		cout << this->t_vars->at(i) << "\t";
	}
	cout << endl;
	return 0;
}

class solution {

private:

bool DEBUG;
int clck;
vector<string> vect_lines;
vector<InstructionParser*> vec_lines_parser;
vector<int>* t_vars;
unordered_map<string, int> instructionsMap;
Unit* unit;
int programCounter;

public :

solution(ifstream &file_in,int clck_in = 10 ,bool DEBUG_in = false){
	setupEnums();
	this->clck = clck_in;
	this->DEBUG = DEBUG_in;
	this->t_vars = new vector<int>();

//	Setting initial state of registers starts
	string initial_state;
	getline(file_in, initial_state);
	vector<string> initialStateVector;
	string delimter = ",";
	split(initial_state, delimter, initialStateVector);

	for(int i =0; i< NUM_OF_REGISTERS; i++){
		int regValue = stoi(initialStateVector[i]);
		this->t_vars->push_back(regValue);
	}
	cout << endl;

//	Setting initial state of registers ends

//	Parsing instructions starts

	int i=0;
	while(!file_in.eof()){
	string instruction;
	getline(file_in, instruction);
	this->vect_lines.push_back(instruction);
	InstructionParser* ip = InstructionParser::parse(instruction, to_string(i), this->t_vars ,programCounter);
	this->vec_lines_parser.push_back(ip);
	this->instructionsMap[ip->getLabel()] = i;
	i++;
	}

//	Parsing instructions end
	programCounter = 0;
	this->unit = new IFUnit(this->t_vars, &(this->instructionsMap), &(this->vec_lines_parser), &(this->programCounter));
}
void dbg(const string &msg){
//	#TODO
}

vector<int>* alu() {
	int rc;
	int cycleCount = 0;
	for(int i =0; i< NUM_OF_REGISTERS; i++){
		cout << this->t_vars->at(i) << ",";
	}
	cout << endl;

	while(true){
		int cycle= mips_clock();
		if(cycle == 0){
			rc = this->unit->work();
			cycleCount++;
			if(rc ==-1){
				break;
			}
		}
	}

	cout << "Total cycle Elapsed: " << cycleCount << endl;
	return t_vars;
}
int mips_clock();

};



int solution::mips_clock() {
chrono::milliseconds timespan(clck); 

this_thread::sleep_for(timespan);
static int cycle = 0;
if (cycle == 0 )
	cycle = 1;
else 
	cycle = 0;
return cycle;
}

