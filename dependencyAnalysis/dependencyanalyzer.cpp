//
// The Violet project
//
// Created by yigonghu on 6/30/20.
//
// Copyright (c) 2020, Johns Hopkins University - Order Lab
//
//      All rights reserved.
//      Licensed under the Apache License, Version 2.0 (the "License");

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

#include "dependencyanalyzer.h"
#include <vector>
#include <list>
#include <queue>
#include <fstream>
#include <cxxabi.h>
//#define CLASS_NAME(somePointer) ((const char *) cppDemangle(typeid(*somePointer).name()).get() )

using namespace llvm;
static cl::opt<int> gAnalysisType("t", cl::desc("Specify what type of analyze the user want"), cl::value_desc("1"));
static cl::opt<std::string>
    gExecutableName("e", cl::desc("Specify the executable name"), cl::value_desc("mysql"), cl::init("mysql"));
static cl::opt<std::string>
    gInput("i", cl::desc("Specify the iutput file name"), cl::init("mysql_config.log"), cl::init("mysql_config.log"));

/*
 * Perform the static analysis to each module,
 */
bool DependencyAnalyzer::runOnModule(Module &M) {

  switch (gAnalysisType) {
    case DEPENDENCY_ANALYSIS: {
      getDependentConfiguration(M);
      break;
    }
    case MAPPING_ANALYSIS: {
      break;
    }
    case RECALCULATE_OFFSET:
      if (!recalculateOffset(M))
        return false;
      break;
    case USAGE_ANALYSIS:
      usageAnalysis(M);
      break;
    default:errs() << "Unknown analyzer type " << gAnalysisType << "\n";
      return false;

  }
  return true;
}

bool DependencyAnalyzer::usageAnalysis(Module &M) {
  std::string outName = gExecutableName + "_usage.log";
  std::error_code OutErrorInfo;
  llvm::raw_fd_ostream outFile(llvm::StringRef(outName), OutErrorInfo, sys::fs::F_None);

  if (!parseConfigFile()) {
    return false;
  }

  // Search the global variable first, find the usage of configurations in global variable
  for (auto &Global : M.getGlobalList()) {
    std::vector<int> config_index;
    if (getConfigIndex(&(Global), &config_index)) {
//      errs() << "the global variable " << Global <<"\n";
      getConfigurationUsage(&Global, config_index);
    }
  }

  // Find the configuration usage for local variable or argument
  for (Module::iterator func_iterator = M.begin(), moduleEnd = M.end();
       func_iterator != moduleEnd; func_iterator++) {

    for (auto arg = func_iterator->arg_begin(); arg != func_iterator->arg_end(); arg++) {
      std::vector<int> config_index;
      if (getConfigIndex(&(*arg), &config_index)) {
        getConfigurationUsage(&(*arg),config_index);
      }
    }

    for (Function::iterator block_iterator = func_iterator->begin(), functionEnd = func_iterator->end();
         block_iterator != functionEnd; ++block_iterator) {
      for (BasicBlock::iterator inst_iterator = block_iterator->begin(), blockEnd = block_iterator->end();
           inst_iterator != blockEnd; inst_iterator++) {
        Instruction *inst = dyn_cast<Instruction>(inst_iterator);
        std::vector<int> config_index;
        if (getConfigIndex(&(*inst), &config_index)) {
          getConfigurationUsage(inst,config_index);
        }

        if (CallInst* callInst = dyn_cast<CallInst>(inst)) {
          llvm::ConstantInt *CI;
          Function *caller = callInst->getCalledFunction();
          if (!caller)
            continue;

          if (caller->getName() == "thd_test_options") {
            if(CI = dyn_cast<llvm::ConstantInt>(callInst->getArgOperand(1))){
              if (int index = getBitIndex(CI->getSExtValue())) {
                if(index != -1) {
                  storeVariableUse(configInfoList[index].config_name, callInst);
                }
              }
            }
          }
        }
      }
    }
  }

  relatedConfigurationAnalyze();

  for (auto usesInfo:configUsages) {
    std::vector<Function*> visited_functions;
    outFile << "{ configuration: " << usesInfo.first << ",[";
    for (auto useInfo: usesInfo.second) {
      Function *func = useInfo.inst->getParent()->getParent();
      if (!func)
        continue;
      if (std::find(visited_functions.begin(), visited_functions.end(), func)
          == visited_functions.end()) {
        char *function_name = cppDemangle(func->getName().data());
        if (function_name) {
          outFile << function_name << ";";
        } else {
          outFile << func->getName() << ";";
        }
        visited_functions.push_back(func);

        for (Function * prev_func :useInfo.prev_functions) {
          if (std::find(visited_functions.begin(), visited_functions.end(), prev_func)
              == visited_functions.end()) {
            char *function_name = cppDemangle(prev_func->getName().data());
            if (function_name) {
              outFile << function_name << ";";
            } else {
              outFile << prev_func->getName() << ";";
            }
            visited_functions.push_back(func);
          }
        }
      }
    }
    outFile << "]}\n";
  }

  return true;

}

bool DependencyAnalyzer::getDependentConfiguration(Module &M) {
  std::string outName = gExecutableName + "_result.log";
  std::error_code OutErrorInfo;
  llvm::raw_fd_ostream outFile(llvm::StringRef(outName), OutErrorInfo, sys::fs::F_None);

  if (!parseConfigFile()) {
    return false;
  }

  // Search the global variable first, find the usage of configurations in global variable
  for (auto &Global : M.getGlobalList()) {
    std::vector<int> config_index;
    if (getConfigIndex(&(Global), &config_index)) {
      getConfigurationUsage(&Global, config_index);
    }
  }
  errs() << "finish the global\n";
  // Find the configuration usage for local variable or argument
  for (Module::iterator func_iterator = M.begin(), moduleEnd = M.end();
       func_iterator != moduleEnd; func_iterator++) {
    for (auto arg = func_iterator->arg_begin(); arg != func_iterator->arg_end(); arg++) {
      std::vector<int> config_index;
      if (getConfigIndex(&(*arg), &config_index)) {
        getConfigurationUsage(&(*arg),config_index);
      }
    }

    for (Function::iterator block_iterator = func_iterator->begin(), functionEnd = func_iterator->end();
         block_iterator != functionEnd; ++block_iterator) {
      for (BasicBlock::iterator inst_iterator = block_iterator->begin(), blockEnd = block_iterator->end();
           inst_iterator != blockEnd; inst_iterator++) {
        Instruction *inst = dyn_cast<Instruction>(inst_iterator);
        std::vector<int> config_index;
        if (getConfigIndex(&(*inst), &config_index)) {
          getConfigurationUsage(inst,config_index);
        }
        if (CallInst* callInst = dyn_cast<CallInst>(inst)) {
          llvm::ConstantInt *CI;
          Function *caller = callInst->getCalledFunction();
          if (!caller)
            continue;

          if (caller->getName() == "thd_test_options") {
            if(CI = dyn_cast<llvm::ConstantInt>(callInst->getArgOperand(1))){
              if (int index = getBitIndex(CI->getSExtValue())) {
                if(index != -1) {
                  storeVariableUse(configInfoList[index].config_name, callInst);
                }
              }
            }
          }
        }
      }
    }
  }
  errs() << "start to caclulate the related config\n";
  relatedConfigurationAnalyze();
  errs() << "finish to caclulate the related config\n";
//  //Log the related configuration result
  for (auto usesInfo:configUsages) {
    std::vector<std::string> visited_configuration;
    outFile << "{ configuration: " << usesInfo.first << ", prev configurations: [";
    for (auto useInfo: usesInfo.second) {
      for (std::string conf:useInfo.prev_configurations) {
        if (std::find(visited_configuration.begin(), visited_configuration.end(), conf)
            == visited_configuration.end()) {
          outFile << conf << ",";
          visited_configuration.push_back(conf);
        }
      }
    }
    outFile << "]}\n";
  }

  for (auto usesInfo:configUsages) {
    std::vector<std::string> visited_configuration;
    outFile << "{ configuration: " << usesInfo.first << ", succ configurations: [";
    for (auto useInfo: usesInfo.second) {
      for (std::string conf:useInfo.succ_configurations) {
        if (std::find(visited_configuration.begin(), visited_configuration.end(), conf)
            == visited_configuration.end()) {
          outFile << conf << ",";
          visited_configuration.push_back(conf);
        }
      }
    }
    outFile << "]},\n";
  }
  outFile.close();

  return true;
}

void DependencyAnalyzer::relatedConfigurationAnalyze() {
  std::string outName = gExecutableName + "_result.log";
  std::error_code OutErrorInfo;
  llvm::raw_fd_ostream outFile(llvm::StringRef(outName), OutErrorInfo, sys::fs::F_None);
  CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  // Get the Control flow of the executable
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

  for (auto &usages: configUsages) {
    for (UsageInfo &usage:usages.second) {
      getPrevConfig(usages.first, &usage);
      getSuccConfig(usages.first, &usage);
    }
  }
}

template<typename T>
void DependencyAnalyzer::getConfigurationUsage(T *config, std::vector<int> configIndex) {
  std::map<std::string, std::vector<Value *>> related_variables;

  /** Handle the struct and bit variable**/
  for (int index:configIndex) {
    if (configInfoList[index].bit != -1) {
      std::vector<Value *> options;
      if (isStructMemeber(index))
        options.push_back(config);
      else
        options = getStructMember(config, configInfoList[index].offsetList);
      for (auto option:options) {
        std::vector<Value *> v = getBitVariables(option, configInfoList[index].bit);
        std::vector<Value *> usagelist = related_variables[configInfoList[index].config_name];
        usagelist.insert(usagelist.end(), v.begin(), v.end());
        related_variables[configInfoList[index].config_name] = usagelist;
      }
    } else {
      std::vector<Value *> v;
      if (isStructMemeber(index))
        v.push_back(config);
      else
        v = getStructMember(config, configInfoList[index].offsetList);
      std::vector<Value *> usagelist = related_variables[configInfoList[index].config_name];
      usagelist.insert(usagelist.end(), v.begin(), v.end());
      related_variables[configInfoList[index].config_name] = usagelist;
    }
  }

  for (auto variables: related_variables)
    for (auto var:variables.second) {
      storeVariableUse(variables.first, var);
    }
}

/** Check whether the configuration is a struct member **/
bool DependencyAnalyzer::isStructMemeber(int index) {
  return configInfoList[index].offsetList.empty();
}

char *DependencyAnalyzer::cppDemangle(const char *abiName)
{
  int status;
  char *ret = abi::__cxa_demangle(abiName, 0, 0, &status);
  return ret;
}

void DependencyAnalyzer::getPrevConfig(std::string config_name, UsageInfo *usage) {
  std::vector<CallerRecord> callers;
  std::vector<Function *> visitedCallers;

  callers = callerGraph[usage->inst->getParent()->getParent()];
  callers.emplace_back(usage->inst, usage->inst->getParent()->getParent());
  while (!callers.empty()) {
    Function *f = callers.back().second;
    Instruction *callInst = callers.back().first;
    callers.pop_back();
    if (f->getName() == "_ZL13fix_log_stateP7sys_varP3THD13enum_var_type"
        || f->getName() == "_ZN6LOGGER20activate_log_handlerEP3THDj")
      continue;

    if (std::find(visitedCallers.begin(), visitedCallers.end(), f) != visitedCallers.end())
      continue;
    visitedCallers.push_back(f);
    callers.insert(callers.end(), callerGraph[f].begin(), callerGraph[f].end());
    std::map<std::string, std::vector<Instruction *>> confFunctionMap = functionUsages[f];
    if (!confFunctionMap.empty()) {
      for (auto conf: confFunctionMap)
        for (auto u:conf.second) {
          if (conf.first == config_name)
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
                  usage->prev_functions.insert(callInst->getParent()->getParent());
                  usage->prev_configurations.insert(conf.first);
                }
              }
            }
          }
          delete PostDT;
        }
    }
  }
}

void DependencyAnalyzer::getSuccConfig(std::string config_name, UsageInfo *usage) {
  std::vector<CallerRecord> callees;
  std::vector<Function *> visitedCallees;
  callees = calleeGraph[usage->inst->getParent()->getParent()];

  while (!callees.empty()) {
    CallerRecord calleeRecord = callees.back();
    callees.pop_back();
    if (std::find(visitedCallees.begin(), visitedCallees.end(), calleeRecord.first->getParent()->getParent())
        != visitedCallees.end())
      continue;

    PostDominatorTree *PostDT = new PostDominatorTree();
    PostDT->runOnFunction(*usage->inst->getParent()->getParent());
    if (isa<CmpInst>(usage->inst) && !PostDT->dominates(calleeRecord.first->getParent(), usage->inst->getParent())) {
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
          if (*PI != usage->inst->getParent()) {
            if (!PostDT->dominates(calleeRecord.first->getParent(), *PI))
              continue;
          }
          prevBlocks.push_back(*PI);
          if (*PI == usage->inst->getParent()) {
            flag = false;
            Function *callee = calleeRecord.first->getParent()->getParent();
            visitedCallees.push_back(calleeRecord.first->getParent()->getParent());
            std::map<std::string, std::vector<Instruction *>> confFunctionMap = functionUsages[callee];
            if (!confFunctionMap.empty()) {
              for (auto conf: confFunctionMap) {
                if (conf.first != config_name)
                  usage->succ_configurations.insert(conf.first);
              }
            }
            continue;
          }
        }
      }
    }
    delete PostDT;
  }

  Function *function = usage->inst->getParent()->getParent();
  std::map<std::string, std::vector<Instruction *>> confFunctionMap = functionUsages[function];
  if (!confFunctionMap.empty()) {
    for (auto conf: confFunctionMap)
      for (auto u:conf.second) {
        if (conf.first == config_name)
          continue;
        PostDominatorTree *PostDT = new PostDominatorTree();
        PostDT->runOnFunction(*function);
        if (!PostDT->dominates(u->getParent(), usage->inst->getParent())) {
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
                usage->succ_configurations.insert(conf.first);
              }
            }
          }
        }
        delete PostDT;
      }
  }
}

bool DependencyAnalyzer::recalculateOffset(Module &M) {
  std::string file_name = gExecutableName + "_config.log";
  std::error_code OutErrorInfo;
  llvm::raw_fd_ostream configFile(llvm::StringRef(file_name), OutErrorInfo, sys::fs::F_None);

  calculateSize(M);
  if (!parseConfigFile()) {
    return false;
  }

  for (auto &config:configInfoList) {
    if (config.offsetList.size() == 0)
      continue;
    int offset = config.offsetList.back();
    int index = 0;
    for (auto element:sysvarOffsets) {
      if (offset < element)
        break;
      index++;
      offset -= element;
    }
    config.offsetList.pop_back();
    config.offsetList.push_back(index);
  }

  for (auto &config:configInfoList) {
    configFile << "{\"" << config.config_name << "\",\"" << config.type_name << "\"," << config.bit << ",{";
    for (uint i = 0; i < config.offsetList.size(); i++) {
      configFile << config.offsetList[i];
      if (i != config.offsetList.size() - 1)
        configFile << ",";
    }
    configFile << "}},\n";
  }
  return true;
}

bool DependencyAnalyzer::parseConfigFile() {
  std::ifstream configMappingFile;
  std::string line;
  ConfigInfo cInfo;

  configMappingFile.open(gInput, std::ios::in);
  if (!configMappingFile.is_open()) {
    errs() << "Fail to open configuration mapping file\n";
    return false;
  }

  while (getline(configMappingFile, line)) {
    cInfo.offsetList.clear();
    cInfo.config_name = parseName(&line);
    cInfo.type_name = parseName(&line);
    if (cInfo.config_name == "" || cInfo.type_name == "")
      return false;
    cInfo.bit = parseLong(&line);

    ulong position = line.find('}');
    line = line.substr(1, position - 1);
    position = line.find(',');
    while (position != std::string::npos) {
      cInfo.offsetList.push_back(parseInteger(&line));
      position = line.find(',');
    }

    if (line != "") {
      cInfo.offsetList.push_back(std::stoi(line));
    }
    configInfoList.push_back(cInfo);
  }
  return true;
}

std::string DependencyAnalyzer::parseName(std::string *line) {
  ulong position = line->find("\"");
  std::string token;
  if (position == std::string::npos) {
    errs() << "can't find the name " << *line << "\n";
    return "";
  }
  *line = line->substr(position + 1, line->size());
  position = line->find("\"");
  token = line->substr(0, position);
  *line = line->substr(position + 1, line->size());
  return token;
}

long long DependencyAnalyzer::parseLong(std::string *line) {
  ulong position = line->find(",");
  std::string token;
  if (position == std::string::npos) {
    errs() << "can't find the long variable " << *line << "\n";
    return -2;
  }
  *line = line->substr(position + 1, line->size());
  position = line->find(",");
  token = line->substr(0, position);
  *line = line->substr(position + 1, line->size());
  return std::stoll(token);
}

int DependencyAnalyzer::parseInteger(std::string *line) {
  ulong position = line->find(",");
  std::string token;
  if (position == std::string::npos) {
    errs() << "can't find the integer variable " << *line << "\n";
    return -2;
  }
  token = line->substr(0, position);
  *line = line->substr(position + 1, line->size());
  return std::stoi(token);
}

void DependencyAnalyzer::calculateSize(Module &M) {
  int lastWidth;

  for (auto *sty : M.getIdentifiedStructTypes()) {
    if (sty->getName() == "struct.System_variables") {
      unsigned length;
      unsigned rest = 64;
      bool flag = false;
      for (auto element : sty->elements()) {
        switch (element->getTypeID()) {
          case llvm::CompositeType::VoidTyID:sysvarOffsets.push_back(0);
            break;
          case llvm::CompositeType::DoubleTyID:
            if (flag) {
              int lWidth = sysvarOffsets.back();
              lWidth += rest / 8;
              sysvarOffsets.pop_back();
              sysvarOffsets.push_back(lWidth);
              rest = 64;
              flag = false;
            }
            sysvarOffsets.push_back(sizeof(long));
            break;
          case llvm::CompositeType::IntegerTyID:length = cast<IntegerType>(element)->getBitWidth();
            //align to 64 bit
            if (length != 64) {
              flag = true;
              if (rest < length)
                rest = 64;
              rest -= length;
              sysvarOffsets.push_back(length / 8);
            } else {
              if (flag) {
                int lWidth = sysvarOffsets.back();
                lWidth += rest / 8;
                sysvarOffsets.pop_back();
                sysvarOffsets.push_back(lWidth);
                rest = 64;
                flag = false;
              }
              sysvarOffsets.push_back(length / 8);
            }
            break;
          case llvm::CompositeType::PointerTyID:
            if (flag) {
              int lWidth = sysvarOffsets.back();
              lWidth += rest / 8;
              sysvarOffsets.pop_back();
              sysvarOffsets.push_back(lWidth);
              rest = 64;
              flag = false;
            }
            sysvarOffsets.push_back(sizeof(char *));
            break;
          default:sysvarOffsets.push_back(-1);
        }
      }
    }
  }

  //Algin the variable
  lastWidth = sysvarOffsets.back();
  if (lastWidth != 8) {
    lastWidth = 8;
    sysvarOffsets.pop_back();
    sysvarOffsets.push_back(lastWidth);
  }
}

/*
 * Return ture for getting the config_name info
 */
template<typename T>
bool DependencyAnalyzer::getConfigIndex(T *variable, std::vector<int> *dst) {
  int i = -1;
  if (dyn_cast<LoadInst>(variable)) {
    return false;
  }
  for (ConfigInfo cInfo: configInfoList) {
    i++;
    if (!cInfo.offsetList.empty()) {
      if (!isPointStructVariable(variable))
        continue;

      StringRef structName = variable->getType()->getPointerElementType()->getStructName();
      if (structName == cInfo.type_name)
        dst->push_back(i);

      continue;
    }

    if (cInfo.type_name == variable->getName())
      dst->push_back(i);
  }

  if (dst->empty())
    return false;
  else
    return true;
}

int DependencyAnalyzer::getBitIndex(int64_t option) {
  int i = -1;
  for (ConfigInfo cInfo: configInfoList) {
    i++;
    if(option & cInfo.bit) {
      if (cInfo.type_name == "class.THD")
        return i;
    }
  }
  return -1;
}

template<typename T>
bool DependencyAnalyzer::isPointStructVariable(T *variable) {
  Type *pointerElementTpye;

  if (!variable->getType()->isPointerTy())
    return variable->getType()->isStructTy();

  pointerElementTpye = variable->getType()->getPointerElementType();
  return pointerElementTpye->isStructTy();
}

/*
 * If the variable is load to other variable, also check the usage of the other variable
 */
DependencyAnalyzer::VariableWrapper DependencyAnalyzer::handleMemoryAcess(Instruction *inst,
                                           VariableWrapper var_wrapper
                                           ) {
  VariableWrapper v;
  if (auto storeInst = dyn_cast<StoreInst>(inst)) {
    if (storeInst->getValueOperand() == var_wrapper.variable) {
      v.variable = storeInst->getPointerOperand();
      v.level = var_wrapper.level;
      return v;
    }
  }

  if (auto *loadInst = dyn_cast<LoadInst>(inst)) {
    if (loadInst->getPointerOperand() == var_wrapper.variable) {
      v.variable = loadInst;
      v.level = var_wrapper.level;
      return v;
    }
  }

  v.variable = NULL;
  v.level = 0;
  return v;
}

template<typename T>
std::vector<Value *> DependencyAnalyzer::getBitVariables(T *variable, long long bitvalue) {
  std::vector<Value *> immediate_variable;
  std::vector<Value *> visited_variables;
  std::vector<Value *> result;
  immediate_variable.push_back(variable);

  while (!immediate_variable.empty()) {
    Value *value = immediate_variable.back();
    immediate_variable.pop_back();

    if (std::find(visited_variables.begin(), visited_variables.end(), value)
        != visited_variables.end())
      continue;
    visited_variables.push_back(value);

    for (auto use: value->users()) {
      Instruction *inst = dyn_cast<Instruction>(use);

      if (auto *storeInst = dyn_cast<StoreInst>(inst)) {
        if (storeInst->getValueOperand() == value) {
          immediate_variable.push_back(storeInst->getPointerOperand());
        }
      }

      if (auto *loadInst = dyn_cast<LoadInst>(inst)) {
        if (loadInst->getPointerOperand() == value) {
          immediate_variable.push_back(loadInst);
        }
      }

      if (inst->getOpcode() == Instruction::And) {
        if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(inst->getOperand(0)))
          if ((CI->getZExtValue() & bitvalue) && (CI->getSExtValue() > 0))
            result.push_back(inst);

        if (llvm::ConstantInt *CI = dyn_cast<llvm::ConstantInt>(inst->getOperand(1)))
          if ((CI->getZExtValue() & bitvalue) && (CI->getSExtValue() > 0))
            result.push_back(inst);

        for(unsigned int i = 0; i < inst->getNumOperands(); i++) {
          if (inst->getOperand(i) == value) {
            immediate_variable.push_back(inst);
            break;
          }
        }
      }


    }
  }
  return result;
}

/*
 * Find the definition of the target variable
 */
template<typename T>
std::vector<Value *> DependencyAnalyzer::getStructMember(T *variable, std::vector<int> offsetList) {
  std::vector<VariableWrapper> candidates;
  std::vector<Value *> result;
  VariableWrapper init_variable;

  if (offsetList.empty()) {
    result.push_back(variable);
    return result;
  }

  init_variable.variable = variable;
  init_variable.level = 0;
  candidates.push_back(init_variable);
  while (!candidates.empty()) {
    VariableWrapper value = candidates.back();
    candidates.pop_back();
    if (value.level == offsetList.size()) {
      result.push_back(value.variable);
      continue;
    }

    for (User *U : value.variable->users()) {
      if (Instruction *Inst = dyn_cast<Instruction>(U)) {
        VariableWrapper variableWrapper = handleMemoryAcess(Inst, value);
        if (variableWrapper.variable)
          candidates.push_back(variableWrapper);
        if (isa<GetElementPtrInst>(Inst)) {
          GetElementPtrInst *getElementPtrInst = dyn_cast<GetElementPtrInst>(Inst);
          ConstantInt *structOffset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2));
          if (structOffset && structOffset->getValue() == offsetList[value.level]) {
            VariableWrapper v;
            v.variable = Inst;
            v.level = value.level + 1;
            candidates.push_back(v);
          }
        }
      } else {
        Value *val = dyn_cast<Value>(U);
        bool flag = true;
        if (isa<ConstantExpr>(val)) {
          ConstantExpr *constExpr = dyn_cast<ConstantExpr>(val);
          if (constExpr->isGEPWithNoNotionalOverIndexing()) {
            if (constExpr->getOpcode() == Instruction::GetElementPtr) {
              if (offsetList.size() == constExpr->getNumOperands() - 2) {
                for (uint i = 2; i < constExpr->getNumOperands(); i++) {
                  Value *variable = constExpr->getOperand(i);
                  if (const ConstantInt *CI = dyn_cast<ConstantInt>(variable)) {
                    if (CI->getBitWidth() <= 32) {
                      int64_t val = CI->getSExtValue();
                      if (val != offsetList[i - 2]) {
                        flag = false;
                        break;
                      }
                    }
                  }
                }
                if (flag) {
                  result.push_back(val);
                }
              }
            }
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
void DependencyAnalyzer::storeVariableUse(std::string configuration, T *variable) {
  std::vector<Value *> variables;
  std::map<Instruction *, Function *> callerMap;
  std::vector<Value *> visited_variables;
  std::vector<Parameter> visited_parameters;
  variables.push_back(variable);
  while (!variables.empty()) {
    Value *value = variables.back();
    variables.pop_back();

    if (std::find(visited_variables.begin(), visited_variables.end(), value)
        != visited_variables.end())
      continue;
    visited_variables.push_back(value);

    /** Find the use of the instance member**/
    if (auto *getElementPtrInst = dyn_cast<GetElementPtrInst>(value)) {
      bool flag = false;
      if (auto *structOffset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2))) {
        int64_t offset = structOffset->getValue().getSExtValue();
        Function *func = getElementPtrInst->getParent()->getParent();
        for (auto calling : callerMap) {
          if (calling.second == func) {
            value = calling.first->getOperand(0);
            flag = true;
          }
        }
        if (flag) {
          if (std::find(visited_variables.begin(), visited_variables.end(), value)
              != visited_variables.end())
            continue;
          visited_variables.push_back(value);
          storeStructMemberUse(value,offset,&variables);
          continue;
        }
      }
    }

    for (User *U : value->users()) {
      if (Instruction *inst = dyn_cast<Instruction>(U)) {
        if(Value* related_val = getDataRelatedVariable(inst, value, visited_variables,&visited_parameters, &callerMap, false))
          variables.push_back(related_val);

        std::vector<UsageInfo> usagelist = configUsages[configuration];
        UsageInfo usage;
        usage.inst = inst;
        usagelist.push_back(usage);
        configUsages[configuration] = usagelist;

        Function *function = inst->getParent()->getParent();
        std::map<std::string, std::vector<Instruction *>> &usages = functionUsages[function];
        std::vector<Instruction *> &configUsages = usages[configuration];
        configUsages.push_back(inst);
      } else {
        errs() << "no instruction " << *U << "\n";
      }
    }
  }
}

void DependencyAnalyzer::storeStructMemberUse(Value *value,  int64_t offset, std::vector<Value *> *variables) {
  std::vector<Value *> instance_var;
  std::vector<Value *> visited_instance;
  std::vector<Parameter> visited_parameter;

  instance_var.push_back(value);
  while(!instance_var.empty()) {
    Value *val = instance_var.back();
    instance_var.pop_back();

    if (std::find(visited_instance.begin(), visited_instance.end(), val)
        != visited_instance.end())
      continue;
    visited_instance.push_back(value);

    for (User *U : val->users()) {
      if (auto inst = dyn_cast<Instruction>(U)) {
        if (Value* related_val = getDataRelatedVariable(inst, val, visited_instance, &visited_parameter, NULL, false)) {
          if (std::find(visited_instance.begin(), visited_instance.end(), related_val)
              == visited_instance.end()) {
//            errs() << "inst " << *inst << "; func " << inst->getParent()->getParent()->getName() <<"\n";
            instance_var.push_back(related_val);
          }
        }

        if (auto getElementPtrInst = dyn_cast<GetElementPtrInst>(inst)) {
          ConstantInt *element_offset = dyn_cast<ConstantInt>(getElementPtrInst->getOperand(2));
          if (element_offset && element_offset->getValue() == offset)
            if (std::find(visited_instance.begin(), visited_instance.end(), getElementPtrInst)
                == visited_instance.end()) {
//              errs() << "add value " << *getElementPtrInst << ";func " << getElementPtrInst->getParent()->getParent()->getName() << "\n";
              variables->push_back(getElementPtrInst);
            }
        }
      } else {
        Value *val = dyn_cast<Value>(U);
        if (isa<ConstantExpr>(val)) {
          ConstantExpr *constExpr = dyn_cast<ConstantExpr>(val);
          if (constExpr->isGEPWithNoNotionalOverIndexing()) {
            if (constExpr->getOpcode() == Instruction::GetElementPtr) {
              Value *variable = constExpr->getOperand(2);
              if (const ConstantInt *CI = dyn_cast<ConstantInt>(variable)) {
                if (CI->getBitWidth() <= 32) {
                  int64_t off = CI->getSExtValue();
                  if (off == offset) {
                    if (std::find(visited_instance.begin(), visited_instance.end(), val)
                        == visited_instance.end()) {
//                      errs() << "add value " << *val << "\n";
                      variables->push_back(val);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

Value *DependencyAnalyzer::getDataRelatedVariable(Instruction *inst,
                                                  Value *value,
                                                  std::vector<Value *> visitedInstruction,
                                                  std::vector<Parameter> *visitedArgument,
                                                  std::map<Instruction *, Function *> *callerMap,
                                                  bool isInstance) {
  if (auto *storeInst = dyn_cast<StoreInst>(inst))
    if (storeInst->getValueOperand() == value) {
      if (std::find(visitedInstruction.begin(), visitedInstruction.end(), storeInst->getPointerOperand())
          == visitedInstruction.end())
        return storeInst->getPointerOperand();
    }

  if (auto *loadInst = dyn_cast<LoadInst>(inst))
    if (loadInst->getPointerOperand() == value) {
      if (std::find(visitedInstruction.begin(), visitedInstruction.end(), loadInst) == visitedInstruction.end()) {
        return loadInst;
      }
    }

  if (CallInst *callInst = dyn_cast<CallInst>(inst)) {
    if (!isInstance) {
      for (unsigned int i = 0; i < callInst->getNumArgOperands(); i++) {
        if (value == callInst->getArgOperand(i)) {
          int flag = false;
          unsigned int func_body = callInst->getNumArgOperands();
          Value *val = callInst->getOperand(func_body);

          if (isa<Function>(val)) {
            Function *func = dyn_cast<Function>(val);
            auto arg = func->arg_begin();
            unsigned int index = 1;

            if (func->arg_size() <= i) {
              errs() << "can't handle random parameter now\n";
              continue;
            }

            while (index <= i) {
              arg++;
              index++;
            }

            for (auto argument : (*visitedArgument)) {
              if (argument.second == &(*arg) && argument.first == func) {
                flag = true;
                break;
              }
            }
            if (!flag) {
              if(callerMap)
                (*callerMap)[callInst] = func;
              visitedArgument->emplace_back(func, &(*arg));
              return &(*arg);
            }
          } else {
            if(isa<ConstantExpr>(val)){
              ConstantExpr *constExpr = dyn_cast<ConstantExpr>(val);
              if (constExpr->getOpcode() == Instruction::BitCast) {
                Value *op0 = constExpr->getOperand(0);
                if(isa<Function>(op0)) {
                  Function *func = dyn_cast<Function>(op0);
                  auto arg = func->arg_begin();
                  unsigned int index = 1;

                  if (func->arg_size() <= i) {
                    errs() << "can't handle random parameter now\n";
                    continue;
                  }

                  while (index <= i) {
                    arg++;
                    index++;
                  }

                  for (auto argument : *visitedArgument) {
                    if (argument.second == arg && argument.first == func) {
                      flag = true;
                      break;
                    }
                  }

                  if (!flag) {
                    if(callerMap)
                      (*callerMap)[callInst] = func;
                    visitedArgument->emplace_back(func, &(*arg));
                    return &(*arg);
                  }
                } else {
                  errs() << "op0 is not a function\n";
                }
              }
            }
          }
        }
      }
    }
  }

  if(InvokeInst *invokeInst = dyn_cast<InvokeInst>(inst)) {
    if (!isInstance) {
      for (unsigned int i = 0; i < invokeInst->getNumArgOperands(); i++) {
        if (value == invokeInst->getArgOperand(i)) {
          int flag = false;
          unsigned int func_body = invokeInst->getNumArgOperands();
          Value *val = invokeInst->getOperand(func_body);

          if (isa<Function>(val)) {
            Function *func = dyn_cast<Function>(val);
            auto arg = func->arg_begin();
            unsigned int index = 1;

            if (func->arg_size() <= i) {
              errs() << "can't handle random parameter now\n";
              continue;
            }

            while (index <= i) {
              arg++;
              index++;
            }

            for (auto argument : (*visitedArgument)) {
              if (argument.second == &(*arg) && argument.first == func) {
                flag = true;
                break;
              }
            }
            if (!flag) {
              if(callerMap)
                (*callerMap)[invokeInst] = func;
              visitedArgument->emplace_back(func, &(*arg));
              return &(*arg);
            }
          } else {
            if(isa<ConstantExpr>(val)){
              ConstantExpr *constExpr = dyn_cast<ConstantExpr>(val);
              if (constExpr->getOpcode() == Instruction::BitCast) {
                Value *op0 = constExpr->getOperand(0);
                if(isa<Function>(op0)) {
                  Function *func = dyn_cast<Function>(op0);
                  auto arg = func->arg_begin();
                  unsigned int index = 1;

                  if (func->arg_size() <= i) {
                    errs() << "can't handle random parameter now\n";
                    continue;
                  }

                  while (index <= i) {
                    arg++;
                    index++;
                  }

                  for (auto argument : *visitedArgument) {
                    if (argument.second == arg && argument.first == func) {
                      flag = true;
                      break;
                    }
                  }

                  if (!flag) {
                    if(callerMap)
                      (*callerMap)[invokeInst] = func;
                    visitedArgument->emplace_back(func, &(*arg));
                    return &(*arg);
                  }
                } else {
                  errs() << "op0 is not a function\n";
                }
              }
            }
          }
        }
      }
    }
  }
  return NULL;
}

void DependencyAnalyzer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<llvm::CallGraphWrapperPass>();
}

char DependencyAnalyzer::ID = 0;
static RegisterPass<DependencyAnalyzer> X("analyzer", "Find the dependent config_name for a target system");