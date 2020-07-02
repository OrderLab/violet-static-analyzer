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
    case DEPENDENCY: {
      break;
    }
    case MAPPING: {
      break;
    }
    case RECALCULATE_OFFSET:
      if(!recalculateOffset(M))
        return false;
      break;
    default:errs() << "Unknown analyzer type " << gAnalysisType << "\n";
      return false;

  }
  return true;
}

bool DependencyAnalyzer::recalculateOffset(Module &M) {
  std::string file_name = gExecutableName + "_config.log";
  std::error_code OutErrorInfo;
  llvm::raw_fd_ostream configFile(llvm::StringRef(file_name), OutErrorInfo, sys::fs::F_None);

  calculateSize(M);
  if(!parseConfigFile()) {
    return false;
  }

  for (auto &config:configMap) {
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

  for (auto &config:configMap) {
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

  configMappingFile.open(gInput,std::ios::in);
  if(!configMappingFile.is_open()) {
    errs() << "Fail to open configuration mapping file\n";
    return false;
  }

  while (getline(configMappingFile,line)) {
    cInfo.offsetList.clear();
    cInfo.config_name = parseName(&line);
    cInfo.type_name = parseName(&line);
    if (cInfo.config_name == "" || cInfo.type_name == "")
      return false;
    cInfo.bit = parseLong(&line);

    ulong position = line.find('}');
    line = line.substr(1,position-1);
    position = line.find(',');
    while (position != std::string::npos) {
      cInfo.offsetList.push_back(parseInteger(&line));
      position = line.find(',');
    }

    if (line != "") {
      cInfo.offsetList.push_back(std::stoi(line));
    }
    configMap.push_back(cInfo);
  }
  return true;
}

std::string DependencyAnalyzer::parseName(std::string *line) {
  ulong position = line->find("\"");
  std::string token;
  if (position == std::string::npos) {
    errs() << "can't fine the name " <<  *line << "\n";
    return "";
  }
  *line = line->substr(position + 1,line->size());
  position = line->find("\"");
  token = line->substr(0,position);
  *line = line->substr(position + 1, line->size());
  return token;
}

long long DependencyAnalyzer::parseLong(std::string *line) {
  ulong position = line->find(",");
  std::string token;
  if (position == std::string::npos) {
    errs() << "can't fine the long variable " <<  *line << "\n";
    return -2;
  }
  *line = line->substr(position + 1,line->size());
  position = line->find(",");
  token = line->substr(0,position);
  *line = line->substr(position + 1, line->size());
  return std::stoll(token);
}

int DependencyAnalyzer::parseInteger(std::string *line) {
  ulong position = line->find(",");
  std::string token;
  if (position == std::string::npos) {
    errs() << "can't fine the integer variable " <<  *line << "\n";
    return -2;
  }
  token = line->substr(0,position);
  *line = line->substr(position + 1, line->size());
  return std::stoi(token);
}

void DependencyAnalyzer::calculateSize(Module &M) {
  int lastWidth;

  for (auto *sty : M.getIdentifiedStructTypes()) {
    if (sty->getName() == "struct.system_variables") {
      unsigned length;
      unsigned rest = 64, flag = false;
      for (auto element : sty->elements()) {
        switch (element->getTypeID()) {
          case llvm::CompositeType::VoidTyID:sysvarOffsets.push_back(0);
            break;
          case llvm::CompositeType::DoubleTyID:
            if (flag) {
              int lastWidth = sysvarOffsets.back();
              lastWidth += rest / 8;
              sysvarOffsets.pop_back();
              sysvarOffsets.push_back(lastWidth);
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
                int lastWidth = sysvarOffsets.back();
                lastWidth += rest / 8;
                sysvarOffsets.pop_back();
                sysvarOffsets.push_back(lastWidth);
                rest = 64;
                flag = false;
              }
              sysvarOffsets.push_back(length / 8);
            }
            break;
          case llvm::CompositeType::PointerTyID:
            if (flag) {
              int lastWidth = sysvarOffsets.back();
              lastWidth += rest / 8;
              sysvarOffsets.pop_back();
              sysvarOffsets.push_back(lastWidth);
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

template<typename T>
void DependencyAnalyzer::handleVariableUse(T *variable) {
  std::map<std::string, std::vector<Value *>> confVariableMap;
  std::vector<int> configurations;
  if (getConfigurationInfo(variable, &configurations)) {
    while (!configurations.empty()) {
      int i = configurations.back();
      configurations.pop_back();
      if (configMap[i].bit != -1) {
        std::vector<Value *> options = getVariables(variable, &(configMap[i].offsetList));
        for (auto option:options) {
          std::vector<Value *> v = getBitVariables(option, configMap[i].bit);
          std::vector<Value *> usagelist = confVariableMap[configMap[i].config_name];
          usagelist.insert(usagelist.end(), v.begin(), v.end());
          confVariableMap[configMap[i].config_name] = usagelist;
        }
      } else {
        std::vector<Value *> v = getVariables(variable, &(configMap[i].offsetList));
        std::vector<Value *> usagelist = confVariableMap[configMap[i].config_name];
        usagelist.insert(usagelist.end(), v.begin(), v.end());
        confVariableMap[configMap[i].config_name] = usagelist;
      }
    }

    for (auto configUsage: confVariableMap)
      for (auto var:configUsage.second)
        storeVariableUse(configUsage.first, var);
  }
}

/*
 * Return ture for getting the config_name info
 */
template<typename T>
bool DependencyAnalyzer::getConfigurationInfo(T *variable, std::vector<int> *dst) {
  int i = -1;
  if (dyn_cast<LoadInst>(variable)) {
    return false;
  }
  for (ConfigInfo cInfo: configMap) {
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
void DependencyAnalyzer::handleMemoryAcess(Instruction *inst,
                                           VariableWrapper *variable,
                                           std::vector<VariableWrapper> *immediate_variable) {
  if (StoreInst *storeInst = dyn_cast<StoreInst>(inst)) {
    if (storeInst->getValueOperand() == variable->variable) {
      VariableWrapper v;
      v.variable = storeInst->getPointerOperand();
      v.level = variable->level;
      immediate_variable->push_back(v);
    }
  }

  if (LoadInst *loadInst = dyn_cast<LoadInst>(inst)) {
    if (loadInst->getPointerOperand() == variable->variable) {
      VariableWrapper v;
      v.variable = loadInst;
      v.level = variable->level;
      immediate_variable->push_back(v);
    }
  }
  return;
}

template<typename T>
std::vector<Value *> DependencyAnalyzer::getBitVariables(T *variable, long long bitvalue) {
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
std::vector<Value *> DependencyAnalyzer::getVariables(T *variable, std::vector<int> *offsetList) {
  std::vector<VariableWrapper> immediate_variable;
  std::vector<Value *> result;
  VariableWrapper init_variable;

  init_variable.variable = variable;
  init_variable.level = 0;
  immediate_variable.push_back(init_variable);

  while (!immediate_variable.empty()) {
    VariableWrapper value = immediate_variable.back();

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
            VariableWrapper v;
            v.variable = Inst;
            v.level = value.level + 1;
            immediate_variable.push_back(v);
          }
        }
      } else {
        Value *val = dyn_cast<Value>(U);
        bool flag = true;
        if (isa<ConstantExpr>(val)) {
          ConstantExpr *constExpr = dyn_cast<ConstantExpr>(val);
          if (constExpr->isGEPWithNoNotionalOverIndexing()) {
            if (constExpr->getOpcode() == Instruction::GetElementPtr) {
              if (offsetList->size() == constExpr->getNumOperands() - 2) {
                for (uint i = 2; i < constExpr->getNumOperands(); i++) {
                  Value *variable = constExpr->getOperand(i);
                  if (const ConstantInt *CI = dyn_cast<ConstantInt>(variable)) {
                    if (CI->getBitWidth() <= 32) {
                      int64_t val = CI->getSExtValue();
                      if (val != (*offsetList)[i - 2]) {
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
            if (std::find(visited_variable.begin(), visited_variable.end(), storeInst->getPointerOperand())
                == visited_variable.end())
              immediate_variable.push_back(storeInst->getPointerOperand());
          }

        if (LoadInst *loadInst = dyn_cast<LoadInst>(inst))
          if (loadInst->getPointerOperand() == value) {
            if (std::find(visited_variable.begin(), visited_variable.end(), loadInst) == visited_variable.end())
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

void DependencyAnalyzer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<llvm::CallGraphWrapperPass>();
}

char DependencyAnalyzer::ID = 0;
static RegisterPass<DependencyAnalyzer> X("analyzer", "Find the dependent config_name for a target system");