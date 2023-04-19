#pragma once
#include "BasicBlock.h"
#include "Constant.h"
#include "DeadCode.h"
#include "FuncInfo.h"
#include "Function.h"
#include "IRprinter.h"
#include "Instruction.h"
#include "Module.h"
#include "PassManager.hpp"
#include "Value.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GVNExpression
{

    // fold the constant value
    class ConstFolder
    {
    public:
        ConstFolder(Module *m) : module_(m) {}
        Constant *compute(Instruction *instr, Constant *value1, Constant *value2);
        Constant *compute(Instruction::OpID op, Constant *value1, Constant *value2);
        Constant *compute(Instruction *instr, Constant *value1);

    private:
        Module *module_;
    };

    /**
     * for constructor of class derived from `Expression`, we make it public
     * because `std::make_shared` needs the constructor to be publicly available,
     * but you should call the static factory method `create` instead the constructor itself to get the desired data
     */
    class Expression
    {
    public:
        // TODO: you need to extend expression types according to testcases
        enum gvn_expr_t
        {
            e_constant,
            e_bin,
            e_phi,
            e_var,
            e_cmp,
            e_call,
            e_gep,
            e_fcmp,
            e_only
        };
        Expression(gvn_expr_t t) : expr_type(t) {}
        virtual ~Expression() = default;
        virtual std::string print() = 0;
        virtual std::shared_ptr<Expression> get_lhs() = 0;
        virtual std::shared_ptr<Expression> get_rhs() = 0;
        virtual Instruction::OpID get_op() = 0;
        gvn_expr_t get_expr_type() const { return expr_type; }

    private:
        gvn_expr_t expr_type;
    };

    bool operator==(const std::shared_ptr<Expression> &lhs, const std::shared_ptr<Expression> &rhs);
    bool operator==(const GVNExpression::Expression &lhs, const GVNExpression::Expression &rhs);
    class GepExpression : public Expression
    {
    public:
        static std::shared_ptr<GepExpression> create(Type *ty, std::vector<std::shared_ptr<Expression>> args)
        {
            return std::make_shared<GepExpression>(ty, args);
        }
        virtual std::string print()
        {
            std::string ans = "";
            ans += "GEP ";
            for (auto it : args_)
                ans += it->print() + " ";
            return "(" + ans + ")";
        }
        bool equiv(const GepExpression *other) const
        {
            if (!(ty_ == other->ty_))
                return false;
            if (args_.size() != other->args_.size())
                return false;
            for (auto it1 = args_.begin(), it2 = other->args_.begin(); it1 != args_.end(); it1++, it2++)
            {
                if (!(*it1 == *it2))
                    return false;
            }
            return true;
        }
        GepExpression(Type *ty, std::vector<std::shared_ptr<Expression>> args) : Expression(e_gep), ty_(ty), args_(args) {}
        virtual std::shared_ptr<Expression> get_lhs() { return nullptr; }
        virtual std::shared_ptr<Expression> get_rhs() { return nullptr; }
        virtual Instruction::OpID get_op() { return {}; }

    private:
        Type *ty_;
        std::vector<std::shared_ptr<Expression>> args_;
    };
    class VarExpression : public Expression
    {
    public:
        static std::shared_ptr<VarExpression> create(std::string s) { return std::make_shared<VarExpression>(s); }
        virtual std::string print() { return name; }
        bool equiv(const VarExpression *other) const { return name == other->name; }
        VarExpression(std::string s) : Expression(e_var), name(s) {}
        virtual std::shared_ptr<Expression> get_lhs() { return nullptr; }
        virtual std::shared_ptr<Expression> get_rhs() { return nullptr; }
        virtual Instruction::OpID get_op() { return {}; }

    private:
        std::string name;
    };
    class CallExpression : public Expression
    {
    public:
        static std::shared_ptr<CallExpression> create(Function *func, std::vector<std::shared_ptr<Expression>> args)
        {
            return std::make_shared<CallExpression>(func, args);
        }
        virtual std::string print()
        {
            std::string ans = "";
            ans += func_->get_name() + " ";
            for (auto it : args_)
                ans += it->print() + " ";
            return "(" + ans + ")";
        }
        bool equiv(const CallExpression *other) const
        {
            if (!(func_ == other->func_))
                return false;
            if (args_.size() != other->args_.size())
                return false;
            for (auto it1 = args_.begin(), it2 = other->args_.begin(); it1 != args_.end(); it1++, it2++)
            {
                if (!(*it1 == *it2))
                    return false;
            }
            return true;
        }
        CallExpression(Function *func, std::vector<std::shared_ptr<Expression>> args) : Expression(e_call), func_(func), args_(args) {}
        virtual std::shared_ptr<Expression> get_lhs() { return nullptr; }
        virtual std::shared_ptr<Expression> get_rhs() { return nullptr; }
        virtual Instruction::OpID get_op() { return {}; }

    private:
        Function *func_;
        std::vector<std::shared_ptr<Expression>> args_;
    };
    class ConstantExpression : public Expression
    {
    public:
        static std::shared_ptr<ConstantExpression> create(Constant *c) { return std::make_shared<ConstantExpression>(c); }
        virtual std::string print() { return c_->print(); }
        // we leverage the fact that constants in lightIR have unique addresses
        bool equiv(const ConstantExpression *other) const { return c_ == other->c_; }
        ConstantExpression(Constant *c) : Expression(e_constant), c_(c) {}
        virtual std::shared_ptr<Expression> get_lhs() { return nullptr; }
        virtual std::shared_ptr<Expression> get_rhs() { return nullptr; }
        virtual Instruction::OpID get_op() { return {}; }
        Constant *get_const() { return c_; }

    private:
        Constant *c_;
    };
    class CmpExpression : public Expression
    {
    public:
        static std::shared_ptr<CmpExpression> create(CmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        {
            return std::make_shared<CmpExpression>(op, lhs, rhs);
        }
        virtual std::string print()
        {
            return "(" + print_cmp_type(op_) + " " + lhs_->print() + " " + rhs_->print() + ")";
        }
        bool equiv(const CmpExpression *other) const
        {
            if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
                return true;
            else
                return false;
        }
        CmpExpression(CmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
            : Expression(e_cmp), op_(op), lhs_(lhs), rhs_(rhs) {}
        virtual std::shared_ptr<Expression> get_lhs() { return lhs_; }
        virtual std::shared_ptr<Expression> get_rhs() { return rhs_; }
        virtual Instruction::OpID get_op() { return {}; }
        CmpInst::CmpOp get_cmp_op() { return op_; }

    private:
        CmpInst::CmpOp op_;
        std::shared_ptr<Expression> lhs_, rhs_;
    };
    class FCmpExpression : public Expression
    {
    public:
        static std::shared_ptr<FCmpExpression> create(FCmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        {
            return std::make_shared<FCmpExpression>(op, lhs, rhs);
        }
        virtual std::string print()
        {
            return "(" + print_fcmp_type(op_) + " " + lhs_->print() + " " + rhs_->print() + ")";
        }
        bool equiv(const FCmpExpression *other) const
        {
            if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
                return true;
            else
                return false;
        }
        FCmpExpression(FCmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
            : Expression(e_fcmp), op_(op), lhs_(lhs), rhs_(rhs) {}
        virtual std::shared_ptr<Expression> get_lhs() { return lhs_; }
        virtual std::shared_ptr<Expression> get_rhs() { return rhs_; }
        virtual Instruction::OpID get_op() { return {}; }
        FCmpInst::CmpOp get_cmp_op() { return op_; }

    private:
        FCmpInst::CmpOp op_;
        std::shared_ptr<Expression> lhs_, rhs_;
    };
    class OnlyExpression : public Expression
    {
    public:
        static std::shared_ptr<OnlyExpression> create(Instruction::OpID op, std::shared_ptr<Expression> ss)
        {
            return std::make_shared<OnlyExpression>(op, ss);
        }
        virtual std::string print()
        {
            return "(" + Instruction::get_instr_op_name(op_) + " " + hs->print() + ")";
        }
        bool equiv(const OnlyExpression *other) const
        {
            if (op_ == other->op_ and *hs == *other->hs)
                return true;
            else
                return false;
        }
        OnlyExpression(Instruction::OpID op, std::shared_ptr<Expression> ss) : Expression(e_only), op_(op), hs(ss) {}
        virtual std::shared_ptr<Expression> get_lhs() { return hs; }
        virtual std::shared_ptr<Expression> get_rhs() { return hs; }
        virtual Instruction::OpID get_op() { return op_; }

    private:
        Instruction::OpID op_;
        std::shared_ptr<Expression> hs;
    };
    // arithmetic expression
    class BinaryExpression : public Expression
    {
    public:
        static std::shared_ptr<BinaryExpression> create(Instruction::OpID op,
                                                        std::shared_ptr<Expression> lhs,
                                                        std::shared_ptr<Expression> rhs)
        {
            return std::make_shared<BinaryExpression>(op, lhs, rhs);
        }
        virtual std::string print()
        {
            return "(" + Instruction::get_instr_op_name(op_) + " " + lhs_->print() + " " + rhs_->print() + ")";
        }

        bool equiv(const BinaryExpression *other) const
        {
            if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
                return true;
            else
                return false;
        }

        BinaryExpression(Instruction::OpID op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
            : Expression(e_bin), op_(op), lhs_(lhs), rhs_(rhs) {}
        virtual std::shared_ptr<Expression> get_lhs() { return lhs_; }
        virtual std::shared_ptr<Expression> get_rhs() { return rhs_; }
        virtual Instruction::OpID get_op() { return op_; }

    private:
        Instruction::OpID op_;
        std::shared_ptr<Expression> lhs_, rhs_;
    };

    class PhiExpression : public Expression
    {
    public:
        static std::shared_ptr<PhiExpression> create(std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        {
            return std::make_shared<PhiExpression>(lhs, rhs);
        }
        virtual std::string print() { return "(phi " + lhs_->print() + " " + rhs_->print() + ")"; }
        bool equiv(const PhiExpression *other) const
        {
            if (*lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
                return true;
            else
                return false;
        }
        PhiExpression(std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
            : Expression(e_phi), lhs_(lhs), rhs_(rhs) {}
        virtual std::shared_ptr<Expression> get_lhs() { return lhs_; }
        virtual std::shared_ptr<Expression> get_rhs() { return rhs_; }
        virtual Instruction::OpID get_op() { return {}; }

    private:
        std::shared_ptr<Expression> lhs_, rhs_;
    };
} // namespace GVNExpression

/**
 * Congruence class in each partitions
 * note: for constant propagation, you might need to add other fields
 * and for load/store redundancy detection, you most certainly need to modify the class
 */
struct CongruenceClass
{
    size_t index_;
    // representative of the congruence class, used to replace all the members (except itself) when analysis is done
    Value *leader_;
    // value expression in congruence class
    std::shared_ptr<GVNExpression::Expression> value_expr_;
    // value φ-function is an annotation of the congruence class
    std::shared_ptr<GVNExpression::PhiExpression> value_phi_;
    // equivalent variables in one congruence class
    std::set<Value *> members_;

    CongruenceClass(size_t index) : index_(index), leader_{}, value_expr_{}, value_phi_{}, members_{} {}

    bool operator<(const CongruenceClass &other) const { return this->index_ < other.index_; }
    bool operator==(const CongruenceClass &other) const;
};

namespace std
{
    template <>
    // overload std::less for std::shared_ptr<CongruenceClass>, i.e. how to sort the congruence classes
    struct less<std::shared_ptr<CongruenceClass>>
    {
        bool operator()(const std::shared_ptr<CongruenceClass> &a, const std::shared_ptr<CongruenceClass> &b) const
        {
            // nullptrs should never appear in partitions, so we just dereference it
            return *a < *b;
        }
    };
} // namespace std

class GVN : public Pass
{
public:
    using partitions = std::set<std::shared_ptr<CongruenceClass>>;
    GVN(Module *m, bool dump_json) : Pass(m), dump_json_(dump_json) {}
    // pass start
    void run() override;
    // init for pass metadata;
    void initPerFunction();

    // fill the following functions according to Pseudocode, **you might need to add more arguments**
    void detectEquivalences();
    partitions join(const partitions &P1, const partitions &P2);
    std::shared_ptr<CongruenceClass> intersect(std::shared_ptr<CongruenceClass>, std::shared_ptr<CongruenceClass>);
    partitions transferFunction(Instruction *x, Value *e, partitions pin);
    std::shared_ptr<GVNExpression::PhiExpression> valuePhiFunc(std::shared_ptr<GVNExpression::Expression>,
                                                               const partitions &);
    std::shared_ptr<GVNExpression::Expression> valueExpr(Instruction *instr, partitions pin);
    std::shared_ptr<GVNExpression::Expression> getVN(const partitions &pout,
                                                     std::shared_ptr<GVNExpression::Expression> ve);

    // replace cc members with leader
    void replace_cc_members();

    // note: be careful when to use copy constructor or clone
    partitions clone(const partitions &p);

    // create congruence class helper
    std::shared_ptr<CongruenceClass> createCongruenceClass(size_t index = 0)
    {
        return std::make_shared<CongruenceClass>(index);
    }

private:
    bool dump_json_;
    std::uint64_t next_value_number_ = 1;
    Function *func_;
    std::map<BasicBlock *, partitions> pin_, pout_;
    std::unique_ptr<FuncInfo> func_info_;
    std::unique_ptr<GVNExpression::ConstFolder> folder_;
    std::unique_ptr<DeadCode> dce_;
};

bool operator==(const GVN::partitions &p1, const GVN::partitions &p2);
