#include "injector.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

uint32_t parseCommand(uint32_t command, uint32_t from, uint32_t to)
{
    uint32_t mask = ((1 << (to - from + 1)) - 1) << from;
    return (command & mask) >> from;
}

void* _GetGP()
{
    void* gp;
    asm(
        "move %0, $gp\n"
        : "=r"(gp)
    );
    return gp;
}

uintptr_t adjustAddress(uintptr_t addr)
{
    return addr;
}

void WriteMemoryRaw(uintptr_t addr, void* value, size_t size)
{
    memcpy((void*)adjustAddress(addr), value, size);
}

void ReadMemoryRaw(uintptr_t addr, void* ret, size_t size)
{
    memcpy((void*)ret, (void*)adjustAddress(addr), size);
}

void MemoryFill(uintptr_t addr, uint8_t value, size_t size)
{
    unsigned char* p = (unsigned char*)adjustAddress(addr);
    while (size--)
    {
        *p++ = (unsigned char)value;
    }
}

void WriteInstr(uintptr_t addr, uint32_t value)
{
    *(uint32_t*)(adjustAddress(addr)) = value;
}

void WriteMemory8(uintptr_t addr, uint8_t value)
{
    *(uint8_t*)(adjustAddress(addr)) = value;
}

void WriteMemory16(uintptr_t addr, uint16_t value)
{
    *(uint16_t*)(adjustAddress(addr)) = value;
}

void WriteMemory32(uintptr_t addr, uint32_t value)
{
    *(uint32_t*)(adjustAddress(addr)) = value;
}

void WriteMemory64(uintptr_t addr, uint64_t value)
{
    *(uint64_t*)(adjustAddress(addr)) = value;
}

void WriteMemoryFloat(uintptr_t addr, float value)
{
    *(float*)(adjustAddress(addr)) = value;
}

void WriteMemoryDouble(uintptr_t addr, double value)
{
    *(double*)(adjustAddress(addr)) = value;
}

uint8_t ReadMemory8(uintptr_t addr)
{
    return *(uint8_t*)(adjustAddress(addr));
}

uint16_t ReadMemory16(uintptr_t addr)
{
    return *(uint16_t*)(adjustAddress(addr));
}

uint32_t ReadMemory32(uintptr_t addr)
{
    return *(uint32_t*)(adjustAddress(addr));
}

uint64_t ReadMemory64(uintptr_t addr)
{
    return *(uint64_t*)(adjustAddress(addr));
}

float ReadMemoryFloat(uintptr_t addr)
{
    return *(float*)(adjustAddress(addr));
}

double ReadMemoryDouble(uintptr_t addr)
{
    return *(double*)(adjustAddress(addr));
}

uintptr_t GetBranchDestination(uintptr_t at)
{
    static const uint32_t instr_len = 4;

    uint8_t J = parseCommand(j(0x123456), 26, 31);
    uint8_t JAL = parseCommand(jal(0x123456), 26, 31);

    uint32_t bytes = ReadMemory32(at);
    uint8_t instr = parseCommand(bytes, 26, 31);

    if (instr == J || instr == JAL)
        return parseCommand(bytes, 0, 25) * instr_len;
    return 0;
}

uintptr_t MakeJMP(uintptr_t at, uintptr_t dest)
{
    uintptr_t bd = GetBranchDestination(at);
    WriteMemory32(adjustAddress(at), (0x08000000 | ((adjustAddress(dest) & 0x0FFFFFFC) >> 2)));
    return bd;
}

uintptr_t MakeJMPwNOP(uintptr_t at, uintptr_t dest)
{
    uintptr_t bd = GetBranchDestination(at);
    WriteMemory32(adjustAddress(at), (0x08000000 | ((adjustAddress(dest) & 0x0FFFFFFC) >> 2)));
    injector.MakeNOP(adjustAddress(at + 4));
    return bd;
}

uintptr_t MakeJAL(uintptr_t at, uintptr_t dest)
{
    uintptr_t bd = GetBranchDestination(at);
    WriteMemory32(adjustAddress(at), (0x0C000000 | ((adjustAddress(dest)) >> 2)));
    return bd;
}

void MakeNOP(uintptr_t at)
{
    MemoryFill(adjustAddress(at), 0x00, 4);
}

void MakeNOPWithSize(uintptr_t at, size_t size)
{
    MemoryFill(adjustAddress(at), 0x00, size);
}

void MakeRangedNOP(uintptr_t at, uintptr_t until)
{
    return MakeNOPWithSize(adjustAddress(at), (size_t)(until - at));
}

uintptr_t MakeInline(size_t instrCount, uintptr_t at, ...)
{
    uintptr_t functor = MakeCallStub(instrCount * 4 + 8);
    size_t i;

    va_list ap;
    va_start(ap, instrCount);
    for (i = 0; i < instrCount; i++) {
        uint32_t arg = va_arg(ap, uint32_t);
        WriteInstr(functor + i * 4, arg);
    }
    va_end(ap);

    injector.MakeJMP(functor + i * 4, at + 8); // should be +8, cuz next instruction is executed before jump
    i++;
    injector.WriteMemory32(functor + i * 4, injector.ReadMemory32(at + 4)); //so we write it here
    injector.MakeNOP(at + 4); //and nop here, so it won't be executed twice
    injector.MakeJMP(at, functor);
    return functor;
}

uintptr_t MakeCallStub(uintptr_t numInstr) {
    return (uintptr_t)AllocMemBlock(numInstr * sizeof(uintptr_t));
}

void MakeLUIORI(uintptr_t at, RegisterID reg, float imm)
{
    uintptr_t functor = MakeCallStub(4); // lui ori j nop
    injector.WriteMemory32(functor + 0, lui(reg, HIWORD(imm)));
    injector.WriteMemory32(functor + 4, ori(reg, reg, LOWORD(imm)));
    injector.MakeJMP(functor + 8, at + 4); // should be +8 as well, but it won't work, e.g. two lui instructions in a row
    injector.WriteMemory32(functor + 12, injector.ReadMemory32(at + 4));
    injector.MakeNOP(at + 4);
    injector.MakeJMP(at, functor);
}

void MakeLI(uintptr_t at, RegisterID reg, int32_t imm)
{
    uintptr_t functor = MakeCallStub(4); // lui ori j nop
    injector.WriteMemory32(functor + 0, lui(reg, HIWORD(imm)));
    injector.WriteMemory32(functor + 4, ori(reg, reg, LOWORD(imm)));
    injector.MakeJMP(functor + 8, at + 4); // should be +8 as well, but it won't work as well
    injector.MakeNOP(functor + 12);
    injector.MakeJMP(at, functor);
}

uint32_t isDelaySlotNearby(uintptr_t at)
{
    static const uint32_t instr_len = 4;

    uint8_t J = parseCommand(j(0x123456), 26, 31);
    uint8_t JAL = parseCommand(jal(0x123456), 26, 31);
    uint8_t B = parseCommand(b(10), 26, 31);
    uint8_t BEQ = parseCommand(beq(v0, v1, 1), 26, 31);
    uint8_t BNE = parseCommand(bne(v0, v1, 1), 26, 31);
    uint8_t BC1F = parseCommand(bc1f(10), 26, 31);
    uint8_t BC1FL = parseCommand(bc1fl(10), 26, 31);
    uint8_t BC1TL = parseCommand(bc1tl(10), 26, 31);

    uint32_t prev_instr = parseCommand(ReadMemory32(at - instr_len), 26, 31);
    uint32_t next_instr = parseCommand(ReadMemory32(at + instr_len), 26, 31);

    if (prev_instr == J || prev_instr == JAL || prev_instr == B || prev_instr == BEQ || prev_instr == BNE || prev_instr == BC1F || prev_instr == BC1FL || prev_instr == BC1TL)
        return 1;
    else if (next_instr == J || next_instr == JAL || next_instr == B || next_instr == BEQ || next_instr == BNE || next_instr == BC1F || next_instr == BC1FL || next_instr == BC1TL)
        return 1;

    return 0;
}

void MakeInlineLUIORI(uintptr_t at, float imm)
{
    static const uint32_t instr_len = 4;

    uint8_t LUI = parseCommand(lui(0, 0), 26, 31);
    uint8_t ORI = parseCommand(ori(0, 0, 0), 26, 31);

    uint32_t bytes = ReadMemory32(at);
    uint8_t instr = parseCommand(bytes, 26, 31);
    uint8_t reg_lui = parseCommand(bytes, 16, 20);

    if (instr == LUI)
    {
        for (uintptr_t i = at + instr_len; i <= (at + instr_len + (5 * instr_len)); i += instr_len)
        {
            bytes = ReadMemory32(i);
            instr = parseCommand(bytes, 26, 31);
            uint8_t reg_ori = parseCommand(bytes, 16, 20);

            if (instr == ORI && reg_lui == reg_ori)
            {
                if (isDelaySlotNearby(i))
                {
                    WriteMemory16(at, HIWORD(imm));
                    WriteMemory16(i, LOWORD(imm));
                    return;
                }
                else
                    return MakeLUIORI(i, reg_ori, imm);
            }
        }
        if (isDelaySlotNearby(at))
            WriteMemory16(at, HIWORD(imm));
        else
            return MakeLUIORI(at, reg_lui, imm);
    }
}

void MakeInlineLI(uintptr_t at, int32_t imm)
{
    static const uint32_t instr_len = 4;

    uint8_t ORI = parseCommand(ori(0, 0, 0), 26, 31);
    uint8_t ZERO = parseCommand(ori(0, 0, 0), 21, 25);

    uint32_t bytes = ReadMemory32(at);
    uint8_t instr = parseCommand(bytes, 26, 31);
    uint8_t reg_ori = parseCommand(bytes, 16, 20);
    uint8_t reg_zero = parseCommand(bytes, 21, 25);

    if (instr == ORI && reg_zero == ZERO)
    {
        if (isDelaySlotNearby(at))
            return WriteMemory16(at, LOWORD(imm));
        else
            return MakeLI(at, reg_ori, imm);
    }
}

struct injector_t injector =
{
    .GetGP = _GetGP,
    .WriteMemoryRaw = WriteMemoryRaw,
    .ReadMemoryRaw = ReadMemoryRaw,
    .MemoryFill = MemoryFill,
    .WriteInstr = WriteInstr,
    .WriteMemory8 = WriteMemory8,
    .WriteMemory16 = WriteMemory16,
    .WriteMemory32 = WriteMemory32,
    .WriteMemory64 = WriteMemory64,
    .WriteMemoryFloat = WriteMemoryFloat,
    .WriteMemoryDouble = WriteMemoryDouble,
    .ReadMemory8 = ReadMemory8,
    .ReadMemory16 = ReadMemory16,
    .ReadMemory32 = ReadMemory32,
    .ReadMemory64 = ReadMemory64,
    .ReadMemoryFloat = ReadMemoryFloat,
    .ReadMemoryDouble = ReadMemoryDouble,
    .GetBranchDestination = GetBranchDestination,
    .MakeJMP = MakeJMP,
    .MakeJMPwNOP = MakeJMPwNOP,
    .MakeJAL = MakeJAL,
    .MakeNOP = MakeNOP,
    .MakeNOPWithSize = MakeNOPWithSize,
    .MakeRangedNOP = MakeRangedNOP,
    .MakeInline = MakeInline,
    .MakeInlineLUIORI = MakeInlineLUIORI,
    .MakeLUIORI = MakeLUIORI,
    .MakeInlineLI = MakeInlineLI
};