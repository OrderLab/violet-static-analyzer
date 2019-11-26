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

template<typename T>
void getVariableUse(T *variable);

void getArgumentUse(Function *fn, int position);

void printIndentation();

template<typename T>
void getTaintVariableUse(Value *taintedVariable, StoreInst *startPoint, bool isTainted);

template<typename T>
void checkInstructionUse(Instruction *instruction, T *variable, bool isTainted);

namespace {
    static cl::opt<std::string> gVariableName("v", cl::desc("Specify variable name"), cl::value_desc("variablename"));
//    static cl::list <int> OffsetList("offset", cl::desc("Specify the position of the memeber in a struct variable"));

    int indentation = 0;
    std::list<Instruction *> taintedVariableList;

    struct DefUse : public ModulePass {
        static char ID; // Pass identification, replacement for typeid
        int mIndex = 0;
        int OffsetList[2];
        std::ofstream result_log;
        bool flag = false;

        DefUse() : ModulePass(ID) {}


        bool runOnModule(Module &M) override {
            OffsetList[0] = 14;
            OffsetList[1] = 10;
            result_log.open("result.log");
            errs() << "We want to trace the uses of variable: " << gVariableName << "\n";

            //check whether it is a global variable
            for (auto &Global : M.getGlobalList()) {
                if (Global.getName() == gVariableName) {
                    errs() << "It is a global variable " << Global.getName() << "\n";
                    getVariableUse(&Global);
                    return false;
                }
            }

            //check whether it is a common variable
            for (Module::iterator moduleInterator = M.begin(), moduleEnd = M.end();
                 moduleInterator != moduleEnd; moduleInterator++) {
                for (Function::iterator functionInterator = moduleInterator->begin(), functionEnd = moduleInterator->end();
                     functionInterator != functionEnd; ++functionInterator) {
                    for (BasicBlock::iterator blockInterator = functionInterator->begin(), blockEnd = functionInterator->end();
                         blockInterator != blockEnd; blockInterator++) {
                        Instruction *inst = dyn_cast<Instruction>(blockInterator);
                        if (inst->getName() == gVariableName) {
//                            errs() << "It is a local variable defined in Function \"" << moduleInterator->getName()
//                                   << "\"\n";
//                            errs() << "The definition is" << *inst << "\n";
                            getVariableUse(inst);
                        }

                        if (CallInst *callInst = dyn_cast<CallInst>(inst)) {
                            Function *callee = callInst->getCalledFunction();
                            if (!callee)
                                continue;
                            if (callee->getName() == "thd_test_options") {
                                errs() << "Yigong Hu\n";
                                flag = true;
                            }
                            for (auto arg = callee->arg_begin(); arg != callee->arg_end(); arg++) {
                                if (arg->getName() == gVariableName) {
//                                    errs() << "It is a argument of Function \""
//                                           << callInst->getCalledFunction()->getName()
//                                           << "\"\n";
//                                    errs() << "The definition is " << *arg << "\n";
                                    if (flag) {
                                        errs() << "the arg is " << *arg << "\n";
                                    }
                                    getVariableUse(arg);
                                }
                            }
                            flag = false;

                        }
                    }
                }
            }
            return false;
        }

        template<typename T>
        void getVariableUse(T variable) {
            std::vector<Value::use_iterator> worklist;
            bool isStruct = false;
            bool isClass = false;

            if (variable->getType()->isPointerTy() && variable->getType()->getPointerElementType()->isStructTy()) {
//                errs() << "Variable is a struct variable\n";
                isStruct = true;
            }

            if (variable->getType()->isPointerTy() && variable->getType()->getPointerElementType()->isPointerTy() &&
                variable->getType()->getPointerElementType()->getPointerElementType()->isStructTy()) {
//                errs() << "Variable is a class variable\n";
                isClass = true;
            }
//                errs() << "Variable is used in instruction:\n";
            for (User *U : variable->users()) {
                if (flag) {
                    errs() << "the usage is " << *U << "\n";
                }
                if (Instruction *Inst = dyn_cast<Instruction>(U)) {
                    if (isStruct) {
                        if (isa<GetElementPtrInst>(Inst)) {
                            GetElementPtrInst *getElementPtrInst = dyn_cast<GetElementPtrInst>(Inst);
                            Type *operandType = getElementPtrInst->getPointerOperand()->getType();
                            Type *pointValueType = operandType->getPointerElementType();
                            ConstantInt *structOffset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2));
                            if (operandType->isPointerTy() && pointValueType->isStructTy()) {
                                if (structOffset->getValue() == OffsetList[mIndex]) {
                                    mIndex++;
                                    printIndentation();
                                    errs() << "The definition is " << *Inst << "\n";
//                                    errs() << *Inst << "\n";
                                    getVariableUse(Inst);
                                    mIndex--;
                                }
                            }
                        }
                    } else if (isClass) {
                        if (flag) {
                            errs() << "the ins is " << *Inst << "\n";
                        }
                        if (isa<GetElementPtrInst>(Inst)) {
                            GetElementPtrInst *getElementPtrInst = dyn_cast<GetElementPtrInst>(Inst);
                            Type *operandType = getElementPtrInst->getPointerOperand()->getType();
                            Type *pointValueType = operandType->getPointerElementType();
                            ConstantInt *structOffset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2));
                            errs() << "Yigong Hu\n";
                            if (operandType->isPointerTy() && pointValueType->isPointerTy() && pointValueType->getPointerElementType()->isPointerTy()) {
                                if (structOffset->getValue() == OffsetList[mIndex]) {
                                    mIndex++;
                                    printIndentation();
                                    errs() << "The definition is " << *Inst << "\n";
//                                    errs() << *Inst << "\n";
                                    getVariableUse(Inst);
                                    mIndex--;
                                }
                            }
                        }

                    } else {
//                        errs() << "The definition is " << *Inst << "\n";
                        errs() << *Inst << "\n";
                    }

                }
            }


        }


        void printIndentation() {
            for (int i = 0; i < indentation; i++) {
                errs() << "\t";
            }
        }
    };
}

char DefUse::ID = 0;
static RegisterPass<DefUse> X("defuse", "This is def-use Pass");
