#pragma once
#include <array>
#include <initializer_list>
#include <utility>

#include "common/jit_code_buffer.h"

#include "cpu_recompiler_register_cache.h"
#include "cpu_recompiler_thunks.h"
#include "cpu_recompiler_types.h"
#include "cpu_types.h"

// ABI selection
#if defined(Y_CPU_X64)
#if defined(Y_PLATFORM_WINDOWS)
#define ABI_WIN64 1
#elif defined(Y_PLATFORM_LINUX) || defined(Y_PLATFORM_OSX)
#define ABI_SYSV 1
#else
#error Unknown ABI.
#endif
#endif

namespace CPU::Recompiler {

class CodeGenerator
{
public:
  CodeGenerator(Core* cpu, JitCodeBuffer* code_buffer, const ASMFunctions& asm_functions);
  ~CodeGenerator();

  static u32 CalculateRegisterOffset(Reg reg);
  static const char* GetHostRegName(HostReg reg, RegSize size = HostPointerSize);
  static void AlignCodeBuffer(JitCodeBuffer* code_buffer);

  RegisterCache& GetRegisterCache() { return m_register_cache; }
  CodeEmitter& GetCodeEmitter() { return m_emit; }

  bool CompileBlock(const CodeBlock* block, CodeBlock::HostCodePointer* out_host_code, u32* out_host_code_size);

  //////////////////////////////////////////////////////////////////////////
  // Code Generation
  //////////////////////////////////////////////////////////////////////////
  void EmitBeginBlock();
  void EmitEndBlock();
  void EmitBlockExitOnBool(const Value& value);
  void FinalizeBlock(CodeBlock::HostCodePointer* out_host_code, u32* out_host_code_size);

  void EmitSignExtend(HostReg to_reg, RegSize to_size, HostReg from_reg, RegSize from_size);
  void EmitZeroExtend(HostReg to_reg, RegSize to_size, HostReg from_reg, RegSize from_size);
  void EmitCopyValue(HostReg to_reg, const Value& value);
  void EmitAdd(HostReg to_reg, const Value& value);
  void EmitSub(HostReg to_reg, const Value& value);
  void EmitCmp(HostReg to_reg, const Value& value);
  void EmitInc(HostReg to_reg, RegSize size);
  void EmitDec(HostReg to_reg, RegSize size);
  void EmitShl(HostReg to_reg, RegSize size, const Value& amount_value);
  void EmitShr(HostReg to_reg, RegSize size, const Value& amount_value);
  void EmitSar(HostReg to_reg, RegSize size, const Value& amount_value);
  void EmitAnd(HostReg to_reg, const Value& value);
  void EmitOr(HostReg to_reg, const Value& value);
  void EmitXor(HostReg to_reg, const Value& value);
  void EmitTest(HostReg to_reg, const Value& value);
  void EmitNot(HostReg to_reg, RegSize size);

  void EmitLoadGuestRegister(HostReg host_reg, Reg guest_reg);
  void EmitStoreGuestRegister(Reg guest_reg, const Value& value);
  void EmitLoadCPUStructField(HostReg host_reg, RegSize size, u32 offset);
  void EmitStoreCPUStructField(u32 offset, const Value& value);
  void EmitAddCPUStructField(u32 offset, const Value& value);

  u32 PrepareStackForCall();
  void RestoreStackAfterCall(u32 adjust_size);

  void EmitFunctionCallPtr(Value* return_value, const void* ptr);
  void EmitFunctionCallPtr(Value* return_value, const void* ptr, const Value& arg1);
  void EmitFunctionCallPtr(Value* return_value, const void* ptr, const Value& arg1, const Value& arg2);
  void EmitFunctionCallPtr(Value* return_value, const void* ptr, const Value& arg1, const Value& arg2,
                           const Value& arg3);
  void EmitFunctionCallPtr(Value* return_value, const void* ptr, const Value& arg1, const Value& arg2,
                           const Value& arg3, const Value& arg4);

  template<typename FunctionType>
  void EmitFunctionCall(Value* return_value, const FunctionType ptr)
  {
    EmitFunctionCallPtr(return_value, reinterpret_cast<const void**>(ptr));
  }

  template<typename FunctionType>
  void EmitFunctionCall(Value* return_value, const FunctionType ptr, const Value& arg1)
  {
    EmitFunctionCallPtr(return_value, reinterpret_cast<const void**>(ptr), arg1);
  }

  template<typename FunctionType>
  void EmitFunctionCall(Value* return_value, const FunctionType ptr, const Value& arg1, const Value& arg2)
  {
    EmitFunctionCallPtr(return_value, reinterpret_cast<const void**>(ptr), arg1, arg2);
  }

  template<typename FunctionType>
  void EmitFunctionCall(Value* return_value, const FunctionType ptr, const Value& arg1, const Value& arg2,
                        const Value& arg3)
  {
    EmitFunctionCallPtr(return_value, reinterpret_cast<const void**>(ptr), arg1, arg2, arg3);
  }

  template<typename FunctionType>
  void EmitFunctionCall(Value* return_value, const FunctionType ptr, const Value& arg1, const Value& arg2,
                        const Value& arg3, const Value& arg4)
  {
    EmitFunctionCallPtr(return_value, reinterpret_cast<const void**>(ptr), arg1, arg2, arg3, arg4);
  }

  // Host register saving.
  void EmitPushHostReg(HostReg reg);
  void EmitPopHostReg(HostReg reg);

  // Flags copying from host.
#if defined(Y_CPU_X64)
  void ReadFlagsFromHost(Value* value);
  Value ReadFlagsFromHost();
#endif

  // Value ops
  Value AddValues(const Value& lhs, const Value& rhs);
  Value MulValues(const Value& lhs, const Value& rhs);
  Value ShlValues(const Value& lhs, const Value& rhs);
  Value ShrValues(const Value& lhs, const Value& rhs);
  Value OrValues(const Value& lhs, const Value& rhs);

private:
  // Host register setup
  void InitHostRegs();

  Value ConvertValueSize(const Value& value, RegSize size, bool sign_extend);
  void ConvertValueSizeInPlace(Value* value, RegSize size, bool sign_extend);

  //////////////////////////////////////////////////////////////////////////
  // Code Generation Helpers
  //////////////////////////////////////////////////////////////////////////
  // branch target, memory address, etc
  void BlockPrologue();
  void BlockEpilogue();
  void InstructionPrologue(const CodeBlockInstruction& cbi, TickCount cycles,
                           bool force_sync = false);
  void InstructionEpilogue(const CodeBlockInstruction& cbi);
  void SyncCurrentInstructionPC();
  void SyncPC();
  void AddPendingCycles();
  void EmitDelaySlotUpdate(bool skip_check_for_delay, bool skip_check_old_value, bool move_next);

  //////////////////////////////////////////////////////////////////////////
  // Instruction Code Generators
  //////////////////////////////////////////////////////////////////////////
  bool CompileInstruction(const CodeBlockInstruction& cbi);
  bool Compile_Fallback(const CodeBlockInstruction& cbi);
  bool Compile_lui(const CodeBlockInstruction& cbi);
  bool Compile_ori(const CodeBlockInstruction& cbi);
  bool Compile_sll(const CodeBlockInstruction& cbi);
  bool Compile_srl(const CodeBlockInstruction& cbi);
  bool Compile_addiu(const CodeBlockInstruction& cbi);

  Core* m_cpu;
  JitCodeBuffer* m_code_buffer;
  const ASMFunctions& m_asm_functions;
  const CodeBlock* m_block = nullptr;
  const CodeBlockInstruction* m_block_start = nullptr;
  const CodeBlockInstruction* m_block_end = nullptr;
  RegisterCache m_register_cache;
  CodeEmitter m_emit;

  u32 m_delayed_pc_add = 0;
  TickCount m_delayed_cycles_add = 0;

  std::array<Value, 3> m_operand_memory_addresses{};

  // whether various flags need to be reset.
  bool m_current_instruction_in_branch_delay_slot_dirty = false;
  bool m_branch_was_taken_dirty = false;
  bool m_current_instruction_was_branch_taken_dirty = false;
  bool m_load_delay_dirty = false;
  bool m_next_load_delay_dirty = false;
};

} // namespace CPU_X86::Recompiler