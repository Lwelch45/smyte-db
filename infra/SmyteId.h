#ifndef INFRA_SMYTEID_H_
#define INFRA_SMYTEID_H_

#include <string>

#include "boost/endian/buffers.hpp"
#include "glog/logging.h"

namespace infra {

class SmyteId {
 public:
  // Total 64 bits for smyte id: Sign:1 TimeMs:40 Unique:10 Machine:13
  // 40 bits local machine increment
  static constexpr int kTimestampBits = 40;
  static constexpr int64_t kTimestampSize = 1L << kTimestampBits;
  static constexpr int64_t kTimestampEpoch = 1262304000000;  // 2010-01-01

  // 10 bits local machine increment
  static constexpr int kUniqueBits = 10;
  static constexpr int64_t kUniqueSize = 1L << kUniqueBits;

  // 13 bits machine id
  static constexpr int kMachineBits = 13;
  static constexpr int64_t kMachineSize = 1L << kMachineBits;
  static constexpr int64_t kMachineBase = kMachineSize - 1024;  // use the last 1024 number

  // Configurations for virtual sharding
  static constexpr int kVirtualShardBits = 10;
  static constexpr int kVirtualShardCount = 1L << kVirtualShardBits;

  // Generate a smyte id deterministically from kafka offset, timestamp and virtual shard.
  static SmyteId generateFromKafka(int64_t kafkaOffset, int64_t timestampMs, int virtualShard) {
    int64_t shiftedTimestamp = timestampMs - SmyteId::kTimestampEpoch;
    CHECK(shiftedTimestamp >= 0 && shiftedTimestamp < kTimestampSize)
        << "timestamp " << timestampMs << " for kafka offset " << kafkaOffset << " is out of range";
    CHECK(virtualShard >= 0 && virtualShard < kVirtualShardCount)
        << "virtual shard " << virtualShard << " for kafka offset " << kafkaOffset << " is out of range";
    int64_t unique = kafkaOffset % SmyteId::kUniqueSize;
    int64_t machine = SmyteId::kMachineBase + virtualShard;
    int64_t smyteId = (((shiftedTimestamp << SmyteId::kUniqueBits) + unique) << SmyteId::kMachineBits) + machine;

    return SmyteId(smyteId);
  }

  explicit SmyteId(int64_t smyteId) : smyteId_(smyteId) {}

  explicit SmyteId(const std::string& smyteId)
      : smyteId_(boost::endian::detail::load_big_endian<int64_t, sizeof(int64_t)>(smyteId.data())) {}

  // Encode smyte id as an an 8-byte string. Use big endian so that it can be sorted in numerical order.
  std::string asBinary() const {
    std::string result;  // any reasonable compiler would apply Return Value Optimization here
    appendAsBinary(&result);
    return result;
  }

  // Append the smyte id to the output as an 8-byte string. Use big endian so that it can be sorted in numerical order.
  void appendAsBinary(std::string* out) const {
    boost::endian::big_int64_buf_t value(smyteId_);
    out->append(value.data(), sizeof(int64_t));
  }

  int getShardIndex(int shardCount) const {
    return (smyteId_ ^ (smyteId_ >> kMachineBits)) % shardCount;
  }

  // Return the virtual shard number [0, 1024) and -1 for error.
  int getVirtualShard() const {
    int64_t machine = smyteId_ % kMachineSize;
    if (machine < kMachineBase) return -1;
    return machine % kVirtualShardCount;
  }

  bool operator==(const SmyteId& rhs) const {
    return smyteId_ == rhs.smyteId_;
  }

  int64_t timestamp() const {
    return  (smyteId_ >> (SmyteId::kUniqueBits +  SmyteId::kMachineBits)) + SmyteId::kTimestampEpoch;
  }

 private:
  int64_t smyteId_;
};

}  // namespace infra

#endif  // INFRA_SMYTEID_H_
