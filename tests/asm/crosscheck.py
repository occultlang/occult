#!/usr/bin/env python3
"""Cross-checks Occult's assembler against GNU as (Intel syntax), byte for byte.
usage: crosscheck.py <asm_dump binary>      (needs `as` and `objcopy`)"""
import itertools, os, subprocess, sys, tempfile

R64 = ["rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"]
R32 = ["eax","ecx","edx","ebx","esp","ebp","esi","edi","r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"]
R16 = ["ax","cx","dx","bx","sp","bp","si","di","r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w"]
R8  = ["al","cl","dl","bl","spl","bpl","sil","dil","r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b"]
MEMS = ["[rax]","[rbx+8]","[rbp]","[rbp-8]","[rsp]","[rsp+16]","[r12]","[r13]","[r13+4]","[rax+rcx*4]","[rbp+rax*8-24]","[r8+r9*2+1000]","[rsp+rdx]",
        "[rax*8+0x100]","[0x1000]","[rip+0x20]","[rdi+0x12345678]","[r15-0x80]","[rax+0x7f]","[rax+0x80]"]
SZ = {8: ("byte", R8), 16: ("word", R16), 32: ("dword", R32), 64: ("qword", R64)}
lines = []
add = lines.append

for op in ["add","or","adc","sbb","and","sub","xor","cmp"]:
    for bits in (8,16,32,64):
        kw, regs = SZ[bits]
        for a, b in [(0,1),(1,0),(3,7),(8,9),(4,5),(15,0),(0,15)]:
            add(f"{op} {regs[a]}, {regs[b]}")
        for m in MEMS:
            add(f"{op} {regs[0]}, {kw} ptr {m}") if False else None
            add(f"{op} {regs[3]}, {m}".replace(f"{m}", f"{kw} ptr {m}"))
            add(f"{op} {kw} ptr {m}, {regs[9]}")
        for imm in (0,1,-1,127,128,-128,-129,0x1000,0x7fffffff if bits>=32 else 0x7fff,-0x1000):
            if bits == 8 and not -128 <= imm <= 255: continue
            if bits == 16 and not -32768 <= imm <= 65535: continue
            for r in (0,1,5,12):
                add(f"{op} {regs[r]}, {imm}")
            add(f"{op} {kw} ptr [rbx+8], {imm}")
            add(f"{op} {kw} ptr [rsp], {imm}")

# mov
for bits in (8,16,32,64):
    kw, regs = SZ[bits]
    for a, b in [(0,1),(5,6),(8,15),(15,8),(4,4)]:
        add(f"mov {regs[a]}, {regs[b]}")
    for m in MEMS:
        add(f"mov {regs[2]}, {kw} ptr {m}")
        add(f"mov {kw} ptr {m}, {regs[10]}")
        add(f"mov {kw} ptr {m}, 5")
    for imm in (0,1,-1,127,255,256,0x7fff,0x12345678,0x7fffffff,-0x80000000):
        if bits == 8 and not -128 <= imm <= 255: continue
        if bits == 16 and not -32768 <= imm <= 65535: continue
        for r in (0,3,9):
            add(f"mov {regs[r]}, {imm}")
for imm in (0x80000000, 0xffffffff, 0x100000000, 0x123456789abcdef0, -0x80000001, 0xffffffffffffffff):
    add(f"mov rax, {imm}"); add(f"mov r11, {imm}")
add("mov qword ptr [rax], -1"); add("mov byte ptr [rax], 0xff")

# lea, test, xchg, inc/dec/neg/not/mul/div/idiv, imul
for m in MEMS:
    add(f"lea rax, {m}"); add(f"lea r10, {m}"); add(f"lea eax, {m}")
for bits in (8,16,32,64):
    kw, regs = SZ[bits]
    add(f"test {regs[0]}, {regs[0]}"); add(f"test {regs[3]}, {regs[9]}")
    add(f"test {kw} ptr [rax], {regs[1]}")
    for imm in (1,0x7f,0xff, 0x1000):
        if bits == 8 and imm > 255: continue
        add(f"test {regs[0]}, {imm}"); add(f"test {regs[5]}, {imm}"); add(f"test {kw} ptr [rbx], {imm}")
    for op in ["inc","dec","neg","not","mul","div","idiv","imul"]:
        add(f"{op} {regs[0]}"); add(f"{op} {regs[9]}"); add(f"{op} {kw} ptr [rbp-4]")
    add(f"xchg {regs[3]}, {regs[1]}"); add(f"xchg {regs[0]}, {regs[1]}"); add(f"xchg {regs[1]}, {regs[0]}"); add(f"xchg {regs[9]}, {regs[0]}")
    add(f"xchg {kw} ptr [rax], {regs[2]}"); add(f"xchg {regs[2]}, {kw} ptr [rax]")
for bits in (16,32,64):
    kw, regs = SZ[bits]
    add(f"imul {regs[0]}, {regs[1]}"); add(f"imul {regs[8]}, {kw} ptr [rax+rbx]")
    add(f"imul {regs[0]}, {regs[1]}, 10"); add(f"imul {regs[0]}, {regs[1]}, 1000"); add(f"imul {regs[9]}, {kw} ptr [rsp+8], -3")
    add(f"imul {regs[0]}, 10")

# shifts
for op in ["rol","ror","rcl","rcr","shl","shr","sal","sar"]:
    for bits in (8,16,32,64):
        kw, regs = SZ[bits]
        add(f"{op} {regs[0]}, 1"); add(f"{op} {regs[9]}, 3"); add(f"{op} {regs[3]}, cl"); add(f"{op} {kw} ptr [rax], 5"); add(f"{op} {kw} ptr [rax], cl"); add(f"{op} {kw} ptr [rax], 1")

# push/pop/misc
for r in R64: add(f"push {r}"); add(f"pop {r}")
for imm in (0,1,-1,127,128,0x1000,-129,0x7fffffff): add(f"push {imm}")
add("push qword ptr [rax]"); add("pop qword ptr [rbp-8]")
for x in ["nop","ret","leave","hlt","int3","syscall","cpuid","rdtsc","ud2","pause","cld","std","cdq","cqo","cdqe","cwde","cwd","cbw","mfence","lfence","sfence","lahf","sahf","clc","stc","cmc"]:
    add(x)
add("ret 8"); add("int 0x80"); add("int 3")
for p in ["rep","repe","repne"]:
    for s in ["movsb","movsq","stosb","stosq","stosd","lodsb","scasb","cmpsb","movsw"]:
        add(f"{p} {s}")
add("lock xadd qword ptr [rax], rbx"); add("lock cmpxchg qword ptr [rdi], rcx"); add("lock add dword ptr [rax], 1"); add("lock inc qword ptr [rax]")
add("nop dword ptr [rax]"); add("nop word ptr [rax+rax]"); add("nop dword ptr [rax+rax*1+0]")

# movzx/movsx/movsxd
for d in (R16[:2]+R32[:2]+R64[:2]+[R64[9]]):
    pass
for dst in (["ax","r9w"], ["eax","r9d","edi"], ["rax","r9","rdi"]):
    for d in dst:
        for s in ("al","cl","sil","r9b","bl"):
            add(f"movzx {d}, {s}"); add(f"movsx {d}, {s}")
        add(f"movzx {d}, byte ptr [rax]"); add(f"movsx {d}, byte ptr [rbx+4]")
        if d not in ("ax","r9w"):
            add(f"movzx {d}, word ptr [rax]"); add(f"movsx {d}, word ptr [rax]"); add(f"movzx {d}, cx"); add(f"movsx {d}, r10w")
for d in ("rax","r9"):
    add(f"movsxd {d}, ecx"); add(f"movsxd {d}, dword ptr [rax+rbx*4]")

# cmovcc / setcc / bit ops
ccs = ["o","no","b","ae","e","ne","be","a","s","ns","p","np","l","ge","le","g","z","nz","c","nc","nae","nb","na","nbe","pe","po","nge","nl","ng","nle"]
for cc in ccs:
    add(f"cmov{cc} rax, rbx"); add(f"cmov{cc} r9d, dword ptr [rax]")
    add(f"set{cc} al"); add(f"set{cc} sil"); add(f"set{cc} byte ptr [rax]"); add(f"set{cc} r9b")
for op in ["bt","bts","btr","btc"]:
    add(f"{op} rax, rbx"); add(f"{op} eax, 5"); add(f"{op} qword ptr [rax], rcx"); add(f"{op} dword ptr [rax], 7"); add(f"{op} r9, 63")
for op in ["shld","shrd"]:
    for bits in (16,32,64):
        kw, regs = SZ[bits]
        for a, b in [(0,1),(3,7),(9,12),(15,0)]:
            add(f"{op} {regs[a]}, {regs[b]}, 1")
            add(f"{op} {regs[a]}, {regs[b]}, 31")
            add(f"{op} {regs[a]}, {regs[b]}, cl")
        add(f"{op} {kw} ptr [rax], {regs[1]}, 4")
        add(f"{op} {kw} ptr [rbx+rcx*4+16], {regs[9]}, cl")
for op in ["bsf","bsr","popcnt","lzcnt","tzcnt"]:
    add(f"{op} rax, rbx"); add(f"{op} r9d, dword ptr [rax]"); add(f"{op} ax, bx")
add("bswap eax"); add("bswap rax"); add("bswap r9"); add("bswap r9d")

# indirect call/jmp
for r in ("rax","r9","rbx"): add(f"call {r}"); add(f"jmp {r}")
add("call qword ptr [rax]"); add("jmp qword ptr [rax+rbx*8]"); add("call qword ptr [rip+0x10]")
add("mov rax, qword ptr fs:[0x28]"); add("mov qword ptr gs:[0x10], rbx"); add("mov rax, fs:[0]")

# SSE
x = ["xmm0","xmm1","xmm7","xmm8","xmm15"]
for op in ["movaps","movups","movapd","movupd","movdqa","movdqu","movss","movsd"]:
    add(f"{op} xmm0, xmm1"); add(f"{op} xmm9, xmm2"); add(f"{op} xmm0, [rax]"); add(f"{op} [rbp-16], xmm10"); add(f"{op} xmm3, [rip+0x40]")
for base in ["add","sub","mul","div","min","max","sqrt"]:
    for suf in ["ss","sd","ps","pd"]:
        add(f"{base}{suf} xmm0, xmm1"); add(f"{base}{suf} xmm8, [rax+rbx*2]"); add(f"{base}{suf} xmm1, xmm15")
for op in ["andps","andpd","andnps","andnpd","orps","orpd","xorps","xorpd","ucomiss","ucomisd","comiss","comisd","cvtss2sd","cvtsd2ss","cvtps2pd","cvtpd2ps","cvtdq2ps","cvtdq2pd",
           "pxor","por","pand","pandn","paddb","paddw","paddd","paddq","psubb","psubw","psubd","psubq","pcmpeqb","pcmpeqw","pcmpeqd","pcmpgtb","punpcklbw","punpcklqdq","unpcklps","unpcklpd"]:
    add(f"{op} xmm0, xmm1"); add(f"{op} xmm9, [rax]")
add("cvtsi2sd xmm0, rax"); add("cvtsi2sd xmm0, eax"); add("cvtsi2ss xmm9, r10"); add("cvtsi2sd xmm0, qword ptr [rax]"); add("cvtsi2ss xmm0, dword ptr [rax]")
for op in ["cvttsd2si","cvttss2si","cvtsd2si","cvtss2si"]:
    add(f"{op} rax, xmm0"); add(f"{op} eax, xmm9"); add(f"{op} r9, qword ptr [rax]") if "sd" in op else add(f"{op} r9, dword ptr [rax]")
add("movq xmm0, rax"); add("movq rax, xmm0"); add("movq xmm9, r10"); add("movq xmm0, xmm1"); add("movq xmm0, qword ptr [rax]"); add("movq qword ptr [rax], xmm0")
add("movd xmm0, eax"); add("movd eax, xmm0"); add("movd xmm9, r9d"); add("movd xmm0, dword ptr [rax]"); add("movd dword ptr [rax], xmm1")
add("movmskpd eax, xmm0"); add("movmskps r9d, xmm1"); add("pmovmskb eax, xmm2")
add("shufps xmm0, xmm1, 0x1b"); add("pshufd xmm0, xmm1, 0xe4"); add("shufpd xmm2, [rax], 1")
add("ldmxcsr dword ptr [rax]"); add("stmxcsr dword ptr [rsp]")
add("prefetcht0 byte ptr [rax]"); add("prefetchnta byte ptr [rax]")

seen = set(); uniq = []
for l in lines:
    if l and l not in seen:
        seen.add(l); uniq.append(l)

dump = sys.argv[1]
tmp = tempfile.mkdtemp()
with open(os.path.join(tmp, "in.txt"), "w") as f:
    f.write("\n".join(uniq) + "\n")
mine = subprocess.run([dump, os.path.join(tmp, "in.txt")], capture_output=True, text=True).stdout.splitlines()

# one `as` run for everything: a label before each line, then read the symbol offsets
asm = [".intel_syntax noprefix", ".text"]
for i, l in enumerate(uniq):
    asm.append(f"L{i}:"); asm.append(l)
asm.append("Lend:")
open(os.path.join(tmp, "a.s"), "w").write("\n".join(asm) + "\n")
r = subprocess.run(["as", "--64", "-o", os.path.join(tmp, "a.o"), os.path.join(tmp, "a.s")], capture_output=True, text=True)
bad_gas = set()
if r.returncode:
    # drop the lines gas rejects (e.g. mnemonics it spells differently) and say so
    for ln in r.stderr.splitlines():
        parts = ln.split(":")
        if len(parts) > 3 and parts[1].isdigit():
            bad_gas.add((int(parts[1]) - 3) // 2)
    asm = [".intel_syntax noprefix", ".text"]
    for i, l in enumerate(uniq):
        if i in bad_gas: continue
        asm.append(f"L{i}:"); asm.append(l)
    asm.append("Lend:")
    open(os.path.join(tmp, "a.s"), "w").write("\n".join(asm) + "\n")
    r = subprocess.run(["as", "--64", "-o", os.path.join(tmp, "a.o"), os.path.join(tmp, "a.s")], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr[:2000]); sys.exit(2)
subprocess.run(["objcopy", "-O", "binary", "-j", ".text", os.path.join(tmp, "a.o"), os.path.join(tmp, "a.bin")], check=True)
blob = open(os.path.join(tmp, "a.bin"), "rb").read()
nm = subprocess.run(["nm", "-n", os.path.join(tmp, "a.o")], capture_output=True, text=True).stdout.split("\n")
offs = {}
for ln in nm:
    p = ln.split()
    if len(p) == 3 and p[2].startswith("L"):
        offs[p[2]] = int(p[0], 16)
gas = {}
order = [i for i in range(len(uniq)) if i not in bad_gas]
for k, i in enumerate(order):
    start = offs[f"L{i}"]
    end = offs[f"L{order[k+1]}"] if k + 1 < len(order) else offs["Lend"]
    gas[i] = blob[start:end].hex()

mism = 0; errs = 0; ok = 0
for i, l in enumerate(uniq):
    m = mine[i]
    if i in bad_gas:
        print(f"gas rejects (skipped): {l}  | mine: {m}")
        continue
    if m.startswith("ERR"):
        errs += 1; print(f"MINE REJECTS: {l}   [{m}]"); continue
    if m != gas[i]:
        mism += 1; print(f"MISMATCH: {l:44s} mine={m}  gas={gas[i]}")
    else:
        ok += 1
print(f"{ok} identical, {mism} different, {errs} rejected by Occult, {len(bad_gas)} skipped (gas rejects) of {len(uniq)}")
sys.exit(1 if (mism or errs) else 0)
