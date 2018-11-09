/*
Copyright (C) 2016 beardypig

This file is part of Vectrexia.

Vectrexia is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Vectrexia is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Vectrexia.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef VECTREXIA_UPDATETIMER_H
#define VECTREXIA_UPDATETIMER_H

#include <stdint.h>
#include <vector>
#include <functional>
#include <algorithm>

using update_callback_t = std::function<void(uint64_t)>;

template<typename T>
class UpdateTimer
{
    struct data
    {
        uint64_t cycles;
        T *ptr;
        T value;

        bool operator== (const uint64_t &count)
        {
            if (cycles <= count)
            {
                *ptr = value;
                return true;
            }
            return false;
        }
    };

    std::vector<data> items;
public:
    // enqueue and item to be updated at a later time
    void enqueue(uint64_t cycles, T* ptr, T value)
    {
        items.push_back({cycles, ptr, value });
    }
    void tick(uint64_t cycles)
    {
        // https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom
        items.erase(std::remove(items.begin(), items.end(), cycles), items.end());
    }
    void clear()
    {
        items.clear();
    }
};

#endif //VECTREXIA_UPDATETIMER_H
