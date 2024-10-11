#ifndef MYGL_NSTRUCTIONSIMPLIFY_H
#define MYGL_INSTRUCTIONSIMPLIFY_H

#include "mygl-ir/ir-basic-value.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-define.hxx"

namespace MYGL:: Optimizers {

    using namespace MYGL::IRBase;
    using opCode = MYGL::IRBase::Instruction::OpCode;

    Value *SimplifyAddInstruction(Value *lhs, Value *rhs);

    Value *SimplifySubInst(Value *lhs, Value *rhs);

    Value *SimplifyMulInst(Value *lhs, Value *rhs);

    Value *SimplifySDivInst(Value *lhs, Value *rhs);

    Value *SimplifyShlInst(Value *lhs, Value *rhs);

    Value *SimplifyShrInst(Value *lhs, Value *rhs);

    Value *SimplifyAndInst(Value *lhs, Value *rhs);

    Value *SimplifyOrInst(Value *lhs, Value *rhs);

    Value *SimplifyXorInst(Value *lhs, Value *rhs);

    Value *SimplifyBinOp(Value *lhs, Value *rhs);
    
    Value *SimplifyInstruction(Value *lhs, Value *rhs);

    IR::Constant *foldOrSwapConstant(opCode,Value *lhs, Value *rhs);

    //匹配算式 是否是Y op X。如果传参时Y为空指针，则匹配后将另一个操作数的值传给Y
    Value *matchBalenceOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::self_t opcode); //对称的算式如加法乘法逻辑算式
    Value *matchBalanceOprand( Value *X, Value *oprand, Instruction::OpCode::self_t opcode);
    Value *matchUnBalenceOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::self_t opcode); //不对称的算式如减法除法逻辑算式

    Value *m_Sub(Value *Y, Value *X, Value *oprand){
        return matchUnOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::SUB);
    }
    Value *m_Add(Value *Y, Value *X, Value *oprand){
        return matchBalenceOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::ADD);
    }
    Value *m_Mul(Value *Y, Value *X, Value *oprand){
        return matchBalenceOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::MUL);
    }
    Value *m_Div(Value *Y, Value *X, Value *oprand){
        return matchUnBalenceOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::IDIV);
    }
    Value *m_And(Value *Y, Value *X, Value *oprand){
        return matchBalenceOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::AND);
    }
    Value *m_Or(Value *Y, Value *X, Value *oprand){
        return matchBalenceOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::OR);
    }
    Value *m_Not(Value *X, Value *oprand){
        return matchBalanceOprand( Value *X, Value *oprand, Instruction::OpCode::NOT);
    }
    Value *m_Neg(Value *X, Value *oprand){
        return matchBalanceOprand( Value *X, Value *oprand, Instruction::OpCode::FNEG);
    }



}


#endif