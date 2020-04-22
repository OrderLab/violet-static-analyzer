//
// Created by yigonghu on 3/19/20.
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
#include "PostgresqlAnalyzer.h"
#include <vector>
#include <list>
#include <queue>
#include <fstream>

using namespace llvm;

/*
 * Perform the static analysis to each module,
 */
bool PostgresqlAnalyzer::runOnModule(Module &M) {
  std::string outName("postgresql_result.log");
  std::error_code OutErrorInfo;
  llvm::raw_fd_ostream outFile(llvm::StringRef(outName), OutErrorInfo, sys::fs::F_None);
  llvm::raw_fd_ostream outFile2(llvm::StringRef("result2.log"), OutErrorInfo, sys::fs::F_None);

  // If the variable is a global variable, get the variable usage

  for (auto &Global : M.getGlobalList()) {
    handleVariableUse(&Global);
  }

  // If the variable is a local variable or an argument, get the variable usage
  for (Module::iterator function = M.begin(), moduleEnd = M.end();
       function != moduleEnd; function++) {
    for (auto arg = function->arg_begin(); arg != function->arg_end(); arg++) {
      if (arg->getName() != "synchronous_commit")
        handleVariableUse(&(*arg));
    }

    for (Function::iterator block = function->begin(), functionEnd = function->end();
         block != functionEnd; ++block) {
      for (BasicBlock::iterator instruction = block->begin(), blockEnd = block->end();
           instruction != blockEnd; instruction++) {
        Instruction *inst = dyn_cast<Instruction>(instruction);
        if (inst->getName() != "synchronous_commit")
          handleVariableUse(inst);
      }
    }
  }


  /*
   * Control flow analysis
   */
  CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
  for (CallGraph::iterator node = CG.begin(); node != CG.end(); node++) {
    Function *nodeFunction = node->second->getFunction();
    if (!nodeFunction) {
      continue;
    }
    callerGraph[nodeFunction];
    calleeGraph[nodeFunction];
    for (CallGraphNode::iterator callRecord = node->second->begin(); callRecord != node->second->end(); callRecord++) {
      if (!callRecord->second->getFunction())
        continue;

      // create caller graph
      std::vector<CallerRecord> &callers = callerGraph[callRecord->second->getFunction()];
      std::pair<Instruction *, Function *> record;

      record.first = dyn_cast<Instruction>(callRecord->first);
      record.second = nodeFunction;
      callers.emplace_back(record);

      // Create callee graph
      std::vector<CallerRecord> &callees = calleeGraph[nodeFunction];
      CallerRecord callee_record;
      callee_record.first = dyn_cast<Instruction>(callRecord->first);
      callee_record.second = callRecord->second->getFunction();
      callees.emplace_back(callee_record);
    }
  }

  for (auto &usages: configurationUsages) {
    for (auto &usage:usages.second) {
      // Get the prev configurations
      std::vector<CallerRecord> callers;
      std::vector<Function *> visitedCallers;

      callers = callerGraph[usage.inst->getParent()->getParent()];
      callers.emplace_back(usage.inst, usage.inst->getParent()->getParent());
      while (!callers.empty()) {
        Function *f = callers.back().second;
        Instruction *callInst = callers.back().first;
        callers.pop_back();

        if (std::find(visitedCallers.begin(), visitedCallers.end(), f) != visitedCallers.end())
          continue;

        visitedCallers.push_back(f);
        callers.insert(callers.end(), callerGraph[f].begin(), callerGraph[f].end());
        std::map<std::string, std::vector<Instruction *>> confFunctionMap = functionUsages[f];
        if (!confFunctionMap.empty()) {
          for (auto conf: confFunctionMap)
            for (auto u:conf.second) {
              if (conf.first == usages.first)
                continue;
              PostDominatorTree *PostDT = new PostDominatorTree();
              PostDT->runOnFunction(*f);
              if (isa<CmpInst>(u) && !PostDT->dominates(callInst->getParent(), u->getParent())) {
                std::vector<BasicBlock *> prevBlocks;
                std::vector<BasicBlock *> visitedBlocks;
                prevBlocks.push_back(callInst->getParent());
                bool flag = true;
                while (!prevBlocks.empty() && flag) {
                  BasicBlock *predlock = prevBlocks.back();
                  prevBlocks.pop_back();
                  if (std::find(visitedBlocks.begin(), visitedBlocks.end(), predlock) != visitedBlocks.end())
                    continue;
                  visitedBlocks.push_back(predlock);
                  for (pred_iterator PI = pred_begin(predlock), E = pred_end(predlock); PI != E; ++PI) {
                    if (*PI != u->getParent()) {
                      if (!PostDT->dominates(callInst->getParent(), *PI))
                        continue;
                    }
                    prevBlocks.push_back(*PI);
                    if (*PI == u->getParent()) {
                      flag = false;
                      usage.prev_functions.insert(callInst->getParent()->getParent());
                      usage.prev_configurations.insert(conf.first);
                    }
                  }
                }
              }
            }
        }
      }


      // Get the succ configurations
      std::vector<CallerRecord> callees;
      std::vector<Function *> visitedCallees;
      callees = calleeGraph[usage.inst->getParent()->getParent()];
      while (!callees.empty()) {
        CallerRecord calleeRecord = callees.back();
        callees.pop_back();

        if (std::find(visitedCallees.begin(), visitedCallees.end(), calleeRecord.first->getParent()->getParent()) != visitedCallees.end())
          continue;

        PostDominatorTree *PostDT = new PostDominatorTree();
        PostDT->runOnFunction(*usage.inst->getParent()->getParent());
        if (isa<CmpInst>(usage.inst) && !PostDT->dominates(calleeRecord.first->getParent(), usage.inst->getParent())) {
          std::vector<BasicBlock *> prevBlocks;
          std::vector<BasicBlock *> visitedBlocks;
          prevBlocks.push_back(calleeRecord.first->getParent());
          bool flag = true;
          while (!prevBlocks.empty() && flag) {
            BasicBlock *predlock = prevBlocks.back();
            prevBlocks.pop_back();
            if (std::find(visitedBlocks.begin(), visitedBlocks.end(), predlock) != visitedBlocks.end())
              continue;
            visitedBlocks.push_back(predlock);
            for (pred_iterator PI = pred_begin(predlock), E = pred_end(predlock); PI != E; ++PI) {
              if (*PI != usage.inst->getParent()) {
                if (!PostDT->dominates(calleeRecord.first->getParent(), *PI))
                  continue;
              }
              prevBlocks.push_back(*PI);
              if (*PI == usage.inst->getParent()) {
                flag = false;
                Function *callee = calleeRecord.first->getParent()->getParent();
                usage.succ_functions.insert(callee);
                visitedCallees.push_back(calleeRecord.first->getParent()->getParent());
                std::map<std::string, std::vector<Instruction *>> confFunctionMap = functionUsages[callee];
//                callees.insert(callees.end(), calleeGraph[callee].begin(), calleeGraph[callee].end());
                if (!confFunctionMap.empty()) {
                  for (auto conf: confFunctionMap) {
                    if (conf.first != usages.first)
                      usage.succ_configurations.insert(conf.first);
                  }
                }
                continue;
              }
            }
          }
        }
      }

      Function *function = usage.inst->getParent()->getParent();
      std::map<std::string, std::vector<Instruction *>> confFunctionMap = functionUsages[function];
      if (!confFunctionMap.empty()) {
        for (auto conf: confFunctionMap)
          for (auto u:conf.second) {
            if (conf.first == usages.first)
              continue;
            PostDominatorTree *PostDT = new PostDominatorTree();
            PostDT->runOnFunction(*function);
            if (!PostDT->dominates(u->getParent(),usage.inst->getParent())) {
              std::vector<BasicBlock *> prevBlocks;
              std::vector<BasicBlock *> visitedBlocks;
              prevBlocks.push_back(u->getParent());
              bool flag = true;
              while (!prevBlocks.empty() && flag) {
                BasicBlock *predlock = prevBlocks.back();
                prevBlocks.pop_back();
                if (std::find(visitedBlocks.begin(), visitedBlocks.end(), predlock) != visitedBlocks.end())
                  continue;
                visitedBlocks.push_back(predlock);
                for (pred_iterator PI = pred_begin(predlock), E = pred_end(predlock); PI != E; ++PI) {
                  if (*PI != u->getParent()) {
                    if (!PostDT->dominates(u->getParent(), *PI))
                      continue;
                  }
                  prevBlocks.push_back(*PI);
                  if (*PI == u->getParent()) {
                    flag = false;
                    usage.succ_configurations.insert(conf.first);
                  }
                }
              }
            }
          }
      }
    }
  }

  for (auto usage_list:configurationUsages) {
    std::vector<std::string> visited_configuration;
//    outFile2 << "Configuration " << usage_list.first << " is used in \n";
    outFile << "{ configuration: " << getConfigurationName(usage_list.first) << ", prev configurations: [";
    for (auto usage: usage_list.second) {
//      outFile2 << "In function " << usage.inst->getParent()->getParent()->getName() << "; " << *usage.inst << "\n";
//      if (!usage.prev_configurations.empty())
//        outFile2 << "The related configurations are \n";
      for (std::string conf:usage.prev_configurations) {
//        outFile2 << conf << ",";
        if (std::find(visited_configuration.begin(), visited_configuration.end(), conf)
            == visited_configuration.end()) {
          outFile << getConfigurationName(conf) << ",";
          visited_configuration.push_back(conf);
        }
      }
//      outFile2 << "\n";
//      for (auto function:usage.prev_functions) {
//        outFile2 << function->getName() << ",";
//      }
    }
    outFile << "]}\n";
  }

  for (auto usage_list:configurationUsages) {
    std::vector<std::string> visited_configuration;
    outFile2 << "Configuration " << usage_list.first << " is used in \n";
    outFile << "{ configuration: " << getConfigurationName(usage_list.first) << ", succ configurations: [";
    for (auto usage: usage_list.second) {
      outFile2 << "In function " << usage.inst->getParent()->getParent()->getName() << "; " << *usage.inst << "\n";
      if (!usage.prev_configurations.empty())
        outFile2 << "The related configurations are \n";
      for (std::string conf:usage.succ_configurations) {
        outFile2 << getConfigurationName(conf) << ",";
        if (std::find(visited_configuration.begin(), visited_configuration.end(), conf)
            == visited_configuration.end()) {
          outFile << getConfigurationName(conf) << ",";
          visited_configuration.push_back(conf);
        }
      }
      outFile2 << "\n";
      for (auto function:usage.succ_functions) {
        outFile2 << function->getName() << ",";
      }
    }
    outFile << "]},\n";
  }

  outFile2.close();
  outFile.close();
  return false;

}


std::string PostgresqlAnalyzer::getConfigurationName(std::string config_variable) {
  for (ConfigInfo cInfo: configInfo) {
    if (cInfo.name == config_variable)
      return cInfo.configuration;
  }

  return NULL;
}

template<typename T>
void PostgresqlAnalyzer::handleVariableUse(T *variable) {
  if (getConfigurationInfo(variable)) {
    std::vector<Value *> variables = getVariables(variable);
    for (auto var: variables)
        storeVariableUse(variable->getName(), var);
  }
}

/*
 * Return ture for getting the configuration info
 */
template<typename T>
int PostgresqlAnalyzer::getConfigurationInfo(T *variable) {
  int i = -1;
  if (dyn_cast<LoadInst>(variable)) {
    return false;
  }
  for (ConfigInfo cInfo: configInfo) {
    i++;
    if (cInfo.name == variable->getName())
      return i;
  }

 return 0;
}

/*
 * Find all the target variable
 */
template<typename T>
std::vector<Value *> PostgresqlAnalyzer::getVariables(T *variable) {

  std::vector<Value *> result;
    for (User *U : variable->users()) {
      if (Instruction *Inst = dyn_cast<Instruction>(U)) {
        result.push_back(Inst);
//        errs() << "inst " << *Inst << "\n";
      }
    }

  return result;
}

/*
* Find all the instructions that use the target variable.
* @param variable the target variable
*/
template<typename T>
void PostgresqlAnalyzer::storeVariableUse(std::string configuration, T *variable) {
  std::vector<Value *> immediate_variable;
  std::vector<Value *> visited_variable;
  immediate_variable.push_back(variable);

  while (!immediate_variable.empty()) {
    Value *value = immediate_variable.back();
    visited_variable.push_back(value);
    immediate_variable.pop_back();
    for (User *U : value->users()) {
      if (Instruction *inst = dyn_cast<Instruction>(U)) {
        if (StoreInst *storeInst = dyn_cast<StoreInst>(inst))
          if (storeInst->getValueOperand() == value) {
            if(std::find(visited_variable.begin(),visited_variable.end(),storeInst->getPointerOperand())== visited_variable.end())
              immediate_variable.push_back(storeInst->getPointerOperand());
          }

        if (LoadInst *loadInst = dyn_cast<LoadInst>(inst))
          if (loadInst->getPointerOperand() == value) {
            if(std::find(visited_variable.begin(),visited_variable.end(),loadInst)== visited_variable.end())
              immediate_variable.push_back(loadInst);
          }

        std::vector<UsageInfo> usagelist = configurationUsages[configuration];
        UsageInfo usage;
        usage.inst = inst;
        usagelist.push_back(usage);
        configurationUsages[configuration] = usagelist;

        Function *function = inst->getParent()->getParent();
        std::map<std::string, std::vector<Instruction *>> &usages = functionUsages[function];
        std::vector<Instruction *> &configUsages = usages[configuration];
        configUsages.push_back(inst);
      }
    }
  }
}

void PostgresqlAnalyzer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<llvm::CallGraphWrapperPass>();
}

char PostgresqlAnalyzer::ID = 0;
static RegisterPass<PostgresqlAnalyzer> X("postgresql_config", "This is def-use Pass");
