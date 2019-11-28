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
#include <fstream>

using namespace llvm;


namespace {
    static cl::opt<std::string> gVariableName("v", cl::desc("Specify variable name"), cl::value_desc("variablename"));
//    static cl::list <int> OffsetList("offset", cl::desc("Specify the position of the memeber in a struct variable"));

    class DefUse : public ModulePass {
        template<typename T>
        void getVariableUse(T *variable);
        void printIndentation();

    public:
        static char ID; // Pass identification, replacement for typeid
        int mIndex = 0;
        int OffsetList[2];
        std::ofstream result_log;
        int indentation = 0;
        bool flag = false;

        DefUse() : ModulePass(ID) {}

        /*
         * Perform the static analysis to each module,
         */
        bool runOnModule(Module &M) override {
            //TODO: automatically bound the variable name to the offset
            OffsetList[0] = 14;
            OffsetList[1] = 10;
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

                if (function->getName() == "thd_test_options" && !flag) {
                    flag = true;
                    for (SymbolTableList<Argument>::iterator arg = function->arg_begin(); arg != function->arg_end(); arg++) {
                        if (arg->getName() == gVariableName) {
                            errs() << "It is a argument of Function \""
                                   << arg->getParent()->getName()
                                   << "\"\n";
                            errs() << "The definition is " << *arg << "\n";
                            getVariableUse(&(*arg));
                        }
                    }
                }

                for (Function::iterator block = function->begin(), functionEnd = function->end();
                     block != functionEnd; ++block) {
                    for (BasicBlock::iterator instruction = block->begin(), blockEnd = block->end();
                         instruction != blockEnd; instruction++) {
                        Instruction *inst = dyn_cast<Instruction>(instruction);
                        if (inst->getName() == gVariableName && !inst->user_empty()) {
                            errs() << "It is a local variable defined in Function \"" << function->getName()
                                   << "\"\n";
                            errs() << "The definition is" << *inst << "\n";
                            getVariableUse(inst);
                        }
                    }
                }
            }
            return false;
        }
    };

    /*
    * Find all the instructions that use the target variable. If the variable is assigned to other variable, also find the
    * usage of the other variable
    *
    * @param variable the target variable
    */
    template<typename T>
    void DefUse::getVariableUse(T* variable) {
        std::vector<Value::use_iterator> worklist;
        bool isStruct = false;
        bool isClass = false;

        if (variable->getType()->isPointerTy() && variable->getType()->getPointerElementType()->isStructTy()) {
            errs() << "Variable is a struct variable " << *variable <<"\n";
            isStruct = true;
        }

        if (variable->getType()->isPointerTy() && variable->getType()->getPointerElementType()->isPointerTy() &&
            variable->getType()->getPointerElementType()->getPointerElementType()->isStructTy()) {
            errs() << "Variable is a point variable that points to a struct" << *variable <<"\n";
            isClass = true;
        }
//                errs() << "Variable is used in instruction:\n";
        for (User *U : variable->users()) {
            if (Instruction *Inst = dyn_cast<Instruction>(U)) {
                errs() << *Inst << "\n";
                if (isStruct) {
                    if (StoreInst * storeInst = dyn_cast<StoreInst>(Inst)) {
                        if (storeInst->getValueOperand() == variable) {
                            getVariableUse(storeInst->getPointerOperand());
                        }
                    }
                    if (isa<GetElementPtrInst>(Inst)) {
                        GetElementPtrInst *getElementPtrInst = dyn_cast<GetElementPtrInst>(Inst);
                        Type *operandType = getElementPtrInst->getPointerOperand()->getType();
                        Type *pointValueType = operandType->getPointerElementType();
                        ConstantInt *structOffset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2));
                        if (operandType->isPointerTy() && pointValueType->isStructTy()) {
                            if (structOffset->getValue() == OffsetList[mIndex]) {
                                errs() << "The offset is " << OffsetList[mIndex] << "\n";
                                mIndex++;
                                printIndentation();
//                                    errs() git s<< *Inst << "\n";
                                getVariableUse(Inst);
                                errs() << "----------------\n";
                                mIndex--;
                            }
                        }
                    }
                } else if (isClass) {
                    if (StoreInst *storeInst = dyn_cast<StoreInst>(Inst)) {
                        if (storeInst->getValueOperand() == variable) {
                            getVariableUse(storeInst->getPointerOperand());
                        }
                    }

                    if (LoadInst *loadInst = dyn_cast<LoadInst>(Inst)) {
                        if (loadInst->getPointerOperand() == variable) {
                            getVariableUse(loadInst);
                        }
                    }

                    if (isa<GetElementPtrInst>(Inst)) {
                        GetElementPtrInst *getElementPtrInst = dyn_cast<GetElementPtrInst>(Inst);
                        Type *operandType = getElementPtrInst->getPointerOperand()->getType();
                        Type *pointValueType = operandType->getPointerElementType();
                        ConstantInt *structOffset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2));
                        if (operandType->isPointerTy() && pointValueType->isPointerTy() && pointValueType->getPointerElementType()->isPointerTy()) {
                            if (structOffset->getValue() == OffsetList[mIndex]) {
                                mIndex++;
                                printIndentation();
                                errs() << *Inst << "\n";
                                getVariableUse(Inst);
                                mIndex--;
                            }
                        }
                    }
                } else {
                    if (StoreInst *storeInst = dyn_cast<StoreInst>(Inst)) {
                        if (storeInst->getValueOperand() == variable) {
                            getVariableUse(storeInst->getPointerOperand());
                        }
                    }
                    if (LoadInst *loadInst = dyn_cast<LoadInst>(Inst)) {
                        if (loadInst->getPointerOperand() == variable) {
                            getVariableUse(loadInst);
                        }
                    }
                    errs() << *Inst << "\n";
                }
            }
        }
    }

    void DefUse::printIndentation() {
        for (int i = 0; i < indentation; i++) {
            errs() << "\t";
        }
    }

}

char DefUse::ID = 0;
static RegisterPass<DefUse> X("defuse", "This is def-use Pass");
