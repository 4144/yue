// Copyright 2016 Cheng Zhao. All rights reserved.
// Use of this source code is governed by the license that can be found in the
// LICENSE file.

#ifndef LUA_METATABLE_H_
#define LUA_METATABLE_H_

#include <type_traits>

#include "lua/metatable_internal.h"

namespace lua {

// Defines how the wrapper of RefCounted is destructed.
template<typename T>
struct UserData<T, typename std::enable_if<std::is_base_of<
                       base::subtle::RefCountedBase, T>::value>::type> {
  using Type = T*;
  static inline void Construct(State* state, T** data, T* ptr) {
    ptr->AddRef();
    *data = ptr;
    internal::WrapperTableSet(state, ptr, -1);
  }
  static inline void Destruct(State* state, T** data) {
    (*data)->Release();
  }
};

// Defines how the wrapper of WeakPtr is destructed.
template<typename T>
struct UserData<T, typename std::enable_if<std::is_base_of<
                       base::internal::WeakPtrBase,
                       decltype(((T*)nullptr)->GetWeakPtr())>::value>::type> {
  using Type = base::WeakPtr<T>;
  static inline void Construct(State* state, base::WeakPtr<T>* data, T* ptr) {
    new(data) base::WeakPtr<T>(ptr->GetWeakPtr());
  }
  static inline void Destruct(State* state, base::WeakPtr<T>* data) {
    data->~Type();
  }
};

// Generate metatable for native classes.
template<typename T, typename Enable = void>
struct MetaTable {
};

// Create metatable for RefCounted classes.
template<typename T>
struct MetaTable<T, typename std::enable_if<std::is_base_of<
                        base::subtle::RefCountedBase, T>::value>::type> {
  // Create a new instance of T.
  template<typename... ArgTypes>
  static T* NewInstance(State* state, const ArgTypes&... args) {
    StackAutoReset reset(state);
    T* instance = new T(args...);
    PushNewWrapper(state, instance);
    return instance;
  }

  // Create a new lua wrapper from existing |ptr|.
  static void PushNewWrapper(State* state, T* ptr) {
    NewUserData(state, ptr);
    internal::InheritanceChain<T>::Push(state);
    SetMetaTable(state, -2);
  }

  // Check if current metatable is base of the metatable on top of stack.
  static bool IsBaseOf(State* state) {
    StackAutoReset reset(state);
    base::StringPiece name;
    if (RawGetAndPop(state, -1, "__name", &name) && name == Type<T>::name)
      return true;
    if (!GetMetaTable(state, -1))
      return false;
    RawGet(state, -1, "__index");
    return IsBaseOf(state);
  }
};

// The default type information for RefCounted class.
template<typename T>
struct Type<T*, typename std::enable_if<std::is_base_of<
                    base::subtle::RefCountedBase, T>::value>::type> {
  static constexpr const char* name = Type<T>::name;
  static bool To(State* state, int index, T** out) {
    index = AbsIndex(state, index);
    StackAutoReset reset(state);
    // Verify the type and length.
    if (GetType(state, index) != lua::LuaType::UserData ||
        RawLen(state, index) != sizeof(T**) ||
        !GetMetaTable(state, index))
      return false;
    // Verify fine the inheritance chain.
    if (!MetaTable<T>::IsBaseOf(state))
      return false;
    // Convert pointer to actual class.
    *out = *static_cast<T**>(lua_touserdata(state, index));
    return true;
  }
  static inline void Push(State* state, T* ptr) {
    if (!ptr)
      lua::Push(state, nullptr);
    else if (!internal::WrapperTableGet(state, ptr))
      MetaTable<T>::PushNewWrapper(state, ptr);
  }
};

// The default type information for WeakPtr class.
template<typename T>
struct Type<T*, typename std::enable_if<std::is_base_of<
                    base::internal::WeakPtrBase,
                    decltype(((T*)nullptr)->GetWeakPtr())>::value>::type> {
  static constexpr const char* name = Type<T>::name;
  static bool To(State* state, int index, T** out) {
    index = AbsIndex(state, index);
    StackAutoReset reset(state);
    // Verify the type and length.
    if (GetType(state, index) != lua::LuaType::UserData ||
        RawLen(state, index) != sizeof(base::WeakPtr<T>))
      return false;
    // Convert pointer to actual class.
    auto* wrapper = static_cast<base::WeakPtr<T>*>(
        lua_touserdata(state, index));
    T* ptr = wrapper->get();
    // WeakPtr might be invalidated.
    if (!ptr)
      return false;
    *out = ptr;
    return true;
  }
  static inline void Push(State* state, T* ptr) {
    if (!ptr)
      lua::Push(state, nullptr);
    NewUserData(state, ptr);
    internal::InheritanceChain<T>::Push(state);
    SetMetaTable(state, -2);
  }
};

// Helper to push metatable.
template<typename T>
inline void Push(State* state, MetaTable<T>) {
  internal::InheritanceChain<T>::Push(state);
}

}  // namespace lua

#endif  // LUA_METATABLE_H_
