//
// Created by yigonghu on 6/22/20.
//
#include <fstream>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/CallGraph.h"
#include <set>

using namespace llvm;

#ifndef STATIC_ANALYZER_HTTPANALYZER_H
#define STATIC_ANALYZER_HTTPANALYZER_H

typedef struct configurationInfo {
  std::string configuration;
  std::string name;
} ConfigInfo;

static ConfigInfo configInfo[] = {
    {"empty type","empty type"},
    {"HostnameLookups","hostname_lookups"},
    {"allow",""}
};

class HttpAnalyzer : public ModulePass {
  typedef struct usage_info {
    Instruction* inst;
    std::set<std::string> prev_configurations;
    std::set<Function *> prev_functions;
    std::set<Function *> succ_functions;
    std::set<std::string> succ_configurations;
  } UsageInfo;

  typedef std::pair<Instruction*, Function*> CallerRecord;

  template<typename T>
  void storeVariableUse(std::string configuration, T *variable);

  template<typename T>
  std::vector<Value *> getVariables(T *variable);

  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &Info) const override;
  template<typename T>
  int getConfigurationInfo(T *variable);
  template<typename T>
  void handleVariableUse(T *variable);
  std::string getConfigurationName(std::string config_variable);

 public:
  std::map<std::string,std::vector<usage_info>> configurationUsages;
  std::map<Function*, std::map<std::string,std::vector<Instruction*>>> functionUsages;
  std::map<Function*, std::vector<CallerRecord>> callerGraph;
  std::map<Function*, std::vector<CallerRecord>> calleeGraph;
  std::vector<int> sysvar_offsets;
  static char ID; // Pass identification, replacement for typeid
  HttpAnalyzer() : ModulePass(ID) {}

};

#endif //STATIC_ANALYZER_HTTPANALYZER_H
