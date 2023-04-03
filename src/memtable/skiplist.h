#pragma once

#include "../utils/random_utils.h"

#include <atomic>
#include <assert.h>
#include <stdint.h>
#include <iostream>

namespace kv {
    struct SkipListOption {
        static const int32_t kMaxHeight = 20;
        // 有多少概率被选中
        static const unsigned int kBranching = 4;
    };
    template <typename Key, typename KeyComparator, typename Allocator>
    class SkipList final{
    private:
        struct Node;
        
    public:
        explicit SkipList(KeyComparator comparator);
        SkipList(const SkipList&) = delete;
        
        void Insert(const Key& key);
        bool Contains(const Key& key) const;
        bool Equal(const Key& a, const Key& b) const {return (comparator_(a, b) == 0);}

        // 迭代skiplist，主要给Memtable中的MemIterator使用
        class Iterator {
            public:

                // Initialize an iterator over the specified list.
                explicit Iterator(const SkipList* list);

                // Returns true iff the iterator is positioned at a valid node.
                bool Valid() const;

                /* Returns the key at the current position.
                REQUIRES: Valid() 
                */
                const Key& key() const;

                /* Advances to the next position.
                REQUIRES: Valid() 
                */
                void Next();

                /* Advances to the previous position.
                REQUIRES: Valid() 
                */
                void Prev();

                /* Advance to the first entry with a key >= target
                */
                void Seek(const Key& target);

                /* Position at the first entry in list.
                Final state of iterator is Valid() iff list is not empty.
                */
                void SeekToFirst();
                
                /* Position at the last entry in list.
                Final state of iterator is Valid() iff list is not empty.
                */
                void SeekToLast();
            private:
                const SkipList* list_;
                Node* node_;
                // Intentionally copyable
        };
    private:
        /* 节点的构造函数
              参数key是节点的key
              参数height是节点的高度
           为什么只有构造函数没有析构函数？
              SkipList只有添加和查找操作，没有删除操作
              因为Node的内存是从arena_中分配的，所以不需要析构函数
         */
        Node* NewNode(const Key& key, int32_t height);
        // 生成一个随机的高度
        int32_t RandomHeight();
        // 返回当前skiplist的最大高度
        inline int32_t GetMaxHeight() {return cur_height_.load(std::memory_order_relaxed);}
        // 判断key是不是大于节点n的key，也就意味着如果存在key的节点，那么就会在节点n的后面
        bool KeyIsAfterNode(const Key& key, Node* n) {return n != nullptr && comparator_.};
        /* 在跳表中查找不小于给定Key的第一个值，如果没有找到则返回nullptr
           如果参数prev不为空，在查找过程中会记录查找路径上的节点（节点在各层中的前驱节点）
           如果是查找操作，则指定prev为nullptr
           如果要插入数据，需要传入一个尺寸合适的prev参数
         */
        Node* FindGreaterOrEqual(const Key& key, Node** prev);
        // 找到小于key中最大的key对应的节点
        Node* FindLessThan(const Key& key) const;
        // 返回skiplist中的最后一个节点
        Node* FindLast() const;
    private:
        KeyComparator comparator_; // 比较器
        Allocator arena_; // 内存管理对象
        Node* head_ = nullptr; // skiplist头节点
        std::atomic<int32_t> cur_height_; // 当前skiplist的高度（有效的层数）
        RandomUtils rnd_; // 随机数生成器
    };
    // 实现skipList的Node结构
    template <typename Key, typename KeyComparator, typename Allocator>
    struct SkipList<Key, KeyComparator, Allocator>::Node {
        // 构造函数只有Key类型，这个类型在Memtable中被定义为const char*，其实包含了key和value
        explicit Node(const Key& k) : key(k) {}
        const Key key;

        // 采用内存屏障的方式获取节点的下一个节点，其中n为高度
        Node* Next(int n) {
            assert(n >= 0);
            /* std::memory_order_acquire 用在 load 时
               保证同线程中该 load 之后的对相关内存读写语句不会被重排到 load 之前
               并且其他线程中对同样内存用了 store release 都对其可见 
            */
            return next_[n].load(std::memory_order_acquire);
        }

        // 采用内存屏障的方式设置节点的下一个节点，其中n为高度
        void SetNext(int n, Node* x) {
            assert(n >= 0);
            /* std::memory_order_release 用在 store 时
               保证同线程中该 store 之前的对相关内存读写语句不会被重排到 store 之后
               并且其他线程中对同样内存用了 load acquire 都对其可见 
            */
            next_[n].store(x, std::memory_order_release);
        }

        // 不带内存屏障版本的访问器
        Node* NoBarrier_Next(int n) {
            assert(n >= 0);
            /* std::memory_order_relaxed：不对重排做限制
               只保证相关共享内存访问的原子性
               无内存屏障的方式获取下一个Node，其中n为高度
             */
            return next_[n].load(std::memory_order_relaxed);
        }
        // 无内存屏障的方式设置集结点高度为n的下一个节点
        void NoBarrier_SetNext(int n, Node* x) {
            assert(n >= 0);
            next_[n].store(x, std::memory_order_relaxed);
        }
        private:
            /* 指针数组的长度即为该节点的 level，next_[0] 是最低层指针
               在C++中，new一个对象其实就是malloc(sizeof(type))大小的内存，然后再执行构造函数的过程，delete先执行析构函数再free内存
               有没有发现这是一个结构体？next_[1]正好在结构体的尾部，那么申请内存的时候如果多申请一些内存
               那么通过索引的方式&next_[n]的地址就是多出来的那部分空间，所以可知Node不是通过普通的new出来的
             */
            std::atomic<Node*> next_[1];

    };
    // 节点的构造函数 NewNode
    template <typename Key, typename KeyComparator, typename Allocator>
    typename SkipList<Key, KeyComparator, Allocator>::Node* SkipList<Key, KeyComparator, Allocator>::NewNode(const Key& key, int32_t height) {
        // 将Allocate替换成AllocateAligned，可以将Node的内存对齐到8字节
        char* node_mem = arena_.AllocateAligned(sizeof(Node) + sizeof(std::atomic<Node*>) * (height - 1));
        return new (node_mem) Node(key);
    }
    // 生成一个随机的高度 RandomHeight()
    template <typename Key, typename KeyComparator, typename Allocator>
    int32_t SkipList<Key, KeyComparator, Allocator>::RandomHeight() {
        // 生成一个随机数
        int32_t height = 1;
        while (height < SkipListOption::kMaxHeight && rnd_.GetSimpleRandomNum() % SkipListOption::kBranching == 0) {
            height++;
        }
        assert(height > 0);
        assert(height <= kMaxHeight);
        return height;
    }
    // 找到第一个大于等于给定的key的节点 FindGreaterOrEqual(const Key& key, Node** prev)
    template <typename Key, typename KeyComparator, typename Allocator>
    typename SkipList<Key, KeyComparator, Allocator>::Node* SkipList<Key, KeyComparator, Allocator>::FindGreaterOrEqual(const Key& key, Node** prev) {
        // 从头节点开始
        Node* x = head_;
        // 从最高层开始
        int32_t level = GetMaxHeight() - 1;
        while (true) {
            // 该层的下一个节点
            Node* next = x->Next(level);
            if (KeyIsAfterNode(key, next)) {
                // Keep searching in this list
                x = next;
            } else {
                /* 降低高度，继续搜索
                   输出当前高度的前一个节点
                 */
                if (prev != nullptr) {
                    prev[level] = x;
                }
                /* 到最底层，直接返回下一个节点即可
                   因为当前节点x的key要比给定的key小
                 */
                if (level == 0) {
                    return next;
                } else {
                    // Switch to next list
                    level--;
                }
            }
        }
    }
    // 找到小于key中最大的key对应的节点 FindLessThan(const Key& key)
    template <typename Key, typename KeyComparator, typename Allocator>
    typename SkipList<Key, KeyComparator, Allocator>::Node* SkipList<Key, KeyComparator, Allocator>::FindLessThan(const Key& key) const {
        Node* x = head_;
        int32_t level = GetMaxHeight() - 1;
        while (true) {
            assert(x == head_ || comparator_(x->key, key) < 0);
            Node* next = x->Next(level);
            if (next == nullptr || comparator_(next->key, key) >= 0) {
                if (level == 0) {
                    return x;
                } else {
                    // Switch to next list
                    level--;
                }
            } else {
                x = next;
            }
        }
    }
    // 找到最后一个节点 FindLast()
    template <typename Key, typename KeyComparator, typename Allocator>
    typename SkipList<Key, KeyComparator, Allocator>::Node* SkipList<Key, KeyComparator, Allocator>::FindLast() const {
        Node* x = head_;
        int32_t level = GetMaxHeight() - 1;
        while (true) {
            Node* next = x->Next(level);
            if (next == nullptr) {
                if (level == 0) {
                    return x;
                } else {
                    // Switch to next list
                    level--;
                }
            } else {
                x = next;
            }
        }
    }
    
    template <typename Key, typename KeyComparator, typename Allocator>
    SkipList<Key, KeyComparator, Allocator>::SkipList(KeyComparator comparator) 
    : comparator_(comparator), 
    cur_height_(1),
    head_(NewNode(0, SkipListOption::kMaxHeight))) {
        for (int i = 0; i < SkipListOption::kMaxHeight; i++) {
            head_->SetNext(i, nullptr);
        }
    }
    // 向跳表中插入一条记录
    template <typename Key, typename KeyComparator, typename Allocator>
    void SkipList<Key, KeyComparator, Allocator>::Insert(const Key& key) {
        /* 节点的前驱节点
           用来保存每一层的前驱节点
         */
        Node* prev[SkipListOption::kMaxHeight] = {nullptr};
        // 在key的构造过程中，有一个持续递增的序号，因此理论上不会有重复的key
	    // 找到第一个大于等于key的节点，因为我们要把新的记录插入到这个节点前面
        Node* node = FindGreaterOrEqual(key, prev);
        if (node != nullptr) {
            if (Equal(key, node->key)) {
                std::cout<<"WARN: key "<<key<< "has existed"<<std::endl;
                // 如果key已经存在，那么直接返回
                return;
            }
        }
        // 生成一个随机的高度
        int new_level = RandomHeight();
        int cur_max_level = GetMaxHeight();
        if (new_level > cur_max_level) {
            // 如果随机生成的高度大于当前的最大高度，那么将prev中的高度也要更新
            for (int i = cur_max_level; i < new_level; i++) {
                prev[i] = head_;
            }
            // 更新当前的最大高度
            /* 此处不用为并发读加锁
               并发读在读取到更新以后的跳表层数而该节点还没有插入时也不会出错，因为此时会读取到nullptr
               而在存储引擎的比较器comparator设定中， nullptr比所有key都大
               而并发读的情况是在另外的进程中
               通过FindGreaterOrEqual中的GetMaxHeight访问cur_height_，实际上不影响FindGreaterOrEqual的调用结果
             */
            cur_height_.store(new_level, std::memory_order_relaxed);
        }

        // 生成一个新的节点
        Node* new_node = NewNode(key, new_level);
        // 将新节点插入到跳表中
        for (int i = 0; i < new_level; i++) {
            /* 这里NoBarrier_SetNext()版本就够了，因为后续prev[i] -> SetNext(i, x)语句会强制同步
               并且为了保证并发读的正确性，一定要先设置本节点指针，再设置原链表中节点prev的指针
             */
            new_node->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
            prev[i]->SetNext(i, new_node);
        }
    }
    // 判断跳表中是否有指定的数据
    template <typename Key, typename KeyComparator, typename Allocator>
    bool SkipList<Key, KeyComparator, Allocator>::Contains(const Key& key) const {
        Node* x = FindGreaterOrEqual(key, nullptr);
        if (x != nullptr && Equal(key, x->key)) {
            return true;
        } else {
            return false;
        }
    }

    template <typename Key, typename KeyComparator, typename Allocator>
    inline SkipList<Key, KeyComparator, Allocator>::Iterator::Iterator(const SkipList* list) : 
    list_(list) , node_(nullptr) {}
    
    template <typename Key, typename KeyComparator, typename Allocator>
    inline bool SkipList<Key, KeyComparator, Allocator>::Iterator::Valid() const {
        return node_ != nullptr;
    }

    template <typename Key, typename KeyComparator, typename Allocator>
    inline const Key& SkipList<Key, KeyComparator, Allocator>::Iterator::key() const {
        assert(Valid());
        return node_->key;
    }

    // 指向下一个节点
    template <typename Key, typename KeyComparator, typename Allocator>
    inline void SkipList<Key, KeyComparator, Allocator>::Iterator::Next() {
        assert(Valid());
        node_ = node_->Next(0);
    }

    // 指向前一个节点
    template <typename Key, typename KeyComparator, typename Allocator>
    inline void SkipList<Key, KeyComparator, Allocator>::Iterator::Prev() {
        assert(Valid());
        node_ = list_->FindLessThan(node_->key);
        if (node_ == list_->head_) {
            node_ = nullptr;
        }
    }

    // 根据key定位到指定的节点
    template <typename Key, typename KeyComparator, typename Allocator>
    inline void SkipList<Key, KeyComparator, Allocator>::Iterator::Seek(const Key& target) {
        node_ = list_->FindGreaterOrEqual(target, nullptr);
    }
    
    // 定位到最后一个节点
    template <typename Key, typename KeyComparator, typename Allocator>
    inline void SkipList<Key, KeyComparator, Allocator>::Iterator::SeekToLast() {
        // 通过SkipList找到最后一个节点
        node_ = list_->FindLast();
        // 如果返回的是表头的指针，那就说明链表中没有数据
        if (node_ == list_->head_) {
            node_ = nullptr;
        }
    }
};