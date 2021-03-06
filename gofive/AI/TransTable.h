#ifndef __TRANSTABLE_H__
#define __TRANSTABLE_H__
#include <map>
#include <unordered_map>
using namespace std;

//#define USE_LOCK

template<class data_type>
class TransTable
{
public:
    typedef uint32_t key_type;
    typedef map<key_type, data_type>  NMap;

    struct TransTableMap
    {
        NMap map;
        shared_mutex lock;
    };

    void init(uint32_t maxmem)
    {
        maxTableSize = maxmem / 3 / (sizeof(NMap::value_type) + sizeof(void*));
    }

    bool memoryValid()
    {
        return transTable.map.size() < maxTableSize;
    }

    inline bool get(key_type key, data_type& data)
    {
#ifdef USE_LOCK
        transTable.lock.lock_shared();
#endif // USE_LOCK
        NMap::iterator it = transTable.map.find(key);
        if (it != transTable.map.end())
        {
            data = it->second;
#ifdef USE_LOCK
            transTable.lock.unlock_shared();
#endif
            return true;
        }
        else
        {
#ifdef USE_LOCK
            transTable.lock.unlock_shared();
#endif
            return false;
        }
    }
    inline void insert(key_type key, const data_type& data)
    {
#ifdef USE_LOCK
        transTable.lock.lock();
#endif
        NMap::iterator it = transTable.map.find(key);
        if (it != transTable.map.end())
        {
            it->second = data;
        }
        else
        {
            transTable.map.insert(make_pair(key, data));
        }
#ifdef USE_LOCK
        transTable.lock.unlock();
#endif
    }

    void clearTransTable()
    {
        transTable.map.clear();
    }

    size_t getTransTableSize()
    {
        return transTable.map.size();
    }

private:
    size_t maxTableSize;
    TransTableMap transTable;
};


template<class data_type>
class TransTableArray
{
public:
    TransTableArray()
    {

    }
    void init(uint32_t maxmem)
    {
        if (maxmem == maxMem) return;

        if (transTable) delete[] transTable;
        maxMem = maxmem;
        maxTableSize = maxMem / sizeof(data_type);
        for (uint32_t i = 2;; i *= 2)
        {
            if (i > maxTableSize)
            {
                maxTableSize = i / 2;
                break;
            }
        }
        transTable = new data_type[maxTableSize];
    }

    inline bool get(uint32_t key, data_type& data)
    {
        data = transTable[key%maxTableSize];
        return true;
    }
    inline void insert(uint32_t key, const data_type& data)
    {
        transTable[key%maxTableSize] = data;
    }

    bool memoryValid()
    {
        return true;
    }

    void clearTransTable()
    {
        return;
    }

    size_t getTransTableSize()
    {
        return maxTableSize;
    }

    ~TransTableArray()
    {
        if (transTable) delete[] transTable;
    }
private:
    uint32_t maxMem = 0;
    uint32_t maxTableSize;
    data_type* transTable = NULL;
};

#endif
