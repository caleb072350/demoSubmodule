#ifndef _MAPREDUCE_H_
#define _MAPREDUCE_H_

#include <utility>
#include <vector>
#include <functional>
#include <type_traits>
#include "rbtree.h"

namespace algorithm
{

template<typename VAL>
class ReduceIterator
{
public:
    virtual const VAL *next() = 0;
    virtual size_t size() = 0;
protected:
    virtual ~ReduceIterator() { }
};

template<typename KEY, typename VAL>
using reduce_function_t = 
    std::function<void (const KEY *, ReduceIterator<VAL> *, VAL *)>;

template<typename KEY, typename VAL>
class Reducer
{
public:
    void insert(KEY&& key, VAL&& val);

public:
    void start(reduce_function_t<KEY, VAL> reduce,
               std::vector<std::pair<KEY, VAL>> *output);
    
private:
    struct rb_root key_tree;
public:
    Reducer() { this->key_tree.rb_node = NULL; }
    virtual ~Reducer();
};

#include "MapReduce.inl"

}
#endif