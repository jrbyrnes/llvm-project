; RUN: llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx1250 -print-after=amdgpu-tensor-descriptor-opt -o /dev/null < %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx1250 -amdgpu-tensor-descriptor-opt=0 -print-after=amdgpu-tensor-descriptor-opt -o /dev/null < %s 2>&1 | FileCheck %s --check-prefix=DISABLED

; Test that the tensor descriptor optimizer eliminates redundant insertelement
; instructions where the same index is overwritten.

@global_smem = external addrspace(3) global [0 x i8], align 16

; CHECK-LABEL: define amdgpu_kernel void @test_redundant_inserts_eliminated
; The redundant inserts of %a, %b, %c at indices 1, 2, 3 should be eliminated
; since they are immediately overwritten by constants 111, 222, 333.
; After optimization, the chain starts from the constant base, inserts 42 at index 5,
; then inserts 111, 222, 333 at indices 1, 2, 3.
; CHECK-NOT: insertelement <8 x i32> {{.*}}, i32 %a, i64 1
; CHECK-NOT: insertelement <8 x i32> {{.*}}, i32 %b, i64 2
; CHECK-NOT: insertelement <8 x i32> {{.*}}, i32 %c, i64 3
; CHECK: insertelement <8 x i32> <i32 100, i32 poison, i32 poison, i32 poison, i32 64, i32 poison, i32 0, i32 0>, i32 42, i64 5
; CHECK: insertelement <8 x i32> %{{.*}}, i32 111, i64 1
; CHECK: insertelement <8 x i32> %{{.*}}, i32 222, i64 2
; CHECK: insertelement <8 x i32> %{{.*}}, i32 333, i64 3
; CHECK: call void @llvm.amdgcn.tensor.load.to.lds

; When disabled, all inserts should be present:
; DISABLED-LABEL: define amdgpu_kernel void @test_redundant_inserts_eliminated
; DISABLED: insertelement <8 x i32> <i32 100, i32 poison, i32 poison, i32 poison, i32 64, i32 poison, i32 0, i32 0>, i32 %a, i64 1
; DISABLED: insertelement <8 x i32> %{{.*}}, i32 %b, i64 2
; DISABLED: insertelement <8 x i32> %{{.*}}, i32 %c, i64 3
define amdgpu_kernel void @test_redundant_inserts_eliminated(i32 %a, i32 %b, i32 %c) {
entry:
  ; First set - indices 1, 2, 3 will be overwritten
  %d0 = insertelement <8 x i32> <i32 100, i32 poison, i32 poison, i32 poison, i32 64, i32 poison, i32 0, i32 0>, i32 %a, i64 1
  %d1 = insertelement <8 x i32> %d0, i32 %b, i64 2
  %d2 = insertelement <8 x i32> %d1, i32 %c, i64 3
  %d3 = insertelement <8 x i32> %d2, i32 42, i64 5

  ; Second set - overwrites indices 1, 2, 3
  %d4 = insertelement <8 x i32> %d3, i32 111, i64 1
  %d5 = insertelement <8 x i32> %d4, i32 222, i64 2
  %d6 = insertelement <8 x i32> %d5, i32 333, i64 3

  %addr = insertelement <4 x i32> <i32 1, i32 0, i32 0, i32 0>, i32 0, i64 1
  %addr2 = insertelement <4 x i32> %addr, i32 0, i64 2
  %addr3 = insertelement <4 x i32> %addr2, i32 0, i64 3

  call void @llvm.amdgcn.tensor.load.to.lds(<4 x i32> %addr3, <8 x i32> %d6, <4 x i32> zeroinitializer, <4 x i32> zeroinitializer, <8 x i32> zeroinitializer, i32 0)
  ret void
}

; CHECK-LABEL: define amdgpu_kernel void @test_non_overwritten_preserved
; Inserts that are NOT overwritten should be preserved
; CHECK: insertelement <8 x i32> <i32 100, i32 poison, i32 poison, i32 poison, i32 64, i32 poison, i32 0, i32 0>, i32 %a, i64 1
; CHECK: insertelement <8 x i32> %{{.*}}, i32 %b, i64 2
; CHECK: insertelement <8 x i32> %{{.*}}, i32 42, i64 5
; CHECK: call void @llvm.amdgcn.tensor.load.to.lds
define amdgpu_kernel void @test_non_overwritten_preserved(i32 %a, i32 %b) {
entry:
  ; Index 1 is set but not overwritten - should be preserved
  %d0 = insertelement <8 x i32> <i32 100, i32 poison, i32 poison, i32 poison, i32 64, i32 poison, i32 0, i32 0>, i32 %a, i64 1
  ; Index 2 is set - should be preserved
  %d1 = insertelement <8 x i32> %d0, i32 %b, i64 2
  ; Index 5 is set - should be preserved
  %d2 = insertelement <8 x i32> %d1, i32 42, i64 5

  %addr = insertelement <4 x i32> <i32 1, i32 0, i32 0, i32 0>, i32 0, i64 1
  %addr2 = insertelement <4 x i32> %addr, i32 0, i64 2
  %addr3 = insertelement <4 x i32> %addr2, i32 0, i64 3

  call void @llvm.amdgcn.tensor.load.to.lds(<4 x i32> %addr3, <8 x i32> %d2, <4 x i32> zeroinitializer, <4 x i32> zeroinitializer, <8 x i32> zeroinitializer, i32 0)
  ret void
}

declare void @llvm.amdgcn.tensor.load.to.lds(<4 x i32>, <8 x i32>, <4 x i32>, <4 x i32>, <8 x i32>, i32)
