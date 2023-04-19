; 直接copy 大概
source_filename = "assign.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; 全局main
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca [10 x i32], align 16
  store i32 0, i32* %1, align 4
  ; %1 是返回值
  %3 = getelementptr [10 x i32], [10 x i32]* %2, i32 0, i32 0
  store i32 10, i32* %3, align 16
  ; a[0] = 10
  %4 = getelementptr [10 x i32], [10 x i32]* %2, i32 0, i32 0
  %5 = load i32, i32* %4, align 16
  %6 = mul nsw i32 %5, 2
  ; 取出a[0] * 2
  %7 = getelementptr [10 x i32], [10 x i32]* %2, i32 0, i32 1
  store i32 %6, i32* %7, align 4
  ; 赋值
  %8 = getelementptr [10 x i32], [10 x i32]* %2, i32 0, i32 1
  %9 = load i32, i32* %8, align 4
  ret i32 %9
  ; 返回
}

; 复制粘贴
attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.1 "}
