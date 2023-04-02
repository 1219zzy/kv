#include "filter_policy.h"

namespace kv {
    class BloomFilter final : public FilterPolicy {
    public:
        BloomFilter(int32_t bits_per_key_);
        BloomFilter(int32_t entries_num, float positive = 0.01);
        ~BloomFilter() = default;
        // 过滤策略的名字，用来唯一标识该Filter持久化、载入内存时的编码方法
        const char* Name() override;
        // 创建过滤器
        void CreateFilter(const std::string* keys, int n) override;
        // 判断key是否在过滤器中
        bool KeyMayMatch(const std::string& key, int32_t start_pos, int32_t len) override;
        // 返回过滤器的数据
        const std::string& Data() override {return bloomfilter_data_;};
        // 返回当前过滤器底层对象的空间占用大小
        uint32_t Size() override {return bloomfilter_data_.size();};
    private:
        void CalcBloomBitsPerKey(int32_t entries_num, float positive);
        void CalcHashNum();
        // 每个key对应的bit位数
        int32_t bits_per_key_;
        // 哈希函数个数
        int32_t k_;
        // 用于存储过滤器的数据
        std::string bloomfilter_data_;
    };
} // namespace kv