#ifndef UDPLIBRARY_UDPHASHTABLE_H
#define UDPLIBRARY_UDPHASHTABLE_H

// Copyright 2004 Sony Online Entertainment, all rights reserved.
// Author: Jeff Petersen

#include <stddef.h>
#include <string.h>

namespace UdpLibrary
{


int NextPrime(int value);


    // This is a simple hash table template.  Any type of object can be stored in this hash table
    // provided it can be copied.  Resizing the table while in use is generally not recommended.
    // putting the same object in the table twice is generally not recommended as it makes the find ambiguous
struct HashTableStatistics
{
    int tableSize;
    int usedSlots;
    int totalEntries;
};


    //////////////////////////////////////////////////////////////////////////
    // HashTable
    //////////////////////////////////////////////////////////////////////////
template<typename T> class HashTable
{
    public:
        HashTable(int hashSize);
        ~HashTable();

        void Insert(T& obj, int hashValue);
        bool Remove(T& obj, int hashValue);
        void Reset();            // removes all entries from the table

        T *FindFirst(int hashValue) const;        // returns NULL if not found
        T *FindNext(T *prevResult) const;            // returns NULL if not found

        T *WalkFirst() const;                        // returns NULL if not found
        T *WalkNext(T *prevResult) const;            // returns NULL if not found

        void Resize(int hashSize);            // the actual hash size will be rounded up to the next larger prime number (very slow, not recommended)
        void GetStatistics(HashTableStatistics *stats) const;

    protected:
        struct HashEntry
        {
            T obj;
            int hashValue;
            HashEntry *nextEntry;
        };

        HashEntry **mTable;
        int mTableSize;
        int mEntryCount;
        int mStatUsedSlots;
};


template<typename T> HashTable<T>::HashTable(int hashSize)
{
    mTable = NULL;
    mTableSize = 0;
    mEntryCount = 0;
    mStatUsedSlots = 0;
    Resize(hashSize);
}

template<typename T> HashTable<T>::~HashTable()
{
    Reset();    // deletes everything
    delete[] mTable;
}

template<typename T> void HashTable<T>::Insert(T& obj, int hashValue)
{
    HashEntry *entry = new HashEntry;
    entry->obj = obj;
    entry->hashValue = hashValue;

    int spot = ((unsigned)hashValue) % mTableSize;
    if (mTable[spot] == NULL)
    {
        entry->nextEntry = NULL;
        mTable[spot] = entry;
        mStatUsedSlots++;
    }
    else
    {
        entry->nextEntry = mTable[spot];
        mTable[spot] = entry;
    }
    mEntryCount++;
}

template<typename T> bool HashTable<T>::Remove(T& obj, int hashValue)
{
    int spot = ((unsigned)hashValue) % mTableSize;
    HashEntry* next = mTable[spot];
    HashEntry** prev = &mTable[spot];
    while (next != NULL)
    {
        if (next->obj == obj && next->hashValue == hashValue)
        {
            *prev = next->nextEntry;
            delete next;
            mEntryCount--;
            if (mTable[spot] == NULL)
                mStatUsedSlots--;
            return(true);
            break;
        
        }
        prev = &next->nextEntry;
        next = next->nextEntry;
    }
    return(false);
}

template<typename T> void HashTable<T>::Reset()
{
    for (int spot = 0; spot < mTableSize; spot++)
    {
        HashEntry *curr = mTable[spot];
        if (curr != NULL)
        {
            mStatUsedSlots--;
            while (curr != NULL)
            {
                HashEntry *next = curr->nextEntry;
                delete curr;
                mEntryCount--;
                curr = next;
            }
            mTable[spot] = NULL;
        }
    }
}

template<typename T> T *HashTable<T>::FindFirst(int hashValue) const
{
    HashEntry *entry = mTable[((unsigned)hashValue) % mTableSize];
    while (entry != NULL)
    {
        if (entry->hashValue == hashValue)
            return(&entry->obj);
        entry = entry->nextEntry;
    }
    return(NULL);
}

template<typename T> T *HashTable<T>::FindNext(T *prevResult) const
{
    HashEntry *entry = (HashEntry *)(((char *)prevResult) - offsetof(HashEntry, obj));
    int hashValue = entry->hashValue;
    entry = entry->nextEntry;
    while (entry != NULL)
    {
        if (entry->hashValue == hashValue)
            return(&entry->obj);
        entry = entry->nextEntry;
    }
    return(NULL);
}

template<typename T> T *HashTable<T>::WalkFirst() const
{
    for (int bucket = 0; bucket < mTableSize; bucket++)
    {
        HashEntry *entry = mTable[bucket];
        if (entry != NULL)
            return(&entry->obj);
    }
    return(NULL);
}

template<typename T> T *HashTable<T>::WalkNext(T *prevResult) const
{
    HashEntry *entry = (HashEntry *)(((char *)prevResult) - offsetof(HashEntry, obj));
    int bucket = ((unsigned)entry->hashValue) % mTableSize;

    entry = entry->nextEntry;
    if (entry != NULL)
        return(&entry->obj);

    bucket++;    // go onto next bucket
    for (; bucket < mTableSize; bucket++)
    {
        HashEntry *entry = mTable[bucket];
        if (entry != NULL)
            return(&entry->obj);
    }
    return(NULL);
}

template<typename T> void HashTable<T>::Resize(int hashSize)
{
    HashEntry **oldTable = mTable;
    int oldSize = mTableSize;
    
        // calculate next prime number greater than hashSize.
    mTableSize = NextPrime(hashSize);
    mTable = new HashEntry*[mTableSize];
    memset(mTable, 0, sizeof(HashEntry *) * mTableSize);

    mStatUsedSlots = 0;

    if (mEntryCount > 0)
    {
        for (int i = 0; i < oldSize; i++)
        {
            HashEntry* next = oldTable[i];
            while (next != NULL)
            {
                HashEntry* hold = next;
                next = next->nextEntry;

                    // insert hold into new table
                int spot = ((unsigned)hold->hashValue) % mTableSize;
                if (mTable[spot] == NULL)
                {
                    hold->nextEntry = NULL;
                    mTable[spot] = hold;
                    mStatUsedSlots++;
                }
                else
                {
                    hold->nextEntry = mTable[spot];
                    mTable[spot] = hold;
                }
            }
        }
    }

    delete[] oldTable;
}

template<typename T> void HashTable<T>::GetStatistics(HashTableStatistics *stats) const
{
    stats->totalEntries = mEntryCount;
    stats->usedSlots = mStatUsedSlots;
    stats->tableSize = mTableSize;
}


    //////////////////////////////////////////////////////////////////////////////
    // ObjectHashTable
    //
    // similar to basic HashTable object, only this version requires the contents
    // be pointers to objects derived from class HashTableMember.  If you can use
    // this version, it avoids allocations of HashEntry objects.
    // making the cost of object inserts/deletes dirt-cheap
    //////////////////////////////////////////////////////////////////////////////
template<typename M> class HashTableMember
{
#if defined(_MSC_VER) && (_MSC_VER < 1300)        // MSVC 7.0 is the first version to support friend templates
    public:
#else
    protected:
        template<typename T, typename P> friend class ObjectHashTable;
#endif
        int mHashValue;
        M *mHashNextEntry;
};

template<typename M> class HashTableMember1 : public HashTableMember<M> {};
template<typename M> class HashTableMember2 : public HashTableMember<M> {};
template<typename M> class HashTableMember3 : public HashTableMember<M> {};
template<typename M> class HashTableMember4 : public HashTableMember<M> {};


template<typename T, typename C = T> class ObjectHashTable
{
    public:
        ObjectHashTable(int hashSize);
        ~ObjectHashTable();

        void Insert(T *obj, int hashValue);
        bool Remove(T *obj);
        void Reset();                            // removes all entries from the table

        T *FindFirst(int hashValue) const;        // returns NULL if not found
        T *FindNext(T *prevResult) const;        // returns NULL if not found

        T *WalkFirst() const;                    // returns NULL if not found
        T *WalkNext(T *prevResult) const;        // returns NULL if not found

        void Resize(int hashSize);                // the actual hash size will be rounded up to the next larger prime number (very slow, not recommended)
        void GetStatistics(HashTableStatistics *stats) const;

    protected:
        T **mTable;
        int mTableSize;
        int mEntryCount;
        int mStatUsedSlots;
};


template<typename T, typename C> ObjectHashTable<T, C>::ObjectHashTable(int hashSize)
{
    mTable = NULL;
    mTableSize = 0;
    mEntryCount = 0;
    mStatUsedSlots = 0;
    Resize(hashSize);
}

template<typename T, typename C> ObjectHashTable<T, C>::~ObjectHashTable()
{
    delete[] mTable;
}

template<typename T, typename C> void ObjectHashTable<T, C>::Insert(T *obj, int hashValue)
{
    static_cast<C *>(obj)->mHashValue = hashValue;

    int spot = ((unsigned)hashValue) % mTableSize;
    if (mTable[spot] == NULL)
    {
        static_cast<C *>(obj)->mHashNextEntry = NULL;
        mTable[spot] = obj;
        mStatUsedSlots++;
    }
    else
    {
        static_cast<C *>(obj)->mHashNextEntry = mTable[spot];
        mTable[spot] = obj;
    }
    mEntryCount++;
}

template<typename T, typename C> bool ObjectHashTable<T, C>::Remove(T *obj)
{
    int spot = ((unsigned)static_cast<C *>(obj)->mHashValue) % mTableSize;
    T *cur = mTable[spot];
    T **prev = &mTable[spot];
    while (cur != NULL)
    {
        if (cur == obj)
        {
            *prev = static_cast<C *>(cur)->mHashNextEntry;
            static_cast<C *>(cur)->mHashNextEntry = NULL;
            mEntryCount--;
            if (mTable[spot] == NULL)
                mStatUsedSlots--;
            return(true);
            break;
        
        }
        prev = &static_cast<C *>(cur)->mHashNextEntry;
        cur = static_cast<C *>(cur)->mHashNextEntry;
    }
    return(false);
}

template<typename T, typename C> void ObjectHashTable<T, C>::Reset()
{
    memset(mTable, 0, sizeof(T *) * mTableSize);
    mStatUsedSlots = 0;
    mEntryCount = 0;    
}

template<typename T, typename C> T *ObjectHashTable<T, C>::FindFirst(int hashValue) const
{
    T *entry = mTable[((unsigned)hashValue) % mTableSize];
    while (entry != NULL)
    {
        if (static_cast<C *>(entry)->mHashValue == hashValue)
            return(entry);
        entry = static_cast<C *>(entry)->mHashNextEntry;
    }
    return(NULL);
}

template<typename T, typename C> T *ObjectHashTable<T, C>::FindNext(T *prevResult) const
{
    T *entry = prevResult;
    int hashValue = static_cast<C *>(entry)->mHashValue;
    entry = static_cast<C *>(entry)->mHashNextEntry;
    while (entry != NULL)
    {
        if (static_cast<C *>(entry)->mHashValue == hashValue)
            return(entry);
        entry = static_cast<C *>(entry)->mHashNextEntry;
    }
    return(NULL);
}

template<typename T, typename C> T *ObjectHashTable<T, C>::WalkFirst() const
{
    for (int bucket = 0; bucket < mTableSize; bucket++)
    {
        T *entry = mTable[bucket];
        if (entry != NULL)
            return(entry);
    }
    return(NULL);
}

template<typename T, typename C> T *ObjectHashTable<T, C>::WalkNext(T *prevResult) const
{
    T *entry = prevResult;
    int bucket = ((unsigned)static_cast<C *>(entry)->mHashValue) % mTableSize;

    entry = static_cast<C *>(entry)->mHashNextEntry;
    if (entry != NULL)
        return(entry);

    bucket++;    // go onto next bucket
    for (; bucket < mTableSize; bucket++)
    {
        entry = mTable[bucket];
        if (entry != NULL)
            return(entry);
    }
    return(NULL);
}

template<typename T, typename C> void ObjectHashTable<T, C>::Resize(int hashSize)
{
    T **oldTable = mTable;
    int oldSize = mTableSize;
    
        // calculate next prime number greater than hashSize.
    mTableSize = NextPrime(hashSize);
    mTable = new T*[mTableSize];
    memset(mTable, 0, sizeof(T *) * mTableSize);

    mStatUsedSlots = 0;

    if (mEntryCount > 0)
    {
        for (int i = 0; i < oldSize; i++)
        {
            T *cur = oldTable[i];
            while (cur != NULL)
            {
                T *hold = cur;
                cur = static_cast<C *>(cur)->mHashNextEntry;

                    // insert hold into new table
                int spot = ((unsigned)static_cast<C *>(hold)->mHashValue) % mTableSize;
                if (mTable[spot] == NULL)
                {
                    static_cast<C *>(hold)->mHashNextEntry = NULL;
                    mTable[spot] = hold;
                    mStatUsedSlots++;
                }
                else
                {
                    static_cast<C *>(hold)->mHashNextEntry = mTable[spot];
                    mTable[spot] = hold;
                }
            }
        }
    }

    delete[] oldTable;
}

template<typename T, typename C> void ObjectHashTable<T, C>::GetStatistics(HashTableStatistics *stats) const
{
    stats->totalEntries = mEntryCount;
    stats->usedSlots = mStatUsedSlots;
    stats->tableSize = mTableSize;
}






    ////////////////////////////////////////////////////////
    // helper functions for the above hash table classes
    ////////////////////////////////////////////////////////

inline bool IsPrime(int value)        // works on positive numbers up 62,710,561
{
    static int sPrimeTab[] =
    {      2,      3,      5,      7,     11,    13,    17,    19,    23,    29,
          31,     37,     41,     43,     47,    53,    59,    61,    67,    71,
          73,     79,     83,     89,     97,   101,   103,   107,   109,   113,
         127,    131,    137,    139,    149,   151,   157,   163,   167,   173,
         179,    181,    191,    193,    197,   199,   211,   223,   227,   229,
         233,    239,    241,    251,    257,   263,   269,   271,   277,   281,
         283,    293,    307,    311,    313,   317,   331,   337,   347,   349,
         353,    359,    367,    373,    379,   383,   389,   397,   401,   409,
         419,    421,    431,    433,    439,   443,   449,   457,   461,   463,
         467,    479,    487,    491,    499,   503,   509,   521,   523,   541,
         547,    557,    563,    569,    571,   577,   587,   593,   599,   601,
         607,    613,    617,    619,    631,   641,   643,   647,   653,   659,
         661,    673,    677,    683,    691,   701,   709,   719,   727,   733,
         739,    743,    751,    757,    761,   769,   773,   787,   797,   809,
         811,    821,    823,    827,    829,   839,   853,   857,   859,   863,
         877,    881,    883,    887,    907,   911,   919,   929,   937,   941,
         947,    953,    967,    971,    977,   983,   991,   997,  1009,  1013,
        1019,   1021,   1031,   1033,   1039,  1049,  1051,  1061,  1063,  1069,
        1087,   1091,   1093,   1097,   1103,  1109,  1117,  1123,  1129,  1151,
        1153,   1163,   1171,   1181,   1187,  1193,  1201,  1213,  1217,  1223,
        1229,   1231,   1237,   1249,   1259,  1277,  1279,  1283,  1289,  1291,
        1297,   1301,   1303,   1307,   1319,  1321,  1327,  1361,  1367,  1373,
        1381,   1399,   1409,   1423,   1427,  1429,  1433,  1439,  1447,  1451,
        1453,   1459,   1471,   1481,   1483,  1487,  1489,  1493,  1499,  1511,
        1523,   1531,   1543,   1549,   1553,  1559,  1567,  1571,  1579,  1583,
        1597,   1601,   1607,   1609,   1613,  1619,  1621,  1627,  1637,  1657,
        1663,   1667,   1669,   1693,   1697,  1699,  1709,  1721,  1723,  1733,
        1741,   1747,   1753,   1759,   1777,  1783,  1787,  1789,  1801,  1811,
        1823,   1831,   1847,   1861,   1867,  1871,  1873,  1877,  1879,  1889,
        1901,   1907,   1913,   1931,   1933,  1949,  1951,  1973,  1979,  1987,
        1993,   1997,   1999,   2003,   2011,  2017,  2027,  2029,  2039,  2053,
        2063,   2069,   2081,   2083,   2087,  2089,  2099,  2111,  2113,  2129,
        2131,   2137,   2141,   2143,   2153,  2161,  2179,  2203,  2207,  2213,
        2221,   2237,   2239,   2243,   2251,  2267,  2269,  2273,  2281,  2287,
        2293,   2297,   2309,   2311,   2333,  2339,  2341,  2347,  2351,  2357,
        2371,   2377,   2381,   2383,   2389,  2393,  2399,  2411,  2417,  2423,
        2437,   2441,   2447,   2459,   2467,  2473,  2477,  2503,  2521,  2531,
        2539,   2543,   2549,   2551,   2557,  2579,  2591,  2593,  2609,  2617,
        2621,   2633,   2647,   2657,   2659,  2663,  2671,  2677,  2683,  2687,
        2689,   2693,   2699,   2707,   2711,  2713,  2719,  2729,  2731,  2741,
        2749,   2753,   2767,   2777,   2789,  2791,  2797,  2801,  2803,  2819,
        2833,   2837,   2843,   2851,   2857,  2861,  2879,  2887,  2897,  2903,
        2909,   2917,   2927,   2939,   2953,  2957,  2963,  2969,  2971,  2999,
        3001,   3011,   3019,   3023,   3037,  3041,  3049,  3061,  3067,  3079,
        3083,   3089,   3109,   3119,   3121,  3137,  3163,  3167,  3169,  3181,
        3187,   3191,   3203,   3209,   3217,  3221,  3229,  3251,  3253,  3257,
        3259,   3271,   3299,   3301,   3307,  3313,  3319,  3323,  3329,  3331,
        3343,   3347,   3359,   3361,   3371,  3373,  3389,  3391,  3407,  3413,
        3433,   3449,   3457,   3461,   3463,  3467,  3469,  3491,  3499,  3511,
        3517,   3527,   3529,   3533,   3539,  3541,  3547,  3557,  3559,  3571,
        3581,   3583,   3593,   3607,   3613,  3617,  3623,  3631,  3637,  3643,
        3659,   3671,   3673,   3677,   3691,  3697,  3701,  3709,  3719,  3727,
        3733,   3739,   3761,   3767,   3769,  3779,  3793,  3797,  3803,  3821,
        3823,   3833,   3847,   3851,   3853,  3863,  3877,  3881,  3889,  3907,
        3911,   3917,   3919,   3923,   3929,  3931,  3943,  3947,  3967,  3989,
        4001,   4003,   4007,   4013,   4019,  4021,  4027,  4049,  4051,  4057,
        4073,   4079,   4091,   4093,   4099,  4111,  4127,  4129,  4133,  4139,
        4153,   4157,   4159,   4177,   4201,  4211,  4217,  4219,  4229,  4231,
        4241,   4243,   4253,   4259,   4261,  4271,  4273,  4283,  4289,  4297,
        4327,   4337,   4339,   4349,   4357,  4363,  4373,  4391,  4397,  4409,
        4421,   4423,   4441,   4447,   4451,  4457,  4463,  4481,  4483,  4493,
        4507,   4513,   4517,   4519,   4523,  4547,  4549,  4561,  4567,  4583,
        4591,   4597,   4603,   4621,   4637,  4639,  4643,  4649,  4651,  4657,
        4663,   4673,   4679,   4691,   4703,  4721,  4723,  4729,  4733,  4751,
        4759,   4783,   4787,   4789,   4793,  4799,  4801,  4813,  4817,  4831,
        4861,   4871,   4877,   4889,   4903,  4909,  4919,  4931,  4933,  4937,
        4943,   4951,   4957,   4967,   4969,  4973,  4987,  4993,  4999,  5003,
        5009,   5011,   5021,   5023,   5039,  5051,  5059,  5077,  5081,  5087,
        5099,   5101,   5107,   5113,   5119,  5147,  5153,  5167,  5171,  5179,
        5189,   5197,   5209,   5227,   5231,  5233,  5237,  5261,  5273,  5279,
        5281,   5297,   5303,   5309,   5323,  5333,  5347,  5351,  5381,  5387,
        5393,   5399,   5407,   5413,   5417,  5419,  5431,  5437,  5441,  5443,
        5449,   5471,   5477,   5479,   5483,  5501,  5503,  5507,  5519,  5521,
        5527,   5531,   5557,   5563,   5569,  5573,  5581,  5591,  5623,  5639,
        5641,   5647,   5651,   5653,   5657,  5659,  5669,  5683,  5689,  5693,
        5701,   5711,   5717,   5737,   5741,  5743,  5749,  5779,  5783,  5791,
        5801,   5807,   5813,   5821,   5827,  5839,  5843,  5849,  5851,  5857,
        5861,   5867,   5869,   5879,   5881,  5897,  5903,  5923,  5927,  5939,
        5953,   5981,   5987,   6007,   6011,  6029,  6037,  6043,  6047,  6053,
        6067,   6073,   6079,   6089,   6091,  6101,  6113,  6121,  6131,  6133,
        6143,   6151,   6163,   6173,   6197,  6199,  6203,  6211,  6217,  6221,
        6229,   6247,   6257,   6263,   6269,  6271,  6277,  6287,  6299,  6301,
        6311,   6317,   6323,   6329,   6337,  6343,  6353,  6359,  6361,  6367,
        6373,   6379,   6389,   6397,   6421,  6427,  6449,  6451,  6469,  6473,
        6481,   6491,   6521,   6529,   6547,  6551,  6553,  6563,  6569,  6571,
        6577,   6581,   6599,   6607,   6619,  6637,  6653,  6659,  6661,  6673,
        6679,   6689,   6691,   6701,   6703,  6709,  6719,  6733,  6737,  6761,
        6763,   6779,   6781,   6791,   6793,  6803,  6823,  6827,  6829,  6833,
        6841,   6857,   6863,   6869,   6871,  6883,  6899,  6907,  6911,  6917,
        6947,   6949,   6959,   6961,   6967,  6971,  6977,  6983,  6991,  6997,
        7001,   7013,   7019,   7027,   7039,  7043,  7057,  7069,  7079,  7103,
        7109,   7121,   7127,   7129,   7151,  7159,  7177,  7187,  7193,  7207,
        7211,   7213,   7219,   7229,   7237,  7243,  7247,  7253,  7283,  7297,
        7307,   7309,   7321,   7331,   7333,  7349,  7351,  7369,  7393,  7411,
        7417,   7433,   7451,   7457,   7459,  7477,  7481,  7487,  7489,  7499,
        7507,   7517,   7523,   7529,   7537,  7541,  7547,  7549,  7559,  7561,
        7573,   7577,   7583,   7589,   7591,  7603,  7607,  7621,  7639,  7643,
        7649,   7669,   7673,   7681,   7687,  7691,  7699,  7703,  7717,  7723,
        7727,   7741,   7753,   7757,   7759,  7789,  7793,  7817,  7823,  7829,
        7841,   7853,   7867,   7873,   7877,  7879,  7883,  7901,  7907,  7919 
    };

    for (unsigned int j = 0; j < sizeof(sPrimeTab) / sizeof(int) && sPrimeTab[j] * sPrimeTab[j] < value; j++)
    {
        if (value % sPrimeTab[j] == 0)
            return(false);
    }
    return(true);
}

inline int NextPrime(int value)
{
    if (value % 2 == 0)
        value++;
    while (!IsPrime(value))
        value += 2;
    return(value);
}

inline int HashString(const char *string)
{
    int h = 0;
    while (*string != 0)
        h = ((31 * h) + *string++) ^ (h >> 26);
    return(h);
}

inline int UpperHashString(const char *string)
{
    int h = 0;
    while (*string != 0)
    {
        h = ((31 * h) + (*string >= 'a' && *string <= 'z') ? (*string - 0x20) : *string) ^ (h >> 26);
        string++;
    }
    return(h);
}

}   // namespace

#endif
