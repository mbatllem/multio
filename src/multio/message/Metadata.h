/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

/// @author Philipp Geier

/// @date Sept 2023

#pragma once

#include "multio/message/MetadataValue.h"


//----------------------------------------------------------------------------------------------------------------------


namespace multio::message {

class Metadata {
public:
    using This = Metadata;
    using KeyType = typename MetadataTypes::KeyType;
    using MapType = typename MetadataTypes::MapType<MetadataValue>;

public:
    Metadata(const Metadata&) = default;
    Metadata(Metadata&&) noexcept = default;

    Metadata();
    Metadata(std::initializer_list<std::pair<const KeyType, MetadataValue>> li);

protected:
    // Used from update()
    Metadata(MapType&& values);

public:
    This& operator=(const This&) = default;
    This& operator=(This&&) noexcept = default;

    MetadataValue&& get(const KeyType& k) &&;
    MetadataValue& get(const KeyType& k) &;
    const MetadataValue& get(const KeyType& k) const&;


    std::optional<MetadataValue> getOpt(const KeyType& k) && noexcept { return optionalGetter(std::move(*this), k); }
    std::optional<MetadataValue> getOpt(const KeyType& k) & noexcept { return optionalGetter(*this, k); }
    std::optional<MetadataValue> getOpt(const KeyType& k) const& noexcept { return optionalGetter(*this, k); }

    template <typename T>
    T&& get(const KeyType& k) && {
        return std::move(referenceGetter<T>(*this, k).get());
    }

    template <typename T>
    T& get(const KeyType& k) & {
        return referenceGetter<T>(*this, k).get();
    }

    template <typename T>
    const T& get(const KeyType& k) const& {
        return referenceGetter<T>(*this, k).get();
    }

    template <typename T>
    std::optional<T> getOpt(const KeyType& k) && noexcept {
        return optionalGetter<T>(std::move(*this), k);
    }

    template <typename T>
    std::optional<T> getOpt(const KeyType& k) const& noexcept {
        return optionalGetter<T>(*this, k);
    }

    MetadataValue& operator[](const KeyType& key);
    MetadataValue& operator[](KeyType&& key);

    template <typename V>
    void set(KeyType&& k, V&& v) {
        values_.insert_or_assign(std::move(k), std::forward<V>(v));
    }

    template <typename V>
    void set(const KeyType& k, V&& v) {
        values_.insert_or_assign(k, std::forward<V>(v));
    }

    // Adds a value if not already contained
    template <typename V>
    auto trySet(KeyType&& k, V&& v) {
        return values_.try_emplace(std::move(k), std::forward<V>(v));
    }

    // Adds a value if not already contained
    template <typename V>
    auto trySet(const KeyType& k, V&& v) {
        return values_.try_emplace(k, std::forward<V>(v));
    }


    auto find(const KeyType& k) { return values_.find(k); };
    auto find(const KeyType& k) const { return values_.find(k); };
    auto begin() noexcept { return values_.begin(); };
    auto begin() const noexcept { return values_.begin(); };
    auto cbegin() const noexcept { return values_.cbegin(); };
    auto end() noexcept { return values_.end(); };
    auto end() const noexcept { return values_.end(); };
    auto cend() const noexcept { return values_.cend(); };


    bool empty() const noexcept;

    std::size_t size() const noexcept;

    void clear() noexcept;

    /**
     * Extracts all values from other metadata whose keys are not contained yet in this metadata.
     * Explicitly always modifies both metadata.
     */
    void merge(This& other);
    void merge(This&& other);

    /**
     * Adds all Metadata contained in other and returns a Metadata object with key/values that have been overwritten.
     * Existing iterators to this container are invalidated after an update.
     */
    This update(const This& other);
    This update(This&& other);


    std::string toString() const;
    void json(eckit::JSON& j) const;

private:
    MapType values_;

    //----------------------------------------------------------------------------------------------------------------------

    // Implementation details

    template <typename T, typename This_>
    static decltype(auto) referenceGetter(This_&& val, const KeyType& k) {
        if (auto search = val.values_.find(k); search != val.values_.end()) {
            try {
                return std::ref(search->second.template get<T>());
            }
            catch (const MetadataException& err) {
                std::throw_with_nested(MetadataKeyException(k, err.what(), Here()));
            }
        }
        throw MetadataMissingKeyException(k, Here());
    }

    template <typename This_>
    static decltype(auto) referenceGetter(This_&& val, const KeyType& k) {
        if (auto search = val.values_.find(k); search != val.values_.end()) {
            return std::ref(search->second);
        }
        throw MetadataMissingKeyException(k, Here());
    }

    template <typename T, typename This_>
    static std::optional<T> optionalGetter(This_&& val, const KeyType& k) {
        if (auto search = val.values_.find(k); search != val.values_.end()) {
            try {
                if constexpr (std::is_rvalue_reference_v<This_>) {
                    return std::move(search->second.template get<T>());
                }
                else {
                    return search->second.template get<T>();
                }
            }
            catch (const MetadataException& err) {
                std::throw_with_nested(MetadataKeyException(k, err.what(), Here()));
            }
        }
        return std::nullopt;
    }

    template <typename This_>
    static std::optional<MetadataValue> optionalGetter(This_&& val, const KeyType& k) noexcept {
        if (auto search = val.values_.find(k); search != val.values_.end()) {
            if constexpr (std::is_rvalue_reference_v<This_>) {
                return std::optional<MetadataValue>{std::move(search->second)};
            }
            else {
                return std::optional<MetadataValue>{search->second};
            }
        }
        return std::nullopt;
    }

    //----------------------------------------------------------------------------------------------------------------------
};


//----------------------------------------------------------------------------------------------------------------------

eckit::JSON& operator<<(eckit::JSON& json, const Metadata& metadata);

std::ostream& operator<<(std::ostream& os, const Metadata& metadata);


//----------------------------------------------------------------------------------------------------------------------

Metadata metadataFromYAML(const std::string& yamlString);

Metadata toMetadata(const eckit::Value& value);


//----------------------------------------------------------------------------------------------------------------------


}  // namespace multio::message
