#include "../src/filter/bloomfilter.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

static const std::vector<std::string> kTestKeys = {"corekv", "corekv1", "corekv2"};

TEST(bloomFilter, CreateFilter) {
    std::unique_ptr<kv::FilterPolicy> filter_policy = std::make_unique<kv::BloomFilter>(30, 0.01);
    std::vector<std::string> tmp;
    for (const auto& item : kTestKeys) {
        tmp.emplace_back(item);
    }
    filter_policy->CreateFilter(&tmp[0], tmp.size());
    tmp.emplace_back("hardcore");
    for(const auto& item : tmp)
	{
		for (const auto& item : tmp) {
		std::cout << "[ key:" << item
			<< ", has_existed:" << filter_policy->KeyMayMatch(item, 0, 0) << " ]"
			<< std::endl;
		}
	}
}