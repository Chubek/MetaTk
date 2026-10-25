from pathlib import Path
import argparse
parser=argparse.ArgumentParser(description='Regenerate the 200 base and 120 numeric Optikern kernels.')
parser.add_argument('--check',action='store_true',help='check committed output without writing')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]/'optimize-kernels'
assert (root/'ConstantFolding.optk').exists()
outputs={}
def emit(path,text):
 assert path not in outputs,path
 outputs[path]=text
used={'ConstantFolding'}
analyses={
'InstructionCount':('instruction-count',{}),'FunctionCount':('function-count',{}),
'ExpressionNodeCount':('node-count',{}),'MaximumExpressionDepth':('maximum-expression-depth',{}),
'LiteralHistogram':('literal-histogram',{}),'VariableUses':('variable-uses',{}),
'DefinitionCounts':('definitions',{}),'UnusedDefinitions':('unused-definitions',{}),
'CallGraph':('call-graph',{}),'ImpureCallSites':('impure-call-sites',{}),
'ControlFlowGraph':('cfg',{}),'ReachableBlocks':('reachable-blocks',{}),
'UnreachableBlocks':('unreachable-blocks',{}),'Dominators':('dominators',{}),
'PostDominators':('postdominators',{}),'LiveVariables':('liveness',{}),
'ReachingDefinitions':('reaching-definitions',{}),'NaturalLoops':('natural-loops',{}),
'CriticalEdges':('critical-edges',{}),'CyclomaticComplexity':('cyclomatic-complexity',{}),
'LoopHeaders':('loop-headers',{}),'BlockDefUse':('block-def-use',{}),
'ExpressionRedundancy':('expression-redundancy',{}),'ConstantBranches':('constant-branches',{}),
'LiteralValueRange':('literal-range',{}),'ParameterUses':('parameter-uses',{}),
'BranchTargets':('branch-targets',{}),'TerminalBlocks':('terminal-blocks',{}),
'OpcodeHistogram':('opcode-histogram',{}),'StatementHistogram':('statement-histogram',{}),
'ExpressionPurity':('expression-purity',{}),
}
ops=['add','sub','mul','div','mod','band','bor','bxor','shl','shr','eq','ne','lt','le','gt','ge','land','lor','neg','bnot','lnot','select']
for op in ops: analyses[f'Count{op.title()}Operations']=('operator-count',{'op':op})
# 31 + 22 = 53 static analyses; 7 statement variants = 60.
for stmt in ['let','return','eval','assume','br','goto','label']:
 analyses[f'Count{stmt.title()}Statements']=('statement-count',{'statement':stmt})

def lua_cfg(d):
 return '{ '+', '.join(f'{k} = "{v}"' for k,v in d.items())+' }'
for name,(kind,kwargs) in analyses.items():
 assert name not in used
 used.add(name)
 emit(root/(name+'.optk'),f'''-- Executable {kind} analysis kernel.\nlocal K = lkopt.optimizer.new "{name}"\nK:analysis {lua_cfg({'kind':kind,**kwargs})}\n''')
for name,kind in [
 ('DeadAssignmentElimination','dead-assignments'),
 ('ConstantPropagation','constant-propagation'),
 ('CopyPropagation','copy-propagation'),
 ('BranchSimplification','simplify-branches'),
 ('UnreachableCodeElimination','unreachable-code'),
 ('CommonSubexpressionElimination','common-subexpressions'),
]:
 assert name not in used
 used.add(name)
 emit(root/(name+'.optk'),f'''-- Procedural CFG-aware optimization.\nlocal K = lkopt.optimizer.new "{name}"\nK:builtin "{kind}"\n''')
rewrites=[]
def rule(name,expr,repl,guard='',why=''):
 assert name not in used,name
 assert expr!=repl,name
 rewrites.append((name,expr,repl,guard,why))
 used.add(name)
# Primitive identities, absorption, comparison simplifications.
for name,a,b,g in [
 ('AddZeroRight','add($x,0)','$x',''),('AddZeroLeft','add(0,$x)','$x',''),
 ('SubtractZero','sub($x,0)','$x',''),('ZeroMinus','sub(0,$x)','neg($x)',''),
 ('MultiplyOneRight','mul($x,1)','$x',''),('MultiplyOneLeft','mul(1,$x)','$x',''),
 ('MultiplyZeroRight','mul($x,0)','0','pure:x'),('MultiplyZeroLeft','mul(0,$x)','0','pure:x'),
 ('DivideByOne','div($x,1)','$x',''),('ModuloOne','mod($x,1)','0','pure:x'),
 ('BitAndAllOnesRight','band($x,-1)','$x',''),('BitAndAllOnesLeft','band(-1,$x)','$x',''),
 ('BitAndZeroRight','band($x,0)','0','pure:x'),('BitAndZeroLeft','band(0,$x)','0','pure:x'),
 ('BitOrZeroRight','bor($x,0)','$x',''),('BitOrZeroLeft','bor(0,$x)','$x',''),
 ('BitOrAllOnesRight','bor($x,-1)','-1','pure:x'),('BitOrAllOnesLeft','bor(-1,$x)','-1','pure:x'),
 ('BitXorZeroRight','bxor($x,0)','$x',''),('BitXorZeroLeft','bxor(0,$x)','$x',''),
 ('ShiftLeftByZero','shl($x,0)','$x',''),('ShiftRightByZero','shr($x,0)','$x',''),
 ('DoubleNegate','neg(neg($x))','$x',''),('DoubleBitwiseNot','bnot(bnot($x))','$x',''),
 ('DoubleLogicalNot','lnot(lnot($x))','ne($x,0)',''),
 ('SubtractSelf','sub($x,$x)','0','pure:x'),('XorSelf','bxor($x,$x)','0','pure:x'),
 ('AndSelf','band($x,$x)','$x','pure:x'),('OrSelf','bor($x,$x)','$x','pure:x'),
 ('EqualityReflexive','eq($x,$x)','1','pure:x'),('InequalityReflexive','ne($x,$x)','0','pure:x'),
 ('LessThanReflexive','lt($x,$x)','0','pure:x'),('LessEqualReflexive','le($x,$x)','1','pure:x'),
 ('GreaterThanReflexive','gt($x,$x)','0','pure:x'),('GreaterEqualReflexive','ge($x,$x)','1','pure:x'),
 ('SelectTrue','select(1,$a,$b)','$a','pure:b'),('SelectFalse','select(0,$a,$b)','$b','pure:a'),
 ('SelectIdenticalArms','select($c,$v,$v)','$v','pure:c'),
 ('SelectTrueFalse','select($c,1,0)','ne($c,0)',''),('SelectFalseTrue','select($c,0,1)','eq($c,0)',''),
 ('SelectSwappedCondition','select(lnot($c),$a,$b)','select($c,$b,$a)',''),
 ('LogicalAndZeroRight','land($x,0)','0','pure:x'),('LogicalAndZeroLeft','land(0,$x)','0','pure:x'),
 ('LogicalOrOneRight','lor($x,1)','1','pure:x'),('LogicalOrOneLeft','lor(1,$x)','1','pure:x'),
 ('LogicalAndOneRight','land($x,1)','ne($x,0)',''),('LogicalAndOneLeft','land(1,$x)','ne($x,0)',''),
 ('LogicalOrZeroRight','lor($x,0)','ne($x,0)',''),('LogicalOrZeroLeft','lor(0,$x)','ne($x,0)',''),
 ('LogicalAndIdempotence','land($x,$x)','ne($x,0)','pure:x'),
 ('LogicalOrIdempotence','lor($x,$x)','ne($x,0)','pure:x'),
 ('EqualityZeroRight','eq($x,0)','lnot($x)',''),('EqualityZeroLeft','eq(0,$x)','lnot($x)',''),
 ('InequalityZeroRight','ne($x,0)','ne($x,0)',''), # special no-op: omitted below
 ('BitAndComplementRight','band($x,bnot($x))','0','pure:x'),
 ('BitAndComplementLeft','band(bnot($x),$x)','0','pure:x'),
 ('BitOrComplementRight','bor($x,bnot($x))','-1','pure:x'),
 ('BitOrComplementLeft','bor(bnot($x),$x)','-1','pure:x'),
 ('BitXorComplementRight','bxor($x,bnot($x))','-1','pure:x'),
 ('BitXorComplementLeft','bxor(bnot($x),$x)','-1','pure:x'),
 ('BitwiseAndAbsorbRight','band($x,bor($x,$y))','$x','pure:x,y'),
 ('BitwiseAndAbsorbLeft','band(bor($x,$y),$x)','$x','pure:x,y'),
 ('BitwiseOrAbsorbRight','bor($x,band($x,$y))','$x','pure:x,y'),
 ('BitwiseOrAbsorbLeft','bor(band($x,$y),$x)','$x','pure:x,y'),
 ('NegateSubtraction','neg(sub($x,$y))','sub($y,$x)','pure:x,y'),
 ('SubtractNegation','sub($x,neg($y))','add($x,$y)',''),
 ('AddNegativeRight','add($x,neg($y))','sub($x,$y)',''),
 ('AddNegativeLeft','add(neg($x),$y)','sub($y,$x)','pure:x,y'),
 ('BitNotAnd','bnot(band($x,$y))','bor(bnot($x),bnot($y))',''),
 ('BitNotOr','bnot(bor($x,$y))','band(bnot($x),bnot($y))',''),
 ('CompareDoubleBitNot','eq(bnot($x),bnot($y))','eq($x,$y)',''),
 ('NotEqualDoubleBitNot','ne(bnot($x),bnot($y))','ne($x,$y)',''),
 ('CompareDoubleNegation','eq(neg($x),neg($y))','eq($x,$y)',''),
 ('NotEqualDoubleNegation','ne(neg($x),neg($y))','ne($x,$y)',''),
 ('SubtractOfSame','sub(add($x,$y),$x)','$y','pure:x'),
 ('SubtractOfSameRight','sub(add($x,$y),$y)','$x','pure:y'),
 ('AddSubtractCancel','add(sub($x,$y),$y)','$x','pure:y'),
 ('AddSubtractCancelLeft','add($y,sub($x,$y))','$x','pure:y'),
 ('SubtractSubtractCancel','sub($x,sub($x,$y))','$y','pure:x'),
 ('XorXorCancel','bxor(bxor($x,$y),$y)','$x','pure:y'),
 ('XorXorCancelLeft','bxor($y,bxor($x,$y))','$x','pure:y'),
 ('AndIdempotentNested','band(band($x,$y),$y)','band($x,$y)','pure:y'),
 ('OrIdempotentNested','bor(bor($x,$y),$y)','bor($x,$y)','pure:y'),
 ('SelectNestedTrue','select($c,select($c,$a,$b),$b)','select($c,$a,$b)','pure:c'),
 ('SelectNestedFalse','select($c,$a,select($c,$b,$d))','select($c,$a,$d)','pure:c'),
 ('SelectNestedBoth','select($c,select($c,$a,$b),select($c,$d,$e))','select($c,$a,$e)','pure:c'),
 ('EqualitySameAdd','eq(add($x,$c),add($y,$c))','eq($x,$y)','pure:c'),
 ('InequalitySameAdd','ne(add($x,$c),add($y,$c))','ne($x,$y)','pure:c'),
 ('EqualitySameXor','eq(bxor($x,$c),bxor($y,$c))','eq($x,$y)','pure:c'),
 ('InequalitySameXor','ne(bxor($x,$c),bxor($y,$c))','ne($x,$y)','pure:c'),
 ('EqualitySameSub','eq(sub($x,$c),sub($y,$c))','eq($x,$y)','pure:c'),
 ('InequalitySameSub','ne(sub($x,$c),sub($y,$c))','ne($x,$y)','pure:c'),
]:
 if a==b: continue
 rule(name,a,b,g,'Signed-64 wraparound; pure guards preserve both effects and traps.')
# Constant-combination canonicalizations retain a single evaluation of variables.
for name,a,b,guard in [
 ('AddConstantsRightNested','add(add($x,$a),$b)','add($x,add($a,$b))','const:a,b'),
 ('AddConstantsLeftNested','add($a,add($x,$b))','add($x,add($a,$b))','const:a,b'),
 ('AddConstantsMixedNested','add(add($a,$x),$b)','add($x,add($a,$b))','const:a,b'),
 ('SubtractConstantsNested','sub(sub($x,$a),$b)','sub($x,add($a,$b))','const:a,b'),
 ('AddMinusConstantNested','add(sub($x,$a),$b)','add($x,sub($b,$a))','const:a,b'),
 ('SubtractAddedConstants','sub(add($x,$a),$b)','add($x,sub($a,$b))','const:a,b'),
 ('MultiplyConstantsNested','mul(mul($x,$a),$b)','mul($x,mul($a,$b))','const:a,b'),
 ('MultiplyConstantsLeftNested','mul($a,mul($x,$b))','mul($x,mul($a,$b))','const:a,b'),
 ('XorConstantsNested','bxor(bxor($x,$a),$b)','bxor($x,bxor($a,$b))','const:a,b'),
 ('AndConstantsNested','band(band($x,$a),$b)','band($x,band($a,$b))','const:a,b'),
 ('OrConstantsNested','bor(bor($x,$a),$b)','bor($x,bor($a,$b))','const:a,b'),
]:rule(name,a,b,guard,'combines adjacent integer constants')
# Arithmetic strength reduction for literal powers of two. Full unsigned wrap makes this valid even on negative operands.
for n in range(1,7):
 power=1<<n
 rule(f'MultiplyByPowerOfTwoRight{n:02d}',f'mul($x,{power})',f'shl($x,{n})','','replace multiplication by constant with shift')
 rule(f'MultiplyByPowerOfTwoLeft{n:02d}',f'mul({power},$x)',f'shl($x,{n})','','replace multiplication by constant with shift')
# Collapse nested well-defined signed-left/logical-arithmetic-right shifts.
for n in range(1,8):
 for op,word in [('shl','Left'),('shr','Right')]:
  rule(f'CollapseShift{word}By{n:02d}',f'{op}({op}($x,{n}),1)',f'{op}($x,{n+1})','','remove redundant shift stage')
# Mask absorption: wider masks cannot restore bits already cleared by a narrower mask.
for n in range(1,7):
 narrower=(1<<n)-1;wider=(1<<(n+1))-1
 rule(f'AbsorbWiderMask{n:02d}',f'band(band($x,{narrower}),{wider})',f'band($x,{narrower})','','drop subsumed upper mask')
# Selected kernels above already exceed the needed count; keep high-signal range.
# At least 200 *additional* kernels beyond the supplied ConstantFolding entry.
target=200
remaining=target-(len(analyses)+6)
assert len(rewrites)>=remaining,(len(rewrites),remaining)
rewrites=rewrites[:remaining]
for name,pat,repl,guard,why in rewrites:
 emit(root/(name+'.optk'),
 f'-- {why or "Semantics-preserving expression rewrite."}\n'
 f'local K = lkopt.optimizer.new "{name}"\n'
 f'K:rewrite {{ pattern = "{pat}", replace = "{repl}", guard = "{guard}" }}\n')
assert len(outputs)==200
base_rewrites=len(rewrites)
rewrites=[]
for name,pat,repl,guard in [
 ('MultiplyMinusOneRight','mul($x,-1)','neg($x)',''),
 ('MultiplyMinusOneLeft','mul(-1,$x)','neg($x)',''),
 ('MultiplyNegatedBoth','mul(neg($x),neg($y))','mul($x,$y)',''),
 ('AddComplementRight','add($x,bnot($x))','-1','pure:x'),
 ('AddComplementLeft','add(bnot($x),$x)','-1','pure:x'),
 ('NegateIncrement','neg(add($x,1))','bnot($x)',''),
 ('ComplementIncrement','add(bnot($x),1)','neg($x)',''),
 ('NegatedMinusOne','sub(neg($x),1)','bnot($x)',''),
 ('SubtractNegatedSelf','sub($x,neg($x))','shl($x,1)','pure:x'),
 ('DoubleAsShift','add($x,$x)','shl($x,1)','pure:x'),
 ('FactorAddRight','add(mul($x,$c),mul($y,$c))','mul(add($x,$y),$c)','pure:c'),
 ('FactorAddLeft','add(mul($c,$x),mul($c,$y))','mul($c,add($x,$y))','pure:c'),
 ('FactorSubtractRight','sub(mul($x,$c),mul($y,$c))','mul(sub($x,$y),$c)','pure:c'),
 ('FactorSubtractLeft','sub(mul($c,$x),mul($c,$y))','mul($c,sub($x,$y))','pure:c'),
 ('CancelNegatedProductLeft','neg(mul(neg($x),$y))','mul($x,$y)',''),
 ('CancelNegatedProductRight','neg(mul($x,neg($y)))','mul($x,$y)',''),
 ('CompareComplementLT','lt(bnot($x),bnot($y))','gt($x,$y)',''),
 ('CompareComplementLE','le(bnot($x),bnot($y))','ge($x,$y)',''),
 ('CompareComplementGT','gt(bnot($x),bnot($y))','lt($x,$y)',''),
 ('CompareComplementGE','ge(bnot($x),bnot($y))','le($x,$y)',''),
 ('XorComplements','bxor(bnot($x),bnot($y))','bxor($x,$y)',''),
 ('AndContradiction','band(band($x,$y),bnot($y))','0','pure:x,y'),
 ('OrTautology','bor(bor($x,$y),bnot($y))','-1','pure:x,y'),
 ('XorCancelOuter','bxor(bxor($x,$y),$x)','$y','pure:x'),
]: rule('Numeric'+name,pat,repl,guard)

for width in (8,16,24,32,40,48):
 mask=(1<<width)-1
 power=1<<width
 half=width//2
 lowmask=(1<<half)-1
 for name,pat,repl,guard in [
  ('MaskIdempotence',f'band(band($x,{mask}),{mask})',f'band($x,{mask})',''),
  ('MaskOrSaturation',f'band(bor($x,{mask}),{mask})',str(mask),'pure:x'),
  ('DisjointMasks',f'band(band($x,{-power}),{mask})','0','pure:x'),
  ('MaskedLeftShiftVanish',f'band(shl($x,{width}),{mask})','0','pure:x'),
  ('MaskedRightShiftVanish',f'shr(band($x,{mask}),{width})','0','pure:x'),
  ('MaskedDivision',f'div(band($x,{mask}),{1<<half})',f'shr(band($x,{mask}),{half})',''),
  ('MaskedRemainder',f'mod(band($x,{mask}),{1<<half})',f'band($x,{lowmask})',''),
  ('MaskedModuloIdentity',f'mod(band($x,{mask}),{power})',f'band($x,{mask})',''),
  ('MaskedShiftRoundTrip',f'shr(shl(band($x,{mask}),1),1)',f'band($x,{mask})',''),
  ('ShiftRoundTripMask',f'band(shr(shl($x,{64-width}),{64-width}),{mask})',f'band($x,{mask})',''),
  ('MultiplyNegativePowerRight',f'mul($x,{-power})',f'neg(shl($x,{width}))',''),
  ('MultiplyNegativePowerLeft',f'mul({-power},$x)',f'neg(shl($x,{width}))',''),
 ]: rule(f'Numeric{name}{width:02d}',pat,repl,guard)

assert len(rewrites)==96
for name,pat,repl,guard,_ in rewrites:
 emit(root/(name+'.optk'),
  '-- Signed-64 numeric rewrite; preserve wraparound, evaluation order, and traps.\n'
  f'local K = lkopt.optimizer.new "{name}"\n'
  f'K:rewrite {{ pattern = "{pat}", replace = "{repl}", guard = "{guard}" }}\n')
statistics=['Mean','SampleVariance','PopulationVariance','SampleStdDev','PopulationStdDev',
            'Median','Minimum','Maximum','Quantile10','Quantile25','Quantile75','Quantile90']
for source in ('Literals','ExpressionSizes'):
 for statistic in statistics:
  name=f'Numeric{source}{statistic}'
  assert name not in used
  used.add(name)
  emit(root/(name+'.optk'),
   '-- Descriptive numeric analysis computed by MetaNum/numeric.hpp.\n'
   f'local K = lkopt.optimizer.new "{name}"\n'
   f'K:analysis {{ kind = "numeric-statistic", source = "{source}", statistic = "{statistic}" }}\n')
assert len(outputs)==320
if args.check:
 for path,text in outputs.items():
  assert path.exists() and path.read_text()==text,f'stale generated kernel: {path}'
else:
 for path,text in outputs.items():
  path.parent.mkdir(parents=True,exist_ok=True)
  path.write_text(text)
files=set(root.rglob('*.optk'))
assert files==set(outputs)|{root/'ConstantFolding.optk'},'unexpected kernel files (never deleted automatically)'
print(f'Total: 321; analyses: 84; builtins: 7; rewrites: {base_rewrites+len(rewrites)}; numeric additions: 120')
