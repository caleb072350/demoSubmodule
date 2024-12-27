#include <assert.h>
#include <utility>
#include <vector>
#include <type_traits>
#include "rbtree.h"
#include "list.h"

namespace algorithm
{
template<typename VAL>
struct __ReduceValue
{
    struct list_head list;
    VAL value;
    __ReduceValue(VAL&& val) : value(std::move(val)) { }
};

template<typename KEY, typename VAL>
struct __ReduceKey
{
    struct rb_node rb;
    KEY key;
    struct list_head value_list;
    size_t value_cnt;

    __ReduceKey(KEY&& k) : key(std::move(k))
    {
        INIT_LIST_HEAD(&this->value_list);
        this->value_cnt = 0;
    }

    void insert(VAL&& value)
    {
        __ReduceValue<VAL> *entry = new __ReduceValue<VAL>(std::move(value));
        list_add_tail(&entry->list, &this->value_list);
        this->value_cnt++;
    }

    ~__ReduceKey()
    {
        struct list_head *pos, *tmp;
        list_for_each_safe(pos, tmp, &this->value_list)
            delete list_entry(pos, struct __ReduceValue<VAL>, list);
    }
};

template<typename VAL, bool = std::is_class<VAL>::value>
class __ReduceIterator;

#define __REDUCE_ITERATOR_HEAP_MAX  256

/* VAL is a class. VAL must have size() method. */
template<typename VAL>
class __ReduceIterator<VAL, true> : public ReduceIterator<VAL>
{
public:
    virtual const VAL *next();
    virtual size_t size() { return this->original_size; }

private:
    void reduce_begin() { this->original_size = this->heap_size; }

    void reduce_end(VAL&& value)
    {
        size_t n = this->original_size;

        assert(n != this->heap_size);
        while (--n != this->heap_size)
            delete this->heap[n];
        this->heap[n]->value = std::move(value);
        this->heap_insert(this->heap[n]);
    }

    size_t count() { return this->heap_size; }

    __ReduceValue<VAL> *value() { return this->heap[0]; }

private:
    void heapify(int top);
    void heap_insert(__ReduceValue<VAL> *data);

private:
    __ReduceValue<VAL> *heap[__REDUCE_ITERATOR_HEAP_MAX];
    int heap_size;
    int original_size;

private:
    __ReduceIterator(struct list_head *value_list, size_t *value_cnt);
    template<class, class> friend class Reducer;
};

template<typename VAL>
const VAL *__ReduceIterator<VAL, true>::next()
{
    __ReduceValue<VAL> *data = this->heap[0];
    if (this->heap_size == 0)
        return NULL;
    
    this->heap[0] = this->heap[--this->heap_size];
    this->heapify(0);
    this->heap[this->heap_size] = data;
    return &data->value;
}

template<typename VAL>
void __ReduceIterator<VAL, true>::heapify(int top)
{
    __ReduceValue<VAL> *data = this->heap[top];
    __ReduceValue<VAL> **child;
    int last = this->heap_size - 1;
    int i;

    while (i = 2 * top + 1; i < last)
    {
        child = &this->heap[i];
        if (child[0]->value.size() < data->value.size())
        {
            if (child[1]->value.size() < child[0]->value.size())
            {
                this->heap[top] = child[1];
                top = i + 1;
            } else {
                this->heap[top] = child[0];
                top = i;
            }
        } else {
            if (child[1]->value.size() < data.value.size())
            {
                this->heap[top] = child[1];
                top = i + 1;
            } else {
                this->heap[top] = data;
                return;
            }
        }
    }

    if (i == last)
    {
        child = &this->heap[i];
        if (child[0]->value.size() < data->value.size())
        {
            this->heap[top] = child[0];
            top = i;
        }
    }

    this->heap[top] = data;
}

template<typename VAL>
void __ReduceIterator<VAL, true>::heap_insert(__ReduceValue<VAL> *data)
{
    __ReduceValue<VAL> *parent;
    int i = this->heap_size;

    while (i > 0)
    {
        parent = this->heap[(i-1)/2];
        if (data->value.size() < parent->value.size())
        {
            this->heap[i] = parent;
            i = (i-1)/2;
        } else 
            break;
    }

    this->heap[i] = data;
    this->heap_size++;
}

template<typename VAL>
__ReduceIterator<VAL, true>::__ReduceIterator(struct list_head *value_list,
                                              size_t *value_cnt)
{
    struct list_head *pos, *tmp;
    int n = 0;

    list_for_each_safe(pos, tmp, value_list)
    {
        if (n == __REDUCE_ITERATOR_HEAP_MAX)
            break;
        list_del(pos);
        this->heap[n++] = list_entry(pos, __ReduceValue<VAL>, list);
    }
    this->heap_size = n;
    *value_cnt = n;
    n /= 2;
    while (n > 0)
        this->heapify(--n);
}  

#undef __REDUCE_ITERATOR_HEAP_MAX


}