/*
 * Gating Functions
 * 2018 Tong Zhang <t.zhang2@partner.samsung.com>
 */

#include "color.h"
#include "gating_function_base.h"

//#include "llvm/IR/CallSite.h"
#include "llvm/Support/raw_ostream.h"

#include "utility.h"

#include <fstream>


// CAP and DAC use the same framwork
// if we want to enable one of these, define the macro that is used in kernel source code.
#define CAP_MAGIC            51525300
#define DAC_MAGIC            61525000

#define TraceInner       0
#define LLVMV        1 //define then version >10, else <= 10  

// actually the | & ~ binary operations use the result of permutation and combination of these numbers
uint64_t  DAC_numbers[] = {
  0x00000001,
  0x00000002,
  0x00000004,
  0x00000008,
  0x00000010,
  0x00000020,
  0x00000040,
  0x00000080
};

//char GatingNew::ID;

/*
 * we ignore the macro related with CAP_TO_MASK because they are not magic number anymore
 * 
 * dictionary: include/linux/capability.h
#define CAP_LAST_U32      ((_KERNEL_CAPABILITY_U32S) - 1)
#define CAP_LAST_U32_VALID_MASK   (CAP_TO_MASK(CAP_LAST_CAP + 1) -1)

# define CAP_EMPTY_SET    ((kernel_cap_t){{ 0, 0 }})
# define CAP_FULL_SET     ((kernel_cap_t){{ ~0, CAP_LAST_U32_VALID_MASK }})
# define CAP_FS_SET       ((kernel_cap_t){{ CAP_FS_MASK_B0 \
            | CAP_TO_MASK(CAP_LINUX_IMMUTABLE), \
            CAP_FS_MASK_B1 } })
# define CAP_NFSD_SET     ((kernel_cap_t){{ CAP_FS_MASK_B0 \
            | CAP_TO_MASK(CAP_SYS_RESOURCE), \
            CAP_FS_MASK_B1 } })


%call3.i = tail call %struct.file.41045* bitcast (%struct.file.127832* (i32, %struct.filename*, %struct.open_flags*)* @do_filp_open to %struct.file.41045* (i32, %struct.filename*, %struct.open_flags*)*)(i32 -100, %struct.filename* %call.i, %struct.open_flags* nonnull @__do_sys_uselib.uselib_flags) #72
 */


// SimpleSet *skip_vars;
// SimpleSet *skip_funcs;
// SimpleSet *crit_syms;
// SimpleSet *kernel_api;

// void initialize_gatlin_sets(StringRef knob_skip_func_list,
//                             StringRef knob_skip_var_list,
//                             StringRef knob_crit_symbol,
//                             StringRef knob_kernel_api) {
//   llvm::errs() << "Load supplimental files...\n";
//   StringList builtin_skip_functions(std::begin(_builtin_skip_functions),
//                                     std::end(_builtin_skip_functions));
//   skip_funcs = new SimpleSet(knob_skip_func_list, builtin_skip_functions);
//   if (!skip_funcs->use_builtin())
//     llvm::errs() << "    - Skip function list, total:" << skip_funcs->size()
//                  << "\n";

//   StringList builtin_skip_var(std::begin(_builtin_skip_var),
//                               std::end(_builtin_skip_var));
//   skip_vars = new SimpleSet(knob_skip_var_list, builtin_skip_var);
//   if (!skip_vars->use_builtin())
//     llvm::errs() << "    - Skip var list, total:" << skip_vars->size() << "\n";

//   StringList builtin_crit_symbol;
//   crit_syms = new SimpleSet(knob_crit_symbol, builtin_crit_symbol);
//   if (!crit_syms->use_builtin())
//     llvm::errs() << "    - Critical symbols, total:" << crit_syms->size()
//                  << "\n";

//   StringList builtin_kapi;
//   kernel_api = new SimpleSet(knob_kernel_api, builtin_kapi);
//   if (!kernel_api->use_builtin())
//     llvm::errs() << "    - Kernel API list, total:" << kernel_api->size()
//                  << "\n";
// }

//#ifdef CAP_MAGIC
bool isCAP(Value *v) {
  ConstantInt *vc= dyn_cast<ConstantInt>(v);
  if (!vc)
    return false;

  unsigned int wi = vc->getBitWidth();
    if (wi > 64) {
      return false;
    }

  uint64_t cons;
  cons = vc->getZExtValue() - CAP_MAGIC;
    if (cons >= 0 && cons <=37)
      return true;
    return false;
}
//#endif


//#ifdef DAC_MAGIC
bool isDAC(Value *v) {
  ConstantInt *vc = dyn_cast<ConstantInt>(v);
  if (!vc)
    return false;

  unsigned int wi = vc->getBitWidth();
  if (wi > 64)
    return false;

  uint64_t cons;
  long long l = vc->getZExtValue();
  l = l > 0 ? l : -l;
  cons = l - DAC_MAGIC;
//    unsigned int len = sizeof(DAC_numbers)/sizeof(DAC_numbers[0]);
//    for (unsigned int i = 0; i < len; i++) {
//      if(cons == DAC_numbers[i])
//        return true;
//    }
  if (cons >= 0 && cons <=999)
    return true;
  return false;
}
//#endif

//only care about case where all indices are constantint
void getGepIndicies(GetElementPtrInst* gep, Indices& indices)
{
    if (!gep)
        return;
    //replace all non-constant with zero
    //because they are literally an array...
    //and we are only interested in the type info
    for (auto i = gep->idx_begin(); i!=gep->idx_end(); ++i)
    {
        ConstantInt* idc = dyn_cast<ConstantInt>(i);
        if (idc)
            indices.push_back(idc->getSExtValue());
        else
            indices.push_back(0);
    }
}

//compare two indices
bool isIndcsEqual(Indices* a, Indices* b)
{
    if (a->size()!=b->size())
        return false;
    auto ai = a->begin();
    auto bi = b->begin();
    while(ai!=a->end())
    {
        if (*ai!=*bi)
            return false;
        bi++;
        ai++;
    }
    return true;
}

/*InstructionSet get_user_instruction(Value *v) {
  InstructionSet ret;
  ValueSet vset;
  ValueSet visited;
  visited.insert(v);
  for (auto *u : v->users()) {
    vset.insert(u);
  }
  while (vset.size()) {
    for (auto x : vset) {
      v = x;
      break;
    }
    visited.insert(v);
    vset.erase(v);
    // if a user is a instruction add it to ret and remove from vset
    if (Instruction *i = dyn_cast<Instruction>(v)) {
      ret.insert(i);
      continue;
    }
    // otherwise add all user of current one
    for (auto *_u : v->users()) {
      if (visited.count(_u) == 0)
        vset.insert(_u);
    }
  }
  return ret;
}*/

/*
 * user can trace back to function argument?
 * only support simple wrapper
 * return the cap parameter position in parameter list
 * TODO: track full def-use chain
 * return -1 for not found
 */
/*int use_parent_func_arg(Value *v, Function *f) {
  int cnt = 0;
  for (auto a = f->arg_begin(), b = f->arg_end(); a != b; ++a) {
    if (dyn_cast<Value>(a) == v)
      return cnt;
    cnt++;
  }
  return -1;
}*/

// there is no store inst, we ingore it 
bool store2LocalVar(Value *v) {
  return false; // TODO:fix this
}

// backward trace the operand of the instruction is And/Or/Xor instruction or ConstanExpr
bool findMacroOperand(Instruction *ins, bool (*isMacro)(Value *v), InstructionSet *visited) {
  if(ins == NULL || visited->count(ins))
    return false;
  visited->insert(ins);

  unsigned int op = ins->getOpcode();
  if (op == Instruction::And 
      || op == Instruction::Or 
      || op == Instruction::Xor) {

    // errs() << "testing: ins" << *ins << "\n";
    // errs() << "testing: 0 op" << *ins->getOperand(0) << "\n";
    // errs() << "testing: 1 op" << *ins->getOperand(1) << "\n";
    
    for (unsigned int i = 0; i < ins->getNumOperands(); i++) {
      Value *v = ins->getOperand(i);
      if (ConstantInt *ci = dyn_cast<ConstantInt>(v)) {
        if (isMacro(ci)) {
          errs() << "binary op instruction:" << *ins << "\n";
          return true;
        }

      } else if (ConstantExpr *ce = dyn_cast<ConstantExpr>(v)) {
        Instruction *ceinst = ce->getAsInstruction();
        if (ceinst && findMacroOperand(ceinst, isMacro, visited))
          return true;
      } else if (Instruction * inst = dyn_cast<Instruction>(v)) {
        if (inst && findMacroOperand(inst, isMacro, visited))
          return true;
      }
    }
  }
  
  return false;
}

//preprocess for find macro in instruction
bool handleCmpBitwiseBinaryOperations(Value *v, bool (*isMacro)(Value *v)) {
  InstructionSet visited;
  if (ConstantExpr *ce = dyn_cast<ConstantExpr>(v)) {
    Instruction *ceinst = ce->getAsInstruction();
    if (ceinst && findMacroOperand(ceinst, isMacro, &visited))
      return true;

  } else if (Instruction *i = dyn_cast<Instruction>(v)) {
    if(i && findMacroOperand(i, isMacro, &visited))
      return true;
  }
  return false;
}

// get the callee function of direct call
Function* getCalleeFunctionDirect(Instruction* i)
{
    CallInst* ci = dyn_cast<CallInst>(i);
    if (Function* f = ci->getCalledFunction())
        return f;
    Value* cv = NULL;
#ifdef LLVMV
    cv = ci->getCalledOperand();
#else
    cv = ci->getCalledValue();
#endif
    Function* f = dyn_cast<Function>(cv->stripPointerCasts());
    return f;
}

// if the macro store to a field of variable
//Instruction *traceMacroField(GetElementPtrInst *gep) {
//}

// we trace the users of gv field (contain the macro)
// 1) used as an argument, call @f (... ,gv, ...)
// 2) its address is taken, trace the address pointer, store
// 3) gep: trace the macro field, also use interprocedural, i.e., call @f (... ,gv.field(macro), ...)
/*
void traceEveryUserofGV(Value *user, GVI *gvi, FunctionSet *funcset) {
  ValueList worklist;
  ValueSet visited;
  worklist.push_back(user);
  Value *v = user;

  std::vector<Value*> argvs;
  argvs.push_back(dyn_cast<Value>(gvi->first));
  std::vector<bool> isField;

  bool macro = false;
  if (gvi->second->empty())
    macro = true;
  isField.push_back(macro);

  Value *argv = argvs.back();
  //int countdepth = 0;

  while (worklist.size()) {
    v = worklist.front();
    worklist.pop_front();
    if (visited.count(v))
      continue;
    visited.insert(v);
    
    if (Instruction *i = dyn_cast<Instruction>(v)) {
      switch (i->getOpcode()) {
        case (Instruction::Xor):
        case (Instruction::Or) :
        case (Instruction::And): {
          errs() << "& | op:" << *i <<"\n"; // may need to add to check icmp instruction
          //errs() << "countdepth" << countdepth << "\n";
          //if (countdepth == 0)
          funcset->insert(i->getFunction());
          errs() << "find from gv" << i->getFunction()->getName() << "\n";
          break;
        }

        case (Instruction::Load): {
          //countdepth--;
          //errs() << "find core dump load\n ";
          LoadInst *load = dyn_cast<LoadInst>(i);
          Value *loadp = load->getPointerOperand()->stripPointerCasts();
          if (ConstantExpr *loadcon = dyn_cast<ConstantExpr>(loadp)) {
            Instruction *loadi = loadcon->getAsInstruction();
            if (GetElementPtrInst *loadgep = dyn_cast<GetElementPtrInst>(loadi)) {
              Indices idcs;
              getGepIndicies(loadgep, idcs);
              idcs.pop_front();
              for (auto ind : *gvi->second) {
                if (isIndcsEqual(&idcs, ind)) {
                  isField.push_back(true);
                }
              }
            }

          } 
          argv = dyn_cast<Value>(load);
          for (auto u : i->users())
            worklist.push_back(u);
          break;
          
        }


        case (Instruction::Store): {
          //errs() << "find core dump store\n ";
          errs() << "handle store instruction" << *i << "\n";
          //countdepth++;
          StoreInst *si = dyn_cast<StoreInst>(i);
          Value *pointer = si->getPointerOperand()->stripPointerCasts();
          if (pointer) {
            argv = dyn_cast<Value>(pointer);
            worklist.push_back(pointer);
          }
          break;
        }

        case (Instruction::GetElementPtr): {
          GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(i);
          Indices idcs;
          getGepIndicies(gep, idcs);
          idcs.pop_front();

          for (auto ind : *gvi->second) {
            if (isIndcsEqual(&idcs, ind)) {
              errs() << "find gep\n " << *gep;
              macro = true;
              argv = dyn_cast<Value>(gep);
              errs() << "trace the macro field: " << *gep;
              //countdepth++;
              for (auto u : i->users())
                worklist.push_back(u);
            }
          }
          break;
        }
        // inter-procedural analysis
        // gv or its field is used as a function parameter
        case (Instruction::Call): {
          //errs() << "find core dump call\n ";
          CallInst *ci = dyn_cast<CallInst>(i);
          if(ci == NULL)
            break;
          Function *cif = getCalleeFunctionDirect(ci);
          if (cif == NULL) {
            errs() << "it is a indirect call: " << *ci << "\n";
          }
          int argidx = -1;
          for (unsigned int ai = 0; ai < ci->getNumArgOperands(); ai++) {
            if (ci->getArgOperand(ai) == argv) { // this v need to figure out
              argidx = ai;
              break;
            }
          }
          if (argidx == -1) {
            errs() << "used in a call instruc but not an argument\n";
            errs() << *ci << "\n";
            break;
          }
          auto targ = cif->arg_begin();
          for (int ai =0; ai < argidx; ai++)
            targ++;
          
          if (macro) {
            errs() << "find from gv" << cif->getName() << "\n";
            funcset->insert(cif);
          }
          else {
            //argv = targ;
            worklist.push_back(targ);
          }

          break;
        }

        

        case(Instruction::Select):
                {
                  //errs() << "find core dump Select\n ";
                    SelectInst* sli = dyn_cast<SelectInst>(i);
                    worklist.push_back(sli->getTrueValue());
                    worklist.push_back(sli->getFalseValue());
                    break;
                }

                case (Instruction::PHI) : {
          //errs() << "find core dump phi-----------" << *i << "\n";

          PHINode* phi = dyn_cast<PHINode>(i);
          if (phi == NULL)
            break;
                    for (unsigned int i=0;i<phi->getNumIncomingValues();i++)
                        worklist.push_back(phi->getIncomingValue(i));
                    break;
        }

        default: {
          errs() << "GV other instruction: " << *i << "\n";
          //for (auto u : i->users())
          //  worklist.push_back(u);
          break;
        }
      }
    } else {
      errs() << "GV non-instruction: " << *v << "\n";
        for (auto u : v->users())
          worklist.push_back(u);
    }
  }

}*/



// backtrace the source of the memcpy instruction until we find the allocinst
Value *traceMemcpyDes(CallInst *ci) {
  MemCpyInst *mci = dyn_cast<MemCpyInst>(ci);
  if(!mci)
    return NULL;
  
  Value *des = mci->getOperand(0);
  Value *v = des;
  //errs() << "first op ? des: " << *des << "\n";

  if (BitCastInst *bci = dyn_cast<BitCastInst>(des)) {
    v = bci->getOperand(0);
  }
  if (AllocaInst *ai = dyn_cast<AllocaInst>(v)) {
    return ai;
  }
  return NULL;
/*  int memdepth = 0;

  ValueList vs;
  vs.push_back(des);

  while (vs.size()) {
    if(memdepth > 1000)
      return NULL;
    v = vs.front();
    vs.pop_front();
    for (auto u : v->users()) {
      errs() << "memcpy user: " << *u << "\n";
      vs.push_back(u);
    }
    if (AllocaInst *ai = dyn_cast<AllocaInst>(v))
      return ai;
    memdepth++;
  }*/
}

//int depth = 0;


// inter-procedural analysis
// if the aggregate that contains the macro is used as an argument in a callinst
// we will continue dig into the callee and find the usage of macro field.
// no-need pointer analysis, becasue there is no store instruction
void traceEveryUsersofVar(Value *argv, bool isField, IndSet *indset, 
  FunctionSet *funcset, ValueSet &visited, int depth) {

  depth++;
  for (auto v : argv->users()){
    if (depth > 20)
      return;

    if(visited.count(v))
      return;
    visited.insert(v);

    if (Instruction *i = dyn_cast<Instruction>(v)) {
      switch (i->getOpcode()) {
        case (Instruction::Xor):
        case (Instruction::Or) :
        case (Instruction::And): {
          //errs() << "& | op:" << *i <<"\n"; // may need to add to check icmp instruction
          //errs() << "countdepth" << countdepth << "\n";
          //if (countdepth == 0)
          funcset->insert(i->getFunction());
          //errs() << "find from gv: " << i->getFunction()->getName() << "\n";
          break;
        }

        case (Instruction::Load): {
          //countdepth--;
          //errs() << "find core dump load\n ";
          LoadInst *load = dyn_cast<LoadInst>(i);
          Value *loadp = load->getPointerOperand()->stripPointerCasts();

          // handle the inner constantexpr in load, gep will get the macro field.
          if (ConstantExpr *loadcon = dyn_cast<ConstantExpr>(loadp)) {
            Instruction *loadi = loadcon->getAsInstruction();
            if (GetElementPtrInst *loadgep = dyn_cast<GetElementPtrInst>(loadi)) {
              Indices idcs;
              getGepIndicies(loadgep, idcs);
              idcs.pop_front();
              for (auto ind : *indset) {
                if (isIndcsEqual(&idcs, ind)) {
                  isField = true;
                }
              }
            }

          } 
          Value *aargv = dyn_cast<Value>(load);
          //errs() << "gv load inst argv: " << *aargv << "\nits function: " << i->getFunction()->getName() << "\n";
          traceEveryUsersofVar(aargv, isField, indset, funcset, visited, depth);
            //worklist.push_back(u);
          break;
          
        }


        case (Instruction::Store): {
          //errs() << "find core dump store\n ";
          //errs() << "handle store instruction" << *i << "\n";
          //countdepth++;
          StoreInst *si = dyn_cast<StoreInst>(i);
          Value *pointer = si->getPointerOperand()->stripPointerCasts();
          if (pointer) {
            Value *aargv = dyn_cast<Value>(pointer);
            //errs() << "\ngv store inst argv: " << *aargv << "\nits function: " << i->getFunction()->getName()<< "\n";
            traceEveryUsersofVar(aargv, isField, indset, funcset, visited, depth);
          }
          break;
        }

        case (Instruction::GetElementPtr): {
          GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(i);
          Indices idcs;
          getGepIndicies(gep, idcs);
          idcs.pop_front();

          for (auto ind : *indset) {
            if (isIndcsEqual(&idcs, ind)) {
              errs() << "find gep\n " << *gep;
              isField = true;
              Value *aargv = dyn_cast<Value>(gep);
              //errs() << "trace the macro field: " << *gep;
              //countdepth++;
              //errs() << "\ngv gep inst argv: " << *aargv << "\nits function: " << i->getFunction()->getName()<< "\n";
              traceEveryUsersofVar(aargv, isField, indset, funcset, visited, depth);
            }
          }
          break;
        }
        // inter-procedural analysis
        // gv or its field is used as a function parameter
        case (Instruction::Call): {
          //errs() << "find core dump call\n ";
          CallInst *ci = dyn_cast<CallInst>(i);
          if(ci == NULL)
            break;
          Function *cif = getCalleeFunctionDirect(ci);
          if (cif == NULL) {
            errs() << "it is a indirect call: " << *ci << "\n";
            break;
          }
          if(cif->getName().startswith("llvm.lifetime") || cif->getName().startswith("printk"))
            break;

          //gv is copied to a alloced memory, we trace this memory.
          if (cif->getName().startswith("llvm.memcpy")){
            //errs() << "gv llvm memcpy: " << *ci << "\n";
            Value * vv = traceMemcpyDes(ci);
            if (vv) {// if we are not able to trace the mem, return NULL
              Value *aargv = vv;
              //errs() << "argv: " << *vv << "\n";
              traceEveryUsersofVar(aargv, isField, indset, funcset, visited, depth);
            }
          } else { // other call ins
            int argidx = -1;
            for (unsigned int ai = 0; ai < ci->getNumArgOperands(); ai++) {
              if (ci->getArgOperand(ai) == argv) { // this v need to figure out
                argidx = ai;
                break;
              }
            }
            if (argidx == -1) {
              //errs() << "used in a call instruc but not an argument\n";
              errs() << *ci << "\n";
              break;
            }
            int num = 0;
            //auto argit = f->arg_begin();
            for (auto a = cif->arg_begin(), b = cif->arg_end(); a != b; ++a){
              ++num;
              //++argit;
            }
            if (argidx >= num)
              break;
            Value* targ = cif->getArg(argidx);
            //auto targ = cif->arg_begin();
            //for (int ai =0; ai < argidx; ai++)
            //  targ++;

            //special handle, if the macro appears in a call, add it to targetSet
            // if the aggregate gv that contain the macro appears in a call, we wil trace inter-procedurally
            if (isField) {
              errs() << "find from gv: " << cif->getName() << "\n";
              funcset->insert(cif);
            }
            else {
              Value *aargv = targ;
              //errs() << "gv call inst:" << *ci <<"\nits function: " << ci->getFunction()->getName()<< "\n";
              //errs() << "\ngv call inst argv: " << *aargv << "\n";
              traceEveryUsersofVar(aargv, isField, indset, funcset, visited, depth);
            }
          }

          break;
        }

        
/*
        case(Instruction::Select):
        {
                //errs() << "find core dump Select\n ";
          SelectInst* sli = dyn_cast<SelectInst>(i);
          errs() << "\ngv Select inst argv: " << *sli << "\n";

          traceEveryUsersofVar(sli->getTrueValue(), isField, indset, funcset, visited);
          traceEveryUsersofVar(sli->getFalseValue(), isField, indset, funcset, visited);
          break;
        }

        case (Instruction::PHI) : {
          //errs() << "find core dump phi-----------" << *i << "\n";

          PHINode* phi = dyn_cast<PHINode>(i);
          if (phi == NULL)
            break;
          errs() << "\ngv phi inst argv: " << *phi << "\n";

          //for (unsigned int i=0;i<phi->getNumIncomingValues();i++)
            traceEveryUsersofVar(phi->getIncomingValue(i), isField, indset, funcset, visited);
            
          break;
        }
*/
        default: {
          //errs() << "GV other instruction: " << *i << "\n";
          traceEveryUsersofVar(i, isField, indset, funcset, visited, depth);
          //for (auto u : i->users())
          //  worklist.push_back(u);
          break;
        }
      }
    } else {
      //errs() << "GV non-instruction: " << *v << "\n";
      traceEveryUsersofVar(v, isField, indset, funcset, visited, depth);
      //for (auto u : v->users())
      //  worklist.push_back(u);
    }

  }
}



void traceGVI(GVIS *gvis, FunctionSet *funcset) {
  for (auto gvi : *gvis) {
    GlobalVariable *gv = gvi->first;
    errs () << "\n\ngv: " << *gv << "\n";
    IndSet *indset = gvi->second;
    ValueSet visited;
    bool macro = false;
    if (indset->empty())
      macro = true;
    //for (auto *user : gv->users()) {
    int depth = 0;
    visited.insert(gv);
    traceEveryUsersofVar(gv, macro, indset, funcset, visited, depth);
    //}
  }
}


/*
 * we trace the gv that uses the macro to initialize
 * field-sensitive: record the indies of the gv, so we can trace the usages of this field
 */
bool __resolveInitializer(Constant *con, IndSet &indset, bool (*isMacro)(Value *v)) {
  if (con == NULL)
    return false;
  bool ret = false;

  if (ConstantInt *coni = dyn_cast<ConstantInt>(con)) {
    if(isMacro(coni)) {
      Indices *ind = new Indices;
      indset.insert(ind);
      return true;
    }
  } else if (ConstantAggregate *ca = dyn_cast<ConstantAggregate>(con)) {
    for (unsigned int i = 0; i < ca->getNumOperands(); i++) {
      Value *v = ca->getOperand(i);
      if(Constant *cc = dyn_cast<Constant>(v)) {
        IndSet indsetnew;
        if (__resolveInitializer(cc, indsetnew, isMacro)) {
          for (auto idc : indsetnew) {
            idc->push_front(i);
            indset.insert(idc);
            ret = true;
          } 
        }
      }
    }
  } else if (ConstantDataArray *cda = dyn_cast<ConstantDataArray>(con)) {
    for (unsigned int i = 0; i < cda->getNumElements(); i++) {
      Constant *cdac = cda->getElementAsConstant(i);
      IndSet indsetnew;
      if (__resolveInitializer(cdac, indsetnew, isMacro)) {
        for (auto idc: indsetnew) {
          idc->push_front(i);
          indset.insert(idc);
          ret = true;
        }
      }
    }
  }
  return ret;
}

// also process the binary operations
bool useParentFuncArg(Value *v, Function *f, int pos) {
  //errs() << "may wrong function: " << f->getName()<< " and its index: " << pos << "\n";
    int num = 0;
    //auto argit = f->arg_begin();
    for (auto a = f->arg_begin(), b = f->arg_end(); a != b; ++a){
      ++num;
      //++argit;
    }
    if(pos >= num) {
      //errs() << "num: "<< num << "\n";
      return false;
    }

    Value *arg = f->getArg(pos);
    //dyn_cast<Value>(argit);
    if(arg == NULL)
      return false;
    
    if (arg == v)
        return true;
    else {
      ValueList worklist;
      ValueSet visited;
      for (auto u : arg->users()) {
        if (Instruction *i = dyn_cast<Instruction>(u)) {
          unsigned int op = i->getOpcode();
          if (op == Instruction::And 
              || op == Instruction::Or 
              || op == Instruction::Xor) {
            worklist.push_back(u);
          }
          
        }
      }
      
      while (worklist.size()) {
        Value *wv = worklist.front();
        worklist.pop_front();
        if(visited.count(wv))
          continue;
        visited.insert(wv);
        if(wv == v)
          return true;

        for (auto u : wv->users()) {
          if (Instruction *i = dyn_cast<Instruction>(u)) {
            unsigned int op = i->getOpcode();
          if (op == Instruction::And 
              || op == Instruction::Or 
              || op == Instruction::Xor) {
            worklist.push_back(u);
          }
            
          }
        }
      }

    }
    return false;
}


void GatingNew::resolveInitializer(GlobalVariable *gv, bool (*isMacro)(Value *v), GVIS *gvis) {
  IndSet indset;
  Constant *con =  gv->getInitializer();
  
  if (isa<ConstantInt>(con) || isa<ConstantAggregate>(con) || isa<ConstantDataArray>(con)) {
    //bool retvalue = false;
    __resolveInitializer(con, indset, isMacro);
    if (!indset.empty()) {
      IndSet *inds = new IndSet;
      for (auto id : indset)
        inds->insert(id);
      GVI *gvi = new GVI(gv, inds);
      gvis->push_back(gvi);
    }
  }

}



// there is no store instruction, we do not need to handle it
void GatingNew::handleStoreInst(StoreInst *si, FunctionSet *fs) {
  Value *addr = si->getPointerOperand()->stripPointerCasts();
    if (store2LocalVar(addr)) { //store to var and populate;
        //TODO: tarce local variable.
    }
    //traceEveryUsersofVar
    errs() << "StoreInst Local Var: " << *si << "\n";
    errs() << "!!!!!!!!!trace the variable!!!!!!!!!!\n";              
}


Function* GatingNew::handleCallInst(CallInst *ci) {
  Function *ret = NULL;
  errs() << "CallInst user: " << *ci << "\n";
  if (Function *f = ci->getCalledFunction()) {
    errs() << "target function: " << f->getName() << "\n";
    ret = f;
        //fs->insert(f); //insert the target function
    }
    else {
      Value *cv = NULL;
#ifdef LLVMV
      cv = ci->getCalledOperand();
#else
      cv = ci->getCalledValue();
#endif
      
      Function *bcf = dyn_cast<Function>(cv->stripPointerCasts());
      if (bcf) {
        errs() << "target function: " << bcf->getName() << "\n";
        ret = bcf;
            //fs->insert(bcf); //insert the target function
            
            //ii->getDebugLoc().print(errs());
            }
        else 
            errs() << "2 We need to handle indirect call!\n";
    }
    return ret;
}



void GatingNew::findInnerFuncs(Function *f, unsigned int cap_pos, bool (*isMacro)(Value *v)) {
  //
  // auto arg = f->arg_begin();
  // for (unsigned int i = 0; i < i; i++) {
  //  arg++;
  // }
  // Value *v = arg;

  // last time discovered functions
    
  // functions that will be used for discovery next time
    //FunctionData pass_data_next;
    FunctionSet  visited;

    int round = 10;
    FunctionData pass_data[round];
    pass_data[round-1][f] = cap_pos;

    while(round > 1) {
      --round;
      for (auto fpair : pass_data[round]) {
        Function *func = fpair.first;
        int pos = fpair.second;
        if(visited.count(func))
          continue;
        visited.insert(func);

        for (Function::iterator fi = func->begin(), fe = func->end(); fi != fe; ++fi) {
        BasicBlock *bb = dyn_cast<BasicBlock>(fi);
        for (BasicBlock::iterator ii = bb->begin(), ie = bb->end(); ii != ie; ++ii) {
          CallInst *ci = dyn_cast<CallInst>(ii);
          if (!ci)
            continue;
              // we are expecting a direct call
          Function *child = getCalleeFunctionDirect(ci);
          if (!child)
            continue;
          StringRef fname = child->getName();
              // dont bother if this belongs to skip function
          
          /*if (skip_funcs->exists_ignore_dot_number(fname) ||
            kernel_api->exists_ignore_dot_number(fname))
            continue;*/
          if(fname.startswith("print"))
            continue;

              // for each of the function argument
          for (unsigned int i = 0; i < ci->getNumArgOperands(); ++i) {
            Value *capv = ci->getArgOperand(i);

            if (useParentFuncArg(capv, func, pos)) {
              pass_data[round-1][child] = i;
              //errs()<< "parent func:" << func->getName() << " its index: " << pos <<"\n";
              //errs()<< "child func:" << child->getName() << " its index: " << i <<"\n";
              if(isMacro == isCAP) {
                CAPBasic[child] = i;
                CAPInners.insert(child); // not include the original functions
              }
              else {
                DACBasic[child] = i;
                DACInners.insert(child);
              }

              break;
            }
          }
        }
      }

    // // merge result of this round
    //  if (pass_data_next.size()) {
    //    //errs() << ANSI_COLOR(BG_WHITE, FG_RED)<< "Inner checking functions:" << ANSI_COLOR_RESET << "\n";
    //    for (auto fpair : pass_data_next) {
    //      Function *fu = fpair.first;
    //      int cap_pos = fpair.second;
    //      errs() << "    - " << f->getName() << " @ " << cap_pos << "\n";
    //      pass_data[fu] = cap_pos;
    //    }
    //  }
      }
      //pass_data_next.clear();
    } 
}

void GatingNew::traceMask(Value* mask, bool (*isMacro)(Value *v)) {
  ValueList worklist;
  ValueSet visited;
  ValueSet useful;
  worklist.push_back(mask);

  while(worklist.size()) {
    Value *v = worklist.front();
    worklist.pop_front();
    if (visited.count(v))
      continue;
    visited.insert(v);

    Instruction *i = dyn_cast<Instruction>(v);
    if (i) {
      unsigned int op = i->getOpcode();
      if (op == Instruction::And  //handle binary op
        || op == Instruction::Or 
        || op == Instruction::Xor) {
        useful.insert(i);
        continue;
      } else if(op == Instruction::Call) { //handle call inst
        CallInst *ci = dyn_cast<CallInst>(i);
        Function *callee = getCalleeFunctionDirect(ci);
        if (callee == NULL) {
          errs() << "it is a indirect call: " << *ci << "\n";
          continue; // ingore indirect call
        }

        
        DACFuncs.insert(callee);
        for (unsigned int j =  0; j < ci->getNumArgOperands(); j++) {
          Value *argv = ci->getArgOperand(j);
          if (useful.count(argv)) {
            findInnerFuncs(callee, j, isMacro);
          }
        }
        
        
      }
    }

    for (auto u : v->users()) {
      worklist.push_back(u);
    }
  }
  
}



void GatingNew::processInstructions(Instruction *ii, bool (*isMacro)(Value *v), FunctionSet *fs) {
  if (CallInst *ci = dyn_cast<CallInst>(ii)) { // call instruction
      for (unsigned int i = 0; i < ci->getNumArgOperands(); i++) {
            Value *v = ci->getArgOperand(i);
            if (isMacro(v)) {
                Function *f = handleCallInst(ci);
                if (f == NULL) {
                  //errs() << "can not find the callee: " << *ci << "\n";
                  return;
                }
                fs->insert(f);
                add2Basic(isMacro, f, i);
#ifdef  TraceInner
                //errs() << "dac/cap in call as parameter: " << *ci << "\nthe arg index: " << i << "\n";
                // trace the argument and find the inner basic function
                findInnerFuncs(f, i, isMacro);
#endif
            }
        }

    } else if (StoreInst *si = dyn_cast<StoreInst>(ii)) {//store instruction
        Value *sv = si->getValueOperand();
        if (isMacro(sv)) 
            handleStoreInst(si, fs);

    } else if (CmpInst *cmpi = dyn_cast<CmpInst>(ii)) {
        for (unsigned int i = 0; i < cmpi->getNumOperands(); i++) {
            Value *cmpop = cmpi->getOperand(i);
            if (isMacro(cmpop)) {
                //errs() << "CmpInst user: " << *cmpi << "\n";
                Function *f = cmpi->getFunction();
                if (f) {
                  //errs() << "target function: " << f->getName() << "\n";
                  fs->insert(f); //insert the target function
                } else {
                  //errs() << "can not find the function?\n";
                  ii->getDebugLoc().print(errs());
                }
                if (i<=1 && isMacro == isDAC)
                  traceMaskArgument(isMacro, cmpi);
                      
            }
            else if (handleCmpBitwiseBinaryOperations(cmpop, isMacro)) { //if it is not a Bitwise Binary Operations, return 100 
              //errs() << "CmpInst user: " << *cmpi << "\n";
                Function *f = cmpi->getFunction();
                if (f) {
                  //errs() << "target function: " << f->getName() << "\n";
                  fs->insert(f); //insert the target function
                } else {
                  //errs() << "can not find the function?\n";
                  ii->getDebugLoc().print(errs());
                }
                if (isMacro == isDAC)
                  traceMaskArgument(isMacro, cmpi);
            }
      }
    } else {
      Instruction *inst = dyn_cast<Instruction>(ii);
      InstructionSet visited;
      if (inst && findMacroOperand(inst, isMacro, &visited)) {
        fs->insert(ii->getFunction());
        traceMaskArgument(isMacro, inst); // to find if the op comes from the argument
        
#ifdef  TraceInner
        Value *mask = inst;//inst->getOperand(0);/////////////????????????????
        traceMask(mask, isMacro);
#endif
      }
    }
}


void GatingNew::add2Basic(bool (*isMacro)(Value *v), Function *f, int arg) {
  if(isMacro == isCAP) {
    CAPBasic[f] = arg;
  } else if (isMacro == isDAC) {
    DACBasic[f] = arg;
  }
}

// backward: find the mask wheather comes from any function argument
void GatingNew::traceMaskArgument(bool (*isMacro)(Value *v), Instruction * inst) {
  ValueList worklist;
  ValueSet visited;
  Function *f = inst->getFunction();
  if (!f)
    return;

  for (unsigned int i = 0; i < inst->getNumOperands(); i++) {
    worklist.push_back(inst->getOperand(i));
  }

  while (worklist.size()) {
    Value *v = worklist.front();
    worklist.pop_front();
    if (visited.count(v))
      continue;
    visited.insert(v);

    int num = 0;
    for (auto a = f->arg_begin(), b = f->arg_end(); a != b; ++a) {
        num++;
        if (dyn_cast<Value>(a) == v || v == f->getArg(num-1)) {//the value is argument
          errs() << "backward trace mask function:" << f->getName() << "  index: " << num << "\n";
          add2Basic(isMacro, f, num);
          return;
        }
      }

    if (ConstantExpr *cxpr = dyn_cast<ConstantExpr>(v)) {
          Instruction *cxpri = cxpr->getAsInstruction();
          worklist.push_back(cxpri);
          continue;
      }
    if (Instruction *i = dyn_cast<Instruction>(v)) {
      switch(i->getOpcode()) {
        case (Instruction::And):  //handle binary op
        case (Instruction::Or):
        case (Instruction::Xor):
        case (Instruction::ICmp): {
          for (unsigned int j = 0; j < i->getNumOperands(); j++)
            worklist.push_back(i->getOperand(j));
        } 
        case (Instruction::BitCast): {
          BitCastInst *bci = dyn_cast<BitCastInst>(i);
          worklist.push_back(i->getOperand(0));
        }
        case (Instruction::PHI): {
              PHINode *phi = dyn_cast<PHINode>(i);
              if(!phi)
                continue;
              for (unsigned int i = 0; i < phi->getNumIncomingValues(); i++)
                worklist.push_back(phi->getIncomingValue(i));
              break;
          }
          case (Instruction::Select): {
              SelectInst *sli = dyn_cast<SelectInst>(i);
              if(!sli)
                continue;
              worklist.push_back(sli->getTrueValue());
              worklist.push_back(sli->getFalseValue());
              break;
          }
          default: {}
      }
    }
  }
}

void GatingNew::detectWrapper(FunctionData basic, FunctionData &wrapper) {
  FunctionData pass_data;
  // functions that will be used for discovery next time
    FunctionData pass_data_next;

  for (auto fpair : basic) {
    pass_data[fpair.first] = fpair.second;
  }

again:
  for (auto fpair : pass_data) {
    Function *func = fpair.first;
      int pos_original = fpair.second;
      assert(pos_original >= 0);
      // we got a capability check function or a wrapper function,
      // find all use without Constant Value and add them to wrapper
      for (auto u : func->users()) {
          InstructionSet uis;
          if (Instruction *i = dyn_cast<Instruction>(u))
            uis.insert(i);
          else
            uis = get_user_instruction(dyn_cast<Value>(u));
          if (uis.size() == 0) {
            u->print(errs());
            errs() << "\n";
            continue;
          }
          for (auto ui : uis) {
            CallInst *cs = dyn_cast<CallInst>(ui);
            if (cs == NULL)
                continue; // how come?
            Function *callee = getCalleeFunctionDirect(cs);
            if (callee != func)
                continue;
            // assert(cs->getCalledFunction()==func);
            if (pos_original >= (int)cs->getNumArgOperands()) {
                func->print(errs());
                //errs() << "Check permission function parameter\n";
                //errs() <<"function: " << func->getName() << " index: " << pos_original << "\n";
                continue;
                //llvm_unreachable(ANSI_COLOR_RED
                   //          "Check permission function parameter" ANSI_COLOR_RESET);
            }
            Value *capv = cs->getArgOperand(pos_original);
            if (isa<ConstantInt>(capv))
                continue;
            Function *parent_func = cs->getFunction();
            // we have a wrapper,
            int pos = use_parent_func_arg(capv, parent_func);
            if (pos >= 0) {
                // type 1 wrapper, cap is from parent function argument
                pass_data_next[parent_func] = pos;
            } else {
                // type 2 wrapper, cap is from inside this function
                // what to do with this?
                // llvm_unreachable("What??");
            }
          }
      }
  }
  // put pass_data_next in pass_data and chk_func_cap_position
    pass_data.clear();
    for (auto fpair : pass_data_next) {
      Function *f = fpair.first;
      int pos = fpair.second;
      if (wrapper.count(f) == 0) {
          pass_data[f] = pos;
          wrapper[f] = pos;
      }
    }
    // do this until we discovered everything
    if (pass_data.size())
      goto again;
}

void GatingNew::summary(FunctionSet &ffs, FunctionSet inner, FunctionData wrapper) {
  for (auto f : inner)
    ffs.insert(f);
  for(auto fp : wrapper)
    ffs.insert(fp.first);
}

void GatingNew::print_fs_result()
{
  for(auto f: fs_result )
  {
    errs() << f->getName() <<"\n";
  }
}

void GatingNew::findSecurity(Module& module){
  for (Module::iterator fi = module.begin(), f_end = module.end(); fi != f_end; ++fi) {
    Function *func = dyn_cast<Function>(fi);
    if (func->isDeclaration() || func->isIntrinsic() || (!func->hasName()))
      continue;
    StringRef fname = func->getName();
    if (fname.startswith("security_")) {
      fs_result.insert(func);
    }
  }
}

void GatingNew::detectLSM(Module& module) {
  for (GlobalVariable &gvi: module.globals())
  {
    GlobalVariable* gi = &gvi;
    if ( gi->isDeclaration() )
      continue;

    //Type* ty = gi -> getType();
    StringRef nm = gi -> getName();


    //if (ty->isStructTy())
    //{
      //errs()<<"--------------first-----------\n";

      //errs()<<nm<<"\n";

      if( nm.startswith("security_hook_heads"))
      {
      //StructType *tygi = dyn_cast<StructType>(ty);
        //errs()<<"--------------second-----------\n";

        for (auto *U : gi -> users())
        {
          //errs()<<"------------------third--------------\n";

          Instruction* ui = dyn_cast<Instruction>(U);
          //if (!ui)
          //  continue;
          Function* uf = NULL;
          if(ui) {
            uf = ui -> getFunction();
            if (uf)
            {
              fs_result.insert(uf);
            //  errs()<< uf->getName() <<"\n";
            }
          }
          
          uf = NULL;

          ConstantExpr* co = dyn_cast<ConstantExpr>(U);
          //errs()<< "ConstantExpr : " << *co <<"\n";
          if(co)
          {
            for(auto *u : co ->users())
            {
              //Instruction* ui = dyn_cast<Instruction>(u);

              if(Instruction* ui = dyn_cast<Instruction>(u))
              {
                uf = ui -> getFunction();
                if(uf)
                {
                  fs_result.insert(uf);
                }
              }
              
              else if(ConstantExpr* ce = dyn_cast<ConstantExpr>(u))
              {
                for(auto* us : ce->users())
                {
                  if(Instruction* ci = dyn_cast<Instruction>(us))
                  {
                    uf = ci -> getFunction();
                    //errs() << "elseif-- can we get uf?: " << uf->getName() <<"\n";
                    if(uf)
                      fs_result.insert(uf);
                  }
                }
              }
              else
                continue;
              /*else
              {
                InstructionSet u_instru;
                u_instru = get_user_instruction(dyn_cast<Value>(u));
                for(auto u_:u_instru)
                {
                  fs_result.insert(u_->getFunction());
                }
              }*/
              
              //if (uf)
              //{
              //  fs_result.insert(uf);
                //errs()<< uf->getName() <<"\n";
              //}
            }
            //errs()<<"------------------ConstantExpr--------------\n";
          }
          
        }
      }
    //}


  }
  errs() << "before size: " << fs_result.size() << "\n";
  findSecurity(module);
  errs() << "after size: " << fs_result.size() << "\n";
  errs()<<"\n\n\n";
  print_fs_result();

  int loop_cnt = 0;
  wrappers = fs_result;
  FunctionSet temp_wrappers;
  FunctionSet count_wrappers;

again:
    for (auto *lsmh: wrappers)
    {
      if(count_wrappers.count(lsmh))
        continue;
      count_wrappers.insert(lsmh);
        //errs()<<" lsmh - "<<lsmh->getName()<<"\n";
        for (auto* u: lsmh->users())
        {
            //should be call instruction and the callee is dacf
            InstructionSet uis;
            if (Instruction *i = dyn_cast<Instruction>(u))
                uis.insert(i);
            else
                uis = get_user_instruction(dyn_cast<Value>(u));
            if (uis.size()==0)
            {
                u->print(errs());
                errs()<<"\n";
                continue;
            }
            for (auto ui: uis)
            {
                CallInst *ci = dyn_cast<CallInst>(ui);
                if (!ci)
                    continue;
                Function* callee = get_callee_function_direct(ci);
                if (callee!=lsmh)
                    continue;
                Function* userf = ci->getFunction();
                //errs()<<"    used by - "<<userf->getName()<<"\n";
                //parameters comes from wrapper's parameter?
                for (unsigned int i = 0;i<ci->getNumOperands();i++)
                {
                    Value* a = ci->getOperand(i);
                    if (use_parent_func_arg_deep(a, userf)>=0)
                    {
                        temp_wrappers.insert(userf);
                        break;
                    }
                }
            }
        }
    }
    if (temp_wrappers.size())
    {
        for (auto *wf: temp_wrappers)
        {
          StringRef fname = wf->getName();
          std::string fnamestr = fname.str();
          if (skip_funcs->exists_ignore_dot_number(fnamestr) ||
            kernel_api->exists_ignore_dot_number(fnamestr))
            continue;
          if(fname.startswith("__sys") || fname.startswith("___sys") || fname.startswith("____sys")
            || fname.startswith("__ia32") || fname.startswith("__x64") ||
            fname.startswith("__se"))
            continue;

          wrappers.insert(wf);
        }

        temp_wrappers.clear();
        loop_cnt++;
        if (loop_cnt<10)
            goto again;
    }

  for(auto *fs: fs_result)
  {
    wrappers.erase(fs);
  }

  errs() << "\n\n\nwrappers"<<"\n";
  for(auto f: wrappers )
  {
    std::string fnamestr = f->getName().str();
    if((fnamestr.find("perm")!= string::npos || fnamestr.find("cap")!= string::npos 
            || fnamestr.find("security")!= string::npos || fnamestr.find("check")!= string::npos || fnamestr.find("may_")!= string::npos) && fnamestr.find("capture")==string::npos && fnamestr.find("set")==string::npos && fnamestr.find("get")==string::npos && fnamestr.find("l2cap_")==string::npos) {
      errs() << f->getName() <<"\n";
      fs_result.insert(f);
    }
  }
}

// automatically find permission functions 
GatingNew::GatingNew(Module& module)
    : GatingFunctionBase(module) {
  for (Module::iterator fi = module.begin(), fe = module.end(); fi != fe; fi++) {
    Function *func = dyn_cast<Function>(fi);
    for (Function::iterator bi = func->begin(), be = func->end(); bi != be; bi++) {
      BasicBlock *bb = dyn_cast<BasicBlock>(bi);
      for (BasicBlock::iterator ii = bb->begin(), ie = bb->end(); ii!=ie; ++ii) {
        Instruction *inst = dyn_cast<Instruction>(ii);
        if (!inst)
          continue;
#ifdef  CAP_MAGIC
        //bool FoundCAP = false;
        processInstructions(inst, isCAP, &CAPFuncs);
        //detectWrapperCAP();
#endif
#ifdef  DAC_MAGIC
        //bool FoundDAC = false;
        processInstructions(inst, isDAC, &DACFuncs);
                //detectWrapperDAC();
#endif
      }
    } 
  }

  // trace global variable
  for (GlobalVariable &gvi : module.globals()) {
    GlobalVariable *gv = &gvi;
    //errs() << "gv name: " << gv->getName() << "\n";
    if (gv->isDeclaration())
      continue;
    StringRef gvn = gv->getName();
        if (gvn.startswith("__kstrtab") || 
                gvn.startswith("__tpstrtab") || 
                gvn.startswith(".str") ||
                gvn.startswith("llvm.") ||
                gvn.startswith("__setup_str") ||
                gvn.startswith("cap_last_cap"))
            continue;
/*        if (gvn.startswith("prepare_open.oflag2acc")) {
          errs() << "found it\n";
          Constant *c = gv->getInitializer();
          errs() << *c << "\n";
          ConstantDataSequential *cds = dyn_cast<ConstantDataSequential>(c);
          if (cds) {
            errs() << "is ConstantDataSequential" << *c << "\n";
            for (unsigned int i = 0; i < cds->getNumElements(); i++) {
              errs() << cds->getElementAsInteger(i) << "\n";
            }
          }
          
          errs() << "end of fount if\n";
        }*/

        if (gv->hasInitializer()) {
#ifdef  CAP_MAGIC
          resolveInitializer(gv, isCAP, &CAP_gvis);
#endif

#ifdef  DAC_MAGIC
//      resolveInitializer(gv, isDAC, &DAC_gvis);  
#endif        
        }
  }


#ifdef  CAP_MAGIC
          traceGVI(&CAP_gvis, &CAPFuncs);
          detectWrapper(CAPBasic, CAPWrapper);
#endif
/*	  
#ifdef  DAC_MAGIC
      traceGVI(&DAC_gvis, &DACFuncs);
      for (auto gvi : DAC_gvis) {
        errs() << "\n\n\ngv: " << *gvi->first << "\n";
        for (auto indices : *gvi->second) {
          errs() << "indices:" ;
          for (auto index : *indices)
            errs() << "\t" << index;
          errs() << "\n";
        }
        
      }
      detectWrapper(DACBasic, DACWrapper);
#endif
*/
  //summary(DACFuncs, DACInners, DACWrapper);
  summary(CAPFuncs, CAPInners, CAPWrapper);

  errs() << "\n\nCAP function: \n";
  for (auto f :CAPFuncs) {
    if (f!=NULL)
      errs() << f->getName() << "\n";
  }

#ifdef  TraceInner
  errs() << "\nCAP Inner: \n"; 
  for (auto f: CAPInners) {
    if (f!=NULL)
      errs() << f->getName() << "\n";
  }

  errs() << "\nCAP Basic: \n"; 
  for (auto fpair: CAPBasic) {
    Function *f = fpair.first;
    if (f!=NULL)
      errs() << f->getName() << "\n";
  }

  errs() << "\nCAP wrapper: \n"; 
  for (auto fpair: CAPWrapper) {
    Function *f = fpair.first;
    if (f!=NULL)
      errs() << f->getName() << "\n";
  }

#endif

  
/*
  errs() << "\n---------------------------\nDAC function: \n";
  for (auto f :DACFuncs) {
    if (f!=NULL)
      errs() << f->getName() << "\n";
  }

#ifdef  TraceInner
  errs() << "\nDAC Inner: \n"; 
  for (auto f: DACInners) {
    if (f!=NULL)
      errs() << f->getName() << "\n";
  }

  errs() << "\nDAC Basic: \n"; 
  for (auto fpair: DACBasic) {
    Function *f = fpair.first;
    if (f!=NULL)
      errs() << f->getName() << "\n";
  }

  errs() << "\nDAC wrapper: \n"; 
  for (auto fpair: DACWrapper) { 
    Function *f = fpair.first;
    if (f!=NULL)
      errs() << f->getName() << "\n";
  }
#endif
*/
  
  //fs.insert(CAPFuncs.begin(), CAPFuncs.end());
  //fs.insert(DACFuncs.begin(), DACFuncs.end());
  fs.clear();

  for (auto f: CAPFuncs) {
    std::string fnamestr = f->getName().str();
    if((fnamestr.find("perm")!= string::npos || fnamestr.find("cap")!= string::npos
            || fnamestr.find("security")!= string::npos || fnamestr.find("smack_privileged")!= string::npos 
            || fnamestr.find("may_")!= string::npos) && fnamestr.find("capture")==string::npos 
      && fnamestr.find("set")==string::npos && fnamestr.find("get")==string::npos && fnamestr.find("l2cap_")==string::npos) {
      //errs() << f->getName() <<"\n";
      fs.insert(f);
    }
    if(fnamestr.find("ns_capable_setid")!= string::npos || fnamestr.find("security_")==0)
      fs.insert(f);
  }

  // for (auto f : fs) {
  //   StringRef fn = f->getName();
  //   if(fn.startswith("security") || fn.startswith("printk") || fn.startswith("has_capability_noaudit"))
  //     fs.erase(f);
  // }
  FunctionSet to_remove;

  for (auto f : CAPFuncs) {
    std::string fnamestr = f->getName().str();
    if(!((fnamestr.find("perm")!= string::npos || fnamestr.find("cap")!= string::npos
            || fnamestr.find("security")!= string::npos || fnamestr.find("smack_privileged")!= string::npos 
            || fnamestr.find("may_")!= string::npos) && fnamestr.find("capture")==string::npos 
            && fnamestr.find("set")==string::npos && fnamestr.find("get")==string::npos && fnamestr.find("l2cap_")==string::npos) && !(fnamestr.find("security_")==0)){
      to_remove.insert(f);
    }
    StringRef fn = f->getName();
    if(fn.startswith("print"))//fn.startswith("has_capability_noaudit") || fn.startswith("print"))
      to_remove.insert(f);
  }
  
  for(auto f : to_remove){
    if(!(f->getName().startswith("ns_capable_setid")) && !(f->getName().startswith("security_"))) 
      fs.erase(f);
  }
  // for(auto f: CAPFuncs)
  // FunctionSet::::iterator it;
  // for(it=fs.begin(); it!=fs.end(); it++){
  //   StringRef fn = (*it)->getName();
  //   if(fn.startswith("security") || fn.startswith("printk") || fn.startswith("has_capability_noaudit"))
  //     fs.erase(it);
  // }
  dump();

  //lsm
  /*
  detectLSM(module);
  
  //fs.insert(wrappers.begin(), wrappers.end());

  errs() << "\n\nthe functions that not found before:\n";
  for (auto f : fs_result) {
    if (!fs.count(f))
      errs() << f->getName() << "\n";
  }
  //fs.insert(fs_result.begin(), fs_result.end());
*/

  errs() << "--------------------end dump-----------------\n\n\n";

}

bool GatingNew::is_gating_function(Function *f) { 
  return fs.count(f);
}

bool GatingNew::is_gating_function(std::string &str) {
  for (auto f: fs) {
    if (f->getName() == str)
      return true;
  }
  return false;
}

void GatingNew::dump() {
  errs() << ANSI_COLOR(BG_BLUE, FG_WHITE)
         << "=permission chk functions and inners (total: "
         << fs.size() << ")=" << ANSI_COLOR_RESET << "\n";
  for (auto f: fs) {
    errs() << f->getName() << "\n";
  }
  errs() << "=o=\n";
}

void GatingFunctionBase::dump_interesting(InstructionSet *cis) {
  for (auto *ci : *cis) {
    CallInst *cs = dyn_cast<CallInst>(ci);
    Function *cf = get_callee_function_direct(cs);
    if (is_gating_function(cf)) {
      errs() << "    " << cf->getName() << " @ ";
      cs->getDebugLoc().print(errs());
      errs() << "\n";
    }
  }
}









////////////////////////////////////////////////////////////////////////////////
// GatingCap

void GatingCap::load_cap_func_list(std::string &file) {
  std::ifstream input(file);
  if (!input.is_open()) {
    // TODO:load builtin list into cap_func_name2cap_arg_pos
    for (int i = 0; i < BUILTIN_CAP_FUNC_LIST_SIZE; i++) {
      const struct str2int *p = &_builtin_cap_functions[i];
      cap_func_name2cap_arg_pos[p->k] = p->v;
    }
    return;
  }
  std::string line;
  while (std::getline(input, line)) {
    std::size_t found = line.find(" ");
    assert(found != std::string::npos);
    std::string name = line.substr(0, found);
    int pos = stoi(line.substr(found + 1));
    cap_func_name2cap_arg_pos[name] = pos;
  }
  input.close();
  errs() << "Load CAP FUNC list, total:" << cap_func_name2cap_arg_pos.size()
         << "\n";
}

bool GatingCap::is_builtin_gatlin_function(const std::string &str) {
  for (int i = 0; i < BUILTIN_CAP_FUNC_LIST_SIZE; i++) {
    const struct str2int *p = &_builtin_cap_functions[i];
    if (p->k == str)
      return true;
  }
  return false;
}

GatingCap::GatingCap(Module &module, std::string &capfile)
    : GatingFunctionBase(module) {
  errs() << "Gating Function Type: capability\n";
  load_cap_func_list(capfile);
  // add capable and ns_capable to chk_func_cap_position so that we can use them
  for (Module::iterator fi = module.begin(), f_end = module.end(); fi != f_end;
       ++fi) {
    Function *func = dyn_cast<Function>(fi);
    StringRef fname = func->getName();

    if (cap_func_name2cap_arg_pos.find(fname.str()) !=
        cap_func_name2cap_arg_pos.end()) {
      chk_func_cap_position[func] = cap_func_name2cap_arg_pos[fname.str()];
      if (chk_func_cap_position.size() == cap_func_name2cap_arg_pos.size())
        break; // we are done here
    }
  }

  // last time discovered functions
  FunctionData pass_data;
  // functions that will be used for discovery next time
  FunctionData pass_data_next;

  for (auto fpair : chk_func_cap_position)
    pass_data[fpair.first] = fpair.second;

  /*
   * First round:
   * discover inner permission check functions and add them all as basic
   * permission check functions
   */
  for (auto fpair : pass_data) {
    Function *f = fpair.first;
    int cap_pos = fpair.second;
    assert(cap_pos >= 0);
    // discover call instructions which use the parameter directly
    for (Function::iterator fi = f->begin(), fe = f->end(); fi != fe; ++fi) {
      BasicBlock *bb = dyn_cast<BasicBlock>(fi);
      for (BasicBlock::iterator ii = bb->begin(), ie = bb->end(); ii != ie;
           ++ii) {
        CallInst *ci = dyn_cast<CallInst>(ii);
        if (!ci)
          continue;
        // we are expecting a direct call
        Function *child = get_callee_function_direct(ci);
        if (!child)
          continue;
        StringRef fname = child->getName();
        // dont bother if this belongs to skip function
        if (skip_funcs->exists_ignore_dot_number(fname.str()) ||
            kernel_api->exists_ignore_dot_number(fname.str()))
          continue;

        // for each of the function argument
        for (unsigned int i = 0; i < ci->getNumArgOperands(); ++i) {
          Value *capv = ci->getArgOperand(i);
          int pos = use_parent_func_arg(capv, f);
          if (pos == cap_pos) {
            pass_data_next[child] = i;
            break;
          }
        }
      }
    }
  }
  // merge result of first round
  if (pass_data_next.size()) {
    errs() << ANSI_COLOR(BG_WHITE, FG_RED)
           << "Inner checking functions:" << ANSI_COLOR_RESET << "\n";
    for (auto fpair : pass_data_next) {
      Function *f = fpair.first;
      int cap_pos = fpair.second;
      errs() << "    - " << f->getName() << " @ " << cap_pos << "\n";
      pass_data[f] = cap_pos;
    }
    pass_data_next.clear();
  }
  // backward discovery and mark..
again:
  for (auto fpair : pass_data) {
    Function *func = fpair.first;
    int cap_pos = fpair.second;
    assert(cap_pos >= 0);
    // we got a capability check function or a wrapper function,
    // find all use without Constant Value and add them to wrapper
    for (auto u : func->users()) {
      InstructionSet uis;
      if (Instruction *i = dyn_cast<Instruction>(u))
        uis.insert(i);
      else
        uis = get_user_instruction(dyn_cast<Value>(u));
      if (uis.size() == 0) {
        u->print(errs());
        errs() << "\n";
        continue;
      }
      for (auto ui : uis) {
        CallInst *cs = dyn_cast<CallInst>(ui);
        if (cs == NULL)
          continue; // how come?
        Function *callee = get_callee_function_direct(cs);
        if (callee != func)
          continue;
        // assert(cs->getCalledFunction()==func);
        if (cap_pos >= (int)cs->getNumArgOperands()) {
          func->print(errs());
          llvm_unreachable(ANSI_COLOR_RED
                           "Check capability parameter" ANSI_COLOR_RESET);
        }
        Value *capv = cs->getArgOperand(cap_pos);
        if (isa<ConstantInt>(capv))
          continue;
        Function *parent_func = cs->getFunction();
        // we have a wrapper,
        int pos = use_parent_func_arg(capv, parent_func);
        if (pos >= 0) {
          // type 1 wrapper, cap is from parent function argument
          pass_data_next[parent_func] = pos;
        } else {
          // type 2 wrapper, cap is from inside this function
          // what to do with this?
          // llvm_unreachable("What??");
        }
      }
    }
  }
  // put pass_data_next in pass_data and chk_func_cap_position
  pass_data.clear();
  for (auto fpair : pass_data_next) {
    Function *f = fpair.first;
    int pos = fpair.second;
    if (chk_func_cap_position.count(f) == 0) {
      pass_data[f] = pos;
      chk_func_cap_position[f] = pos;
    }
  }
  // do this until we discovered everything
  if (pass_data.size())
    goto again;
}

bool GatingCap::is_gating_function(Function *f) {
  return chk_func_cap_position.find(f) != chk_func_cap_position.end();
}

bool GatingCap::is_gating_function(std::string &str) {
  for (auto &f2p : chk_func_cap_position) {
    if (f2p.first->getName() == str)
      return true;
  }
  return false;
}

void GatingCap::dump() {
  errs() << ANSI_COLOR(BG_BLUE, FG_WHITE)
         << "=chk functions and wrappers (total:"
         << chk_func_cap_position.size() << ")=" << ANSI_COLOR_RESET << "\n";
  for (auto &f2p : chk_func_cap_position) {
    errs() << ". " << f2p.first->getName() << "  @ " << f2p.second << "\n";
  }
  errs() << "=o=\n";
}

void GatingCap::dump_interesting(InstructionSet *cis) {
  int last_cap_no = -1;
  bool mismatched_chk_func = false;
  Function *last_cap_chk_func = NULL;
  for (auto *ci : *cis) {
    CallInst *cs = dyn_cast<CallInst>(ci);
    Function *cf = get_callee_function_direct(cs);
    int cap_no = -1;
    if (is_gating_function(cf)) {
      cap_no = chk_func_cap_position[cf];
      Value *capv = cs->getArgOperand(cap_no);
      if (!isa<ConstantInt>(capv)) {
        cs->getDebugLoc().print(errs());
        errs() << "\n";
        cs->print(errs());
        errs() << "\n";
        // llvm_unreachable("expect ConstantInt in capable");
        errs() << "Dynamic Load CAP\n";
        cs->getDebugLoc().print(errs());
        errs() << "\n";
        continue;
      }
      cap_no = dyn_cast<ConstantInt>(capv)->getSExtValue();
      if (last_cap_no == -1) {
        last_cap_no = cap_no;
        last_cap_chk_func = cf;
      }
      if (last_cap_no != cap_no)
        last_cap_no = -2;
      if (last_cap_chk_func != cf)
        mismatched_chk_func = true;
    }
    if (!((cap_no >= CAP_CHOWN) && (cap_no <= CAP_LAST_CAP))) {
      cs->print(errs());
      errs() << "\n";
      errs() << "cap_no=" << cap_no << "\n";
      cs->getDebugLoc().print(errs());
      errs() << "\n";
      // assert((cap_no>=CAP_CHOWN) && (cap_no<=CAP_LAST_CAP));
      continue;
    }
    errs() << "    " << cap2string[cap_no] << " @ " << cf->getName() << " ";
    cs->getDebugLoc().print(errs());
    errs() << "\n";
  }
  if ((last_cap_no == -2) || (mismatched_chk_func))
    errs() << ANSI_COLOR_RED << "inconsistent check" << ANSI_COLOR_RESET
           << "\n";
}

////////////////////////////////////////////////////////////////////////////////
// LSM

void GatingLSM::load_lsm_hook_list(std::string &file) {
  std::ifstream input(file);
  if (!input.is_open())
    return;
  std::string line;
  while (std::getline(input, line))
    lsm_hook_names.insert(line);
  input.close();
  errs() << "Load LSM hook list, total:" << lsm_hook_names.size() << "\n";
}

bool GatingLSM::is_lsm_hook(StringRef &str) {
  if (lsm_hook_names.size()) {
    return lsm_hook_names.find(str.str()) != lsm_hook_names.end();
  }
  // use builtin name
  if (str.startswith("security_"))
    return true;
  return false;
}

GatingLSM::GatingLSM(Module &module, std::string &lsmfile)
    : GatingFunctionBase(module) {
  errs() << "Gating Function Type: LSM\n";
  load_lsm_hook_list(lsmfile);
  for (Module::iterator fi = module.begin(), f_end = module.end(); fi != f_end;
       ++fi) {
    Function *func = dyn_cast<Function>(fi);
    StringRef fname = func->getName();
    if (is_lsm_hook(fname)) {
      lsm_hook_functions.insert(func);
    }
  }

  // also try to discover wrapper function for LSM hooks
  FunctionSet wrappers;
  int loop_cnt = 0;
again:
  for (auto *lsmh : lsm_hook_functions) {
    errs() << " lsmh - " << lsmh->getName() << "\n";
    for (auto *u : lsmh->users()) {
      // should be call instruction and the callee is dacf
      InstructionSet uis;
      if (Instruction *i = dyn_cast<Instruction>(u))
        uis.insert(i);
      else
        uis = get_user_instruction(dyn_cast<Value>(u));
      if (uis.size() == 0) {
        u->print(errs());
        errs() << "\n";
        continue;
      }
      for (auto ui : uis) {
        CallInst *ci = dyn_cast<CallInst>(ui);
        if (!ci)
          continue;
        Function *callee = get_callee_function_direct(ci);
        if (callee != lsmh)
          continue;
        Function *userf = ci->getFunction();
        errs() << "    used by - " << userf->getName() << "\n";
        // parameters comes from wrapper's parameter?
        for (unsigned int i = 0; i < ci->getNumOperands(); i++) {
          Value *a = ci->getOperand(i);
          if (use_parent_func_arg_deep(a, userf) >= 0) {
            wrappers.insert(userf);
            break;
          }
        }
      }
    }
  }
  if (wrappers.size()) {
    for (auto *wf : wrappers)
      lsm_hook_functions.insert(wf);
    wrappers.clear();
    loop_cnt++;
    if (loop_cnt < 1)
      goto again;
  }
}

bool GatingLSM::is_gating_function(Function *f) {
  return lsm_hook_functions.find(f) != lsm_hook_functions.end();
}

bool GatingLSM::is_gating_function(std::string &str) {
  for (auto f : lsm_hook_functions) {
    if (f->getName() == str)
      return true;
  }
  return false;
}

void GatingLSM::dump() {
  errs() << ANSI_COLOR(BG_BLUE, FG_WHITE)
         << "=LSM hook functions (total:" << lsm_hook_functions.size()
         << ")=" << ANSI_COLOR_RESET << "\n";
  for (auto f : lsm_hook_functions) {
    errs() << ". " << f->getName() << "\n";
  }
  errs() << "=o=\n";
}

////////////////////////////////////////////////////////////////////////////////
// DAC
GatingDAC::GatingDAC(Module &module) : GatingFunctionBase(module) {
  errs() << "Gating Function Type: DAC\n";
  for (Module::iterator fi = module.begin(), f_end = module.end(); fi != f_end;
       ++fi) {
    Function *func = dyn_cast<Function>(fi);
    StringRef fname = func->getName();
    if ((fname == "posix_acl_permission") || (fname == "check_acl") ||
        (fname == "acl_permission_check") || (fname == "generic_permission") ||
        (fname == "sb_permission") || (fname == "inode_permission")) {
      dac_functions.insert(func);
    }
    if (dac_functions.size() == 6)
      break; // we are done here
  }
#if 1
  // discover wrapper
  // for all user of dac function, find whether the parameter comes from
  // out layer wrapper parameter?
  // inline function may cause problem here...
  FunctionSet wrappers;
  int loop_cnt = 0;
again:
  for (auto *dacf : dac_functions) {
    errs() << " dacf - " << dacf->getName() << "\n";
    for (auto *u : dacf->users()) {
      // should be call instruction and the callee is dacf
      InstructionSet uis;
      if (Instruction *i = dyn_cast<Instruction>(u))
        uis.insert(i);
      else
        uis = get_user_instruction(dyn_cast<Value>(u));
      if (uis.size() == 0) {
        u->print(errs());
        errs() << "\n";
        continue;
      }
      for (auto ui : uis) {
        CallInst *ci = dyn_cast<CallInst>(ui);
        if (!ci)
          continue;
        Function *callee = get_callee_function_direct(ci);
        if (callee != dacf)
          continue;
        Function *userf = ci->getFunction();
        errs() << "    used by - " << userf->getName() << "\n";
        // parameters comes from wrapper's parameter?
        for (unsigned int i = 0; i < ci->getNumOperands(); i++) {
          Value *a = ci->getOperand(i);
          if (use_parent_func_arg_deep(a, userf) >= 0) {
            wrappers.insert(userf);
            break;
          }
        }
      }
    }
  }
  if (wrappers.size()) {
    for (auto *wf : wrappers)
      dac_functions.insert(wf);
    wrappers.clear();
    loop_cnt++;
    if (loop_cnt < 1)
      goto again;
  }
#endif
}

bool GatingDAC::is_gating_function(Function *f) {
  return dac_functions.find(f) != dac_functions.end();
}

bool GatingDAC::is_gating_function(std::string &str) {
  for (auto &f : dac_functions) {
    if (f->getName() == str)
      return true;
  }
  return false;
}

void GatingDAC::dump() {
  errs() << ANSI_COLOR(BG_BLUE, FG_WHITE)
         << "=chk functions and wrappers (total:" << dac_functions.size()
         << ")=" << ANSI_COLOR_RESET << "\n";
  for (auto &f : dac_functions) {
    errs() << ". " << f->getName() << "\n";
  }
  errs() << "=o=\n";
}
