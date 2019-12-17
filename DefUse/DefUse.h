//
// Created by yigonghu on 11/29/19.
//
#include <fstream>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/CallGraph.h"
#include<set>

using namespace llvm;
#ifndef STATIC_ANALYZER_DEFUSE_H
#define STATIC_ANALYZER_DEFUSE_H

#define SELECT_DISTINCT         (1ULL << 0)     // SELECT, user
#define SELECT_STRAIGHT_JOIN    (1ULL << 1)     // SELECT, user
#define SELECT_DESCRIBE         (1ULL << 2)     // SELECT, user
#define SELECT_SMALL_RESULT     (1ULL << 3)     // SELECT, user
#define SELECT_BIG_RESULT       (1ULL << 4)     // SELECT, user
#define OPTION_FOUND_ROWS       (1ULL << 5)     // SELECT, user
#define OPTION_TO_QUERY_CACHE   (1ULL << 6)     // SELECT, user
#define SELECT_NO_JOIN_CACHE    (1ULL << 7)     // intern
/** always the opposite of OPTION_NOT_AUTOCOMMIT except when in fix_autocommit() */
#define OPTION_AUTOCOMMIT       (1ULL << 8)    // THD, user
#define OPTION_BIG_SELECTS      (1ULL << 9)     // THD, user
#define OPTION_LOG_OFF          (1ULL << 10)    // THD, user
#define OPTION_QUOTE_SHOW_CREATE (1ULL << 11)   // THD, user, unused
#define TMP_TABLE_ALL_COLUMNS   (1ULL << 12)    // SELECT, intern
#define OPTION_WARNINGS         (1ULL << 13)    // THD, user
#define OPTION_AUTO_IS_NULL     (1ULL << 14)    // THD, user, binlog
#define OPTION_FOUND_COMMENT    (1ULL << 15)    // SELECT, intern, parser
#define OPTION_SAFE_UPDATES     (1ULL << 16)    // THD, user
#define OPTION_BUFFER_RESULT    (1ULL << 17)    // SELECT, user
#define OPTION_BIN_LOG          (1ULL << 18)    // THD, user
#define OPTION_NOT_AUTOCOMMIT   (1ULL << 19)    // THD, user
#define OPTION_BEGIN            (1ULL << 20)    // THD, intern
#define OPTION_TABLE_LOCK       (1ULL << 21)    // THD, intern
#define OPTION_QUICK            (1ULL << 22)    // SELECT (for DELETE)
#define OPTION_KEEP_LOG         (1ULL << 23)    // THD, user

/* The following is used to detect a conflict with DISTINCT */
#define SELECT_ALL              (1ULL << 24)    // SELECT, user, parser
/** The following can be set when importing tables in a 'wrong order'
   to suppress foreign key checks */
#define OPTION_NO_FOREIGN_KEY_CHECKS    (1ULL << 26) // THD, user, binlog
/** The following speeds up inserts to InnoDB tables by suppressing unique
   key checks in some cases */
#define OPTION_RELAXED_UNIQUE_CHECKS    (1ULL << 27) // THD, user, binlog
#define SELECT_NO_UNLOCK                (1ULL << 28) // SELECT, intern
#define OPTION_SCHEMA_TABLE             (1ULL << 29) // SELECT, intern
/** Flag set if setup_tables already done */
#define OPTION_SETUP_TABLES_DONE        (1ULL << 30) // intern
/** If not set then the thread will ignore all warnings with level notes. */
#define OPTION_SQL_NOTES                (1ULL << 31) // THD, user
/**
  Force the used temporary table to be a MyISAM table (because we will use
  fulltext functions when reading from it.
*/
#define TMP_TABLE_FORCE_MYISAM          (1ULL << 32)
#define OPTION_PROFILING                (1ULL << 33)
/**
  Indicates that this is a HIGH_PRIORITY SELECT.
  Currently used only for printing of such selects.
  Type of locks to be acquired is specified directly.
*/
#define SELECT_HIGH_PRIORITY            (1ULL << 34)     // SELECT, user
/**
  Is set in slave SQL thread when there was an
  error on master, which, when is not reproducible
  on slave (i.e. the query succeeds on slave),
  is not terminal to the state of repliation,
  and should be ignored. The slave SQL thread,
  however, needs to rollback the effects of the
  succeeded statement to keep replication consistent.
*/
#define OPTION_MASTER_SQL_ERROR (1ULL << 35)

struct bitVariableInfo {
    const char *name;
    uint64_t bit;
};

typedef struct configurationInfo {
    std::string configuration;
    std::string variableOrType; // variable name for no struct member; type name for struct memeber
    int bit;
    std::vector<int> offsetList;
}ConfigInfo;

  /* Table of bit variable. */
static const struct bitVariableInfo handlerInfo[] ={
    {"select distinct", SELECT_DISTINCT},
    {"select straight join", SELECT_STRAIGHT_JOIN},
    {"select describe",SELECT_DESCRIBE},
    {"no_autocommit",OPTION_NOT_AUTOCOMMIT},
    {"autocommit",OPTION_AUTOCOMMIT}
};

static ConfigInfo configInfo[] = {
     {"","empty type",-1,{}},
     {"autocommit","class.THD",524288,{1488,72}},
     {"query_cache_type","class.THD",-1,{1488,416}},
     {"innodb_flush_log_at_trx_commit","flush_log_at_trx_commit",-1,{}},
     {"innodb_flush_method","srv_unix_file_flush_method",-1,{}}
};

class DefUse : public ModulePass {
    struct variable_wrapper {
        Value *variable;
        uint level;
    };

    typedef struct usage_info {
        Instruction* inst;
        std::set<std::string> relatedConfigurations;
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
    std::vector<Value *> getBitVariables(T *variable, uint64_t bitvalue);
    uint64_t getBitValue(std::string bit_name);
    bool runOnModule(Module &M) override;
    void getAnalysisUsage(AnalysisUsage &Info) const override;
    template<typename T>
    bool getConfigurationInfo(T *variable, std::vector<int>* dst);
    template<typename T>
    void handleVariableUse(T *variable);

public:
    std::map<std::string,std::vector<usage_info>> configurationUsages;
    std::map<Function*, std::map<std::string,std::vector<Instruction*>>> functionUsages;
    std::map<Function*, std::vector<CallerRecord>> callerGraph;
    static char ID; // Pass identification, replacement for typeid
    DefUse() : ModulePass(ID) {}

};

#endif //STATIC_ANALYZER_DEFUSE_H
