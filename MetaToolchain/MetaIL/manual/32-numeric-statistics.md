# 32. Numeric statistics and MetaNum

Twenty-four analysis kernels use MetaNum's executable statistics. Names combine
`Numeric`, a source (`Literals` or `ExpressionSizes`), and a statistic:

```text
Mean, SampleVariance, PopulationVariance, SampleStdDev,
PopulationStdDev, Median, Minimum, Maximum,
Quantile10, Quantile25, Quantile75, Quantile90
```

For example, `NumericLiteralsMean` reports the arithmetic mean of integer-literal
occurrences in each function. `NumericExpressionSizesQuantile90` measures the
90th percentile of root expression node counts.

```cpp
auto report = context.analyze(module,
    "NumericLiteralsMean,NumericExpressionSizesQuantile90");
```

These are descriptive syntactic statistics. They include unreachable statements
and unevaluated select arms, and do not represent runtime distributions. Integer
samples convert to binary64; values beyond 2^53 may lose low bits. Statistical
results are not used to prove signed-64 rewrites.

Reports include the function's sample count, source, statistic, precision, and
status. A successful result has `status="ok"` and a value. Insufficient samples
or non-finite values are reported explicitly, keeping JSON valid. Sample variance
and standard deviation need at least two samples; population statistics need one.

The Lua `metanum` facade also exposes covariance, correlation, quantiles, and
catalog metadata. The upstream GSL/ISL/PPL catalog entries carry `callable=false`;
listing them does not imply an executable binding. See `docs/MetaNum.md` for the
complete numeric contract and the 96 separate signed-64 numeric rewrite kernels.

[Contents](README.md) · [Next: Registry](33-kernel-registry.md)
