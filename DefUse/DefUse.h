//
// Created by yigonghu on 11/29/19.
//
#include <fstream>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/CallGraph.h"
#include <set>

using namespace llvm;
#ifndef STATIC_ANALYZER_DEFUSE_H
#define STATIC_ANALYZER_DEFUSE_H

typedef struct configurationInfo {
    std::string configuration;
    std::string name; // variable name for no struct member; type name for struct member
    long long bit;
    std::vector<int> offsetList;
}ConfigInfo;

static std::string black_list[] = {
    {"_ZL13fix_log_stateP7sys_varP3THD13enum_var_type"},
};

static ConfigInfo configInfo[] = {
    {"empty type","empty type",-1,{}},
    {"autocommit","class.THD",524288,{14,72}},
    {"autocommit","struct.system_variables",524288,{72}},
    {"auto_increment_increment","class.THD",-1,{14,96}},
    {"auto_increment_increment","struct.system_variables",-1,{96}},
    {"auto_increment_offset","class.THD",-1,{14,104}},
    {"auto_increment_offset","struct.system_variables",-1,{104}},
    {"automatic_sp_privileges","sp_automatic_privileges",-1,{}},
    {"binlog_cache_size","binlog_cache_size",-1,{}},
    {"binlog_stmt_cache_size","binlog_stmt_cache_size",-1,{}},
    {"binlog_format","class.THD",-1,{14,392}},
    {"binlog_format","struct.system_variables",-1,{392}},
    {"binlog_direct_non_transactional_updates","class.THD",-1,{14,400}},
    {"binlog_direct_non_transactional_updates","struct.system_variables",-1,{400}},
    {"bulk_insert_buffer_size","class.THD",-1,{14,112}},
    {"bulk_insert_buffer_size","struct.system_variables",-1,{112}},
    {"completion_type","class.THD",-1,{14,408}},
    {"completion_type","struct.system_variables",-1,{408}},
    {"concurrent_insert","myisam_concurrent_insert",-1,{}},
    {"connect_timeout","connect_timeout",-1,{}},
    {"delay_key_write","delay_key_write_options",-1,{}},
    {"delayed_insert_limit","delayed_insert_limit",-1,{}},
    {"delayed_insert_timeout","delayed_insert_timeout",-1,{}},
    {"delayed_queue_size","delayed_queue_size",-1,{}},
    {"event_scheduler","_ZN6Events19opt_event_schedulerE",-1,{}},
    {"expire_logs_days","expire_logs_days",-1,{}},
    {"flush","myisam_flush",-1,{}},
    {"flush_time","flush_time",-1,{}},
    {"ft_boolean_syntax","ft_boolean_syntax",-1,{}},
    {"interactive_timeout","class.THD",-1,{14,208}},
    {"interactive_timeout","struct.system_variables",-1,{208}},
    {"join_buffer_size","class.THD",-1,{14,120}},
    {"join_buffer_size","struct.system_variables",-1,{120}},
    {"key_buffer_size","global_status_var",-1,{}},
    {"key_cache_block_size","global_status_var",-1,{}},
    {"key_cache_division_limit","global_status_var",-1,{}},
    {"key_cache_age_threshold","global_status_var",-1,{}},
    {"local_infile","opt_local_infile",-1,{}},
    {"lock_wait_timeout","class.THD",-1,{14,128}},
    {"lock_wait_timeout","struct.system_variables",-1,{128}},
    {"log_bin_trust_function_creators","trust_function_creators",-1,{}},
    {"log_queries_not_using_indexes","opt_log_queries_not_using_indexes",-1,{}},
    {"log_warnings","class.THD",-1,{14,376}},
    {"log_warnings","struct.system_variables",-1,{376}},
    {"long_query_time","class.THD",-1,{14,552}},
    {"long_query_time","struct.system_variables",-1,{552}},
    {"low_priority_updates","class.THD",-1,{14,456}},
    {"low_priority_updates","struct.system_variables",-1,{456}},
    {"sql_low_priority_updates","class.THD",-1,{14,456}},
    {"sql_low_priority_updates","struct.system_variables",-1,{456}},
    {"max_allowed_packet","class.THD",-1,{14,136}},
    {"max_allowed_packet","struct.system_variables",-1,{136}},
    {"slave_max_allowed_packet","slave_max_allowed_packet",-1,{}},
    {"max_binlog_cache_size","max_binlog_cache_size",-1,{}},
    {"max_binlog_stmt_cache_size","max_binlog_stmt_cache_size",-1,{}},
    {"max_binlog_size","max_binlog_size",-1,{}},
    {"max_connections","max_connections",-1,{}},
    {"max_connect_errors","max_connect_errors",-1,{}},
    {"max_insert_delayed_threads","class.THD",-1,{14,176}},
    {"max_insert_delayed_threads","struct.system_variables",-1,{176}},
    {"max_delayed_threads","class.THD",-1,{14,176}},
    {"max_delayed_threads","struct.system_variables",-1,{176}},
    {"max_error_count","class.THD",-1,{14,144}},
    {"max_error_count","struct.system_variables",-1,{144}},
    {"max_heap_table_size","class.THD",-1,{14,32}},
    {"max_heap_table_size","struct.system_variables",-1,{32}},
    {"max_join_size","class.THD",-1,{14,88}},
    {"max_join_size","struct.system_variables",-1,{88}},
    {"max_seeks_for_key","class.THD",-1,{14,328}},
    {"max_seeks_for_key","struct.system_variables",-1,{328}},
    {"max_length_for_sort_data","class.THD",-1,{14,152}},
    {"max_length_for_sort_data","struct.system_variables",-1,{152}},
    {"sql_max_join_size","class.THD",-1,{14,88}},
    {"sql_max_join_size","struct.system_variables",-1,{88}},
    {"max_prepared_stmt_count","max_prepared_stmt_count",-1,{}},
    {"max_relay_log_size","max_relay_log_size",-1,{}},
    {"max_sort_length","class.THD",-1,{14,160}},
    {"max_sort_length","struct.system_variables",-1,{160}},
    {"max_sp_recursion_depth","class.THD",-1,{14,312}},
    {"max_sp_recursion_depth","struct.system_variables",-1,{312}},
    {"max_user_connections","class.THD",-1,{14,440}},
    {"max_user_connections","struct.system_variables",-1,{440}},
    {"max_tmp_tables","class.THD",-1,{14,168}},
    {"max_tmp_tables","struct.system_variables",-1,{168}},
    {"max_write_lock_count","max_write_lock_count",-1,{}},
    {"min_examined_row_limit","class.THD",-1,{14,184}},
    {"min_examined_row_limit","struct.system_variables",-1,{184}},
    {"net_buffer_length","class.THD",-1,{14,200}},
    {"net_buffer_length","struct.system_variables",-1,{200}},
    {"net_read_timeout","class.THD",-1,{14,216}},
    {"net_read_timeout","struct.system_variables",-1,{216}},
    {"net_write_timeout","class.THD",-1,{14,240}},
    {"net_write_timeout","struct.system_variables",-1,{240}},
    {"net_retry_count","class.THD",-1,{14,224}},
    {"net_retry_count","struct.system_variables",-1,{224}},
    {"new","class.THD",-1,{14,457}},
    {"new","struct.system_variables",-1,{457}},
    {"old_alter_table","class.THD",-1,{14,461}},
    {"old_alter_table","struct.system_variables",-1,{461}},
    {"old_passwords","class.THD",-1,{14,462}},
    {"old_passwords","struct.system_variables",-1,{462}},
    {"optimizer_prune_level","class.THD",-1,{14,248}},
    {"optimizer_prune_level","struct.system_variables",-1,{248}},
    {"optimizer_search_depth","class.THD",-1,{14,256}},
    {"optimizer_search_depth","struct.system_variables",-1,{256}},
    {"optimizer_switch","class.THD",-1,{14,56}},
    {"optimizer_switch","struct.system_variables",-1,{56}},
    {"preload_buffer_size","class.THD",-1,{14,264}},
    {"preload_buffer_size","struct.system_variables",-1,{264}},
    {"read_buffer_size","class.THD",-1,{14,280}},
    {"read_buffer_size","struct.system_variables",-1,{280}},
    {"read_only","read_only",-1,{}},
    {"read_rnd_buffer_size","class.THD",-1,{14,288}},
    {"read_rnd_buffer_size","struct.system_variables",-1,{288}},
    {"div_precision_increment","class.THD",-1,{14,296}},
    {"div_precision_increment","struct.system_variables",-1,{296}},
    {"rpl_recovery_rank","rpl_recovery_rank",-1,{}},
    {"range_alloc_block_size","class.THD",-1,{14,336}},
    {"range_alloc_block_size","struct.system_variables",-1,{336}},
    {"multi_range_count","class.THD",-1,{14,192}},
    {"multi_range_count","struct.system_variables",-1,{192}},
    {"query_alloc_block_size","class.THD",-1,{14,344}},
    {"query_alloc_block_size","struct.system_variables",-1,{344}},
    {"query_prealloc_size","class.THD",-1,{14,352}},
    {"query_prealloc_size","struct.system_variables",-1,{352}},
    {"transaction_alloc_block_size","class.THD",-1,{14,360}},
    {"transaction_alloc_block_size","struct.system_variables",-1,{360}},
    {"transaction_prealloc_size","class.THD",-1,{14,368}},
    {"transaction_prealloc_size","struct.system_variables",-1,{368}},
    {"query_cache_size","query_cache_size",-1,{}},
    {"query_cache_limit","query_cache",-1,{}},
    {"query_cache_min_res_unit","query_cache_min_res_unit",-1,{}},
    {"query_cache_type","class.THD",-1,{14,416}},
    {"query_cache_type","struct.system_variables",-1,{416}},
    {"query_cache_wlock_invalidate","class.THD",-1,{14,458}},
    {"query_cache_wlock_invalidate","struct.system_variables",-1,{458}},
    {"secure_auth","opt_secure_auth",-1,{}},
    {"server_id","server_id",-1,{}},
    {"slave_compressed_protocol","opt_slave_compressed_protocol",-1,{}},
    {"slave_exec_mode","slave_exec_mode_options",-1,{}},
    {"slave_type_conversions","slave_type_conversions_options",-1,{}},
    {"slow_launch_time","slow_launch_time",-1,{}},
    {"sort_buffer_size","class.THD",-1,{14,304}},
    {"sort_buffer_size","struct.system_variables",-1,{304}},
    {"sql_mode","class.THD",-1,{14,64}},
    {"sql_mode","struct.system_variables",-1,{64}},
    {"updatable_views_with_limit","class.THD",-1,{14,432}},
    {"updatable_views_with_limit","struct.system_variables",-1,{432}},
    {"sync_frm","opt_sync_frm",-1,{}},
    {"table_definition_cache","table_def_size",-1,{}},
    {"table_open_cache","table_cache_size",-1,{}},
    {"thread_cache_size","thread_cache_size",-1,{}},
    {"tmp_table_size","class.THD",-1,{14,40}},
    {"tmp_table_size","struct.system_variables",-1,{40}},
    {"timed_mutexes","timed_mutexes",-1,{}},
    {"wait_timeout","class.THD",-1,{14,232}},
    {"wait_timeout","struct.system_variables",-1,{232}},
    {"engine_condition_pushdown","class.THD",-1,{14,459}},
    {"engine_condition_pushdown","struct.system_variables",-1,{459}},
    {"big_tables","class.THD",-1,{14,463}},
    {"big_tables","struct.system_variables",-1,{463}},
    {"sql_big_tables","class.THD",-1,{14,463}},
    {"sql_big_tables","struct.system_variables",-1,{463}},
    {"sql_big_selects","class.THD",512,{14,72}},
    {"sql_big_selects","struct.system_variables",512,{72}},
    {"sql_log_off","class.THD",1024,{14,72}},
    {"sql_log_off","struct.system_variables",1024,{72}},
    {"sql_log_bin","class.THD",-1,{14,401}},
    {"sql_log_bin","struct.system_variables",-1,{401}},
    {"sql_warnings","class.THD",8192,{14,72}},
    {"sql_warnings","struct.system_variables",8192,{72}},
    {"sql_notes","class.THD",2147483648,{14,72}},
    {"sql_notes","struct.system_variables",2147483648,{72}},
    {"sql_auto_is_null","class.THD",16384,{14,72}},
    {"sql_auto_is_null","struct.system_variables",16384,{72}},
    {"sql_safe_updates","class.THD",65536,{14,72}},
    {"sql_safe_updates","struct.system_variables",65536,{72}},
    {"sql_buffer_result","class.THD",131072,{14,72}},
    {"sql_buffer_result","struct.system_variables",131072,{72}},
    {"sql_quote_show_create","class.THD",2048,{14,72}},
    {"sql_quote_show_create","struct.system_variables",2048,{72}},
    {"foreign_key_checks","class.THD",67108864,{14,72}},
    {"foreign_key_checks","struct.system_variables",67108864,{72}},
    {"unique_checks","class.THD",134217728,{14,72}},
    {"unique_checks","struct.system_variables",134217728,{72}},
    {"profiling","class.THD",8589934592,{14,72}},
    {"profiling","struct.system_variables",8589934592,{72}},
    {"profiling_history_size","class.THD",-1,{14,272}},
    {"profiling_history_size","struct.system_variables",-1,{272}},
    {"sql_select_limit","class.THD",-1,{14,80}},
    {"sql_select_limit","struct.system_variables",-1,{80}},
    {"default_week_format","class.THD",-1,{14,320}},
    {"default_week_format","struct.system_variables",-1,{320}},
    {"group_concat_max_len","class.THD",-1,{14,384}},
    {"group_concat_max_len","struct.system_variables",-1,{384}},
    {"keep_files_on_create","class.THD",-1,{14,460}},
    {"keep_files_on_create","struct.system_variables",-1,{460}},
    {"general_log","opt_log",-1,{}},
    {"log","opt_log",-1,{}},
    {"slow_query_log","opt_slow_log",-1,{}},
    {"log_slow_queries","opt_slow_log",-1,{}},
    {"log_output","log_output_options",-1,{}},
    {"relay_log_purge","relay_log_purge",-1,{}},
    {"relay_log_recovery","relay_log_recovery",-1,{}},
    {"slave_net_timeout","slave_net_timeout",-1,{}},
    {"sql_slave_skip_counter","sql_slave_skip_counter",-1,{}},
    {"sync_relay_log","sync_relaylog_period",-1,{}},
    {"sync_relay_log_info","sync_relayloginfo_period",-1,{}},
    {"sync_binlog","sync_binlog_period",-1,{}},
    {"sync_master_info","sync_masterinfo_period",-1,{}},
    {"slave_transaction_retries","slave_trans_retries",-1,{}},
    {"stored_program_cache","stored_program_cache_size",-1,{}},
    {"innodb_additional_mem_pool_size","srv_additional_mem_pool_size",-1,{}},
    {"innodb_autoextend_increment","srv_autoextend_increment",-1,{}},
    {"innodb_buffer_pool_size","srv_buffer_pool_size",-1,{}},
    {"innodb_buffer_pool_instances","srv_buffer_pool_instances",-1,{}},
    {"innodb_checksums","srv_checksums",-1,{}},
    {"innodb_commit_concurrency","srv_commit_concurrency",-1,{}},
    {"innodb_concurrency_tickets","srv_concurrency_tickets",-1,{}},
    {"innodb_data_file_path","srv_data_file_path",-1,{}},
    {"innodb_doublewrite","srv_doublewrite",-1,{}},
    {"innodb_fast_shutdown","srv_fast_shutdown",-1,{}},
    {"innodb_read_io_threads","srv_read_io_threads",-1,{}},
    {"innodb_write_io_threads","srv_write_io_threads",-1,{}},
    {"innodb_file_per_table","srv_file_per_table",-1,{}},
    {"innodb_file_format","srv_file_format",-1,{}},
    {"innodb_file_format_check","srv_file_format_check",-1,{}},
    {"innodb_file_format_max","srv_file_format_max",-1,{}},
    {"innodb_flush_log_at_trx_commit","srv_flush_log_at_trx_commit",-1,{}},
    {"innodb_force_recovery","srv_force_recovery",-1,{}},
    {"innodb_large_prefix","srv_large_prefix",-1,{}},
    {"innodb_force_load_corrupted","srv_force_load_corrupted",-1,{}},
    {"innodb_locks_unsafe_for_binlog","srv_locks_unsafe_for_binlog",-1,{}},
    {"innodb_lock_wait_timeout","srv_lock_wait_timeout",-1,{}},
    {"innodb_log_buffer_size","srv_log_buffer_size",-1,{}},
    {"innodb_log_file_size","srv_log_file_size",-1,{}},
    {"innodb_log_files_in_group","srv_log_files_in_group",-1,{}},
    {"innodb_log_group_home_dir","srv_log_group_home_dir",-1,{}},
    {"innodb_max_dirty_pages_pct","srv_max_dirty_pages_pct",-1,{}},
    {"innodb_adaptive_flushing","srv_adaptive_flushing",-1,{}},
    {"innodb_max_purge_lag","srv_max_purge_lag",-1,{}},
    {"innodb_mirrored_log_groups","srv_mirrored_log_groups",-1,{}},
    {"innodb_old_blocks_pct","srv_old_blocks_pct",-1,{}},
    {"innodb_old_blocks_time","srv_old_blocks_time",-1,{}},
    {"innodb_open_files","srv_open_files",-1,{}},
    {"innodb_rollback_on_timeout","srv_rollback_on_timeout",-1,{}},
    {"innodb_stats_on_metadata","srv_stats_on_metadata",-1,{}},
    {"innodb_stats_sample_pages","srv_stats_sample_pages",-1,{}},
    {"innodb_adaptive_hash_index","srv_adaptive_hash_index",-1,{}},
    {"innodb_stats_method","srv_stats_method",-1,{}},
    {"innodb_replication_delay","srv_replication_delay",-1,{}},
    {"innodb_strict_mode","srv_strict_mode",-1,{}},
    {"innodb_support_xa","srv_support_xa",-1,{}},
    {"innodb_sync_spin_loops","srv_sync_spin_loops",-1,{}},
    {"innodb_spin_wait_delay","srv_spin_wait_delay",-1,{}},
    {"innodb_table_locks","srv_table_locks",-1,{}},
    {"innodb_thread_concurrency","srv_thread_concurrency",-1,{}},
    {"innodb_thread_sleep_delay","srv_thread_sleep_delay",-1,{}},
    {"innodb_autoinc_lock_mode","srv_autoinc_lock_mode",-1,{}},
    {"innodb_version","srv_version",-1,{}},
    {"innodb_use_sys_malloc","srv_use_sys_malloc",-1,{}},
    {"innodb_use_native_aio","srv_use_native_aio",-1,{}},
    {"innodb_change_buffering","srv_change_buffering",-1,{}},
    {"innodb_change_buffering_debug","srv_change_buffering_debug",-1,{}},
    {"innodb_random_read_ahead","srv_random_read_ahead",-1,{}},
    {"innodb_read_ahead_threshold","srv_read_ahead_threshold",-1,{}},
    {"innodb_io_capacity","srv_io_capacity",-1,{}},
    {"innodb_purge_threads","srv_purge_threads",-1,{}},
    {"innodb_purge_batch_size","srv_purge_batch_size",-1,{}},
    {"innodb_rollback_segments","srv_rollback_segments",-1,{}},
    {"innodb_trx_rseg_n_slots_debug","srv_trx_rseg_n_slots_debug",-1,{}},
    {"innodb_limit_optimistic_insert_debug","srv_limit_optimistic_insert_debug",-1,{}},
    {"innodb_trx_purge_view_update_only_debug","srv_trx_purge_view_update_only_debug",-1,{}},
    {"innodb_flush_method","srv_unix_file_flush_method",-1,{}},
};

class DefUse : public ModulePass {
    struct variable_wrapper {
        Value *variable;
        uint level;
    };


    typedef struct usage_info {
        Instruction* inst;
        std::set<std::string> prev_configurations;
        std::set<Function *> prev_functions;
        std::set<std::string> succ_configurations;
    }UsageInfo;

    typedef std::pair<Instruction*, Function*> CallerRecord;

    template<typename T>
    void storeVariableUse(std::string configuration, T *variable);
    template<typename T>
    bool isPointStructVariable(T *variable);
    void
    handleMemoryAcess(Instruction *inst, variable_wrapper *variable, std::vector<variable_wrapper> *immediate_variable);
    template<typename T>
    std::vector<Value *> getVariables(T *variable, std::vector<int>* offsetList);
    template<typename T>
    std::vector<Value *> getBitVariables(T *variable, long long bitvalue);
    bool runOnModule(Module &M) override;
    void getAnalysisUsage(AnalysisUsage &Info) const override;
    template<typename T>
    bool getConfigurationInfo(T *variable, std::vector<int>* dst);
    template<typename T>
    void handleVariableUse(T *variable);
    void recalculate_offset();

public:
    std::map<std::string,std::vector<usage_info>> configurationUsages;
    std::map<Function*, std::map<std::string,std::vector<Instruction*>>> functionUsages;
    std::map<Function*, std::vector<CallerRecord>> callerGraph;
    std::map<Function*, std::vector<CallerRecord>> calleeGraph;
    std::vector<int> sysvar_offsets;
    static char ID; // Pass identification, replacement for typeid
    DefUse() : ModulePass(ID) {}

};

#endif //STATIC_ANALYZER_DEFUSE_H
