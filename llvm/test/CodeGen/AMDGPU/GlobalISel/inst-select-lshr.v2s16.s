--- |
  ; ModuleID = '/home/jeff/ROCm2/llvm-project/llvm/test/CodeGen/AMDGPU/GlobalISel/inst-select-lshr.v2s16.mir'
  source_filename = "/home/jeff/ROCm2/llvm-project/llvm/test/CodeGen/AMDGPU/GlobalISel/inst-select-lshr.v2s16.mir"
  target datalayout = "e-p:64:64-p1:64:64-p2:32:32-p3:32:32-p4:64:64-p5:32:32-p6:32:32-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-v2048:2048-n32:64-S32-A5-G1-ni:7"
  
  define void @lshr_v2s16_ss() #0 {
  entry:
    unreachable
  }
  
  define void @lshr_v2s16_sv() #0 {
  entry:
    unreachable
  }
  
  define void @lshr_v2s16_vs() #0 {
  entry:
    unreachable
  }
  
  define void @lshr_v2s16_vv() #0 {
  entry:
    unreachable
  }
  
  attributes #0 = { "target-cpu"="gfx900" }

...
---
name:            lshr_v2s16_ss
alignment:       1
exposesReturnsTwice: false
legalized:       true
regBankSelected: true
selected:        true
failedISel:      true
tracksRegLiveness: false
hasWinCFI:       false
registers:
  - { id: 0, class: sgpr, preferred-register: '' }
  - { id: 1, class: sgpr, preferred-register: '' }
  - { id: 2, class: sgpr, preferred-register: '' }
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
  argumentInfo:
    privateSegmentBuffer: { reg: '$sgpr0_sgpr1_sgpr2_sgpr3' }
  mode:
    ieee:            true
    dx10-clamp:      true
    fp32-input-denormals: true
    fp32-output-denormals: true
    fp64-fp16-input-denormals: true
    fp64-fp16-output-denormals: true
  highBitsOf32BitAddress: 0
  occupancy:       10
body:             |
  bb.0:
    %0:sgpr(<2 x s16>) = COPY $sgpr0
    %1:sgpr(<2 x s16>) = COPY $sgpr1
    %2:sgpr(<2 x s16>) = G_LSHR %0, %1(<2 x s16>)
    S_ENDPGM 0, implicit %2(<2 x s16>)

...
---
name:            lshr_v2s16_sv
alignment:       1
exposesReturnsTwice: false
legalized:       true
regBankSelected: true
selected:        true
failedISel:      false
tracksRegLiveness: false
hasWinCFI:       false
registers:
  - { id: 0, class: sreg_32, preferred-register: '' }
  - { id: 1, class: vgpr_32, preferred-register: '' }
  - { id: 2, class: vgpr_32, preferred-register: '' }
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
  argumentInfo:
    privateSegmentBuffer: { reg: '$sgpr0_sgpr1_sgpr2_sgpr3' }
  mode:
    ieee:            true
    dx10-clamp:      true
    fp32-input-denormals: true
    fp32-output-denormals: true
    fp64-fp16-input-denormals: true
    fp64-fp16-output-denormals: true
  highBitsOf32BitAddress: 0
  occupancy:       10
body:             |
  bb.0:
    %0:sreg_32 = COPY $sgpr0
    %1:vgpr_32 = COPY $vgpr0
    %2:vgpr_32 = V_PK_LSHRREV_B16 8, %1, 8, %0, 0, 0, 0, 0, 0, implicit $exec
    S_ENDPGM 0, implicit %2

...
---
name:            lshr_v2s16_vs
alignment:       1
exposesReturnsTwice: false
legalized:       true
regBankSelected: true
selected:        true
failedISel:      false
tracksRegLiveness: false
hasWinCFI:       false
registers:
  - { id: 0, class: vgpr_32, preferred-register: '' }
  - { id: 1, class: sreg_32, preferred-register: '' }
  - { id: 2, class: vgpr_32, preferred-register: '' }
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
  argumentInfo:
    privateSegmentBuffer: { reg: '$sgpr0_sgpr1_sgpr2_sgpr3' }
  mode:
    ieee:            true
    dx10-clamp:      true
    fp32-input-denormals: true
    fp32-output-denormals: true
    fp64-fp16-input-denormals: true
    fp64-fp16-output-denormals: true
  highBitsOf32BitAddress: 0
  occupancy:       10
body:             |
  bb.0:
    %0:vgpr_32 = COPY $vgpr0
    %1:sreg_32 = COPY $sgpr0
    %2:vgpr_32 = V_PK_LSHRREV_B16 8, %1, 8, %0, 0, 0, 0, 0, 0, implicit $exec
    S_ENDPGM 0, implicit %2

...
---
name:            lshr_v2s16_vv
alignment:       1
exposesReturnsTwice: false
legalized:       true
regBankSelected: true
selected:        true
failedISel:      false
tracksRegLiveness: false
hasWinCFI:       false
registers:
  - { id: 0, class: vgpr_32, preferred-register: '' }
  - { id: 1, class: vgpr_32, preferred-register: '' }
  - { id: 2, class: vgpr_32, preferred-register: '' }
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
  argumentInfo:
    privateSegmentBuffer: { reg: '$sgpr0_sgpr1_sgpr2_sgpr3' }
  mode:
    ieee:            true
    dx10-clamp:      true
    fp32-input-denormals: true
    fp32-output-denormals: true
    fp64-fp16-input-denormals: true
    fp64-fp16-output-denormals: true
  highBitsOf32BitAddress: 0
  occupancy:       10
body:             |
  bb.0:
    %0:vgpr_32 = COPY $vgpr0
    %1:vgpr_32 = COPY $vgpr1
    %2:vgpr_32 = V_PK_LSHRREV_B16 8, %1, 8, %0, 0, 0, 0, 0, 0, implicit $exec
    S_ENDPGM 0, implicit %2

...
