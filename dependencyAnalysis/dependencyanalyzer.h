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

enum analysis_type { DEPENDENCY_ANALYSIS, MAPPING_ANALYSIS, RECALCULATE_OFFSET, USAGE_ANALYSIS };

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
  typedef std::pair<Function *, Argument *> Parameter;

  typedef struct variable_wrapper {
    Value *variable;
    uint level;
  } VariableWrapper;

  typedef struct configurationInfo {
    std::string config_name;
    std::string type_name; // variable name for no struct member; type name for struct member
    long long bit;
    std::vector<int> offsetList;
  } ConfigInfo;

  void getAnalysisUsage(AnalysisUsage &Info) const override;
  bool getDependentConfiguration(Module &M);
  void getDependentConfig();
  void calculateSize(Module &M);
  bool recalculateOffset(Module &M);
  void getPrevConfig(std::string config_name, UsageInfo *usage);
  void getSuccConfig(std::string config_name, UsageInfo *usage);
  bool runOnModule(Module &M) override;
  bool parseConfigFile();
  std::string parseName(std::string *line);
  long long parseLong(std::string *line);
  int parseInteger(std::string *line);
  template<typename T>
  void storeVariableUse(std::string configuration, T *variable);
  template<typename T>
  bool isPointStructVariable(T *variable);
  VariableWrapper
  handleMemoryAcess(Instruction *inst, VariableWrapper var_wrapper);
  template<typename T>
  std::vector<Value *> getStructMember(T *variable, std::vector<int> offsetList);
  template<typename T>
  std::vector<Value *> getBitVariables(T *variable, long long bitvalue);
  template<typename T>
  bool findConfigIndex(T *variable, std::vector<int> *dst);
  template<typename T>
  void getConfigurationUsage(T *config, std::vector<int> configIndex);
  bool isStructMemeber(int index);
  Value *getDataRelatedVariable(Instruction *inst,
                                Value *value,
                                std::vector<Value *> visitedInstruction,
                                std::vector<Parameter> *visitedArgument,
                                std::map<Instruction *, Function *> *callerMap,
                                bool isInstance);
  void storeStructMemberUse(Value *value,
                            int64_t offset,
                            std::vector<Value *> *variables);
  bool usageAnalysis(Module &M);

  std::vector<int> sysvarOffsets;
  std::vector<ConfigInfo> configInfoList;
  std::map<std::string, std::vector<usage_info>> configUsages;
  std::map<Function *, std::map<std::string, std::vector<Instruction *>>> functionUsages;
  std::map<Function *, std::vector<CallerRecord>> callerGraph;
  std::map<Function *, std::vector<CallerRecord>> calleeGraph;
 public:
  static char ID; // Pass identification, replacement for typeid
  DependencyAnalyzer() : ModulePass(ID) {}
};

#endif //STATIC_ANALYZER_DEPENDENCYANALYZER_H
