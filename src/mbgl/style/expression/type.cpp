#include <mbgl/style/expression/type.hpp>

namespace mbgl {
namespace style {
namespace expression {
namespace type {

bool isGeneric(const Type& t) {
    return t.match(
        [&](const Array& arr) { return isGeneric(arr.itemType); },
        [&](const Typename&) { return true; },
        [&](const auto&) { return false; }
    );
}

Type resolveTypenamesIfPossible(const Type& t, const std::unordered_map<std::string, Type>& map) {
    if (!isGeneric(t)) return t;
    return t.match(
        [&](const Typename& t) -> Type {
            if (map.find(t.getName()) == map.end()) {
                return t;
            } else {
                return map.at(t.getName());
            }
        },
        [&](const Array& arr) {
            return Array(resolveTypenamesIfPossible(arr.itemType, map), arr.N);
        },
        [&](const auto&) { return t; }
    );
}

inline std::string errorMessage(const Type& expected, const Type& t) {
    return {"Expected " + toString(expected) + " but found " + toString(t) + " instead."};
}

optional<std::string> matchType(const Type& expected, const Type& t, TypenameContext& context) {
    if (expected.is<Typename>()) {
        const auto& name = expected.get<Typename>().getName();
        if (
            !isGeneric(t) &&
            !t.is<NullType>() &&
            context.expected.find(name) == context.expected.end()
        ) {
            context.expected.emplace(name, t);
        }
        return {};
    }
    
    if (t.is<Typename>() && isGeneric(expected)) {
        const auto& name = t.get<Typename>().getName();
        if (context.actual.find(name) != context.actual.end()) {
            context.actual.emplace(name, expected);
        }
        return {};
    }
    
    if (t.is<NullType>()) return {};
    
    return expected.match(
        [&] (const Array& expectedArray) -> optional<std::string> {
            if (!t.is<Array>()) { return {errorMessage(expected, t)}; }
            const auto& tArr = t.get<Array>();
            const auto err = matchType(expectedArray.itemType, tArr.itemType, context);
            if (err) return { errorMessage(expected, t) + " (" + (*err) + ")" };
            if (expectedArray.N && expectedArray.N != tArr.N) return { errorMessage(expected, t) };
            return {};
        },
        [&] (const ValueType&) -> optional<std::string> {
            if (t.is<ValueType>()) return {};
            
            const Type members[] = {
                Null,
                Boolean,
                Number,
                String,
                Object,
                Array(Value)
            };
            
            for (const auto& member : members) {
                TypenameContext memberContext;
                const auto err = matchType(member, t, memberContext);
                if (!err) {
                    context.expected.insert(memberContext.expected.begin(), memberContext.expected.end());
                    context.actual.insert(memberContext.actual.begin(), memberContext.actual.end());
                    return {};
                }
            }
            return { errorMessage(expected, t) };
        },
        [&] (const auto&) -> optional<std::string> {
            // TODO silly to stringify for this; implement == / != operators instead.
            if (toString(expected) != toString(t)) {
                return { errorMessage(expected, t) };
            }
            return {};
        }
    );
}


} // namespace type
} // namespace expression
} // namespace style
} // namespace mbgl