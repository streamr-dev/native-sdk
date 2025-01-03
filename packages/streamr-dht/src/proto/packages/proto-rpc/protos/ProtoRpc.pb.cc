// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: packages/proto-rpc/protos/ProtoRpc.proto

#include "packages/proto-rpc/protos/ProtoRpc.pb.h"

#include <algorithm>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace protorpc {
      template <typename>
PROTOBUF_CONSTEXPR RpcMessage_HeaderEntry_DoNotUse::RpcMessage_HeaderEntry_DoNotUse(::_pbi::ConstantInitialized) {}
struct RpcMessage_HeaderEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR RpcMessage_HeaderEntry_DoNotUseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~RpcMessage_HeaderEntry_DoNotUseDefaultTypeInternal() {}
  union {
    RpcMessage_HeaderEntry_DoNotUse _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 RpcMessage_HeaderEntry_DoNotUseDefaultTypeInternal _RpcMessage_HeaderEntry_DoNotUse_default_instance_;

inline constexpr RpcMessage::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        header_{},
        requestid_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        errorclassname_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        errorcode_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        errormessage_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        body_{nullptr},
        errortype_{static_cast< ::protorpc::RpcErrorType >(0)} {}

template <typename>
PROTOBUF_CONSTEXPR RpcMessage::RpcMessage(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct RpcMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR RpcMessageDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~RpcMessageDefaultTypeInternal() {}
  union {
    RpcMessage _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 RpcMessageDefaultTypeInternal _RpcMessage_default_instance_;

inline constexpr Mnfo2uhnf92hvqi2nviouq2hv9puhq::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        empty_{nullptr} {}

template <typename>
PROTOBUF_CONSTEXPR Mnfo2uhnf92hvqi2nviouq2hv9puhq::Mnfo2uhnf92hvqi2nviouq2hv9puhq(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct Mnfo2uhnf92hvqi2nviouq2hv9puhqDefaultTypeInternal {
  PROTOBUF_CONSTEXPR Mnfo2uhnf92hvqi2nviouq2hv9puhqDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~Mnfo2uhnf92hvqi2nviouq2hv9puhqDefaultTypeInternal() {}
  union {
    Mnfo2uhnf92hvqi2nviouq2hv9puhq _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 Mnfo2uhnf92hvqi2nviouq2hv9puhqDefaultTypeInternal _Mnfo2uhnf92hvqi2nviouq2hv9puhq_default_instance_;
}  // namespace protorpc
static ::_pb::Metadata file_level_metadata_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto[3];
static const ::_pb::EnumDescriptor* file_level_enum_descriptors_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto[1];
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto = nullptr;
const ::uint32_t TableStruct_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(
    protodesc_cold) = {
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage_HeaderEntry_DoNotUse, _has_bits_),
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage_HeaderEntry_DoNotUse, _internal_metadata_),
    ~0u,  // no _extensions_
    ~0u,  // no _oneof_case_
    ~0u,  // no _weak_field_map_
    ~0u,  // no _inlined_string_donated_
    ~0u,  // no _split_
    ~0u,  // no sizeof(Split)
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage_HeaderEntry_DoNotUse, key_),
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage_HeaderEntry_DoNotUse, value_),
    0,
    1,
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_._has_bits_),
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _internal_metadata_),
    ~0u,  // no _extensions_
    ~0u,  // no _oneof_case_
    ~0u,  // no _weak_field_map_
    ~0u,  // no _inlined_string_donated_
    ~0u,  // no _split_
    ~0u,  // no sizeof(Split)
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.header_),
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.body_),
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.requestid_),
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.errortype_),
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.errorclassname_),
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.errorcode_),
    PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.errormessage_),
    ~0u,
    3,
    ~0u,
    4,
    0,
    1,
    2,
    PROTOBUF_FIELD_OFFSET(::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq, _impl_._has_bits_),
    PROTOBUF_FIELD_OFFSET(::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq, _internal_metadata_),
    ~0u,  // no _extensions_
    ~0u,  // no _oneof_case_
    ~0u,  // no _weak_field_map_
    ~0u,  // no _inlined_string_donated_
    ~0u,  // no _split_
    ~0u,  // no sizeof(Split)
    PROTOBUF_FIELD_OFFSET(::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq, _impl_.empty_),
    0,
};

static const ::_pbi::MigrationSchema
    schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
        {0, 10, -1, sizeof(::protorpc::RpcMessage_HeaderEntry_DoNotUse)},
        {12, 27, -1, sizeof(::protorpc::RpcMessage)},
        {34, 43, -1, sizeof(::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq)},
};

static const ::_pb::Message* const file_default_instances[] = {
    &::protorpc::_RpcMessage_HeaderEntry_DoNotUse_default_instance_._instance,
    &::protorpc::_RpcMessage_default_instance_._instance,
    &::protorpc::_Mnfo2uhnf92hvqi2nviouq2hv9puhq_default_instance_._instance,
};
const char descriptor_table_protodef_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
    "\n(packages/proto-rpc/protos/ProtoRpc.pro"
    "to\022\010protorpc\032\033google/protobuf/empty.prot"
    "o\032\031google/protobuf/any.proto\"\344\002\n\nRpcMess"
    "age\0220\n\006header\030\001 \003(\0132 .protorpc.RpcMessag"
    "e.HeaderEntry\022\"\n\004body\030\002 \001(\0132\024.google.pro"
    "tobuf.Any\022\021\n\trequestId\030\003 \001(\t\022.\n\terrorTyp"
    "e\030\004 \001(\0162\026.protorpc.RpcErrorTypeH\000\210\001\001\022\033\n\016"
    "errorClassName\030\005 \001(\tH\001\210\001\001\022\026\n\terrorCode\030\006"
    " \001(\tH\002\210\001\001\022\031\n\014errorMessage\030\007 \001(\tH\003\210\001\001\032-\n\013"
    "HeaderEntry\022\013\n\003key\030\001 \001(\t\022\r\n\005value\030\002 \001(\t:"
    "\0028\001B\014\n\n_errorTypeB\021\n\017_errorClassNameB\014\n\n"
    "_errorCodeB\017\n\r_errorMessage\"G\n\036Mnfo2uhnf"
    "92hvqi2nviouq2hv9puhq\022%\n\005empty\030\001 \001(\0132\026.g"
    "oogle.protobuf.Empty*r\n\014RpcErrorType\022\022\n\016"
    "SERVER_TIMEOUT\020\000\022\022\n\016CLIENT_TIMEOUT\020\001\022\026\n\022"
    "UNKNOWN_RPC_METHOD\020\002\022\020\n\014CLIENT_ERROR\020\003\022\020"
    "\n\014SERVER_ERROR\020\004B\002H\002b\006proto3"
};
static const ::_pbi::DescriptorTable* const descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_deps[2] =
    {
        &::descriptor_table_google_2fprotobuf_2fany_2eproto,
        &::descriptor_table_google_2fprotobuf_2fempty_2eproto,
};
static ::absl::once_flag descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto = {
    false,
    false,
    668,
    descriptor_table_protodef_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto,
    "packages/proto-rpc/protos/ProtoRpc.proto",
    &descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_once,
    descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_deps,
    2,
    3,
    schemas,
    file_default_instances,
    TableStruct_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto::offsets,
    file_level_metadata_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto,
    file_level_enum_descriptors_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto,
    file_level_service_descriptors_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto,
};

// This function exists to be marked as weak.
// It can significantly speed up compilation by breaking up LLVM's SCC
// in the .pb.cc translation units. Large translation units see a
// reduction of more than 35% of walltime for optimized builds. Without
// the weak attribute all the messages in the file, including all the
// vtables and everything they use become part of the same SCC through
// a cycle like:
// GetMetadata -> descriptor table -> default instances ->
//   vtables -> GetMetadata
// By adding a weak function here we break the connection from the
// individual vtables back into the descriptor table.
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_getter() {
  return &descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto;
}
// Force running AddDescriptors() at dynamic initialization time.
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto(&descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto);
namespace protorpc {
const ::google::protobuf::EnumDescriptor* RpcErrorType_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto);
  return file_level_enum_descriptors_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto[0];
}
PROTOBUF_CONSTINIT const uint32_t RpcErrorType_internal_data_[] = {
    327680u, 0u, };
bool RpcErrorType_IsValid(int value) {
  return 0 <= value && value <= 4;
}
// ===================================================================

RpcMessage_HeaderEntry_DoNotUse::RpcMessage_HeaderEntry_DoNotUse() {}
RpcMessage_HeaderEntry_DoNotUse::RpcMessage_HeaderEntry_DoNotUse(::google::protobuf::Arena* arena)
    : SuperType(arena) {}
::google::protobuf::Metadata RpcMessage_HeaderEntry_DoNotUse::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_getter, &descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_once,
      file_level_metadata_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto[0]);
}
// ===================================================================

class RpcMessage::_Internal {
 public:
  using HasBits = decltype(std::declval<RpcMessage>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
    8 * PROTOBUF_FIELD_OFFSET(RpcMessage, _impl_._has_bits_);
  static const ::google::protobuf::Any& body(const RpcMessage* msg);
  static void set_has_body(HasBits* has_bits) {
    (*has_bits)[0] |= 8u;
  }
  static void set_has_errortype(HasBits* has_bits) {
    (*has_bits)[0] |= 16u;
  }
  static void set_has_errorclassname(HasBits* has_bits) {
    (*has_bits)[0] |= 1u;
  }
  static void set_has_errorcode(HasBits* has_bits) {
    (*has_bits)[0] |= 2u;
  }
  static void set_has_errormessage(HasBits* has_bits) {
    (*has_bits)[0] |= 4u;
  }
};

const ::google::protobuf::Any& RpcMessage::_Internal::body(const RpcMessage* msg) {
  return *msg->_impl_.body_;
}
void RpcMessage::clear_body() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.body_ != nullptr) _impl_.body_->Clear();
  _impl_._has_bits_[0] &= ~0x00000008u;
}
RpcMessage::RpcMessage(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:protorpc.RpcMessage)
}
inline PROTOBUF_NDEBUG_INLINE RpcMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        header_{visibility, arena, from.header_},
        requestid_(arena, from.requestid_),
        errorclassname_(arena, from.errorclassname_),
        errorcode_(arena, from.errorcode_),
        errormessage_(arena, from.errormessage_) {}

RpcMessage::RpcMessage(
    ::google::protobuf::Arena* arena,
    const RpcMessage& from)
    : ::google::protobuf::Message(arena) {
  RpcMessage* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.body_ = (cached_has_bits & 0x00000008u)
                ? CreateMaybeMessage<::google::protobuf::Any>(arena, *from._impl_.body_)
                : nullptr;
  _impl_.errortype_ = from._impl_.errortype_;

  // @@protoc_insertion_point(copy_constructor:protorpc.RpcMessage)
}
inline PROTOBUF_NDEBUG_INLINE RpcMessage::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0},
        header_{visibility, arena},
        requestid_(arena),
        errorclassname_(arena),
        errorcode_(arena),
        errormessage_(arena) {}

inline void RpcMessage::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, body_),
           0,
           offsetof(Impl_, errortype_) -
               offsetof(Impl_, body_) +
               sizeof(Impl_::errortype_));
}
RpcMessage::~RpcMessage() {
  // @@protoc_insertion_point(destructor:protorpc.RpcMessage)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void RpcMessage::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.requestid_.Destroy();
  _impl_.errorclassname_.Destroy();
  _impl_.errorcode_.Destroy();
  _impl_.errormessage_.Destroy();
  delete _impl_.body_;
  _impl_.~Impl_();
}

::_pbi::CachedSize* RpcMessage::AccessCachedSize() const {
  return &_impl_._cached_size_;
}
void RpcMessage::InternalSwap(RpcMessage* PROTOBUF_RESTRICT other) {
  using std::swap;
  GetReflection()->Swap(this, other);}

::google::protobuf::Metadata RpcMessage::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_getter, &descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_once,
      file_level_metadata_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto[1]);
}
// ===================================================================

class Mnfo2uhnf92hvqi2nviouq2hv9puhq::_Internal {
 public:
  using HasBits = decltype(std::declval<Mnfo2uhnf92hvqi2nviouq2hv9puhq>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
    8 * PROTOBUF_FIELD_OFFSET(Mnfo2uhnf92hvqi2nviouq2hv9puhq, _impl_._has_bits_);
  static const ::google::protobuf::Empty& empty(const Mnfo2uhnf92hvqi2nviouq2hv9puhq* msg);
  static void set_has_empty(HasBits* has_bits) {
    (*has_bits)[0] |= 1u;
  }
};

const ::google::protobuf::Empty& Mnfo2uhnf92hvqi2nviouq2hv9puhq::_Internal::empty(const Mnfo2uhnf92hvqi2nviouq2hv9puhq* msg) {
  return *msg->_impl_.empty_;
}
void Mnfo2uhnf92hvqi2nviouq2hv9puhq::clear_empty() {
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  if (_impl_.empty_ != nullptr) _impl_.empty_->Clear();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
Mnfo2uhnf92hvqi2nviouq2hv9puhq::Mnfo2uhnf92hvqi2nviouq2hv9puhq(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:protorpc.Mnfo2uhnf92hvqi2nviouq2hv9puhq)
}
inline PROTOBUF_NDEBUG_INLINE Mnfo2uhnf92hvqi2nviouq2hv9puhq::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : _has_bits_{from._has_bits_},
        _cached_size_{0} {}

Mnfo2uhnf92hvqi2nviouq2hv9puhq::Mnfo2uhnf92hvqi2nviouq2hv9puhq(
    ::google::protobuf::Arena* arena,
    const Mnfo2uhnf92hvqi2nviouq2hv9puhq& from)
    : ::google::protobuf::Message(arena) {
  Mnfo2uhnf92hvqi2nviouq2hv9puhq* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.empty_ = (cached_has_bits & 0x00000001u)
                ? CreateMaybeMessage<::google::protobuf::Empty>(arena, *from._impl_.empty_)
                : nullptr;

  // @@protoc_insertion_point(copy_constructor:protorpc.Mnfo2uhnf92hvqi2nviouq2hv9puhq)
}
inline PROTOBUF_NDEBUG_INLINE Mnfo2uhnf92hvqi2nviouq2hv9puhq::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void Mnfo2uhnf92hvqi2nviouq2hv9puhq::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  _impl_.empty_ = {};
}
Mnfo2uhnf92hvqi2nviouq2hv9puhq::~Mnfo2uhnf92hvqi2nviouq2hv9puhq() {
  // @@protoc_insertion_point(destructor:protorpc.Mnfo2uhnf92hvqi2nviouq2hv9puhq)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void Mnfo2uhnf92hvqi2nviouq2hv9puhq::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.empty_;
  _impl_.~Impl_();
}

::_pbi::CachedSize* Mnfo2uhnf92hvqi2nviouq2hv9puhq::AccessCachedSize() const {
  return &_impl_._cached_size_;
}
void Mnfo2uhnf92hvqi2nviouq2hv9puhq::InternalSwap(Mnfo2uhnf92hvqi2nviouq2hv9puhq* PROTOBUF_RESTRICT other) {
  using std::swap;
  GetReflection()->Swap(this, other);}

::google::protobuf::Metadata Mnfo2uhnf92hvqi2nviouq2hv9puhq::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_getter, &descriptor_table_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto_once,
      file_level_metadata_packages_2fproto_2drpc_2fprotos_2fProtoRpc_2eproto[2]);
}
// @@protoc_insertion_point(namespace_scope)
}  // namespace protorpc
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
#include "google/protobuf/port_undef.inc"
