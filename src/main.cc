#include <iostream>

#include "absl/numeric/int128.h"
#include "absl/status/statusor.h"
#include "dpf/distributed_point_function.h"

using ::distributed_point_functions::DistributedPointFunction;
using ::distributed_point_functions::EvaluationContext;

int main(int argc, char** argv) {
  std::cout << "My DPF demo (using forked google_dpf) start" << std::endl;

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
