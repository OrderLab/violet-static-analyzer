//
// Created by yigonghu on 6/22/20.
//
#include <fstream>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/CallGraph.h"
#include <set>

using namespace llvm;

#ifndef STATIC_ANALYZER_SQUIDANALYZER_H
#define STATIC_ANALYZER_SQUIDANALYZER_H


typedef struct configurationInfo {
  std::string configuration;
  std::string name;
  std::vector<int> offsetList;
} ConfigInfo;

static std::string black_list[] = {
    {"_ZN11SquidConfigC2Ev"},
    {"_ZN11SquidConfigD2Ev"},
    {"_ZL10parse_linePc"},
    {"_ZL11dump_configP10StoreEntry"},
    {"_ZL8free_allv"},
};

static ConfigInfo configInfo[] = {
    {"empty type","empty type",{}},
    {"workers","class.SquidConfig",{103}},
    {"cpu_affinity_map","class.SquidConfig",{104}},
    {"shared_memory_locking","class.SquidConfig",{2}},
    {"hopeless_kid_revival_delay","class.SquidConfig",{14}},
    {"authenticate_cache_garbage_interval","class.SquidConfig",{36}},
    {"authenticate_ttl","class.SquidConfig",{37}},
    {"authenticate_ip_ttl","class.SquidConfig",{38}},
    {"external_acl_type","class.SquidConfig",{98}},
    {"acl","class.SquidConfig",{70}},
    {"proxy_protocol_access","class.SquidConfig",{71,20}},
    {"follow_x_forwarded_for","class.SquidConfig",{71,19}},
    {"acl_uses_indirect_client","class.SquidConfig",{65,41}},
    {"log_uses_indirect_client","class.SquidConfig",{65,43}},
    {"tproxy_uses_indirect_client","class.SquidConfig",{65,44}},
    {"spoof_client_ip","class.SquidConfig",{71,21}},
    {"http_access","class.SquidConfig",{71,0}},
    {"adapted_http_access","class.SquidConfig",{71,1}},
    {"http_access2","class.SquidConfig",{71,1}},
    {"http_reply_access","class.SquidConfig",{71,15}},
    {"icp_access","class.SquidConfig",{71,2}},
    {"htcp_access","class.SquidConfig",{71,17}},
    {"htcp_clr_access","class.SquidConfig",{71,18}},
    {"miss_access","class.SquidConfig",{71,3}},
    {"ident_lookup_access","class.Ident::IdentConfig",{0}},
    {"reply_body_max_size","class.SquidConfig",{20}},
    {"on_unsupported_protocol","class.SquidConfig",{71,22}},
    {"tcp_outgoing_tos","class.Ip::Qos::Config",{14}},
    {"tcp_outgoing_dscp","class.Ip::Qos::Config",{14}},
    {"tcp_outgoing_ds","class.Ip::Qos::Config",{14}},
    {"clientside_tos","class.Ip::Qos::Config",{15}},
    {"tcp_outgoing_address","class.SquidConfig",{71,16}},
    {"host_verify_strict","class.SquidConfig",{65,48}},
    {"client_dst_passthru","class.SquidConfig",{65,49}},
    {"cache_peer","class.SquidConfig",{56}},
    {"dead_peer_timeout","class.SquidConfig",{15,12}},
    {"forward_max_tries","class.SquidConfig",{68}},
    {"cache_mem","class.SquidConfig",{3}},
    {"maximum_object_size_in_memory","class.SquidConfig",{63,4}},
    {"memory_cache_shared","class.SquidConfig",{1}},
    {"memory_replacement_policy","class.SquidConfig",{7}},
    {"cache_replacement_policy","class.SquidConfig",{6}},
    {"minimum_object_size","class.SquidConfig",{63,3}},
    {"maximum_object_size","class.SquidConfig",{63,2}},
    {"cache_dir","class.SquidConfig",{75}},
    {"store_dir_select_algorithm","class.SquidConfig",{95}},
    {"max_open_disk_fds","class.SquidConfig",{84}},
    {"cache_swap_low","class.SquidConfig",{0,1}},
    {"cache_swap_high","class.SquidConfig",{0,0}},
    {"access_log","class.SquidConfig",{26,2}},
    {"cache_access_log","class.SquidConfig",{26,2}},
    {"icap_log","class.SquidConfig",{26,3}},
    {"logfile_daemon","class.Log::LogConfig",{0}},
    {"stats_collection","class.SquidConfig",{71,10}},
    {"cache_store_log","class.SquidConfig",{26,0}},
    {"cache_swap_state","class.SquidConfig",{26,1}},
    {"cache_swap_log","class.SquidConfig",{26,1}},
    {"logfile_rotate","class.SquidConfig",{26,4}},
    {"mime_table","class.SquidConfig",{44}},
    {"log_mime_hdrs","class.SquidConfig",{65,8}},
    {"pid_filename","class.SquidConfig",{42}},
    {"client_netmask","class.SquidConfig",{51,4}},
    {"strip_query_terms","class.SquidConfig",{65,20}},
    {"buffered_logs","class.SquidConfig",{65,6}},
    {"coredump_dir","class.SquidConfig",{92}},
    {"ftp_user","class.SquidConfig",{73,2}},
    {"ftp_passive","class.SquidConfig",{73,3}},
    {"ftp_epsv_all","class.SquidConfig",{73,4}},
    {"ftp_epsv","class.SquidConfig",{71,23}},
    {"ftp_eprt","class.SquidConfig",{73,6}},
    {"ftp_sanitycheck","class.SquidConfig",{73,7}},
    {"ftp_telnet_protocol","class.SquidConfig",{73,8}},
    {"diskd_program","class.SquidConfig",{33,3}},
    {"unlinkd_program","class.SquidConfig",{33,2}},
    {"url_rewrite_program","class.SquidConfig",{33,0}},
    {"redirect_program","class.SquidConfig",{33,0}},
    {"url_rewrite_host_header","class.SquidConfig",{65,17}},
    {"redirect_rewrites_host_header","class.SquidConfig",{65,17}},
    {"url_rewrite_access","class.SquidConfig",{71,13}},
    {"redirector_access","class.SquidConfig",{71,13}},
    {"url_rewrite_bypass","class.SquidConfig",{65,21}},
    {"redirector_bypass","class.SquidConfig",{65,21}},
    {"url_rewrite_extras","class.SquidConfig",{107}},
    {"url_rewrite_timeout","class.SquidConfig",{108}},
    {"store_id_program","class.SquidConfig",{33,1}},
    {"storeurl_rewrite_program","class.SquidConfig",{33,1}},
    {"store_id_extras","class.SquidConfig",{109}},
    {"store_id_access","class.SquidConfig",{71,14}},
    {"storeurl_rewrite_access","class.SquidConfig",{71,14}},
    {"store_id_bypass","class.SquidConfig",{65,22}},
    {"storeurl_rewrite_bypass","class.SquidConfig",{65,22}},
    {"cache","class.SquidConfig",{71,7}},
    {"no_cache","class.SquidConfig",{71,7}},
    {"send_hit","class.SquidConfig",{71,8}},
    {"store_miss","class.SquidConfig",{71,9}},
    {"max_stale","class.SquidConfig",{9}},
    {"refresh_pattern","class.SquidConfig",{74}},
    {"quick_abort_min","class.SquidConfig",{4,0}},
    {"quick_abort_max","class.SquidConfig",{4,2}},
    {"quick_abort_pct","class.SquidConfig",{4,1}},
    {"read_ahead_gap","class.SquidConfig",{5}},
    {"negative_ttl","class.SquidConfig",{8}},
    {"positive_dns_ttl","class.SquidConfig",{11}},
    {"negative_dns_ttl","class.SquidConfig",{10}},
    {"range_offset_limit","class.SquidConfig",{86}},
    {"minimum_expiry_time","class.SquidConfig",{97}},
    {"store_avg_object_size","class.SquidConfig",{63,1}},
    {"store_objects_per_bucket","class.SquidConfig",{63,0}},
    {"request_header_max_size","class.SquidConfig",{16}},
    {"reply_header_max_size","class.SquidConfig",{19}},
    {"request_body_max_size","class.SquidConfig",{17}},
    {"client_request_buffer_max_size","class.SquidConfig",{18}},
    {"broken_posts","class.SquidConfig",{71,12}},
    {"via","class.SquidConfig",{65,35}},
    {"vary_ignore_expire","class.SquidConfig",{65,27}},
    {"request_entities","class.SquidConfig",{65,29}},
    {"request_header_access","class.SquidConfig",{87}},
    {"reply_header_access","class.SquidConfig",{88}},
    {"request_header_replace","class.SquidConfig",{87}},
    {"header_replace","class.SquidConfig",{87}},
    {"reply_header_replace","class.SquidConfig",{88}},
    {"request_header_add","class.SquidConfig",{89}},
    {"reply_header_add","class.SquidConfig",{90}},
    {"note","class.SquidConfig",{91}},
    {"relaxed_header_parser","class.SquidConfig",{65,32}},
    {"collapsed_forwarding","class.SquidConfig",{65,40}},
    {"collapsed_forwarding_shared_entries_limit","class.SquidConfig",{66}},
    {"forward_timeout","class.SquidConfig",{15,4}},
    {"connect_timeout","class.SquidConfig",{15,3}},
    {"peer_connect_timeout","class.SquidConfig",{15,5}},
    {"read_timeout","class.SquidConfig",{15,0}},
    {"write_timeout","class.SquidConfig",{15,1}},
    {"request_timeout","class.SquidConfig",{15,6}},
    {"request_start_timeout","class.SquidConfig",{15,13}},
    {"client_idle_pconn_timeout","class.SquidConfig",{15,7}},
    {"persistent_request_timeout","class.SquidConfig",{15,7}},
    {"ftp_client_idle_timeout","class.SquidConfig",{15,9}},
    {"client_lifetime","class.SquidConfig",{15,2}},
    {"pconn_lifetime","class.SquidConfig",{15,10}},
    {"half_closed_clients","class.SquidConfig",{65,13}},
    {"server_idle_pconn_timeout","class.SquidConfig",{15,8}},
    {"pconn_timeout","class.SquidConfig",{15,8}},
    {"ident_timeout","class.Ident::IdentConfig",{1}},
    {"shutdown_lifetime","class.SquidConfig",{12}},
    {"cache_mgr","class.SquidConfig",{27}},
    {"mail_from","class.SquidConfig",{28}},
    {"mail_program","class.SquidConfig",{29}},
    {"cache_effective_user","class.SquidConfig",{30}},
    {"cache_effective_group","class.SquidConfig",{32}},
    {"httpd_suppress_version_string","class.SquidConfig",{65,38}},
    {"visible_hostname","class.SquidConfig",{46}},
    {"unique_hostname","class.SquidConfig",{47}},
    {"hostname_aliases","class.SquidConfig",{48}},
    {"umask","class.SquidConfig",{101}},
    {"announce_period","class.SquidConfig",{50,2}},
    {"announce_host","class.SquidConfig",{50,0}},
    {"announce_file","class.SquidConfig",{50,1}},
    {"announce_port","class.SquidConfig",{50,3}},
    {"httpd_accel_surrogate_id","class.SquidConfig",{39,0}},
    {"http_accel_surrogate_remote","class.SquidConfig",{65,28}},
    {"wccp_router","class.SquidConfig",{23,0}},
    {"wccp2_router","class.SquidConfig",{24,0}},
    {"wccp_version","class.SquidConfig",{23,2}},
    {"wccp2_rebuild_wait","class.SquidConfig",{24,6}},
    {"wccp2_forwarding_method","class.SquidConfig",{24,2}},
    {"wccp2_return_method","class.SquidConfig",{24,3}},
    {"wccp2_assignment_method","class.SquidConfig",{24,4}},
    {"wccp2_weight","class.SquidConfig",{24,5}},
    {"wccp_address","class.SquidConfig",{23,1}},
    {"wccp2_address","class.SquidConfig",{24,1}},
    {"client_persistent_connections","class.SquidConfig",{65,24}},
    {"server_persistent_connections","class.SquidConfig",{65,25}},
    {"persistent_connection_after_error","class.SquidConfig",{65,26}},
    {"detect_broken_pconn","class.SquidConfig",{65,30}},
    {"snmp_port","class.SquidConfig",{21,2}},
    {"snmp_access","class.SquidConfig",{71,11}},
    {"snmp_incoming_address","class.SquidConfig",{51,2}},
    {"snmp_outgoing_address","class.SquidConfig",{51,3}},
    {"icp_port","class.SquidConfig",{21,0}},
    {"udp_port","class.SquidConfig",{21,0}},
    {"htcp_port","class.SquidConfig",{21,1}},
    {"log_icp_queries","class.SquidConfig",{65,0}},
    {"udp_incoming_address","class.SquidConfig",{51,0}},
    {"udp_outgoing_address","class.SquidConfig",{51,1}},
    {"icp_hit_stale","class.SquidConfig",{65,5}},
    {"minimum_direct_hops","class.SquidConfig",{60}},
    {"minimum_direct_rtt","class.SquidConfig",{61}},
    {"netdb_low","class.SquidConfig",{64,1}},
    {"netdb_high","class.SquidConfig",{64,0}},
    {"netdb_ping_period","class.SquidConfig",{64,2}},
    {"query_icmp","class.SquidConfig",{65,4}},
    {"test_reachability","class.SquidConfig",{65,12}},
    {"icp_query_timeout","class.SquidConfig",{15,14}},
    {"maximum_icp_query_timeout","class.SquidConfig",{15,15}},
    {"minimum_icp_query_timeout","class.SquidConfig",{15,16}},
    {"background_ping_rate","class.SquidConfig",{13}},
    {"mcast_groups","class.SquidConfig",{54}},
    {"mcast_icp_query_timeout","class.SquidConfig",{15,17}},
    {"icon_directory","class.SquidConfig",{76,0}},
    {"global_internal_static","class.SquidConfig",{65,39}},
    {"short_icon_urls","class.SquidConfig",{76,1}},
    {"error_directory","class.SquidConfig",{77}},
    {"error_default_language","class.SquidConfig",{78}},
    {"error_log_languages","class.SquidConfig",{79}},
    {"err_page_stylesheet","class.SquidConfig",{80}},
    {"err_html_text","class.SquidConfig",{49}},
    {"email_err_data","class.SquidConfig",{65,37}},
    {"deny_info","class.SquidConfig",{72}},
    {"nonhierarchical_direct","class.SquidConfig",{65,19}},
    {"prefer_direct","class.SquidConfig",{65,18}},
    {"cache_miss_revalidate","class.SquidConfig",{65,36}},
    {"always_direct","class.SquidConfig",{71,5}},
    {"never_direct","class.SquidConfig",{71,4}},
    {"incoming_udp_average","class.SquidConfig",{83,1,0}},
    {"incoming_icp_average","class.SquidConfig",{83,1,0}},
    {"incoming_tcp_average","class.SquidConfig",{83,2,0}},
    {"incoming_http_average","class.SquidConfig",{83,2,0}},
    {"incoming_dns_average","class.SquidConfig",{83,0,0}},
    {"min_udp_poll_cnt","class.SquidConfig",{83,1,1}},
    {"min_icp_poll_cnt","class.SquidConfig",{83,1,1}},
    {"min_dns_poll_cnt","class.SquidConfig",{83,0,1}},
    {"min_tcp_poll_cnt","class.SquidConfig",{83,2,1}},
    {"min_http_poll_cnt","class.SquidConfig",{83,2,1}},
    {"accept_filter","class.SquidConfig",{100}},
    {"client_ip_max_connections","class.SquidConfig",{106}},
    {"tcp_recv_bufsize","class.SquidConfig",{52}},
    {"icap_enable","class.Adaptation::Icap::Config",{0,1}},
    {"icap_connect_timeout","class.Adaptation::Icap::Config",{5}},
    {"icap_io_timeout","class.Adaptation::Icap::Config",{6}},
    {"icap_service_revival_delay","class.Adaptation::Icap::Config",{0,4}},
    {"icap_preview_enable","class.Adaptation::Icap::Config",{2}},
    {"icap_preview_size","class.Adaptation::Icap::Config",{3}},
    {"icap_206_enable","class.Adaptation::Icap::Config",{4}},
    {"icap_default_options_ttl","class.Adaptation::Icap::Config",{1}},
    {"icap_persistent_connections","class.Adaptation::Icap::Config",{7}},
    {"icap_client_username_header","class.Adaptation::Icap::Config",{9}},
    {"icap_client_username_encode","class.Adaptation::Icap::Config",{10}},
    {"loadable_modules","class.SquidConfig",{105}},
    {"icap_retry","class.Adaptation::Icap::Config",{12}},
    {"icap_retry_limit","class.Adaptation::Icap::Config",{13}},
    {"check_hostnames","class.SquidConfig",{65,33}},
    {"allow_underscore","class.SquidConfig",{65,34}},
    {"dns_retransmit_interval","class.SquidConfig",{15,18}},
    {"dns_timeout","class.SquidConfig",{15,19}},
    {"dns_packet_max","class.SquidConfig",{110,1}},
    {"dns_defnames","class.SquidConfig",{65,1}},
    {"dns_multicast_local","class.SquidConfig",{65,50}},
    {"dns_nameservers","class.SquidConfig",{55}},
    {"hosts_file","class.SquidConfig",{45}},
    {"append_domain","class.SquidConfig",{40}},
    {"ignore_unknown_nameservers","class.SquidConfig",{65,23}},
    {"dns_v4_first","class.SquidConfig",{110,0}},
    {"ipcache_size","class.SquidConfig",{58,0}},
    {"ipcache_low","class.SquidConfig",{58,1}},
    {"ipcache_high","class.SquidConfig",{58,2}},
    {"fqdncache_size","class.SquidConfig",{59,0}},
    {"memory_pools","class.SquidConfig",{65,11}},
    {"memory_pools_limit","class.SquidConfig",{82,0}},
    {"cachemgr_passwd","class.SquidConfig",{62}},
    {"client_db","class.SquidConfig",{65,3}},
    {"refresh_all_ims","class.SquidConfig",{65,14}},
    {"reload_into_ims","class.SquidConfig",{65,15}},
    {"connect_retries","class.SquidConfig",{69}},
    {"retry_on_error","class.SquidConfig",{81,0}},
    {"as_whois_server","class.SquidConfig",{25}},
    {"offline_mode","class.SquidConfig",{65,16}},
    {"uri_whitespace","class.SquidConfig",{85}},
    {"chroot","class.SquidConfig",{93}},
    {"balance_on_multiple_ip","class.SquidConfig",{65,31}},
    {"pipeline_prefetch","class.SquidConfig",{67}},
    {"high_response_time_warning","class.SquidConfig",{94,0}},
    {"high_page_fault_warning","class.SquidConfig",{94,1}},
    {"sleep_after_fork","class.SquidConfig",{96}},
    {"eui_lookup","class.InstanceId",{0}},
    {"max_filedescriptors","class.SquidConfig",{102}},
    {"max_filedesc","class.SquidConfig",{102}},
    {"force_request_body_continuation","class.SquidConfig",{71,24}},
    {"server_pconn_for_nonretriable","class.SquidConfig",{71,25}},
};

class SquidAnalyzer : public ModulePass {
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
  SquidAnalyzer() : ModulePass(ID) {}

};

#endif //STATIC_ANALYZER_SQUIDANALYZER_H
