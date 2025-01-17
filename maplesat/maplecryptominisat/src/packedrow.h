/*
 * CryptoMiniSat
 *
 * Copyright (c) 2009-2015, Mate Soos. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.0 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
*/

#ifndef PACKEDROW_H
#define PACKEDROW_H

//#define DEBUG_ROW

#include <vector>
#include "constants.h"
#include "solvertypes.h"
#include <string.h>
#include <iostream>
#include <algorithm>
#include <limits>

namespace CMSat {

class PackedMatrix;

class PackedRow
{
public:
    bool operator ==(const PackedRow& b) const;
    bool operator !=(const PackedRow& b) const;

    PackedRow& operator=(const PackedRow& b)
    {
        #ifdef DEBUG_ROW
        assert(size > 0);
        assert(b.size > 0);
        assert(size == b.size);
        #endif

        memcpy(mp-1, b.mp-1, size+1);
        return *this;
    }

    PackedRow& operator^=(const PackedRow& b)
    {
        #ifdef DEBUG_ROW
        assert(size > 0);
        assert(b.size > 0);
        assert(b.size == size);
        #endif

        for (uint32_t i = 0; i != size; i++) {
            *(mp + i) ^= *(b.mp + i);
        }

        is_true_internal ^= b.is_true_internal;
        return *this;
    }

    void xorBoth(const PackedRow& b)
    {
        #ifdef DEBUG_ROW
        assert(size > 0);
        assert(b.size > 0);
        assert(b.size == size);
        #endif

        for (uint32_t i = 0; i != 2*size+1; i++) {
            *(mp + i) ^= *(b.mp + i);
        }

        is_true_internal ^= b.is_true_internal;
    }


    uint32_t popcnt() const;
    uint32_t popcnt(uint32_t from) const;

    bool popcnt_is_one() const
    {
        #if __GNUC__ >= 4
        int ret = 0;
        for (uint32_t i = 0; i != size; i++) {
            ret += __builtin_popcount(mp[i]&0xffffffff);
            ret += __builtin_popcount(mp[i]>>32);
            if (ret > 1) return false;
        }
        return ret == 1;
        #else
        uint32_t popcount = 0;
        for (uint32_t i = 0; i != size; i++) {
            uint64_t tmp = mp[i];
            while(tmp) {
                popcount += tmp & 1;
                tmp >>= 1;
            }
        }
        return popcount == 1;
        #endif
    }

    bool popcnt_is_one(uint32_t from) const
    {
        from++;

        uint64_t tmp = mp[from/64];
        tmp >>= from%64;
        if (tmp) return false;

        for (uint32_t i = from/64+1; i != size; i++)
            if (mp[i]) return false;
        return true;
    }

    inline const uint64_t& is_true() const
    {
        return is_true_internal;
    }

    bool isZero() const
    {
        for (uint32_t i = 0; i != size; i++) {
            if (mp[i]) return false;
        }
        return true;
    }

    void setZero()
    {
        memset(mp, 0, sizeof(uint64_t)*size);
    }

    void clearBit(const uint32_t i)
    {
        mp[i/64] &= ~((uint64_t)1 << (i%64));
    }

    void invert_is_true(const bool b = true)
    {
        is_true_internal ^= (uint64_t)b;
    }

    void setBit(const uint32_t i)
    {
        mp[i/64] |= ((uint64_t)1 << (i%64));
    }

    void swapBoth(PackedRow b)
    {
        #ifdef DEBUG_ROW
        assert(size > 0);
        assert(b.size > 0);
        assert(b.size == size);
        #endif

        uint64_t * __restrict mp1 = mp-1;
        uint64_t * __restrict mp2 = b.mp-1;

        uint32_t i = 2*(size+1);

        while(i != 0) {
            std::swap(*mp1, *mp2);
            mp1++;
            mp2++;
            i--;
        }
    }

    bool operator[](const uint32_t& i) const
    {
        #ifdef DEBUG_ROW
        assert(size*64 > i);
        #endif

        return (mp[i/64] >> (i%64)) & 1;
    }

    template<class T>
    void set(const T& v, const vector<uint16_t>& var_to_col, const uint32_t num_cols)
    {
        assert(size == (num_cols/64) + ((bool)(num_cols % 64)));
        //mp = new uint64_t[size];
        setZero();
        for (uint32_t i = 0; i != v.size(); i++) {
            const uint32_t toset_var = var_to_col[v[i].var()];
            assert(toset_var != std::numeric_limits<uint32_t>::max());

            setBit(toset_var);
        }

        is_true_internal = !v.xorEqualFalse();
    }

    bool fill(vector<Lit>& tmp_clause, const vector<lbool>& assigns, const vector<Var>& col_to_var_original) const;

    unsigned long int scan(const unsigned long int var) const
    {
        #ifdef DEBUG_ROW
        assert(size > 0);
        #endif

        for(uint32_t i = var; i != size*64; i++) {
            if (this->operator[](i)) return i;
        }

        return std::numeric_limits<unsigned long int>::max();
    }

    //friend ::std::ostream& operator << (std::ostream& os, const PackedRow& m);
    uint32_t getSize() const
    {
        return size;
    }

private:
    friend class PackedMatrix;
    PackedRow(const uint32_t _size, uint64_t*  const _mp) :
        mp(_mp+1)
        , is_true_internal(*_mp)
        , size(_size)
    {}

    uint64_t* __restrict const mp;
    uint64_t& is_true_internal;
    const uint32_t size;
};

std::ostream& operator << (std::ostream& os, const PackedRow& m);

} //end namespace

#endif //PACKEDROW_H

