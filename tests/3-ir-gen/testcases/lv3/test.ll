; ModuleID = 'cminus'
source_filename = "../tests/3-ir-gen/testcases/lv3/test.cminus"

declare i32 @input()

declare void @output(i32)

declare void @outputFloat(float)

declare void @neg_idx_except()

define void @main() {
label_entry:
  %op0 = alloca [2801 x i32]
  %op1 = alloca i32
  store i32 0, i32* %op1
  br label %label2
label2:                                                ; preds = %label_entry, %label12
  %op3 = load i32, i32* %op1
  %op4 = icmp slt i32 %op3, 2800
  %op5 = zext i1 %op4 to i32
  %op6 = icmp ne i32 %op5, 0
  br i1 %op6, label %label7, label %label10
label7:                                                ; preds = %label2
  %op8 = load i32, i32* %op1
  %op9 = icmp slt i32 %op8, 0
  br i1 %op9, label %label11, label %label12
label10:                                                ; preds = %label2
  ret void
label11:                                                ; preds = %label7
  call void @neg_idx_except()
  ret void
label12:                                                ; preds = %label7
  %op13 = getelementptr [2801 x i32], [2801 x i32]* %op0, i32 0, i32 %op8
  store i32 2000, i32* %op13
  %op14 = load i32, i32* %op1
  %op15 = add i32 %op14, 1
  store i32 %op15, i32* %op1
  br label %label2
}
