	.amdgcn_target "amdgcn-amd-amdhsa--gfx1250"
	.amdhsa_code_object_version 5
	.text
	.globl	mxgemm_tdm_pipelined_kernel     ; -- Begin function mxgemm_tdm_pipelined_kernel
	.p2align	8
	.type	mxgemm_tdm_pipelined_kernel,@function
mxgemm_tdm_pipelined_kernel:            ; @mxgemm_tdm_pipelined_kernel
;=== Block: 997 cycles ===
;  VALU:324(VOPD:272) SALU:302 TRANS:2 DS:51 SMEM:4 TDM:8 Ctrl:62
;  Stall: 300 cycles (30%)
;    FU:2 | MemFIFO:62 | Wait:123 | RegBank:25 | VaSSRC:11 | RAW:53 | ISFetch:20 (73 fetches) | MSBExposed:4
;      FU: LDS:2
; %bb.0:                                ; %.lr.ph
	s_setreg_imm32_b32 hwreg(HW_REG_WAVE_SCHED_MODE, 0, 5), 2
	s_setreg_imm32_b32 hwreg(HW_REG_WAVE_MODE, 25, 1), 1 ;  msbs: dst=0 src0=0 src1=0 src2=0
	s_clause 0x2
	s_load_b128 s[28:31], s[0:1], 0x28 nv
	s_load_b64 s[6:7], s[0:1], 0x20 nv
	s_load_b256 s[12:19], s[0:1], 0x0 nv
	s_bfe_u32 s5, ttmp8, 0x50019
	s_ashr_i32 s21, ttmp9, 31
	s_abs_i32 s34, ttmp9
	s_mov_b32 s10, 0
	v_and_b32_e32 v111, 15, v0
	s_mov_b32 s11, s10
	s_mov_b32 s24, 1
	s_mov_b32 s8, 64
	;Sim: Stall:1 [RAW hazard]
	v_dual_lshlrev_b32 v1, 8, v111 :: v_dual_bitop2_b32 v112, 16, v0 bitop3:0x40
	s_mov_b32 s4, 0x7500000
	s_mov_b32 s20, 0x3500000
	s_load_b96 s[64:66], s[0:1], 0x38 nv
	s_mov_b32 s33, 2
	s_mov_b32 s78, s10
	;Sim: Cache($)
	v_and_b32_e32 v2, 31, v0
	;Sim: Cache($)
	v_lshrrev_b32_e32 v0, 1, v0
	;Sim: Stall:15 [WaitCnt]
	s_wait_kmcnt 0x0
	s_add_co_i32 s0, s29, 0xff
	s_add_co_i32 s1, s28, 0xff
	s_ashr_i32 s2, s0, 31
	s_ashr_i32 s3, s1, 31
	s_lshr_b32 s2, s2, 24
	s_lshr_b32 s3, s3, 24
	s_add_co_i32 s0, s0, s2
	s_add_co_i32 s1, s1, s3
	s_ashr_i32 s0, s0, 8
	s_ashr_i32 s35, s1, 8
	s_lshl_b32 s67, s0, 3
	s_bfe_i32 s0, s0, 0x1001c
	s_abs_i32 s69, s67
	s_xor_b32 s68, s21, s0
	s_cvt_f32_u32 s1, s69
	s_sub_co_i32 s0, 0, s69
	s_mov_b32 s22, s30
	s_mov_b32 s23, s29
	v_rcp_iflag_f32_e32 v3, s1
	v_dual_lshlrev_b32 v4, 4, v2 :: v_dual_bitop2_b32 v5, 8, v0 bitop3:0x40
	;Sim: Fused
	s_set_vgpr_msb 0xc0                     ;  msbs: dst=3 src0=0 src1=0 src2=0
	;Sim: Cache($)
	v_bitop3_b32 v214 /*v982*/, v0, 40, 32 bitop3:0xc8
	;Sim: Cache($)
	v_bitop3_b32 v212 /*v980*/, v0, 24, 16 bitop3:0xc8
	;Sim: Fused
	s_set_vgpr_msb 0xc040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: Cache($)
	v_bitop3_b32 v194 /*v450*/, v0, 56, 48 bitop3:0xc8
	;Sim: Fused
	s_set_vgpr_msb 0x40c0                   ;  msbs: dst=3 src0=0 src1=0 src2=0
	v_mov_b32_e32 v186 /*v954*/, 0
	;Sim: Stall:1 [RAW hazard]
	v_readfirstlane_b32 s1, v3
	;Sim: Fused
	s_set_vgpr_msb 0xc0c3                   ;  msbs: dst=3 src0=3 src1=0 src2=0
	;Sim: Stall:2 [RAW hazard]
	v_dual_mov_b32 v187 /*v955*/, v186 /*v954*/ :: v_dual_mov_b32 v188 /*v956*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v189 /*v957*/, v186 /*v954*/ :: v_dual_mov_b32 v190 /*v958*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v191 /*v959*/, v186 /*v954*/ :: v_dual_mov_b32 v192 /*v960*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v193 /*v961*/, v186 /*v954*/ :: v_dual_mov_b32 v2 /*v770*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v3 /*v771*/, v186 /*v954*/ :: v_dual_mov_b32 v4 /*v772*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v5 /*v773*/, v186 /*v954*/ :: v_dual_mov_b32 v6 /*v774*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v7 /*v775*/, v186 /*v954*/ :: v_dual_mov_b32 v8 /*v776*/, v186 /*v954*/
	;Sim: Cache($)
	v_mov_b32_e32 v9 /*v777*/, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0xc383                   ;  msbs: dst=2 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v236 /*v748*/, v186 /*v954*/ :: v_dual_mov_b32 v237 /*v749*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v238 /*v750*/, v186 /*v954*/ :: v_dual_mov_b32 v239 /*v751*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v240 /*v752*/, v186 /*v954*/ :: v_dual_mov_b32 v241 /*v753*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v242 /*v754*/, v186 /*v954*/ :: v_dual_mov_b32 v243 /*v755*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v228 /*v740*/, v186 /*v954*/ :: v_dual_mov_b32 v229 /*v741*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v230 /*v742*/, v186 /*v954*/ :: v_dual_mov_b32 v231 /*v743*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v232 /*v744*/, v186 /*v954*/ :: v_dual_mov_b32 v233 /*v745*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v234 /*v746*/, v186 /*v954*/ :: v_dual_mov_b32 v235 /*v747*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v220 /*v732*/, v186 /*v954*/ :: v_dual_mov_b32 v221 /*v733*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v222 /*v734*/, v186 /*v954*/ :: v_dual_mov_b32 v223 /*v735*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v224 /*v736*/, v186 /*v954*/ :: v_dual_mov_b32 v225 /*v737*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v226 /*v738*/, v186 /*v954*/ :: v_dual_mov_b32 v227 /*v739*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v212 /*v724*/, v186 /*v954*/ :: v_dual_mov_b32 v213 /*v725*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v214 /*v726*/, v186 /*v954*/ :: v_dual_mov_b32 v215 /*v727*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v216 /*v728*/, v186 /*v954*/ :: v_dual_mov_b32 v217 /*v729*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v218 /*v730*/, v186 /*v954*/ :: v_dual_mov_b32 v219 /*v731*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v204 /*v716*/, v186 /*v954*/ :: v_dual_mov_b32 v205 /*v717*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v206 /*v718*/, v186 /*v954*/ :: v_dual_mov_b32 v207 /*v719*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v208 /*v720*/, v186 /*v954*/ :: v_dual_mov_b32 v209 /*v721*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v210 /*v722*/, v186 /*v954*/ :: v_dual_mov_b32 v211 /*v723*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v196 /*v708*/, v186 /*v954*/ :: v_dual_mov_b32 v197 /*v709*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v198 /*v710*/, v186 /*v954*/ :: v_dual_mov_b32 v199 /*v711*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v200 /*v712*/, v186 /*v954*/ :: v_dual_mov_b32 v201 /*v713*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v202 /*v714*/, v186 /*v954*/ :: v_dual_mov_b32 v203 /*v715*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v124 /*v636*/, v186 /*v954*/ :: v_dual_mov_b32 v125 /*v637*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v126 /*v638*/, v186 /*v954*/ :: v_dual_mov_b32 v127 /*v639*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v128 /*v640*/, v186 /*v954*/ :: v_dual_mov_b32 v129 /*v641*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v130 /*v642*/, v186 /*v954*/ :: v_dual_mov_b32 v131 /*v643*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v116 /*v628*/, v186 /*v954*/ :: v_dual_mov_b32 v117 /*v629*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v118 /*v630*/, v186 /*v954*/ :: v_dual_mov_b32 v119 /*v631*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v120 /*v632*/, v186 /*v954*/ :: v_dual_mov_b32 v121 /*v633*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v122 /*v634*/, v186 /*v954*/ :: v_dual_mov_b32 v123 /*v635*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v108 /*v620*/, v186 /*v954*/ :: v_dual_mov_b32 v109 /*v621*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v110 /*v622*/, v186 /*v954*/ :: v_dual_mov_b32 v111 /*v623*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v112 /*v624*/, v186 /*v954*/ :: v_dual_mov_b32 v113 /*v625*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v114 /*v626*/, v186 /*v954*/ :: v_dual_mov_b32 v115 /*v627*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v100 /*v612*/, v186 /*v954*/ :: v_dual_mov_b32 v101 /*v613*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v102 /*v614*/, v186 /*v954*/ :: v_dual_mov_b32 v103 /*v615*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v104 /*v616*/, v186 /*v954*/ :: v_dual_mov_b32 v105 /*v617*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v106 /*v618*/, v186 /*v954*/ :: v_dual_mov_b32 v107 /*v619*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v92 /*v604*/, v186 /*v954*/ :: v_dual_mov_b32 v93 /*v605*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v94 /*v606*/, v186 /*v954*/ :: v_dual_mov_b32 v95 /*v607*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v96 /*v608*/, v186 /*v954*/ :: v_dual_mov_b32 v97 /*v609*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v98 /*v610*/, v186 /*v954*/ :: v_dual_mov_b32 v99 /*v611*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v84 /*v596*/, v186 /*v954*/ :: v_dual_mov_b32 v85 /*v597*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v86 /*v598*/, v186 /*v954*/ :: v_dual_mov_b32 v87 /*v599*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v88 /*v600*/, v186 /*v954*/ :: v_dual_mov_b32 v89 /*v601*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v90 /*v602*/, v186 /*v954*/ :: v_dual_mov_b32 v91 /*v603*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v76 /*v588*/, v186 /*v954*/ :: v_dual_mov_b32 v77 /*v589*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v78 /*v590*/, v186 /*v954*/ :: v_dual_mov_b32 v79 /*v591*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v80 /*v592*/, v186 /*v954*/ :: v_dual_mov_b32 v81 /*v593*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v82 /*v594*/, v186 /*v954*/ :: v_dual_mov_b32 v83 /*v595*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v68 /*v580*/, v186 /*v954*/ :: v_dual_mov_b32 v69 /*v581*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v70 /*v582*/, v186 /*v954*/ :: v_dual_mov_b32 v71 /*v583*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v72 /*v584*/, v186 /*v954*/ :: v_dual_mov_b32 v73 /*v585*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v74 /*v586*/, v186 /*v954*/ :: v_dual_mov_b32 v75 /*v587*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v60 /*v572*/, v186 /*v954*/ :: v_dual_mov_b32 v61 /*v573*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v62 /*v574*/, v186 /*v954*/ :: v_dual_mov_b32 v63 /*v575*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v64 /*v576*/, v186 /*v954*/ :: v_dual_mov_b32 v65 /*v577*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v66 /*v578*/, v186 /*v954*/ :: v_dual_mov_b32 v67 /*v579*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v52 /*v564*/, v186 /*v954*/ :: v_dual_mov_b32 v53 /*v565*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v54 /*v566*/, v186 /*v954*/ :: v_dual_mov_b32 v55 /*v567*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v56 /*v568*/, v186 /*v954*/ :: v_dual_mov_b32 v57 /*v569*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v58 /*v570*/, v186 /*v954*/ :: v_dual_mov_b32 v59 /*v571*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v44 /*v556*/, v186 /*v954*/ :: v_dual_mov_b32 v45 /*v557*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v46 /*v558*/, v186 /*v954*/ :: v_dual_mov_b32 v47 /*v559*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v48 /*v560*/, v186 /*v954*/ :: v_dual_mov_b32 v49 /*v561*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v50 /*v562*/, v186 /*v954*/ :: v_dual_mov_b32 v51 /*v563*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v36 /*v548*/, v186 /*v954*/ :: v_dual_mov_b32 v37 /*v549*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v38 /*v550*/, v186 /*v954*/ :: v_dual_mov_b32 v39 /*v551*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v40 /*v552*/, v186 /*v954*/ :: v_dual_mov_b32 v41 /*v553*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v42 /*v554*/, v186 /*v954*/ :: v_dual_mov_b32 v43 /*v555*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v28 /*v540*/, v186 /*v954*/ :: v_dual_mov_b32 v29 /*v541*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v30 /*v542*/, v186 /*v954*/ :: v_dual_mov_b32 v31 /*v543*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v32 /*v544*/, v186 /*v954*/ :: v_dual_mov_b32 v33 /*v545*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v34 /*v546*/, v186 /*v954*/ :: v_dual_mov_b32 v35 /*v547*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v20 /*v532*/, v186 /*v954*/ :: v_dual_mov_b32 v21 /*v533*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v22 /*v534*/, v186 /*v954*/ :: v_dual_mov_b32 v23 /*v535*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v24 /*v536*/, v186 /*v954*/ :: v_dual_mov_b32 v25 /*v537*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v26 /*v538*/, v186 /*v954*/ :: v_dual_mov_b32 v27 /*v539*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v12 /*v524*/, v186 /*v954*/ :: v_dual_mov_b32 v13 /*v525*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v14 /*v526*/, v186 /*v954*/ :: v_dual_mov_b32 v15 /*v527*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v16 /*v528*/, v186 /*v954*/ :: v_dual_mov_b32 v17 /*v529*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v18 /*v530*/, v186 /*v954*/ :: v_dual_mov_b32 v19 /*v531*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v4 /*v516*/, v186 /*v954*/ :: v_dual_mov_b32 v5 /*v517*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v6 /*v518*/, v186 /*v954*/ :: v_dual_mov_b32 v7 /*v519*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v8 /*v520*/, v186 /*v954*/ :: v_dual_mov_b32 v9 /*v521*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v10 /*v522*/, v186 /*v954*/ :: v_dual_mov_b32 v11 /*v523*/, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0x8343                   ;  msbs: dst=1 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v252 /*v508*/, v186 /*v954*/ :: v_dual_mov_b32 v253 /*v509*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v254 /*v510*/, v186 /*v954*/ :: v_dual_mov_b32 v255 /*v511*/, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0x4383                   ;  msbs: dst=2 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v0 /*v512*/, v186 /*v954*/ :: v_dual_mov_b32 v1 /*v513*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v2 /*v514*/, v186 /*v954*/ :: v_dual_mov_b32 v3 /*v515*/, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0x8343                   ;  msbs: dst=1 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v244 /*v500*/, v186 /*v954*/ :: v_dual_mov_b32 v245 /*v501*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v246 /*v502*/, v186 /*v954*/ :: v_dual_mov_b32 v247 /*v503*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v248 /*v504*/, v186 /*v954*/ :: v_dual_mov_b32 v249 /*v505*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v250 /*v506*/, v186 /*v954*/ :: v_dual_mov_b32 v251 /*v507*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v236 /*v492*/, v186 /*v954*/ :: v_dual_mov_b32 v237 /*v493*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v238 /*v494*/, v186 /*v954*/ :: v_dual_mov_b32 v239 /*v495*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v240 /*v496*/, v186 /*v954*/ :: v_dual_mov_b32 v241 /*v497*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v242 /*v498*/, v186 /*v954*/ :: v_dual_mov_b32 v243 /*v499*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v228 /*v484*/, v186 /*v954*/ :: v_dual_mov_b32 v229 /*v485*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v230 /*v486*/, v186 /*v954*/ :: v_dual_mov_b32 v231 /*v487*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v232 /*v488*/, v186 /*v954*/ :: v_dual_mov_b32 v233 /*v489*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v234 /*v490*/, v186 /*v954*/ :: v_dual_mov_b32 v235 /*v491*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v220 /*v476*/, v186 /*v954*/ :: v_dual_mov_b32 v221 /*v477*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v222 /*v478*/, v186 /*v954*/ :: v_dual_mov_b32 v223 /*v479*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v224 /*v480*/, v186 /*v954*/ :: v_dual_mov_b32 v225 /*v481*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v226 /*v482*/, v186 /*v954*/ :: v_dual_mov_b32 v227 /*v483*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v212 /*v468*/, v186 /*v954*/ :: v_dual_mov_b32 v213 /*v469*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v214 /*v470*/, v186 /*v954*/ :: v_dual_mov_b32 v215 /*v471*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v216 /*v472*/, v186 /*v954*/ :: v_dual_mov_b32 v217 /*v473*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v218 /*v474*/, v186 /*v954*/ :: v_dual_mov_b32 v219 /*v475*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v204 /*v460*/, v186 /*v954*/ :: v_dual_mov_b32 v205 /*v461*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v206 /*v462*/, v186 /*v954*/ :: v_dual_mov_b32 v207 /*v463*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v208 /*v464*/, v186 /*v954*/ :: v_dual_mov_b32 v209 /*v465*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v210 /*v466*/, v186 /*v954*/ :: v_dual_mov_b32 v211 /*v467*/, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0x4383                   ;  msbs: dst=2 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v250 /*v762*/, v186 /*v954*/ :: v_dual_mov_b32 v251 /*v763*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v252 /*v764*/, v186 /*v954*/ :: v_dual_mov_b32 v253 /*v765*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v254 /*v766*/, v186 /*v954*/ :: v_dual_mov_b32 v255 /*v767*/, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0x83c3                   ;  msbs: dst=3 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v0 /*v768*/, v186 /*v954*/ :: v_dual_mov_b32 v1 /*v769*/, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0xc343                   ;  msbs: dst=1 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v196 /*v452*/, v186 /*v954*/ :: v_dual_mov_b32 v197 /*v453*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v198 /*v454*/, v186 /*v954*/ :: v_dual_mov_b32 v199 /*v455*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v200 /*v456*/, v186 /*v954*/ :: v_dual_mov_b32 v201 /*v457*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v202 /*v458*/, v186 /*v954*/ :: v_dual_mov_b32 v203 /*v459*/, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0x4303                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v16, v186 /*v954*/ :: v_dual_mov_b32 v17, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v18, v186 /*v954*/ :: v_dual_mov_b32 v19, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v20, v186 /*v954*/ :: v_dual_mov_b32 v21, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v22, v186 /*v954*/ :: v_dual_mov_b32 v23, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0x3c3                    ;  msbs: dst=3 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v178 /*v946*/, v186 /*v954*/ :: v_dual_mov_b32 v179 /*v947*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v180 /*v948*/, v186 /*v954*/ :: v_dual_mov_b32 v181 /*v949*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v182 /*v950*/, v186 /*v954*/ :: v_dual_mov_b32 v183 /*v951*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v184 /*v952*/, v186 /*v954*/ :: v_dual_mov_b32 v185 /*v953*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v170 /*v938*/, v186 /*v954*/ :: v_dual_mov_b32 v171 /*v939*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v172 /*v940*/, v186 /*v954*/ :: v_dual_mov_b32 v173 /*v941*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v174 /*v942*/, v186 /*v954*/ :: v_dual_mov_b32 v175 /*v943*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v176 /*v944*/, v186 /*v954*/ :: v_dual_mov_b32 v177 /*v945*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v162 /*v930*/, v186 /*v954*/ :: v_dual_mov_b32 v163 /*v931*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v164 /*v932*/, v186 /*v954*/ :: v_dual_mov_b32 v165 /*v933*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v166 /*v934*/, v186 /*v954*/ :: v_dual_mov_b32 v167 /*v935*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v168 /*v936*/, v186 /*v954*/ :: v_dual_mov_b32 v169 /*v937*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v154 /*v922*/, v186 /*v954*/ :: v_dual_mov_b32 v155 /*v923*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v156 /*v924*/, v186 /*v954*/ :: v_dual_mov_b32 v157 /*v925*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v158 /*v926*/, v186 /*v954*/ :: v_dual_mov_b32 v159 /*v927*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v160 /*v928*/, v186 /*v954*/ :: v_dual_mov_b32 v161 /*v929*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v146 /*v914*/, v186 /*v954*/ :: v_dual_mov_b32 v147 /*v915*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v148 /*v916*/, v186 /*v954*/ :: v_dual_mov_b32 v149 /*v917*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v150 /*v918*/, v186 /*v954*/ :: v_dual_mov_b32 v151 /*v919*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v152 /*v920*/, v186 /*v954*/ :: v_dual_mov_b32 v153 /*v921*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v138 /*v906*/, v186 /*v954*/ :: v_dual_mov_b32 v139 /*v907*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v140 /*v908*/, v186 /*v954*/ :: v_dual_mov_b32 v141 /*v909*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v142 /*v910*/, v186 /*v954*/ :: v_dual_mov_b32 v143 /*v911*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v144 /*v912*/, v186 /*v954*/ :: v_dual_mov_b32 v145 /*v913*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v130 /*v898*/, v186 /*v954*/ :: v_dual_mov_b32 v131 /*v899*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v132 /*v900*/, v186 /*v954*/ :: v_dual_mov_b32 v133 /*v901*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v134 /*v902*/, v186 /*v954*/ :: v_dual_mov_b32 v135 /*v903*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v136 /*v904*/, v186 /*v954*/ :: v_dual_mov_b32 v137 /*v905*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v122 /*v890*/, v186 /*v954*/ :: v_dual_mov_b32 v123 /*v891*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v124 /*v892*/, v186 /*v954*/ :: v_dual_mov_b32 v125 /*v893*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v126 /*v894*/, v186 /*v954*/ :: v_dual_mov_b32 v127 /*v895*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v128 /*v896*/, v186 /*v954*/ :: v_dual_mov_b32 v129 /*v897*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v114 /*v882*/, v186 /*v954*/ :: v_dual_mov_b32 v115 /*v883*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v116 /*v884*/, v186 /*v954*/ :: v_dual_mov_b32 v117 /*v885*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v118 /*v886*/, v186 /*v954*/ :: v_dual_mov_b32 v119 /*v887*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v120 /*v888*/, v186 /*v954*/ :: v_dual_mov_b32 v121 /*v889*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v106 /*v874*/, v186 /*v954*/ :: v_dual_mov_b32 v107 /*v875*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v108 /*v876*/, v186 /*v954*/ :: v_dual_mov_b32 v109 /*v877*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v110 /*v878*/, v186 /*v954*/ :: v_dual_mov_b32 v111 /*v879*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v112 /*v880*/, v186 /*v954*/ :: v_dual_mov_b32 v113 /*v881*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v98 /*v866*/, v186 /*v954*/ :: v_dual_mov_b32 v99 /*v867*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v100 /*v868*/, v186 /*v954*/ :: v_dual_mov_b32 v101 /*v869*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v102 /*v870*/, v186 /*v954*/ :: v_dual_mov_b32 v103 /*v871*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v104 /*v872*/, v186 /*v954*/ :: v_dual_mov_b32 v105 /*v873*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v90 /*v858*/, v186 /*v954*/ :: v_dual_mov_b32 v91 /*v859*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v92 /*v860*/, v186 /*v954*/ :: v_dual_mov_b32 v93 /*v861*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v94 /*v862*/, v186 /*v954*/ :: v_dual_mov_b32 v95 /*v863*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v96 /*v864*/, v186 /*v954*/ :: v_dual_mov_b32 v97 /*v865*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v82 /*v850*/, v186 /*v954*/ :: v_dual_mov_b32 v83 /*v851*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v84 /*v852*/, v186 /*v954*/ :: v_dual_mov_b32 v85 /*v853*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v86 /*v854*/, v186 /*v954*/ :: v_dual_mov_b32 v87 /*v855*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v88 /*v856*/, v186 /*v954*/ :: v_dual_mov_b32 v89 /*v857*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v74 /*v842*/, v186 /*v954*/ :: v_dual_mov_b32 v75 /*v843*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v76 /*v844*/, v186 /*v954*/ :: v_dual_mov_b32 v77 /*v845*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v78 /*v846*/, v186 /*v954*/ :: v_dual_mov_b32 v79 /*v847*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v80 /*v848*/, v186 /*v954*/ :: v_dual_mov_b32 v81 /*v849*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v66 /*v834*/, v186 /*v954*/ :: v_dual_mov_b32 v67 /*v835*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v68 /*v836*/, v186 /*v954*/ :: v_dual_mov_b32 v69 /*v837*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v70 /*v838*/, v186 /*v954*/ :: v_dual_mov_b32 v71 /*v839*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v72 /*v840*/, v186 /*v954*/ :: v_dual_mov_b32 v73 /*v841*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v58 /*v826*/, v186 /*v954*/ :: v_dual_mov_b32 v59 /*v827*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v60 /*v828*/, v186 /*v954*/ :: v_dual_mov_b32 v61 /*v829*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v62 /*v830*/, v186 /*v954*/ :: v_dual_mov_b32 v63 /*v831*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v64 /*v832*/, v186 /*v954*/ :: v_dual_mov_b32 v65 /*v833*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v50 /*v818*/, v186 /*v954*/ :: v_dual_mov_b32 v51 /*v819*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v52 /*v820*/, v186 /*v954*/ :: v_dual_mov_b32 v53 /*v821*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v54 /*v822*/, v186 /*v954*/ :: v_dual_mov_b32 v55 /*v823*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v56 /*v824*/, v186 /*v954*/ :: v_dual_mov_b32 v57 /*v825*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v42 /*v810*/, v186 /*v954*/ :: v_dual_mov_b32 v43 /*v811*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v44 /*v812*/, v186 /*v954*/ :: v_dual_mov_b32 v45 /*v813*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v46 /*v814*/, v186 /*v954*/ :: v_dual_mov_b32 v47 /*v815*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v48 /*v816*/, v186 /*v954*/ :: v_dual_mov_b32 v49 /*v817*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v34 /*v802*/, v186 /*v954*/ :: v_dual_mov_b32 v35 /*v803*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v36 /*v804*/, v186 /*v954*/ :: v_dual_mov_b32 v37 /*v805*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v38 /*v806*/, v186 /*v954*/ :: v_dual_mov_b32 v39 /*v807*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v40 /*v808*/, v186 /*v954*/ :: v_dual_mov_b32 v41 /*v809*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v26 /*v794*/, v186 /*v954*/ :: v_dual_mov_b32 v27 /*v795*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v28 /*v796*/, v186 /*v954*/ :: v_dual_mov_b32 v29 /*v797*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v30 /*v798*/, v186 /*v954*/ :: v_dual_mov_b32 v31 /*v799*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v32 /*v800*/, v186 /*v954*/ :: v_dual_mov_b32 v33 /*v801*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v18 /*v786*/, v186 /*v954*/ :: v_dual_mov_b32 v19 /*v787*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v20 /*v788*/, v186 /*v954*/ :: v_dual_mov_b32 v21 /*v789*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v22 /*v790*/, v186 /*v954*/ :: v_dual_mov_b32 v23 /*v791*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v24 /*v792*/, v186 /*v954*/ :: v_dual_mov_b32 v25 /*v793*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v10 /*v778*/, v186 /*v954*/ :: v_dual_mov_b32 v11 /*v779*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v12 /*v780*/, v186 /*v954*/ :: v_dual_mov_b32 v13 /*v781*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v14 /*v782*/, v186 /*v954*/ :: v_dual_mov_b32 v15 /*v783*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v16 /*v784*/, v186 /*v954*/ :: v_dual_mov_b32 v17 /*v785*/, v186 /*v954*/
	;Sim: Fused
	s_set_vgpr_msb 0xc383                   ;  msbs: dst=2 src0=3 src1=0 src2=0
	;Sim: Cache($$)
	v_dual_mov_b32 v188 /*v700*/, v186 /*v954*/ :: v_dual_mov_b32 v189 /*v701*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v190 /*v702*/, v186 /*v954*/ :: v_dual_mov_b32 v191 /*v703*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v192 /*v704*/, v186 /*v954*/ :: v_dual_mov_b32 v193 /*v705*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v194 /*v706*/, v186 /*v954*/ :: v_dual_mov_b32 v195 /*v707*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v180 /*v692*/, v186 /*v954*/ :: v_dual_mov_b32 v181 /*v693*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v182 /*v694*/, v186 /*v954*/ :: v_dual_mov_b32 v183 /*v695*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v184 /*v696*/, v186 /*v954*/ :: v_dual_mov_b32 v185 /*v697*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v186 /*v698*/, v186 /*v954*/ :: v_dual_mov_b32 v187 /*v699*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v172 /*v684*/, v186 /*v954*/ :: v_dual_mov_b32 v173 /*v685*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v174 /*v686*/, v186 /*v954*/ :: v_dual_mov_b32 v175 /*v687*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v176 /*v688*/, v186 /*v954*/ :: v_dual_mov_b32 v177 /*v689*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v178 /*v690*/, v186 /*v954*/ :: v_dual_mov_b32 v179 /*v691*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v164 /*v676*/, v186 /*v954*/ :: v_dual_mov_b32 v165 /*v677*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v166 /*v678*/, v186 /*v954*/ :: v_dual_mov_b32 v167 /*v679*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v168 /*v680*/, v186 /*v954*/ :: v_dual_mov_b32 v169 /*v681*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v170 /*v682*/, v186 /*v954*/ :: v_dual_mov_b32 v171 /*v683*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v156 /*v668*/, v186 /*v954*/ :: v_dual_mov_b32 v157 /*v669*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v158 /*v670*/, v186 /*v954*/ :: v_dual_mov_b32 v159 /*v671*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v160 /*v672*/, v186 /*v954*/ :: v_dual_mov_b32 v161 /*v673*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v162 /*v674*/, v186 /*v954*/ :: v_dual_mov_b32 v163 /*v675*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v148 /*v660*/, v186 /*v954*/ :: v_dual_mov_b32 v149 /*v661*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v150 /*v662*/, v186 /*v954*/ :: v_dual_mov_b32 v151 /*v663*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v152 /*v664*/, v186 /*v954*/ :: v_dual_mov_b32 v153 /*v665*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v154 /*v666*/, v186 /*v954*/ :: v_dual_mov_b32 v155 /*v667*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v140 /*v652*/, v186 /*v954*/ :: v_dual_mov_b32 v141 /*v653*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v142 /*v654*/, v186 /*v954*/ :: v_dual_mov_b32 v143 /*v655*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v144 /*v656*/, v186 /*v954*/ :: v_dual_mov_b32 v145 /*v657*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v146 /*v658*/, v186 /*v954*/ :: v_dual_mov_b32 v147 /*v659*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v132 /*v644*/, v186 /*v954*/ :: v_dual_mov_b32 v133 /*v645*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v134 /*v646*/, v186 /*v954*/ :: v_dual_mov_b32 v135 /*v647*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v136 /*v648*/, v186 /*v954*/ :: v_dual_mov_b32 v137 /*v649*/, v186 /*v954*/
	;Sim: Cache($$)
	v_dual_mov_b32 v138 /*v650*/, v186 /*v954*/ :: v_dual_mov_b32 v139 /*v651*/, v186 /*v954*/
	s_mov_b32 s25, s66
	s_mul_f32 s1, s1, 0x4f7ffffe
	s_mov_b64 s[54:55], s[26:27]
	s_mov_b64 s[52:53], s[24:25]
	s_mov_b32 s9, s31
	s_mov_b64 s[58:59], s[26:27]
	s_mov_b64 s[56:57], s[24:25]
	s_cvt_u32_f32 s1, s1
	s_mov_b32 s26, s10
	s_mov_b32 s27, s10
	;Sim: Stall:1 [RAW hazard]
	s_mul_i32 s0, s0, s1
	s_mov_b64 s[62:63], s[26:27]
	s_mov_b64 s[60:61], s[24:25]
	s_mul_hi_u32 s0, s1, s0
	s_mov_b64 s[50:51], s[26:27]
	s_mov_b64 s[48:49], s[24:25]
	s_add_co_i32 s1, s1, s0
	s_mov_b64 s[46:47], s[26:27]
	s_mov_b64 s[44:45], s[24:25]
	s_mul_hi_u32 s45, s34, s1
	s_mov_b64 s[42:43], s[26:27]
	s_mov_b64 s[40:41], s[24:25]
	;Sim: Stall:1 [RegBank conflict]
	s_mul_i32 s0, s45, s69
	s_add_co_i32 s41, s45, 1
	s_sub_co_i32 s42, s34, s0
	s_mov_b64 s[38:39], s[26:27]
	s_mov_b64 s[36:37], s[24:25]
	s_sub_co_i32 s37, s42, s69
	s_cmp_ge_u32 s42, s69
	s_mov_b64 s[0:1], s[24:25]
	s_mov_b64 s[2:3], s[26:27]
	;Sim: Stall:1 [RegBank conflict]
	s_cselect_b32 s1, s41, s45
	s_cselect_b32 s2, s37, s42
	s_add_co_i32 s3, s1, 1
	s_cmp_ge_u32 s2, s69
	s_cselect_b32 s1, s3, s1
	;Sim: Stall:1 [RAW hazard]
	s_xor_b32 s1, s1, s68
	;Sim: Stall:1 [RAW hazard]
	s_sub_co_i32 s1, s1, s68
	;Sim: Stall:1 [RAW hazard]
	s_lshl_b32 s2, s1, 3
	s_mul_i32 s1, s1, s67
	s_sub_co_i32 s3, s35, s2
	;Sim: Stall:1 [RAW hazard]
	s_min_i32 s3, s3, 8
	;Sim: Stall:1 [RAW hazard]
	s_abs_i32 s35, s3
	;Sim: Stall:1 [RAW hazard]
	s_cvt_f32_u32 s37, s35
	s_sub_co_i32 s38, 0, s35
	;Sim: Fused
	s_set_vgpr_msb 0x8300                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: Stall:2 [RAW hazard]
	v_rcp_iflag_f32_e32 v0, s37
	v_nop
	;Sim: Stall:5 [RAW hazard]
	v_readfirstlane_b32 s37, v0
	;Sim: Stall:4 [RAW hazard]
	s_mul_f32 s37, s37, 0x4f7ffffe
	;Sim: Stall:3 [RAW hazard]
	s_cvt_u32_f32 s37, s37
	;Sim: Stall:3 [RAW hazard]
	s_mul_i32 s38, s38, s37
	;Sim: Stall:1 [RAW hazard]
	s_mul_hi_u32 s38, s37, s38
	;Sim: Stall:1 [RAW hazard]
	s_add_co_i32 s37, s37, s38
	;Sim: Stall:1 [RAW hazard]
	s_mul_hi_u32 s38, s34, s37
	;Sim: Stall:1 [RAW hazard]
	s_mul_i32 s38, s38, s35
	;Sim: Stall:1 [RAW hazard]
	s_sub_co_i32 s34, s34, s38
	;Sim: Stall:1 [RAW hazard]
	s_sub_co_i32 s38, s34, s35
	s_cmp_ge_u32 s34, s35
	;Sim: Stall:1 [RegBank conflict]
	s_cselect_b32 s34, s38, s34
	;Sim: Stall:1 [RAW hazard]
	s_sub_co_i32 s38, s34, s35
	s_cmp_ge_u32 s34, s35
	;Sim: Stall:1 [RegBank conflict]
	s_cselect_b32 s34, s38, s34
	s_sub_co_i32 s1, ttmp9, s1
	s_xor_b32 s34, s34, s21
	s_xor_b32 s3, s1, s3
	s_sub_co_i32 s21, s34, s21
	s_ashr_i32 s3, s3, 31
	s_add_co_i32 s21, s2, s21
	s_abs_i32 s1, s1
	;Sim: Stall:1 [RAW hazard]
	s_mul_hi_u32 s2, s1, s37
	;Sim: Stall:1 [RAW hazard]
	s_add_co_i32 s34, s2, 1
	s_mul_i32 s37, s2, s35
	;Sim: Stall:1 [RAW hazard]
	s_sub_co_i32 s1, s1, s37
	;Sim: Stall:1 [RAW hazard]
	s_sub_co_i32 s37, s1, s35
	s_cmp_ge_u32 s1, s35
	;Sim: Stall:1 [RegBank conflict]
	s_cselect_b32 s1, s37, s1
	;Sim: Stall:1 [RegBank conflict]
	s_cselect_b32 s2, s34, s2
	;Sim: Stall:1 [RAW hazard]
	s_add_co_i32 s34, s2, 1
	s_cmp_ge_u32 s1, s35
	;Sim: Stall:1 [RegBank conflict]
	s_cselect_b32 s1, s34, s2
	s_add_co_i32 s89, 0, 0x45060
	s_xor_b32 s1, s1, s3
	s_lshl_b32 s79, s21, 8
	s_sub_co_i32 s1, s1, s3
	;Sim: Stall:1 [RegBank conflict]
	s_mul_i32 s2, s79, s31
	s_lshl_b32 s80, s1, 8
	s_ashr_i32 s3, s2, 31
	;Sim: Stall:1 [RegBank conflict]
	s_mul_i32 s34, s80, s64
	s_add_nc_u64 s[2:3], s[12:13], s[2:3]
	s_ashr_i32 s35, s34, 31
	s_and_b64 s[12:13], s[2:3], 0x7fffffffffffffff
	;Sim: Stall:1 [RegBank conflict]
	s_add_nc_u64 s[14:15], s[14:15], s[34:35]
	s_lshl_b32 s3, s66, 1
	s_bitset0_b32 s15, 31
	s_mul_i32 s2, s3, s21
	s_mul_i32 s34, s3, s1
	s_ashr_i32 s3, s2, 31
	s_ashr_i32 s35, s34, 31
	;Sim: Stall:1 [RegBank conflict]
	s_add_nc_u64 s[18:19], s[18:19], s[2:3]
	;Sim: Stall:1 [RegBank conflict]
	s_add_nc_u64 s[34:35], s[6:7], s[34:35]
	s_bitset0_b32 s19, 31
	s_bitset0_b32 s35, 31
	s_and_b32 s1, s30, 0xffff
	s_lshr_b64 s[2:3], s[22:23], 16
	s_and_b32 s37, s29, 0xffff0000
	s_lshl_b32 s81, s2, 16
	s_lshr_b32 s41, s2, 16
	;Sim: Stall:1 [RegBank conflict]
	s_or_b32 s81, s81, s1
	s_ashr_i32 s1, s28, 31
	s_max_i32 s70, s81, 0
	s_lshr_b32 s1, s1, 25
	s_add_co_i32 s82, 0, 0x21ff0
	s_add_co_i32 s1, s28, s1
	s_ashr_i32 s2, s30, 31
	s_ashr_i32 s1, s1, 7
	s_lshr_b32 s2, s2, 27
	s_ashr_i32 s3, s29, 31
	;Sim: Stall:1 [RegBank conflict]
	s_add_co_i32 s2, s30, s2
	s_lshr_b32 s3, s3, 25
	s_lshl_b32 s2, s2, 2
	s_add_co_i32 s3, s29, s3
	s_and_b32 s83, s2, 0xffffff80
	s_ashr_i32 s45, s3, 7
	s_lshl_b32 s91, s5, 5
	s_and_b32 s47, s5, 3
	s_add_co_i32 s90, 0, 0x43fe0
	s_lshl_b32 s49, s47, 6
	s_mul_i32 s50, s47, 0x4400
	s_mul_i32 s68, s31, s49
	s_add_co_i32 s31, s50, 0
	s_ashr_i32 s69, s68, 31
	s_mov_b32 s53, s31
	;Sim: Stall:1 [RegBank conflict]
	s_add_nc_u64 s[38:39], s[12:13], s[68:69]
	s_lshl_b32 s2, s5, 8
	s_or_b32 s55, s39, 0x80000000
	s_and_b32 s69, s2, 0x200
	;Sim: Stall:1 [RAW hazard]
	s_sub_co_i32 s2, s83, s69
	s_and_b32 s73, s5, 1
	s_max_i32 s2, s2, 0
	s_mul_i32 s84, s66, s73
	s_lshl_b32 s21, s2, 16
	s_add_co_i32 s6, s84, s69
	s_mov_b32 s72, s2
	s_ashr_i32 s7, s6, 31
	;Sim: Stall:1 [RegBank conflict]
	s_sub_co_i32 s1, s1, s73
	;Sim: Stall:1 [RegBank conflict]
	s_add_nc_u64 s[42:43], s[18:19], s[6:7]
	s_max_i32 s3, s1, 0
	s_or_b32 s59, s43, 0x80000000
	s_max_i32 s46, s30, 0
	;Sim: Stall:1 [RAW hazard]
	s_lshl_b32 s5, s46, 16
	s_lshr_b64 s[22:23], s[2:3], 16
	s_lshr_b32 s1, s3, 16
	s_mov_b32 s58, s42
	s_or_b32 s23, s1, 0x2000000
	s_and_b32 s85, s91, 64
	;Sim: Stall:1 [RegBank conflict]
	s_add_nc_u64 s[74:75], s[34:35], s[6:7]
	v_lshl_or_b32 v0, s85, 7, v1
	;Sim: Cache($)
	v_or_b32_e32 v1, s91, v2
	;Sim: Fused
	s_set_vgpr_msb 0xc0                     ;  msbs: dst=3 src0=0 src1=0 src2=0
	;Sim: Stall:3 [RAW hazard]
	v_or_b32_e32 v211 /*v979*/, v0, v112
	v_lshrrev_b32_e32 v216 /*v984*/, 4, v0
	;Sim: Fused
	s_set_vgpr_msb 0xc000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	v_lshlrev_b32_e32 v2, 8, v1
	;Sim: Cache($)
	v_or_b32_e32 v1, 0x1000, v0
	s_or_b32 s51, s75, 0x80000000
	s_brev_b32 s2, s47
	;Sim: Fused
	s_set_vgpr_msb 0xcf                     ;  msbs: dst=3 src0=3 src1=3 src2=0
	v_add_nc_u32_e32 v217 /*v985*/, v211 /*v979*/, v216 /*v984*/
	s_lshr_b32 s6, s2, 21
	s_lshr_b32 s1, s2, 26
	;Sim: Stall:1 [RegBank conflict]
	s_sub_co_i32 s2, s37, s49
	;Sim: Fused
	s_set_vgpr_msb 0xcf0c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	v_add_nc_u32_e32 v3, 0, v217 /*v985*/
	s_add_co_i32 s2, s2, s41
	s_or_b32 s1, s1, s6
	s_max_i32 s71, s2, 0
	s_add_co_i32 s86, s90, s1
	s_mul_i32 s66, s64, s49
	s_mov_b32 s57, s86
	s_ashr_i32 s67, s66, 31
	s_lshr_b64 s[92:93], s[70:71], 16
	;Sim: Stall:1 [RegBank conflict]
	s_add_nc_u64 s[76:77], s[14:15], s[66:67]
	s_lshr_b32 s67, s71, 16
	s_or_b32 s63, s77, 0x80000000
	s_bitset1_b32 s67, 24
	s_mov_b32 s62, s76
	s_sub_co_i32 s2, s28, s49
	;Sim: Stall:1 [RegBank conflict]
	s_add_co_i32 s87, s89, s1
	s_max_i32 s47, s2, 0
	s_mov_b32 s49, s87
	;Sim: Stall:1 [RegBank conflict]
	s_add_co_i32 s88, s82, s50
	s_lshr_b64 s[6:7], s[46:47], 16
	s_mov_b32 s61, s88
	s_lshr_b32 s2, s47, 16
	s_mov_b32 s54, s38
	s_or_b32 s7, s2, 0x1000000
	;Sim: Stall:1 [RAW hazard]
	tensor_load_to_lds s[52:55], s[4:11]
	;Sim: Stall:1 [FU busy]
	tensor_load_to_lds s[56:59], s[20:27]
	;Sim: Fused
	s_set_vgpr_msb 0xcc0                    ;  msbs: dst=3 src0=0 src1=0 src2=0
	v_lshrrev_b32_e32 v218 /*v986*/, 4, v1
	;Sim: Fused
	s_set_vgpr_msb 0xc000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	v_and_b32_e32 v6, 0x2f00, v2
	;Sim: Cache($)
	v_or_b32_e32 v2, 0x4000, v0
	;Sim: Cache($)
	v_or_b32_e32 v7, 0x5000, v0
	;Sim: Cache($)
	v_or_b32_e32 v8, 0x8000, v0
	;Sim: Fused
	s_set_vgpr_msb 0xcf                     ;  msbs: dst=3 src0=3 src1=3 src2=0
	v_add_nc_u32_e32 v222 /*v990*/, v218 /*v986*/, v211 /*v979*/
	;Sim: Fused
	s_set_vgpr_msb 0xcf00                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: Cache($)
	v_or_b32_e32 v9, 0x9000, v0
	s_lshl_b32 s5, s70, 16
	;Sim: Stall:1 [RegBank conflict]
	s_sub_co_i32 s2, s45, s73
	s_mov_b32 s50, s74
	s_max_i32 s73, s2, 0
	s_mov_b32 s6, s92
	s_lshr_b32 s52, s73, 16
	s_mov_b64 s[98:99], s[10:11]
	s_mov_b64 s[96:97], s[8:9]
	s_mov_b64 s[94:95], s[6:7]
	s_mov_b64 s[92:93], s[4:5]
	s_bitset1_b32 s52, 25
	s_mov_b32 s95, s67
	s_mov_b32 s97, s64
	;Sim: Stall:1 [RAW hazard]
	tensor_load_to_lds s[60:63], s[92:99]
	;Sim: Fused
	s_set_vgpr_msb 0xc0                     ;  msbs: dst=3 src0=0 src1=0 src2=0
	v_dual_lshrrev_b32 v219 /*v987*/, 4, v2 :: v_dual_lshrrev_b32 v220 /*v988*/, 4, v7
	v_dual_lshrrev_b32 v221 /*v989*/, 4, v8 :: v_dual_lshrrev_b32 v242 /*v1010*/, 4, v9
	;Sim: Fused
	s_set_vgpr_msb 0xc00c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	v_add_nc_u32_e32 v1, 0, v222 /*v990*/
	;Sim: Fused
	s_set_vgpr_msb 0xccf                    ;  msbs: dst=3 src0=3 src1=3 src2=0
	;Sim: Stall:2 [RAW hazard] Cache(-$-$)
	v_dual_add_nc_u32 v223 /*v991*/, v219 /*v987*/, v211 /*v979*/ :: v_dual_add_nc_u32 v224 /*v992*/, v220 /*v988*/, v211 /*v979*/
	s_lshr_b64 s[54:55], s[72:73], 16
	s_and_b32 s53, s91, 32
	s_mov_b32 s22, s54
	s_lshl_b32 s54, s47, 16
	s_mov_b64 s[62:63], s[26:27]
	s_mov_b64 s[60:61], s[24:25]
	s_mov_b64 s[58:59], s[22:23]
	s_mov_b64 s[56:57], s[20:21]
	s_mov_b32 s59, s52
	;Sim: Stall:1 [RAW hazard]
	tensor_load_to_lds s[48:51], s[56:63]
	;Sim: Stall:59 [WaitCnt]
	s_wait_tensorcnt 0x0
	s_wait_dscnt 0x0
	s_barrier_signal -1
	s_lshl_b32 s55, s3, 16
	s_lshl_b32 s56, s71, 16
	s_lshl_b32 s57, s73, 16
	s_lshr_b32 s37, s85, 4
	s_lshr_b32 s50, s53, 3
	s_add_nc_u64 s[38:39], s[38:39], 0x100
	s_add_co_i32 s45, s31, 0x11000
	s_add_co_i32 s5, s30, 0xffffff00
	s_or_b32 s6, s39, 0x80000000
	s_max_i32 s46, s5, 0
	;Sim: Stall:1 [RAW hazard]
	s_lshl_b32 s5, s46, 16
	s_lshr_b64 s[48:49], s[46:47], 16
	s_mov_b32 s46, s38
	s_mov_b32 s47, s6
	s_mov_b32 s6, s48
	s_or_b32 s2, s69, 0x400
	s_add_nc_u64 s[38:39], s[42:43], 0x400
	s_add_co_i32 s1, s1, 0
	s_sub_co_i32 s2, s83, s2
	s_add_co_i32 s41, s1, 0x44820
	s_max_i32 s2, s2, 0
	s_or_b32 s43, s39, 0x80000000
	s_lshl_b32 s21, s2, 16
	s_lshr_b64 s[48:49], s[2:3], 16
	s_mov_b32 s42, s38
	s_mov_b32 s22, s48
	s_add_nc_u64 s[38:39], s[76:77], 0x100
	s_add_co_i32 s3, s31, 0x32ff0
	s_add_co_i32 s48, s81, 0xffffff00
	;Sim: Fused
	s_set_vgpr_msb 0xcfc0                   ;  msbs: dst=3 src0=0 src1=0 src2=0
	s_barrier_wait -1
	ds_load_b128 v[194:197] /*v[962:965]*/, v3
	ds_load_b128 v[198:201] /*v[966:969]*/, v3 offset:32
	ds_load_b128 v[202:205] /*v[970:973]*/, v3 offset:64
	ds_load_b128 v[206:209] /*v[974:977]*/, v3 offset:96
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0xc000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	ds_load_b128 v[24:27], v1 offset:4096
	ds_load_b128 v[28:31], v1 offset:4128
	ds_load_b128 v[32:35], v1 offset:4160
	ds_load_b128 v[36:39], v1 offset:4192
	v_or_b32_e32 v7, s37, v4
	;Sim: Stall:4 [VA_SSRC blocked]
	s_wait_alu depctr_vm_vsrc(4)
	;Sim: Fused
	s_set_vgpr_msb 12                       ;  msbs: dst=0 src0=0 src1=3 src2=0
	v_dual_add_nc_u32 v2, 0, v223 /*v991*/ :: v_dual_add_nc_u32 v3, 0, v224 /*v992*/
	;Sim: Fused
	s_set_vgpr_msb 0xccf                    ;  msbs: dst=3 src0=3 src1=3 src2=0
	;Sim: Cache(-$-$)
	v_dual_add_nc_u32 v225 /*v993*/, v221 /*v989*/, v211 /*v979*/ :: v_dual_add_nc_u32 v230 /*v998*/, v242 /*v1010*/, v211 /*v979*/
	;Sim: Fused
	s_set_vgpr_msb 0xcfc0                   ;  msbs: dst=3 src0=0 src1=0 src2=0
	v_or_b32_e32 v227 /*v995*/, v7, v5
	;Sim: Fused
	s_set_vgpr_msb 0xc000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: Cache($)
	v_or_b32_e32 v8, 0xc000, v0
	;Sim: Cache($)
	v_or_b32_e32 v9, 0xd000, v0
	s_max_i32 s70, s48, 0
	s_bitset1_b32 s39, 31
	;Sim: Fused
	s_set_vgpr_msb 12                       ;  msbs: dst=0 src0=0 src1=3 src2=0
	v_add_nc_u32_e32 v0, s90, v227 /*v995*/
	;Sim: Fused
	s_set_vgpr_msb 0xc00                    ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: Cache($)
	v_or_b32_e32 v10, s50, v4
	;Sim: Fused
	s_set_vgpr_msb 0xc0                     ;  msbs: dst=3 src0=0 src1=0 src2=0
	v_dual_lshrrev_b32 v228 /*v996*/, 4, v8 :: v_dual_lshrrev_b32 v229 /*v997*/, 4, v9
	;Sim: Fused
	s_set_vgpr_msb 0xc00c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	v_dual_add_nc_u32 v8, 0, v225 /*v993*/ :: v_dual_add_nc_u32 v9, 0, v230 /*v998*/
	;Sim: Fused
	s_set_vgpr_msb 0xcc0                    ;  msbs: dst=3 src0=0 src1=0 src2=0
	;Sim: Stall:2 [RAW hazard] Cache(-$)
	v_or_b32_e32 v232 /*v1000*/, v10, v5
	;Sim: Fused
	s_set_vgpr_msb 0xc0cf                   ;  msbs: dst=3 src0=3 src1=3 src2=0
	;Sim: Cache(-$-$)
	v_dual_add_nc_u32 v231 /*v999*/, v228 /*v996*/, v211 /*v979*/ :: v_dual_add_nc_u32 v237 /*v1005*/, v229 /*v997*/, v211 /*v979*/
	s_lshr_b64 s[48:49], s[70:71], 16
	s_mov_b32 s37, s3
	;Sim: Fused
	s_set_vgpr_msb 0xcf0c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: Stall:2 [RAW hazard]
	v_dual_add_nc_u32 v4, s89, v232 /*v1000*/ :: v_dual_add_nc_u32 v11, 0, v231 /*v999*/
	v_add_nc_u32_e32 v12, 0, v237 /*v1005*/
	;Sim: Fused
	s_set_vgpr_msb 0xcc3                    ;  msbs: dst=3 src0=3 src1=0 src2=0
	v_add_nc_u32_e32 v234 /*v1002*/, v214 /*v982*/, v7
	;Sim: Fused
	s_set_vgpr_msb 0xc3c0                   ;  msbs: dst=3 src0=0 src1=0 src2=0
	;Sim: Stall:1 [RegBank conflict] Cache(---)
	v_dual_lshrrev_b32 v235 /*v1003*/, 4, v6 :: v_dual_bitop2_b32 v213 /*v981*/, v6, v112 bitop3:0x54
	;Sim: Fused
	s_set_vgpr_msb 0xc000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: Cache($)
	v_or_b32_e32 v13, 0x1000, v6
	;Sim: Cache($)
	v_or_b32_e32 v14, 0x4000, v6
	s_wait_alu depctr_va_vdst(14)
	ds_load_b128 v[40:43], v2 offset:16384
	ds_load_b128 v[44:47], v2 offset:16416
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0xc0                     ;  msbs: dst=3 src0=0 src1=0 src2=0
	v_dual_lshrrev_b32 v236 /*v1004*/, 4, v13 :: v_dual_lshrrev_b32 v240 /*v1008*/, 4, v14
	s_add_nc_u64 s[58:59], s[74:75], 0x400
	s_add_co_i32 s1, s1, 0x458a0
	s_or_b32 s49, s59, 0x80000000
	s_mov_b32 s3, s73
	s_wait_alu depctr_va_vdst(12) depctr_vm_vsrc(2)
	;Sim: Fused
	s_set_vgpr_msb 0xc00c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	ds_load_2addr_b32 v[0:1], v0 offset1:2
	ds_load_b128 v[48:51], v2 offset:16448
	ds_load_b128 v[52:55], v2 offset:16480
	ds_load_b128 v[56:59], v3 offset:20480
	ds_load_b128 v[60:63], v3 offset:20512
	ds_load_b128 v[64:67], v3 offset:20544
	ds_load_b128 v[68:71], v3 offset:20576
	s_wait_alu depctr_va_vdst(9)
	ds_load_b128 v[72:75], v8 offset:32768
	s_lshr_b64 s[50:51], s[2:3], 16
	s_mov_b32 s2, s58
	s_mov_b32 s3, s49
	s_add_co_i32 s49, s30, 0xff
	v_add_nc_u32_e32 v13, s90, v234 /*v1002*/
	;Sim: Fused
	s_set_vgpr_msb 0xccf                    ;  msbs: dst=3 src0=3 src1=3 src2=0
	;Sim: Cache(---$)
	v_dual_add_nc_u32 v238 /*v1006*/, v213 /*v981*/, v235 /*v1003*/ :: v_dual_add_nc_u32 v239 /*v1007*/, v236 /*v1004*/, v213 /*v981*/
	v_add_nc_u32_e32 v241 /*v1009*/, v240 /*v1008*/, v213 /*v981*/
	;Sim: Fused
	s_set_vgpr_msb 0xcf00                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: Cache($)
	v_or_b32_e32 v14, 0x5000, v6
	;Sim: Stall:1 [VA_SSRC blocked]
	s_wait_alu depctr_vm_vsrc(5)
	v_add_nc_u32_e32 v2, 0x400, v13
	;Sim: Fused
	s_set_vgpr_msb 12                       ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: Stall:1 [RegBank conflict] Cache(--)
	v_dual_add_nc_u32 v13, s82, v238 /*v1006*/ :: v_dual_add_nc_u32 v15, s82, v239 /*v1007*/
	v_add_nc_u32_e32 v130, s82, v241 /*v1009*/
	;Sim: Fused
	s_set_vgpr_msb 0xcc0                    ;  msbs: dst=3 src0=0 src1=0 src2=0
	v_lshrrev_b32_e32 v233 /*v1001*/, 4, v14
	;Sim: Fused
	s_set_vgpr_msb 0xc000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: Cache($)
	v_or_b32_e32 v14, 0x8000, v6
	;Sim: Cache($)
	v_or_b32_e32 v194, 0x9000, v6
	;Sim: Cache($)
	v_or_b32_e32 v195, 0xc000, v6
	;Sim: Cache($)
	v_or_b32_e32 v6, 0xd000, v6
	;Sim: Fused
	s_set_vgpr_msb 0xcf                     ;  msbs: dst=3 src0=3 src1=3 src2=0
	;Sim: Cache(-$)
	v_add_nc_u32_e32 v210 /*v978*/, v233 /*v1001*/, v213 /*v981*/
	;Sim: Fused
	s_set_vgpr_msb 0xcfc0                   ;  msbs: dst=3 src0=0 src1=0 src2=0
	v_lshrrev_b32_e32 v243 /*v1011*/, 4, v14
	s_lshr_b32 s49, s49, 8
	v_dual_lshrrev_b32 v244 /*v1012*/, 4, v194 :: v_dual_lshrrev_b32 v245 /*v1013*/, 4, v195
	v_lshrrev_b32_e32 v246 /*v1014*/, 4, v6
	s_sub_co_i32 s51, 2, s49
	;Sim: Fused
	s_set_vgpr_msb 0xc00c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	v_add_nc_u32_e32 v6, s82, v210 /*v978*/
	;Sim: Fused
	s_set_vgpr_msb 0xcc0                    ;  msbs: dst=3 src0=0 src1=0 src2=0
	;Sim: Cache(-$)
	v_dual_add_nc_u32 v247 /*v1015*/, s89, v10 :: v_dual_add_nc_u32 v226 /*v994*/, s90, v7
	;Sim: Fused
	s_set_vgpr_msb 0xc000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	ds_load_2addr_b32 v[4:5], v4 offset1:2
	ds_load_b128 v[76:79], v8 offset:32800
	;Sim: Stall:2 [VA_SSRC blocked]
	s_wait_alu depctr_va_vdst(13) depctr_vm_vsrc(3)
	ds_load_2addr_b32 v[2:3], v2 offset1:2
	ds_load_b128 v[80:83], v8 offset:32832
	ds_load_b128 v[84:87], v8 offset:32864
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 64                       ;  msbs: dst=1 src0=0 src1=0 src2=0
	ds_load_b128 v[162:165] /*v[418:421]*/, v9 offset:36864
	ds_load_b128 v[166:169] /*v[422:425]*/, v9 offset:36896
	ds_load_b128 v[170:173] /*v[426:429]*/, v9 offset:36928
	;Sim: Stall:1 [FIFO full]
	ds_load_b128 v[174:177] /*v[430:433]*/, v9 offset:36960
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x4000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	ds_load_b128 v[88:91], v11 offset:49152
	ds_load_b128 v[92:95], v11 offset:49184
	ds_load_b128 v[96:99], v11 offset:49216
	ds_load_b128 v[100:103], v11 offset:49248
	ds_load_b128 v[114:117], v12 offset:53248
	ds_load_b128 v[118:121], v12 offset:53280
	;Sim: Stall:1 [FIFO full]
	ds_load_b128 v[122:125], v12 offset:53312
	;Sim: Stall:27 [FIFO full]
	ds_load_b128 v[126:129], v12 offset:53344
	ds_load_b128 v[178:181], v13
	;Sim: Stall:3 [FIFO full]
	ds_load_b128 v[182:185], v13 offset:32
	ds_load_b128 v[186:189], v13 offset:64
	ds_load_b128 v[190:193], v13 offset:96
	;Sim: Stall:1 [FIFO full]
	ds_load_b128 v[162:165], v15 offset:4096
	ds_load_b128 v[166:169], v15 offset:4128
	ds_load_b128 v[170:173], v15 offset:4160
	;Sim: Stall:1 [FIFO full]
	ds_load_b128 v[174:177], v15 offset:4192
	;Sim: Stall:1 [FIFO full]
	ds_load_b128 v[146:149], v130 offset:16384
	ds_load_b128 v[150:153], v130 offset:16416
	ds_load_b128 v[154:157], v130 offset:16448
	ds_load_b128 v[158:161], v130 offset:16480
	s_wait_alu depctr_vm_vsrc(0)
	ds_load_b128 v[130:133], v6 offset:20480
	ds_load_b128 v[134:137], v6 offset:20512
	ds_load_b128 v[138:141], v6 offset:20544
	;Sim: Stall:27 [FIFO full]
	ds_load_b128 v[142:145], v6 offset:20576
	;Sim: Stall:49 [WaitCnt]
	s_wait_dscnt 0x0
	s_barrier_signal -1
	s_barrier_wait -1
	tensor_load_to_lds s[44:47], s[4:11]
	;Sim: Stall:1 [FU busy]
	tensor_load_to_lds s[40:43], s[20:27]
	s_lshl_b32 s5, s70, 16
	s_mov_b32 s6, s48
	s_mov_b32 s22, s50
	s_mov_b64 s[46:47], s[10:11]
	s_mov_b64 s[44:45], s[8:9]
	s_mov_b64 s[42:43], s[6:7]
	s_mov_b64 s[40:41], s[4:5]
	s_mov_b32 s43, s67
	s_mov_b32 s45, s64
	;Sim: Stall:1 [RAW hazard]
	tensor_load_to_lds s[36:39], s[40:47]
	s_mov_b64 s[42:43], s[26:27]
	s_mov_b64 s[40:41], s[24:25]
	s_mov_b64 s[38:39], s[22:23]
	s_mov_b64 s[36:37], s[20:21]
	s_mov_b32 s39, s52
	s_mov_b32 s41, s25
	;Sim: Stall:1 [RAW hazard]
	tensor_load_to_lds s[0:3], s[36:43]
;=== Block (loop): Cold=1215cyc Warm=1248cyc Trip=32 Scaled=39903cyc [header] ===
;  VALU:43(VOPD:1) SALU:92 WMMA:128 DS:136 TDM:4 Ctrl:226
;  Stall: 809 cycles (67%)
;    FU:649 | WMMACoExec:138(VALU:36+MEM:43+Other:59) | VaSSRC:3 | RAW:2 | RegBankInWMMA:8 (not counted) | MSBExposed:17 (+38 masked) | ISlot:21/65 (wasted:44)
;      FU: XDL:645 VALU:4
;  Speedup: 0.97x warm vs cold
;  WMMA efficiency: 1024 / 1215 cycles (84%)
.LBB0_1:                                ; =>This Inner Loop Header: Depth=1
	;Sim: WMMA[2/10] E OK
	s_and_b32 s0, s78, 1
	;Sim: WMMA[3/10] I OK | Stall:31 [WaitCnt]
	s_wait_dscnt 0x0
	s_barrier_signal -1
	s_mul_i32 s1, s0, 0x840
	;Sim: Fused
	s_set_vgpr_msb 0x5c                     ;  msbs: dst=1 src0=0 src1=3 src2=1
	;Sim: 0EEIEEIIVV
	v_wmma_scale_f32_16x16x128_f8f6f4 v[196:203] /*v[452:459]*/, v[178:193], v[194:209] /*v[962:977]*/, v[196:203] /*v[452:459]*/, v4, v0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_mul_i32 s0, s0, 0x11000
	;Sim: WMMA[2/10] E OK
	s_add_co_i32 s40, s78, 1
	;Sim: Fused
	s_set_vgpr_msb 0x5c0c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[3/10] I OK
	v_dual_add_nc_u32 v226, s1, v247 /*v1015*/ :: v_dual_add_nc_u32 v227, s1, v226 /*v994*/
	;Sim: Fused
	s_set_vgpr_msb 0xc04                    ;  msbs: dst=0 src0=0 src1=1 src2=0
	;Sim: WMMA[4/10] E BLOCKED | Stall:6
	v_add_nc_u32_e32 v8, v226, v194 /*v450*/
	;Sim: Fused
	s_set_vgpr_msb 0x40c                    ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: 0EEIEEIIVV Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[16:23], v[162:177], v[194:209] /*v[962:977]*/, v[16:23], v4, v0 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:3 Cache($-)
	v_add_nc_u32_e32 v9, v226, v214 /*v982*/
	;Sim: WMMA[4/10] E OK
	s_add_co_i32 s6, s82, s0
	;Sim: WMMA[5/10] E OK
	s_add_co_i32 s0, s78, s51
	;Sim: WMMA[6/10] I OK
	v_add_nc_u32_e32 v228, s6, v213 /*v981*/
	;Sim: WMMA[7/10] I | Stall:4 [VA_SSRC blocked]
	s_wait_alu depctr_vm_vsrc(0)
	;Sim: Fused
	s_set_vgpr_msb 0xc00                    ;  msbs: dst=0 src0=0 src1=0 src2=0
	v_add_nc_u32_e32 v6, 0x400, v9
	;Sim: Fused
	s_set_vgpr_msb 0xf0                     ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV
	v_wmma_scale_f32_16x16x128_f8f6f4 v[178:185] /*v[946:953]*/, v[178:193], v[24:39], v[178:185] /*v[946:953]*/, v4, v0 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf04c                   ;  msbs: dst=1 src0=0 src1=3 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:3
	v_add_nc_u32_e32 v146 /*v402*/, v228, v243 /*v1011*/
	;Sim: WMMA[4/10] E OK
	s_lshr_b32 s0, s0, 31
	;Sim: WMMA[5/10] E OK
	s_lshl_b32 s44, s33, 8
	;Sim: Fused
	s_set_vgpr_msb 0x4c00                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[6/10] I OK
	v_add_nc_u32_e32 v7, 0x400, v8
	;Sim: Fused
	s_set_vgpr_msb 0xf0                     ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[170:177] /*v[938:945]*/, v[162:177], v[24:39], v[170:177] /*v[938:945]*/, v4, v0 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_and_b32 s42, s33, 1
	;Sim: WMMA[2/10] E OK
	s_add_co_i32 s2, s44, s68
	;Sim: WMMA[3/10] I OK
	s_barrier_wait -1
	;Sim: WMMA[4/10] E OK
	s_mul_i32 s41, s42, 0x11000
	;Sim: WMMA[5/10] E OK
	s_ashr_i32 s3, s2, 31
	;Sim: Fused
	s_set_vgpr_msb 0xf04c                   ;  msbs: dst=1 src0=0 src1=3 src2=0
	;Sim: WMMA[6/10] I OK Cache($-)
	v_add_nc_u32_e32 v147 /*v403*/, v228, v244 /*v1012*/
	;Sim: Fused
	s_set_vgpr_msb 0x4cfc                   ;  msbs: dst=3 src0=0 src1=3 src2=3
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[162:169] /*v[930:937]*/, v[146:161], v[194:209] /*v[962:977]*/, v[162:169] /*v[930:937]*/, v5, v0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_add_nc_u64 s[2:3], s[12:13], s[2:3]
	;Sim: WMMA[2/10] E OK
	s_add_co_i32 s1, s31, s41
	;Sim: Fused
	s_set_vgpr_msb 0xfc4c                   ;  msbs: dst=1 src0=0 src1=3 src2=0
	;Sim: WMMA[3/10] I OK Cache($-)
	v_add_nc_u32_e32 v154 /*v410*/, v228, v245 /*v1013*/
	;Sim: WMMA[4/10] E OK
	s_sub_co_i32 s5, s30, s44
	;Sim: WMMA[5/10] E OK
	s_bitset1_b32 s3, 31
	;Sim: Fused
	s_set_vgpr_msb 0x4cfc                   ;  msbs: dst=3 src0=0 src1=3 src2=3
	;Sim: WMMA[6/10] I OK Cache($-)
	v_add_nc_u32_e32 v248 /*v1016*/, v228, v246 /*v1014*/
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[154:161] /*v[922:929]*/, v[130:145], v[194:209] /*v[962:977]*/, v[154:161] /*v[922:929]*/, v5, v0 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_max_i32 s21, s5, 0
	;Sim: WMMA[2/10] E OK
	s_mov_b32 s11, s10
	;Sim: WMMA[3/10] I OK
	s_lshl_b32 s5, s21, 16
	;Sim: Fused
	s_set_vgpr_msb 0xfc3c                   ;  msbs: dst=0 src0=0 src1=3 src2=3
	;Sim: WMMA[4/10] E BLOCKED | Stall:2 Cache($)
	v_add3_u32 v8, 0xfffde010, s6, v211 /*v979*/
	;Sim: WMMA[7/10] I | Stall:4 [VA_SSRC blocked]
	s_wait_alu depctr_va_vdst(10)
	ds_load_2addr_b32 v[14:15], v6 offset1:2
	;Sim: Cache($-)
	v_add_nc_u32_e32 v229, v228, v240 /*v1008*/
	;Sim: Fused
	s_set_vgpr_msb 0x3cf0                   ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV
	v_wmma_scale_f32_16x16x128_f8f6f4 v[146:153] /*v[914:921]*/, v[146:161], v[24:39], v[146:153] /*v[914:921]*/, v5, v0 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_alu depctr_va_vdst(10)
	;Sim: Fused
	s_set_vgpr_msb 0xf041                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[82:85] /*v[338:341]*/, v146 /*v402*/ offset:32768
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x410c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[4/10] E BLOCKED | Stall:2
	v_add_nc_u32_e32 v254, v8, v229 /*v997*/
	;Sim: WMMA[7/10] I OK
	s_wait_alu depctr_va_vdst(10) depctr_vm_vsrc(1)
	;Sim: WMMA[8/10] V OK
	ds_load_2addr_b32 v[6:7], v7 offset0:128 offset1:130
	;Sim: WMMA[9/10] V OK
	s_lshr_b32 s6, s21, 16
	;Sim: Cache($-)
	v_add_nc_u32_e32 v9, v8, v216 /*v984*/
	;Sim: Fused
	s_set_vgpr_msb 0xcf0                    ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[138:145] /*v[906:913]*/, v[130:145], v[24:39], v[138:145] /*v[906:913]*/, v5, v0 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf041                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[86:89] /*v[342:345]*/, v146 /*v402*/ offset:32800
	;Sim: WMMA[2/10] E OK
	s_or_b32 s6, s6, s54
	;Sim: Fused
	s_set_vgpr_msb 0x410c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[3/10] I OK Cache($-)
	v_add_nc_u32_e32 v210, v8, v218 /*v986*/
	;Sim: Fused
	s_set_vgpr_msb 0xc41                    ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[90:93] /*v[346:349]*/, v146 /*v402*/ offset:32832
	;Sim: WMMA[5/10] E OK
	s_mulk_i32 s42, 0x840
	;Sim: Fused
	s_set_vgpr_msb 0x410c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[6/10] I OK Cache($-)
	v_add_nc_u32_e32 v230, v8, v219 /*v987*/
	;Sim: Fused
	s_set_vgpr_msb 0xcf0                    ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[130:137] /*v[898:905]*/, v[178:193], v[40:55], v[130:137] /*v[898:905]*/, v4, v1
	;Sim: Fused
	s_set_vgpr_msb 0xf041                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[94:97] /*v[350:353]*/, v146 /*v402*/ offset:32864
	;Sim: WMMA[2/10] E OK
	s_lshl_b32 s21, s33, 10
	;Sim: Fused
	s_set_vgpr_msb 0x410c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[3/10] I OK Cache($-)
	v_add_nc_u32_e32 v242, v8, v220 /*v988*/
	;Sim: Fused
	s_set_vgpr_msb 0xc41                    ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[18:21] /*v[274:277]*/, v147 /*v403*/ offset:36864
	;Sim: WMMA[5/10] E OK
	s_or_b32 s21, s21, s69
	;Sim: Fused
	s_set_vgpr_msb 0x410c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[6/10] I OK Cache($-)
	v_add_nc_u32_e32 v243, v8, v221 /*v989*/
	;Sim: Fused
	s_set_vgpr_msb 0xcf0                    ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[122:129] /*v[890:897]*/, v[162:177], v[40:55], v[122:129] /*v[890:897]*/, v4, v1 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf041                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[22:25] /*v[278:281]*/, v147 /*v403*/ offset:36896
	;Sim: WMMA[2/10] E OK
	s_add_co_i32 s36, s21, s84
	;Sim: Fused
	s_set_vgpr_msb 0x410c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[3/10] I OK Cache($-)
	v_add_nc_u32_e32 v244, v8, v242 /*v1010*/
	;Sim: Fused
	s_set_vgpr_msb 0xc41                    ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[26:29] /*v[282:285]*/, v147 /*v403*/ offset:36928
	;Sim: WMMA[5/10] E OK
	s_ashr_i32 s37, s36, 31
	;Sim: Fused
	s_set_vgpr_msb 0x414c                   ;  msbs: dst=1 src0=0 src1=3 src2=0
	;Sim: WMMA[7/10] I OK Cache($-)
	v_add_nc_u32_e32 v148 /*v404*/, v8, v228 /*v996*/
	;Sim: Fused
	s_set_vgpr_msb 0x4cf0                   ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:2
	v_wmma_scale_f32_16x16x128_f8f6f4 v[114:121] /*v[882:889]*/, v[178:193], v[56:71], v[114:121] /*v[882:889]*/, v4, v1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf041                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[30:33] /*v[286:289]*/, v147 /*v403*/ offset:36960
	;Sim: WMMA[2/10] E OK
	s_sub_co_i32 s21, s83, s21
	;Sim: Fused
	s_set_vgpr_msb 0x410c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[3/10] I OK
	v_add_nc_u32_e32 v8, v227, v212 /*v980*/
	;Sim: WMMA[4/10] E OK
	s_wait_alu depctr_va_vdst(11)
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[202:205], v9 offset:192
	;Sim: WMMA[6/10] I OK
	s_max_i32 s22, s21, 0
	;Sim: Fused
	s_set_vgpr_msb 0xc04                    ;  msbs: dst=0 src0=0 src1=1 src2=0
	;Sim: WMMA[7/10] I OK Cache($-)
	v_add_nc_u32_e32 v227, v227, v194 /*v450*/
	;Sim: Fused
	s_set_vgpr_msb 0x4f0                    ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:2 Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[106:113] /*v[874:881]*/, v[162:177], v[56:71], v[106:113] /*v[874:881]*/, v4, v1 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[198:201], v9 offset:160
	;Sim: WMMA[2/10] E OK
	s_lshl_b32 s21, s22, 16
	;Sim: WMMA[3/10] I OK
	v_add_nc_u32_e32 v12, 0x400, v227
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[194:197], v9 offset:128
	;Sim: WMMA[5/10] E OK
	s_lshr_b32 s43, s22, 16
	;Sim: Fused
	s_set_vgpr_msb 12                       ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[6/10] I OK
	v_add_nc_u32_e32 v13, v228, v235 /*v1003*/
	;Sim: Fused
	s_set_vgpr_msb 0xcf0                    ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[98:105] /*v[866:873]*/, v[146:161], v[40:55], v[98:105] /*v[866:873]*/, v5, v1
	;Sim: Fused
	s_set_vgpr_msb 0xf00c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[206:209], v9 offset:224
	;Sim: WMMA[2/10] E OK
	s_or_b32 s22, s43, s55
	;Sim: WMMA[3/10] I OK Cache($-)
	v_add_nc_u32_e32 v211, v228, v236 /*v1004*/
	;Sim: Fused
	s_set_vgpr_msb 0xc41                    ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[178:181] /*v[434:437]*/, v154 /*v410*/ offset:49152
	;Sim: WMMA[5/10] E OK
	s_mov_b32 s26, s10
	;Sim: WMMA[6/10] I OK
	s_mov_b32 s27, s10
	;Sim: WMMA[7/10] I OK
	s_add_co_i32 s33, s33, 1
	;Sim: Fused
	s_set_vgpr_msb 0x410c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[8/10] V BLOCKED | Stall:2 Cache($-)
	v_add_nc_u32_e32 v227, v228, v233 /*v1001*/
	;Sim: Fused
	s_set_vgpr_msb 0xcf0                    ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[90:97] /*v[858:865]*/, v[130:145], v[40:55], v[90:97] /*v[858:865]*/, v5, v1 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf041                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[182:185] /*v[438:441]*/, v154 /*v410*/ offset:49184
	;Sim: WMMA[2/10] E OK
	s_add_co_i32 s38, s44, s66
	;Sim: Fused
	s_set_vgpr_msb 0x410c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[3/10] I OK
	v_add_nc_u32_e32 v212, v226, v212 /*v980*/
	;Sim: Fused
	s_set_vgpr_msb 0xc41                    ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[186:189] /*v[442:445]*/, v154 /*v410*/ offset:49216
	;Sim: WMMA[5/10] E OK
	s_and_b32 s46, s40, 1
	;Sim: WMMA[6/10] I OK
	s_ashr_i32 s39, s38, 31
	;Sim: WMMA[7/10] I OK
	s_mul_i32 s45, s46, 0x11000
	;Sim: WMMA[8/10] V OK
	s_sub_co_i32 s44, s81, s44
	;Sim: WMMA[9/10] V OK
	s_add_co_i32 s47, s45, 0
	;Sim: Fused
	s_set_vgpr_msb 0x41f0                   ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV
	v_wmma_scale_f32_16x16x128_f8f6f4 v[82:89] /*v[850:857]*/, v[146:161], v[56:71], v[82:89] /*v[850:857]*/, v5, v1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_2addr_b32 v[10:11], v8 offset0:128 offset1:130
	;Sim: WMMA[2/10] E OK
	s_max_i32 s44, s44, 0
	;Sim: Fused
	s_set_vgpr_msb 0xcc                     ;  msbs: dst=3 src0=0 src1=3 src2=0
	;Sim: WMMA[3/10] I OK
	v_add_nc_u32_e32 v249 /*v1017*/, s47, v217 /*v985*/
	;Sim: WMMA[4/10] E | Stall:4 [VA_SSRC blocked]
	s_wait_alu depctr_va_vdst(8) depctr_vm_vsrc(0)
	;Sim: Fused
	s_set_vgpr_msb 0xcc00                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[9/10] V OK
	ds_load_2addr_b32 v[8:9], v12 offset0:128 offset1:130
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0xcc                     ;  msbs: dst=3 src0=0 src1=3 src2=0
	v_add_nc_u32_e32 v250 /*v1018*/, s47, v222 /*v990*/
	;Sim: Fused
	s_set_vgpr_msb 0xccf0                   ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[74:81] /*v[842:849]*/, v[130:145], v[56:71], v[74:81] /*v[842:849]*/, v5, v1 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 | Stall:3 [VA_SSRC blocked]
	s_wait_alu depctr_va_vdst(9)
	;Sim: Fused
	s_set_vgpr_msb 0xf040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[106:109] /*v[362:365]*/, v13 offset:192
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x40cc                   ;  msbs: dst=3 src0=0 src1=3 src2=0
	;Sim: WMMA[5/10] E BLOCKED | Stall:1
	v_add_nc_u32_e32 v251 /*v1019*/, s47, v237 /*v1005*/
	;Sim: Fused
	s_set_vgpr_msb 0xcc40                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[7/10] I OK
	ds_load_b128 v[102:105] /*v[358:361]*/, v13 offset:160
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x40cc                   ;  msbs: dst=3 src0=0 src1=3 src2=0
	;Sim: WMMA[8/10] V BLOCKED | Stall:2
	v_add_nc_u32_e32 v252 /*v1020*/, s47, v231 /*v999*/
	;Sim: Fused
	s_set_vgpr_msb 0xccf0                   ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV
	v_wmma_scale_f32_16x16x128_f8f6f4 v[66:73] /*v[834:841]*/, v[178:193], v[72:87], v[66:73] /*v[834:841]*/, v4, v2
	;Sim: Fused
	s_set_vgpr_msb 0xf040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[98:101] /*v[354:357]*/, v13 offset:128
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x40cc                   ;  msbs: dst=3 src0=0 src1=3 src2=0
	;Sim: WMMA[2/10] E BLOCKED | Stall:1
	v_add_nc_u32_e32 v253 /*v1021*/, s47, v230 /*v998*/
	;Sim: Fused
	s_set_vgpr_msb 0xcc40                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[110:113] /*v[366:369]*/, v13 offset:224
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x40cc                   ;  msbs: dst=3 src0=0 src1=3 src2=0
	;Sim: WMMA[5/10] E BLOCKED | Stall:1
	v_add_nc_u32_e32 v254 /*v1022*/, s47, v225 /*v993*/
	;Sim: Fused
	s_set_vgpr_msb 0xccf0                   ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[58:65] /*v[826:833]*/, v[162:177], v[72:87], v[58:65] /*v[826:833]*/, v4, v2 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 | Stall:2 [VA_SSRC blocked]
	s_wait_alu depctr_va_vdst(10) depctr_vm_vsrc(0)
	;Sim: Fused
	s_set_vgpr_msb 0xf000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[3/10] I OK
	ds_load_2addr_b32 v[12:13], v212 offset0:128 offset1:130
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0xcc                     ;  msbs: dst=3 src0=0 src1=3 src2=0
	;Sim: WMMA[4/10] E BLOCKED | Stall:2
	v_add_nc_u32_e32 v255 /*v1023*/, s47, v224 /*v992*/
	;Sim: Fused
	s_set_vgpr_msb 0xcc40                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[7/10] I OK
	ds_load_b128 v[134:137] /*v[390:393]*/, v211 offset:4256
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x400c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[8/10] V BLOCKED | Stall:2
	v_add_nc_u32_e32 v104, s47, v223 /*v991*/
	;Sim: Fused
	s_set_vgpr_msb 0xc40                    ;  msbs: dst=1 src0=0 src1=0 src2=0
	ds_load_b128 v[130:133] /*v[386:389]*/, v211 offset:4224
	ds_load_b128 v[138:141] /*v[394:397]*/, v211 offset:4288
	ds_load_b128 v[142:145] /*v[398:401]*/, v211 offset:4320
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x4000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	ds_load_b128 v[222:225], v210 offset:4320
	s_add_co_i32 s47, s82, s45
	;Sim: Fused
	s_set_vgpr_msb 0xf4                     ;  msbs: dst=3 src0=0 src1=1 src2=3
	;Sim: 0EEIEEIIVV Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[50:57] /*v[818:825]*/, v[178:193], v[162:177] /*v[418:433]*/, v[50:57] /*v[818:825]*/, v4, v2 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf40c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[214:217], v210 offset:4256
	;Sim: WMMA[2/10] E OK
	s_lshr_b32 s45, s44, 16
	;Sim: WMMA[3/10] I OK
	v_add_nc_u32_e32 v105, s47, v238 /*v1006*/
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[218:221], v210 offset:4288
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0xccc                    ;  msbs: dst=3 src0=0 src1=3 src2=0
	;Sim: WMMA[5/10] E BLOCKED | Stall:1
	v_add_nc_u32_e32 v215 /*v983*/, s47, v239 /*v1007*/
	;Sim: Fused
	s_set_vgpr_msb 0xccf4                   ;  msbs: dst=3 src0=0 src1=1 src2=3
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy] Cache($$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[42:49] /*v[810:817]*/, v[162:177], v[162:177] /*v[418:433]*/, v[42:49] /*v[810:817]*/, v4, v2 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf40c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:3
	v_add_nc_u32_e32 v106, s47, v210 /*v978*/
	;Sim: WMMA[4/10] E | Stall:4 [VA_SSRC blocked]
	s_wait_alu depctr_vm_vsrc(0)
	;Sim: WMMA[9/10] V OK
	ds_load_b128 v[210:213], v210 offset:4224
	v_add_nc_u32_e32 v107, s47, v241 /*v1009*/
	;Sim: Fused
	s_set_vgpr_msb 0xc40                    ;  msbs: dst=1 src0=0 src1=0 src2=0
	ds_load_b128 v[122:125] /*v[378:381]*/, v229 offset:16576
	;Sim: Stall:3 [VA_SSRC blocked]
	s_mulk_i32 s46, 0x840
	;Sim: Stall:1 [RAW hazard]
	s_add_co_i32 s46, s46, 0
	ds_load_b128 v[118:121] /*v[374:377]*/, v229 offset:16544
	s_add_co_i32 s47, s46, 0x43fe0
	;Sim: Fused
	s_set_vgpr_msb 0x40f0                   ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV
	v_wmma_scale_f32_16x16x128_f8f6f4 v[34:41] /*v[802:809]*/, v[146:161], v[72:87], v[34:41] /*v[802:809]*/, v5, v2
	;Sim: Fused
	s_set_vgpr_msb 0xf040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[114:117] /*v[370:373]*/, v229 offset:16512
	;Sim: WMMA[2/10] E OK
	s_add_co_i32 s46, s46, 0x45060
	;Sim: Fused
	s_set_vgpr_msb 0x400c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[3/10] I OK
	v_add_nc_u32_e32 v108, s47, v227 /*v995*/
	;Sim: Fused
	s_set_vgpr_msb 0xc40                    ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[126:129] /*v[382:385]*/, v229 offset:16608
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x400c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[5/10] E BLOCKED | Stall:1
	v_add_nc_u32_e32 v109, s46, v232 /*v1000*/
	;Sim: Fused
	s_set_vgpr_msb 0xcf0                    ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[26:33] /*v[794:801]*/, v[130:145], v[72:87], v[26:33] /*v[794:801]*/, v5, v2 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[50:53] /*v[306:309]*/, v227 offset:20608
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x400c                   ;  msbs: dst=0 src0=0 src1=3 src2=0
	;Sim: WMMA[2/10] E BLOCKED | Stall:1
	v_add_nc_u32_e32 v110, s47, v234 /*v1002*/
	;Sim: Fused
	s_set_vgpr_msb 0xc40                    ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[62:65] /*v[318:321]*/, v227 offset:20704
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x40f4                   ;  msbs: dst=3 src0=0 src1=1 src2=3
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[18:25] /*v[786:793]*/, v[146:161], v[162:177] /*v[418:433]*/, v[18:25] /*v[786:793]*/, v5, v2 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf400                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:3
	v_add_nc_u32_e32 v110, 0x400, v110
	;Sim: WMMA[4/10] E OK
	s_mov_b32 s78, s40
	;Sim: Fused
	s_set_vgpr_msb 0xf4                     ;  msbs: dst=3 src0=0 src1=1 src2=3
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[10:17] /*v[778:785]*/, v[130:145], v[162:177] /*v[418:433]*/, v[10:17] /*v[778:785]*/, v5, v2 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf4a0                   ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[188:195] /*v[700:707]*/, v[178:193], v[88:103], v[188:195] /*v[700:707]*/, v4, v3
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[180:187] /*v[692:699]*/, v[162:177], v[88:103], v[180:187] /*v[692:699]*/, v4, v3 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[172:179] /*v[684:691]*/, v[178:193], v[114:129], v[172:179] /*v[684:691]*/, v4, v3 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[54:57] /*v[310:313]*/, v227 offset:20640
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[58:61] /*v[314:317]*/, v227 offset:20672
	;Sim: WMMA[3/10] I OK
	s_wait_alu depctr_vm_vsrc(0)
	;Sim: Fused
	s_set_vgpr_msb 0x4000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[226:229], v230 offset:16512
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[234:237], v230 offset:16576
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0xa0                     ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:2 [FU busy] Cache($$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[164:171] /*v[676:683]*/, v[162:177], v[114:129], v[164:171] /*v[676:683]*/, v4, v3 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[238:241], v230 offset:16608
	;Sim: WMMA[2/10] E OK
	s_wait_alu depctr_vm_vsrc(0)
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[230:233], v230 offset:16544
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 64                       ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[10:13] /*v[266:269]*/, v242 offset:20672
	;Sim: WMMA[6/10] I OK
	ds_load_b128 v[2:5] /*v[258:261]*/, v242 offset:20608
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x40a0                   ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[156:163] /*v[668:675]*/, v[146:161], v[88:103], v[156:163] /*v[668:675]*/, v5, v3
	;Sim: Fused
	s_set_vgpr_msb 0xa040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[6:9] /*v[262:265]*/, v242 offset:20640
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x40a0                   ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[148:155] /*v[660:667]*/, v[130:145], v[88:103], v[148:155] /*v[660:667]*/, v5, v3 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[140:147] /*v[652:659]*/, v[146:161], v[114:129], v[140:147] /*v[652:659]*/, v5, v3 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[14:17] /*v[270:273]*/, v242 offset:20704
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x4041                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[190:193] /*v[446:449]*/, v154 /*v410*/ offset:49248
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x4103                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[174:177], v248 /*v1016*/ offset:53344
	;Sim: WMMA[6/10] I OK
	ds_load_b128 v[162:165], v248 /*v1016*/ offset:53248
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x3a0                    ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy] Cache($$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[132:139] /*v[644:651]*/, v[130:145], v[114:129], v[132:139] /*v[644:651]*/, v5, v3 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa003                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[166:169], v248 /*v1016*/ offset:53280
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[170:173], v248 /*v1016*/ offset:53312
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x340                    ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[34:37] /*v[290:293]*/, v243 offset:32896
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x4051                   ;  msbs: dst=1 src0=1 src1=0 src2=1
	;Sim: WMMA[6/10] I OK
	s_wait_dscnt 0x1e
	;Sim: 0EEIEEIIVV | Stall:1 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[196:203] /*v[452:459]*/, v[98:113] /*v[354:369]*/, v[194:209], v[196:203] /*v[452:459]*/, v12, v10
	;Sim: Fused
	s_set_vgpr_msb 0x51f1                   ;  msbs: dst=3 src0=1 src1=0 src2=3
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_dscnt 0x16
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[178:185] /*v[946:953]*/, v[98:113] /*v[354:369]*/, v[210:225], v[178:185] /*v[946:953]*/, v12, v10 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf140                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[38:41] /*v[294:297]*/, v243 offset:32928
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x4001                   ;  msbs: dst=0 src0=1 src1=0 src2=0
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[16:23], v[130:145] /*v[386:401]*/, v[194:209], v[16:23], v12, v10 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0x1f1                    ;  msbs: dst=3 src0=1 src1=0 src2=3
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_dscnt 0xb
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[130:137] /*v[898:905]*/, v[98:113] /*v[354:369]*/, v[226:241], v[130:137] /*v[898:905]*/, v12, v11
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[170:177] /*v[938:945]*/, v[130:145] /*v[386:401]*/, v[210:225], v[170:177] /*v[938:945]*/, v12, v10 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[122:129] /*v[890:897]*/, v[130:145] /*v[386:401]*/, v[226:241], v[122:129] /*v[890:897]*/, v12, v11 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf1f5                   ;  msbs: dst=3 src0=1 src1=1 src2=3
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_dscnt 0x7
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[114:121] /*v[882:889]*/, v[98:113] /*v[354:369]*/, v[2:17] /*v[258:273]*/, v[114:121] /*v[882:889]*/, v12, v11 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[106:113] /*v[874:881]*/, v[130:145] /*v[386:401]*/, v[2:17] /*v[258:273]*/, v[106:113] /*v[874:881]*/, v12, v11 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf5f1                   ;  msbs: dst=3 src0=1 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[162:169] /*v[930:937]*/, v[114:129] /*v[370:385]*/, v[194:209], v[162:169] /*v[930:937]*/, v13, v10
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[146:153] /*v[914:921]*/, v[114:129] /*v[370:385]*/, v[210:225], v[146:153] /*v[914:921]*/, v13, v10 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[98:105] /*v[866:873]*/, v[114:129] /*v[370:385]*/, v[226:241], v[98:105] /*v[866:873]*/, v13, v11
	;Sim: Fused
	s_set_vgpr_msb 0xf1f5                   ;  msbs: dst=3 src0=1 src1=1 src2=3
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[82:89] /*v[850:857]*/, v[114:129] /*v[370:385]*/, v[2:17] /*v[258:273]*/, v[82:89] /*v[850:857]*/, v13, v11 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf5f1                   ;  msbs: dst=3 src0=1 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[154:161] /*v[922:929]*/, v[50:65] /*v[306:321]*/, v[194:209], v[154:161] /*v[922:929]*/, v13, v10 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[138:145] /*v[906:913]*/, v[50:65] /*v[306:321]*/, v[210:225], v[138:145] /*v[906:913]*/, v13, v10 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[90:97] /*v[858:865]*/, v[50:65] /*v[306:321]*/, v[226:241], v[90:97] /*v[858:865]*/, v13, v11 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf1f5                   ;  msbs: dst=3 src0=1 src1=1 src2=3
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[74:81] /*v[842:849]*/, v[50:65] /*v[306:321]*/, v[2:17] /*v[258:273]*/, v[74:81] /*v[842:849]*/, v13, v11 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf5fd                   ;  msbs: dst=3 src0=1 src1=3 src2=3
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[186:193] /*v[954:961]*/, v[82:97] /*v[338:353]*/, v[194:209] /*v[962:977]*/, v[186:193] /*v[954:961]*/, v14, v0
	;Sim: Fused
	s_set_vgpr_msb 0xfda1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[236:243] /*v[748:755]*/, v[82:97] /*v[338:353]*/, v[24:39], v[236:243] /*v[748:755]*/, v14, v0 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa151                   ;  msbs: dst=1 src0=1 src1=0 src2=1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[236:243] /*v[492:499]*/, v[82:97] /*v[338:353]*/, v[114:129], v[236:243] /*v[492:499]*/, v14, v3 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[252:259] /*v[508:515]*/, v[82:97] /*v[338:353]*/, v[88:103], v[252:259] /*v[508:515]*/, v14, v3
	;Sim: Fused
	s_set_vgpr_msb 0x51a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[124:131] /*v[636:643]*/, v[82:97] /*v[338:353]*/, v[40:55], v[124:131] /*v[636:643]*/, v14, v1
	;Sim: Fused
	s_set_vgpr_msb 0xa1a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[44:51] /*v[556:563]*/, v[82:97] /*v[338:353]*/, v[162:177] /*v[418:433]*/, v[44:51] /*v[556:563]*/, v14, v2 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa5a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[108:115] /*v[620:627]*/, v[82:97] /*v[338:353]*/, v[56:71], v[108:115] /*v[620:627]*/, v14, v1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[60:67] /*v[572:579]*/, v[82:97] /*v[338:353]*/, v[72:87], v[60:67] /*v[572:579]*/, v14, v2
	;Sim: Fused
	s_set_vgpr_msb 0xa140                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[42:45] /*v[298:301]*/, v243 offset:32960
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[46:49] /*v[302:305]*/, v243 offset:32992
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[66:69] /*v[322:325]*/, v244 offset:36992
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[78:81] /*v[334:337]*/, v244 offset:37088
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x40fd                   ;  msbs: dst=3 src0=1 src1=3 src2=3
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[2:9] /*v[770:777]*/, v[18:33] /*v[274:289]*/, v[194:209] /*v[962:977]*/, v[2:9] /*v[770:777]*/, v14, v0 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xfd40                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[70:73] /*v[326:329]*/, v244 offset:37024
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x4051                   ;  msbs: dst=1 src0=1 src1=0 src2=1
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[228:235] /*v[484:491]*/, v[18:33] /*v[274:289]*/, v[114:129], v[228:235] /*v[484:491]*/, v14, v3 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[244:251] /*v[500:507]*/, v[18:33] /*v[274:289]*/, v[88:103], v[244:251] /*v[500:507]*/, v14, v3 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0x51a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[36:43] /*v[548:555]*/, v[18:33] /*v[274:289]*/, v[162:177] /*v[418:433]*/, v[36:43] /*v[548:555]*/, v14, v2 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa5a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[52:59] /*v[564:571]*/, v[18:33] /*v[274:289]*/, v[72:87], v[52:59] /*v[564:571]*/, v14, v2 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[228:235] /*v[740:747]*/, v[18:33] /*v[274:289]*/, v[24:39], v[228:235] /*v[740:747]*/, v14, v0 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[100:107] /*v[612:619]*/, v[18:33] /*v[274:289]*/, v[56:71], v[100:107] /*v[612:619]*/, v14, v1 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[116:123] /*v[628:635]*/, v[18:33] /*v[274:289]*/, v[40:55], v[116:123] /*v[628:635]*/, v14, v1 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa140                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[74:77] /*v[330:333]*/, v244 offset:37056
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x4000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[246:249], v254 offset:53408
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[250:253], v254 offset:53440
	;Sim: WMMA[5/10] E OK
	s_wait_alu depctr_vm_vsrc(2)
	;Sim: WMMA[6/10] I OK
	ds_load_b128 v[242:245], v254 offset:53376
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0xa1                     ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: WMMA[8/10] V OK
	s_wait_dscnt 0xf
	;Sim: 0EEIEEIIVV | Stall:1 Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[76:83] /*v[588:595]*/, v[178:193] /*v[434:449]*/, v[56:71], v[76:83] /*v[588:595]*/, v15, v1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa151                   ;  msbs: dst=1 src0=1 src1=0 src2=1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[204:211] /*v[460:467]*/, v[178:193] /*v[434:449]*/, v[114:129], v[204:211] /*v[460:467]*/, v15, v3 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[220:227] /*v[476:483]*/, v[178:193] /*v[434:449]*/, v[88:103], v[220:227] /*v[476:483]*/, v15, v3
	;Sim: Fused
	s_set_vgpr_msb 0x51ad                   ;  msbs: dst=2 src0=1 src1=3 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[220:227] /*v[732:739]*/, v[178:193] /*v[434:449]*/, v[194:209] /*v[962:977]*/, v[220:227] /*v[732:739]*/, v15, v0
	;Sim: Fused
	s_set_vgpr_msb 0xada1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[204:211] /*v[716:723]*/, v[178:193] /*v[434:449]*/, v[24:39], v[204:211] /*v[716:723]*/, v15, v0 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa1a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[12:19] /*v[524:531]*/, v[178:193] /*v[434:449]*/, v[162:177] /*v[418:433]*/, v[12:19] /*v[524:531]*/, v15, v2 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa5a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[28:35] /*v[540:547]*/, v[178:193] /*v[434:449]*/, v[72:87], v[28:35] /*v[540:547]*/, v15, v2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[92:99] /*v[604:611]*/, v[178:193] /*v[434:449]*/, v[40:55], v[92:99] /*v[604:611]*/, v15, v1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_alu depctr_vm_vsrc(0)
	;Sim: Fused
	s_set_vgpr_msb 0xa100                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[254:257], v254 offset:53472
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x41                     ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[94:97] /*v[350:353]*/, v148 /*v404*/ offset:49376
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[82:85] /*v[338:341]*/, v148 /*v404*/ offset:49280
	;Sim: WMMA[6/10] I OK
	ds_load_b128 v[86:89] /*v[342:345]*/, v148 /*v404*/ offset:49312
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x41a4                   ;  msbs: dst=2 src0=0 src1=1 src2=2
	;Sim: WMMA[8/10] V OK
	s_wait_dscnt 0xf
	;Sim: 0EEIEEIIVV | Stall:1
	v_wmma_scale_f32_16x16x128_f8f6f4 v[4:11] /*v[516:523]*/, v[162:177], v[162:177] /*v[418:433]*/, v[4:11] /*v[516:523]*/, v15, v2 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa441                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[90:93] /*v[346:349]*/, v148 /*v404*/ offset:49344
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x4101                   ;  msbs: dst=0 src0=1 src1=0 src2=0
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[130:133], v146 /*v402*/ offset:32896
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[134:137], v146 /*v402*/ offset:32928
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[138:141], v146 /*v402*/ offset:32960
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x1a0                    ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:2 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[20:27] /*v[532:539]*/, v[162:177], v[72:87], v[20:27] /*v[532:539]*/, v15, v2 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa001                   ;  msbs: dst=0 src0=1 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[142:145], v146 /*v402*/ offset:32992
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x141                    ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[174:177] /*v[430:433]*/, v147 /*v403*/ offset:37088
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[170:173] /*v[426:429]*/, v147 /*v403*/ offset:37056
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[162:165] /*v[418:421]*/, v147 /*v403*/ offset:36992
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x41a0                   ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:2 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[250:257] /*v[762:769]*/, v[162:177], v[114:129], v[250:257] /*v[762:769]*/, v15, v3 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa041                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[166:169] /*v[422:425]*/, v147 /*v403*/ offset:37024
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[158:161] /*v[414:417]*/, v154 /*v410*/ offset:49376
	;Sim: WMMA[3/10] I OK
	s_wait_alu depctr_vm_vsrc(1)
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[146:149] /*v[402:405]*/, v154 /*v410*/ offset:49280
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[150:153] /*v[406:409]*/, v154 /*v410*/ offset:49312
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x41a0                   ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:2 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[84:91] /*v[596:603]*/, v[162:177], v[40:55], v[84:91] /*v[596:603]*/, v15, v1 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa050                   ;  msbs: dst=1 src0=0 src1=0 src2=1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[212:219] /*v[468:475]*/, v[162:177], v[88:103], v[212:219] /*v[468:475]*/, v15, v3 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0x50a0                   ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[68:75] /*v[580:587]*/, v[162:177], v[56:71], v[68:75] /*v[580:587]*/, v15, v1 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[196:203] /*v[708:715]*/, v[162:177], v[24:39], v[196:203] /*v[708:715]*/, v15, v0 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_alu depctr_vm_vsrc(0)
	;Sim: Fused
	s_set_vgpr_msb 0xa041                   ;  msbs: dst=1 src0=1 src1=0 src2=0
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[154:157] /*v[410:413]*/, v154 /*v410*/ offset:49344
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x4143                   ;  msbs: dst=1 src0=3 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[18:21] /*v[274:277]*/, v248 /*v1016*/ offset:53376
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x43ac                   ;  msbs: dst=2 src0=0 src1=3 src2=2
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[212:219] /*v[724:731]*/, v[162:177], v[194:209] /*v[962:977]*/, v[212:219] /*v[724:731]*/, v15, v0 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xac43                   ;  msbs: dst=1 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[22:25] /*v[278:281]*/, v248 /*v1016*/ offset:53408
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[26:29] /*v[282:285]*/, v248 /*v1016*/ offset:53440
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[30:33] /*v[286:289]*/, v248 /*v1016*/ offset:53472
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x43f5                   ;  msbs: dst=3 src0=1 src1=1 src2=3
	;Sim: WMMA[5/10] E OK
	s_wait_dscnt 0x1c
	;Sim: 0EEIEEIIVV | Stall:2 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[66:73] /*v[834:841]*/, v[98:113] /*v[354:369]*/, v[34:49] /*v[290:305]*/, v[66:73] /*v[834:841]*/, v12, v8
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_dscnt 0x18
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[50:57] /*v[818:825]*/, v[98:113] /*v[354:369]*/, v[66:81] /*v[322:337]*/, v[50:57] /*v[818:825]*/, v12, v8 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf5a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_dscnt 0x14
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[172:179] /*v[684:691]*/, v[98:113] /*v[354:369]*/, v[242:257], v[172:179] /*v[684:691]*/, v12, v9 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa1a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_dscnt 0x10
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[188:195] /*v[700:707]*/, v[98:113] /*v[354:369]*/, v[82:97] /*v[338:353]*/, v[188:195] /*v[700:707]*/, v12, v9
	;Sim: Fused
	s_set_vgpr_msb 0xa5a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[164:171] /*v[676:683]*/, v[130:145] /*v[386:401]*/, v[242:257], v[164:171] /*v[676:683]*/, v12, v9 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa1f5                   ;  msbs: dst=3 src0=1 src1=1 src2=3
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[42:49] /*v[810:817]*/, v[130:145] /*v[386:401]*/, v[66:81] /*v[322:337]*/, v[42:49] /*v[810:817]*/, v12, v8 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf5a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[180:187] /*v[692:699]*/, v[130:145] /*v[386:401]*/, v[82:97] /*v[338:353]*/, v[180:187] /*v[692:699]*/, v12, v9 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa5f5                   ;  msbs: dst=3 src0=1 src1=1 src2=3
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[58:65] /*v[826:833]*/, v[130:145] /*v[386:401]*/, v[34:49] /*v[290:305]*/, v[58:65] /*v[826:833]*/, v12, v8 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_wait_tensorcnt 0x0
	;Sim: WMMA[2/10] E OK
	s_wait_dscnt 0x0
	;Sim: WMMA[3/10] I OK
	s_barrier_signal -1
	;Sim: 0EEIEEIIVV | Stall:4 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[18:25] /*v[786:793]*/, v[114:129] /*v[370:385]*/, v[66:81] /*v[322:337]*/, v[18:25] /*v[786:793]*/, v13, v8 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[34:41] /*v[802:809]*/, v[114:129] /*v[370:385]*/, v[34:49] /*v[290:305]*/, v[34:41] /*v[802:809]*/, v13, v8
	;Sim: Fused
	s_set_vgpr_msb 0xf5a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[156:163] /*v[668:675]*/, v[114:129] /*v[370:385]*/, v[82:97] /*v[338:353]*/, v[156:163] /*v[668:675]*/, v13, v9
	;Sim: Fused
	s_set_vgpr_msb 0xa5a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[140:147] /*v[652:659]*/, v[114:129] /*v[370:385]*/, v[242:257], v[140:147] /*v[652:659]*/, v13, v9 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_barrier_wait -1
	;Sim: WMMA[2/10] E OK
	tensor_load_to_lds s[0:3], s[4:11]
	;Sim: Fused
	s_set_vgpr_msb 0xa1f5                   ;  msbs: dst=3 src0=1 src1=1 src2=3
	;Sim: 0EEIEEIIVV | Stall:5 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[10:17] /*v[778:785]*/, v[50:65] /*v[306:321]*/, v[66:81] /*v[322:337]*/, v[10:17] /*v[778:785]*/, v13, v8 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_add_nc_u64 s[2:3], s[18:19], s[36:37]
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[26:33] /*v[794:801]*/, v[50:65] /*v[306:321]*/, v[34:49] /*v[290:305]*/, v[26:33] /*v[794:801]*/, v13, v8 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_add_co_i32 s1, s86, s42
	;Sim: WMMA[2/10] E OK
	s_bitset1_b32 s3, 31
	;Sim: WMMA[3/10] I | Stall:1 [RAW hazard]
	tensor_load_to_lds s[0:3], s[20:27]
	;Sim: Fused
	s_set_vgpr_msb 0xf5a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[148:155] /*v[660:667]*/, v[50:65] /*v[306:321]*/, v[82:97] /*v[338:353]*/, v[148:155] /*v[660:667]*/, v13, v9 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_add_nc_u64 s[2:3], s[14:15], s[38:39]
	;Sim: WMMA[2/10] E OK
	s_add_co_i32 s1, s88, s41
	;Sim: WMMA[3/10] I OK
	s_bitset1_b32 s3, 31
	;Sim: Fused
	s_set_vgpr_msb 0xa5a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:4 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[132:139] /*v[644:651]*/, v[50:65] /*v[306:321]*/, v[242:257], v[132:139] /*v[644:651]*/, v13, v9 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_lshl_b32 s5, s44, 16
	;Sim: WMMA[2/10] E OK
	s_or_b32 s6, s45, s56
	;Sim: WMMA[3/10] I OK
	s_or_b32 s22, s43, s57
	;Sim: WMMA[4/10] E OK
	s_mov_b64 s[98:99], s[10:11]
	;Sim: WMMA[5/10] E OK
	s_mov_b64 s[96:97], s[8:9]
	;Sim: WMMA[6/10] I OK
	s_mov_b64 s[94:95], s[6:7]
	;Sim: WMMA[7/10] I OK
	s_mov_b64 s[92:93], s[4:5]
	;Sim: WMMA[8/10] V OK
	s_mov_b32 s95, s67
	;Sim: WMMA[9/10] V OK
	s_mov_b32 s97, s64
	;Sim: Stall:1 [RAW hazard]
	tensor_load_to_lds s[0:3], s[92:99]
	;Sim: Fused
	s_set_vgpr_msb 0xa1f0                   ;  msbs: dst=3 src0=0 src1=0 src2=3
	;Sim: 0EEIEEIIVV
	v_wmma_scale_f32_16x16x128_f8f6f4 v[186:193] /*v[954:961]*/, v[130:145], v[194:209], v[186:193] /*v[954:961]*/, v6, v10
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_add_nc_u64 s[2:3], s[34:35], s[36:37]
	;Sim: WMMA[2/10] E OK
	s_add_co_i32 s1, s87, s42
	;Sim: Fused
	s_set_vgpr_msb 0xf0a0                   ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:5 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[236:243] /*v[748:755]*/, v[130:145], v[210:225], v[236:243] /*v[748:755]*/, v6, v10 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_bitset1_b32 s3, 31
	;Sim: WMMA[2/10] E OK
	s_cmp_lg_u32 s40, s49
	;Sim: WMMA[3/10] I OK
	s_mov_b64 s[42:43], s[26:27]
	;Sim: WMMA[4/10] E OK
	s_mov_b64 s[40:41], s[24:25]
	;Sim: WMMA[5/10] E OK
	s_mov_b64 s[38:39], s[22:23]
	;Sim: WMMA[6/10] I OK
	s_mov_b64 s[36:37], s[20:21]
	;Sim: WMMA[7/10] I OK
	s_mov_b32 s39, s52
	;Sim: WMMA[8/10] V OK
	s_mov_b32 s41, s25
	;Sim: WMMA[9/10] V | Stall:1 [RAW hazard]
	tensor_load_to_lds s[0:3], s[36:43]
	;Sim: Fused
	s_set_vgpr_msb 0xa050                   ;  msbs: dst=1 src0=0 src1=0 src2=1
	s_wait_dscnt 0x0
	s_barrier_signal -1
	;Sim: 0EEIEEIIVV Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[236:243] /*v[492:499]*/, v[130:145], v[242:257], v[236:243] /*v[492:499]*/, v6, v9 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0x5054                   ;  msbs: dst=1 src0=0 src1=1 src2=1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[252:259] /*v[508:515]*/, v[130:145], v[82:97] /*v[338:353]*/, v[252:259] /*v[508:515]*/, v6, v9
	;Sim: Fused
	s_set_vgpr_msb 0x54a0                   ;  msbs: dst=2 src0=0 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[124:131] /*v[636:643]*/, v[130:145], v[226:241], v[124:131] /*v[636:643]*/, v6, v11
	;Sim: Fused
	s_set_vgpr_msb 0xa0a4                   ;  msbs: dst=2 src0=0 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[44:51] /*v[556:563]*/, v[130:145], v[66:81] /*v[322:337]*/, v[44:51] /*v[556:563]*/, v6, v8 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[108:115] /*v[620:627]*/, v[130:145], v[2:17] /*v[258:273]*/, v[108:115] /*v[620:627]*/, v6, v11 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa400                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_barrier_wait -1
	;Sim: WMMA[2/10] E OK
	ds_load_2addr_b32 v[0:1], v108 offset1:2
	;Sim: WMMA[3/10] I OK
	ds_load_2addr_b32 v[2:3], v110 offset1:2
	;Sim: WMMA[4/10] E OK
	ds_load_2addr_b32 v[4:5], v109 offset1:2
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0xa4                     ;  msbs: dst=2 src0=0 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[60:67] /*v[572:579]*/, v[130:145], v[34:49] /*v[290:305]*/, v[60:67] /*v[572:579]*/, v6, v8
	;Sim: Fused
	s_set_vgpr_msb 0xa4c3                   ;  msbs: dst=3 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[194:197] /*v[962:965]*/, v249 /*v1017*/
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[198:201] /*v[966:969]*/, v249 /*v1017*/ offset:32
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[202:205] /*v[970:973]*/, v249 /*v1017*/ offset:64
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[206:209] /*v[974:977]*/, v249 /*v1017*/ offset:96
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0xc3f1                   ;  msbs: dst=3 src0=1 src1=0 src2=3
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy]
	v_wmma_scale_f32_16x16x128_f8f6f4 v[2:9] /*v[770:777]*/, v[162:177] /*v[418:433]*/, v[194:209], v[2:9] /*v[770:777]*/, v6, v10 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xf103                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[24:27], v250 /*v1018*/ offset:4096
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[28:31], v250 /*v1018*/ offset:4128
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[32:35], v250 /*v1018*/ offset:4160
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[36:39], v250 /*v1018*/ offset:4192
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x351                    ;  msbs: dst=1 src0=1 src1=0 src2=1
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[228:235] /*v[484:491]*/, v[162:177] /*v[418:433]*/, v[242:257], v[228:235] /*v[484:491]*/, v6, v9 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0x5100                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[40:43], v104 offset:16384
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[44:47], v104 offset:16416
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[48:51], v104 offset:16448
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[52:55], v104 offset:16480
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x55                     ;  msbs: dst=1 src0=1 src1=1 src2=1
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[244:251] /*v[500:507]*/, v[162:177] /*v[418:433]*/, v[82:97] /*v[338:353]*/, v[244:251] /*v[500:507]*/, v6, v9 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0x5503                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[56:59], v255 /*v1023*/ offset:20480
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x3a5                    ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[36:43] /*v[548:555]*/, v[162:177] /*v[418:433]*/, v[66:81] /*v[322:337]*/, v[36:43] /*v[548:555]*/, v6, v8 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[52:59] /*v[564:571]*/, v[162:177] /*v[418:433]*/, v[34:49] /*v[290:305]*/, v[52:59] /*v[564:571]*/, v6, v8 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa5a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[228:235] /*v[740:747]*/, v[162:177] /*v[418:433]*/, v[210:225], v[228:235] /*v[740:747]*/, v6, v10 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa103                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[60:63], v255 /*v1023*/ offset:20512
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x3a5                    ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[100:107] /*v[612:619]*/, v[162:177] /*v[418:433]*/, v[2:17] /*v[258:273]*/, v[100:107] /*v[612:619]*/, v6, v11 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa503                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[64:67], v255 /*v1023*/ offset:20544
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[68:71], v255 /*v1023*/ offset:20576
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[72:75], v254 /*v1022*/ offset:32768
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[76:79], v254 /*v1022*/ offset:32800
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x3a1                    ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[116:123] /*v[628:635]*/, v[162:177] /*v[418:433]*/, v[226:241], v[116:123] /*v[628:635]*/, v6, v11 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa103                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[80:83], v254 /*v1022*/ offset:32832
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[84:87], v254 /*v1022*/ offset:32864
	;Sim: WMMA[3/10] I OK
	s_wait_alu depctr_va_vdst(14)
	;Sim: Fused
	s_set_vgpr_msb 0x343                    ;  msbs: dst=1 src0=3 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[162:165] /*v[418:421]*/, v253 /*v1021*/ offset:36864
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[166:169] /*v[422:425]*/, v253 /*v1021*/ offset:36896
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x43a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:2 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[76:83] /*v[588:595]*/, v[146:161] /*v[402:417]*/, v[2:17] /*v[258:273]*/, v[76:83] /*v[588:595]*/, v7, v11 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa543                   ;  msbs: dst=1 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[170:173] /*v[426:429]*/, v253 /*v1021*/ offset:36928
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[174:177] /*v[430:433]*/, v253 /*v1021*/ offset:36960
	;Sim: MSB_Exposed | Stall:1 [MSB exposed]
	s_set_vgpr_msb 0x4303                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[88:91], v252 /*v1020*/ offset:49152
	;Sim: WMMA[5/10] E OK
	ds_load_b128 v[92:95], v252 /*v1020*/ offset:49184
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x351                    ;  msbs: dst=1 src0=1 src1=0 src2=1
	;Sim: 0EEIEEIIVV | Stall:2 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[204:211] /*v[460:467]*/, v[146:161] /*v[402:417]*/, v[242:257], v[204:211] /*v[460:467]*/, v7, v9 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0x5103                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[96:99], v252 /*v1020*/ offset:49216
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[100:103], v252 /*v1020*/ offset:49248
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[114:117], v251 /*v1019*/ offset:53248
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x355                    ;  msbs: dst=1 src0=1 src1=1 src2=1
	;Sim: 0EEIEEIIVV | Stall:4 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[220:227] /*v[476:483]*/, v[146:161] /*v[402:417]*/, v[82:97] /*v[338:353]*/, v[220:227] /*v[476:483]*/, v7, v9
	;Sim: Fused
	s_set_vgpr_msb 0x55a1                   ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[220:227] /*v[732:739]*/, v[146:161] /*v[402:417]*/, v[194:209], v[220:227] /*v[732:739]*/, v7, v10
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[204:211] /*v[716:723]*/, v[146:161] /*v[402:417]*/, v[210:225], v[204:211] /*v[716:723]*/, v7, v10 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa1a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[12:19] /*v[524:531]*/, v[146:161] /*v[402:417]*/, v[66:81] /*v[322:337]*/, v[12:19] /*v[524:531]*/, v7, v8 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa503                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[118:121], v251 /*v1019*/ offset:53280
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[122:125], v251 /*v1019*/ offset:53312
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[126:129], v251 /*v1019*/ offset:53344
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x3a5                    ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:4 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[28:35] /*v[540:547]*/, v[146:161] /*v[402:417]*/, v[34:49] /*v[290:305]*/, v[28:35] /*v[540:547]*/, v7, v8
	;Sim: Fused
	s_set_vgpr_msb 0xa500                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[178:181], v105
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[182:185], v105 offset:32
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[186:189], v105 offset:64
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[190:193], v105 offset:96
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0xa1                     ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[92:99] /*v[604:611]*/, v[146:161] /*v[402:417]*/, v[226:241], v[92:99] /*v[604:611]*/, v7, v11
	;Sim: Fused
	s_set_vgpr_msb 0xa103                   ;  msbs: dst=0 src0=3 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[162:165], v215 /*v983*/ offset:4096
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[166:169], v215 /*v983*/ offset:4128
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[170:173], v215 /*v983*/ offset:4160
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[174:177], v215 /*v983*/ offset:4192
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x3a5                    ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache(-$)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[20:27] /*v[532:539]*/, v[18:33] /*v[274:289]*/, v[34:49] /*v[290:305]*/, v[20:27] /*v[532:539]*/, v7, v8 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa500                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[146:149], v107 offset:16384
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[150:153], v107 offset:16416
	;Sim: WMMA[3/10] I OK
	ds_load_b128 v[154:157], v107 offset:16448
	;Sim: WMMA[4/10] E OK
	ds_load_b128 v[158:161], v107 offset:16480
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0xa5                     ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:3 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[4:11] /*v[516:523]*/, v[18:33] /*v[274:289]*/, v[66:81] /*v[322:337]*/, v[4:11] /*v[516:523]*/, v7, v8 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa500                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[130:133], v106 offset:20480
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0xa1                     ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[212:219] /*v[724:731]*/, v[18:33] /*v[274:289]*/, v[194:209], v[212:219] /*v[724:731]*/, v7, v10 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[196:203] /*v[708:715]*/, v[18:33] /*v[274:289]*/, v[210:225], v[196:203] /*v[708:715]*/, v7, v10 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa1a5                   ;  msbs: dst=2 src0=1 src1=1 src2=2
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[68:75] /*v[580:587]*/, v[18:33] /*v[274:289]*/, v[2:17] /*v[258:273]*/, v[68:75] /*v[580:587]*/, v7, v11 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa500                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[134:137], v106 offset:20512
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0x55                     ;  msbs: dst=1 src0=1 src1=1 src2=1
	;Sim: 0EEIEEIIVV | Stall:6 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[212:219] /*v[468:475]*/, v[18:33] /*v[274:289]*/, v[82:97] /*v[338:353]*/, v[212:219] /*v[468:475]*/, v7, v9 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0x5500                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	ds_load_b128 v[138:141], v106 offset:20544
	;Sim: WMMA[2/10] E OK
	ds_load_b128 v[142:145], v106 offset:20576
	;Sim: MSB_Exposed(masked)
	s_set_vgpr_msb 0xa1                     ;  msbs: dst=2 src0=1 src1=0 src2=2
	;Sim: 0EEIEEIIVV | Stall:5 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[84:91] /*v[596:603]*/, v[18:33] /*v[274:289]*/, v[226:241], v[84:91] /*v[596:603]*/, v7, v11 matrix_a_scale:MATRIX_SCALE_ROW1
	;Sim: 0EEIEEIIVV | Stall:8 [FU busy] Cache($-)
	v_wmma_scale_f32_16x16x128_f8f6f4 v[250:257] /*v[762:769]*/, v[18:33] /*v[274:289]*/, v[242:257], v[250:257] /*v[762:769]*/, v7, v9 matrix_a_scale:MATRIX_SCALE_ROW1 matrix_b_scale:MATRIX_SCALE_ROW1
	;Sim: Fused
	s_set_vgpr_msb 0xa100                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	;Sim: WMMA[0/10] E0 BLOCKED | Stall:1
	s_cbranch_scc1 .LBB0_1
;=== Block: 2903 cycles ===
;  VALU:305 SALU:76 VMEM:128 Ctrl:41
;  Stall: 2365 cycles (81%)
;    FU:24 | WMMACoExec:5(VALU:5) | MemFIFO:1987 | VaSSRC:237 | VaVdst:18 | RAW:83 | ISFetch:11 (77 fetches) | ISlot:1/2 (wasted:1)
;      FU: VALU:24
; %bb.2:                                ; %._crit_edge
	;Sim: WMMA[2/10] E OK
	s_lshr_b32 s0, s85, 1
	;Sim: WMMA[3/10] I OK
	s_wait_dscnt 0x32
	;Sim: WMMA[4/10] E BLOCKED | Stall:2
	v_lshrrev_b32_e32 v0, 1, v112
	;Sim: WMMA[7/10] I OK
	v_or3_b32 v1, s0, v111, s79
	;Sim: WMMA[8/10] V | Stall:3 [RAW hazard]
	v_or3_b32 v0, v0, s53, s80
	s_wait_dscnt 0x31
	v_mul_lo_u32 v2, v1, s65
	;Sim: Stall:3 [FU busy]
	v_or_b32_e32 v3, 0xd0, v1
	v_cmp_gt_i32_e32 vcc_lo, s29, v0
	s_wait_dscnt 0x30
	;Sim: Stall:2 [RAW hazard]
	v_mul_lo_u32 v4, v3, s65
	;Sim: Stall:3 [FU busy] Cache($)
	v_or_b32_e32 v5, 0xc0, v1
	v_cmp_gt_i32_e64 s14, s28, v3
	;Sim: Stall:3 [RAW hazard]
	v_mul_lo_u32 v3, v5, s65
	;Sim: Stall:3 [FU busy] Cache($)
	v_or_b32_e32 v6, 0x90, v1
	v_cmp_gt_i32_e64 s13, s28, v5
	;Sim: Stall:3 [RAW hazard]
	v_mul_lo_u32 v5, v6, s65
	;Sim: Stall:3 [FU busy] Cache($)
	v_or_b32_e32 v7, 0x80, v1
	v_cmp_gt_i32_e64 s12, s28, v6
	;Sim: Stall:3 [RAW hazard]
	v_mul_lo_u32 v6, v7, s65
	;Sim: Stall:3 [FU busy] Cache($)
	v_or_b32_e32 v8, 0x50, v1
	v_cmp_gt_i32_e64 s11, s28, v7
	;Sim: Stall:3 [RAW hazard]
	v_mul_lo_u32 v7, v8, s65
	;Sim: Stall:3 [FU busy] Cache($)
	v_or_b32_e32 v9, 64, v1
	v_cmp_gt_i32_e64 s10, s28, v8
	;Sim: Stall:3 [RAW hazard]
	v_mul_lo_u32 v8, v9, s65
	;Sim: Stall:3 [FU busy] Cache($)
	v_or_b32_e32 v10, 16, v1
	v_cmp_gt_i32_e64 s9, s28, v9
	;Sim: Stall:3 [RAW hazard]
	v_mul_lo_u32 v9, v10, s65
	;Sim: Stall:3 [FU busy]
	v_cmp_gt_i32_e64 s1, s28, v10
	;Sim: Cache($)
	v_cmp_gt_i32_e64 s0, s28, v1
	;Sim: Cache($)
	v_or_b32_e32 v1, 0xd0, v0
	;Sim: Cache($)
	v_or_b32_e32 v10, 0xc0, v0
	;Sim: Cache($)
	v_or_b32_e32 v11, 0x90, v0
	;Sim: Cache($)
	v_or_b32_e32 v12, 0x80, v0
	;Sim: Cache($)
	v_or_b32_e32 v13, 0x50, v0
	v_cmp_gt_i32_e64 s8, s29, v1
	v_cmp_gt_i32_e64 s7, s29, v10
	v_cmp_gt_i32_e64 s6, s29, v11
	v_cmp_gt_i32_e64 s5, s29, v12
	v_cmp_gt_i32_e64 s3, s29, v13
	;Sim: Cache($)
	v_or_b32_e32 v14, 64, v0
	;Sim: Cache($)
	v_or_b32_e32 v15, 16, v0
	s_wait_dscnt 0x2b
	;Sim: Cache($)
	v_or_b32_e32 v24, 0xd4, v0
	;Sim: Cache($)
	v_or_b32_e32 v25, 0xc4, v0
	;Sim: Cache($)
	v_or_b32_e32 v26, 0x94, v0
	v_cmp_gt_i32_e64 s4, s29, v14
	v_cmp_gt_i32_e64 s2, s29, v15
	;Sim: Cache($)
	v_or_b32_e32 v27, 0x84, v0
	s_wait_dscnt 0x2a
	;Sim: Cache($)
	v_or_b32_e32 v28, 0x54, v0
	;Sim: Cache($)
	v_or_b32_e32 v29, 0x44, v0
	;Sim: Cache($)
	v_or_b32_e32 v30, 20, v0
	;Sim: Cache($)
	v_or_b32_e32 v31, 4, v0
	s_wait_dscnt 0x29
	;Sim: Cache(-$)
	v_add_lshl_u32 v32, v2, v0, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v33, v2, v15, 2
	;Sim: Cache(-$)
	v_add_lshl_u32 v34, v9, v0, 2
	;Sim: Cache($-)
	v_add_lshl_u32 v35, v2, v30, 2
	s_wait_dscnt 0x28
	;Sim: Cache($-)
	v_add_lshl_u32 v36, v2, v31, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v37, v9, v31, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v38, v9, v15, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v39, v9, v30, 2
	s_wait_dscnt 0x27
	;Sim: Cache($$)
	v_add_lshl_u32 v40, v2, v14, 2
	;Sim: Cache($-)
	v_add_lshl_u32 v41, v2, v29, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v42, v2, v13, 2
	;Sim: Cache($-)
	v_add_lshl_u32 v43, v2, v28, 2
	s_wait_dscnt 0x26
	;Sim: Cache($$)
	v_add_lshl_u32 v44, v9, v14, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v45, v9, v29, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v46, v9, v13, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v47, v9, v28, 2
	s_wait_dscnt 0x25
	;Sim: Cache($$)
	v_add_lshl_u32 v48, v2, v12, 2
	;Sim: Cache($-)
	v_add_lshl_u32 v49, v2, v27, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v50, v2, v11, 2
	;Sim: Cache($-)
	v_add_lshl_u32 v51, v2, v26, 2
	s_wait_dscnt 0x24
	;Sim: Cache($$)
	v_add_lshl_u32 v52, v9, v12, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v53, v9, v27, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v54, v9, v11, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v55, v9, v26, 2
	s_wait_dscnt 0x23
	;Sim: Cache($$)
	v_add_lshl_u32 v56, v2, v10, 2
	;Sim: Cache($-)
	v_add_lshl_u32 v57, v2, v25, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v58, v2, v1, 2
	;Sim: Cache($-)
	v_add_lshl_u32 v2, v2, v24, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v59, v9, v10, 2
	s_wait_dscnt 0x22
	;Sim: Cache($$)
	v_add_lshl_u32 v60, v9, v25, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v61, v9, v1, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v9, v9, v24, 2
	;Sim: Cache(-$)
	v_add_lshl_u32 v62, v8, v0, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v63, v8, v31, 2
	s_wait_dscnt 0x21
	;Sim: Cache($$)
	v_add_lshl_u32 v64, v8, v15, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v65, v8, v30, 2
	;Sim: Cache(-$)
	v_add_lshl_u32 v66, v7, v0, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v67, v7, v31, 2
	s_wait_dscnt 0x20
	;Sim: Cache($$)
	v_add_lshl_u32 v68, v7, v15, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v69, v7, v30, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v70, v8, v14, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v71, v8, v29, 2
	s_wait_dscnt 0x1f
	;Sim: Cache($$)
	v_add_lshl_u32 v72, v8, v13, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v73, v8, v28, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v74, v7, v14, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v75, v7, v29, 2
	s_wait_dscnt 0x1e
	;Sim: Cache($$)
	v_add_lshl_u32 v76, v7, v13, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v77, v7, v28, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v78, v8, v12, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v79, v8, v27, 2
	s_wait_dscnt 0x1d
	;Sim: Cache($$)
	v_add_lshl_u32 v80, v8, v11, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v81, v8, v26, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v82, v7, v12, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v83, v7, v27, 2
	s_wait_dscnt 0x1c
	;Sim: Cache($$)
	v_add_lshl_u32 v84, v7, v11, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v85, v7, v26, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v86, v8, v10, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v87, v8, v25, 2
	s_wait_dscnt 0x17
	;Sim: Cache($$)
	v_add_lshl_u32 v88, v8, v1, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v8, v8, v24, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v89, v7, v10, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v90, v7, v25, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v91, v7, v1, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v7, v7, v24, 2
	s_wait_dscnt 0x16
	;Sim: Cache(-$)
	v_add_lshl_u32 v92, v6, v0, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v93, v6, v31, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v94, v6, v15, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v95, v6, v30, 2
	s_wait_dscnt 0x15
	;Sim: Cache(-$)
	v_add_lshl_u32 v96, v5, v0, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v97, v5, v31, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v98, v5, v15, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v99, v5, v30, 2
	s_wait_dscnt 0x14
	;Sim: Cache($$)
	v_add_lshl_u32 v100, v6, v14, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v101, v6, v29, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v102, v6, v13, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v103, v6, v28, 2
	s_wait_alu depctr_vm_vsrc(6)
	;Sim: Cache($$)
	v_add_lshl_u32 v104, v5, v14, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v105, v5, v29, 2
	s_wait_alu depctr_vm_vsrc(0)
	;Sim: Cache($$)
	v_add_lshl_u32 v106, v5, v13, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v107, v5, v28, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v108, v6, v12, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v109, v6, v27, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v110, v6, v11, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v111, v6, v26, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v112, v5, v12, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v113, v5, v27, 2
	s_wait_dscnt 0x13
	;Sim: Cache($$)
	v_add_lshl_u32 v114, v5, v11, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v115, v5, v26, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v116, v6, v10, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v117, v6, v25, 2
	s_wait_dscnt 0x12
	;Sim: Cache($$)
	v_add_lshl_u32 v118, v6, v1, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v6, v6, v24, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v119, v5, v10, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v120, v5, v25, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v121, v5, v1, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v5, v5, v24, 2
	s_wait_dscnt 0x11
	;Sim: Cache(-$)
	v_add_lshl_u32 v122, v3, v0, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v123, v3, v31, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v124, v3, v15, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v125, v3, v30, 2
	;Sim: Cache(-$)
	v_add_lshl_u32 v0, v4, v0, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v31, v4, v31, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v15, v4, v15, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v30, v4, v30, 2
	s_wait_dscnt 0x10
	;Sim: Cache($$)
	v_add_lshl_u32 v126, v3, v14, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v127, v3, v29, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v128, v3, v13, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v129, v3, v28, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v14, v4, v14, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v29, v4, v29, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v13, v4, v13, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v28, v4, v28, 2
	s_wait_dscnt 0x3
	;Sim: Cache($$)
	v_add_lshl_u32 v130, v3, v12, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v131, v3, v27, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v132, v3, v11, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v133, v3, v26, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v12, v4, v12, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v27, v4, v27, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v11, v4, v11, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v26, v4, v26, 2
	s_wait_dscnt 0x2
	;Sim: Cache($$)
	v_add_lshl_u32 v134, v3, v10, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v135, v3, v25, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v136, v3, v1, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v3, v3, v24, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v10, v4, v10, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v25, v4, v25, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v1, v4, v1, 2
	;Sim: Cache($$)
	v_add_lshl_u32 v4, v4, v24, 2
	s_or_b64 s[16:17], s[16:17], 0xfc00000000000000
	s_mov_b32 s19, 0
	s_mov_b32 s18, 0xffffff
	s_and_b32 s15, s14, s8
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v1, 0x80000000, v1, s15
	v_cndmask_b32_e64 v4, 0x80000000, v4, s15
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s15, s14, s7
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v10, 0x80000000, v10, s15
	v_cndmask_b32_e64 v24, 0x80000000, v25, s15
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s15, s13, s8
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v25, 0x80000000, v136, s15
	v_cndmask_b32_e64 v3, 0x80000000, v3, s15
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s15, s13, s7
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v134, 0x80000000, v134, s15
	v_cndmask_b32_e64 v135, 0x80000000, v135, s15
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s15, s14, s6
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v11, 0x80000000, v11, s15
	v_cndmask_b32_e64 v26, 0x80000000, v26, s15
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s15, s14, s5
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v12, 0x80000000, v12, s15
	v_cndmask_b32_e64 v27, 0x80000000, v27, s15
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s15, s13, s6
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v132, 0x80000000, v132, s15
	v_cndmask_b32_e64 v133, 0x80000000, v133, s15
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s15, s13, s5
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v130, 0x80000000, v130, s15
	v_cndmask_b32_e64 v131, 0x80000000, v131, s15
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s15, s14, s3
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v13, 0x80000000, v13, s15
	v_cndmask_b32_e64 v28, 0x80000000, v28, s15
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s15, s14, s4
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v14, 0x80000000, v14, s15
	v_cndmask_b32_e64 v29, 0x80000000, v29, s15
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s15, s13, s3
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v128, 0x80000000, v128, s15
	v_cndmask_b32_e64 v129, 0x80000000, v129, s15
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s15, s13, s4
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v126, 0x80000000, v126, s15
	v_cndmask_b32_e64 v127, 0x80000000, v127, s15
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s15, s14, s2
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v15, 0x80000000, v15, s15
	v_cndmask_b32_e64 v30, 0x80000000, v30, s15
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s14, s14, vcc_lo
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v0, 0x80000000, v0, s14
	v_cndmask_b32_e64 v31, 0x80000000, v31, s14
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s14, s13, s2
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v124, 0x80000000, v124, s14
	v_cndmask_b32_e64 v125, 0x80000000, v125, s14
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s13, vcc_lo
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v122, 0x80000000, v122, s13
	v_cndmask_b32_e64 v123, 0x80000000, v123, s13
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s13, s12, s8
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v121, 0x80000000, v121, s13
	v_cndmask_b32_e64 v5, 0x80000000, v5, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s12, s7
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v119, 0x80000000, v119, s13
	v_cndmask_b32_e64 v120, 0x80000000, v120, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s11, s8
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v118, 0x80000000, v118, s13
	v_cndmask_b32_e64 v6, 0x80000000, v6, s13
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s13, s11, s7
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v116, 0x80000000, v116, s13
	v_cndmask_b32_e64 v117, 0x80000000, v117, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s12, s6
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v114, 0x80000000, v114, s13
	v_cndmask_b32_e64 v115, 0x80000000, v115, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s12, s5
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v112, 0x80000000, v112, s13
	v_cndmask_b32_e64 v113, 0x80000000, v113, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s11, s6
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v110, 0x80000000, v110, s13
	v_cndmask_b32_e64 v111, 0x80000000, v111, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s11, s5
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v108, 0x80000000, v108, s13
	v_cndmask_b32_e64 v109, 0x80000000, v109, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s12, s3
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v106, 0x80000000, v106, s13
	v_cndmask_b32_e64 v107, 0x80000000, v107, s13
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s13, s12, s4
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v104, 0x80000000, v104, s13
	v_cndmask_b32_e64 v105, 0x80000000, v105, s13
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s13, s11, s3
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v102, 0x80000000, v102, s13
	v_cndmask_b32_e64 v103, 0x80000000, v103, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s11, s4
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v100, 0x80000000, v100, s13
	v_cndmask_b32_e64 v101, 0x80000000, v101, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s13, s12, s2
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v98, 0x80000000, v98, s13
	v_cndmask_b32_e64 v99, 0x80000000, v99, s13
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s12, s12, vcc_lo
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v96, 0x80000000, v96, s12
	v_cndmask_b32_e64 v97, 0x80000000, v97, s12
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s12, s11, s2
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v94, 0x80000000, v94, s12
	v_cndmask_b32_e64 v95, 0x80000000, v95, s12
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s11, vcc_lo
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v92, 0x80000000, v92, s11
	v_cndmask_b32_e64 v93, 0x80000000, v93, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s10, s8
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v91, 0x80000000, v91, s11
	v_cndmask_b32_e64 v7, 0x80000000, v7, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s10, s7
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v89, 0x80000000, v89, s11
	v_cndmask_b32_e64 v90, 0x80000000, v90, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s9, s8
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v88, 0x80000000, v88, s11
	v_cndmask_b32_e64 v8, 0x80000000, v8, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s9, s7
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v86, 0x80000000, v86, s11
	v_cndmask_b32_e64 v87, 0x80000000, v87, s11
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s11, s10, s6
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v84, 0x80000000, v84, s11
	v_cndmask_b32_e64 v85, 0x80000000, v85, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s10, s5
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v82, 0x80000000, v82, s11
	v_cndmask_b32_e64 v83, 0x80000000, v83, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s9, s6
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v80, 0x80000000, v80, s11
	v_cndmask_b32_e64 v81, 0x80000000, v81, s11
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s11, s9, s5
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v78, 0x80000000, v78, s11
	v_cndmask_b32_e64 v79, 0x80000000, v79, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s10, s3
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v76, 0x80000000, v76, s11
	v_cndmask_b32_e64 v77, 0x80000000, v77, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s10, s4
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v74, 0x80000000, v74, s11
	v_cndmask_b32_e64 v75, 0x80000000, v75, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s9, s3
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v72, 0x80000000, v72, s11
	v_cndmask_b32_e64 v73, 0x80000000, v73, s11
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s11, s9, s4
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v70, 0x80000000, v70, s11
	v_cndmask_b32_e64 v71, 0x80000000, v71, s11
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s11, s10, s2
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v68, 0x80000000, v68, s11
	v_cndmask_b32_e64 v69, 0x80000000, v69, s11
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s10, s10, vcc_lo
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v66, 0x80000000, v66, s10
	v_cndmask_b32_e64 v67, 0x80000000, v67, s10
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s10, s9, s2
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v64, 0x80000000, v64, s10
	v_cndmask_b32_e64 v65, 0x80000000, v65, s10
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s9, s9, vcc_lo
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v62, 0x80000000, v62, s9
	v_cndmask_b32_e64 v63, 0x80000000, v63, s9
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s9, s1, s8
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v61, 0x80000000, v61, s9
	v_cndmask_b32_e64 v9, 0x80000000, v9, s9
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s9, s1, s7
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v59, 0x80000000, v59, s9
	v_cndmask_b32_e64 v60, 0x80000000, v60, s9
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s8, s0, s8
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v58, 0x80000000, v58, s8
	v_cndmask_b32_e64 v2, 0x80000000, v2, s8
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s7, s0, s7
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v56, 0x80000000, v56, s7
	v_cndmask_b32_e64 v57, 0x80000000, v57, s7
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s7, s1, s6
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v54, 0x80000000, v54, s7
	v_cndmask_b32_e64 v55, 0x80000000, v55, s7
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s7, s1, s5
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v52, 0x80000000, v52, s7
	v_cndmask_b32_e64 v53, 0x80000000, v53, s7
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s6, s0, s6
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v50, 0x80000000, v50, s6
	v_cndmask_b32_e64 v51, 0x80000000, v51, s6
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s5, s0, s5
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v48, 0x80000000, v48, s5
	v_cndmask_b32_e64 v49, 0x80000000, v49, s5
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s5, s1, s3
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v46, 0x80000000, v46, s5
	v_cndmask_b32_e64 v47, 0x80000000, v47, s5
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s5, s1, s4
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v44, 0x80000000, v44, s5
	v_cndmask_b32_e64 v45, 0x80000000, v45, s5
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s3, s0, s3
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v42, 0x80000000, v42, s3
	v_cndmask_b32_e64 v43, 0x80000000, v43, s3
	;Sim: Stall:3 [VA_SSRC blocked]
	s_and_b32 s3, s0, s4
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v40, 0x80000000, v40, s3
	v_cndmask_b32_e64 v41, 0x80000000, v41, s3
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s3, s1, s2
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v38, 0x80000000, v38, s3
	v_cndmask_b32_e64 v39, 0x80000000, v39, s3
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s1, s1, vcc_lo
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v34, 0x80000000, v34, s1
	v_cndmask_b32_e64 v37, 0x80000000, v37, s1
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 s1, s0, s2
	;Sim: Stall:1 [RAW hazard]
	v_cndmask_b32_e64 v33, 0x80000000, v33, s1
	v_cndmask_b32_e64 v35, 0x80000000, v35, s1
	;Sim: Stall:4 [VA_SSRC blocked]
	s_and_b32 vcc_lo, s0, vcc_lo
	v_cndmask_b32_e32 v32, 0x80000000, v32, vcc_lo
	v_cndmask_b32_e32 v36, 0x80000000, v36, vcc_lo
	;Sim: Stall:18 [VA_VDST wait]
	s_wait_alu depctr_va_vdst(1)
	;Sim: Fused
	s_set_vgpr_msb 64                       ;  msbs: dst=1 src0=0 src1=0 src2=0
	buffer_store_b128 v[196:199] /*v[452:455]*/, v32, s[16:19], null offen
	s_wait_alu depctr_va_vdst(0)
	s_clause 0x3e
	buffer_store_b128 v[200:203] /*v[456:459]*/, v36, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0x4000                   ;  msbs: dst=0 src0=0 src1=0 src2=0
	buffer_store_b128 v[16:19], v33, s[16:19], null offen
	buffer_store_b128 v[20:23], v35, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0xc0                     ;  msbs: dst=3 src0=0 src1=0 src2=0
	buffer_store_b128 v[178:181] /*v[946:949]*/, v34, s[16:19], null offen
	buffer_store_b128 v[182:185] /*v[950:953]*/, v37, s[16:19], null offen
	buffer_store_b128 v[170:173] /*v[938:941]*/, v38, s[16:19], null offen
	buffer_store_b128 v[174:177] /*v[942:945]*/, v39, s[16:19], null offen
	buffer_store_b128 v[162:165] /*v[930:933]*/, v40, s[16:19], null offen
	buffer_store_b128 v[166:169] /*v[934:937]*/, v41, s[16:19], null offen
	buffer_store_b128 v[154:157] /*v[922:925]*/, v42, s[16:19], null offen
	buffer_store_b128 v[158:161] /*v[926:929]*/, v43, s[16:19], null offen
	buffer_store_b128 v[146:149] /*v[914:917]*/, v44, s[16:19], null offen
	buffer_store_b128 v[150:153] /*v[918:921]*/, v45, s[16:19], null offen
	buffer_store_b128 v[138:141] /*v[906:909]*/, v46, s[16:19], null offen
	buffer_store_b128 v[142:145] /*v[910:913]*/, v47, s[16:19], null offen
	;Sim: Stall:272 [FIFO full]
	buffer_store_b128 v[186:189] /*v[954:957]*/, v48, s[16:19], null offen
	;Sim: Stall:2 [FIFO full]
	buffer_store_b128 v[190:193] /*v[958:961]*/, v49, s[16:19], null offen
	buffer_store_b128 v[2:5] /*v[770:773]*/, v50, s[16:19], null offen
	buffer_store_b128 v[6:9] /*v[774:777]*/, v51, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0xc080                   ;  msbs: dst=2 src0=0 src1=0 src2=0
	buffer_store_b128 v[236:239] /*v[748:751]*/, v52, s[16:19], null offen
	buffer_store_b128 v[240:243] /*v[752:755]*/, v53, s[16:19], null offen
	buffer_store_b128 v[228:231] /*v[740:743]*/, v54, s[16:19], null offen
	buffer_store_b128 v[232:235] /*v[744:747]*/, v55, s[16:19], null offen
	buffer_store_b128 v[220:223] /*v[732:735]*/, v56, s[16:19], null offen
	buffer_store_b128 v[224:227] /*v[736:739]*/, v57, s[16:19], null offen
	buffer_store_b128 v[212:215] /*v[724:727]*/, v58, s[16:19], null offen
	buffer_store_b128 v[216:219] /*v[728:731]*/, v2, s[16:19], null offen
	buffer_store_b128 v[204:207] /*v[716:719]*/, v59, s[16:19], null offen
	buffer_store_b128 v[208:211] /*v[720:723]*/, v60, s[16:19], null offen
	buffer_store_b128 v[196:199] /*v[708:711]*/, v61, s[16:19], null offen
	;Sim: Stall:10 [FIFO full]
	buffer_store_b128 v[200:203] /*v[712:715]*/, v9, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0x80c0                   ;  msbs: dst=3 src0=0 src1=0 src2=0
	;Sim: Stall:272 [FIFO full]
	buffer_store_b128 v[130:133] /*v[898:901]*/, v62, s[16:19], null offen
	;Sim: Stall:2 [FIFO full]
	buffer_store_b128 v[134:137] /*v[902:905]*/, v63, s[16:19], null offen
	buffer_store_b128 v[122:125] /*v[890:893]*/, v64, s[16:19], null offen
	buffer_store_b128 v[126:129] /*v[894:897]*/, v65, s[16:19], null offen
	buffer_store_b128 v[114:117] /*v[882:885]*/, v66, s[16:19], null offen
	buffer_store_b128 v[118:121] /*v[886:889]*/, v67, s[16:19], null offen
	buffer_store_b128 v[106:109] /*v[874:877]*/, v68, s[16:19], null offen
	buffer_store_b128 v[110:113] /*v[878:881]*/, v69, s[16:19], null offen
	buffer_store_b128 v[98:101] /*v[866:869]*/, v70, s[16:19], null offen
	buffer_store_b128 v[102:105] /*v[870:873]*/, v71, s[16:19], null offen
	buffer_store_b128 v[90:93] /*v[858:861]*/, v72, s[16:19], null offen
	buffer_store_b128 v[94:97] /*v[862:865]*/, v73, s[16:19], null offen
	buffer_store_b128 v[82:85] /*v[850:853]*/, v74, s[16:19], null offen
	buffer_store_b128 v[86:89] /*v[854:857]*/, v75, s[16:19], null offen
	buffer_store_b128 v[74:77] /*v[842:845]*/, v76, s[16:19], null offen
	;Sim: Stall:10 [FIFO full]
	buffer_store_b128 v[78:81] /*v[846:849]*/, v77, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0xc080                   ;  msbs: dst=2 src0=0 src1=0 src2=0
	;Sim: Stall:272 [FIFO full]
	buffer_store_b128 v[124:127] /*v[636:639]*/, v78, s[16:19], null offen
	;Sim: Stall:2 [FIFO full]
	buffer_store_b128 v[128:131] /*v[640:643]*/, v79, s[16:19], null offen
	buffer_store_b128 v[116:119] /*v[628:631]*/, v80, s[16:19], null offen
	buffer_store_b128 v[120:123] /*v[632:635]*/, v81, s[16:19], null offen
	buffer_store_b128 v[108:111] /*v[620:623]*/, v82, s[16:19], null offen
	buffer_store_b128 v[112:115] /*v[624:627]*/, v83, s[16:19], null offen
	buffer_store_b128 v[100:103] /*v[612:615]*/, v84, s[16:19], null offen
	buffer_store_b128 v[104:107] /*v[616:619]*/, v85, s[16:19], null offen
	buffer_store_b128 v[92:95] /*v[604:607]*/, v86, s[16:19], null offen
	buffer_store_b128 v[96:99] /*v[608:611]*/, v87, s[16:19], null offen
	buffer_store_b128 v[84:87] /*v[596:599]*/, v88, s[16:19], null offen
	buffer_store_b128 v[88:91] /*v[600:603]*/, v8, s[16:19], null offen
	buffer_store_b128 v[76:79] /*v[588:591]*/, v89, s[16:19], null offen
	buffer_store_b128 v[80:83] /*v[592:595]*/, v90, s[16:19], null offen
	buffer_store_b128 v[68:71] /*v[580:583]*/, v91, s[16:19], null offen
	;Sim: Stall:10 [FIFO full]
	buffer_store_b128 v[72:75] /*v[584:587]*/, v7, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0x80c0                   ;  msbs: dst=3 src0=0 src1=0 src2=0
	s_clause 0x3e
	;Sim: Stall:271 [FIFO full]
	buffer_store_b128 v[66:69] /*v[834:837]*/, v92, s[16:19], null offen
	;Sim: Stall:2 [FIFO full]
	buffer_store_b128 v[70:73] /*v[838:841]*/, v93, s[16:19], null offen
	buffer_store_b128 v[58:61] /*v[826:829]*/, v94, s[16:19], null offen
	buffer_store_b128 v[62:65] /*v[830:833]*/, v95, s[16:19], null offen
	buffer_store_b128 v[50:53] /*v[818:821]*/, v96, s[16:19], null offen
	buffer_store_b128 v[54:57] /*v[822:825]*/, v97, s[16:19], null offen
	buffer_store_b128 v[42:45] /*v[810:813]*/, v98, s[16:19], null offen
	buffer_store_b128 v[46:49] /*v[814:817]*/, v99, s[16:19], null offen
	buffer_store_b128 v[34:37] /*v[802:805]*/, v100, s[16:19], null offen
	buffer_store_b128 v[38:41] /*v[806:809]*/, v101, s[16:19], null offen
	buffer_store_b128 v[26:29] /*v[794:797]*/, v102, s[16:19], null offen
	buffer_store_b128 v[30:33] /*v[798:801]*/, v103, s[16:19], null offen
	buffer_store_b128 v[18:21] /*v[786:789]*/, v104, s[16:19], null offen
	buffer_store_b128 v[22:25] /*v[790:793]*/, v105, s[16:19], null offen
	buffer_store_b128 v[10:13] /*v[778:781]*/, v106, s[16:19], null offen
	;Sim: Stall:10 [FIFO full]
	buffer_store_b128 v[14:17] /*v[782:785]*/, v107, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0xc080                   ;  msbs: dst=2 src0=0 src1=0 src2=0
	;Sim: Stall:272 [FIFO full]
	buffer_store_b128 v[60:63] /*v[572:575]*/, v108, s[16:19], null offen
	;Sim: Stall:2 [FIFO full]
	buffer_store_b128 v[64:67] /*v[576:579]*/, v109, s[16:19], null offen
	buffer_store_b128 v[52:55] /*v[564:567]*/, v110, s[16:19], null offen
	buffer_store_b128 v[56:59] /*v[568:571]*/, v111, s[16:19], null offen
	buffer_store_b128 v[44:47] /*v[556:559]*/, v112, s[16:19], null offen
	buffer_store_b128 v[48:51] /*v[560:563]*/, v113, s[16:19], null offen
	buffer_store_b128 v[36:39] /*v[548:551]*/, v114, s[16:19], null offen
	buffer_store_b128 v[40:43] /*v[552:555]*/, v115, s[16:19], null offen
	buffer_store_b128 v[28:31] /*v[540:543]*/, v116, s[16:19], null offen
	buffer_store_b128 v[32:35] /*v[544:547]*/, v117, s[16:19], null offen
	buffer_store_b128 v[20:23] /*v[532:535]*/, v118, s[16:19], null offen
	buffer_store_b128 v[24:27] /*v[536:539]*/, v6, s[16:19], null offen
	buffer_store_b128 v[12:15] /*v[524:527]*/, v119, s[16:19], null offen
	buffer_store_b128 v[16:19] /*v[528:531]*/, v120, s[16:19], null offen
	buffer_store_b128 v[4:7] /*v[516:519]*/, v121, s[16:19], null offen
	;Sim: Stall:10 [FIFO full]
	buffer_store_b128 v[8:11] /*v[520:523]*/, v5, s[16:19], null offen
	;Sim: Stall:272 [FIFO full]
	buffer_store_b128 v[188:191] /*v[700:703]*/, v122, s[16:19], null offen
	;Sim: Stall:2 [FIFO full]
	buffer_store_b128 v[192:195] /*v[704:707]*/, v123, s[16:19], null offen
	buffer_store_b128 v[180:183] /*v[692:695]*/, v124, s[16:19], null offen
	buffer_store_b128 v[184:187] /*v[696:699]*/, v125, s[16:19], null offen
	buffer_store_b128 v[172:175] /*v[684:687]*/, v0, s[16:19], null offen
	buffer_store_b128 v[176:179] /*v[688:691]*/, v31, s[16:19], null offen
	buffer_store_b128 v[164:167] /*v[676:679]*/, v15, s[16:19], null offen
	buffer_store_b128 v[168:171] /*v[680:683]*/, v30, s[16:19], null offen
	buffer_store_b128 v[156:159] /*v[668:671]*/, v126, s[16:19], null offen
	buffer_store_b128 v[160:163] /*v[672:675]*/, v127, s[16:19], null offen
	buffer_store_b128 v[148:151] /*v[660:663]*/, v128, s[16:19], null offen
	buffer_store_b128 v[152:155] /*v[664:667]*/, v129, s[16:19], null offen
	buffer_store_b128 v[140:143] /*v[652:655]*/, v14, s[16:19], null offen
	buffer_store_b128 v[144:147] /*v[656:659]*/, v29, s[16:19], null offen
	buffer_store_b128 v[132:135] /*v[644:647]*/, v13, s[16:19], null offen
	;Sim: Stall:10 [FIFO full]
	buffer_store_b128 v[136:139] /*v[648:651]*/, v28, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0x8040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	;Sim: Stall:272 [FIFO full]
	buffer_store_b128 v[252:255] /*v[508:511]*/, v130, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0x4080                   ;  msbs: dst=2 src0=0 src1=0 src2=0
	;Sim: Stall:2 [FIFO full]
	buffer_store_b128 v[0:3] /*v[512:515]*/, v131, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0x8040                   ;  msbs: dst=1 src0=0 src1=0 src2=0
	buffer_store_b128 v[244:247] /*v[500:503]*/, v132, s[16:19], null offen
	buffer_store_b128 v[248:251] /*v[504:507]*/, v133, s[16:19], null offen
	buffer_store_b128 v[236:239] /*v[492:495]*/, v12, s[16:19], null offen
	buffer_store_b128 v[240:243] /*v[496:499]*/, v27, s[16:19], null offen
	buffer_store_b128 v[228:231] /*v[484:487]*/, v11, s[16:19], null offen
	buffer_store_b128 v[232:235] /*v[488:491]*/, v26, s[16:19], null offen
	buffer_store_b128 v[220:223] /*v[476:479]*/, v134, s[16:19], null offen
	buffer_store_b128 v[224:227] /*v[480:483]*/, v135, s[16:19], null offen
	buffer_store_b128 v[212:215] /*v[468:471]*/, v25, s[16:19], null offen
	buffer_store_b128 v[216:219] /*v[472:475]*/, v3, s[16:19], null offen
	buffer_store_b128 v[204:207] /*v[460:463]*/, v10, s[16:19], null offen
	buffer_store_b128 v[208:211] /*v[464:467]*/, v24, s[16:19], null offen
	;Sim: Fused
	s_set_vgpr_msb 0x4080                   ;  msbs: dst=2 src0=0 src1=0 src2=0
	buffer_store_b128 v[250:253] /*v[762:765]*/, v1, s[16:19], null offen
	;Sim: Stall:10 [FIFO full]
	buffer_store_b128 v[254:257] /*v[766:769]*/, v4, s[16:19], null offen
	s_sendmsg sendmsg(MSG_DEALLOC_VGPRS)
	s_endpgm
	.section	.rodata,"a",@progbits
	.p2align	6, 0x0
	.amdhsa_kernel mxgemm_tdm_pipelined_kernel
		.amdhsa_group_segment_fixed_size 0
		.amdhsa_private_segment_fixed_size 0
		.amdhsa_kernarg_size 88
		.amdhsa_user_sgpr_count 2
		.amdhsa_user_sgpr_dispatch_ptr 0
		.amdhsa_user_sgpr_queue_ptr 0
		.amdhsa_user_sgpr_kernarg_segment_ptr 1
		.amdhsa_user_sgpr_dispatch_id 0
		.amdhsa_user_sgpr_kernarg_preload_length 0
		.amdhsa_user_sgpr_kernarg_preload_offset 0
		.amdhsa_user_sgpr_private_segment_size 0
		.amdhsa_wavefront_size32 1
		.amdhsa_uses_dynamic_stack 0
		.amdhsa_enable_private_segment 0
		.amdhsa_system_sgpr_workgroup_id_x 1
		.amdhsa_system_sgpr_workgroup_id_y 1
		.amdhsa_system_sgpr_workgroup_id_z 1
		.amdhsa_system_sgpr_workgroup_info 0
		.amdhsa_system_vgpr_workitem_id 0
		.amdhsa_next_free_vgpr 1024
		.amdhsa_next_free_sgpr 100
		.amdhsa_named_barrier_count 0
		.amdhsa_tcp_split 0
		.amdhsa_reserve_vcc 1
		.amdhsa_reserve_xnack_mask 1
		.amdhsa_float_round_mode_32 0
		.amdhsa_float_round_mode_16_64 0
		.amdhsa_float_denorm_mode_32 3
		.amdhsa_float_denorm_mode_16_64 3
		.amdhsa_fp16_overflow 0
		.amdhsa_memory_ordered 1
		.amdhsa_forward_progress 1
		.amdhsa_inst_pref_size 112
		.amdhsa_round_robin_scheduling 0
		.amdhsa_exception_fp_ieee_invalid_op 0
		.amdhsa_exception_fp_denorm_src 0
		.amdhsa_exception_fp_ieee_div_zero 0
		.amdhsa_exception_fp_ieee_overflow 0
		.amdhsa_exception_fp_ieee_underflow 0
		.amdhsa_exception_fp_ieee_inexact 0
		.amdhsa_exception_int_div_zero 0
	.end_amdhsa_kernel
	.text
.Lfunc_end0:
	.size	mxgemm_tdm_pipelined_kernel, .Lfunc_end0-mxgemm_tdm_pipelined_kernel
                                        ; -- End function
	.set mxgemm_tdm_pipelined_kernel.num_vgpr, 1024
	.set mxgemm_tdm_pipelined_kernel.num_agpr, 0
	.set mxgemm_tdm_pipelined_kernel.numbered_sgpr, 100
	.set mxgemm_tdm_pipelined_kernel.num_named_barrier, 0
	.set mxgemm_tdm_pipelined_kernel.private_seg_size, 0
	.set mxgemm_tdm_pipelined_kernel.uses_vcc, 1
	.set mxgemm_tdm_pipelined_kernel.uses_flat_scratch, 0
	.set mxgemm_tdm_pipelined_kernel.has_dyn_sized_stack, 0
	.set mxgemm_tdm_pipelined_kernel.has_recursion, 0
	.set mxgemm_tdm_pipelined_kernel.has_indirect_call, 0
	.section	.AMDGPU.csdata,"",@progbits
; Kernel info:
; codeLenInByte = 14308
; TotalNumSgprs: 102
; NumVgprs: 1024
; ScratchSize: 0
; MemoryBound: 0
; FloatMode: 240
; IeeeMode: 1
; LDSByteSize: 0 bytes/workgroup (compile time only)
; SGPRBlocks: 0
; VGPRBlocks: 63
; NumSGPRsForWavesPerEU: 102
; NumVGPRsForWavesPerEU: 1024
; NamedBarCnt: 0
; Occupancy: 1
; WaveLimiterHint : 0
; COMPUTE_PGM_RSRC2:SCRATCH_EN: 0
; COMPUTE_PGM_RSRC2:USER_SGPR: 2
; COMPUTE_PGM_RSRC2:TRAP_HANDLER: 0
; COMPUTE_PGM_RSRC2:TGID_X_EN: 1
; COMPUTE_PGM_RSRC2:TGID_Y_EN: 1
; COMPUTE_PGM_RSRC2:TGID_Z_EN: 1
; COMPUTE_PGM_RSRC2:TIDIG_COMP_CNT: 0
;
;; ============================================================
;; mxgemm_tdm_pipelined_kernel - STATIC PERFORMANCE ESTIMATE (gfx1250)
;; ============================================================
;;
;; === Raw Metrics (each block executed once) ===
;;   Instructions: 1932
;;   Cycles:       5115
;;   Stall: 3453 cycles (67.5%)
;;     FU:675 | WMMACoExec:143(VALU:41+MEM:43+Other:59) | MemFIFO:2049 | Wait:123 | RegBank:25 | VaSSRC:251 | VaVdst:18 | RAW:138 | ISFetch:31 (223 fetches) | RegBankInWMMA:8 (not counted) | MSBExposed:0 (+38 masked) | ISlot:22/67 (wasted:45)
;;       FU: XDL:645 VALU:28 LDS:2
;;   Waitcnts: 47 | False waits: 0
;;   WMMA windows: 1024 | Co-executed: 184
;;   WMMA efficiency: 1024 / 5115 cycles (20%)
;;   I-slots: 67 used | 45 wasted on non-VALU (33% VALU)
;;
;; === Scaled Metrics (loops x trip count) ===
;;   Instructions: 21431
;;   Cycles:       43803
;;   Stall: 29028 cycles (66.3%)
;;     FU:20825 | WMMACoExec:4421(VALU:1157+MEM:1376+Other:1888) | MemFIFO:2049 | Wait:1084 | RegBank:25 | VaSSRC:344 | VaVdst:18 | RAW:200 | ISFetch:62 (2486 fetches) | RegBankInWMMA:256 (not counted) | MSBExposed:0 (+1216 masked) | ISlot:673/2082 (wasted:1409)
;;       FU: XDL:20609 VALU:214 LDS:2
;;   Waitcnts: 481 | False waits: 0
;;   WMMA windows: 32768 | Co-executed: 5826 (18%)
;;   WMMA efficiency: 32768 / 43803 cycles (75%)
;;   I-slots: 2082 used | 1409 wasted on non-VALU (32% VALU)
;;
;; === Instruction Breakdown (Raw / Scaled) ===
;;   VALU: 672/2005 (VOPD:273/304) | SALU: 470/3322 | TRANS: 2/2 | WMMA: 128/4096
;;   DS_RD: 187/4403 | DS_WR: 0/0 | VMEM: 128/128 | TDM: 12/136
;;
;; === VGPR Operand Cache ===
;;   VGPR reads: 5205/134196 | From cache: 2794/64546 (48%)
;;   Evictions: 2140/68387
;;
;; === CFG Analysis ===
;;   Loops: 1 | Max depth: 1 | Trip count: 32
;;   Cold: 1215 cycles | Warm: 1248 cycles | Speedup: 0.97x
;;   Branches: 1 (scaled metrics use uniform probability)
;;
;; === Derived Metrics ===
;;   IPC: 0.22 | Stall ratio: 66.3%
;;   False wait ratio: 0.00 per waitcnt
;;
;; ============================================================
	.text
	.p2alignl 7, 3214868480
	.fill 96, 4, 3214868480
	.section	.AMDGPU.gpr_maximums,"",@progbits
	.set amdgpu.max_num_vgpr, 0
	.set amdgpu.max_num_agpr, 0
	.set amdgpu.max_num_sgpr, 0
	.set amdgpu.max_num_named_barrier, 0
	.text
	.section	".note.GNU-stack","",@progbits
	.amdgpu_metadata
---
amdhsa.kernels:
  - .args:
      - .address_space:  global
        .offset:         0
        .size:           8
        .value_kind:     global_buffer
      - .address_space:  global
        .offset:         8
        .size:           8
        .value_kind:     global_buffer
      - .address_space:  global
        .offset:         16
        .size:           8
        .value_kind:     global_buffer
      - .address_space:  global
        .offset:         24
        .size:           8
        .value_kind:     global_buffer
      - .address_space:  global
        .offset:         32
        .size:           8
        .value_kind:     global_buffer
      - .offset:         40
        .size:           4
        .value_kind:     by_value
      - .offset:         44
        .size:           4
        .value_kind:     by_value
      - .offset:         48
        .size:           4
        .value_kind:     by_value
      - .offset:         52
        .size:           4
        .value_kind:     by_value
      - .offset:         56
        .size:           4
        .value_kind:     by_value
      - .offset:         60
        .size:           4
        .value_kind:     by_value
      - .offset:         64
        .size:           4
        .value_kind:     by_value
      - .address_space:  global
        .offset:         72
        .size:           8
        .value_kind:     global_buffer
      - .address_space:  global
        .offset:         80
        .size:           8
        .value_kind:     global_buffer
    .group_segment_fixed_size: 0
    .kernarg_segment_align: 8
    .kernarg_segment_size: 88
    .max_flat_workgroup_size: 128
    .name:           mxgemm_tdm_pipelined_kernel
    .private_segment_fixed_size: 0
    .sgpr_count:     102
    .sgpr_spill_count: 0
    .symbol:         mxgemm_tdm_pipelined_kernel.kd
    .uniform_work_group_size: 1
    .uses_dynamic_stack: false
    .vgpr_count:     1024
    .vgpr_spill_count: 0
    .wavefront_size: 32
amdhsa.target:   amdgcn-amd-amdhsa--gfx1250
amdhsa.version:
  - 1
  - 2
...

	.end_amdgpu_metadata
