#include "bloomfilter.h"
#include "../utils/hash_util.h"
#include <cmath>

namespace kv {
    BloomFilter::BloomFilter(int32_t bits_per_key_) : bits_per_key_(bits_per_key_) {
        CalcHashNum();
    }
    BloomFilter::BloomFilter(int32_t entries_num, float positive) {
        if (entries_num > 0) {
            CalcBloomBitsPerKey(entries_num, positive);
        }
        CalcHashNum();
    }
    // bits_per_key_ = m / n
    void BloomFilter::CalcBloomBitsPerKey(int32_t entries_num, float positive) {
        float size = -1 * entries_num * logf(positive) / powf(0.69314718056, 2);
        bits_per_key_ = static_cast<int32_t>(ceilf(size / entries_num));
    }
    // k = ln2 * m / n
    void BloomFilter::CalcHashNum() {
        k_ = static_cast<int32_t>(ceilf(0.69314718056 * bits_per_key_));
        k_ = k_ > 30 ? 30 : k_;
        k_ = k_ < 1 ? 1 : k_;
    }
    // 当前过滤器的名字
    const char* BloomFilter::Name() {
        return "generic_bloomfilter";
    }
    
    /* 创建过滤器
       给长度为n的keys集合（可能有重复）创建一个过滤策略
       并将策略序列化为string追加到布隆过滤器的成员变量bloomfilter_data_之后
     */
    void BloomFilter::CreateFilter(const std::string* keys, int n) {
        if (n <= 0 || !keys) {
            return;
        }
        int32_t bits = n * bits_per_key_;
        bits = bits < 64 ? 64 : bits;
        // 保证bits是8的倍数 向上取整
        int32_t bytes = (bits + 7) / 8;
        bits = bytes * 8;
        const int init_size_ = bloomfilter_data_.size();
        bloomfilter_data_.resize(init_size_ + bytes, 0);
        // 将bloomfilter_data_转成数组方便使用
        char* array = &(bloomfilter_data_)[init_size_];
        // 对于每个key，计算出k_个哈希值，然后将对应的bit位置为1
        for (int i = 0; i < n; i++) {
            // double-hash 仅使用一个hash函数来生成k个hash值,近似等价于使用k个hash函数的效果
            uint32_t hash_val = hash_util::SimMurMurHash(keys[i].data(), keys[i].size());
            // 循环右移17bits作为步长
            uint32_t delta = (hash_val >> 17) | (hash_val << 15);
            for (int j = 0; j < k_; j++) {
                uint32_t bitpos = hash_val % bits;
                /* bitPos/ 8得到在哪一行
                   bitPos % 8得到在哪一列
                   然后将1左移动n位，并且与当前位置 进行位运算
                 */ 
                array[bitpos / 8] |= (1 << (bitpos % 8));
                hash_val += delta;
            }
        }
    }
    // 判断key是否在过滤器中
    bool BloomFilter::KeyMayMatch(const std::string& key, int32_t start_pos, int32_t len) {
        if (key.empty() || bloomfilter_data_.empty()) {
            return false;
        }
        const size_t total_size = bloomfilter_data_.size();
        if (total_size < start_pos) {
            return false;
        }
        if (len == 0) {
            len = total_size - start_pos;
        }
        std::string current_bloomfilter_data = bloomfilter_data_.substr(start_pos, len);
        const char* cur_array = current_bloomfilter_data.data();
        const int32_t bits = len * 8;
        if (k_ > 30) return true;
        uint32_t hash_val = hash_util::SimMurMurHash(key.data(), key.size());
        uint32_t delta = (hash_val >> 17) | (hash_val << 15);
        for (int32_t i = 0; i < k_; i++) {
            const uint32_t bitpos = hash_val % bits;
            if ((cur_array[bitpos / 8] & (1 << (bitpos % 8))) == 0) {
                return false;
            }
            hash_val += delta;
        }
        return true;
    }
}