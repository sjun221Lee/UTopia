#include "ftg/propanalysis/ArgFlowAnalyzer.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include <list>

using namespace llvm;

namespace ftg {

ArgFlowAnalyzer::ArgFlowAnalyzer(std::shared_ptr<IndCallSolver> Solver,
                                 const std::vector<const Function *> &Funcs)
    : Solver(Solver) {}

void ArgFlowAnalyzer::analyze(const Argument &A) {
  auto &AF = getOrCreateArgFlow(*const_cast<Argument *>(&A));
  if (AF.State != AnalysisState_Not_Analyzed)
    return;

  AF.setState(AnalysisState_Analyzing);
  analyzeProperty(*const_cast<Argument *>(&A));
  AF.setState(AnalysisState_Analyzed);
}

const std::map<Argument *, std::shared_ptr<ArgFlow>>
ArgFlowAnalyzer::getArgFlowMap() const {
  return ArgFlowMap;
}

void ArgFlowAnalyzer::analyze(const std::vector<const Function *> &Funcs) {
  for (const auto *Func : Funcs) {
    if (!Func)
      continue;

    for (const auto &Arg : Func->args())
      analyze(Arg);
  }
}

ArrayType *ArgFlowAnalyzer::getAsArrayType(Value &V) const {
  auto *T = V.getType();
  if (!T)
    return nullptr;

  while (T->isPointerTy())
    T = dyn_cast<PointerType>(T)->getElementType();

  if (T->isArrayTy())
    return dyn_cast<llvm::ArrayType>(T);

  return nullptr;
}

StructType *ArgFlowAnalyzer::getAsStructType(Value &V) const {
  auto *T = V.getType();
  if (!T)
    return nullptr;

  while (T->isPointerTy() || T->isArrayTy()) {
    if (isa<PointerType>(T))
      T = T->getPointerElementType();
    else
      T = T->getArrayElementType();
  }
  if (T->isStructTy() && T->getStructName().split('.').first == "struct")
    return dyn_cast<llvm::StructType>(T);
  return nullptr;
}

Function *ArgFlowAnalyzer::getCalledFunction(CallBase &CB) const {
  auto *CV = CB.getCalledValue();
  if (CV) {
    auto *F = dyn_cast_or_null<Function>(CV->stripPointerCasts());
    if (F)
      return F;
  }

  if (!Solver)
    return nullptr;

  return Solver->getCalledFunction(CB);
}

ArgFlow &ArgFlowAnalyzer::getOrCreateArgFlow(Argument &A) {
  if (ArgFlowMap.find(&A) == ArgFlowMap.end())
    ArgFlowMap[&A] = std::make_shared<ArgFlow>(A);

  const auto *F = A.getParent();
  assert(F && "Unexpected Program State");

  auto &Result = ArgFlowMap[&A];
  assert(Result && "Unexpected Program State");

  return *Result;
}

// TODO: Exception-catching related functions
// ref: lib/Target/WebAssembly/WebAssemblyLowerEmscriptenEHSjLj.cpp
// Function *BeginCatchF = M.getFunction("__cxa_begin_catch");
// Function *EndCatchF = M.getFunction("__cxa_end_catch");
// Function *AllocExceptionF = M.getFunction("__cxa_allocate_exception");
// Function *ThrowF = M.getFunction("__cxa_throw");
// Function *TerminateF = M.getFunction("__clang_call_terminate");
bool ArgFlowAnalyzer::mayThrow(const BasicBlock &BB) const {
  for (const auto &I : BB) {
    const auto *CB = dyn_cast<CallBase>(&I);
    if (!CB)
      continue;

    const auto *CV = CB->getCalledValue();
    const auto *F = dyn_cast_or_null<Function>(CV->stripPointerCasts());
    if (!F || F->getName() != "__cxa_allocate_exception")
      continue;

    return true;
  }
  return false;
}

} // namespace ftg
