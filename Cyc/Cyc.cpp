/*
** Created By cyachen
*/

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/IR/DerivedTypes.h"
#include <map>
#include <vector>
using namespace llvm;

namespace
{
class CycPass : public ModulePass
{
  public:
	virtual bool runOnModule(Module &module) override;
	static char ID;
	CycPass() : ModulePass(ID) {}

  private:
	void initVar(Module &);

	void handleInitMain(Instruction *);
	void handleMalloc(CallInst *, Instruction *);
	void handleFree(CallInst *, Instruction *);
	void handleStackAlloc(AllocaInst *, Instruction *);
	void handleLoad(LoadInst *, Instruction *);
	void handleStore(StoreInst *, Instruction *);

	void handleBitCast(BitCastInst *);
	void handleFunctionCall(CallInst *, Instruction *);
	void handleFunctionArg(Function &, Instruction *);
	void handleReturn(ReturnInst *);

	Value *findKeyOnMap(std::map<Value *, Value *>, Value *);
	Value *getStackAddr(Value *, Instruction *);
	void idPropagate(Value *, Value *);
	Value *getValueId(Value *, Instruction *);

	void objdump(Module &);
	Function *f_test_func_insert;
	Function *f_cyc_uafcheck_init;

	Function *f_cyc_handle_malloc;
	Function *f_cyc_handle_free;
	Function *f_cyc_handle_stack_alloc;
	Function *f_cyc_handle_store_heap_addr;
	Function *f_cyc_handle_store_stack_addr;
	Function *f_cyc_handle_dereference;

	Function *f_cyc_id_arg_stack_push;
	Function *f_cyc_id_arg_stack_pop;

	Function *f_cyc_id_return_stack_push;
	Function *f_cyc_id_return_stack_pop;

	Function *f_cyc_is_id_valid;

	Value *v_cyc_id_cur_stack;

	Type *int8Ty;
	Type *int32Ty;
	Type *int64Ty;
	Type *voidPtrTy;
	Type *voidTy;

	std::map<Value *, Value *> mapObject_Key;
	std::map<Value *, Value *> mapAllocaObject_Addr;
};
} // namespace

///register pass
char CycPass::ID = 0;
static void registerCycPass(const PassManagerBuilder &,
							legacy::PassManagerBase &PM)
{
	PM.add(new CycPass());
}
static RegisterStandardPasses
	RegisterMyPass(PassManagerBuilder::EP_ModuleOptimizerEarly,
				   registerCycPass);
static RegisterStandardPasses
	RegisterMyPass0(PassManagerBuilder::EP_EnabledOnOptLevel0, registerCycPass);

void CycPass::handleMalloc(CallInst *mallocCall, Instruction *insertBefore)
{
	errs() << "malloc : ";

	CallInst *handleMallocCall = CallInst::Create(f_cyc_handle_malloc, "", insertBefore);

	mapObject_Key[mallocCall] = handleMallocCall;
}

void CycPass::handleFree(CallInst *freeCall, Instruction *insertBefore)
{
	Function *callee = freeCall->getCalledFunction();
	Function::arg_iterator arg_it = callee->arg_begin();

	Value *arg = freeCall->getOperand(0);
	errs() << "--------------------free call-------------------" << arg->getName() << "\n";
	//get id
	Value *argId = findKeyOnMap(mapObject_Key, arg);
	if (argId != nullptr)
	{
		SmallVector<Value *, 1> args;
		args.push_back(argId);
		CallInst *handleFreeCall = CallInst::Create(f_cyc_handle_free, args, "", insertBefore);
	}
	else
	{
		errs() << "-------------not find in free\n";
	}
	n
}

void CycPass::handleLoad(LoadInst *loadInst, Instruction *insertBefore)
{
	Value *pointerOp = loadInst->getPointerOperand();
	Type *pointerOpTy = loadInst->getPointerOperandType();

	Value *firstOp = loadInst;
	Type *firstOpTy = firstOp->getType();
	SmallVector<Value *, 1> args;
	//is p valid
	if (findKeyOnMap(mapAllocaObject_Addr, pointerOp) == nullptr && pointerOpTy->isPointerTy())
	{
		//tmp pointer ,check id is valid
		Value *firstOpId = findKeyOnMap(mapObject_Key, firstOp);
		if (firstOpId != nullptr)
		{
			args.push_back(firstOpId);
			CallInst *idCheckCall = CallInst::Create(f_cyc_is_id_valid, args, "", insertBefore);
		}
	}

	//check if 1 is pointer
	if (firstOpTy->isPointerTy())
	{
		//id propagate
		Value *pointerOpAddr = getStackAddr(pointerOp, insertBefore);
		args.push_back(pointerOpAddr);
		CallInst *pointerOpId = CallInst::Create(f_cyc_handle_dereference, args, "", insertBefore);

		mapObject_Key[loadInst] = pointerOpId;
	}
	else
	{
	}
}

void CycPass::handleStore(StoreInst *storeInst, Instruction *insertBefore)
{
	Value *pointerOp = storeInst->getPointerOperand();
	Type *pointerOpTy = storeInst->getPointerOperandType();

	Value *firstOp = storeInst->getOperand(0);
	Type *firstOpTy = firstOp->getType();

	if (pointerOpTy->isPointerTy())
	{
		if (firstOpTy->isPointerTy())
		{
			//get pointerOP addr
			Value *pointerOpAddr = getStackAddr(pointerOp, insertBefore);

			std::vector<Value *> args;
			args.push_back(pointerOpAddr);
			//is firstop on stack
			if (findKeyOnMap(mapAllocaObject_Addr, firstOp))
			{
				CallInst *storeStackAddr = CallInst::Create(f_cyc_handle_store_stack_addr, args, "", insertBefore);
			}
			else
			{
				Value *firstOpId = findKeyOnMap(mapObject_Key, firstOp);
				if (firstOpId != nullptr)
				{
					args.push_back(firstOpId);
					CallInst *storeHeapAddr = CallInst::Create(f_cyc_handle_store_heap_addr, args, "", insertBefore);
				}
			}
		}
		else
		{
			// first is not pointer
			if (findKeyOnMap(mapAllocaObject_Addr, pointerOp) == nullptr)
			{
				//tmp pointer
				errs() << "not on stack p ----------------";
				storeInst->dump();
				Value *pointerOpId = findKeyOnMap(mapObject_Key, pointerOp);
				if (pointerOpId != nullptr)
				{
					SmallVector<Value *, 1> args;
					args.push_back(pointerOpId);

					Instruction *idCall = CallInst::Create(f_cyc_is_id_valid, args, "", storeInst);
				}
			}
		}
	}
}

void CycPass::handleStackAlloc(AllocaInst *allocaInst, Instruction *insertBefore)
{
	errs() << "alloc: ";
	Instruction *getAddrInst = new PtrToIntInst((allocaInst), int64Ty, "", insertBefore);

	mapAllocaObject_Addr[allocaInst] = getAddrInst;
}

void CycPass::handleBitCast(BitCastInst *bitcastInst)
{
	errs() << "----------------------------------";
	Value *pointerOp = bitcastInst->getOperand(0);
	if (pointerOp->getType()->isPointerTy())
	{
		errs() << pointerOp->getName() << "-------------------------";
		idPropagate(pointerOp, bitcastInst);
	}
}

void CycPass::handleFunctionCall(CallInst *callInst, Instruction *insertBefore)
{
	Function *callee = callInst->getCalledFunction();
	Type *returnTy = callee->getReturnType();
	for (Function::arg_iterator callee_argS = callee->arg_begin(), callee_argE = callee->arg_end(); callee_argS != callee_argE; ++callee_argS)
	{
		Value *callee_arg = &(*callee_argS);
		if (callee_arg->getType()->isPointerTy())
		{
			errs() << "---------------has pointer************\n";
			Value *argId = getValueId(callee_arg, callInst);
			SmallVector<Value *, 1> args;
			args.push_back(argId);
			CallInst *pushId = CallInst::Create(f_cyc_id_arg_stack_push, args, "", callInst);
		}
	}
	if (returnTy->isPointerTy())
	{
		Value *returnId = CallInst::Create(f_cyc_id_return_stack_pop, "", insertBefore);
		mapObject_Key[callInst] = returnId;
		// //return pointer
		// Value *returnAddr=getStackAddr(callInst,insertBefore,inst_it);
		// SmallVector<Value *,1> args;
		// args.push_back(returnAddr);
		// CallInst *justhhh=CallInst::Create(f_cyc_handle_stack_alloc,args,"",insertBefore);
	}
}

void handleReturn(ReturnInst *returnInst)
{
	if ()
}

void CycPass::handleInitMain(Instruction *inst)
{
	CallInst *initInst = CallInst::Create(f_cyc_uafcheck_init, "", inst);
	// inst_it++;
}

Value *CycPass::findKeyOnMap(std::map<Value *, Value *> tmpMap, Value *value)
{
	std::map<Value *, Value *>::iterator it;
	it = tmpMap.find(value);
	if (it == tmpMap.end())
	{
		return nullptr;
	}
	else
	{
		return it->second;
	}
}

Value *CycPass::getStackAddr(Value *value, Instruction *insertBefore)
{
	Value *addr = findKeyOnMap(mapAllocaObject_Addr, value);
	if (addr == nullptr)
	{
		addr = new PtrToIntInst(value, int64Ty, "", insertBefore);
	}
	return addr;
}

void CycPass::idPropagate(Value *from, Value *to)
{
	if (findKeyOnMap(mapAllocaObject_Addr, from) == nullptr)
	{
		//tmp pointer
		Value *tmpPointerkey = findKeyOnMap(mapObject_Key, from);
		if (tmpPointerkey != nullptr)
		{
			mapObject_Key[to] = tmpPointerkey;
		}
		else
		{
			errs() << "-------------not find in bitcast\n";
		}
	}
	else
	{
		//stack pointer
		mapObject_Key[to] = v_cyc_id_cur_stack;
	}
}

Value *CycPass::getValueId(Value *value, Instruction *insertBefore)
{
	Value *valueId;
	if (findKeyOnMap(mapAllocaObject_Addr, value) != nullptr)
	{
		Value *valueAddr = getStackAddr(value, insertBefore);
		SmallVector<Value *, 1> args;
		args.push_back(valueAddr);

		valueId = CallInst::Create(f_cyc_handle_dereference, args, "", insertBefore);
	}
	else
	{
		valueId = findKeyOnMap(mapObject_Key, value);
	}
	return valueId;
}

void CycPass::objdump(Module &module)
{
	errs() << "I saw a module called " << module.getName() << "!\n";
	for (auto &function : module)
	{
		errs() << "I saw a function called " << function.getName() << "!\n";
		for (auto &bb : function)
		{
			for (auto &inst : bb)
			{
				inst.dump();
			}
		}
	}
}
bool CycPass::runOnModule(Module &module)
{
	initVar(module);
	for (auto &function : module)
	{
		mapObject_Key.clear();
		mapAllocaObject_Addr.clear();
		//for (auto &bb : function) {
		inst_iterator inst = inst_begin(function);
		if (function.getName().equals("main"))
		{
			handleInitMain(&(*inst));
		}
		for (inst_iterator instE = inst_end(function); inst != instE;)
		{
			(*inst).dump();
			inst_iterator inst_next = inst;
			inst_next++;
			if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(&(*inst)))
			{
				handleStackAlloc(allocaInst, &(*(inst_next)));
			}
			// is callinst
			else if (CallInst *callInst = dyn_cast<CallInst>(&(*inst)))
			{
				Function *callee = callInst->getCalledFunction();
				if (callee != nullptr)
				{
					StringRef FnName = callee->getName();
					if (FnName.equals("free"))
					{
						handleFree(callInst, &(*(inst_next)));
					}
					else if (FnName.equals("malloc"))
					{
						handleMalloc(callInst, &(*(inst_next)));
					}
					else
					{
						errs() << "funcCall :";
						//ignore llvm func
						if (FnName.startswith("llvm.") || FnName.startswith("cyc_"))
						{
							errs() << "ok==========";
						}
						else
						{
							handleFunctionCall(callInst, &(*(inst_next)));
						}
					}
				}
			}
			// is load/store
			else if (LoadInst *loadInst = dyn_cast<LoadInst>(&(*inst)))
			{
				Type *type = loadInst->getPointerOperandType();
				if (type->isPointerTy())
				{
					handleLoad(loadInst, &(*(inst_next)));
				}
			}
			else if (StoreInst *storeInst = dyn_cast<StoreInst>(&(*inst)))
			{
				handleStore(storeInst, &(*(inst_next)));
			}
			else if (BitCastInst *bitcastInst = dyn_cast<BitCastInst>(&(*inst)))
			{
				handleBitCast(bitcastInst);
			}
			else if (ReturnInst *returnInst = dyn_cast<ReturnInst>(&(*inst)))
			{
				handleReturn(returnInst);
			}
			inst = inst_next;
		}
	}

	objdump(module);
	return true;
}

void CycPass::initVar(Module &module)
{
	LLVMContext &Ctx = module.getContext();
	int8Ty = Type::getInt8Ty(Ctx);
	int32Ty = Type::getInt32Ty(Ctx);
	int64Ty = Type::getInt64Ty(Ctx);
	voidPtrTy = Type::getInt8PtrTy(Ctx);
	voidTy = Type::getVoidTy(Ctx);

	Constant *cf = module.getOrInsertFunction("cyc_test_func_insert",
											  Type::getVoidTy(Ctx));
	f_test_func_insert = cast<Function>(cf);

	SmallVector<Type *, 1> arg1Ty;
	arg1Ty.push_back(int64Ty);

	SmallVector<Type *, 2> arg2Ty;
	arg2Ty.push_back(int64Ty);
	arg2Ty.push_back(int64Ty);

	cf = module.getOrInsertFunction("cyc_uafcheck_init", Type::getVoidTy(Ctx));
	f_cyc_uafcheck_init = cast<Function>(cf);

	cf = module.getOrInsertFunction("cyc_handle_malloc", int64Ty);
	f_cyc_handle_malloc = cast<Function>(cf);

	cf = module.getOrInsertFunction("cyc_handle_free", FunctionType::get(voidTy, arg1Ty, false));
	f_cyc_handle_free = cast<Function>(cf);

	cf = module.getOrInsertFunction("cyc_handle_stack_alloc", FunctionType::get(voidTy, arg1Ty, false));
	f_cyc_handle_stack_alloc = cast<Function>(cf);

	cf = module.getOrInsertFunction("cyc_handle_store_stack_addr", FunctionType::get(voidTy, arg1Ty, false));
	f_cyc_handle_store_stack_addr = cast<Function>(cf);

	cf = module.getOrInsertFunction("cyc_handle_store_heap_addr", FunctionType::get(voidTy, arg2Ty, false));
	f_cyc_handle_store_heap_addr = cast<Function>(cf);

	cf = module.getOrInsertFunction("cyc_handle_dereference", FunctionType::get(int64Ty, arg1Ty, false));
	f_cyc_handle_dereference = cast<Function>(cf);

	cf = module.getOrInsertFunction("cyc_id_arg_stack_push", FunctionType::get(voidTy, arg1Ty, false));
	f_cyc_id_arg_stack_push = cast<Function>(cf);

	cf = module.getOrInsertFunction("cyc_id_return_stack_pop", int64Ty);
	f_cyc_id_return_stack_pop = cast<Function>(cf);

	cf = module.getOrInsertGlobal("cyc_id_cur_stack", int64Ty);
	v_cyc_id_cur_stack = cast<Value>(cf);

	cf = module.getOrInsertFunction("cyc_is_id_valid", FunctionType::get(voidTy, arg1Ty, false));
	f_cyc_is_id_valid = cast<Function>(cf);
}
