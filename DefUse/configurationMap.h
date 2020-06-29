//
// Created by yigonghu on 6/22/20.
//
#include <fstream>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/CallGraph.h"
#include <set>
using namespace llvm;

#ifndef STATIC_ANALYZER_CONFIGURATIONMAP_H
#define STATIC_ANALYZER_CONFIGURATIONMAP_H

typedef struct variableInfo {
  std::string name; // variable name for no struct member; type name for struct member
  std::vector<size_t> offsetList;
}VarInfo;

class ConfigurationMap : public ModulePass {
  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &Info) const override;
  std::string getCallArgName(Value *var);
  CallInst* getParseInstruction(BasicBlock *b);
  VarInfo getCallArgOffset(Value *var);
 public:


  static char ID; // Pass identification, replacement for typeid
  ConfigurationMap() : ModulePass(ID) {}
};

#endif //STATIC_ANALYZER_CONFIGURATIONMAP_H
