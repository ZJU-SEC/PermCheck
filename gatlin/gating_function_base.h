/*
 * Gating Function
 * extend this class to identify other types of checker
 * 2018 Tong Zhang<t.zhang2@partner.samsung.com>
 */
#ifndef _GATING_FUNCTION_BASE_
#define _GATING_FUNCTION_BASE_

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IntrinsicInst.h"

#include "aux.h"
#include "commontypes.h"
#include "internal.h"

using namespace llvm;

class GatingFunctionBase {
protected:
  Module &m;

public:
  GatingFunctionBase(Module &_m) : m(_m){};
  virtual ~GatingFunctionBase(){};
  virtual bool is_gating_function(Function *) { return false; };
  virtual bool is_gating_function(std::string &) { return false; };
  virtual void dump(){};
  virtual void dump_interesting(InstructionSet *);
};


class GatingNew : public GatingFunctionBase
{
protected:
  FunctionSet CAPFuncs;
  FunctionSet DACFuncs;
  FunctionSet CAPInners;
  FunctionSet DACInners;
  // we need to trace the gv because it (or its field) has the macro
  GVIS    CAP_gvis; 
  GVIS    DAC_gvis;
  FunctionSet fs;

  FunctionData CAPWrapper;
  FunctionData DACWrapper;
  FunctionData CAPBasic;
  FunctionData DACBasic;
  // lsm
  FunctionSet fs_result;
  FunctionSet wrappers;


private:
  void resolveInitializer(GlobalVariable *gv, bool (*isMacro)(Value *v), GVIS *gvis);
  //void traceGVI();
  void handleStoreInst(StoreInst *si, FunctionSet *fs);
  Function* handleCallInst(CallInst *ci);
  void processInstructions(Instruction *ii, bool (*isMacro)(Value *v), FunctionSet *fs);
  void traceMask(Value* mask, bool (*isMacro)(Value *v));
  void findInnerFuncs(Function *f, unsigned int cap_pos, bool (*isMacro)(Value *v));
  void add2Basic(bool (*isMacro)(Value *v), Function *f, int arg);
  void traceMaskArgument(bool (*isMacro)(Value *v), Instruction * inst);
  void detectWrapper(FunctionData basic, FunctionData &wrapper);
  void summary(FunctionSet &fs, FunctionSet inner, FunctionData wrapper);
  void print_fs_result();
  void detectLSM(Module& module);
  void findSecurity(Module& module);


public:
  GatingNew(Module &);
  ~GatingNew(){};
  virtual bool is_gating_function(Function *);
  virtual bool is_gating_function(std::string &);
  virtual void dump();
  //virtual void dump_interesting(InstructionSet *);
};



class GatingCap : public GatingFunctionBase {
protected:
  /*
   * record capability parameter position passed to capability check function
   * all discovered wrapper function to check functions will also have one entry
   *
   * This data is available after calling collect_wrappers()
   */
  FunctionData chk_func_cap_position;
  Str2Int cap_func_name2cap_arg_pos;

private:
  void load_cap_func_list(std::string &);
  bool is_builtin_gatlin_function(const std::string &);

public:
  GatingCap(Module &, std::string &);
  ~GatingCap(){};
  virtual bool is_gating_function(Function *);
  virtual bool is_gating_function(std::string &);
  virtual void dump();
  virtual void dump_interesting(InstructionSet *);
};

class GatingLSM : public GatingFunctionBase {
protected:
  FunctionSet lsm_hook_functions;
  StringSet lsm_hook_names;

private:
  void load_lsm_hook_list(std::string &);
  bool is_lsm_hook(StringRef &);

public:
  GatingLSM(Module &, std::string &);
  ~GatingLSM(){};
  virtual bool is_gating_function(Function *);
  virtual bool is_gating_function(std::string &);
  virtual void dump();
};

class GatingDAC : public GatingFunctionBase {
protected:
  FunctionSet dac_functions;

private:
  bool is_dac_function(StringRef &);

public:
  GatingDAC(Module &);
  ~GatingDAC(){};
  virtual bool is_gating_function(Function *);
  virtual bool is_gating_function(std::string &);
  virtual void dump();
};

#endif //_GATING_FUNCTION_BASE_
