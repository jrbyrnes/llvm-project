--- |
  ; ModuleID = '../llvm-project/llvm/test/CodeGen/AMDGPU/merge-load-store.mir'
  source_filename = "../llvm-project/llvm/test/CodeGen/AMDGPU/merge-load-store.mir"
  target datalayout = "e-p:64:64-p1:64:64-p2:32:32-p3:32:32-p4:64:64-p5:32:32-p6:32:32-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-v2048:2048-n32:64-S32-A5-G1-ni:7"
  target triple = "amdgcn-amd-amdhsa"
  
  @lds0 = external dso_local unnamed_addr addrspace(3) global [256 x i32], align 4
  @lds1 = external dso_local unnamed_addr addrspace(3) global [256 x i32], align 4
  @lds2 = external dso_local unnamed_addr addrspace(3) global [256 x i32], align 4
  @lds3 = external dso_local unnamed_addr addrspace(3) global [256 x i32], align 4
  
  ; Function Attrs: nounwind
  define amdgpu_kernel void @mem_dependency(i32 addrspace(3)* %ptr.0) #0 {
    %ptr.4 = getelementptr i32, i32 addrspace(3)* %ptr.0, i32 1
    %ptr.64 = getelementptr i32, i32 addrspace(3)* %ptr.0, i32 16
    %1 = load i32, i32 addrspace(3)* %ptr.0, align 4
    store i32 %1, i32 addrspace(3)* %ptr.64, align 4
    %2 = load i32, i32 addrspace(3)* %ptr.64, align 4
    %3 = load i32, i32 addrspace(3)* %ptr.4, align 4
    %4 = add i32 %2, %3
    store i32 %4, i32 addrspace(3)* %ptr.0, align 4
    ret void
  }
  
  ; Function Attrs: convergent nounwind
  define void @asm_defines_address() #1 {
  bb:
    %tmp1 = load i32, i32 addrspace(3)* getelementptr inbounds ([256 x i32], [256 x i32] addrspace(3)* @lds0, i32 0, i32 0), align 4
    %0 = and i32 %tmp1, 255
    %tmp3 = load i32, i32 addrspace(3)* getelementptr ([256 x i32], [256 x i32] addrspace(3)* @lds1, i32 0, i32 undef), align 4
    %tmp6 = load i32, i32 addrspace(3)* getelementptr ([256 x i32], [256 x i32] addrspace(3)* @lds3, i32 0, i32 undef), align 4
    %tmp7 = tail call i32 asm "v_or_b32 $0, 0, $1", "=v,v"(i32 %tmp6) #3
    %tmp10 = lshr i32 %tmp7, 16
    %tmp11 = and i32 %tmp10, 255
    %tmp12 = getelementptr inbounds [256 x i32], [256 x i32] addrspace(3)* @lds1, i32 0, i32 %tmp11
    %tmp13 = load i32, i32 addrspace(3)* %tmp12, align 4
    %tmp14 = xor i32 %tmp3, %tmp13
    %tmp15 = lshr i32 %tmp14, 8
    %tmp16 = and i32 %tmp15, 16711680
    %tmp19 = lshr i32 %tmp16, 16
    %tmp20 = and i32 %tmp19, 255
    %tmp21 = getelementptr inbounds [256 x i32], [256 x i32] addrspace(3)* @lds1, i32 0, i32 %tmp20
    %tmp22 = load i32, i32 addrspace(3)* %tmp21, align 4
    %tmp24 = load i32, i32 addrspace(3)* getelementptr ([256 x i32], [256 x i32] addrspace(3)* @lds2, i32 0, i32 undef), align 4
    %tmp25 = xor i32 %tmp22, %tmp24
    %tmp26 = and i32 %tmp25, -16777216
    %tmp28 = or i32 %0, %tmp26
    store volatile i32 %tmp28, i32 addrspace(1)* undef, align 4
    ret void
  }
  
  ; Function Attrs: convergent nounwind
  define amdgpu_kernel void @move_waw_hazards() #1 {
    ret void
  }
  
  define amdgpu_kernel void @merge_mmos(i32 addrspace(1)* %ptr_addr1) #2 {
    ret void
  }
  
  define amdgpu_kernel void @reorder_offsets(i32 addrspace(1)* %reorder_addr1) #2 {
    ret void
  }
  
  attributes #0 = { nounwind "target-cpu"="gfx908" }
  attributes #1 = { convergent nounwind "target-cpu"="gfx908" }
  attributes #2 = { "target-cpu"="gfx908" }
  attributes #3 = { convergent nounwind readnone }

...
---
name:            mem_dependency
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: true
hasWinCFI:       false
callsEHReturn:   false
callsUnwindInit: false
hasEHCatchret:   false
hasEHScopes:     false
hasEHFunclets:   false
failsVerification: false
tracksDebugUserValues: false
registers:
  - { id: 0, class: vgpr_32, preferred-register: '' }
  - { id: 1, class: vgpr_32, preferred-register: '' }
  - { id: 2, class: vreg_64, preferred-register: '' }
  - { id: 3, class: vgpr_32, preferred-register: '' }
  - { id: 4, class: vgpr_32, preferred-register: '' }
  - { id: 5, class: vgpr_32, preferred-register: '' }
liveins:
  - { reg: '$vgpr0', virtual-reg: '%0' }
frameInfo:
  isFrameAddressTaken: false
  isReturnAddressTaken: false
  hasStackMap:     false
  hasPatchPoint:   false
  stackSize:       0
  offsetAdjustment: 0
  maxAlignment:    1
  adjustsStack:    false
  hasCalls:        false
  stackProtector:  ''
  functionContext: ''
  maxCallFrameSize: 0
  cvBytesOfCalleeSavedRegisters: 0
  hasOpaqueSPAdjustment: false
  hasVAStart:      false
  hasMustTailInVarArgFunc: false
  hasTailCall:     false
  localFrameSize:  0
  savePoint:       ''
  restorePoint:    ''
fixedStack:      []
stack:           []
callSites:       []
debugValueSubstitutions: []
constants:       []
machineFunctionInfo:
  explicitKernArgSize: 0
  maxKernArgAlign: 1
  ldsSize:         0
  gdsSize:         0
  dynLDSAlign:     1
  isEntryFunction: false
  noSignedZerosFPMath: false
  memoryBound:     false
  waveLimiter:     false
  hasSpilledSGPRs: false
  hasSpilledVGPRs: false
  scratchRSrcReg:  '$private_rsrc_reg'
  frameOffsetReg:  '$fp_reg'
  stackPtrOffsetReg: '$sp_reg'
  bytesInStackArgArea: 0
  returnsVoid:     true
  mode:
    ieee:            true
    dx10-clamp:      true
    fp32-input-denormals: true
    fp32-output-denormals: true
    fp64-fp16-input-denormals: true
    fp64-fp16-output-denormals: true
  highBitsOf32BitAddress: 0
  occupancy:       10
  vgprForAGPRCopy: '$vgpr63'
body:             |
  bb.0:
    liveins: $vgpr0
  
    %0:vgpr_32 = COPY $vgpr0
    $m0 = S_MOV_B32 -1
    %1:vgpr_32 = DS_READ_B32 %0, 0, 0, implicit $m0, implicit $exec :: (load (s32) from %ir.ptr.0, addrspace 3)
    %4:vgpr_32 = DS_READ_B32 %0, 4, 0, implicit $m0, implicit $exec :: (load (s32) from %ir.ptr.4, addrspace 3)
    DS_WRITE_B32 %0, %1, 64, 0, implicit $m0, implicit $exec :: (store (s32) into %ir.ptr.64, addrspace 3)
    %2:vreg_64 = DS_READ2_B32 %0, 16, 17, 0, implicit $m0, implicit $exec :: (load (s64) from %ir.ptr.64, align 4, addrspace 3)
    %3:vgpr_32 = COPY %2.sub0
    %5:vgpr_32 = V_ADD_CO_U32_e32 %3, %4, implicit-def $vcc, implicit $exec
    DS_WRITE_B32 %0, %5, 0, 0, implicit killed $m0, implicit $exec :: (store (s32) into %ir.ptr.0, addrspace 3)
    S_ENDPGM 0

...
---
name:            asm_defines_address
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: true
hasWinCFI:       false
callsEHReturn:   false
callsUnwindInit: false
hasEHCatchret:   false
hasEHScopes:     false
hasEHFunclets:   false
failsVerification: false
tracksDebugUserValues: false
registers:
  - { id: 0, class: vgpr_32, preferred-register: '' }
  - { id: 1, class: vgpr_32, preferred-register: '' }
  - { id: 2, class: vgpr_32, preferred-register: '' }
  - { id: 3, class: vgpr_32, preferred-register: '' }
  - { id: 4, class: vgpr_32, preferred-register: '' }
  - { id: 5, class: vgpr_32, preferred-register: '' }
  - { id: 6, class: vgpr_32, preferred-register: '' }
  - { id: 7, class: vgpr_32, preferred-register: '' }
liveins:         []
frameInfo:
  isFrameAddressTaken: false
  isReturnAddressTaken: false
  hasStackMap:     false
  hasPatchPoint:   false
  stackSize:       0
  offsetAdjustment: 0
  maxAlignment:    1
  adjustsStack:    false
  hasCalls:        false
  stackProtector:  ''
  functionContext: ''
  maxCallFrameSize: 4294967295
  cvBytesOfCalleeSavedRegisters: 0
  hasOpaqueSPAdjustment: false
  hasVAStart:      false
  hasMustTailInVarArgFunc: false
  hasTailCall:     false
  localFrameSize:  0
  savePoint:       ''
  restorePoint:    ''
fixedStack:      []
stack:           []
callSites:       []
debugValueSubstitutions: []
constants:       []
machineFunctionInfo:
  explicitKernArgSize: 0
  maxKernArgAlign: 1
  ldsSize:         0
  gdsSize:         0
  dynLDSAlign:     1
  isEntryFunction: false
  noSignedZerosFPMath: false
  memoryBound:     false
  waveLimiter:     false
  hasSpilledSGPRs: false
  hasSpilledVGPRs: false
  scratchRSrcReg:  '$private_rsrc_reg'
  frameOffsetReg:  '$fp_reg'
  stackPtrOffsetReg: '$sp_reg'
  bytesInStackArgArea: 0
  returnsVoid:     true
  argumentInfo:
    privateSegmentBuffer: { reg: '$sgpr0_sgpr1_sgpr2_sgpr3' }
    dispatchPtr:     { reg: '$sgpr4_sgpr5' }
    queuePtr:        { reg: '$sgpr6_sgpr7' }
    dispatchID:      { reg: '$sgpr10_sgpr11' }
    workGroupIDX:    { reg: '$sgpr12' }
    workGroupIDY:    { reg: '$sgpr13' }
    workGroupIDZ:    { reg: '$sgpr14' }
    implicitArgPtr:  { reg: '$sgpr8_sgpr9' }
    workItemIDX:     { reg: '$vgpr31', mask: 1023 }
    workItemIDY:     { reg: '$vgpr31', mask: 1047552 }
    workItemIDZ:     { reg: '$vgpr31', mask: 1072693248 }
  mode:
    ieee:            true
    dx10-clamp:      true
    fp32-input-denormals: true
    fp32-output-denormals: true
    fp64-fp16-input-denormals: true
    fp64-fp16-output-denormals: true
  highBitsOf32BitAddress: 0
  occupancy:       10
  vgprForAGPRCopy: '$vgpr63'
body:             |
  bb.0:
    %1:vgpr_32 = V_MOV_B32_e32 0, implicit $exec
    %4:vgpr_32 = DS_READ_B32 %1, 1024, 0, implicit $m0, implicit $exec :: (load (s32) from `i32 addrspace(3)* getelementptr ([256 x i32], [256 x i32] addrspace(3)* @lds3, i32 0, i32 undef)`, addrspace 3)
    %7:vgpr_32 = DS_READ_B32 %1, 0, 0, implicit $m0, implicit $exec :: (load (s32) from `i32 addrspace(3)* getelementptr ([256 x i32], [256 x i32] addrspace(3)* @lds2, i32 0, i32 undef)`, addrspace 3)
    INLINEASM &"v_or_b32 $0, 0, $1", 32 /* isconvergent attdialect */, 327690 /* regdef:SReg_1_with_sub0 */, def %0, 327689 /* reguse:SReg_1_with_sub0 */, %4
    %5:vgpr_32 = DS_READ_B32 %0, 2048, 0, implicit $m0, implicit $exec :: (load (s32) from %ir.tmp12, addrspace 3)
    %6:vgpr_32 = DS_READ_B32 %5, 2048, 0, implicit $m0, implicit $exec :: (load (s32) from %ir.tmp21, addrspace 3)
    dead %2:vgpr_32 = DS_READ_B32 %1, 3072, 0, implicit $m0, implicit $exec :: (dereferenceable load (s32) from `i32 addrspace(3)* getelementptr inbounds ([256 x i32], [256 x i32] addrspace(3)* @lds0, i32 0, i32 0)`, addrspace 3)
    dead %3:vgpr_32 = DS_READ_B32 %1, 2048, 0, implicit $m0, implicit $exec :: (load (s32) from `i32 addrspace(3)* getelementptr ([256 x i32], [256 x i32] addrspace(3)* @lds1, i32 0, i32 undef)`, addrspace 3)
    S_SETPC_B64_return undef $sgpr30_sgpr31, implicit %6, implicit %7

...
---
name:            move_waw_hazards
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: true
hasWinCFI:       false
callsEHReturn:   false
callsUnwindInit: false
hasEHCatchret:   false
hasEHScopes:     false
hasEHFunclets:   false
failsVerification: false
tracksDebugUserValues: false
registers:
  - { id: 0, class: sgpr_64, preferred-register: '' }
  - { id: 1, class: sreg_32_xm0_xexec, preferred-register: '' }
  - { id: 2, class: sreg_32_xm0, preferred-register: '' }
  - { id: 3, class: sreg_64_xexec, preferred-register: '' }
  - { id: 4, class: sgpr_128, preferred-register: '' }
  - { id: 5, class: sreg_64_xexec, preferred-register: '' }
  - { id: 6, class: sreg_32_xm0_xexec, preferred-register: '' }
  - { id: 7, class: sreg_32_xm0_xexec, preferred-register: '' }
  - { id: 8, class: sreg_32_xm0_xexec, preferred-register: '' }
  - { id: 9, class: sreg_64, preferred-register: '' }
  - { id: 10, class: sreg_64, preferred-register: '' }
  - { id: 11, class: sreg_64_xexec, preferred-register: '' }
  - { id: 12, class: sreg_32_xm0_xexec, preferred-register: '' }
liveins:         []
frameInfo:
  isFrameAddressTaken: false
  isReturnAddressTaken: false
  hasStackMap:     false
  hasPatchPoint:   false
  stackSize:       0
  offsetAdjustment: 0
  maxAlignment:    1
  adjustsStack:    false
  hasCalls:        false
  stackProtector:  ''
  functionContext: ''
  maxCallFrameSize: 4294967295
  cvBytesOfCalleeSavedRegisters: 0
  hasOpaqueSPAdjustment: false
  hasVAStart:      false
  hasMustTailInVarArgFunc: false
  hasTailCall:     false
  localFrameSize:  0
  savePoint:       ''
  restorePoint:    ''
fixedStack:      []
stack:           []
callSites:       []
debugValueSubstitutions: []
constants:       []
machineFunctionInfo:
  explicitKernArgSize: 0
  maxKernArgAlign: 1
  ldsSize:         0
  gdsSize:         0
  dynLDSAlign:     1
  isEntryFunction: false
  noSignedZerosFPMath: false
  memoryBound:     false
  waveLimiter:     false
  hasSpilledSGPRs: false
  hasSpilledVGPRs: false
  scratchRSrcReg:  '$private_rsrc_reg'
  frameOffsetReg:  '$fp_reg'
  stackPtrOffsetReg: '$sp_reg'
  bytesInStackArgArea: 0
  returnsVoid:     true
  mode:
    ieee:            true
    dx10-clamp:      true
    fp32-input-denormals: true
    fp32-output-denormals: true
    fp64-fp16-input-denormals: true
    fp64-fp16-output-denormals: true
  highBitsOf32BitAddress: 0
  occupancy:       10
  vgprForAGPRCopy: '$vgpr63'
body:             |
  bb.0:
    liveins: $sgpr0_sgpr1
  
    dead %0:sgpr_64 = COPY $sgpr0_sgpr1
    %1:sreg_32_xm0_xexec = S_MOV_B32 0
    %2:sreg_32_xm0 = S_MOV_B32 0
    %3:sreg_64_xexec = REG_SEQUENCE %1, %subreg.sub0, %2, %subreg.sub1
    %4:sgpr_128 = S_LOAD_DWORDX4_IMM %3, 0, 0 :: (invariant load (s128), addrspace 6)
    %5:sreg_64_xexec = S_BUFFER_LOAD_DWORDX2_IMM %4, 0, 0 :: (dereferenceable invariant load (s32))
    %8:sreg_32_xm0_xexec = S_BUFFER_LOAD_DWORD_IMM %4, 2, 0 :: (dereferenceable invariant load (s32))
    dead %12:sreg_32_xm0_xexec = S_BUFFER_LOAD_DWORD_IMM %4, 3, 0 :: (dereferenceable invariant load (s32))
    %6:sreg_32_xm0_xexec = COPY %5.sub0
    %7:sreg_32_xm0_xexec = COPY %5.sub1
    %9:sreg_64 = V_CMP_NE_U32_e64 %7, 0, implicit $exec
    %10:sreg_64 = V_CMP_NE_U32_e64 %8, 0, implicit $exec
    dead %11:sreg_64_xexec = S_AND_B64 %9, %10, implicit-def dead $scc
    S_CMP_EQ_U32 %6, 0, implicit-def $scc
    S_ENDPGM 0

...
---
name:            merge_mmos
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: true
hasWinCFI:       false
callsEHReturn:   false
callsUnwindInit: false
hasEHCatchret:   false
hasEHScopes:     false
hasEHFunclets:   false
failsVerification: false
tracksDebugUserValues: false
registers:
  - { id: 0, class: sgpr_128, preferred-register: '' }
  - { id: 1, class: sreg_32_xm0_xexec, preferred-register: '' }
  - { id: 2, class: sreg_32_xm0_xexec, preferred-register: '' }
  - { id: 3, class: vgpr_32, preferred-register: '' }
  - { id: 4, class: vgpr_32, preferred-register: '' }
  - { id: 5, class: vgpr_32, preferred-register: '' }
  - { id: 6, class: vgpr_32, preferred-register: '' }
liveins:         []
frameInfo:
  isFrameAddressTaken: false
  isReturnAddressTaken: false
  hasStackMap:     false
  hasPatchPoint:   false
  stackSize:       0
  offsetAdjustment: 0
  maxAlignment:    1
  adjustsStack:    false
  hasCalls:        false
  stackProtector:  ''
  functionContext: ''
  maxCallFrameSize: 4294967295
  cvBytesOfCalleeSavedRegisters: 0
  hasOpaqueSPAdjustment: false
  hasVAStart:      false
  hasMustTailInVarArgFunc: false
  hasTailCall:     false
  localFrameSize:  0
  savePoint:       ''
  restorePoint:    ''
fixedStack:      []
stack:           []
callSites:       []
debugValueSubstitutions: []
constants:       []
machineFunctionInfo:
  explicitKernArgSize: 0
  maxKernArgAlign: 1
  ldsSize:         0
  gdsSize:         0
  dynLDSAlign:     1
  isEntryFunction: false
  noSignedZerosFPMath: false
  memoryBound:     false
  waveLimiter:     false
  hasSpilledSGPRs: false
  hasSpilledVGPRs: false
  scratchRSrcReg:  '$private_rsrc_reg'
  frameOffsetReg:  '$fp_reg'
  stackPtrOffsetReg: '$sp_reg'
  bytesInStackArgArea: 0
  returnsVoid:     true
  mode:
    ieee:            true
    dx10-clamp:      true
    fp32-input-denormals: true
    fp32-output-denormals: true
    fp64-fp16-input-denormals: true
    fp64-fp16-output-denormals: true
  highBitsOf32BitAddress: 0
  occupancy:       10
  vgprForAGPRCopy: '$vgpr63'
body:             |
  bb.0:
    liveins: $sgpr0_sgpr1_sgpr2_sgpr3
  
    %0:sgpr_128 = COPY $sgpr0_sgpr1_sgpr2_sgpr3
    %4:vgpr_32 = BUFFER_LOAD_DWORD_OFFSET %0, 0, 4, 0, 0, 0, implicit $exec :: (dereferenceable load (s32))
    %5:vgpr_32 = BUFFER_LOAD_DWORD_OFFSET %0, 0, 64, 0, 0, 0, implicit $exec :: (dereferenceable load (s32) from %ir.ptr_addr1 + 64, addrspace 1)
    %6:vgpr_32 = BUFFER_LOAD_DWORD_OFFSET %0, 0, 68, 0, 0, 0, implicit $exec :: (dereferenceable load (s32) from %ir.ptr_addr1 + 68, addrspace 1)
    %3:vgpr_32 = BUFFER_LOAD_DWORD_OFFSET %0, 0, 0, 0, 0, 0, implicit $exec :: (dereferenceable load (s32))
    dead %2:sreg_32_xm0_xexec = S_BUFFER_LOAD_DWORD_IMM %0, 1, 0 :: (dereferenceable invariant load (s32))
    dead %1:sreg_32_xm0_xexec = S_BUFFER_LOAD_DWORD_IMM %0, 0, 0 :: (dereferenceable invariant load (s32))
    BUFFER_STORE_DWORD_OFFSET_exact %3, %0, 0, 0, 0, 0, 0, implicit $exec :: (dereferenceable store (s32))
    BUFFER_STORE_DWORD_OFFSET_exact %4, %0, 0, 4, 0, 0, 0, implicit $exec :: (dereferenceable store (s32))
    BUFFER_STORE_DWORD_OFFSET_exact %5, %0, 0, 64, 0, 0, 0, implicit $exec :: (dereferenceable store (s32) into %ir.ptr_addr1 + 64, addrspace 1)
    BUFFER_STORE_DWORD_OFFSET_exact %6, %0, 0, 68, 0, 0, 0, implicit $exec :: (dereferenceable store (s32) into %ir.ptr_addr1 + 68, addrspace 1)
    S_ENDPGM 0

...
---
name:            reorder_offsets
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: true
hasWinCFI:       false
callsEHReturn:   false
callsUnwindInit: false
hasEHCatchret:   false
hasEHScopes:     false
hasEHFunclets:   false
failsVerification: false
tracksDebugUserValues: false
registers:
  - { id: 0, class: sgpr_128, preferred-register: '' }
  - { id: 1, class: vgpr_32, preferred-register: '' }
liveins:         []
frameInfo:
  isFrameAddressTaken: false
  isReturnAddressTaken: false
  hasStackMap:     false
  hasPatchPoint:   false
  stackSize:       0
  offsetAdjustment: 0
  maxAlignment:    1
  adjustsStack:    false
  hasCalls:        false
  stackProtector:  ''
  functionContext: ''
  maxCallFrameSize: 4294967295
  cvBytesOfCalleeSavedRegisters: 0
  hasOpaqueSPAdjustment: false
  hasVAStart:      false
  hasMustTailInVarArgFunc: false
  hasTailCall:     false
  localFrameSize:  0
  savePoint:       ''
  restorePoint:    ''
fixedStack:      []
stack:           []
callSites:       []
debugValueSubstitutions: []
constants:       []
machineFunctionInfo:
  explicitKernArgSize: 0
  maxKernArgAlign: 1
  ldsSize:         0
  gdsSize:         0
  dynLDSAlign:     1
  isEntryFunction: false
  noSignedZerosFPMath: false
  memoryBound:     false
  waveLimiter:     false
  hasSpilledSGPRs: false
  hasSpilledVGPRs: false
  scratchRSrcReg:  '$private_rsrc_reg'
  frameOffsetReg:  '$fp_reg'
  stackPtrOffsetReg: '$sp_reg'
  bytesInStackArgArea: 0
  returnsVoid:     true
  mode:
    ieee:            true
    dx10-clamp:      true
    fp32-input-denormals: true
    fp32-output-denormals: true
    fp64-fp16-input-denormals: true
    fp64-fp16-output-denormals: true
  highBitsOf32BitAddress: 0
  occupancy:       10
  vgprForAGPRCopy: '$vgpr63'
body:             |
  bb.0:
    liveins: $sgpr0_sgpr1_sgpr2_sgpr3
  
    %0:sgpr_128 = COPY $sgpr0_sgpr1_sgpr2_sgpr3
    %1:vgpr_32 = V_MOV_B32_e32 0, implicit $exec
    BUFFER_STORE_DWORD_OFFSET_exact %1, %0, 0, 4, 0, 0, 0, implicit $exec :: (dereferenceable store (s32) into %ir.reorder_addr1 + 4, addrspace 1)
    BUFFER_STORE_DWORD_OFFSET_exact %1, %0, 0, 8, 0, 0, 0, implicit $exec :: (dereferenceable store (s32) into %ir.reorder_addr1 + 8, addrspace 1)
    BUFFER_STORE_DWORD_OFFSET_exact %1, %0, 0, 12, 0, 0, 0, implicit $exec :: (dereferenceable store (s32) into %ir.reorder_addr1 + 12, addrspace 1)
    BUFFER_STORE_DWORD_OFFSET_exact %1, %0, 0, 16, 0, 0, 0, implicit $exec :: (dereferenceable store (s32) into %ir.reorder_addr1 + 16, addrspace 1)
    BUFFER_STORE_DWORD_OFFSET_exact %1, %0, 0, 20, 0, 0, 0, implicit $exec :: (dereferenceable store (s32) into %ir.reorder_addr1 + 20, addrspace 1)
    BUFFER_STORE_DWORD_OFFSET_exact %1, %0, 0, 0, 0, 0, 0, implicit $exec :: (dereferenceable store (s32) into %ir.reorder_addr1, addrspace 1)
    S_ENDPGM 0

...
