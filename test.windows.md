# 完整步骤 (Win10, no WSL)
下面我按「完全在 Windows 里，用 Bazel + 自己的工程，依赖 `google/distributed_point_functions`」给你一整套步骤。你可以一步步照做，如果中途有任何一步报错，把命令和完整输出贴出来，我再帮你改。

我会假设你想用 **C++** 写自己的小测试程序。

---

## 一、Windows 端的前置准备

### 1. 安装 Bazelisk（推荐，而不是直接装 Bazel）

1. 在浏览器打开 [Bazelisk Releases](https://github.com/bazelbuild/bazelisk/releases)。
2. 下载适用于 Windows 的可执行文件，一般叫：

   - `bazelisk-windows-amd64.exe`

3. 把它重命名为 `bazel.exe`（不是必须，但这样命令更简单）。
4. 放到一个在系统 `PATH` 里的目录，比如：

   - `C:\Tools\bazel\bazel.exe`
   - 然后把 `C:\Tools\bazel` 加到系统环境变量 `PATH` 里。

5. 打开 **PowerShell**（或 CMD），执行：

   ```powershell
   bazel version
   ```

   能看到版本信息就说明 OK。

---

### 2. 安装 C++ 编译环境

两种主流方案：MSVC（VS Build Tools）或 MinGW。  
这个项目是 Google 风格，大概率走 **MSVC** 更顺。

#### 2.1 安装 Visual Studio Build Tools（推荐）

1. 下载并安装 [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/)。
2. 安装时勾选：

   - 「使用 C++ 的桌面开发」工作负载（Desktop development with C++）
   - 确保勾选 Windows 10 或 11 SDK。

3. 安装完成后，建议在「x64 Native Tools Command Prompt for VS 20xx」里运行 Bazel，这样环境变量会自动配置好。如果你更喜欢 PowerShell：

   - 可以先在「Developer Command Prompt」里执行一次 `where cl` 看路径；
   - 然后在 PowerShell 里用 `VsDevCmd.bat` 初始化环境（可选，第一阶段不纠结，直接在 Developer CMD 里跑也行）。

---

### 3. 克隆原项目到一个固定路径

在 Windows（不是 WSL）里，找一个目录，例如 `D:\code`：

```powershell
cd D:\code
git clone https://github.com/google/distributed_point_functions.git
```

记住这个路径，比如以后用：

- 原库路径：`D:\code\distributed_point_functions`

---

## 二、新建你自己的 Bazel 工程（Windows 本地，含 submodule）

下面假设你在 Windows 上的代码目录是 `D:\code`，你 fork 的仓库是 `https://github.com/hccyril/google_DPF.git`。

### 1. 创建你的工程目录，并添加 submodule

```powershell
cd D:\code

# 1. 创建你的工程目录
mkdir my_dpf_app
cd my_dpf_app

# 2. 初始化 git 仓库（因为你要加 submodule）
git init

# 3. 创建存放第三方代码的目录（可自定义名字）
mkdir third_party

# 4. 把你 fork 的 DPF 仓库作为 submodule 加入
cd third_party
git submodule add https://github.com/hccyril/google_DPF.git
cd ..
```

此时目录结构大致是：

```
D:\code\my_dpf_app
  .git/
  .gitmodules
  third_party/
    google_DPF/          <- 这里就是你的 fork 仓库（submodule）
      WORKSPACE / BUILD / dpf/ ...
```

**注意**：之后你在 `my_dpf_app` 里 `git clone` / `git pull` 时，记得使用 `git submodule update --init --recursive`，不过这是 git 层面的事，和 Bazel 无关。

---

## 三、在 WORKSPACE 里用相对路径接入 `google_DPF`（fork 仓库）

在 `D:\code\my_dpf_app` 下新建 `WORKSPACE` 文件：

````python name=WORKSPACE
workspace(name = "my_dpf_app")

# 使用相对路径指向 submodule 目录
local_repository(
    name = "google_DPF",
    path = "third_party/google_DPF",
)
````

说明：

- `name = "google_DPF"` 是日后在 Bazel 里引用这个仓库的 “外部名”，比如 `@google_DPF//dpf:xxx`；
- `path = "third_party/google_DPF"` 是相对 `WORKSPACE` 所在目录的路径；
- 你的 fork 仓库 `hccyril/google_DPF` 自己内部已经是一个 Bazel workspace（有 `WORKSPACE` 和一堆 `BUILD`），`local_repository` 会把它当成一个完整外部仓库来使用。

到这里，「用相对路径把 fork 仓库接入」已经完成。

---

## 四、在 fork 仓库中找到 DPF 的 C++ 库 target

**这一步只查一次**，确定你应该依赖哪个 `cc_library`。

1. 打开一个终端（Windows，建议 VS 的 Developer Command Prompt 或配置好 MSVC 的 PowerShell），进入 submodule 目录：

   ```powershell
   cd D:\code\my_dpf_app\third_party\google_DPF
   ```

2. 运行：

   ```powershell
   bazel query "kind(cc_library, //dpf:*)"
   ```

   你会看到类似：

   ```text
   //dpf:dpf
   //dpf:dpf_internal
   //dpf:dpf_proto
   ...
   ```

   上面只是示例，**以你实际输出为准**。

3. 选择一个你认为是「对外主库」的 target，假设是：

   ```text
   //dpf:dpf
   ```

   那么等会儿在你自己的工程中，就会用到：

   ```python
   deps = [
       "@google_DPF//dpf:dpf",
   ]
   ```

> 如果你不确定哪个库最合适，可以直接把这条 `bazel query` 的输出粘贴给我，我可以帮你选。

---

## 五、在你自己的工程中写一个 C++ demo

现在回到你自己的工程目录：

```powershell
cd D:\code\my_dpf_app
mkdir main
```

### 1. 写 `main.cc`（先用结构正确的示例，稍后可以替换具体 API）

创建文件 `main/main.cc`，内容示例（注意这里只是示意结构，API 需要你根据 fork 仓库实际内容调整）：

```cpp
```cpp name=main/main.cc
#include <iostream>

// TODO: 根据 google_DPF 仓库里实际的头文件路径修改 include。
// 假设在 third_party/google_DPF 里有 dpf/distributed_point_function.h 这样的头文件：
//
//   D:\code\my_dpf_app\third_party\google_DPF\dpf\distributed_point_function.h
//
// 那么这里写：
#include "dpf/distributed_point_function.h"

int main(int argc, char** argv) {
  std::cout << "My DPF demo (from forked google_DPF) start" << std::endl;

  // 以下为“伪代码示例”，说明如何使用 DPF：
  //
  // - 设定义域为 {0, ..., 2^n - 1}，我们选 n = 4 => 域大小 16
  // - 设特殊点 alpha = 5，在该点处的值为 beta = 42
  //
  // 实际的类名 / 函数名请根据 google_DPF 仓库里 DPF 的头文件和实现来替换。

  int domain_bits = 4;  // log2(domain_size)，定义域大小 = 1 << domain_bits
  int alpha = 5;
  int64_t beta = 42;

  // 伪代码（你需要根据实际 API 填写）：
  //
  // using distributed_point_functions::DistributedPointFunction;
  //
  // // 根据库的接口创建 DPF 对象（可能是工厂函数或构造函数）
  // auto dpf = DistributedPointFunction::Create(domain_bits).value();
  //
  // // 生成两把密钥（两方持有）
  // auto key_pair = dpf.GenerateKeys(alpha, beta).value();
  // auto key0 = key_pair.first;
  // auto key1 = key_pair.second;
  //
  // // 在整个定义域上评估 share 并求和
  // int domain_size = 1 << domain_bits;
  // for (int x = 0; x < domain_size; ++x) {
  //   auto v0 = dpf.Evaluate(key0, x).value();
  //   auto v1 = dpf.Evaluate(key1, x).value();
  //
  //   auto sum = v0 + v1;  // 在 x == alpha 时应为 beta，其它为 0
  //   std::cout << "x = " << x
  //             << "  f0(x) = " << v0
  //             << "  f1(x) = " << v1
  //             << "  sum = " << sum
  //             << std::endl;
  // }

  std::cout << "My DPF demo finished (placeholder, please fill real API)." << std::endl;
  return 0;
}
```

你真正需要做的是：

1. 在 `third_party/google_DPF` 里找到 DPF 的头文件和类/函数（比如 `dpf/distributed_point_function.h`，`namespace distributed_point_functions` 等）；  
2. 用真实的 API 替换上述伪代码部分。  
   如果你不熟，可以把相关 `.h` 文件的关键内容贴给我，我帮你写一个实际可编译的版本。

---

### 2. 写 `BUILD` 文件，声明依赖 fork 仓库中的 DPF 库

在 `my_dpf_app/main` 下创建 `BUILD` 文件：

```python
```python name=main/BUILD
cc_binary(
    name = "my_dpf_demo",
    srcs = ["main.cc"],
    deps = [
        # 依赖你 fork 仓库（以 submodule 形式引入）的 DPF C++ 库。
        # "@google_DPF" 对应的是 WORKSPACE 里 local_repository 的 name，
        # "//dpf:dpf" 是 fork 仓库内部的 package + target 名。
        "@google_DPF//dpf:dpf",
        # 如果实际 target 叫 "@google_DPF//dpf:distributed_point_functions"
        # 或 "@google_DPF//dpf:dpf_lib" 等，请按 bazel query 输出替换。
    ],
)
```

**要点回顾：**

- `@google_DPF`：来自你 `WORKSPACE` 中的：

  ```python
  local_repository(
      name = "google_DPF",
      path = "third_party/google_DPF",
  )
  ```

- `//dpf:dpf`：来自你在 submodule 仓库中运行的：

  ```powershell
  bazel query "kind(cc_library, //dpf:*)"
  ```

  输出的某个 `cc_library` target。

---

## 六、在 Windows 上构建并运行你的 demo

在 Windows 终端（已安装好 VS Build Tools / MSVC，并正确配置环境）中：

```powershell
cd D:\code\my_dpf_app

# 第一次构建，建议限制 jobs 数，避免机器太多并发卡死
bazel build //main:my_dpf_demo --jobs=4
```

如果成功，会看到：

```text
INFO: Analyzed target //main:my_dpf_demo (...).
INFO: Found 1 target...
Target //main:my_dpf_demo up-to-date:
  bazel-bin/main/my_dpf_demo.exe
INFO: Build completed successfully, ...
```

然后运行：

```powershell
bazel run //main:my_dpf_demo --jobs=4
```

如果你的 demo 需要参数，例如 `--alpha`、`--beta` 等，可以这样：

```powershell
bazel run //main:my_dpf_demo --jobs=4 -- --alpha=5 --beta=42
```

---

## 七、推荐你接下来做的几步

1. **验证 submodule + local_repository 是否接入成功**

   在 `D:\code\my_dpf_app` 目录执行：

   ```powershell
   bazel query "@google_DPF//dpf:all"
   ```

   如果能看到一堆 target（包括你刚才在 fork 仓库里用到的那个 `cc_library`），说明外部仓库接入无误。

2. **确认你依赖的是正确的库**

   在 `D:\code\my_dpf_app\third_party\google_DPF` 里执行：

   ```powershell
   bazel query "kind(cc_library, //dpf:*)"
   ```

   把结果贴出来，我可以帮你确认 `@google_DPF//dpf:xxx` 里应该用哪个。

3. **替换 `main.cc` 中的伪代码为真实调用**

   - 在 fork 仓库里找到 DPF 的主头文件（`.h`）和实现（`.cc`），了解它的 API；
   - 把头文件路径、类名、函数签名发给我，我可以直接为你写一个“真实可编译”的 demo 版本，你只需要再 `bazel build` + `bazel run` 即可。

---

如果你现在方便，可以先：

```powershell
cd D:\code\my_dpf_app\third_party\google_DPF
bazel query "kind(cc_library, //dpf:*)"
```

把输出贴过来，我帮你把 `main/BUILD` 和 `main/main.cc` 里的依赖名字写成与你 fork 仓库完全匹配的版本。


# Answer by AI
很好，有了这些信息我们就可以把依赖和 demo 写完整了。

你当前状态：

- fork 仓库作为 submodule 在：`third_party/google_DPF`
- 你自己的 demo 源文件在：`src/main.cc`
- DPF 相关 C++ 库 target 有：
  - `//dpf:aes_128_fixed_key_hash`
  - `//dpf:distributed_point_function`  ✅ 这就是我们要依赖的主库
  - `//dpf:int_mod_n`
  - `//dpf:status_macros`
  - `//dpf:tuple`
  - `//dpf:xor_wrapper`

下面我直接给出你项目中需要的几个文件的完整内容（按你的目录结构来），包括：

1. `WORKSPACE`
2. `src/BUILD`
3. `src/main.cc`（带真实的 include 和可运行的示例代码）

你只需要按下面文件内容创建 / 覆盖，然后在项目根目录执行 `bazel build` / `bazel run`。

---

## 1. `WORKSPACE`

放在项目根目录：`D:\UserData\CODE\GIT\my_dpf_app\WORKSPACE`

````python name=WORKSPACE
workspace(name = "my_dpf_app")

# 使用相对路径指向你 fork 的 google_DPF 仓库（作为 git submodule 放在 third_party 目录下）
local_repository(
    name = "google_DPF",
    path = "third_party/google_DPF",
)
````

---

## 2. `src/BUILD`

你现在 `main.cc` 在 `src` 目录下，我们就在 `src` 目录放一个 `BUILD` 文件。

路径：`D:\UserData\CODE\GIT\my_dpf_app\src\BUILD`

```python name=src/BUILD
cc_binary(
    name = "my_dpf_demo",
    srcs = ["main.cc"],
    deps = [
        # 依赖 fork 仓库中的 DPF 主库。
        # "@google_DPF" 对应 WORKSPACE 中 local_repository 的 name。
        # "//dpf:distributed_point_function" 来自你刚刚的 bazel query 结果。
        "@google_DPF//dpf:distributed_point_function",
    ],
)
```

---

## 3. `src/main.cc`

接下来是一个可以直接编译运行的 Demo，使用 DPF 功能生成两方密钥并对一个点进行分布式评估。  
我们直接用这个仓库里暴露的 C++ API（`distributed_point_function.h`）。

路径：`D:\UserData\CODE\GIT\my_dpf_app\src\main.cc`

```cpp
```cpp name=src/main.cc
#include <iostream>

#include "absl/numeric/int128.h"
#include "absl/status/statusor.h"
#include "dpf/distributed_point_function.h"

using ::distributed_point_functions::DistributedPointFunction;
using ::distributed_point_functions::EvaluationContext;

int main(int argc, char** argv) {
  std::cout << "My DPF demo (using forked google_DPF) start" << std::endl;

  // 1. 定义简单的一维 DPF：
  //    - 域大小 = 8（点为 0..7）
  //    - 特殊点 alpha = 3
  //    - 在 alpha 处的值 beta = 42，其它点值为 0
  constexpr int64_t domain_size = 8;
  constexpr int64_t alpha = 3;
  constexpr int64_t beta = 42;

  // 2. 构造一维 DPF 模板（template）
  //    这是生成密钥之前必须要做的步骤，告诉库我们希望构造什么样的 DPF。
  absl::StatusOr<std::unique_ptr<DistributedPointFunction>> dpf_or =
      DistributedPointFunction::CreateIncremental(
          {DistributedPointFunction::DpfParameters{
              /*log_domain_size=*/3,  // log2(8) = 3
              /*value_type=*/DistributedPointFunction::ValueType::kInt64}});

  if (!dpf_or.ok()) {
    std::cerr << "Failed to create DPF: " << dpf_or.status() << std::endl;
    return 1;
  }
  std::unique_ptr<DistributedPointFunction> dpf = std::move(dpf_or.value());

  // 3. 基于这个 DPF 模板，生成两把密钥 key0 / key1
  absl::StatusOr<std::pair<std::string, std::string>> keys_or =
      dpf->GenerateKeys({alpha}, {absl::int128(beta)});

  if (!keys_or.ok()) {
    std::cerr << "Failed to generate DPF keys: " << keys_or.status()
              << std::endl;
    return 1;
  }
  auto [key0, key1] = std::move(keys_or.value());

  std::cout << "Keys generated successfully." << std::endl;

  // 4. 为每一方创建一个 EvaluationContext，用于增量评估
  absl::StatusOr<std::unique_ptr<EvaluationContext>> ctx0_or =
      dpf->CreateEvaluationContext(key0, /*level=*/0);
  absl::StatusOr<std::unique_ptr<EvaluationContext>> ctx1_or =
      dpf->CreateEvaluationContext(key1, /*level=*/0);

  if (!ctx0_or.ok() || !ctx1_or.ok()) {
    std::cerr << "Failed to create evaluation contexts: "
              << (ctx0_or.ok() ? "" : ctx0_or.status().ToString()) << " "
              << (ctx1_or.ok() ? "" : ctx1_or.status().ToString()) << std::endl;
    return 1;
  }

  std::unique_ptr<EvaluationContext> ctx0 = std::move(ctx0_or.value());
  std::unique_ptr<EvaluationContext> ctx1 = std::move(ctx1_or.value());

  // 5. 在整个域 [0, domain_size) 上评估 DPF 的 share，并把 share 相加
  // 每一方分别得到长度为 domain_size 的向量 shares0 / shares1。
  absl::StatusOr<std::vector<absl::int128>> shares0_or =
      dpf->EvaluateAtLevel(*ctx0, /*level=*/0, domain_size);
  absl::StatusOr<std::vector<absl::int128>> shares1_or =
      dpf->EvaluateAtLevel(*ctx1, /*level=*/0, domain_size);

  if (!shares0_or.ok() || !shares1_or.ok()) {
    std::cerr << "Failed to evaluate DPF shares: "
              << (shares0_or.ok() ? "" : shares0_or.status().ToString()) << " "
              << (shares1_or.ok() ? "" : shares1_or.status().ToString()) << std::endl;
    return 1;
  }

  std::vector<absl::int128> shares0 = std::move(shares0_or.value());
  std::vector<absl::int128> shares1 = std::move(shares1_or.value());

  std::cout << "Evaluation results (sum of shares):" << std::endl;
  for (int64_t x = 0; x < domain_size; ++x) {
    absl::int128 sum = shares0[x] + shares1[x];

    // 打印成 int64 方便看（我们在构造时就是用 int64 范围的值）
    int64_t sum_i64 = static_cast<int64_t>(sum);

    std::cout << "x = " << x
              << "  share0 = " << static_cast<int64_t>(shares0[x])
              << "  share1 = " << static_cast<int64_t>(shares1[x])
              << "  sum = " << sum_i64;

    if (x == alpha) {
      std::cout << "  <-- should equal beta = " << beta;
    } else {
      std::cout << "  <-- should equal 0";
    }
    std::cout << std::endl;
  }

  std::cout << "My DPF demo finished successfully." << std::endl;
  return 0;
}
```

说明：

- `#include "dpf/distributed_point_function.h"`：这是你 fork 仓库里 DPF 的核心头文件；
- 我们用的是该库的 “增量 DPF” 接口：
  - `DistributedPointFunction::CreateIncremental(...)`
  - `GenerateKeys(...)`
  - `CreateEvaluationContext(...)`
  - `EvaluateAtLevel(...)`
- 这里的 demo：
  - 域大小：8（`log_domain_size = 3`）；
  - 特殊点 `alpha = 3`；
  - 在 `x = 3` 处的值是 `beta = 42`；
  - 两方 share 在这个点上相加得到 42，其它点为 0。

---

## 4. 构建和运行

在 **Windows 终端**（VS Developer Command Prompt 或配置好的 PowerShell）中，进入项目根目录：

```powershell
cd D:\UserData\CODE\GIT\my_dpf_app

# 构建
bazel build //src:my_dpf_demo --jobs=4

# 运行
bazel run //src:my_dpf_demo --jobs=4
```

如果一切正常，你会看到类似输出：

- 前面是 Bazel 的构建信息；
- 程序输出类似：

```text
My DPF demo (using forked google_DPF) start
Keys generated successfully.
Evaluation results (sum of shares):
x = 0  share0 = ...  share1 = ...  sum = 0  <-- should equal 0
x = 1  ...
x = 3  share0 = ...  share1 = ...  sum = 42  <-- should equal beta = 42
...
My DPF demo finished successfully.
```

---

## 5. 如果出现问题，建议你贴这几样信息

如果构建或运行过程中有报错，请把：

1. 你执行的命令（比如 `bazel build //src:my_dpf_demo --jobs=4`）；
2. 终端完整错误输出（尤其是最后 30～50 行）；

原样贴出来，我可以针对具体错误帮你改：

- 如果是找不到头文件，可能是 include path 不对；
- 如果是 link error，可能是 deps 还缺其他 target（例如 `//dpf:aes_128_fixed_key_hash` 等，我可以补上）；
- 如果是 Windows/MSVC 特有的问题（比如某些宏冲突或 /EH /MD 之类选项），也可以一起排。

按上面的文件内容直接复制过去，大概率能一次构建通过；如果不行，我们就根据错误信息微调。