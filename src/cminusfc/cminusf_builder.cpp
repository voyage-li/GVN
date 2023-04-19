/*
 * 声明：本代码为 2020 秋 中国科大编译原理（李诚）课程实验参考实现。
 * 请不要以任何方式，将本代码上传到可以公开访问的站点或仓库
 */

#include "cminusf_builder.hpp"

#define CONST_FP(num) ConstantFP::get((float)num, module.get())
#define CONST_INT(num) ConstantInt::get(num, module.get())

// You can define global variables here
// to store state

// store temporary value
Value *tmp_val = nullptr;
// whether require lvalue
bool require_lvalue = false;
// function that is being built
Function *cur_fun = nullptr;
// detect scope pre-enter (for elegance only)
bool pre_enter_scope = false;

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *INT32PTR_T;
Type *FLOAT_T;
Type *FLOATPTR_T;

bool promote(IRBuilder *builder, Value **l_val_p, Value **r_val_p)
{
    bool is_int;
    auto &l_val = *l_val_p;
    auto &r_val = *r_val_p;
    if (l_val->get_type() == r_val->get_type())
    {
        is_int = l_val->get_type()->is_integer_type();
    }
    else
    {
        is_int = false;
        if (l_val->get_type()->is_integer_type())
            l_val = builder->create_sitofp(l_val, FLOAT_T);
        else
            r_val = builder->create_sitofp(r_val, FLOAT_T);
    }
    return is_int;
}

/*
 * use CMinusfBuilder::Scope to construct scopes
 * scope.enter: enter a new scope
 * scope.exit: exit current scope
 * scope.push: add a new binding to current scope
 * scope.find: find and return the value bound to the name
 */

void CminusfBuilder::visit(ASTProgram &node)
{
    VOID_T = Type::get_void_type(module.get());
    INT1_T = Type::get_int1_type(module.get());
    INT32_T = Type::get_int32_type(module.get());
    INT32PTR_T = Type::get_int32_ptr_type(module.get());
    FLOAT_T = Type::get_float_type(module.get());
    FLOATPTR_T = Type::get_float_ptr_type(module.get());

    for (auto decl : node.declarations)
    {
        decl->accept(*this);
    }
}

void CminusfBuilder::visit(ASTNum &node)
{
    if (node.type == TYPE_INT)
        tmp_val = CONST_INT(node.i_val);
    else
        tmp_val = CONST_FP(node.f_val);
}

void CminusfBuilder::visit(ASTVarDeclaration &node)
{
    Type *var_type;
    if (node.type == TYPE_INT)
        var_type = Type::get_int32_type(module.get());
    else
        var_type = Type::get_float_type(module.get());
    if (node.num == nullptr)
    {
        if (scope.in_global())
        {
            auto initializer = ConstantZero::get(var_type, module.get());
            auto var = GlobalVariable::create(node.id, module.get(), var_type, false, initializer);
            scope.push(node.id, var);
        }
        else
        {
            auto var = builder->create_alloca(var_type);
            scope.push(node.id, var);
        }
    }
    else
    {
        auto *array_type = ArrayType::get(var_type, node.num->i_val);
        if (scope.in_global())
        {
            auto initializer = ConstantZero::get(array_type, module.get());
            auto var = GlobalVariable::create(node.id, module.get(), array_type, false, initializer);
            scope.push(node.id, var);
        }
        else
        {
            auto var = builder->create_alloca(array_type);
            scope.push(node.id, var);
        }
    }
}

void CminusfBuilder::visit(ASTFunDeclaration &node)
{
    FunctionType *fun_type;
    Type *ret_type;
    std::vector<Type *> param_types;
    if (node.type == TYPE_INT)
        ret_type = INT32_T;
    else if (node.type == TYPE_FLOAT)
        ret_type = FLOAT_T;
    else
        ret_type = VOID_T;

    for (auto &param : node.params)
    {
        if (param->type == TYPE_INT)
        {
            if (param->isarray)
            {
                param_types.push_back(INT32PTR_T);
            }
            else
            {
                param_types.push_back(INT32_T);
            }
        }
        else
        {
            if (param->isarray)
            {
                param_types.push_back(FLOATPTR_T);
            }
            else
            {
                param_types.push_back(FLOAT_T);
            }
        }
    }

    fun_type = FunctionType::get(ret_type, param_types);
    auto fun = Function::create(fun_type, node.id, module.get());
    scope.push(node.id, fun);
    cur_fun = fun;
    auto funBB = BasicBlock::create(module.get(), "entry", fun);
    builder->set_insert_point(funBB);
    scope.enter();
    pre_enter_scope = true;
    std::vector<Value *> args;
    for (auto arg = fun->arg_begin(); arg != fun->arg_end(); arg++)
    {
        args.push_back(*arg);
    }
    for (int i = 0; i < node.params.size(); ++i)
    {
        if (node.params[i]->isarray)
        {
            Value *array_alloc;
            if (node.params[i]->type == TYPE_INT)
                array_alloc = builder->create_alloca(INT32PTR_T);
            else
                array_alloc = builder->create_alloca(FLOATPTR_T);
            builder->create_store(args[i], array_alloc);
            scope.push(node.params[i]->id, array_alloc);
        }
        else
        {
            Value *alloc;
            if (node.params[i]->type == TYPE_INT)
                alloc = builder->create_alloca(INT32_T);
            else
                alloc = builder->create_alloca(FLOAT_T);
            builder->create_store(args[i], alloc);
            scope.push(node.params[i]->id, alloc);
        }
    }
    node.compound_stmt->accept(*this);
    // can't deal with return in both blocks
    if (builder->get_insert_block()->get_terminator() == nullptr)
    {
        if (cur_fun->get_return_type()->is_void_type())
            builder->create_void_ret();
        else if (cur_fun->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));
    }
    scope.exit();
}

void CminusfBuilder::visit(ASTParam &node) {}

void CminusfBuilder::visit(ASTCompoundStmt &node)
{
    bool need_exit_scope = !pre_enter_scope;
    if (pre_enter_scope)
    {
        pre_enter_scope = false;
    }
    else
    {
        scope.enter();
    }

    for (auto &decl : node.local_declarations)
    {
        decl->accept(*this);
    }

    for (auto &stmt : node.statement_list)
    {
        stmt->accept(*this);
        if (builder->get_insert_block()->get_terminator() != nullptr)
            break;
    }

    if (need_exit_scope)
    {
        scope.exit();
    }
}

void CminusfBuilder::visit(ASTExpressionStmt &node)
{
    if (node.expression != nullptr)
        node.expression->accept(*this);
}

void CminusfBuilder::visit(ASTSelectionStmt &node)
{
    node.expression->accept(*this);
    auto ret_val = tmp_val;
    auto trueBB = BasicBlock::create(module.get(), "", cur_fun);
    BasicBlock *falseBB{};
    auto contBB = BasicBlock::create(module.get(), "", cur_fun);
    Value *cond_val;
    if (ret_val->get_type()->is_integer_type())
        cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
    else
        cond_val = builder->create_fcmp_ne(ret_val, CONST_FP(0.));

    if (node.else_statement == nullptr)
    {
        builder->create_cond_br(cond_val, trueBB, contBB);
    }
    else
    {
        falseBB = BasicBlock::create(module.get(), "", cur_fun);
        builder->create_cond_br(cond_val, trueBB, falseBB);
    }
    builder->set_insert_point(trueBB);
    node.if_statement->accept(*this);

    if (builder->get_insert_block()->get_terminator() == nullptr)
        builder->create_br(contBB);

    if (node.else_statement == nullptr)
    {
        // falseBB->erase_from_parent(); // did not clean up memory
    }
    else
    {
        builder->set_insert_point(falseBB);
        node.else_statement->accept(*this);
        if (builder->get_insert_block()->get_terminator() == nullptr)
            builder->create_br(contBB);
    }

    builder->set_insert_point(contBB);
}

void CminusfBuilder::visit(ASTIterationStmt &node)
{
    auto exprBB = BasicBlock::create(module.get(), "", cur_fun);
    if (builder->get_insert_block()->get_terminator() == nullptr)
        builder->create_br(exprBB);
    builder->set_insert_point(exprBB);
    node.expression->accept(*this);
    auto ret_val = tmp_val;
    auto trueBB = BasicBlock::create(module.get(), "", cur_fun);
    auto contBB = BasicBlock::create(module.get(), "", cur_fun);
    Value *cond_val;
    if (ret_val->get_type()->is_integer_type())
        cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
    else
        cond_val = builder->create_fcmp_ne(ret_val, CONST_FP(0.));

    builder->create_cond_br(cond_val, trueBB, contBB);
    builder->set_insert_point(trueBB);
    node.statement->accept(*this);
    if (builder->get_insert_block()->get_terminator() == nullptr)
        builder->create_br(exprBB);
    builder->set_insert_point(contBB);
}

void CminusfBuilder::visit(ASTReturnStmt &node)
{
    if (node.expression == nullptr)
    {
        builder->create_void_ret();
    }
    else
    {
        auto fun_ret_type = cur_fun->get_function_type()->get_return_type();
        node.expression->accept(*this);
        if (fun_ret_type != tmp_val->get_type())
        {
            if (fun_ret_type->is_integer_type())
                tmp_val = builder->create_fptosi(tmp_val, INT32_T);
            else
                tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
        }

        builder->create_ret(tmp_val);
    }
}

void CminusfBuilder::visit(ASTVar &node)
{
    auto var = scope.find(node.id);
    assert(var != nullptr);
    auto is_int = var->get_type()->get_pointer_element_type()->is_integer_type();
    auto is_float = var->get_type()->get_pointer_element_type()->is_float_type();
    auto is_ptr = var->get_type()->get_pointer_element_type()->is_pointer_type();
    bool should_return_lvalue = require_lvalue;
    require_lvalue = false;
    if (node.expression == nullptr)
    {
        if (should_return_lvalue)
        {
            tmp_val = var;
            require_lvalue = false;
        }
        else
        {
            if (is_int || is_float || is_ptr)
            {
                tmp_val = builder->create_load(var);
            }
            else
            {
                tmp_val = builder->create_gep(var, {CONST_INT(0), CONST_INT(0)});
            }
        }
    }
    else
    {
        node.expression->accept(*this);
        auto val = tmp_val;
        Value *is_neg;
        auto exceptBB = BasicBlock::create(module.get(), "", cur_fun);
        auto contBB = BasicBlock::create(module.get(), "", cur_fun);
        if (val->get_type()->is_float_type())
            val = builder->create_fptosi(val, INT32_T);

        is_neg = builder->create_icmp_lt(val, CONST_INT(0));

        builder->create_cond_br(is_neg, exceptBB, contBB);
        builder->set_insert_point(exceptBB);
        auto neg_idx_except_fun = scope.find("neg_idx_except");
        builder->create_call(static_cast<Function *>(neg_idx_except_fun), {});
        if (cur_fun->get_return_type()->is_void_type())
            builder->create_void_ret();
        else if (cur_fun->get_return_type()->is_float_type())
            builder->create_ret(CONST_FP(0.));
        else
            builder->create_ret(CONST_INT(0));

        builder->set_insert_point(contBB);
        Value *tmp_ptr;
        if (is_int || is_float)
            tmp_ptr = builder->create_gep(var, {val});
        else if (is_ptr)
        {
            auto array_load = builder->create_load(var);
            tmp_ptr = builder->create_gep(array_load, {val});
        }
        else
            tmp_ptr = builder->create_gep(var, {CONST_INT(0), val});
        if (should_return_lvalue)
        {
            tmp_val = tmp_ptr;
            require_lvalue = false;
        }
        else
        {
            tmp_val = builder->create_load(tmp_ptr);
        }
    }
}

void CminusfBuilder::visit(ASTAssignExpression &node)
{
    node.expression->accept(*this);
    auto expr_result = tmp_val;
    require_lvalue = true;
    node.var->accept(*this);
    auto var_addr = tmp_val;
    if (var_addr->get_type()->get_pointer_element_type() != expr_result->get_type())
    {
        if (expr_result->get_type() == INT32_T)
            expr_result = builder->create_sitofp(expr_result, FLOAT_T);
        else
            expr_result = builder->create_fptosi(expr_result, INT32_T);
    }
    builder->create_store(expr_result, var_addr);
    tmp_val = expr_result;
}

void CminusfBuilder::visit(ASTSimpleExpression &node)
{
    if (node.additive_expression_r == nullptr)
    {
        node.additive_expression_l->accept(*this);
    }
    else
    {
        node.additive_expression_l->accept(*this);
        auto l_val = tmp_val;
        node.additive_expression_r->accept(*this);
        auto r_val = tmp_val;
        bool is_int = promote(&*builder, &l_val, &r_val);
        Value *cmp;
        switch (node.op)
        {
        case OP_LT:
            if (is_int)
                cmp = builder->create_icmp_lt(l_val, r_val);
            else
                cmp = builder->create_fcmp_lt(l_val, r_val);
            break;
        case OP_LE:
            if (is_int)
                cmp = builder->create_icmp_le(l_val, r_val);
            else
                cmp = builder->create_fcmp_le(l_val, r_val);
            break;
        case OP_GE:
            if (is_int)
                cmp = builder->create_icmp_ge(l_val, r_val);
            else
                cmp = builder->create_fcmp_ge(l_val, r_val);
            break;
        case OP_GT:
            if (is_int)
                cmp = builder->create_icmp_gt(l_val, r_val);
            else
                cmp = builder->create_fcmp_gt(l_val, r_val);
            break;
        case OP_EQ:
            if (is_int)
                cmp = builder->create_icmp_eq(l_val, r_val);
            else
                cmp = builder->create_fcmp_eq(l_val, r_val);
            break;
        case OP_NEQ:
            if (is_int)
                cmp = builder->create_icmp_ne(l_val, r_val);
            else
                cmp = builder->create_fcmp_ne(l_val, r_val);
            break;
        }

        tmp_val = builder->create_zext(cmp, INT32_T);
    }
}

void CminusfBuilder::visit(ASTAdditiveExpression &node)
{
    if (node.additive_expression == nullptr)
    {
        node.term->accept(*this);
    }
    else
    {
        node.additive_expression->accept(*this);
        auto l_val = tmp_val;
        node.term->accept(*this);
        auto r_val = tmp_val;
        bool is_int = promote(&*builder, &l_val, &r_val);
        switch (node.op)
        {
        case OP_PLUS:
            if (is_int)
                tmp_val = builder->create_iadd(l_val, r_val);
            else
                tmp_val = builder->create_fadd(l_val, r_val);
            break;
        case OP_MINUS:
            if (is_int)
                tmp_val = builder->create_isub(l_val, r_val);
            else
                tmp_val = builder->create_fsub(l_val, r_val);
            break;
        }
    }
}

void CminusfBuilder::visit(ASTTerm &node)
{
    if (node.term == nullptr)
    {
        node.factor->accept(*this);
    }
    else
    {
        node.term->accept(*this);
        auto l_val = tmp_val;
        node.factor->accept(*this);
        auto r_val = tmp_val;
        bool is_int = promote(&*builder, &l_val, &r_val);
        switch (node.op)
        {
        case OP_MUL:
            if (is_int)
                tmp_val = builder->create_imul(l_val, r_val);
            else
                tmp_val = builder->create_fmul(l_val, r_val);
            break;
        case OP_DIV:
            if (is_int)
                tmp_val = builder->create_isdiv(l_val, r_val);
            else
                tmp_val = builder->create_fdiv(l_val, r_val);
            break;
        }
    }
}

void CminusfBuilder::visit(ASTCall &node)
{
    auto fun = static_cast<Function *>(scope.find(node.id));
    std::vector<Value *> args;
    auto param_type = fun->get_function_type()->param_begin();
    for (auto &arg : node.args)
    {
        arg->accept(*this);
        if (!tmp_val->get_type()->is_pointer_type() && *param_type != tmp_val->get_type())
        {
            if (tmp_val->get_type()->is_integer_type())
                tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
            else
                tmp_val = builder->create_fptosi(tmp_val, INT32_T);
        }
        args.push_back(tmp_val);
        param_type++;
    }

    tmp_val = builder->create_call(static_cast<Function *>(fun), args);
}

// // 这也太困难了 不想玩辣

// #include "cminusf_builder.hpp"

// #define CONST_FP(num) ConstantFP::get((float)num, module.get())
// #define CONST_INT(num) ConstantInt::get(num, module.get())

// // TODO: Global Variable Declarations
// // You can define global variables here
// // to store state. You can expand these
// // definitions if you need to.

// // function that is being built
// // 存储当前value
// Value *tmp_val = nullptr;
// // 当前函数
// Function *cur_fun = nullptr;
// // 表示是否在之前已经进入scope，用于CompoundStmt
// // 进入CompoundStmt不仅包括通过Fundeclaration进入，也包括selection-stmt等。
// // pre_enter_scope用于控制是否在CompoundStmt中添加scope.enter,scope.exit
// bool pre_enter_scope = false;
// bool return_addr = false;

// // types
// Type *VOID_T;
// Type *INT1_T;
// Type *INT32_T;
// Type *INT32PTR_T;
// Type *FLOAT_T;
// Type *FLOATPTR_T;

// /*
//  * use CMinusfBuilder::Scope to construct scopes
//  * scope.enter: enter a new scope
//  * scope.exit: exit current scope
//  * scope.push: add a new binding to current scope
//  * scope.find: find and return the value bound to the name
//  */

// void CminusfBuilder::visit(ASTProgram &node)
// {
//     VOID_T = Type::get_void_type(module.get());
//     INT1_T = Type::get_int1_type(module.get());
//     INT32_T = Type::get_int32_type(module.get());
//     INT32PTR_T = Type::get_int32_ptr_type(module.get());
//     FLOAT_T = Type::get_float_type(module.get());
//     FLOATPTR_T = Type::get_float_ptr_type(module.get());

//     for (auto decl : node.declarations)
//     {                        // program -> declaration-list
//         decl->accept(*this); // 进入下一层函数
//     }
// }

// void CminusfBuilder::visit(ASTNum &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     if (node.type == TYPE_INT)
//         tmp_val = CONST_INT(node.i_val);
//     else
//         tmp_val = CONST_FP(node.f_val);
// }

// void CminusfBuilder::visit(ASTVarDeclaration &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     // type-specifier ID ; ∣ type-specifier ID [ INTEGER ] ;
//     Type *var_type;
//     if (node.type == TYPE_INT)
//         var_type = INT32_T;
//     else
//         var_type = FLOAT_T;
//     if (node.num == nullptr)
//     {
//         if (scope.in_global())
//         {
//             auto initializer = ConstantZero::get(var_type, module.get());
//             auto var = GlobalVariable::create(node.id, module.get(), var_type, false, initializer);
//             scope.push(node.id, var);
//         }
//         else
//         {
//             auto var = builder->create_alloca(var_type);
//             scope.push(node.id, var);
//         }
//     }
//     else
//     {
//         auto *array_type = ArrayType::get(var_type, node.num->i_val);
//         if (scope.in_global())
//         {
//             auto initializer = ConstantZero::get(array_type, module.get());
//             auto var = GlobalVariable::create(node.id, module.get(), array_type, false, initializer);
//             scope.push(node.id, var);
//         }
//         else
//         {
//             auto var = builder->create_alloca(array_type);
//             scope.push(node.id, var);
//         }
//     }
// }

// void CminusfBuilder::visit(ASTFunDeclaration &node)
// {
//     FunctionType *fun_type;
//     Type *ret_type;
//     std::vector<Type *> param_types;
//     if (node.type == TYPE_INT) // 函数返回值类型
//         ret_type = INT32_T;
//     else if (node.type == TYPE_FLOAT)
//         ret_type = FLOAT_T;
//     else
//         ret_type = VOID_T;

//     for (auto &param : node.params)
//     { // 补全param_types
//       // TODO：
//       // 根据param的类型分类
//       // 需要考虑int型数组，int型，float型数组，float型
//       // 对于不同的类型，向param_types中添加不同的Type
//       // param_types.push_back
//         if (param->isarray)
//         {
//             if (param->type == TYPE_INT)
//                 param_types.push_back(INT32PTR_T);
//             else
//                 param_types.push_back(FLOATPTR_T);
//         }
//         else
//         {
//             if (param->type == TYPE_INT)
//                 param_types.push_back(INT32_T);
//             else
//                 param_types.push_back(FLOAT_T);
//         }
//     }

//     fun_type = FunctionType::get(ret_type, param_types);
//     auto fun =
//         Function::create(
//             fun_type,
//             node.id,
//             module.get()); // 定义函数变量
//     scope.push(node.id, fun);
//     cur_fun = fun;
//     auto funBB = BasicBlock::create(module.get(), "entry", fun); // 创建基本块
//     builder->set_insert_point(funBB);
//     scope.enter();
//     pre_enter_scope = true;
//     std::vector<Value *> args;
//     for (auto arg = fun->arg_begin(); arg != fun->arg_end(); arg++)
//     {
//         args.push_back(*arg);
//     }
//     for (int i = 0; i < node.params.size(); ++i)
//     {
//         // TODO：
//         // 需要考虑int型数组，int型，float型数组，float型
//         // builder->create_alloca创建alloca语句
//         // builder->create_store创建store语句
//         // scope.push
//         if (node.params[i]->isarray)
//         {
//             Value *alloc;
//             if (node.params[i]->type == TYPE_INT)
//                 alloc = builder->create_alloca(INT32PTR_T);
//             else
//                 alloc = builder->create_alloca(FLOATPTR_T);
//             builder->create_store(args[i], alloc);
//             scope.push(node.params[i]->id, alloc);
//         }
//         else
//         {
//             Value *alloc;
//             if (node.params[i]->type == TYPE_INT)
//                 alloc = builder->create_alloca(INT32_T);
//             else
//                 alloc = builder->create_alloca(FLOAT_T);
//             builder->create_store(args[i], alloc);
//             scope.push(node.params[i]->id, alloc);
//         }
//     }
//     node.compound_stmt->accept(*this); // fun-declaration -> type-specifier ID ( params ) compound-stmt
//     if (builder->get_insert_block()->get_terminator() == nullptr)
//     { // 创建ret语句
//         if (cur_fun->get_return_type()->is_void_type())
//             builder->create_void_ret();
//         else if (cur_fun->get_return_type()->is_float_type())
//             builder->create_ret(CONST_FP(0.));
//         else
//             builder->create_ret(CONST_INT(0));
//     }
//     scope.exit();
// }

// void CminusfBuilder::visit(ASTParam &node) {}

// void CminusfBuilder::visit(ASTCompoundStmt &node)
// {
//     // TODO：此函数为完整实现
//     bool need_exit_scope = !pre_enter_scope; // 添加need_exit_scope变量
//     if (pre_enter_scope)
//     {
//         pre_enter_scope = false;
//     }
//     else
//     {
//         scope.enter();
//     }

//     for (auto &decl : node.local_declarations)
//     { // compound-stmt -> { local-declarations statement-list }
//         decl->accept(*this);
//     }

//     for (auto &stmt : node.statement_list)
//     {
//         stmt->accept(*this);
//         if (builder->get_insert_block()->get_terminator() != nullptr)
//             break;
//     }

//     if (need_exit_scope)
//     {
//         scope.exit();
//     }
// }

// void CminusfBuilder::visit(ASTExpressionStmt &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     if (node.expression != nullptr)
//         node.expression->accept(*this);
// }
// void CminusfBuilder::visit(ASTSelectionStmt &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     // if else 语句
//     node.expression->accept(*this);
//     auto ret_val = tmp_val;
//     auto trueBB = BasicBlock::create(module.get(), "", cur_fun);
//     auto falseBB = BasicBlock::create(module.get(), "", cur_fun);
//     auto retBB = BasicBlock::create(module.get(), "", cur_fun);
//     Value *cmp;
//     if (ret_val->get_type()->is_integer_type())
//         cmp = builder->create_icmp_ne(ret_val, CONST_INT(0));
//     else
//         cmp = builder->create_fcmp_ne(ret_val, CONST_FP(0.0));
//     if (node.else_statement == nullptr)
//         builder->create_cond_br(cmp, trueBB, retBB);
//     else
//         builder->create_cond_br(cmp, trueBB, falseBB);
//     builder->set_insert_point(trueBB);
//     node.if_statement->accept(*this);
//     if (builder->get_insert_block()->get_terminator() == nullptr)
//         builder->create_br(retBB);
//     if (node.else_statement == nullptr)
//         falseBB->erase_from_parent();
//     else
//     {
//         builder->set_insert_point(falseBB);
//         node.else_statement->accept(*this);
//         if (builder->get_insert_block()->get_terminator() == nullptr)
//             builder->create_br(retBB);
//     }
//     builder->set_insert_point(retBB);
// }

// void CminusfBuilder::visit(ASTIterationStmt &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     // while 语句  已经完全掌握了
//     auto exprBB = BasicBlock::create(module.get(), "", cur_fun);
//     if (builder->get_insert_block()->get_terminator() == nullptr)
//         builder->create_br(exprBB);
//     builder->set_insert_point(exprBB);
//     // 与if不同的是expression需要多次判断
//     node.expression->accept(*this);
//     auto ret_val = tmp_val;
//     auto trueBB = BasicBlock::create(module.get(), "", cur_fun);
//     auto falseBB = BasicBlock::create(module.get(), "", cur_fun);
//     Value *cmp;
//     if (ret_val->get_type()->is_integer_type())
//         cmp = builder->create_icmp_ne(ret_val, CONST_INT(0));
//     else
//         cmp = builder->create_fcmp_ne(ret_val, CONST_FP(0.0));
//     builder->create_cond_br(cmp, trueBB, falseBB);
//     builder->set_insert_point(trueBB);
//     node.statement->accept(*this);
//     if (builder->get_insert_block()->get_terminator() == nullptr)
//         builder->create_br(exprBB);
//     builder->set_insert_point(falseBB);
// }

// void CminusfBuilder::visit(ASTReturnStmt &node)
// {
//     if (node.expression == nullptr)
//     {
//         builder->create_void_ret();
//     }
//     else
//     {
//         //! TODO: The given code is incomplete.
//         // You need to solve other return cases (e.g. return an integer).
//         auto ret_type = cur_fun->get_function_type()->get_return_type();
//         node.expression->accept(*this);
//         if (ret_type != tmp_val->get_type())
//         {
//             // TODO：
//             // 需要考虑类型转换
//             // 函数返回值和表达式值类型不同时，转换成函数返回值的类型
//             // 用cur_fun获取当前函数返回值类型
//             // 类型转换：builder->create_fptosi
//             // ret语句
//             if (ret_type->is_integer_type())
//                 tmp_val = builder->create_fptosi(tmp_val, INT32_T);
//             else
//                 tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
//         }
//         builder->create_ret(tmp_val);
//     }
// }

// void CminusfBuilder::visit(ASTVar &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     // ID ∣ ID [ expression]
//     auto var = scope.find(node.id);
//     auto is_int = var->get_type()->get_pointer_element_type()->is_integer_type();
//     auto is_float = var->get_type()->get_pointer_element_type()->is_float_type();
//     auto is_ptr = var->get_type()->get_pointer_element_type()->is_pointer_type();
//     bool temp = return_addr;
//     return_addr = false;
//     if (node.expression == nullptr)
//     {
//         // 取决于是取值还是赋值
//         if (temp)
//             tmp_val = var;
//         else
//         {
//             if (is_int || is_float || is_ptr)
//                 tmp_val = builder->create_load(var);
//             else
//                 tmp_val = builder->create_gep(var, {CONST_INT(0), CONST_INT(0)});
//             // 返回首
//         }
//     }
//     else
//     {
//         node.expression->accept(*this);
//         auto val = tmp_val; // 接受表达式值
//         Value *negative;    // 判断下标取得是否为负数
//         auto errorBB = BasicBlock::create(module.get(), "", cur_fun);
//         auto successBB = BasicBlock::create(module.get(), "", cur_fun);
//         // 下标肯定是int
//         if (val->get_type()->is_float_type())
//             val = builder->create_fptosi(val, INT32_T);
//         negative = builder->create_icmp_lt(val, CONST_INT(0));
//         builder->create_cond_br(negative, errorBB, successBB);

//         builder->set_insert_point(errorBB);
//         auto neg_idx_except_fun = (Function *)scope.find("neg_idx_except"); // 这写在hpp内的函数也寐介绍啊
//         builder->create_call(neg_idx_except_fun, {});
//         if (cur_fun->get_return_type()->is_void_type())
//             builder->create_void_ret();
//         else if (cur_fun->get_return_type()->is_integer_type())
//             builder->create_ret(CONST_INT(0));
//         else
//             builder->create_ret(CONST_FP(0.0));

//         builder->set_insert_point(successBB);
//         Value *tmp_ptr;
//         if (is_int || is_float)
//             tmp_ptr = builder->create_gep(var, {val});
//         else if (is_ptr)
//         {
//             auto array_load = builder->create_load(var);
//             tmp_ptr = builder->create_gep(array_load, {val});
//         }
//         else
//             tmp_ptr = builder->create_gep(var, {CONST_INT(0), val});
//         if (temp)
//         {
//             tmp_val = tmp_ptr;
//             return_addr = false;
//         }
//         else
//             tmp_val = builder->create_load(tmp_ptr);
//     }
// }

// void CminusfBuilder::visit(ASTAssignExpression &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     // 赋值语句： var = expression
//     node.expression->accept(*this);
//     auto result = tmp_val;
//     return_addr = true; // 需要获得地址
//     node.var->accept(*this);
//     auto var_result = tmp_val;
//     // var_result是地址，result是表达式的值，需要强制类型转换
//     if (var_result->get_type()->get_pointer_element_type() != result->get_type())
//     {
//         if (var_result->get_type()->get_pointer_element_type() == INT32_T)
//             result = builder->create_fptosi(result, INT32_T);
//         else
//             result = builder->create_sitofp(result, FLOAT_T);
//     }
//     builder->create_store(result, var_result);
//     tmp_val = result;
//     // 赋值表达式返回赋值
// }

// void CminusfBuilder::visit(ASTSimpleExpression &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     // additive-expression relop additive-expression | additive-expression
//     if (node.additive_expression_r == nullptr)
//         node.additive_expression_l->accept(*this);
//     else
//     {
//         node.additive_expression_l->accept(*this);
//         auto left = tmp_val;
//         node.additive_expression_r->accept(*this);
//         auto right = tmp_val;
//         int judge = 1;
//         if (left->get_type() != right->get_type())
//         {
//             if (left->get_type()->is_integer_type())
//                 left = builder->create_sitofp(left, FLOAT_T);
//             else
//                 right = builder->create_sitofp(right, FLOAT_T);
//         }
//         if (left->get_type()->is_float_type())
//             judge = 0;
//         Value *cmp;
//         // 这里不能写三目运算符，会报错，因为返回值不一样
//         if (node.op == OP_LE)
//         {
//             if (judge)
//                 cmp = builder->create_icmp_le(left, right);
//             else
//                 cmp = builder->create_fcmp_le(left, right);
//         }
//         else if (node.op == OP_LT)
//         {
//             if (judge)
//                 cmp = builder->create_icmp_lt(left, right);
//             else
//                 cmp = builder->create_fcmp_lt(left, right);
//         }
//         else if (node.op == OP_GT)
//         {
//             if (judge)
//                 cmp = builder->create_icmp_gt(left, right);
//             else
//                 cmp = builder->create_fcmp_gt(left, right);
//         }
//         else if (node.op == OP_GE)
//         {
//             if (judge)
//                 cmp = builder->create_icmp_ge(left, right);
//             else
//                 cmp = builder->create_fcmp_ge(left, right);
//         }
//         else if (node.op == OP_EQ)
//         {
//             if (judge)
//                 cmp = builder->create_icmp_eq(left, right);
//             else
//                 cmp = builder->create_fcmp_eq(left, right);
//         }
//         else if (node.op == OP_NEQ)
//         {
//             if (judge)
//                 cmp = builder->create_icmp_ne(left, right);
//             else
//                 cmp = builder->create_fcmp_ne(left, right);
//         }
//         tmp_val = builder->create_zext(cmp, INT32_T);
//     }
// }

// void CminusfBuilder::visit(ASTAdditiveExpression &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     // additive-expression addop term ∣ term
//     if (node.additive_expression == nullptr)
//         node.term->accept(*this);
//     else
//     {
//         node.additive_expression->accept(*this);
//         auto left = tmp_val;
//         node.term->accept(*this);
//         auto right = tmp_val;
//         int judge = 1;
//         if (left->get_type() != right->get_type())
//         {
//             if (left->get_type()->is_integer_type())
//                 left = builder->create_sitofp(left, FLOAT_T);
//             else
//                 right = builder->create_sitofp(right, FLOAT_T);
//         }
//         if (left->get_type()->is_float_type())
//             judge = 0;
//         if (node.op == OP_PLUS)
//             tmp_val = judge ? builder->create_iadd(left, right) : builder->create_fadd(left, right);
//         else if (node.op == OP_MINUS)
//             tmp_val = judge ? builder->create_isub(left, right) : builder->create_fsub(left, right);
//     }
// }

// void CminusfBuilder::visit(ASTTerm &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     // term mulop factor ∣ factor
//     if (node.term == nullptr)
//         node.factor->accept(*this);
//     else
//     {
//         node.term->accept(*this);
//         auto left = tmp_val;
//         node.factor->accept(*this);
//         auto right = tmp_val;
//         int judge = 1;
//         if (left->get_type() != right->get_type())
//         {
//             if (left->get_type()->is_integer_type())
//                 left = builder->create_sitofp(left, FLOAT_T);
//             else
//                 right = builder->create_sitofp(right, FLOAT_T);
//         }
//         if (left->get_type()->is_float_type())
//             judge = 0;
//         if (node.op == OP_MUL)
//             tmp_val = judge ? builder->create_imul(left, right) : builder->create_fmul(left, right);
//         else if (node.op == OP_DIV)
//             tmp_val = judge ? builder->create_isdiv(left, right) : builder->create_fdiv(left, right);
//     }
// }

// void CminusfBuilder::visit(ASTCall &node)
// {
//     //! TODO: This function is empty now.
//     // Add some code here.
//     // ID (args)
//     auto fun = (Function *)scope.find(node.id);
//     std::vector<Value *> args;
//     auto param_type = fun->get_function_type()->param_begin();
//     for (auto arg : node.args)
//     {
//         arg->accept(*this);
//         if (*param_type != tmp_val->get_type())
//         {
//             if (tmp_val->get_type()->is_integer_type())
//                 tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
//             else
//                 tmp_val = builder->create_fptosi(tmp_val, INT32_T);
//         }
//         args.push_back(tmp_val);
//         param_type++;
//     }
//     tmp_val = builder->create_call(fun, args);
// }