#pragma once

template<typename T>
struct CacheLoadResult {
    bool success;
    //bool newLoad;
    T* data;

    CacheLoadResult(bool success/*, bool newLoad*/, T* data) : success(success)/*, newLoad(newLoad)*/, data(data) {}
};
