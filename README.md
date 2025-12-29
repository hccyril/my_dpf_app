# My DPF Demo App

This repository is a minimal C++ demo that shows how to use the
[`google/distributed_point_functions`](https://github.com/google/distributed_point_functions)
library (DPF: Distributed Point Functions) in your own Bazel project.

它演示了下面几件事情：

1. 如何在自己的 Bazel 工程中，把一个已有的 Bazel 仓库（`google_DPF`）作为 **子模块 + 外部依赖** 引入；
2. 如何使用当前版本的 `DistributedPointFunction` C++ API，构造一个最简单的一维 DPF；
3. 如何生成两方密钥并在定义域上评估，验证「两方 share 在环上的和」符合预期。

---

## 1. 仓库结构

```text
my_dpf_app/
  WORKSPACE
  MODULE.bazel?          # 可选，本 demo 实际是以 WORKSPACE + local_repository 为主
  .bazelrc               # Bazel 配置（例如关闭或控制 Bzlmod）
  third_party/
    google_DPF/          # 作为 git submodule 引入的 fork 仓库
                         # 实际代码来自 hccyril/google_DPF 对上游的同步分支
  src/
    BUILD                # 定义本 demo 的 cc_binary
    main.cc              # DPF 示例程序（本 README 主要说明这个文件）
```

> 注意：`third_party/google_DPF` 本身是一个完整的 Bazel 仓库，  
> 内部包含 `dpf/` 源码目录、`experiments/` 基准测试等。

---

## 2. Demo 程序做了什么？

`src/main.cc` 实现了一个「**一维 DPF 示例**」：

- 定义一个一维函数 $f : \{0, \dots, 2^3 - 1\} \to \mathbb{Z}_{2^{32}}$
- 令：
  - 域大小：$2^3 = 8$，即 $x \in \{0, 1, \dots, 7\}$
  - 特殊点：$\alpha = 3$
  - 在 $\alpha$ 处的值：$\beta = 42$
  - 其它点处的值：$0$
- 然后使用 DPF 生成两把密钥 $k_0, k_1$，分别属于两方（server 0 / server 1）；
- 对每个 $x$，两方分别计算：
  - $f_0(x)$：使用 $k_0$ 得到的 share
  - $f_1(x)$：使用 $k_1$ 得到的 share
- 在 **模 $2^{32}$ 的环上**，我们有：
  - 对所有 $x \ne \alpha$：$f_0(x) + f_1(x) \equiv 0 \pmod{2^{32}}$
  - 对 $x = \alpha$：$f_0(\alpha) + f_1(\alpha) \equiv \beta = 42 \pmod{2^{32}}$

程序会打印出：

- 每个点的两方 share；
- 两个 share 在 64 位整数下的「原始和」；
- 把和取模 $2^{32}$ 之后的结果（这才是 DPF 的数学语义）。

---

## 3. 关键代码解析

### 3.1 参数设置

```c++
constexpr int kLogDomainSize = 3;      // 域大小 = 2^3 = 8
constexpr int64_t kDomainSize = 1 << kLogDomainSize;
constexpr int64_t kAlpha = 3;          // 特殊点
constexpr int64_t kBeta = 42;          // 在 alpha 处的值
constexpr int kValueBits = 32;         // 值所在环为 Z_{2^32}
```

- `kLogDomainSize` 指定 DPF 的定义域对数大小；
- `kValueBits` 控制值的 bit 宽度，即计算在模 $2^{32}$ 的整数环中进行。

### 3.2 构造一维 DPF 参数

```c++
DpfParameters parameters;
parameters.set_log_domain_size(kLogDomainSize);
parameters.mutable_value_type()->mutable_integer()->set_bitsize(kValueBits);
```

`DpfParameters` 是 DPF 库定义的一种参数描述类型，可以配置：

- 域大小（通过 `log_domain_size`）；
- 值类型（这里是整数，`bitsize = 32`）。

### 3.3 创建 DPF 实例

```c++
absl::StatusOr<std::unique_ptr<DistributedPointFunction>> dpf_or =
    DistributedPointFunction::CreateIncremental({parameters});
```

- 使用 `CreateIncremental` 构造一维 **增量 DPF**；
- 入参是一个 `std::vector<DpfParameters>`，这里只有一层，所以传 `{parameters}`。

### 3.4 生成两方密钥

```c++
absl::uint128 alpha = absl::uint128(kAlpha);
absl::uint128 beta = absl::uint128(kBeta);

absl::StatusOr<std::pair<DpfKey, DpfKey>> keys_or =
    dpf->GenerateKeys(alpha, beta);
auto [key0, key1] = std::move(keys_or.value());
```

- `GenerateKeys(alpha, beta)` 生成一维 DPF 的两把密钥；
- `DpfKey` 是库定义的密钥类型；
- `key0` 和 `key1` 分别是两方所持的密钥 share。

### 3.5 创建评估上下文

```c++
absl::StatusOr<EvaluationContext> ctx0_or =
    dpf->CreateEvaluationContext(key0);
absl::StatusOr<EvaluationContext> ctx1_or =
    dpf->CreateEvaluationContext(key1);

EvaluationContext ctx0 = std::move(ctx0_or.value());
EvaluationContext ctx1 = std::move(ctx1_or.value());
```

`EvaluationContext` 封装了在某个密钥下评估 DPF 所需的内部状态（增量评估用）。

### 3.6 准备要评估的点

```c++
std::vector<absl::uint128> points;
points.reserve(kDomainSize);
for (int64_t x = 0; x < kDomainSize; ++x) {
  points.push_back(absl::uint128(x));
}
```

- 这里直接把整个域 `[0, 2^3)` 里的所有整数作为评估点；
- 每个点用 `absl::uint128` 表示。

### 3.7 对所有点评估两方 share

```c++
using T = uint32_t;  // 输出类型，与 bitsize=32 对应

absl::StatusOr<std::vector<T>> shares0_or =
    dpf->EvaluateAt<T>(key0, /*level=*/0, points);
absl::StatusOr<std::vector<T>> shares1_or =
    dpf->EvaluateAt<T>(key1, /*level=*/0, points);

std::vector<T> shares0 = std::move(shares0_or.value());
std::vector<T> shares1 = std::move(shares1_or.value());
```

- `EvaluateAt<T>(key, level, points)` 会返回每个点上的 share；
- 这里只用一层（level=0），返回长度为 `kDomainSize` 的两个向量。

### 3.8 在模 $2^{32}$ 环上验证两方和

```c++
std::cout << "Evaluation results (sum of shares, modulo 2^"
          << kValueBits << "):" << std::endl;

const uint64_t modulus = 1ULL << kValueBits;

for (int64_t x = 0; x < kDomainSize; ++x) {
  uint64_t v0 = static_cast<uint64_t>(shares0[x]);
  uint64_t v1 = static_cast<uint64_t>(shares1[x]);
  uint64_t sum_raw = v0 + v1;            // 64 位普通加法
  uint64_t sum_mod = sum_raw % modulus;  // 在模 2^32 环上的和

  std::cout << "x = " << x
            << "  share0 = " << v0
            << "  share1 = " << v1
            << "  sum_raw = " << sum_raw
            << "  sum_mod = " << sum_mod;

  if (x == kAlpha) {
    std::cout << "  <-- expected beta = " << kBeta;
  } else {
    std::cout << "  <-- expected 0";
  }
  std::cout << std::endl;
}
```

- `sum_raw` 是两个 32 位 share 在 64 位整数上的普通相加结果；
- `sum_mod` 是 `sum_raw` 在模 $2^{32}$ 环上的值（真正对应 DPF 的语义）：
  - 对 `x != alpha`，`sum_mod` 应当是 0；
  - 对 `x == alpha`，`sum_mod` 应当是 42。

---

## 4. 构建与运行

你需要在 Linux 或 WSL 上安装：

- Bazel 或 Bazelisk（这里用 Bazelisk）；
- 一个 C++ 工具链（如 `build-essential`）。

### 4.1 克隆并初始化子模块

```bash
git clone https://github.com/hccyril/my_dpf_app.git
cd my_dpf_app

# 初始化并更新子模块（包含 google_DPF）
git submodule sync --recursive
git submodule update --init --recursive --remote
```

> `.gitmodules` 已配置让 `third_party/google_DPF` 跟踪 fork 仓库的 `mainsync` 分支，  
> 该分支与上游 `google/distributed_point_functions` 同步。

### 4.2 构建 demo

```bash
bazelisk build //src:my_dpf_demo --jobs=4
```

如果成功，你会看到类似输出：

```text
Target //src:my_dpf_demo up-to-date:
  bazel-bin/src/my_dpf_demo
INFO: Build completed successfully
```

### 4.3 运行 demo

```bash
bazelisk run //src:my_dpf_demo --jobs=4
```

示例输出（数值可能不同）：

```text
Simple DPF demo (API-compatible) start
I0000 ... distributed_point_function.cc:596] Highway is in AVX2 mode
Keys generated successfully.
Evaluation results (sum of shares, modulo 2^32):
x = 0  share0 = ...  share1 = ...  sum_raw = 4294967296  sum_mod = 0    <-- expected 0
x = 1  share0 = ...  share1 = ...  sum_raw = 4294967296  sum_mod = 0    <-- expected 0
x = 2  ...
x = 3  share0 = ...  share1 = ...  sum_raw = 4294967338  sum_mod = 42   <-- expected beta = 42
...
Simple DPF demo finished successfully.
```

可以看到：

- 对除 3 以外的所有点，`sum_mod = 0`；
- 对 `x=3`，`sum_mod = 42`，与设定的 `beta` 一致。

---

## 5. 如何把这个 demo 改成你自己的需求？

你可以在 `src/main.cc` 里修改以下部分以实验不同场景：

1. **修改定义域大小：**

   ```c++
   constexpr int kLogDomainSize = 10;  // 域大小改为 2^10 = 1024
   ```

2. **修改特殊点和 beta：**

   ```c++
   constexpr int64_t kAlpha = 123;
   constexpr int64_t kBeta = 1000;
   ```

3. **修改值的 bit 宽度（环大小）：**

   ```c++
   constexpr int kValueBits = 64;  // 使用 64 位整型环 Z_{2^64}
   ```

   同时将 `using T = uint32_t;` 改为：

   ```c++
   using T = uint64_t;
   ```

4. **只评估部分点：**

   将构造 `points` 的那一段替换成你感兴趣的点集合即可，例如只评估 `{0, alpha}`：

   ```c++
   std::vector<absl::uint128> points = {
       absl::uint128(0),
       absl::uint128(kAlpha),
   };
   ```

---

## 6. 与上游 `distributed_point_functions` 的关系

本 demo 显示了两件事情：

1. **如何以 “子模块 + Bazel 外部依赖” 的方式复用现有 Bazel 仓库：**

   - `third_party/google_DPF` 作为 git submodule 引入；
   - 在 `WORKSPACE` 中通过 `local_repository` 或在 Bzlmod 中通过 `local_path_override` 接入；
   - 在 `src/BUILD` 中用：
     ```python
     deps = [
       "@distributed_point_functions//dpf:distributed_point_function",
     ]
     ```
     这样的写法来依赖对方的 `cc_library`。

2. **如何跟随上游 API 变更：**

   - demo 程序使用的是**当前版本** `dpf/distributed_point_function.h` 提供的接口：
     - `DpfParameters`（全局类型）
     - `CreateIncremental({parameters})`
     - `GenerateKeys(alpha, beta)`
     - `CreateEvaluationContext(key)`
     - `EvaluateAt<T>(key, level, points)`
   - 如果上游 API 未来再次更新，你只需要参考头文件文档修改 `main.cc` 中对应调用即可。

---

## 7. 反馈与扩展

如果你在：

- 复现 demo；
- 修改参数（域大小、bit 宽度、alpha/beta）；
- 或者把 DPF 集成到你自己的应用逻辑中

的过程中遇到问题，可以结合：

- 本仓库的 `src/main.cc`；
- 子模块 `third_party/google_DPF` 下的官方测试 / benchmark（如 `experiments/`）；

一起对照调试。

如果你希望进一步：

- 支持 **多层 DPF / 分层前缀评估**；
- 跟 `experiments/synthetic_data_benchmarks.cc` 那样读入真实 CSV 数据，做大规模评估；
- 或在 Windows / Linux 上做性能基准测试，

可以以本 demo 为起点，把更多 `google_DPF` 的 API 和用法搬到自己的工程中。
---

## my_dpf_app
My Demo App of DPF (Distributed Point Function)

## output of "bazel query"
```
//dpf:aes_128_fixed_key_hash
//dpf:distributed_point_function
//dpf:int_mod_n
//dpf:status_macros
//dpf:tuple
//dpf:xor_wrapper
```
