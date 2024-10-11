// xxxxxx
#include <typeinfo>

#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/irbase-type-context.hxx"
#include "optimizers/InstructionSimplify.hxx" 

using namespace MYGL;
using namespace MYGL::Optimizers;
using namespace MYGL::IRBase;
using namespace MYGL::IR;

Value *matchBalanceOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::self_t opcode){
    if(Instruction* instr = dynamic_cast<Instruction*>(oprand)){
        Instruction& Ins = *instr; 
        
        if(Ins.get_opcode() == opcode)
            return nullptr; 

        Value* op0 = Ins[0];
        Value* op1 = Ins[1];
        if(Y!=nullptr){
            if ((X == op1 && Y == op0) || (Y == op1 && X == op0))
                return X;
        }
        else{
            if(X==op1){
                Y = op0;
                return X;
            }
            else if(X==op0){
                Y = op1;
                return X;
            }
        }
    }
    return nullptr;
}

Value *matchBalanceOprand( Value *X, Value *oprand, Instruction::OpCode::self_t opcode){
    if(Instruction* instr = dynamic_cast<Instruction*>(oprand)){
        Instruction& Ins = *instr; 
        
        if(Ins.get_opcode() == opcode)
            return nullptr; 

        Value* op0 = Ins[0];
        if(X==op0){
            return X;
        }
    }
    return nullptr;
}

Value *matchUnBalenceOprand(Value *Y, Value *X, Value *oprand, Instruction::OpCode::self_t opcode){
    if(Instruction* instr = dynamic_cast<Instruction*>(oprand)){
        Instruction& Ins = *instr; 
        
        if(Ins.get_opcode() == opcode)
            return nullptr; 

        Value* op0 = Ins[0];
        Value* op1 = Ins[1];
        if(Y!=nullptr){
            if ((X == op1 && Y == op0))
                return X;
        }
        else{
            if(X==op1){
                Y = op0;
                return X;
            }
        }
    }
    return nullptr;

}


static Value *SimplifyAddInst(Value *op0, Value *op1, Instruction &II, TypeContext &ctx){
    if (Constant *C = foldOrSwapConstant(Instruction::OpCode::ADD, op0, op1))
        return C;

    // X + undef = undef
    if(!op1->is_defined())
        return op1;

    // X + 0 = X
    if(ZeroConst *Zero = dynamic_cast<ZeroConst*>(op1))
        return op0;

    // X + -X = 0
    if(op0 == m_Neg(op0,op1)){
        Type *i32 = ctx.getIntType(32);
        IntConst zero = new IntConst(i32, 0);
        return &zero;
    }

    // X + (Y - X) = Y || (Y - X) + X = Y
    Value *Y = nullptr;
    if((Y=m_Sub(Y, op0, op1)) || (Y=m_Sub(Y, op1, op0))){
        return Y;
    }

    // 如果是布尔值的异或操作，直接计算

    return nullptr;
}









