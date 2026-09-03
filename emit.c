// emits b target
u32 emit_b(u32 here, u32 target)
{
    i32 offset = (i32)(target - (here + 8));
    u32 imm24 = (u32)(offset >> 2) & 0x00FFFFFF;
    return 0xEA000000 | imm24;
}

// emits CMP reg, c
u32 emit_cmp(u8 reg, u8 c)
{
    return 0xE3500000 | ((u32)(reg & 0xF) << 16) | c;
}

// emits bl target
u32 emit_bl(u32 here, u32 target)
{
    i32 offset = (i32)(target - (here + 8));
    u32 imm24 = (u32)(offset >> 2) & 0x00FFFFFF;
    return 0xEB000000 | imm24;
}
