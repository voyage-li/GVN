#include "GVN.h"

#include "BasicBlock.h"
#include "Constant.h"
#include "DeadCode.h"
#include "FuncInfo.h"
#include "Function.h"
#include "Instruction.h"
#include "logging.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

using namespace GVNExpression;
using std::string_literals::operator""s;
using std::shared_ptr;

BasicBlock *now_bb;

static auto get_const_int_value = [](Value *v)
{ return dynamic_cast<ConstantInt *>(v)->get_value(); };
static auto get_const_fp_value = [](Value *v)
{ return dynamic_cast<ConstantFP *>(v)->get_value(); };
// Constant Propagation helper, folders are done for you
Constant *ConstFolder::compute(Instruction::OpID op, Constant *value1, Constant *value2)
{
    switch (op)
    {
    case Instruction::add:
        return ConstantInt::get(get_const_int_value(value1) + get_const_int_value(value2), module_);
    case Instruction::sub:
        return ConstantInt::get(get_const_int_value(value1) - get_const_int_value(value2), module_);
    case Instruction::mul:
        return ConstantInt::get(get_const_int_value(value1) * get_const_int_value(value2), module_);
    case Instruction::sdiv:
        return ConstantInt::get(get_const_int_value(value1) / get_const_int_value(value2), module_);
    case Instruction::fadd:
        return ConstantFP::get(get_const_fp_value(value1) + get_const_fp_value(value2), module_);
    case Instruction::fsub:
        return ConstantFP::get(get_const_fp_value(value1) - get_const_fp_value(value2), module_);
    case Instruction::fmul:
        return ConstantFP::get(get_const_fp_value(value1) * get_const_fp_value(value2), module_);
    case Instruction::fdiv:
        return ConstantFP::get(get_const_fp_value(value1) / get_const_fp_value(value2), module_);
    default:
        return nullptr;
    }
}
Constant *ConstFolder::compute(Instruction *instr, Constant *value1, Constant *value2)
{
    auto op = instr->get_instr_type();
    switch (op)
    {
    case Instruction::add:
        return ConstantInt::get(get_const_int_value(value1) + get_const_int_value(value2), module_);
    case Instruction::sub:
        return ConstantInt::get(get_const_int_value(value1) - get_const_int_value(value2), module_);
    case Instruction::mul:
        return ConstantInt::get(get_const_int_value(value1) * get_const_int_value(value2), module_);
    case Instruction::sdiv:
        return ConstantInt::get(get_const_int_value(value1) / get_const_int_value(value2), module_);
    case Instruction::fadd:
        return ConstantFP::get(get_const_fp_value(value1) + get_const_fp_value(value2), module_);
    case Instruction::fsub:
        return ConstantFP::get(get_const_fp_value(value1) - get_const_fp_value(value2), module_);
    case Instruction::fmul:
        return ConstantFP::get(get_const_fp_value(value1) * get_const_fp_value(value2), module_);
    case Instruction::fdiv:
        return ConstantFP::get(get_const_fp_value(value1) / get_const_fp_value(value2), module_);

    case Instruction::cmp:
        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op())
        {
        case CmpInst::EQ:
            return ConstantInt::get(get_const_int_value(value1) == get_const_int_value(value2), module_);
        case CmpInst::NE:
            return ConstantInt::get(get_const_int_value(value1) != get_const_int_value(value2), module_);
        case CmpInst::GT:
            return ConstantInt::get(get_const_int_value(value1) > get_const_int_value(value2), module_);
        case CmpInst::GE:
            return ConstantInt::get(get_const_int_value(value1) >= get_const_int_value(value2), module_);
        case CmpInst::LT:
            return ConstantInt::get(get_const_int_value(value1) < get_const_int_value(value2), module_);
        case CmpInst::LE:
            return ConstantInt::get(get_const_int_value(value1) <= get_const_int_value(value2), module_);
        }
    case Instruction::fcmp:
        switch (dynamic_cast<FCmpInst *>(instr)->get_cmp_op())
        {
        case FCmpInst::EQ:
            return ConstantInt::get(get_const_fp_value(value1) == get_const_fp_value(value2), module_);
        case FCmpInst::NE:
            return ConstantInt::get(get_const_fp_value(value1) != get_const_fp_value(value2), module_);
        case FCmpInst::GT:
            return ConstantInt::get(get_const_fp_value(value1) > get_const_fp_value(value2), module_);
        case FCmpInst::GE:
            return ConstantInt::get(get_const_fp_value(value1) >= get_const_fp_value(value2), module_);
        case FCmpInst::LT:
            return ConstantInt::get(get_const_fp_value(value1) < get_const_fp_value(value2), module_);
        case FCmpInst::LE:
            return ConstantInt::get(get_const_fp_value(value1) <= get_const_fp_value(value2), module_);
        }
    default:
        return nullptr;
    }
}

Constant *ConstFolder::compute(Instruction *instr, Constant *value1)
{
    auto op = instr->get_instr_type();
    switch (op)
    {
    case Instruction::sitofp:
        return ConstantFP::get((float)get_const_int_value(value1), module_);
    case Instruction::fptosi:
        return ConstantInt::get((int)get_const_fp_value(value1), module_);
    case Instruction::zext:
        return ConstantInt::get((int)get_const_int_value(value1), module_);
    default:
        return nullptr;
    }
}

namespace utils
{
    static std::string print_congruence_class(const CongruenceClass &cc)
    {
        std::stringstream ss;
        if (cc.index_ == 0)
        {
            ss << "top class\n";
            return ss.str();
        }
        ss << "\nindex: " << cc.index_ << "\nleader: " << cc.leader_->print()
           << "\nvalue phi: " << (cc.value_phi_ ? cc.value_phi_->print() : "nullptr"s)
           << "\nvalue expr: " << (cc.value_expr_ ? cc.value_expr_->print() : "nullptr"s) << "\nmembers: {";
        for (auto &member : cc.members_)
            ss << member->print() << "; ";
        ss << "}\n";
        return ss.str();
    }

    static std::string dump_cc_json(const CongruenceClass &cc)
    {
        std::string json;
        json += "[";
        for (auto member : cc.members_)
        {
            if (dynamic_cast<Constant *>(member) != nullptr)
                json += member->print() + ", ";
            else
                json += "\"%" + member->get_name() + "\", ";
        }
        json += "]";
        return json;
    }

    static std::string dump_partition_json(const GVN::partitions &p)
    {
        std::string json;
        json += "[";
        for (auto cc : p)
            json += dump_cc_json(*cc) + ", ";
        json += "]";
        return json;
    }

    static std::string dump_bb2partition(const std::map<BasicBlock *, GVN::partitions> &map)
    {
        std::string json;
        json += "{";
        for (auto [bb, p] : map)
            json += "\"" + bb->get_name() + "\": " + dump_partition_json(p) + ",";
        json += "}";
        return json;
    }

    // logging utility for you
    static void print_partitions(const GVN::partitions &p)
    {
        if (p.empty())
        {
            LOG_DEBUG << "empty partitions\n";
            return;
        }
        std::string log;
        for (auto &cc : p)
            log += print_congruence_class(*cc);
        LOG_DEBUG << log; // please don't use std::cout
    }
} // namespace utils

GVN::partitions GVN::join(const partitions &P1, const partitions &P2)
{
    // TODO: do intersection pair-wise
    partitions P = {};
    if (P1.size() == 1 && (*(P1.begin()))->index_ == 0)
        return clone(P2);
    if (P2.size() == 1 && (*(P2.begin()))->index_ == 0)
        return clone(P1);
    for (auto Ci : P1)
    {
        for (auto Cj : P2)
        {
            if (Ci->index_ == 0 || Cj->index_ == 0)
                continue;
            auto CK = intersect(Ci, Cj);
            if (CK != nullptr)
                P.insert(CK);
        }
    }
    return P;
}

std::shared_ptr<CongruenceClass> GVN::intersect(std::shared_ptr<CongruenceClass> Ci,
                                                std::shared_ptr<CongruenceClass> Cj)
{
    // TODO
    std::shared_ptr<CongruenceClass> Ck = createCongruenceClass();
    if (Ci->index_ == Cj->index_)
        Ck->index_ = Ci->index_;
    if (Ci->value_expr_ == Cj->value_expr_)
        Ck->value_expr_ = Ci->value_expr_;
    if (Ci->value_phi_ == Cj->value_phi_)
        Ck->value_phi_ = Ci->value_phi_;

    for (auto i : Ci->members_)
    {
        for (auto j : Cj->members_)
        {
            if (i == j)
            {
                Ck->members_.insert(i);
                if (Ck->members_.size() == 1)
                    Ck->leader_ = i;
            }
        }
    }
    if (Ck->value_expr_ && Ci->value_expr_ == Cj->value_expr_)
        Ck->index_ = Ci->index_;

    if (Ck->value_phi_ && Ci->value_phi_ == Cj->value_phi_)
        Ck->index_ = Ci->index_;

    if (Ck->index_ == 0 && Ck->members_.size() != 0)
    {
        Ck->index_ = next_value_number_++;
        if (Ci->value_expr_ == Cj->value_expr_)
        {
            Ck->value_expr_ = Ci->value_expr_;
        }
        else
        {
            Ck->value_expr_ = PhiExpression::create(Ci->value_expr_, Cj->value_expr_);
            Ck->value_phi_ = PhiExpression::create(Ci->value_expr_, Cj->value_expr_);
        }
    }

    if (Ck->index_ != 0 && Ck->members_.size() != 0)
        return Ck;
    else
        return nullptr;
}

void GVN::detectEquivalences()
{
    bool changed = false;
    // initialize pout with top
    // iterate until converge
    auto entry = func_->get_entry_block();
    pin_[entry] = {};
    auto p = clone(pin_[entry]);
    auto &args_var = func_->get_args();
    for (auto ii : args_var)
    {
        auto cc = createCongruenceClass(next_value_number_++);
        cc->leader_ = ii;
        cc->members_ = {ii};
        cc->value_expr_ = VarExpression::create(ii->get_name());
        p.insert(cc);
    }
    auto &globalvar = m_->get_global_variable();
    for (auto &ii : globalvar)
    {
        auto cc = createCongruenceClass(next_value_number_++);
        cc->leader_ = &ii;
        cc->members_ = {&ii};
        cc->value_expr_ = VarExpression::create(ii.get_name());
        p.insert(cc);
    }

    for (auto &instr : entry->get_instructions())
        if (!instr.is_void())
            p = transferFunction(&instr, &instr, p);

    auto bb_ne = entry->get_succ_basic_blocks();
    if (bb_ne.size() != 0)
    {
        for (auto b : bb_ne)
        {
            for (auto &instr : b->get_instructions())
            {
                if (!instr.is_phi())
                    continue;
                for (auto cc : p)
                {
                    for (auto vv : cc->members_)
                    {
                        if (vv->get_name() == instr.get_name())
                        {
                            cc->members_.erase(vv);
                            break;
                        }
                    }
                    if (cc->members_.size() == 0)
                    {
                        p.erase(cc);
                        break;
                    }
                }
                Value *this_op;
                if (instr.get_operand(1)->get_name() == entry->get_name())
                    this_op = instr.get_operand(0);
                else
                    this_op = instr.get_operand(2);

                int is_in = 0;
                for (auto cc : p)
                    for (auto vv : cc->members_)
                        if (vv->get_name() == this_op->get_name())
                        {
                            cc->members_.insert(dynamic_cast<Value *>(&instr));
                            is_in = 1;
                        }

                if (is_in == 0)
                {
                    if (!dynamic_cast<Constant *>(this_op))
                    {
                        auto cc = createCongruenceClass(next_value_number_++);
                        cc->leader_ = &instr;
                        cc->members_ = {&instr};
                        cc->value_expr_ = VarExpression::create(this_op->get_name());
                        p.insert(cc);
                    }
                    else
                    {
                        auto constvalue = ConstantExpression::create(dynamic_cast<Constant *>(this_op));
                        for (auto cc : p)
                        {
                            // if (cc->value_expr_ && constvalue == cc->value_expr_)
                            if (cc->value_expr_ && cc->value_expr_->get_expr_type() == Expression::e_constant &&
                                cc->value_expr_->print() == constvalue->print())
                            {
                                cc->members_.insert(&instr);
                                is_in = 1;
                            }
                        }
                        if (is_in == 0)
                        {
                            auto cc = createCongruenceClass(next_value_number_++);
                            cc->leader_ = dynamic_cast<Constant *>(this_op);
                            cc->members_ = {&instr};
                            cc->value_expr_ = constvalue;
                            p.insert(cc);
                        }
                    }
                }
            }
        }
    }
    pout_[entry] = std::move(p);
    for (auto &bb : func_->get_basic_blocks())
    {
        if (&bb == entry)
            continue;
        partitions top;
        top.insert(createCongruenceClass());
        pout_[&bb] = top;
    }
    do
    {
        changed = false;
        // see the pseudo code in documentation
        for (auto &bb : func_->get_basic_blocks())
        {
            now_bb = &bb;
            if (&bb == entry)
                continue;
            if (bb.get_pre_basic_blocks().size() == 0)
                continue;
            // you might need to visit the blocks in depth-first order
            // get PIN of bb by predecessor(s)
            // iterate through all instructions in the block
            // and the phi instruction in all the successors
            // check changes in pout
            else if (bb.get_pre_basic_blocks().size() >= 2)
            {
                auto bb1 = bb.get_pre_basic_blocks().front();
                auto bb2 = bb.get_pre_basic_blocks().back();
                pin_[&bb] = join(pout_[bb1], pout_[bb2]);
            }
            else
            {
                auto bb1 = bb.get_pre_basic_blocks().front();
                pin_[&bb] = clone(pout_[bb1]);
            }

            auto p = clone(pin_[&bb]);
            for (auto &instr : bb.get_instructions())
                if (!instr.is_void())
                    p = transferFunction(&instr, &instr, p);
            // copy statement
            auto bb_ne = bb.get_succ_basic_blocks();

            if (bb_ne.size() != 0)
            {
                for (auto b : bb_ne)
                {
                    for (auto &instr : b->get_instructions())
                    {
                        if (!instr.is_phi())
                            continue;
                        for (auto cc : p)
                        {
                            for (auto vv : cc->members_)
                            {
                                if (vv->get_name() == instr.get_name())
                                {
                                    cc->members_.erase(vv);
                                    break;
                                }
                            }
                            if (cc->members_.size() == 0)
                            {
                                p.erase(cc);
                                break;
                            }
                        }
                        Value *this_op;
                        if (instr.get_operand(1)->get_name() == bb.get_name())
                            this_op = instr.get_operand(0);
                        else
                            this_op = instr.get_operand(2);

                        int is_in = 0;
                        for (auto cc : p)
                            for (auto vv : cc->members_)
                                if (vv->get_name() == this_op->get_name())
                                {
                                    cc->members_.insert(dynamic_cast<Value *>(&instr));
                                    is_in = 1;
                                }

                        if (is_in == 0)
                        {
                            if (!dynamic_cast<Constant *>(this_op))
                            {
                                auto cc = createCongruenceClass(next_value_number_++);
                                cc->leader_ = &instr;
                                cc->members_ = {&instr};
                                cc->value_expr_ = VarExpression::create(this_op->get_name());
                                p.insert(cc);
                            }
                            else
                            {
                                auto constvalue = ConstantExpression::create(dynamic_cast<Constant *>(this_op));
                                for (auto cc : p)
                                {
                                    // if (cc->value_expr_ && constvalue == cc->value_expr_)
                                    if (cc->value_expr_ && cc->value_expr_->get_expr_type() == Expression::e_constant &&
                                        cc->value_expr_->print() == constvalue->print())
                                    {
                                        cc->members_.insert(&instr);
                                        is_in = 1;
                                    }
                                }
                                if (is_in == 0)
                                {
                                    auto cc = createCongruenceClass(next_value_number_++);
                                    cc->leader_ = dynamic_cast<Constant *>(this_op);
                                    cc->members_ = {&instr};
                                    cc->value_expr_ = constvalue;
                                    p.insert(cc);
                                }
                            }
                        }
                    }
                }
            }
            utils::print_partitions(p);
            if (p != pout_[&bb])
                changed = true;
            pout_[&bb] = std::move(p);

            LOG_DEBUG << "\n\n\n\n\n";
        }
        LOG_DEBUG << "\n\n\n\n\nnew turn\n\n\n\n\n";
    } while (changed);
}

shared_ptr<Expression> GVN::valueExpr(Instruction *instr, partitions pin)
{
    // TODO
    if (instr->isBinary())
    {
        auto op = instr->get_operands();
        auto op1 = op[0];
        auto op2 = op[1];
        std::shared_ptr<Expression> ep1, ep2;

        if (dynamic_cast<Constant *>(op1))
            ep1 = ConstantExpression::create(dynamic_cast<Constant *>(op1));
        else
        {
            ep1 = VarExpression::create(op1->get_name());
            for (auto cc : pin)
                for (auto vv : cc->members_)
                    if (op1->get_name() == vv->get_name())
                    {
                        if (dynamic_cast<Constant *>(cc->leader_))
                            op1 = dynamic_cast<Constant *>(cc->leader_);
                        if (cc->value_phi_ != nullptr)
                            ep1 = cc->value_phi_;
                        if (cc->value_expr_ != nullptr)
                            ep1 = cc->value_expr_;
                    }
        }

        if (dynamic_cast<Constant *>(op2))
            ep2 = ConstantExpression::create(dynamic_cast<Constant *>(op2));
        else
        {
            ep2 = VarExpression::create(op2->get_name());
            for (auto cc : pin)
                for (auto vv : cc->members_)
                    if (op2->get_name() == vv->get_name())
                    {
                        if (dynamic_cast<Constant *>(cc->leader_))
                            op2 = dynamic_cast<Constant *>(cc->leader_);
                        if (cc->value_phi_ != nullptr)
                            ep2 = cc->value_phi_;
                        if (cc->value_expr_ != nullptr)
                            ep2 = cc->value_expr_;
                    }
        }
        if (dynamic_cast<Constant *>(op1) && dynamic_cast<Constant *>(op2))
        {
            ConstFolder tmp(m_);
            auto tt = tmp.compute(instr, dynamic_cast<Constant *>(op1), dynamic_cast<Constant *>(op2));
            return ConstantExpression::create(tt);
        }
        else
            return BinaryExpression::create(instr->get_instr_type(), ep1, ep2);
    }
    else if (instr->is_cmp())
    {
        auto op = instr->get_operands();
        auto op1 = op[0];
        auto op2 = op[1];
        std::shared_ptr<Expression> ep1, ep2;
        auto op_id = dynamic_cast<CmpInst *>(instr)->get_cmp_op();
        if (dynamic_cast<Constant *>(op1))
            ep1 = ConstantExpression::create(dynamic_cast<Constant *>(op1));
        else
        {
            ep1 = VarExpression::create(op1->get_name());
            for (auto cc : pin)
                for (auto vv : cc->members_)
                    if (op1->get_name() == vv->get_name())
                    {
                        if (dynamic_cast<Constant *>(cc->leader_))
                            op1 = dynamic_cast<Constant *>(cc->leader_);
                        if (cc->value_phi_ != nullptr)
                            ep1 = cc->value_phi_;
                        if (cc->value_expr_ != nullptr)
                            ep1 = cc->value_expr_;
                    }
        }

        if (dynamic_cast<Constant *>(op2))
            ep2 = ConstantExpression::create(dynamic_cast<Constant *>(op2));
        else
        {
            ep2 = VarExpression::create(op2->get_name());
            for (auto cc : pin)
                for (auto vv : cc->members_)
                    if (op2->get_name() == vv->get_name())
                    {
                        if (dynamic_cast<Constant *>(cc->leader_))
                            op2 = dynamic_cast<Constant *>(cc->leader_);
                        if (cc->value_phi_ != nullptr)
                            ep2 = cc->value_phi_;
                        if (cc->value_expr_ != nullptr)
                            ep2 = cc->value_expr_;
                    }
        }
        if (dynamic_cast<Constant *>(op1) && dynamic_cast<Constant *>(op2))
        {
            ConstFolder tmp(m_);
            auto tt = tmp.compute(instr, dynamic_cast<Constant *>(op1), dynamic_cast<Constant *>(op2));
            return ConstantExpression::create(tt);
        }
        else
            return CmpExpression::create(op_id, ep1, ep2);
    }
    else if (instr->is_fcmp())
    {
        auto op = instr->get_operands();
        auto op1 = op[0];
        auto op2 = op[1];
        std::shared_ptr<Expression> ep1, ep2;
        auto op_id = dynamic_cast<FCmpInst *>(instr)->get_cmp_op();
        if (dynamic_cast<Constant *>(op1))
            ep1 = ConstantExpression::create(dynamic_cast<Constant *>(op1));
        else
        {
            ep1 = VarExpression::create(op1->get_name());
            for (auto cc : pin)
                for (auto vv : cc->members_)
                    if (op1->get_name() == vv->get_name())
                    {
                        if (dynamic_cast<Constant *>(cc->leader_))
                            op1 = dynamic_cast<Constant *>(cc->leader_);
                        if (cc->value_phi_ != nullptr)
                            ep1 = cc->value_phi_;
                        if (cc->value_expr_ != nullptr)
                            ep1 = cc->value_expr_;
                    }
        }

        if (dynamic_cast<Constant *>(op2))
            ep2 = ConstantExpression::create(dynamic_cast<Constant *>(op2));
        else
        {
            ep2 = VarExpression::create(op2->get_name());
            for (auto cc : pin)
                for (auto vv : cc->members_)
                    if (op2->get_name() == vv->get_name())
                    {
                        if (dynamic_cast<Constant *>(cc->leader_))
                            op2 = dynamic_cast<Constant *>(cc->leader_);
                        if (cc->value_phi_ != nullptr)
                            ep2 = cc->value_phi_;
                        if (cc->value_expr_ != nullptr)
                            ep2 = cc->value_expr_;
                    }
        }
        if (dynamic_cast<Constant *>(op1) && dynamic_cast<Constant *>(op2))
        {
            ConstFolder tmp(m_);
            auto tt = tmp.compute(instr, dynamic_cast<Constant *>(op1), dynamic_cast<Constant *>(op2));
            return ConstantExpression::create(tt);
        }
        else
            return FCmpExpression::create(op_id, ep1, ep2);
    }
    else if (instr->is_call())
    {
        // auto call_inst = dynamic_cast<CallInst *>(instr);
        auto op = instr->get_operands();
        auto thisfunction = dynamic_cast<Function *>(op[0]);
        if (!func_info_->is_pure_function(thisfunction))
            return VarExpression::create(instr->get_name());
        else
        {
            std::vector<std::shared_ptr<Expression>> args;
            for (auto it : op)
            {
                if (it == op[0])
                    continue;
                std::shared_ptr<Expression> ep;
                if (dynamic_cast<Constant *>(it))
                    ep = ConstantExpression::create(dynamic_cast<Constant *>(it));
                else
                {
                    ep = VarExpression::create(it->get_name());
                    for (auto cc : pin)
                        for (auto vv : cc->members_)
                            if (it->get_name() == vv->get_name())
                            {
                                if (dynamic_cast<Constant *>(cc->leader_))
                                    it = dynamic_cast<Constant *>(cc->leader_);
                                if (cc->value_phi_ != nullptr)
                                    ep = cc->value_phi_;
                                if (cc->value_expr_ != nullptr)
                                    ep = cc->value_expr_;
                            }
                }
                if (dynamic_cast<Constant *>(it))
                    ep = ConstantExpression::create(dynamic_cast<Constant *>(it));
                args.push_back(ep);
            }
            return CallExpression::create(thisfunction, args);
        }
    }
    else if (instr->is_gep())
    {
        auto op = instr->get_operands();
        auto ist = dynamic_cast<GetElementPtrInst *>(instr);
        std::vector<std::shared_ptr<Expression>> args;
        for (auto it : op)
        {
            std::shared_ptr<Expression> ep;
            if (dynamic_cast<Constant *>(it))
                ep = ConstantExpression::create(dynamic_cast<Constant *>(it));
            else
            {
                ep = VarExpression::create(it->get_name());
                for (auto cc : pin)
                    for (auto vv : cc->members_)
                        if (it->get_name() == vv->get_name())
                        {
                            if (dynamic_cast<Constant *>(cc->leader_))
                                it = dynamic_cast<Constant *>(cc->leader_);
                            if (cc->value_phi_ != nullptr)
                                ep = cc->value_phi_;
                            if (cc->value_expr_ != nullptr)
                                ep = cc->value_expr_;
                        }
            }
            if (dynamic_cast<Constant *>(it))
                ep = ConstantExpression::create(dynamic_cast<Constant *>(it));
            args.push_back(ep);
        }
        return GepExpression::create(ist->get_element_type(), args);
    }
    else if (instr->is_fp2si() || instr->is_si2fp())
    {
        auto op = instr->get_operand(0);
        std::shared_ptr<Expression> ep;
        ep = VarExpression::create(instr->get_name());
        for (auto cc : pin)
            for (auto vv : cc->members_)
                if (op->get_name() == vv->get_name())
                {
                    if (dynamic_cast<Constant *>(cc->leader_))
                        op = dynamic_cast<Constant *>(cc->leader_);
                    if (cc->value_phi_ != nullptr)
                        ep = cc->value_phi_;
                    if (cc->value_expr_ != nullptr)
                        ep = cc->value_expr_;
                }
        if (dynamic_cast<Constant *>(op))
        {
            ConstFolder tmp(m_);
            return ConstantExpression::create(tmp.compute(instr, dynamic_cast<Constant *>(op)));
        }
        else
            return OnlyExpression::create(instr->get_instr_type(), ep);
    }
    else if (instr->is_zext())
    {
        std::shared_ptr<Expression> ep;
        auto op = instr->get_operand(0);
        ep = VarExpression::create(instr->get_name());
        for (auto cc : pin)
            for (auto vv : cc->members_)
                if (op->get_name() == vv->get_name())
                {
                    if (cc->value_phi_ != nullptr)
                        ep = cc->value_phi_;
                    if (cc->value_expr_ != nullptr)
                        ep = cc->value_expr_;
                }
        return OnlyExpression::create(instr->get_instr_type(), ep);
    }
    else if (!instr->is_phi() && !instr->is_void())
        return VarExpression::create(instr->get_name());
    return nullptr;
}

// instruction of the form `x = e`, mostly x is just e (SSA), but for copy stmt x is a phi instruction in the
// successor. Phi values (not copy stmt) should be handled in detectEquiv
/// \param bb basic block in which the transfer function is called
GVN::partitions GVN::transferFunction(Instruction *x, Value *e, partitions pin)
{
    partitions pout = clone(pin);
    // TODO: get different ValueExpr by Instruction::OpID, modify pout
    if (!x->is_phi())
        for (auto p : pout)
        {
            for (auto &ii : p->members_)
            {
                if (x == ii)
                {
                    p->members_.erase(ii);
                    break;
                }
            }
        }
    auto ve = valueExpr(dynamic_cast<Instruction *>(x), pin);
    auto vpf = valuePhiFunc(ve, pin);
    if (vpf && vpf->get_lhs() == vpf->get_rhs())
    {
        ve = vpf->get_lhs();
        vpf = nullptr;
    }
    bool flag = true;
    if (ve != nullptr && vpf == nullptr)
    {
        for (auto it = pout.begin(); it != pout.end(); it++)
            if ((*it)->value_expr_ and *(*it)->value_expr_ == *ve)
            {
                flag = false;
                (*it)->members_.insert(x);
            }
    }
    else if (vpf != nullptr)
    {
        for (auto it = pout.begin(); it != pout.end(); it++)
            if ((*it)->value_phi_ and *(*it)->value_phi_ == *vpf)
            {
                flag = false;
                (*it)->members_.insert(x);
            }
    }

    if (flag && !x->is_phi())
    {
        auto cc = createCongruenceClass(next_value_number_++);
        if (ve && ve->get_expr_type() == Expression::e_constant)
            cc->leader_ = std::dynamic_pointer_cast<ConstantExpression>(ve)->get_const();
        else
            cc->leader_ = x;
        cc->members_ = {x};
        if (cc->value_phi_)
            cc->value_expr_ = vpf;
        else
            cc->value_expr_ = ve;
        cc->value_phi_ = vpf;
        pout.insert(cc);
    }
    return pout;
}

shared_ptr<PhiExpression> GVN::valuePhiFunc(shared_ptr<Expression> ve, const partitions &P)
{
    // TODO
    if (ve == nullptr || P.empty())
        return nullptr;

    BasicBlock *bb = now_bb;

    std::shared_ptr<Expression> vi, vj;
    if (ve->get_expr_type() == Expression::e_bin)
    {
        std::shared_ptr<Expression> lhs = ve->get_lhs();
        std::shared_ptr<Expression> rhs = ve->get_rhs();
        if (lhs && rhs && lhs->get_expr_type() == Expression::e_phi && rhs->get_expr_type() == Expression::e_phi)
        {
            std::shared_ptr<Expression> v1, v2;
            if (lhs->get_lhs()->get_expr_type() == Expression::e_constant && rhs->get_lhs()->get_expr_type() == Expression::e_constant)
            {
                ConstFolder tmp(m_);
                std::shared_ptr<ConstantExpression> l = std::dynamic_pointer_cast<ConstantExpression>(lhs->get_lhs());
                std::shared_ptr<ConstantExpression> r = std::dynamic_pointer_cast<ConstantExpression>(rhs->get_lhs());
                auto tt = tmp.compute(ve->get_op(), l->get_const(), r->get_const());
                v1 = ConstantExpression::create(tt);
            }
            else
                v1 = BinaryExpression::create(ve->get_op(), lhs->get_lhs(), rhs->get_lhs());
            if (lhs->get_rhs()->get_expr_type() == Expression::e_constant && rhs->get_rhs()->get_expr_type() == Expression::e_constant)
            {
                ConstFolder tmp(m_);
                std::shared_ptr<ConstantExpression> l = std::dynamic_pointer_cast<ConstantExpression>(lhs->get_rhs());
                std::shared_ptr<ConstantExpression> r = std::dynamic_pointer_cast<ConstantExpression>(rhs->get_rhs());
                auto tt = tmp.compute(ve->get_op(), l->get_const(), r->get_const());
                v2 = ConstantExpression::create(tt);
            }
            else
                v2 = BinaryExpression::create(ve->get_op(), lhs->get_rhs(), rhs->get_rhs());

            vi = getVN(pout_[bb->get_pre_basic_blocks().front()], v1);
            if (vi == nullptr)
            {
                now_bb = bb->get_pre_basic_blocks().front();
                vi = valuePhiFunc(v1, pout_[bb->get_pre_basic_blocks().front()]);
                now_bb = bb;
            }
            vj = getVN(pout_[bb->get_pre_basic_blocks().back()], v2);
            if (vj == nullptr)
            {
                now_bb = bb->get_pre_basic_blocks().back();
                vj = valuePhiFunc(v2, pout_[bb->get_pre_basic_blocks().back()]);
                now_bb = bb;
            }
        }
    }
    if (vi && vj)
        return PhiExpression::create(vi, vj);
    else
        return nullptr;
}

shared_ptr<Expression> GVN::getVN(const partitions &pout, shared_ptr<Expression> ve)
{
    // TODO: return what?
    for (auto it = pout.begin(); it != pout.end(); it++)
        if ((*it)->value_expr_ and *(*it)->value_expr_ == *ve)
            return {(*it)->value_expr_};
    return nullptr;
}

void GVN::initPerFunction()
{
    next_value_number_ = 1;
    pin_.clear();
    pout_.clear();
}

void GVN::replace_cc_members()
{
    for (auto &[_bb, part] : pout_)
    {
        auto bb = _bb; // workaround: structured bindings can't be captured in C++17
        for (auto &cc : part)
        {
            if (cc->index_ == 0)
                continue;
            // if you are planning to do constant propagation, leaders should be set to constant at some point
            for (auto &member : cc->members_)
            {
                bool member_is_phi = dynamic_cast<PhiInst *>(member);
                bool value_phi = cc->value_phi_ != nullptr;
                if (member != cc->leader_ and (value_phi or !member_is_phi))
                {
                    // only replace the members if users are in the same block as bb
                    member->replace_use_with_when(cc->leader_, [bb](User *user)
                                                  {
                        if (auto instr = dynamic_cast<Instruction *>(user)) {
                            auto parent = instr->get_parent();
                            auto &bb_pre = parent->get_pre_basic_blocks();
                            if (instr->is_phi()) // as copy stmt, the phi belongs to this block
                                return std::find(bb_pre.begin(), bb_pre.end(), bb) != bb_pre.end();
                            else
                                return parent == bb;
                        }
                        return false; });
                }
            }
        }
    }
    return;
}

// top-level function, done for you
void GVN::run()
{
    std::ofstream gvn_json;
    if (dump_json_)
    {
        gvn_json.open("gvn.json", std::ios::out);
        gvn_json << "[";
    }
    m_->set_print_name();
    folder_ = std::make_unique<ConstFolder>(m_);
    func_info_ = std::make_unique<FuncInfo>(m_);
    func_info_->run();
    dce_ = std::make_unique<DeadCode>(m_);
    dce_->run(); // let dce take care of some dead phis with undef

    for (auto &f : m_->get_functions())
    {
        if (f.get_basic_blocks().empty())
            continue;
        func_ = &f;
        initPerFunction();
        LOG_INFO << "Processing " << f.get_name();
        detectEquivalences();
        LOG_INFO << "===============pin=========================\n";
        for (auto &[bb, part] : pin_)
        {
            LOG_INFO << "\n===============bb: " << bb->get_name() << "=========================\npartitionIn: ";
            for (auto &cc : part)
                LOG_INFO << utils::print_congruence_class(*cc);
        }
        LOG_INFO << "\n===============pout=========================\n";
        for (auto &[bb, part] : pout_)
        {
            LOG_INFO << "\n=====bb: " << bb->get_name() << "=====\npartitionOut: ";
            for (auto &cc : part)
                LOG_INFO << utils::print_congruence_class(*cc);
        }
        if (dump_json_)
        {
            gvn_json << "{\n\"function\": ";
            gvn_json << "\"" << f.get_name() << "\", ";
            gvn_json << "\n\"pout\": " << utils::dump_bb2partition(pout_);
            gvn_json << "},";
        }
        replace_cc_members(); // don't delete instructions, just replace them
    }
    dce_->run(); // let dce do that for us
    if (dump_json_)
        gvn_json << "]";
}

template <typename T>
static bool equiv_as(const Expression &lhs, const Expression &rhs)
{
    // we use static_cast because we are very sure that both operands are actually T, not other types.
    return static_cast<const T *>(&lhs)->equiv(static_cast<const T *>(&rhs));
}

bool GVNExpression::operator==(const Expression &lhs, const Expression &rhs)
{
    if (lhs.get_expr_type() != rhs.get_expr_type())
        return false;
    switch (lhs.get_expr_type())
    {
    case Expression::e_constant:
        return equiv_as<ConstantExpression>(lhs, rhs);
    case Expression::e_bin:
        return equiv_as<BinaryExpression>(lhs, rhs);
    case Expression::e_phi:
        return equiv_as<PhiExpression>(lhs, rhs);
    case Expression::e_var:
        return equiv_as<VarExpression>(lhs, rhs);
    case Expression::e_cmp:
        return equiv_as<CmpExpression>(lhs, rhs);
    case Expression::e_fcmp:
        return equiv_as<FCmpExpression>(lhs, rhs);
    case Expression::e_call:
        return equiv_as<CallExpression>(lhs, rhs);
    case Expression::e_gep:
        return equiv_as<GepExpression>(lhs, rhs);
    case Expression::e_only:
        return equiv_as<OnlyExpression>(lhs, rhs);
    }
    return false;
}

bool GVNExpression::operator==(const shared_ptr<Expression> &lhs, const shared_ptr<Expression> &rhs)
{
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}

GVN::partitions GVN::clone(const partitions &p)
{
    partitions data;
    for (auto &cc : p)
    {
        data.insert(std::make_shared<CongruenceClass>(*cc));
    }
    return data;
}

bool operator==(const GVN::partitions &p1, const GVN::partitions &p2)
{
    // TODO: how to compare partitions?
    if (p1.size() != p2.size())
        return false;
    for (auto iter1 = p1.begin(), iter2 = p2.begin(); iter1 != p1.end(); iter1++, iter2++)
    {
        if (!(*(*iter1).get() == *(*iter2).get()))
            return false;
    }
    return true;
}

bool CongruenceClass::operator==(const CongruenceClass &other) const
{
    // TODO: which fields need to be compared?
    // if (!(this->value_expr_ == other.value_expr_))
    //     return false;
    // if (!(this->value_phi_ == other.value_phi_))
    //     return false;
    auto p1 = this->members_;
    auto p2 = other.members_;
    if (p1.size() != p2.size())
        return false;
    for (auto iter1 = p1.begin(), iter2 = p2.begin(); iter1 != p1.end(); iter1++, iter2++)
    {
        if (*iter1 != *iter2)
            return false;
    }
    return true;
}
