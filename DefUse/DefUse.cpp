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
#include "DefUse.h"
#include <vector>
#include <list>
#include <queue>
#include <fstream>

using namespace llvm;

/*
 * Perform the static analysis to each module,
 */
bool DefUse::runOnModule(Module &M) {
  std::string outName("result.log");
  std::error_code OutErrorInfo;
  llvm::raw_fd_ostream outFile(llvm::StringRef(outName), OutErrorInfo, sys::fs::F_None);
//  llvm::raw_fd_ostream outFile2(llvm::StringRef("result2.log"), OutErrorInfo, sys::fs::F_None);

  for (auto *sty : M.getIdentifiedStructTypes()) {
    if (sty->getName() == "struct.system_variables") {
      unsigned length;
      int rest = 64,flag = false;
      for(auto element : sty->elements()) {
        switch (element->getTypeID()) {
          case llvm::CompositeType::VoidTyID:
            sysvar_offsets.push_back(0);
            break;
          case llvm::CompositeType::DoubleTyID:
            if (flag) {
              int lastWidth = sysvar_offsets.back();
              lastWidth += rest/8;
              sysvar_offsets.pop_back();
              sysvar_offsets.push_back(lastWidth);
              rest = 64;
              flag = false;
            }
            sysvar_offsets.push_back(sizeof(long));
            break;
          case llvm::CompositeType::IntegerTyID:
            length = cast<IntegerType>(element)->getBitWidth();
            //align to 64 bit
            if (length != 64) {
              flag = true;
              if (rest < length)
                rest = 64;
              rest -= length;
              sysvar_offsets.push_back(length/8);
            } else {
              if (flag) {
                int lastWidth = sysvar_offsets.back();
                lastWidth += rest/8;
                sysvar_offsets.pop_back();
                sysvar_offsets.push_back(lastWidth);
                rest = 64;
                flag = false;
              }
              sysvar_offsets.push_back(length/8);

            }
            break;
          case llvm::CompositeType::PointerTyID:
            if (flag) {
              int lastWidth = sysvar_offsets.back();
              lastWidth += rest/8;
              sysvar_offsets.pop_back();
              sysvar_offsets.push_back(lastWidth);
              rest = 64;
              flag = false;
            }
            sysvar_offsets.push_back(sizeof(char *));
            break;
          default:
            sysvar_offsets.push_back(-1);
        }
      }
    }
  }
  int lastWidth = sysvar_offsets.back();
  if (lastWidth != 8) {
    lastWidth = 8;
    sysvar_offsets.pop_back();
    sysvar_offsets.push_back(lastWidth);
  }
  recalculate_offset();

  // If the variable is a global variable, get the variable usage

  for (auto &Global : M.getGlobalList()) {
    handleVariableUse(&Global);
  }

  // If the variable is a local variable or an argument, get the variable usage
  for (Module::iterator function = M.begin(), moduleEnd = M.end();
       function != moduleEnd; function++) {
    for (auto arg = function->arg_begin(); arg != function->arg_end(); arg++)
      handleVariableUse(&(*arg));

    for (Function::iterator block = function->begin(), functionEnd = function->end();
         block != functionEnd; ++block) {
      for (BasicBlock::iterator instruction = block->begin(), blockEnd = block->end();
           instruction != blockEnd; instruction++) {
        Instruction *inst = dyn_cast<Instruction>(instruction);
        llvm::ConstantInt *CI;
        CallInst *callInst;
        Function *calledFunction;
        handleVariableUse(inst);

        if (!isa<CallInst>(instruction))
          continue;

        callInst = dyn_cast<CallInst>(instruction);
        calledFunction = callInst->getCalledFunction();
        if (!calledFunction)
          continue;

        if (calledFunction->getName() == "thd_test_options"
            && (CI = dyn_cast<llvm::ConstantInt>(callInst->getArgOperand(1)))) {
          if (CI->getSExtValue() & configInfo[1].bit ) {;
            std::map<std::string, std::vector<Value *>> confVariableMap;
            std::vector<Value *> usagelist = confVariableMap[configInfo[1].configuration];
            usagelist.push_back(callInst);
            confVariableMap[configInfo[1].configuration] = usagelist;
            for (auto configUsage: confVariableMap) {
              for (auto variable:configUsage.second)
                storeVariableUse(configUsage.first, variable);
            }
          }
        }
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
      std::pair<Instruction *, Function *> callee_record;
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

        if(f->getName() == "_ZL13fix_log_stateP7sys_varP3THD13enum_var_type" || f->getName() == "_ZN6LOGGER20activate_log_handlerEP3THDj")
          continue;

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
        Function *f = callees.back().second;
        callees.pop_back();
        if (std::find(visitedCallees.begin(), visitedCallees.end(), f) != visitedCallees.end())
          continue;

        visitedCallees.push_back(f);
        std::map<std::string, std::vector<Instruction *>> confFunctionMap = functionUsages[f];
        callees.insert(callees.end(), calleeGraph[f].begin(), calleeGraph[f].end());
        if (!confFunctionMap.empty()) {
          for (auto conf: confFunctionMap) {
            if (conf.first != usages.first)
                 usage.succ_configurations.insert(conf.first);
          }

        }
      }
    }
  }


  for (auto usage_list:configurationUsages) {
    std::vector<std::string> visited_configuration;
//    outFile2 << "Configuration " << usage_list.first << " is used in \n";
    outFile << "{ configuration: " << usage_list.first << ", prev configurations: [";
    for (auto usage: usage_list.second) {
//      outFile2 << "In function " << usage.inst->getParent()->getParent()->getName() << "; " << *usage.inst << "\n";
//      if (!usage.prev_configurations.empty())
//        outFile2 << "The related configurations are \n";
//      for (std::string conf:usage.prev_configurations) {
//        outFile2 << conf << ",";
        if (std::find(visited_configuration.begin(),visited_configuration.end(),conf)== visited_configuration.end()) {
            outFile << conf<< ",";
            visited_configuration.push_back(conf);
        }
      }
//      outFile2 << "\n";
//      for (auto function:usage.prev_functions) {
//        outFile2 << function->getName() << ",";
//      }
//      outFile2 << "\n";
    }

    outFile << "]}\n";
  }

  for (auto usage_list:configurationUsages) {
    std::vector<std::string> visited_configuration;
    outFile << "{ configuration: " << usage_list.first << ", succ configurations: [";
    for (auto usage: usage_list.second) {
      for (std::string conf:usage.succ_configurations) {
        if (std::find(visited_configuration.begin(),visited_configuration.end(),conf)== visited_configuration.end()) {
          outFile << conf<< ",";
          visited_configuration.push_back(conf);
        }

      }
    }
    outFile << "]},\n";
  }
//  outFile2.close();
  outFile.close();
  return false;
}

void DefUse::recalculate_offset(){
  for (auto &config:configInfo){
    if (config.offsetList.size() <= 1)
      continue;
    int offset = config.offsetList.back();
    int index = 0;
    for(auto element:sysvar_offsets) {
     if (offset < element)
       break;
     index++;
     offset -= element;
    }
    config.offsetList.pop_back();
    config.offsetList.push_back(index);
  }
}


template<typename T>
void DefUse::handleVariableUse(T *variable) {
  std::map<std::string, std::vector<Value *>> confVariableMap;
  std::vector<int> configurations;
  if (getConfigurationInfo(variable, &configurations)) {
    while (!configurations.empty()) {
      int i = configurations.back();
      configurations.pop_back();
      if (configInfo[i].bit != -1) {
        std::vector<Value *> options = getVariables(variable, &(configInfo[i].offsetList));
        for (auto option:options) {
          std::vector<Value *> v = getBitVariables(option, configInfo[i].bit);
          std::vector<Value *> usagelist = confVariableMap[configInfo[i].configuration];
          usagelist.insert(usagelist.end(), v.begin(), v.end());
          confVariableMap[configInfo[i].configuration] = usagelist;
        }
      } else {
        std::vector<Value *> v = getVariables(variable, &(configInfo[i].offsetList));
        std::vector<Value *> usagelist = confVariableMap[configInfo[i].configuration];
        usagelist.insert(usagelist.end(), v.begin(), v.end());
        confVariableMap[configInfo[i].configuration] = usagelist;
      }
    }

    for (auto configUsage: confVariableMap)
      for (auto var:configUsage.second)
        storeVariableUse(configUsage.first, var);
  }
}

/*
 * Return ture for getting the configuration info
 */
template<typename T>
bool DefUse::getConfigurationInfo(T *variable, std::vector<int> *dst) {
  int i = -1;
  if (dyn_cast<LoadInst>(variable)) {
    return false;
  }
  for (ConfigInfo cInfo: configInfo) {
    i++;
    if (!cInfo.offsetList.empty()) {
      if (!isPointStructVariable(variable))
        continue;

      StringRef structName = variable->getType()->getPointerElementType()->getStructName();
      if (structName == cInfo.variableOrType)
        dst->push_back(i);

      continue;
    }


    if (cInfo.variableOrType == variable->getName())
      dst->push_back(i);
  }

  if (dst->empty())
    return false;
  else
    return true;
}

template<typename T>
bool DefUse::isPointStructVariable(T *variable) {
  Type *pointerElementTpye;

  if (!variable->getType()->isPointerTy())
    return variable->getType()->isStructTy();

  pointerElementTpye = variable->getType()->getPointerElementType();
  return pointerElementTpye->isStructTy();
}

/*
 * If the variable is load to other variable, also check the usage of the other variable
 */
void DefUse::handleMemoryAcess(Instruction *inst,
                               variable_wrapper *variable,
                               std::vector<variable_wrapper> *immediate_variable) {
  if (StoreInst *storeInst = dyn_cast<StoreInst>(inst)) {
    if (storeInst->getValueOperand() == variable->variable) {
      struct variable_wrapper v;
      v.variable = storeInst->getPointerOperand();
      v.level = variable->level;
      immediate_variable->push_back(v);
    }
  }

  if (LoadInst *loadInst = dyn_cast<LoadInst>(inst)) {
    if (loadInst->getPointerOperand() == variable->variable) {
      struct variable_wrapper v;
      v.variable = loadInst;
      v.level = variable->level;
      immediate_variable->push_back(v);
    }
  }
  return;
}

template<typename T>
std::vector<Value *> DefUse::getBitVariables(T *variable, long long bitvalue) {
  std::vector<Value *> immediate_variable;
  std::vector<Value *> result;
  immediate_variable.push_back(variable);

  while (!immediate_variable.empty()) {
    Value *value = immediate_variable.back();

    immediate_variable.pop_back();
    for (auto use: value->users()) {
      Instruction *inst_use = dyn_cast<Instruction>(use);
      if (StoreInst *storeInst = dyn_cast<StoreInst>(inst_use)) {
        if (storeInst->getValueOperand() == value) {
          immediate_variable.push_back(storeInst->getPointerOperand());
        }
      }

      if (LoadInst *loadInst = dyn_cast<LoadInst>(inst_use)) {
        if (loadInst->getPointerOperand() == value) {
          immediate_variable.push_back(loadInst);
        }
      }

      if (inst_use->getOpcode() == Instruction::And) {
        if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(inst_use->getOperand(0)))
          if ((CI->getZExtValue() & bitvalue) && (CI->getSExtValue() > 0))
            result.push_back(inst_use);

        if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(inst_use->getOperand(1)))
          if ((CI->getZExtValue() & bitvalue) && (CI->getSExtValue() > 0))
            result.push_back(inst_use);
      }
    }
  }
  return result;
}

/*
 * Find all the target variable
 */
template<typename T>
std::vector<Value *> DefUse::getVariables(T *variable, std::vector<int> *offsetList) {
  std::vector<struct variable_wrapper> immediate_variable;
  std::vector<Value *> result;
  struct variable_wrapper init_variable;

  init_variable.variable = variable;
  init_variable.level = 0;
  immediate_variable.push_back(init_variable);

  while (!immediate_variable.empty()) {
    struct variable_wrapper value = immediate_variable.back();

    immediate_variable.pop_back();
    if (value.level == offsetList->size()) {
      result.push_back(value.variable);
      continue;
    }
    for (User *U : value.variable->users()) {
      if (Instruction *Inst = dyn_cast<Instruction>(U)) {
        handleMemoryAcess(Inst, &value, &immediate_variable);
        if (isa<GetElementPtrInst>(Inst)) {
          GetElementPtrInst *getElementPtrInst = dyn_cast<GetElementPtrInst>(Inst);
          ConstantInt *structOffset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2));
          if (structOffset && structOffset->getValue() == (*offsetList)[value.level]) {
            struct variable_wrapper v;
            v.variable = Inst;
            v.level = value.level + 1;
            immediate_variable.push_back(v);
          }
        }
      }
    }
  }
  return result;
}

/*
* Find all the instructions that use the target variable.
* @param variable the target variable
*/
template<typename T>
void DefUse::storeVariableUse(std::string configuration, T *variable) {
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

void DefUse::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<llvm::CallGraphWrapperPass>();
}

char DefUse::ID = 0;
static RegisterPass<DefUse> X("defuse", "This is def-use Pass");
