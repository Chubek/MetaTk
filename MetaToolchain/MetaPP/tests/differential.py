"""Compare preprocessing tokens with an independent system C preprocessor."""
import random
import re
import subprocess
import sys

tool, compiler = sys.argv[1:]
token = re.compile(r'''(?:u8|[LuU])?(?:"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|[A-Za-z_][A-Za-z_0-9]*|(?:[0-9]|\.[0-9])(?:[A-Za-z_0-9.]|(?<=[eEpP])[+-])*|%:%:|>>=|<<=|\.\.\.|##|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[*/%+&^|!-]=|\S''')

cases = [
    '#define f(x) x x\nf(f(1))\n',
    '#define x 2\n#define f(a) f(x * (a))\n#define g f\n#define z z[0]\n#define h g(~\n#define m(a) a(w)\n#define w 0,1\n#define t(a) a\nf(y+1) + f(f(z)) % t(t(g)(0) + t)(1);\ng(x+(3,4)-w) | h 5) & m(f)^m(m);\n',
    '#define foo(x) bar x\n#define bar foo\nfoo(foo)(2)\n',
    '#define A() B\n#define B() A\nA()()()\n',
    '#define F(x) x\n#define G F\nF(G)(4)\n',
    '#define AFTERX(x) X_ ## x\n#define XAFTERX(x) AFTERX(x)\n#define BUFSIZE TABLESIZE\n#define TABLESIZE 1024\n#define X_BUFSIZE 7\nAFTERX(BUFSIZE) XAFTERX(BUFSIZE)\n',
    '#define hash_hash # ## #\n#define mkstr(a) # a\n#define in_between(a) mkstr(a)\n#define join(c,d) in_between(c hash_hash d)\njoin(x,y)\n',
    '#define S(x) #x\nS(a/**/b) S(a\nb) S( "a\\\\b" )\n',
    '#define X a/*\n*/b\nX __LINE__\n',
    '#define CAT(a,b,c) a##b##c\nCAT(,,) CAT(a,,b) CAT(,a,)\n',
    '#define V(...) #__VA_ARGS__\nV() V(a,b,c)\n',
    '#line 70 "other.c"\n__LINE__ __FILE__\n',
    '#define D(x) _Pragma(#x)\nD(pack(push, 1))\nint x;\nD(pack(pop))\n',
    '#define S(x) #x\nS(%:) S(%:%:) S(<:) S(:>)\n',
    '%:define S(x) %:x\nS(a  + b)\n',
    '%:define CAT(a,b) a %:%: b\nCAT(%,:) CAT(%:,%:)\n',
    '#define S(x) #x\n#define X(x) S(x)\n#define E\nX(a E+b)\n',
    '#define S(x) #x\n#define X(x) S(x)\n#define F(x) x\nX(a F()+b)\n',
    '#define S(x) #x\n#define X(x) S(x)\n#define F(x) a x+b\nX(F())\n',
    '#define S(x) #x\n#define X(x) S(x)\n#define F(x) a x\nX(F()+b)\n',
]
rng = random.Random(7919)
ops = ['+', '-', '*', '/', '%', '&', '|', '^', '==', '!=', '<', '<=', '>', '>=', '&&', '||']
for _ in range(160):
    a, b, c = [rng.randint(1, 200) for _ in range(3)]
    op, op2 = rng.choice(ops), rng.choice(ops)
    expression = f'({a} {op} {b}) {op2} {c}'
    cases.append(f'#if {expression}\nYES\n#else\nNO\n#endif\n')
for expression in ['-1 < 1U', '(1 ? -1 : 1U) < 0', '0xffffffffffffffff > 0',
                   '1 || 1/0', '0 && 1/0', '1 ? 2 : 1/0', '(1U << 63) >> 63',
                   '-8 >> 2 == -2', '(0 ? 1/0 : -1) == -1']:
    cases.append(f'#if {expression}\nYES\n#else\nNO\n#endif\n')
for index, source in enumerate(cases):
    ours = subprocess.run([tool], input=source, text=True, capture_output=True, timeout=10)
    reference = subprocess.run([compiler, '-std=c11', '-E', '-P', '-x', 'c', '-'],
                               input=source, text=True, capture_output=True, timeout=10)
    if ours.returncode or reference.returncode or token.findall(ours.stdout) != token.findall(reference.stdout):
        print(f'Case {index}:\n{source}\nMetaPP:\n{ours.stdout}{ours.stderr}\nReference:\n{reference.stdout}{reference.stderr}')
        sys.exit(1)
print(f'{len(cases)} independent preprocessor comparisons passed')
for literal in ['1lL', '1Ll', '1ulL', '1LlU', '0x1uLl']:
    source = f'#if {literal}\nYES\n#endif\n'
    for command in [[tool], [compiler, '-std=c11', '-E', '-P', '-x', 'c', '-']]:
        result = subprocess.run(command, input=source, text=True, capture_output=True, timeout=10)
        if result.returncode == 0:
            sys.exit(f'{command[0]} accepted malformed integer suffix {literal}')
print('5 independent invalid-suffix comparisons passed')
