## 编译/运行方法
_注1：如果安装的是`bazelisk`，则下面`bazel`应当替换成`bazelisk`_
_注2：`--jobs=4`在服务器上不加也可以，但如果编译过程中卡死并且见到10个并行任务，则必须加_
```bash
bazel build //src:my_dpf_demo --jobs=4
bazel run //src:my_dpf_demo --jobs=4
```

## 执行输出
```bash
Simple DPF demo (API-compatible) start
WARNING: All log messages before absl::InitializeLog() is called are written to STDERR
I0000 00:00:1767008796.144649   18087 distributed_point_function.cc:596] Highway is in AVX2 mode
Keys generated successfully.
Evaluation results (sum of shares, modulo 2^32):
x = 0  share0 = 4167456498  share1 = 127510798  sum_raw = 4294967296  sum_mod = 0  <-- expected 0
x = 1  share0 = 3566850802  share1 = 728116494  sum_raw = 4294967296  sum_mod = 0  <-- expected 0
x = 2  share0 = 3873118683  share1 = 421848613  sum_raw = 4294967296  sum_mod = 0  <-- expected 0
x = 3  share0 = 4273945744  share1 = 21021594  sum_raw = 4294967338  sum_mod = 42  <-- expected beta = 42
x = 4  share0 = 3280303340  share1 = 1014663956  sum_raw = 4294967296  sum_mod = 0  <-- expected 0
x = 5  share0 = 1866756023  share1 = 2428211273  sum_raw = 4294967296  sum_mod = 0  <-- expected 0
x = 6  share0 = 212465842  share1 = 4082501454  sum_raw = 4294967296  sum_mod = 0  <-- expected 0
x = 7  share0 = 1519399754  share1 = 2775567542  sum_raw = 4294967296  sum_mod = 0  <-- expected 0
Simple DPF demo finished successfully.
```
