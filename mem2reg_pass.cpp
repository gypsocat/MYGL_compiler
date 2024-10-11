#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/CFG.h"
#include <set>
#include <cassert>
#include <map>
#include <vector>

using namespace llvm;

namespace {
    struct Alloca2RegPass : public FunctionPass {
        static char ID;
        Alloca2RegPass() : FunctionPass(ID) {}

        // std::vector
        std::vector<AllocaInst*> Allocas;
        std::map<AllocaInst*, std::set<BasicBlock*>> DefsBlock;
        std::map<AllocaInst*, std::set<BasicBlock*>> UsesBlock;

        std::map<BasicBlock*, std::map<PHINode*, AllocaInst*>> PhiMap;
        std::map<AllocaInst*, Value*> ValueMap;

        /// @brief 存储基本块的支配边界
        std::map<BasicBlock*, std::vector<BasicBlock*>> DomFsBlock;
        StoreInst *OnlyStore;
        BasicBlock *OnlyBlock;
        // 使用支配树来实现的alloca2reg版本
        DominatorTree DT;


        /// @brief 计算基本块的支配边界
        /// @param BB 
        void calcuDf(Function& F) {
            for(BasicBlock& BB : F) {
                DomFsBlock[&BB] = {};
            }
            BasicBlock* pB, *prevBB, *idomBB;
            for(BasicBlock& BB : F) {
                if(pred_size(&BB) > 1) {
                    idomBB = DT[&BB]->getIDom()->getBlock();
                    for(auto prev : predecessors(&BB)) {
                        prevBB = dyn_cast<BasicBlock>(prev);
                        while(prevBB != idomBB) {
                            DomFsBlock[prevBB].push_back(&BB);
                            prevBB = DT[prevBB]->getIDom()->getBlock();
                        }
                    }
                }
            }
        }

        int getInstructionIndex(BasicBlock* BB, Instruction* I) {
            int index = 0;
            for(BasicBlock::iterator it = BB->begin(), end = --BB->end() ; it != end ; ++it) {
                if(I == dyn_cast<Instruction>(it))
                    return index;
                index++;
            }
            return -1;
        }

        bool rewriteSingleStoreAlloca(AllocaInst* alloca) {
            bool StoringGlobalVal = !isa<Instruction>(OnlyStore->getOperand(0));
            BasicBlock *StoreBB = OnlyStore->getParent();
            int StoreIndex = -1;

            UsesBlock[alloca].clear();

            for (User *U : alloca->users()) {
                Instruction *UserInst = cast<Instruction>(U);
                if (UserInst == OnlyStore)
                    continue;
                LoadInst *LI = cast<LoadInst>(UserInst);

                if (!StoringGlobalVal) { // Non-instructions are always dominated.
                    if (LI->getParent() == StoreBB) {
                        if (StoreIndex == -1)
                            StoreIndex = getInstructionIndex(StoreBB, OnlyStore);

                        if (unsigned(StoreIndex) > getInstructionIndex(StoreBB, LI)) {
                            UsesBlock[alloca].insert(StoreBB);
                            continue;
                        }
                    } else if (!DT.dominates(StoreBB, LI->getParent())) {
                        UsesBlock[alloca].insert(LI->getParent());
                        continue;
                    }
                }

                Value *ReplVal = OnlyStore->getOperand(0);

                LI->replaceAllUsesWith(ReplVal);
                LI->eraseFromParent();
            }

            // Finally, after the scan, check to see if the store is all that is left.
            if (UsesBlock[alloca].empty())
                return false; // If not, we'll have to fall back for the remainder.

            OnlyStore->eraseFromParent();
            alloca->eraseFromParent();
            return true;
        }

        void removeFromAllocaList(unsigned& AllocaIndex) {
            Allocas[AllocaIndex] = Allocas.back();
            Allocas.pop_back();
            --AllocaIndex;
        }

        bool isAggregateType(llvm::Type* ty) {
            return ty->isIntegerTy();
        }

        /// 这个函数用于判断alloca指令，是否只有load/store. 如果没有使用，或者只有load和store返回true, 否则返回false
        bool isPromote(const AllocaInst* allocaInst) {
            if(!isAggregateType(allocaInst->getAllocatedType()))
                return false;
            for(const User *user : allocaInst->users()) {
                if(const LoadInst* load = dyn_cast<LoadInst>(user)) {
                    continue;
                }
                else if(const StoreInst* store = dyn_cast<StoreInst>(user)) {
                    continue;
                }
                else{
                    return false;
                }
            }
            return true;
        }

        /// 收集promoted的alloca指令
        void collectPromotedAllocas(Function& F) {
            BasicBlock* BB;

            Allocas.clear();

            for(Function::iterator bbIt = F.begin(), bbEnd = F.end() ; bbIt != bbEnd ; ++bbIt) {
                BB = &F.getEntryBlock();
                for(BasicBlock::iterator instIt = BB->begin(), instEnd = --BB->end() ; instIt != instEnd ; ++instIt) {
                    if(AllocaInst* alloca = dyn_cast<AllocaInst>(instIt)) {
                        if(isPromote(alloca))
                            Allocas.push_back(alloca);
                    }
                }
            }
        }

        void clear() {
            OnlyStore = nullptr;
        }

        /// 分析alloca指令，找出他定义的块和使用的块
        void analysisAlloca(AllocaInst* alloca) {
            for(User* u : alloca->users()) {
                Instruction* i = dyn_cast<Instruction>(u);

                if (StoreInst *SI = dyn_cast<StoreInst>(i)) {
                    DefsBlock[alloca].insert(SI->getParent());
                    OnlyStore = SI;
                } else {
                    LoadInst *LI = cast<LoadInst>(i);
                    UsesBlock[alloca].insert(LI->getParent());
                }
            }
        }


        /// @brief 执行alloca2reg
        /// @param F 
        void executeMem2Reg(Function& F) {

              /// step1 删除掉没有使用的alloca指令
              /// @example int getValue() {
              ///               int a;          // this value will be remove
              ///               return 10;
              ///          }
              ///
              for (unsigned AllocaNum = 0; AllocaNum != Allocas.size(); ++AllocaNum) {
                AllocaInst *AI = Allocas[AllocaNum];

                // 如果当前alloca没有user，删除掉这条alloca指令
                if (AI->use_empty()) {
                    AI->eraseFromParent();
                    removeFromAllocaList(AllocaNum);
                }

                /// 分析alloca指令，寻找定义和使用他的地方
                analysisAlloca(AI);

                // 如果alloca只被定义了一次
                // if(DefsBlock[AI].size() == 1) {
                //     if (rewriteSingleStoreAlloca(AI)) {
                //         removeFromAllocaList(AllocaNum);
                //         continue;
                //     }
                // } 
              }
            
              
              std::set<BasicBlock*> PhiSet;
              std::vector<BasicBlock*> W;
              PHINode* phi;
              BasicBlock* X;
              /// 插入phi节点
              for(AllocaInst* alloca : Allocas) {
                PhiSet.clear();
                W.clear();
                phi = nullptr;
                for (BasicBlock* BB : DefsBlock[alloca]) {
                    W.push_back(BB);
                }
                while(!W.empty()) {
                    X = W.back();
                    W.pop_back();
                    for(BasicBlock* Y : DomFsBlock[X]) {
                        if(PhiSet.find(Y) == PhiSet.end()) {
                            phi = PHINode::Create(alloca->getAllocatedType(), 0);
                            phi->insertBefore(dyn_cast<Instruction>(Y->begin()));
                            PhiSet.insert(Y);
                            PhiMap[Y].insert({phi, alloca});
                            if(std::find(W.begin(), W.end(), Y) == W.end())
                                W.push_back(Y);
                        }
                    }
                } 
              }

              
              std::vector<Instruction*> InstCanBeRemoveList;
              std::vector<std::pair<BasicBlock*, std::map<AllocaInst*, Value*>>> WorkList;
              std::set<BasicBlock*> VisitedSet;
              BasicBlock *SuccBB, *BB;
              std::map<AllocaInst*, Value*> IncommingValues;
              Instruction* Inst;
              WorkList.push_back({&F.getEntryBlock(), {}});
              for(AllocaInst* alloca : Allocas) {
                WorkList[0].second[alloca] = UndefValue::get(alloca->getAllocatedType());
              }

              ///> 遍历所有基本块，用phi节点替换掉那些没有使用到的基本块以及填充phi的imcoming value
              while(!WorkList.empty()) {
                    BB = WorkList.back().first;
                    IncommingValues = WorkList.back().second;
                    WorkList.pop_back();

                    if(VisitedSet.find(BB) != VisitedSet.end())
                        continue;
                    else
                        VisitedSet.insert(BB);

                    for(BasicBlock::iterator it = BB->begin(), end = --BB->end() ; it != end ; ++it) {
                        Inst = dyn_cast<Instruction>(it);
                        if(AllocaInst* AI = dyn_cast<AllocaInst>(it)) {
                            if(std::find(Allocas.begin(), Allocas.end(), AI) == Allocas.end())
                                continue;
                            InstCanBeRemoveList.push_back(Inst);
                        }
                        else if(LoadInst* LI = dyn_cast<LoadInst>(it)) {
                            AllocaInst* AI = dyn_cast<AllocaInst>(LI->getPointerOperand());
                            if(!AI)
                                continue;
                            if(std::find(Allocas.begin(), Allocas.end(), AI) != Allocas.end()) {
                                if(IncommingValues.find(AI) == IncommingValues.end())
                                    IncommingValues[AI] = UndefValue::get(AI->getAllocatedType());
                                LI->replaceAllUsesWith(IncommingValues[AI]);
                                InstCanBeRemoveList.push_back(Inst);
                            }
                        }
                        else if(StoreInst* SI = dyn_cast<StoreInst>(it)) {
                            AllocaInst* AI = dyn_cast<AllocaInst>(SI->getPointerOperand());
                            if(!AI)
                                continue;
                            if(std::find(Allocas.begin(), Allocas.end(), AI) == Allocas.end()) 
                                continue;
                            IncommingValues[AI] = SI->getValueOperand();
                            InstCanBeRemoveList.push_back(Inst);
                        }
                        else if(PHINode* Phi = dyn_cast<PHINode>(it)) {
                            if(PhiMap[BB].find(Phi) == PhiMap[BB].end())
                                continue;
                            IncommingValues[PhiMap[BB][Phi]] = Phi;
                        }
                    }

                    for(succ_iterator PI = succ_begin(BB), E = succ_end(BB); PI != E; ++PI) {
                        SuccBB = dyn_cast<BasicBlock>(*PI);
                        WorkList.push_back({SuccBB, IncommingValues});
                        for(BasicBlock::iterator it = SuccBB->begin(), end = --SuccBB->end() ; it != end ; ++it) {
                            if(PHINode* Phi = dyn_cast<PHINode>(it)) {
                                if(PhiMap[SuccBB].find(Phi) == PhiMap[SuccBB].end()) {
                                    continue;
                                }
                                if(IncommingValues[PhiMap[SuccBB][Phi]]) {
                                    Phi->addIncoming(IncommingValues[PhiMap[SuccBB][Phi]], BB);
                                }
                            }
                        }
                    }
              }

              /// 因为迭代器的原因，指令先存在一个vector中，然后处理完之后一次性删除
              while(!InstCanBeRemoveList.empty()) {
                Inst = InstCanBeRemoveList.back();
                Inst->eraseFromParent();
                InstCanBeRemoveList.pop_back();
              }

              /// 删除掉那些没有使用的phi指令
              for(auto & it1 : PhiMap) {
                for(auto &it : it1.second) {
                    if(it.first->use_empty())
                        it.first->eraseFromParent();
                }
              }

        }

        virtual bool runOnFunction(Function& F) {
            
            while(true) {
                errs() << "Working on function called " << F.getName() << "!\n";

                collectPromotedAllocas(F);

                if(Allocas.empty())
                    break;

                DefsBlock.clear();
                UsesBlock.clear();
                PhiMap.clear();
                DomFsBlock.clear();

                /// 计算支配树
                DT.recalculate(F);
                /// 计算支配边界
                calcuDf(F);

                executeMem2Reg(F);

            }

            return true;
        }
    };
}

char Alloca2RegPass::ID = 0;

static RegisterPass<Alloca2RegPass> X("alloca2reg", "Alloca-to-register pass for minic", false, false);

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerAlloca2RegPass(const PassManagerBuilder &,
                                    legacy::PassManagerBase &PM) {
    PM.add(new Alloca2RegPass());
}
static RegisterStandardPasses
        RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
                       registerAlloca2RegPass);
