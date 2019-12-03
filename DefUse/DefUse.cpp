#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/ADT/SCCIterator.h"
#include "DefUse.h"
#include <vector>
#include <list>
#include <queue>
#include <fstream>

using namespace llvm;

static cl::opt <std::string> gVariableName("v", cl::desc("Specify variable name"), cl::value_desc("variablename"));
//    static cl::list <int> OffsetList("offset", cl::desc("Specify the position of the memeber in a struct variable"));
static cl::opt <std::string> bit_name("b", cl::desc("Specify the bit variable name"), cl::value_desc("bit variable name"));


/*
 * Perform the static analysis to each module,
 */
bool DefUse::runOnModule(Module &M)  {
    //TODO: automatically bound the variable name to the offset
    std::vector<int> offsetList;
    offsetList.push_back(14);
    offsetList.push_back(10);
    std::vector<Value *> variables;
    std::vector<Value *> callers;
    uint64_t bit_value;


    errs() << "We want to trace the uses of variable: " << gVariableName << "\n";
    if (bit_name != "")
        bit_value = getBitValue(bit_name);

    errs() << "The bit name is " << bit_name << "\n";

    //If the variable is a global variable, get the variable usage
    for (auto &Global : M.getGlobalList()) {
        if (Global.getName() == gVariableName) {
            errs() << "It is a global variable " << Global.getName() << "\n";
            getVariableUse(&Global);
            return false;
        }
    }

    //If the variable is a local variable or an argument, get the variable usage
    for (Module::iterator function = M.begin(), moduleEnd = M.end();
         function != moduleEnd; function++) {
        for (SymbolTableList<Argument>::iterator arg = function->arg_begin();
             arg != function->arg_end(); arg++) {
            if (arg->getName() == gVariableName) {
                if (bit_name != "") {
                    std::vector<Value *> options = getVariables(&(*arg),offsetList);
                    for (auto option:options) {
                        std::vector<Value *> v = getBitVariables(option,bit_value);
                        variables.insert(variables.end(), v.begin(), v.end());
                    }
                } else {
                    std::vector<Value *> v = getVariables(&(*arg),offsetList);
                    variables.insert(variables.end(), v.begin(), v.end());
                }
            }
        }

        for (Function::iterator block = function->begin(), functionEnd = function->end();
             block != functionEnd; ++block) {
            for (BasicBlock::iterator instruction = block->begin(), blockEnd = block->end();
                 instruction != blockEnd; instruction++) {
                if (bit_name != "") {
                    if (CallInst *callInst = dyn_cast<CallInst>(instruction)) {
                        if (Function *calledFunction = callInst->getCalledFunction()) {
                            if (calledFunction->getName() == "thd_test_options"){
                                if (llvm::ConstantInt* CI = dyn_cast<llvm::ConstantInt>(callInst->getArgOperand(1))) {
                                    if (CI->getSExtValue() & bit_value)
                                        callers.push_back(callInst);
                                }

                            }
                        }
                    }
                    Instruction *inst = dyn_cast<Instruction>(instruction);
                    if (inst->getName() == gVariableName) {
                        std::vector<Value *> options = getVariables(inst,offsetList);
                        for (auto option:options) {
                            std::vector<Value *> v = getBitVariables(option,bit_value);
                            variables.insert(variables.end(), v.begin(), v.end());
                        }
                    }

                } else {
                    Instruction *inst = dyn_cast<Instruction>(instruction);
                    if (inst->getName() == gVariableName) {
                        std::vector<Value *> v  = getVariables(inst,offsetList);
                        variables.insert(variables.end(), v.begin(), v.end());
                    }
                }
            }
        }

//        if (!variables.empty())
//            errs() << "In function " << function->getName() << ", the variable is used in instruction: \n";
//        for (auto variable: variables) {
//            getVariableUse(variable);
//        }

//        if (!callers.empty())
//            errs() << "In function " << function->getName() << ", the test_option_bits is called with "<< bit_name << " : \n";
//        for (auto caller:callers)
//            errs() << *caller << "\n";

        if (!callers.empty() || !variables.empty())
            target_functions.push_back(&(*function));

        variables.clear();
        callers.clear();
    }

    /*
     * Control flow analysis
     */

    CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
//    for (CallGraph::iterator i = CG.begin(); i != CG.end();i++) {
//        for(CallGraphNode::iterator j = i->second->begin(); j != i->second->end(); j++){
//            Function *func = j->second->getFunction();
//
//            if (func)
//                errs() << "Yigong HU " << func->getName() <<"\n";
//        }
//    }

    for (Function* function:target_functions) {
        getCallee(function, &CG);
    }
    return false;
}

void DefUse::getCallee(Function* function, CallGraph* CG) {
        CallGraphNode *CGN = (*CG)[function];
        if (Function *F = CGN->getFunction())
            errs() << "In Function: '" << F->getName() << "' the variable is used ";
        else
            errs() << "Call graph node <<null function>>";

        for (CallGraphNode::iterator ti = CGN->begin(); ti != CGN->end(); ++ti) {
            errs() << "  CS<" << ti->first << "> calls ";
            if (Function *FI = ti->second->getFunction())
                errs() << "function '" << FI->getName() <<"'\n";
            else
                errs() << "external node\n";
        }
        errs() << '\n';

}

uint64_t DefUse::getBitValue(std::string bit_name) {
    for(auto bitInfo:handlerInfo) {
        if (bitInfo.name == bit_name) {
            return bitInfo.bit;
        }
    }
    errs() <<"The bit can not be find\n";
    assert(0);
    return -1;
}

template<typename T>
bool DefUse::isPointStructVariable(T *variable) {
    Type *pointerElementTpye;

    if (!variable->getType()->isPointerTy())
        return variable->getType()->isStructTy();

    pointerElementTpye = variable->getType()->getPointerElementType();
    return pointerElementTpye->isStructTy();
}

/*
 * If the variable is load to other variable, also check the usage of the other variable
 */
void DefUse::handleMemoryAcess(Instruction *inst, variable_wrapper *variable,  std::vector< variable_wrapper>* immediate_variable) {
    if (StoreInst * storeInst = dyn_cast<StoreInst>(inst)) {
        if (storeInst->getValueOperand() == variable->variable) {
            struct variable_wrapper v;
            v.variable = storeInst->getPointerOperand();
            v.level = variable->level;
            immediate_variable->push_back(v);
        }
    }

    if (LoadInst * loadInst = dyn_cast<LoadInst>(inst)) {
        if (loadInst->getPointerOperand() == variable->variable) {
            struct variable_wrapper v;
            v.variable = loadInst;
            v.level = variable->level;
            immediate_variable->push_back(v);
        }
    }
    return;
}

template<typename T>
std::vector<Value *> DefUse::getBitVariables(T *variable, uint64_t bitvalue) {
    std::vector<Value *> immediate_variable;
    std::vector<Value *> result;
    immediate_variable.push_back(variable);

    while (!immediate_variable.empty()) {
        Value* value = immediate_variable.back();

        immediate_variable.pop_back();
        for (auto use: value->users()) {
            Instruction *inst_use = dyn_cast<Instruction>(use);
            if (StoreInst * storeInst = dyn_cast<StoreInst>(inst_use)) {
                if (storeInst->getValueOperand() == value) {
                    immediate_variable.push_back(storeInst->getPointerOperand());
                }
            }

            if (LoadInst * loadInst = dyn_cast<LoadInst>(inst_use)) {
                if (loadInst->getPointerOperand() == value) {
                    immediate_variable.push_back(loadInst);
                }
            }

            if (inst_use->getOpcode() == Instruction::And) {
                if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(inst_use->getOperand(0)))
                    if ((CI->getZExtValue() & bitvalue) && (CI->getSExtValue()  > 0)) {
                        result.push_back(inst_use);
                    }

                if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(inst_use->getOperand(1)))
                    if ((CI->getZExtValue() & bitvalue) && (CI->getSExtValue()  > 0)) {
                          result.push_back(inst_use);
                    }
            }
        }
    }
    return result;
}

/*
 * Find all the terget variable
 */
template<typename T>
std::vector<Value *> DefUse::getVariables(T *variable,std::vector<int> offsetList) {
    std::vector<struct variable_wrapper> immediate_variable;
    std::vector<Value *> result;
    struct variable_wrapper init_variable;

    init_variable.variable = variable;
    init_variable.level = 0;
    immediate_variable.push_back(init_variable);

    while (!immediate_variable.empty()) {
        struct variable_wrapper value = immediate_variable.back();

        immediate_variable.pop_back();
        if (value.level == offsetList.size()) {
            result.push_back(value.variable);
            continue;
        }
        for (User *U : value.variable->users()) {
            if (Instruction *Inst = dyn_cast<Instruction>(U)) {
                handleMemoryAcess(Inst, &value, &immediate_variable);
                if (isa<GetElementPtrInst>(Inst)) {
                    GetElementPtrInst *getElementPtrInst = dyn_cast<GetElementPtrInst>(Inst);
                    ConstantInt *structOffset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2));

                    if (structOffset->getValue() == offsetList[value.level]) {
                        struct variable_wrapper v;
                        v.variable = Inst;
                        v.level = value.level + 1;
                        immediate_variable.push_back(v);
                    }
                }
            }
        }
    }
    return  result;
}


/*
* Find all the instructions that use the target variable.
* @param variable the target variable
*/
template<typename T>
void DefUse::getVariableUse(T *variable) {
    std::vector<Value *> immediate_variable;
    immediate_variable.push_back(variable);

    while (!immediate_variable.empty()) {
        Value *value = immediate_variable.back();

        immediate_variable.pop_back();
        errs() << *value << "\n";
        for (User *U : value->users()) {
            Instruction *inst = dyn_cast<Instruction>(U);
            if (StoreInst *storeInst = dyn_cast<StoreInst>(inst)) {
                if (storeInst->getValueOperand() == value) {
                    immediate_variable.push_back(storeInst->getPointerOperand());
                }
            }

            if (LoadInst *loadInst = dyn_cast<LoadInst>(inst)) {
                if (loadInst->getPointerOperand() == value) {
                    immediate_variable.push_back(loadInst);
                }
            }
            errs() << *inst << "\n";
        }
    }
}

void DefUse::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<llvm::CallGraphWrapperPass>();
}

char DefUse::ID = 0;
static RegisterPass <DefUse> X("defuse", "This is def-use Pass");
