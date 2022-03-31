	.text
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_lo                        ; -- Begin function lo_to_lo
	.p2align	2
	.type	lo_to_lo,@function
lo_to_lo:                               ; @lo_to_lo
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end0:
	.size	lo_to_lo, .Lfunc_end0-lo_to_lo
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_hi                        ; -- Begin function lo_to_hi
	.p2align	2
	.type	lo_to_hi,@function
lo_to_hi:                               ; @lo_to_hi
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end1:
	.size	lo_to_hi, .Lfunc_end1-lo_to_hi
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	hi_to_lo                        ; -- Begin function hi_to_lo
	.p2align	2
	.type	hi_to_lo,@function
hi_to_lo:                               ; @hi_to_lo
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end2:
	.size	hi_to_lo, .Lfunc_end2-hi_to_lo
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	hi_to_hi                        ; -- Begin function hi_to_hi
	.p2align	2
	.type	hi_to_hi,@function
hi_to_hi:                               ; @hi_to_hi
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end3:
	.size	hi_to_hi, .Lfunc_end3-hi_to_hi
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_lo_samereg                ; -- Begin function lo_to_lo_samereg
	.p2align	2
	.type	lo_to_lo_samereg,@function
lo_to_lo_samereg:                       ; @lo_to_lo_samereg
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end4:
	.size	lo_to_lo_samereg, .Lfunc_end4-lo_to_lo_samereg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_hi_samereg                ; -- Begin function lo_to_hi_samereg
	.p2align	2
	.type	lo_to_hi_samereg,@function
lo_to_hi_samereg:                       ; @lo_to_hi_samereg
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end5:
	.size	lo_to_hi_samereg, .Lfunc_end5-lo_to_hi_samereg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	hi_to_lo_samereg                ; -- Begin function hi_to_lo_samereg
	.p2align	2
	.type	hi_to_lo_samereg,@function
hi_to_lo_samereg:                       ; @hi_to_lo_samereg
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end6:
	.size	hi_to_lo_samereg, .Lfunc_end6-hi_to_lo_samereg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	hi_to_hi_samereg                ; -- Begin function hi_to_hi_samereg
	.p2align	2
	.type	hi_to_hi_samereg,@function
hi_to_hi_samereg:                       ; @hi_to_hi_samereg
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end7:
	.size	hi_to_hi_samereg, .Lfunc_end7-hi_to_hi_samereg
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_lo_def_livein             ; -- Begin function lo_to_lo_def_livein
	.p2align	2
	.type	lo_to_lo_def_livein,@function
lo_to_lo_def_livein:                    ; @lo_to_lo_def_livein
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end8:
	.size	lo_to_lo_def_livein, .Lfunc_end8-lo_to_lo_def_livein
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_hi_def_livein             ; -- Begin function lo_to_hi_def_livein
	.p2align	2
	.type	lo_to_hi_def_livein,@function
lo_to_hi_def_livein:                    ; @lo_to_hi_def_livein
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end9:
	.size	lo_to_hi_def_livein, .Lfunc_end9-lo_to_hi_def_livein
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	hi_to_lo_def_livein             ; -- Begin function hi_to_lo_def_livein
	.p2align	2
	.type	hi_to_lo_def_livein,@function
hi_to_lo_def_livein:                    ; @hi_to_lo_def_livein
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end10:
	.size	hi_to_lo_def_livein, .Lfunc_end10-hi_to_lo_def_livein
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	hi_to_hi_def_livein             ; -- Begin function hi_to_hi_def_livein
	.p2align	2
	.type	hi_to_hi_def_livein,@function
hi_to_hi_def_livein:                    ; @hi_to_hi_def_livein
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end11:
	.size	hi_to_hi_def_livein, .Lfunc_end11-hi_to_hi_def_livein
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_lo_hi_to_hi               ; -- Begin function lo_to_lo_hi_to_hi
	.p2align	2
	.type	lo_to_lo_hi_to_hi,@function
lo_to_lo_hi_to_hi:                      ; @lo_to_lo_hi_to_hi
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end12:
	.size	lo_to_lo_hi_to_hi, .Lfunc_end12-lo_to_lo_hi_to_hi
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_hi_hi_to_lo               ; -- Begin function lo_to_hi_hi_to_lo
	.p2align	2
	.type	lo_to_hi_hi_to_lo,@function
lo_to_hi_hi_to_lo:                      ; @lo_to_hi_hi_to_lo
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end13:
	.size	lo_to_hi_hi_to_lo, .Lfunc_end13-lo_to_hi_hi_to_lo
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_lo_undef                  ; -- Begin function lo_to_lo_undef
	.p2align	2
	.type	lo_to_lo_undef,@function
lo_to_lo_undef:                         ; @lo_to_lo_undef
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end14:
	.size	lo_to_lo_undef, .Lfunc_end14-lo_to_lo_undef
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	.AMDGPU.config
	.long	47176
	.long	0
	.long	47180
	.long	0
	.long	47200
	.long	0
	.long	4
	.long	0
	.long	8
	.long	0
	.text
	.globl	lo_to_lo_sgpr_to_sgpr           ; -- Begin function lo_to_lo_sgpr_to_sgpr
	.p2align	2
	.type	lo_to_lo_sgpr_to_sgpr,@function
lo_to_lo_sgpr_to_sgpr:                  ; @lo_to_lo_sgpr_to_sgpr
; %bb.0:
	s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
	s_waitcnt_vscnt null, 0x0
	s_endpgm
.Lfunc_end15:
	.size	lo_to_lo_sgpr_to_sgpr, .Lfunc_end15-lo_to_lo_sgpr_to_sgpr
                                        ; -- End function
	.section	.AMDGPU.csdata
; Function info:
; codeLenInByte = 12
; NumSgprs: 0
; NumVgprs: 0
; ScratchSize: 0
; MemoryBound: 0
	.section	".note.GNU-stack"
	.amd_amdgpu_isa "amdgcn-unknown-linux-gnu-gfx1035"
