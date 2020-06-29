//
// Created by yigonghu on 11/25/19.
//
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Support/FileSystem.h>
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
    struct Hello : public ModulePass {
        static char ID;
        Hello() : ModulePass(ID) {}

        void findCallee(llvm::raw_fd_ostream *outFile, CallGraphNode* node, int level) {
          for (CallGraphNode::iterator callee = node->begin(); callee != node->end(); callee++) {
            if (!callee->second->getFunction()) {
              continue;
            }
            if (callee->second->getFunction()->getName() == "SyncRepWaitForLSN") {
              (*outFile) << "Level " << level << ": "<< callee->second->getFunction()->getName() << "\n";
              return;
            }

            if (level < 10) {
              (*outFile) << "Level " << level << ": "<< callee->second->getFunction()->getName() << "\n";
                findCallee(outFile, callee->second,level+1);
            }
          }
        }

        bool runOnModule(Module &M) override {
          std::error_code OutErrorInfo;
          llvm::raw_fd_ostream outFile(llvm::StringRef("test.log"), OutErrorInfo, sys::fs::F_None);
          CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
          CallGraphNode* start_function,*end_function;
          for (CallGraph::iterator node = CG.begin(); node != CG.end(); node++) {
            Function *nodeFunction = node->second->getFunction();
            if (!nodeFunction) {
              continue;
            }
            if (nodeFunction->getName() == "CheckPWChallengeAuth") {
              findCallee(&outFile,node->second.get(),0);
            }
          }
            outFile.close();
            return false;
        }

      void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.addRequired<llvm::CallGraphWrapperPass>();
      }

    }; // end of struct Hello
}  // end of anonymous namespace

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);