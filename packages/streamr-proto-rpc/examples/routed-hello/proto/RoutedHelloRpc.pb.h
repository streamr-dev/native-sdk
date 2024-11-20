// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: RoutedHelloRpc.proto
// Protobuf C++ Version: 4.25.1

#ifndef GOOGLE_PROTOBUF_INCLUDED_RoutedHelloRpc_2eproto_2epb_2eh
#define GOOGLE_PROTOBUF_INCLUDED_RoutedHelloRpc_2eproto_2epb_2eh

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/port_def.inc"
#if PROTOBUF_VERSION < 4025000
#error "This file was generated by a newer version of protoc which is"
#error "incompatible with your Protocol Buffer headers. Please update"
#error "your headers."
#endif  // PROTOBUF_VERSION

#if 4025001 < PROTOBUF_MIN_PROTOC_VERSION
#error "This file was generated by an older version of protoc which is"
#error "incompatible with your Protocol Buffer headers. Please"
#error "regenerate this file with a newer version of protoc."
#endif  // PROTOBUF_MIN_PROTOC_VERSION
#include "google/protobuf/port_undef.inc"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/arenastring.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/metadata_lite.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/message.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_RoutedHelloRpc_2eproto

namespace google {
namespace protobuf {
namespace internal {
class AnyMetadata;
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_RoutedHelloRpc_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_RoutedHelloRpc_2eproto;
class RoutedHelloRequest;
struct RoutedHelloRequestDefaultTypeInternal;
extern RoutedHelloRequestDefaultTypeInternal _RoutedHelloRequest_default_instance_;
class RoutedHelloResponse;
struct RoutedHelloResponseDefaultTypeInternal;
extern RoutedHelloResponseDefaultTypeInternal _RoutedHelloResponse_default_instance_;
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google


// ===================================================================


// -------------------------------------------------------------------

class RoutedHelloResponse final :
    public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:RoutedHelloResponse) */ {
 public:
  inline RoutedHelloResponse() : RoutedHelloResponse(nullptr) {}
  ~RoutedHelloResponse() override;
  template<typename = void>
  explicit PROTOBUF_CONSTEXPR RoutedHelloResponse(::google::protobuf::internal::ConstantInitialized);

  inline RoutedHelloResponse(const RoutedHelloResponse& from)
      : RoutedHelloResponse(nullptr, from) {}
  RoutedHelloResponse(RoutedHelloResponse&& from) noexcept
    : RoutedHelloResponse() {
    *this = ::std::move(from);
  }

  inline RoutedHelloResponse& operator=(const RoutedHelloResponse& from) {
    CopyFrom(from);
    return *this;
  }
  inline RoutedHelloResponse& operator=(RoutedHelloResponse&& from) noexcept {
    if (this == &from) return *this;
    if (GetArena() == from.GetArena()
  #ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetArena() != nullptr
  #endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const RoutedHelloResponse& default_instance() {
    return *internal_default_instance();
  }
  static inline const RoutedHelloResponse* internal_default_instance() {
    return reinterpret_cast<const RoutedHelloResponse*>(
               &_RoutedHelloResponse_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    1;

  friend void swap(RoutedHelloResponse& a, RoutedHelloResponse& b) {
    a.Swap(&b);
  }
  inline void Swap(RoutedHelloResponse* other) {
    if (other == this) return;
  #ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() != nullptr &&
        GetArena() == other->GetArena()) {
   #else  // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() == other->GetArena()) {
  #endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(RoutedHelloResponse* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  RoutedHelloResponse* New(::google::protobuf::Arena* arena = nullptr) const final {
    return CreateMaybeMessage<RoutedHelloResponse>(arena);
  }
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  ::google::protobuf::internal::CachedSize* AccessCachedSize() const final;
  void SharedCtor(::google::protobuf::Arena* arena);
  void SharedDtor();
  void InternalSwap(RoutedHelloResponse* other);

  private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() {
    return "RoutedHelloResponse";
  }
  protected:
  explicit RoutedHelloResponse(::google::protobuf::Arena* arena);
  RoutedHelloResponse(::google::protobuf::Arena* arena, const RoutedHelloResponse& from);
  public:

  ::google::protobuf::Metadata GetMetadata() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  enum : int {
    kGreetingFieldNumber = 1,
  };
  // string greeting = 1;
  void clear_greeting() ;
  const std::string& greeting() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_greeting(Arg_&& arg, Args_... args);
  std::string* mutable_greeting();
  PROTOBUF_NODISCARD std::string* release_greeting();
  void set_allocated_greeting(std::string* value);

  private:
  const std::string& _internal_greeting() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_greeting(
      const std::string& value);
  std::string* _internal_mutable_greeting();

  public:
  // @@protoc_insertion_point(class_scope:RoutedHelloResponse)
 private:
  class _Internal;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {

        inline explicit constexpr Impl_(
            ::google::protobuf::internal::ConstantInitialized) noexcept;
        inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                              ::google::protobuf::Arena* arena);
        inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                              ::google::protobuf::Arena* arena, const Impl_& from);
    ::google::protobuf::internal::ArenaStringPtr greeting_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_RoutedHelloRpc_2eproto;
};// -------------------------------------------------------------------

class RoutedHelloRequest final :
    public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:RoutedHelloRequest) */ {
 public:
  inline RoutedHelloRequest() : RoutedHelloRequest(nullptr) {}
  ~RoutedHelloRequest() override;
  template<typename = void>
  explicit PROTOBUF_CONSTEXPR RoutedHelloRequest(::google::protobuf::internal::ConstantInitialized);

  inline RoutedHelloRequest(const RoutedHelloRequest& from)
      : RoutedHelloRequest(nullptr, from) {}
  RoutedHelloRequest(RoutedHelloRequest&& from) noexcept
    : RoutedHelloRequest() {
    *this = ::std::move(from);
  }

  inline RoutedHelloRequest& operator=(const RoutedHelloRequest& from) {
    CopyFrom(from);
    return *this;
  }
  inline RoutedHelloRequest& operator=(RoutedHelloRequest&& from) noexcept {
    if (this == &from) return *this;
    if (GetArena() == from.GetArena()
  #ifdef PROTOBUF_FORCE_COPY_IN_MOVE
        && GetArena() != nullptr
  #endif  // !PROTOBUF_FORCE_COPY_IN_MOVE
    ) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const RoutedHelloRequest& default_instance() {
    return *internal_default_instance();
  }
  static inline const RoutedHelloRequest* internal_default_instance() {
    return reinterpret_cast<const RoutedHelloRequest*>(
               &_RoutedHelloRequest_default_instance_);
  }
  static constexpr int kIndexInFileMessages =
    0;

  friend void swap(RoutedHelloRequest& a, RoutedHelloRequest& b) {
    a.Swap(&b);
  }
  inline void Swap(RoutedHelloRequest* other) {
    if (other == this) return;
  #ifdef PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() != nullptr &&
        GetArena() == other->GetArena()) {
   #else  // PROTOBUF_FORCE_COPY_IN_SWAP
    if (GetArena() == other->GetArena()) {
  #endif  // !PROTOBUF_FORCE_COPY_IN_SWAP
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(RoutedHelloRequest* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  RoutedHelloRequest* New(::google::protobuf::Arena* arena = nullptr) const final {
    return CreateMaybeMessage<RoutedHelloRequest>(arena);
  }
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  ::google::protobuf::internal::CachedSize* AccessCachedSize() const final;
  void SharedCtor(::google::protobuf::Arena* arena);
  void SharedDtor();
  void InternalSwap(RoutedHelloRequest* other);

  private:
  friend class ::google::protobuf::internal::AnyMetadata;
  static ::absl::string_view FullMessageName() {
    return "RoutedHelloRequest";
  }
  protected:
  explicit RoutedHelloRequest(::google::protobuf::Arena* arena);
  RoutedHelloRequest(::google::protobuf::Arena* arena, const RoutedHelloRequest& from);
  public:

  ::google::protobuf::Metadata GetMetadata() const final;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  enum : int {
    kMyNameFieldNumber = 1,
  };
  // string myName = 1;
  void clear_myname() ;
  const std::string& myname() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_myname(Arg_&& arg, Args_... args);
  std::string* mutable_myname();
  PROTOBUF_NODISCARD std::string* release_myname();
  void set_allocated_myname(std::string* value);

  private:
  const std::string& _internal_myname() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_myname(
      const std::string& value);
  std::string* _internal_mutable_myname();

  public:
  // @@protoc_insertion_point(class_scope:RoutedHelloRequest)
 private:
  class _Internal;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {

        inline explicit constexpr Impl_(
            ::google::protobuf::internal::ConstantInitialized) noexcept;
        inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                              ::google::protobuf::Arena* arena);
        inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                              ::google::protobuf::Arena* arena, const Impl_& from);
    ::google::protobuf::internal::ArenaStringPtr myname_;
    mutable ::google::protobuf::internal::CachedSize _cached_size_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_RoutedHelloRpc_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// RoutedHelloRequest

// string myName = 1;
inline void RoutedHelloRequest::clear_myname() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.myname_.ClearToEmpty();
}
inline const std::string& RoutedHelloRequest::myname() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:RoutedHelloRequest.myName)
  return _internal_myname();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void RoutedHelloRequest::set_myname(Arg_&& arg,
                                                     Args_... args) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ;
  _impl_.myname_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:RoutedHelloRequest.myName)
}
inline std::string* RoutedHelloRequest::mutable_myname() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_myname();
  // @@protoc_insertion_point(field_mutable:RoutedHelloRequest.myName)
  return _s;
}
inline const std::string& RoutedHelloRequest::_internal_myname() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.myname_.Get();
}
inline void RoutedHelloRequest::_internal_set_myname(const std::string& value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ;
  _impl_.myname_.Set(value, GetArena());
}
inline std::string* RoutedHelloRequest::_internal_mutable_myname() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ;
  return _impl_.myname_.Mutable( GetArena());
}
inline std::string* RoutedHelloRequest::release_myname() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  // @@protoc_insertion_point(field_release:RoutedHelloRequest.myName)
  return _impl_.myname_.Release();
}
inline void RoutedHelloRequest::set_allocated_myname(std::string* value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.myname_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.myname_.IsDefault()) {
          _impl_.myname_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:RoutedHelloRequest.myName)
}

// -------------------------------------------------------------------

// RoutedHelloResponse

// string greeting = 1;
inline void RoutedHelloResponse::clear_greeting() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.greeting_.ClearToEmpty();
}
inline const std::string& RoutedHelloResponse::greeting() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:RoutedHelloResponse.greeting)
  return _internal_greeting();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void RoutedHelloResponse::set_greeting(Arg_&& arg,
                                                     Args_... args) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ;
  _impl_.greeting_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:RoutedHelloResponse.greeting)
}
inline std::string* RoutedHelloResponse::mutable_greeting() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_greeting();
  // @@protoc_insertion_point(field_mutable:RoutedHelloResponse.greeting)
  return _s;
}
inline const std::string& RoutedHelloResponse::_internal_greeting() const {
  PROTOBUF_TSAN_READ(&_impl_._tsan_detect_race);
  return _impl_.greeting_.Get();
}
inline void RoutedHelloResponse::_internal_set_greeting(const std::string& value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ;
  _impl_.greeting_.Set(value, GetArena());
}
inline std::string* RoutedHelloResponse::_internal_mutable_greeting() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ;
  return _impl_.greeting_.Mutable( GetArena());
}
inline std::string* RoutedHelloResponse::release_greeting() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  // @@protoc_insertion_point(field_release:RoutedHelloResponse.greeting)
  return _impl_.greeting_.Release();
}
inline void RoutedHelloResponse::set_allocated_greeting(std::string* value) {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  _impl_.greeting_.SetAllocated(value, GetArena());
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
        if (_impl_.greeting_.IsDefault()) {
          _impl_.greeting_.Set("", GetArena());
        }
  #endif  // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  // @@protoc_insertion_point(field_set_allocated:RoutedHelloResponse.greeting)
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_INCLUDED_RoutedHelloRpc_2eproto_2epb_2eh
