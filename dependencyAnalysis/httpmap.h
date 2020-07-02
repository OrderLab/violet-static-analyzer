//
// Created by yigonghu on 6/26/20.
//

#ifndef STATIC_ANALYZER_HTTPMAP_H
#define STATIC_ANALYZER_HTTPMAP_H

#include <fstream>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/CallGraph.h"
#include <set>
using namespace llvm;

typedef struct variableInfo {
  std::string name; // variable name for no struct member; type name for struct member
  std::vector<size_t> offsetList;
}VarInfo;

typedef struct functionMap {
  std::string config_name;
  std::string func_name;
}FunctionMap;

static FunctionMap functionList[] = {
    {"AcceptPathInfo","set_accept_path_info"},
    {"AddDefaultCharset","set_add_default_charset"},
    {"allow","allow_cmd"},
    {"AllowEncodedSlashes","set_allow2f"},
    {"AllowOverride","set_override"},
    {"BufferedLogs","set_buffered_logs_on"},
    {"BufferSize","set_buffer_size"},
    {"CGIPassAuth","set_cgi_pass_auth"},
    {"deny","allow_cmd"},
    {"EnableMMAP","set_enable_mmap"},
    {"EnableSendfile","set_enable_sendfile"},
    {"ExtendedStatus","ap_set_extended_status"},
    {"HostnameLookups","set_hostname_lookups"},
    {"IncludeOptional","include_config"},
    {"KeepAlive","set_keep_alive"},
    {"KeepAliveTimeout","set_keep_alive_timeout"},
    {"LimitInternalRecursion","set_recursion_limit"},
    {"LimitRequestBody","set_limit_req_body"},
    {"LimitRequestFields","set_limit_req_fields"},
    {"LimitRequestFieldsize","set_limit_req_fieldsize"},
    {"LimitRequestLine","set_limit_req_line"},
    {"LimitXMLRequestBody","set_limit_xml_req_body"},
    {"LogLevel","set_loglevel"},
    {"MaxConnectionsPerChild","ap_mpm_set_max_requests"},
    {"MaxKeepAliveRequests","set_keep_alive_max"},
    {"MaxMemFree","ap_mpm_set_max_mem_free"},
    {"MaxRangeOverlaps","set_max_overlaps"},
    {"MaxRangeReversals","set_max_reversals"},
    {"MaxRanges","set_max_ranges"},
    {"MaxRequestsPerChild","ap_mpm_set_max_requests"},
    {"MaxSpareServers","set_max_free_servers"},
    {"MergeTrailers","set_merge_trailers"},
    {"MinSpareServers","set_min_free_servers"},
    {"order","order"},
    {"Protocol","set_protocol"},
    {"Protocols","set_protocols"},
    {"ProtocolsHonorOrder","set_protocols_honor_order"},
    {"RegexDefaultOptions","set_regex_default_options"},
    {"RegisterHttpMethod","set_http_method"},
    {"RLimitCPU","set_limit_cpu"},
    {"RLimitMEM","set_limit_mem"},
    {"RLimitNPROC","set_limit_nproc"},
    {"ScoreBoardFile","ap_set_scoreboard"},
    {"SeeRequestTail","ap_set_reqtail"},
    {"ServerSignature","set_signature_flag"},
    {"ServerTokens","set_serv_tokens"},
    {"StartServers","set_daemons_to_start"},
    {"ThreadStackSize","ap_mpm_set_thread_stacksize"},
    {"Timeout","set_timeout"},
    {"TraceEnable","set_trace_enable"},
    {"UseCanonicalName","set_use_canonical_name"},
};

class HttpMap : public ModulePass {
  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &Info) const override;
  std::string getCallArgName(Value *var);
  CallInst* getParseInstruction(BasicBlock *b);
  VarInfo getCallArgOffset(Value *var);
 public:


  static char ID; // Pass identification, replacement for typeid
  HttpMap() : ModulePass(ID) {}
};

#endif //STATIC_ANALYZER_HTTPMAP_H
