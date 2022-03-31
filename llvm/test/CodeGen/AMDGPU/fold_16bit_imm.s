--- |
  ; ModuleID = 'ROCm2/llvm-project/llvm/test/CodeGen/AMDGPU/fold_16bit_imm.mir'
  source_filename = "ROCm2/llvm-project/llvm/test/CodeGen/AMDGPU/fold_16bit_imm.mir"
  target datalayout = "e-p:64:64-p1:64:64-p2:32:32-p3:32:32-p4:64:64-p5:32:32-p6:32:32-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-v2048:2048-n32:64-S32-A5-G1-ni:7"
  
  define void @fold_simm_16_sub_to_lo() #0 {
  entry:
    unreachable
  }
  
  define void @fold_simm_16_sub_to_phys() #0 {
  entry:
    unreachable
  }
  
  define void @fold_aimm_16_sub_to_phys() #0 {
  entry:
    unreachable
  }
  
  define void @fold_vimm_16_sub_to_lo() #0 {
  entry:
    unreachable
  }
  
  define void @fold_vimm_16_sub_to_phys() #0 {
  entry:
    unreachable
  }
  
  attributes #0 = { "target-cpu"="gfx906" }

...
---
name:            fold_simm_16_sub_to_lo
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: false
hasWinCFI:       false
registers:
  - { id: 0, class: sreg_32, preferred-register: '' }
  - { id: 1, class: sgpr_lo16, preferred-register: '' }
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
    %0:sreg_32 = S_MOV_B32 2048
    %1:sgpr_lo16 = COPY %0.lo16
    SI_RETURN_TO_EPILOG %1

...
---
name:            fold_simm_16_sub_to_phys
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: false
hasWinCFI:       false
registers:
  - { id: 0, class: sreg_32, preferred-register: '' }
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
    %0:sreg_32 = S_MOV_B32 2048
    $sgpr0_lo16 = COPY %0.lo16
    SI_RETURN_TO_EPILOG $sgpr0_lo16

...
---
name:            fold_aimm_16_sub_to_phys
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: false
hasWinCFI:       false
registers:
  - { id: 0, class: sreg_32, preferred-register: '' }
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
    %0:sreg_32 = S_MOV_B32 0
    $agpr0_lo16 = COPY %0.lo16
    SI_RETURN_TO_EPILOG $agpr0_lo16

...
---
name:            fold_vimm_16_sub_to_lo
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: false
hasWinCFI:       false
registers:
  - { id: 0, class: sreg_32, preferred-register: '' }
  - { id: 1, class: vgpr_lo16, preferred-register: '' }
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
    %0:sreg_32 = S_MOV_B32 2048
    %1:vgpr_lo16 = COPY %0.lo16
    SI_RETURN_TO_EPILOG %1

...
---
name:            fold_vimm_16_sub_to_phys
alignment:       1
exposesReturnsTwice: false
legalized:       false
regBankSelected: false
selected:        false
failedISel:      false
tracksRegLiveness: false
hasWinCFI:       false
registers:
  - { id: 0, class: sreg_32, preferred-register: '' }
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
    %0:sreg_32 = S_MOV_B32 2048
    $vgpr0_lo16 = COPY %0.lo16
    SI_RETURN_TO_EPILOG $vgpr0_lo16

...
