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
  std::vector<int> offsetList;
} ConfigInfo;

static std::string black_list[] = {
};

static ConfigInfo configInfo[] = {
    {"empty type","empty type",{}},
    {"MaxConnectionsPerChild","ap_max_requests_per_child",{}},
    {"MaxRequestsPerChild","ap_max_requests_per_child",{}},
    {"MaxMemFree","ap_max_mem_free",{}},
    {"ThreadStackSize","ap_thread_stacksize",{}},
    {"ScoreBoardFile","ap_scoreboard_fname",{}},
    {"ExtendedStatus","ap_extended_status",{}},
    {"SeeRequestTail","ap_mod_status_reqtail",{}},
    {"AddDefaultCharset","struct.core_dir_config",{12}},
    {"AcceptPathInfo","struct.core_dir_config",{27}},
    {"AllowOverride","struct.core_dir_config",{5}},
    {"EnableMMAP","struct.core_dir_config",{31}},
    {"EnableSendfile","struct.core_dir_config",{32}},
    {"Protocol","struct.core_server_config",{7}},
    {"HostnameLookups","struct.core_dir_config",{8}},
    {"ServerSignature","struct.core_dir_config",{19}},
    {"Timeout","struct.cmd_parms_struct",{15,11}},
    {"UseCanonicalName","struct.core_dir_config",{10}},
    {"ServerTokens","ap_server_tokens",{}},
    {"LimitRequestLine","struct.cmd_parms_struct",{23,11}},
    {"LimitRequestFieldsize","struct.cmd_parms_struct",{24,11}},
    {"LimitRequestFields","struct.cmd_parms_struct",{25,11}},
    {"LimitRequestBody","struct.core_dir_config",{17}},
    {"LimitXMLRequestBody","struct.core_dir_config",{18}},
    {"MaxRanges","struct.core_dir_config",{39}},
    {"MaxRangeOverlaps","struct.core_dir_config",{40}},
    {"MaxRangeReversals","struct.core_dir_config",{41}},
    {"RLimitCPU","struct.core_dir_config",{14}},
    {"RLimitMEM","struct.core_dir_config",{15}},
    {"RLimitNPROC","struct.core_dir_config",{16}},
    {"LimitInternalRecursion","struct.core_server_config",{6}},
    {"CGIPassAuth","struct.core_dir_config",{44}},
    {"AllowEncodedSlashes","struct.core_dir_config",{34}},
    {"TraceEnable","struct.core_server_config",{12}},
    {"MergeTrailers","struct.core_server_config",{13}},
    {"Protocols","struct.core_server_config",{14}},
    {"ProtocolsHonorOrder","struct.core_server_config",{15}},
    {"KeepAliveTimeout","struct.cmd_parms_struct",{16,11}},
    {"MaxKeepAliveRequests","struct.cmd_parms_struct",{17,11}},
    {"KeepAlive","struct.cmd_parms_struct",{18,11}},

};

class HttpAnalyzer : public ModulePass {
  struct variable_wrapper {
    Value *variable;
    uint level;
  };

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
  std::vector<Value *> getVariables(T *variable, std::vector<int> *offsetList);

  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &Info) const override;
  template<typename T>
  bool getConfigurationInfo(T *variable, std::vector<int>* dst);
  template<typename T>
  bool isPointStructVariable(T *variable);
  void
  handleMemoryAcess(Instruction *inst, variable_wrapper *variable, std::vector<variable_wrapper> *immediate_variable);
  template<typename T>
  void handleVariableUse(T *variable);


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
