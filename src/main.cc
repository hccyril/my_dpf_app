#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/status/statusor.h"
#include "dpf/distributed_point_function.h"

using ::distributed_point_functions::DistributedPointFunction;
using ::distributed_point_functions::DpfKey;
using ::distributed_point_functions::DpfParameters;
using ::distributed_point_functions::EvaluationContext;

int main(int argc, char** argv) {
  std::cout << "Simple DPF demo (API-compatible) start" << std::endl;

  // 一维 DPF 设置：
  //   定义域大小 = 2^3 = 8
  //   特殊点 alpha = 3
  //   在 alpha 处的值 beta = 42，其它点为 0
  constexpr int kLogDomainSize = 3;
  constexpr int64_t kDomainSize = 1 << kLogDomainSize;
  constexpr int64_t kAlpha = 3;
  constexpr int64_t kBeta = 42;
  constexpr int kValueBits = 32;  // 聚合值位数

  // 1. 构造一维 DPF 的参数
  DpfParameters parameters;
  parameters.set_log_domain_size(kLogDomainSize);
  parameters.mutable_value_type()->mutable_integer()->set_bitsize(kValueBits);

  // 2. 创建一维增量 DPF 实例
  absl::StatusOr<std::unique_ptr<DistributedPointFunction>> dpf_or =
      DistributedPointFunction::CreateIncremental({parameters});
  if (!dpf_or.ok()) {
    std::cerr << "Failed to create DistributedPointFunction: "
              << dpf_or.status() << std::endl;
    return 1;
  }
  std::unique_ptr<DistributedPointFunction> dpf = std::move(dpf_or.value());

  // 3. 生成两把密钥 key0 / key1
  //   新版 API 通常提供 GenerateKeysIncremental(alpha, betas)
  //   也可以使用 GenerateKeys(alpha, value) 这种简单形式。
  absl::uint128 alpha = absl::uint128(kAlpha);
  absl::uint128 beta = absl::uint128(kBeta);

  absl::StatusOr<std::pair<DpfKey, DpfKey>> keys_or =
      dpf->GenerateKeys(alpha, beta);  // 使用简单版接口
  if (!keys_or.ok()) {
    std::cerr << "Failed to generate DPF keys: " << keys_or.status()
              << std::endl;
    return 1;
  }
  auto [key0, key1] = std::move(keys_or.value());
  std::cout << "Keys generated successfully." << std::endl;

  // 4. 为每一方创建 EvaluationContext
  absl::StatusOr<EvaluationContext> ctx0_or =
      dpf->CreateEvaluationContext(key0);
  absl::StatusOr<EvaluationContext> ctx1_or =
      dpf->CreateEvaluationContext(key1);

  if (!ctx0_or.ok() || !ctx1_or.ok()) {
    std::cerr << "Failed to create evaluation contexts." << std::endl;
    if (!ctx0_or.ok()) {
      std::cerr << "ctx0 status: " << ctx0_or.status() << std::endl;
    }
    if (!ctx1_or.ok()) {
      std::cerr << "ctx1 status: " << ctx1_or.status() << std::endl;
    }
    return 1;
  }

  EvaluationContext ctx0 = std::move(ctx0_or.value());
  EvaluationContext ctx1 = std::move(ctx1_or.value());

  // 5. 在整个定义域 [0, kDomainSize) 上评估 share 并相加
  //    新版接口通常提供 EvaluateAt<T>(key, level, points) 或
  //    EvaluateAt<T>(key, level, start, size) 这类形式。
  //
  // 这里我们用 EvaluateAt<T>(key, /*level=*/0, points) 的形式：
  std::vector<absl::uint128> points;
  points.reserve(kDomainSize);
  for (int64_t x = 0; x < kDomainSize; ++x) {
    points.push_back(absl::uint128(x));
  }

  using T = uint32_t;  // 输出类型与 bitsize 对应，这里用 32 位无符号整数

  absl::StatusOr<std::vector<T>> shares0_or =
      dpf->EvaluateAt<T>(key0, /*level=*/0, points);
  absl::StatusOr<std::vector<T>> shares1_or =
      dpf->EvaluateAt<T>(key1, /*level=*/0, points);

  if (!shares0_or.ok() || !shares1_or.ok()) {
    std::cerr << "Failed to evaluate DPF shares." << std::endl;
    if (!shares0_or.ok()) {
      std::cerr << "shares0 status: " << shares0_or.status() << std::endl;
    }
    if (!shares1_or.ok()) {
      std::cerr << "shares1 status: " << shares1_or.status() << std::endl;
    }
    return 1;
  }

  std::vector<T> shares0 = std::move(shares0_or.value());
  std::vector<T> shares1 = std::move(shares1_or.value());

  std::cout << "Evaluation results (sum of shares):" << std::endl;
  for (int64_t x = 0; x < kDomainSize; ++x) {
    uint64_t sum = static_cast<uint64_t>(shares0[x]) +
                   static_cast<uint64_t>(shares1[x]);

    std::cout << "x = " << x
              << "  share0 = " << static_cast<uint64_t>(shares0[x])
              << "  share1 = " << static_cast<uint64_t>(shares1[x])
              << "  sum = " << sum;

    if (x == kAlpha) {
      std::cout << "  <-- expected beta = " << kBeta;
    } else {
      std::cout << "  <-- expected 0";
    }
    std::cout << std::endl;
  }

  std::cout << "Simple DPF demo finished successfully." << std::endl;
  return 0;
}