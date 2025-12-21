#pragma once

#include <random>
#include <type_traits>

namespace Tools {

    template<class T>
    using uniform_distribution = typename std::conditional<
        std::is_floating_point<T>::value,
        std::uniform_real_distribution<T>,
        typename std::conditional<
            std::is_integral<T>::value,
            std::uniform_int_distribution<T>,
            void
        >::type
    >::type;

    template <typename T>
    T RandU(T max = 1, T min = 0) {
        thread_local std::mt19937_64 mt{ std::random_device{}() };
        return uniform_distribution<T>(min, max)(mt);
    };
   
}