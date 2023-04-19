# lab3 实验报告
PB20000137 李远航

## 实验要求

  * 实验部分:
    * 需要完善 `src/cminusfc/cminusf_builder.cpp`
    
    * 需要在 `Reports/3-ir-gen/` 目录下撰写实验报告 `report.md`
      * 实验报告内容主要包括实验要求、实现难点、实验反馈等（具体参考[repord.md](../../Reports/3-ir-gen/report.md)）
    
    * 本次实验收取 `src/cminusfc/cminusf_builder.cpp` 文件和 `Reports/3-ir-gen` 目录

## 实验难点

1. 对`scope`的理解存在困难
2. 调试存在困难
3. 对实验框架的部分函数使用并不熟练

## 实验设计

请写明为了顺利完成本次实验，加入了哪些亮点设计，并对这些设计进行解释。
可能的阐述方向有:

1. 如何设计全局变量

    > scope中有判断是否为全局变量的函数，在变量声明时，调用判断，并使用`lightir`中的`GlobalVariable`，如下所示，为声明一个变量的过程
    >
    > ```c++
    > if (scope.in_global())
    > {
    >     auto initializer = ConstantZero::get(var_type, module.get());
    >     auto var = GlobalVariable::create(node.id, module.get(), var_type, false, initializer);
    >     scope.push(node.id, var);
    > }
    > else
    > {
    >     auto var = builder->create_alloca(var_type);
    >     scope.push(node.id, var);
    > }
    > ```

2. 遇到的难点以及解决方案

    > - `scope`的理解
    >     - 阅读助教的tips文件，与同学交流
    > - 调试存在困难
    >     - F10会跳过相关函数，F11又经常跳到stl内部，解决的办法是慢慢F11
    > - var的实现存在困难
    >     - 没有处理下标为负数的情况，对create_store等的理解不够透彻，通过交流解决此问题

3. 实验完成的步骤

    > 本次实验，先从助教已经给出的代码入手，学习了主体流程的实现
    >
    > expression部分可以通过lab2中的计算器部分学习，发现了一个神奇的特性，也有可能是~~基础太差~~，三目运算符返回的值不一样会报错
    >
    > 第一遍实现代码没有理解scope的用处，认为只是一个用来调试观察的工具，导致第一次本地测试并不成功

### 实验总结

1. 对代码的翻译有了更深的理解
2. 增强了调试的能力
3. 学习了更多`c++`的特性
