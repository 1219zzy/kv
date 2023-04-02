#pragma once
#include <string>

namespace kv {
    class FilterPolicy {
    public:
        FilterPolicy() = default;
        virtual ~FilterPolicy() = default;
        // 过滤策略的名字，用来唯一标识该Filter持久化、载入内存时的编码方法
        virtual const char* Name() = 0;
        /* 创建过滤器
           给长度为n的keys集合(有可能重复)创建一个过滤策略
        */
        virtual void CreateFilter(const std::string* keys, int n) = 0;
        /* 判断key是否在过滤器中
           返回true表示key可能在集合中，返回false表示key一定不在集合中
        */
        virtual bool KeyMayMatch(const std::string& key, int32_t start_pos, int32_t len) = 0;
        virtual const std::string& Data() = 0;
        // 返回当前过滤器底层对象的空间占用大小
        virtual uint32_t Size() = 0;
    };
}