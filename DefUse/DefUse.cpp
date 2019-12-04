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
#include "llvm/Support/FileSystem.h"
#include "DefUse.h"
#include <vector>
#include <list>
#include <queue>
#include <fstream>

using namespace llvm;


/*
 * Perform the static analysis to each module,
 */
bool DefUse::runOnModule(Module &M)  {
    std::string outName ("result.log");
    std::error_code OutErrorInfo;
    llvm::raw_fd_ostream outFile(llvm::StringRef(outName),OutErrorInfo, sys::fs::F_None);


    // If the variable is a global variable, get the variable usage
    for (auto &Global : M.getGlobalList())
        handleVariableUse(&Global);

    // If the variable is a local variable or an argument, get the variable usage
    for (Module::iterator function = M.begin(), moduleEnd = M.end();
         function != moduleEnd; function++) {
        for (auto arg = function->arg_begin(); arg != function->arg_end(); arg++)
                handleVariableUse(&(*arg));

        for (Function::iterator block = function->begin(), functionEnd = function->end();
             block != functionEnd; ++block) {
            for (BasicBlock::iterator instruction = block->begin(), blockEnd = block->end();
                 instruction != blockEnd; instruction++) {
                Instruction *inst = dyn_cast<Instruction>(instruction);
                llvm::ConstantInt* CI;
                CallInst *callInst;
                Function *calledFunction;
                handleVariableUse(inst);

                if (!isa<CallInst>(instruction))
                    continue;

                callInst = dyn_cast<CallInst>(instruction);
                calledFunction = callInst->getCalledFunction();

                if (!calledFunction)
                    continue;

                if (calledFunction->getName() == "thd_test_options" && (CI = dyn_cast<llvm::ConstantInt>(callInst->getArgOperand(1)))) {
                    uint64_t bit_value = getBitValue(configInfo[1].bitname);
                    if (CI->getSExtValue() & bit_value) {
                        std::map<std::string,std::vector<Value *>> confVariableMap;
                        std::vector<Value*> usagelist = confVariableMap[configInfo[1].configuration];
                        usagelist.push_back(callInst);
                        confVariableMap[configInfo[1].configuration] = usagelist;
                        for (auto configUsage: confVariableMap) {
                            for (auto variable:configUsage.second)
                                storeVariableUse(configUsage.first, variable);
                        }
                    }
                }
            }
        }
    }

    /*
     * Control flow analysis
     */
    CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
    for (CallGraph::iterator i = CG.begin(); i != CG.end();i++) {
        Function* nodeFunction = i->second->getFunction();
        if (!nodeFunction) {
            continue;
        }
        for(CallGraphNode::iterator j = i->second->begin(); j != i->second->end(); j++) {
            Function *func = j->second->getFunction();
            for(auto usageList =usage_map.begin(); usageList != usage_map.end(); usageList++) {
                for(struct usage_info &usage: usageList->second){
                    Function* function = usage.inst->getParent()->getParent();
                    if (func == function)
                        usage.callers.push_back(nodeFunction);
                }
            }
        }
    }

    for(auto usageList =usage_map.begin(); usageList != usage_map.end(); usageList++) {
        for(struct usage_info &usage: usageList->second){
            Function* function = usage.inst->getParent()->getParent();
            CallGraphNode *CGN = CG[function];
            for (CallGraphNode::iterator ti = CGN->begin(); ti != CGN->end(); ++ti) {
                if (Function *FI = ti->second->getFunction())
                    usage.callees.push_back(FI);
            }
        }
    }

    for(auto usageList:usage_map) {
        outFile << "Configuration " << usageList.first << " is used in \n";
        for(auto usage: usageList.second){
            outFile << "In function "<< usage.inst->getParent()->getParent()->getName()<<  "; " <<*usage.inst << "\n";

            for (auto callee:usage.callees ) {
                outFile << "callee funciton" << callee->getName()<< "\n";
            }
            for(auto caller:usage.callers)
                outFile << "caller function " << caller->getName() << "\n";
        }
    }
    outFile.close();
    return false;
}

template<typename T>
void DefUse::handleVariableUse(T *variable) {
    std::map<std::string,std::vector<Value *>> confVariableMap;
    std::vector<int> configurations;
    if (getConfigurationInfo(variable,&configurations)) {
        while(!configurations.empty()) {
            int i = configurations.back();
            configurations.pop_back();
            if (configInfo[i].bitname != "") {
                std::vector<Value *> options = getVariables(variable,&(configInfo[i].offsetList));
                for (auto option:options) {
                    uint64_t bit_value = getBitValue(configInfo[i].bitname);
                    std::vector<Value *> v = getBitVariables(option,bit_value);
                    std::vector<Value*> usagelist = confVariableMap[configInfo[i].configuration];
                    usagelist.insert(usagelist.end(), v.begin(), v.end());
                    confVariableMap[configInfo[i].configuration] = usagelist;
                }
            } else {
                std::vector<Value *> v = getVariables(variable,&(configInfo[i].offsetList));
                std::vector<Value*> usagelist = confVariableMap[configInfo[i].configuration];
                usagelist.insert(usagelist.end(), v.begin(), v.end());
                confVariableMap[configInfo[i].configuration] = usagelist;
            }
        }

        for (auto configUsage: confVariableMap)
            for (auto variable:configUsage.second)
                storeVariableUse(configUsage.first, variable);
    }
}

template<typename T>
bool DefUse::getConfigurationInfo(T *variable, std::vector<int>* dst) {
    int i = -1;
    if (dyn_cast<LoadInst>(variable)) {
        return false;
    }
    for (ConfigInfo cInfo: configInfo) {
        i++;
        if(!cInfo.offsetList.empty()) {
            if (!isPointStructVariable(variable))
                continue;

            StringRef structName = variable->getType()->getPointerElementType()->getStructName();
            if (structName == cInfo.variableOrType)
                dst->push_back(i);

            continue;
        }

        if (cInfo.variableOrType == variable->getName())
            dst->push_back(i);
    }

    if(dst->empty())
        return false;
    else
        return true;
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
 * Find all the target variable
 */
template<typename T>
std::vector<Value *> DefUse::getVariables(T *variable,std::vector<int>* offsetList) {
    std::vector<struct variable_wrapper> immediate_variable;
    std::vector<Value *> result;
    struct variable_wrapper init_variable;

    init_variable.variable = variable;
    init_variable.level = 0;
    immediate_variable.push_back(init_variable);

    while (!immediate_variable.empty()) {
        struct variable_wrapper value = immediate_variable.back();

        immediate_variable.pop_back();
        if (value.level == offsetList->size()) {
            result.push_back(value.variable);
            continue;
        }
        for (User *U : value.variable->users()) {
            if (Instruction *Inst = dyn_cast<Instruction>(U)) {
                handleMemoryAcess(Inst, &value, &immediate_variable);
                if (isa<GetElementPtrInst>(Inst)) {
                    GetElementPtrInst *getElementPtrInst = dyn_cast<GetElementPtrInst>(Inst);
                    ConstantInt *structOffset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2));
                    if (structOffset && structOffset->getValue() == (*offsetList)[value.level]) {
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
void DefUse::storeVariableUse(std::string configuration, T *variable) {
    std::vector<Value *> immediate_variable;
    immediate_variable.push_back(variable);


    while (!immediate_variable.empty()) {
        Value *value = immediate_variable.back();

        immediate_variable.pop_back();
//        std::vector<usage_info> usagelist = usage_map[configuration];
//        struct usage_info usage;
//        usage.inst = dyn_cast<Instruction>(value);
//        usagelist.push_back(usage);
//        usage_map[configuration] = usagelist;
        for (User *U : value->users()) {
            if (Instruction *inst = dyn_cast<Instruction>(U)) {
                if (StoreInst *storeInst = dyn_cast<StoreInst>(inst))
                    if (storeInst->getValueOperand() == value)
                        immediate_variable.push_back(storeInst->getPointerOperand());

                if (LoadInst *loadInst = dyn_cast<LoadInst>(inst))
                    if (loadInst->getPointerOperand() == value)
                        immediate_variable.push_back(loadInst);

                std::vector<usage_info> usagelist = usage_map[configuration];
                struct usage_info usage;
                usage.inst = inst;
                usagelist.push_back(usage);
                usage_map[configuration] = usagelist;
            }
        }
    }
}

void DefUse::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<llvm::CallGraphWrapperPass>();
}

char DefUse::ID = 0;
static RegisterPass <DefUse> X("defuse", "This is def-use Pass");
