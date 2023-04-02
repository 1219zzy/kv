#pragma once
#include <math.h>
#include <cstdint>
#include <ctime>
#include <random>
namespace KV {
    class RandomUtil final {
        public:
            ~RandomUtil() = default;
            RandomUtil(uint32_t seed = 0) : seed_val_(seed) {
                if (seed_val_ > 0) {
                    engine_.seed(seed_val_);
                } else {
                    engine_.seed(std::time(0));
                }
            }
            int64_t GetSimpleRandomNum() {return rand();}
            int64_t GetRandomNum() {return engine_();}
        private:
            uint32_t seed_val_ = 0;
            std::default_random_engine engine_;
    };
}