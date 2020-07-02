//
// The Violet project
//
// Created by yigonghu on 6/30/20.
//
// Copyright (c) 2020, Johns Hopkins University - Order Lab
//
//      All rights reserved.
//      Licensed under the Apache License, Version 2.0 (the "License");
#include <fstream>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/CallGraph.h"
#include <set>

using namespace llvm;

#ifndef STATIC_ANALYZER_DEPENDENCYANALYZER_H
#define STATIC_ANALYZER_DEPENDENCYANALYZER_H

enum analysis_type{DEPENDENCY,MAPPING,RECALCULATE_OFFSET};

static std::string black_list[] = {
    {"_ZL13fix_log_stateP7sys_varP3THD13enum_var_type"},
};


class DependencyAnalyzer : public ModulePass {
 private:
  typedef struct usage_info {
    Instruction *inst;
    std::set<std::string> prev_configurations;
    std::set<Function *> prev_functions;
    std::set<Function *> succ_functions;
    std::set<std::string> succ_configurations;
  } UsageInfo;

  typedef std::pair<Instruction *, Function *> CallerRecord;

  typedef struct variable_wrapper {
    Value *variable;
    uint level;
  }VariableWrapper;

  typedef struct configurationInfo {
    std::string config_name;
    std::string type_name; // variable name for no struct member; type name for struct member
    long long bit;
    std::vector<int> offsetList;
  } ConfigInfo;

  void getAnalysisUsage(AnalysisUsage &Info) const override;
  void calculateSize(Module &M);
  bool recalculateOffset(Module &M);
  bool runOnModule(Module &M) override;
  bool parseConfigFile();
  std::string parseName(std::string *line);
  long long parseLong(std::string *line);
  int parseInteger(std::string *line);
  template<typename T>
  void storeVariableUse(std::string configuration, T *variable);
  template<typename T>
  bool isPointStructVariable(T *variable);
  void
  handleMemoryAcess(Instruction *inst, VariableWrapper *variable, std::vector<VariableWrapper> *immediate_variable);
  template<typename T>
  std::vector<Value *> getVariables(T *variable, std::vector<int> *offsetList);
  template<typename T>
  std::vector<Value *> getBitVariables(T *variable, long long bitvalue);
  template<typename T>
  bool getConfigurationInfo(T *variable, std::vector<int> *dst);
  template<typename T>
  void handleVariableUse(T *variable);

  std::vector<int> sysvarOffsets;
  std::vector<ConfigInfo> configMap;

 public:
  std::map<std::string, std::vector<usage_info>> configurationUsages;
  std::map<Function *, std::map<std::string, std::vector<Instruction *>>> functionUsages;
  std::map<Function *, std::vector<CallerRecord>> callerGraph;
  std::map<Function *, std::vector<CallerRecord>> calleeGraph;


  static char ID; // Pass identification, replacement for typeid
  DependencyAnalyzer() : ModulePass(ID) {}
};

#endif //STATIC_ANALYZER_DEPENDENCYANALYZER_H
