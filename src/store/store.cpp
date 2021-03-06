// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "store.h"

#include <napa/memory.h>

#include <mutex>
#include <unordered_map>

using namespace napa::store;

class StoreImpl: public Store {
public:
    /// <summary> Constructor. </summary>
    explicit StoreImpl(const char* id)
        : _id(id) {
    }

    /// <summary> Get ID of this store. </summary>
    const char* GetId() const override {
        return _id.c_str();
    }

    /// <summary> Set value with a key. </summary>
    /// <param name="key"> Case-sensitive key to set. </param>
    /// <param name="value"> A shared pointer of ValueType,
    /// which is composed by a pair of payload and transport context. </returns>
    void Set(const char* key, std::shared_ptr<Store::ValueType> value) override {
        std::lock_guard<std::mutex> lock(_storeAccess);
        auto it = _valueMap.find(key);
        if (it != _valueMap.end()) {
            it->second = std::move(value);
        } else {
            _valueMap.emplace(std::string(key), std::move(value));
        }
    }

    /// <summary> Get value by a key. </summary>
    /// <param name="key"> Case-sensitive key to get. </param>
    /// <returns> A ValueType shared pointer, empty if not found. </returns>
    std::shared_ptr<ValueType> Get(const char* key) const override {
        std::lock_guard<std::mutex> lock(_storeAccess);
        auto it = _valueMap.find(key);
        if (it != _valueMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    /// <summary> Check if this store has a key. </summary>
    /// <param name="key"> Case-sensitive key. </param>
    /// <returns> True if the key exists in store. </returns>
    bool Has(const char* key) const override {
        std::lock_guard<std::mutex> lock(_storeAccess);
        return _valueMap.find(key) != _valueMap.end();
    }

    /// <summary> Delete a key. No-op if key is not found in store. </summary>
    void Delete(const char* key) override {
        std::lock_guard<std::mutex> lock(_storeAccess);
        _valueMap.erase(key);
    }

    /// <summary> Return size of the store. </summary>
    size_t Size() const override {
        std::lock_guard<std::mutex> lock(_storeAccess);
        return _valueMap.size();
    }

private:
    /// <summary> ID. Case sensitive. </summary>
    std::string _id;

    /// <summary> Key to value map. </summary>
    std::unordered_map<std::string, std::shared_ptr<Store::ValueType>> _valueMap;

    /// <summary> Mutex to value map access. (use std::shared_mutex when it's public) </summary>
    mutable std::mutex _storeAccess;
};

namespace napa {
namespace store {

    namespace {
        std::unordered_map<std::string, std::weak_ptr<Store>> _storeRegistry;
        std::mutex _registryAccess;
    } // namespace

    std::shared_ptr<Store> CreateStore(const char* id) {
        std::lock_guard<std::mutex> lockWrite(_registryAccess);
        
        std::shared_ptr<Store> store;
        auto it = _storeRegistry.find(id);
        if (it == _storeRegistry.end()) {
            store = std::make_shared<StoreImpl>(id);
            _storeRegistry.insert(std::make_pair(std::string(id), store));
        }
        return store;
    }

    std::shared_ptr<Store> GetOrCreateStore(const char* id) {
        auto store = GetStore(id);
        if (store == nullptr) {
            store = CreateStore(id);
            if (store == nullptr) {
                // Already created just now. Lookup again.
                store = GetStore(id);
            }
        }
        return store;
    }

    std::shared_ptr<Store> GetStore(const char* id) {
        std::lock_guard<std::mutex> lockRead(_registryAccess);
        auto it = _storeRegistry.find(id);
        if (it != _storeRegistry.end()) {
            return it->second.lock();
        }
        return std::shared_ptr<Store>();
    }

    size_t GetStoreCount() {
        std::lock_guard<std::mutex> lockWrite(_registryAccess);
        for (auto it = _storeRegistry.begin(); it != _storeRegistry.end(); ) {
            if (it->second.use_count() == 0) {
                _storeRegistry.erase(it++);
            } else {
                ++it;
            }
        }
        return _storeRegistry.size();
    }
} // namespace store
} // namespace napa
