#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include <vector>
#include <list>
#include <queue>
#include <fstream>

using namespace llvm;


namespace {
    static cl::opt <std::string> gVariableName("v", cl::desc("Specify variable name"), cl::value_desc("variablename"));
//    static cl::list <int> OffsetList("offset", cl::desc("Specify the position of the memeber in a struct variable"));

    class DefUse : public ModulePass {
        struct variable_wrapper {
            Value* variable;
            int level;
        };

        template<typename T>
        void getVariableUse(T *variable);
        template<typename T>
        bool isPointStructVariable(T *variable);
        void handleMemoryAcess(Instruction *inst, variable_wrapper *variable, std::vector<variable_wrapper>* immediate_variable);

        template<typename T>
        std::vector<Value *> getVariables(T *variable,std::vector<int> offsetList);

    public:
        static char ID; // Pass identification, replacement for typeid
        std::ofstream result_log;


        DefUse() : ModulePass(ID) {}

        /*
         * Perform the static analysis to each module,
         */
        bool runOnModule(Module &M) override {
            //TODO: automatically bound the variable name to the offset
            std::vector<int> offsetList;
            offsetList.push_back(14);
            offsetList.push_back(54);
            std::vector<Value *> variables;

            result_log.open("result.log");
            errs() << "We want to trace the uses of variable: " << gVariableName << "\n";

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

//                if (function->getName() == "thd_test_options") {
                    for (SymbolTableList<Argument>::iterator arg = function->arg_begin();
                         arg != function->arg_end(); arg++) {
                        if (arg->getName() == gVariableName) {
                            std::vector<Value *> v = getVariables(&(*arg),offsetList);
                            variables.insert(variables.end(), v.begin(), v.end());
                        }

                    }

                    for (Function::iterator block = function->begin(), functionEnd = function->end();
                         block != functionEnd; ++block) {
                        for (BasicBlock::iterator instruction = block->begin(), blockEnd = block->end();
                             instruction != blockEnd; instruction++) {
                            Instruction *inst = dyn_cast<Instruction>(instruction);
                            if (inst->getName() == gVariableName) {
                                std::vector<Value *> v  = getVariables(inst,offsetList);
                                variables.insert(variables.end(), v.begin(), v.end());
                            }
                        }
                    }

                    if (!variables.empty())
                        errs() << "In function " << function->getName() << ", the variable is used in instruction: \n";
                    for (auto variable: variables) {
                        getVariableUse(variable);
                    }

                    variables.clear();
                }
//            }

            return false;
        }



    };

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

        for (User *U : variable->users()) {
            if (Instruction * Inst = dyn_cast<Instruction>(U)) {
                errs() << *Inst << "\n";
            }
        }
    }
}

char DefUse::ID = 0;
static RegisterPass <DefUse> X("defuse", "This is def-use Pass");
