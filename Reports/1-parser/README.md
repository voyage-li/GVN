# lab1 实验报告

<p style="text-align:right">PB20000137 李远航</p>

## 实验要求

- 从无到有完成一个完整的 `Cminus-f` 解析器，包括基于 `flex` 的词法分析器和基于 `bison` 的语法分析器。

## 实验难点

- 初次接触`flex`和`bison`,使用存在难度
- 多行注释正则表达式的编写
- `debug`困难

## 实验设计

- 词法分析器部分使用正则表达式，然后根据匹配到的长度，增减 `pos` 的值,其中多行注释部分需要特别展开分析
- 语法分析器根据`Cminus-f`的语法，编写相关代码

## 实验结果验证

- 成功通过线上评测

- 自定义样例

  *主要测试注释的匹配情况*

  ```c
  /* this is my test */
  
  /*
     test
  */
  int function(int a, int b)
  {
      if (a < b)
          return a + b;
      else
          return a - b;
  }
  ```

  输出的语法树

  ```bash
  >--+ program
  |  >--+ declaration-list
  |  |  >--+ declaration
  |  |  |  >--+ fun-declaration
  |  |  |  |  >--+ type-specifier
  |  |  |  |  |  >--* int
  |  |  |  |  >--* function
  |  |  |  |  >--* (
  |  |  |  |  >--+ params
  |  |  |  |  |  >--+ param-list
  |  |  |  |  |  |  >--+ param-list
  |  |  |  |  |  |  |  >--+ param
  |  |  |  |  |  |  |  |  >--+ type-specifier
  |  |  |  |  |  |  |  |  |  >--* int
  |  |  |  |  |  |  |  |  >--* a
  |  |  |  |  |  |  >--* ,
  |  |  |  |  |  |  >--+ param
  |  |  |  |  |  |  |  >--+ type-specifier
  |  |  |  |  |  |  |  |  >--* int
  |  |  |  |  |  |  |  >--* b
  |  |  |  |  >--* )
  |  |  |  |  >--+ compound-stmt
  |  |  |  |  |  >--* {
  |  |  |  |  |  >--+ local-declarations
  |  |  |  |  |  |  >--* epsilon
  |  |  |  |  |  >--+ statement-list
  |  |  |  |  |  |  >--+ statement-list
  |  |  |  |  |  |  |  >--* epsilon
  |  |  |  |  |  |  >--+ statement
  |  |  |  |  |  |  |  >--+ selection-stmt
  |  |  |  |  |  |  |  |  >--* if
  |  |  |  |  |  |  |  |  >--* (
  |  |  |  |  |  |  |  |  >--+ expression
  |  |  |  |  |  |  |  |  |  >--+ simple-expression
  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
  |  |  |  |  |  |  |  |  |  |  |  >--+ term
  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* a
  |  |  |  |  |  |  |  |  |  |  >--+ relop
  |  |  |  |  |  |  |  |  |  |  |  >--* <
  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
  |  |  |  |  |  |  |  |  |  |  |  >--+ term
  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* b
  |  |  |  |  |  |  |  |  >--* )
  |  |  |  |  |  |  |  |  >--+ statement
  |  |  |  |  |  |  |  |  |  >--+ return-stmt
  |  |  |  |  |  |  |  |  |  |  >--* return
  |  |  |  |  |  |  |  |  |  |  >--+ expression
  |  |  |  |  |  |  |  |  |  |  |  >--+ simple-expression
  |  |  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ term
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* a
  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ addop
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* +
  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ term
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* b
  |  |  |  |  |  |  |  |  |  |  >--* ;
  |  |  |  |  |  |  |  |  >--* else
  |  |  |  |  |  |  |  |  >--+ statement
  |  |  |  |  |  |  |  |  |  >--+ return-stmt
  |  |  |  |  |  |  |  |  |  |  >--* return
  |  |  |  |  |  |  |  |  |  |  >--+ expression
  |  |  |  |  |  |  |  |  |  |  |  >--+ simple-expression
  |  |  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ term
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* a
  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ addop
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* -
  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ term
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* b
  |  |  |  |  |  |  |  |  |  |  >--* ;
  |  |  |  |  |  >--* }
  ```

## 实验反馈

- 实验文档描述的可能浅显了，从基础到完成实验需要自己很多的探究
- `cmakelists`可不可以加一个忽视 `warning`
