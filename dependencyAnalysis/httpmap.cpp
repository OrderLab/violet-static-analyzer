//
// Created by yigonghu on 6/26/20.
//

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/TypeFinder.h"
#include "httpmap.h"
#include <vector>
#include <list>
#include <queue>
#include <fstream>

using namespace llvm;

bool HttpMap::runOnModule(Module &M) {

  for (Module::iterator function = M.begin(), moduleEnd = M.end();
       function != moduleEnd; function++) {
    for (auto funMap : functionList) {
      if (function->getName() == funMap.func_name) {
        VarInfo varInfo;
        for (Function::iterator block = function->begin(), functionEnd = function->end();
             block != functionEnd; ++block) {
          for (BasicBlock::iterator instruction = block->begin(), blockEnd = block->end();
               instruction != blockEnd; instruction++) {
            CallInst *callInst;
            if (!isa<CallInst>(instruction))
              continue;
            callInst = dyn_cast<CallInst>(instruction);
            if (callInst->getCalledFunction()->getName() != "may_s2e_make_symbolic")
              continue;
            Value *var = callInst->getArgOperand(0);
            if (isa<ConstantExpr>(var)) {
              ConstantExpr *constExpr = dyn_cast<ConstantExpr>(var);
              if (constExpr->getOpcode() == Instruction::BitCast) {
                Value *variable = constExpr->getOperand(0);
                varInfo.name = variable->getName();
              }
            } else {
//              errs() << "config " << funMap.config_name  << "\n";
              Instruction* inst = dyn_cast<Instruction>(var);
//              errs() << "inst " << *inst  << "\n";
              while(!isa<AllocaInst>(inst)) {
                Value* val;
                if (inst->getOpcode() == Instruction::BitCast) {
                  val = inst->getOperand(0);
                    if (isa<Instruction>(val))
                      inst = dyn_cast<Instruction>(val);
//                    errs() << *inst << "\n";
                } else if (inst->getOpcode() == Instruction::GetElementPtr){
                  val = inst->getOperand(0);
                  if (const PointerType *PTy = dyn_cast<PointerType>(val->getType())) {
                    if(StructType *STy = cast<StructType>(PTy->getElementType())) {
                      varInfo.name = STy->getName();

                      for(uint i = 2; i < inst->getNumOperands(); i++) {
                        val = inst->getOperand(i);
                        if (const ConstantInt *CI = dyn_cast<ConstantInt>(val)) {
                          if (CI->getBitWidth() <=32){
                            int64_t v = CI->getSExtValue();
                            varInfo.offsetList.push_back(v);
                          }
                        }
                      }
                    }
                  }
                  val = inst->getOperand(0);
                  if (isa<Instruction>(val))
                    inst = dyn_cast<Instruction>(val);
//                  errs() << *inst << "\n";
                } else if (inst->getOpcode() == Instruction::Load){
                  val = inst->getOperand(0);
                  if (isa<Instruction>(val))
                    inst = dyn_cast<Instruction>(val);
//                  errs() << *inst << "\n";
                }
              }
            }


//            CallInst* parseInst = getParseInstruction(b);

              if (varInfo.name != "") {
                errs() << "{\""<< funMap.config_name << "\",\"" << varInfo.name << "\",{";
                for(size_t i = 0; i < varInfo.offsetList.size(); i++) {
                  size_t offset = varInfo.offsetList[i];
                  errs() << offset;
                  if (i != varInfo.offsetList.size()-1)
                    errs() << ",";
                }
                errs() << "}},\n";
              }

          }
        }
      }
    }
  }
  return true;
}

std::string HttpMap::getCallArgName(Value *var) {
  if (isa<ConstantExpr>(var)) {
    ConstantExpr* constExpr = dyn_cast<ConstantExpr>(var);
    if (constExpr->isGEPWithNoNotionalOverIndexing()) {
      if (constExpr->getOpcode() == Instruction::GetElementPtr) {
        if (isa<GlobalVariable>(constExpr->getOperand(0))) {
          auto var = dyn_cast<GlobalVariable>(constExpr->getOperand(0));
          auto a5 = dyn_cast<ConstantDataArray>(var->getInitializer());
          if (a5) {
            auto a6 = a5->getAsString();
            return a6;
          }
        }
      }
    }
  }
  return "";
}

VarInfo HttpMap::getCallArgOffset(Value *var){
  VarInfo varInfo;
  if (isa<ConstantExpr>(var)) {
    ConstantExpr* constExpr = dyn_cast<ConstantExpr>(var);
    if (constExpr->isGEPWithNoNotionalOverIndexing()) {
      if (constExpr->getOpcode() == Instruction::GetElementPtr) {
        if (isa<GlobalVariable>(constExpr->getOperand(0))) {
          Value* variable;
          variable = constExpr->getOperand(0);

          if (const PointerType *PTy = dyn_cast<PointerType>(variable->getType())) {
            if(StructType *STy = cast<StructType>(PTy->getElementType())) {
              varInfo.name = STy->getName();
              for (uint i = 2; i < constExpr->getNumOperands(); i++) {
                variable = constExpr->getOperand(i);
                if (const ConstantInt *CI = dyn_cast<ConstantInt>(variable)) {
                  if (CI->getBitWidth() <= 32) {
                    int64_t val = CI->getSExtValue();
                    varInfo.offsetList.push_back(val);
                  }
                }
              }
            }
          }
        } else {
          Value* variable;
          variable = constExpr->getOperand(0);
          if (const PointerType *PTy = dyn_cast<PointerType>(variable->getType())) {
            if(StructType *STy = cast<StructType>(PTy->getElementType())) {
              varInfo.name = STy->getName();
              size_t index = varInfo.name.rfind(".");
              varInfo.name = varInfo.name.substr(0,index);

              for(uint i = 2; i < constExpr->getNumOperands(); i++) {
                variable = constExpr->getOperand(i);
                if (const ConstantInt *CI = dyn_cast<ConstantInt>(variable)) {
                  if (CI->getBitWidth() <=32){
                    int64_t val = CI->getSExtValue();
                    varInfo.offsetList.push_back(val);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return varInfo;
}

CallInst* HttpMap::getParseInstruction(BasicBlock *b) {
  succ_iterator PI = succ_begin(b);
  std::vector<BasicBlock *> succBlocks;
  std::vector<BasicBlock *> visitedBlocks;
  succBlocks.push_back(*(++PI));
  while(!succBlocks.empty()) {
    bool flag = false;
    BasicBlock *bb = succBlocks.back();
    succBlocks.pop_back();
    if (std::find(visitedBlocks.begin(), visitedBlocks.end(), bb) != visitedBlocks.end())
      continue;
    visitedBlocks.push_back(bb);
    CallInst *callInst;
    for(BasicBlock::iterator instruction = bb->begin(), blockEnd = bb->end();
        instruction != blockEnd; instruction++) {
      if (!isa<CallInst>(instruction))
        continue;
      callInst = dyn_cast<CallInst>(instruction);
      if(strstr(callInst->getCalledFunction()->getName().begin(),"parse_")) {
        if(callInst->getCalledFunction()->getName() == "_ZL14parse_obsoletePKc")
          continue;
        flag = true;
        continue;
      }
    }
    if(!flag){
      for (succ_iterator PI = succ_begin(bb), E = succ_end(bb); PI != E; ++PI) {
        succBlocks.push_back(*PI);
      }
    } else {
      return callInst;
    }
  }
  return NULL;
}

void HttpMap::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<llvm::CallGraphWrapperPass>();
}

char HttpMap::ID = 0;
static RegisterPass<HttpMap> X("map", "This is configuration map Pass");