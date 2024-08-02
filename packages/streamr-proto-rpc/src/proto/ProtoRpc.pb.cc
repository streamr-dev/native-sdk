// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: ProtoRpc.proto

#include "ProtoRpc.pb.h"

#include <algorithm>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// @@protoc_insertion_point(includes)
#include <google/protobuf/port_def.inc>

PROTOBUF_PRAGMA_INIT_SEG

namespace _pb = ::PROTOBUF_NAMESPACE_ID;
namespace _pbi = _pb::internal;

namespace protorpc {
PROTOBUF_CONSTEXPR RpcMessage_HeaderEntry_DoNotUse::RpcMessage_HeaderEntry_DoNotUse(
    ::_pbi::ConstantInitialized) {}
struct RpcMessage_HeaderEntry_DoNotUseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR RpcMessage_HeaderEntry_DoNotUseDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~RpcMessage_HeaderEntry_DoNotUseDefaultTypeInternal() {}
  union {
    RpcMessage_HeaderEntry_DoNotUse _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 RpcMessage_HeaderEntry_DoNotUseDefaultTypeInternal _RpcMessage_HeaderEntry_DoNotUse_default_instance_;
PROTOBUF_CONSTEXPR RpcMessage::RpcMessage(
    ::_pbi::ConstantInitialized): _impl_{
    /*decltype(_impl_._has_bits_)*/{}
  , /*decltype(_impl_._cached_size_)*/{}
  , /*decltype(_impl_.header_)*/{::_pbi::ConstantInitialized()}
  , /*decltype(_impl_.requestid_)*/{&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{}}
  , /*decltype(_impl_.errorclassname_)*/{&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{}}
  , /*decltype(_impl_.errorcode_)*/{&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{}}
  , /*decltype(_impl_.errormessage_)*/{&::_pbi::fixed_address_empty_string, ::_pbi::ConstantInitialized{}}
  , /*decltype(_impl_.body_)*/nullptr
  , /*decltype(_impl_.errortype_)*/0} {}
struct RpcMessageDefaultTypeInternal {
  PROTOBUF_CONSTEXPR RpcMessageDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~RpcMessageDefaultTypeInternal() {}
  union {
    RpcMessage _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 RpcMessageDefaultTypeInternal _RpcMessage_default_instance_;
PROTOBUF_CONSTEXPR Mnfo2uhnf92hvqi2nviouq2hv9puhq::Mnfo2uhnf92hvqi2nviouq2hv9puhq(
    ::_pbi::ConstantInitialized): _impl_{
    /*decltype(_impl_.empty_)*/nullptr
  , /*decltype(_impl_._cached_size_)*/{}} {}
struct Mnfo2uhnf92hvqi2nviouq2hv9puhqDefaultTypeInternal {
  PROTOBUF_CONSTEXPR Mnfo2uhnf92hvqi2nviouq2hv9puhqDefaultTypeInternal()
      : _instance(::_pbi::ConstantInitialized{}) {}
  ~Mnfo2uhnf92hvqi2nviouq2hv9puhqDefaultTypeInternal() {}
  union {
    Mnfo2uhnf92hvqi2nviouq2hv9puhq _instance;
  };
};
PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 Mnfo2uhnf92hvqi2nviouq2hv9puhqDefaultTypeInternal _Mnfo2uhnf92hvqi2nviouq2hv9puhq_default_instance_;
}  // namespace protorpc
static ::_pb::Metadata file_level_metadata_ProtoRpc_2eproto[3];
static const ::_pb::EnumDescriptor* file_level_enum_descriptors_ProtoRpc_2eproto[1];
static constexpr ::_pb::ServiceDescriptor const** file_level_service_descriptors_ProtoRpc_2eproto = nullptr;

const uint32_t TableStruct_ProtoRpc_2eproto::offsets[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage_HeaderEntry_DoNotUse, _has_bits_),
  PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage_HeaderEntry_DoNotUse, _internal_metadata_),
  ~0u,  // no _extensions_
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  ~0u,  // no _inlined_string_donated_
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
  PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.header_),
  PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.body_),
  PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.requestid_),
  PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.errortype_),
  PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.errorclassname_),
  PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.errorcode_),
  PROTOBUF_FIELD_OFFSET(::protorpc::RpcMessage, _impl_.errormessage_),
  ~0u,
  ~0u,
  ~0u,
  3,
  0,
  1,
  2,
  ~0u,  // no _has_bits_
  PROTOBUF_FIELD_OFFSET(::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq, _internal_metadata_),
  ~0u,  // no _extensions_
  ~0u,  // no _oneof_case_
  ~0u,  // no _weak_field_map_
  ~0u,  // no _inlined_string_donated_
  PROTOBUF_FIELD_OFFSET(::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq, _impl_.empty_),
};
static const ::_pbi::MigrationSchema schemas[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {
  { 0, 8, -1, sizeof(::protorpc::RpcMessage_HeaderEntry_DoNotUse)},
  { 10, 23, -1, sizeof(::protorpc::RpcMessage)},
  { 30, -1, -1, sizeof(::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq)},
};

static const ::_pb::Message* const file_default_instances[] = {
  &::protorpc::_RpcMessage_HeaderEntry_DoNotUse_default_instance_._instance,
  &::protorpc::_RpcMessage_default_instance_._instance,
  &::protorpc::_Mnfo2uhnf92hvqi2nviouq2hv9puhq_default_instance_._instance,
};

const char descriptor_table_protodef_ProtoRpc_2eproto[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =
  "\n\016ProtoRpc.proto\022\010protorpc\032\033google/proto"
  "buf/empty.proto\032\031google/protobuf/any.pro"
  "to\"\344\002\n\nRpcMessage\0220\n\006header\030\001 \003(\0132 .prot"
  "orpc.RpcMessage.HeaderEntry\022\"\n\004body\030\002 \001("
  "\0132\024.google.protobuf.Any\022\021\n\trequestId\030\003 \001"
  "(\t\022.\n\terrorType\030\004 \001(\0162\026.protorpc.RpcErro"
  "rTypeH\000\210\001\001\022\033\n\016errorClassName\030\005 \001(\tH\001\210\001\001\022"
  "\026\n\terrorCode\030\006 \001(\tH\002\210\001\001\022\031\n\014errorMessage\030"
  "\007 \001(\tH\003\210\001\001\032-\n\013HeaderEntry\022\013\n\003key\030\001 \001(\t\022\r"
  "\n\005value\030\002 \001(\t:\0028\001B\014\n\n_errorTypeB\021\n\017_erro"
  "rClassNameB\014\n\n_errorCodeB\017\n\r_errorMessag"
  "e\"G\n\036Mnfo2uhnf92hvqi2nviouq2hv9puhq\022%\n\005e"
  "mpty\030\001 \001(\0132\026.google.protobuf.Empty*r\n\014Rp"
  "cErrorType\022\022\n\016SERVER_TIMEOUT\020\000\022\022\n\016CLIENT"
  "_TIMEOUT\020\001\022\026\n\022UNKNOWN_RPC_METHOD\020\002\022\020\n\014CL"
  "IENT_ERROR\020\003\022\020\n\014SERVER_ERROR\020\004B\002H\002b\006prot"
  "o3"
  ;
static const ::_pbi::DescriptorTable* const descriptor_table_ProtoRpc_2eproto_deps[2] = {
  &::descriptor_table_google_2fprotobuf_2fany_2eproto,
  &::descriptor_table_google_2fprotobuf_2fempty_2eproto,
};
static ::_pbi::once_flag descriptor_table_ProtoRpc_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_ProtoRpc_2eproto = {
    false, false, 642, descriptor_table_protodef_ProtoRpc_2eproto,
    "ProtoRpc.proto",
    &descriptor_table_ProtoRpc_2eproto_once, descriptor_table_ProtoRpc_2eproto_deps, 2, 3,
    schemas, file_default_instances, TableStruct_ProtoRpc_2eproto::offsets,
    file_level_metadata_ProtoRpc_2eproto, file_level_enum_descriptors_ProtoRpc_2eproto,
    file_level_service_descriptors_ProtoRpc_2eproto,
};
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_ProtoRpc_2eproto_getter() {
  return &descriptor_table_ProtoRpc_2eproto;
}

// Force running AddDescriptors() at dynamic initialization time.
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2 static ::_pbi::AddDescriptorsRunner dynamic_init_dummy_ProtoRpc_2eproto(&descriptor_table_ProtoRpc_2eproto);
namespace protorpc {
const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor* RpcErrorType_descriptor() {
  ::PROTOBUF_NAMESPACE_ID::internal::AssignDescriptors(&descriptor_table_ProtoRpc_2eproto);
  return file_level_enum_descriptors_ProtoRpc_2eproto[0];
}
bool RpcErrorType_IsValid(int value) {
  switch (value) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
      return true;
    default:
      return false;
  }
}


// ===================================================================

RpcMessage_HeaderEntry_DoNotUse::RpcMessage_HeaderEntry_DoNotUse() {}
RpcMessage_HeaderEntry_DoNotUse::RpcMessage_HeaderEntry_DoNotUse(::PROTOBUF_NAMESPACE_ID::Arena* arena)
    : SuperType(arena) {}
void RpcMessage_HeaderEntry_DoNotUse::MergeFrom(const RpcMessage_HeaderEntry_DoNotUse& other) {
  MergeFromInternal(other);
}
::PROTOBUF_NAMESPACE_ID::Metadata RpcMessage_HeaderEntry_DoNotUse::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_ProtoRpc_2eproto_getter, &descriptor_table_ProtoRpc_2eproto_once,
      file_level_metadata_ProtoRpc_2eproto[0]);
}

// ===================================================================

class RpcMessage::_Internal {
 public:
  using HasBits = decltype(std::declval<RpcMessage>()._impl_._has_bits_);
  static const ::PROTOBUF_NAMESPACE_ID::Any& body(const RpcMessage* msg);
  static void set_has_errortype(HasBits* has_bits) {
    (*has_bits)[0] |= 8u;
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

const ::PROTOBUF_NAMESPACE_ID::Any&
RpcMessage::_Internal::body(const RpcMessage* msg) {
  return *msg->_impl_.body_;
}
void RpcMessage::clear_body() {
  if (GetArenaForAllocation() == nullptr && _impl_.body_ != nullptr) {
    delete _impl_.body_;
  }
  _impl_.body_ = nullptr;
}
RpcMessage::RpcMessage(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor(arena, is_message_owned);
  if (arena != nullptr && !is_message_owned) {
    arena->OwnCustomDestructor(this, &RpcMessage::ArenaDtor);
  }
  // @@protoc_insertion_point(arena_constructor:protorpc.RpcMessage)
}
RpcMessage::RpcMessage(const RpcMessage& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  RpcMessage* const _this = this; (void)_this;
  new (&_impl_) Impl_{
      decltype(_impl_._has_bits_){from._impl_._has_bits_}
    , /*decltype(_impl_._cached_size_)*/{}
    , /*decltype(_impl_.header_)*/{}
    , decltype(_impl_.requestid_){}
    , decltype(_impl_.errorclassname_){}
    , decltype(_impl_.errorcode_){}
    , decltype(_impl_.errormessage_){}
    , decltype(_impl_.body_){nullptr}
    , decltype(_impl_.errortype_){}};

  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  _this->_impl_.header_.MergeFrom(from._impl_.header_);
  _impl_.requestid_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.requestid_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  if (!from._internal_requestid().empty()) {
    _this->_impl_.requestid_.Set(from._internal_requestid(), 
      _this->GetArenaForAllocation());
  }
  _impl_.errorclassname_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.errorclassname_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  if (from._internal_has_errorclassname()) {
    _this->_impl_.errorclassname_.Set(from._internal_errorclassname(), 
      _this->GetArenaForAllocation());
  }
  _impl_.errorcode_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.errorcode_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  if (from._internal_has_errorcode()) {
    _this->_impl_.errorcode_.Set(from._internal_errorcode(), 
      _this->GetArenaForAllocation());
  }
  _impl_.errormessage_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.errormessage_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  if (from._internal_has_errormessage()) {
    _this->_impl_.errormessage_.Set(from._internal_errormessage(), 
      _this->GetArenaForAllocation());
  }
  if (from._internal_has_body()) {
    _this->_impl_.body_ = new ::PROTOBUF_NAMESPACE_ID::Any(*from._impl_.body_);
  }
  _this->_impl_.errortype_ = from._impl_.errortype_;
  // @@protoc_insertion_point(copy_constructor:protorpc.RpcMessage)
}

inline void RpcMessage::SharedCtor(
    ::_pb::Arena* arena, bool is_message_owned) {
  (void)arena;
  (void)is_message_owned;
  new (&_impl_) Impl_{
      decltype(_impl_._has_bits_){}
    , /*decltype(_impl_._cached_size_)*/{}
    , /*decltype(_impl_.header_)*/{::_pbi::ArenaInitialized(), arena}
    , decltype(_impl_.requestid_){}
    , decltype(_impl_.errorclassname_){}
    , decltype(_impl_.errorcode_){}
    , decltype(_impl_.errormessage_){}
    , decltype(_impl_.body_){nullptr}
    , decltype(_impl_.errortype_){0}
  };
  _impl_.requestid_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.requestid_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  _impl_.errorclassname_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.errorclassname_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  _impl_.errorcode_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.errorcode_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
  _impl_.errormessage_.InitDefault();
  #ifdef PROTOBUF_FORCE_COPY_DEFAULT_STRING
    _impl_.errormessage_.Set("", GetArenaForAllocation());
  #endif // PROTOBUF_FORCE_COPY_DEFAULT_STRING
}

RpcMessage::~RpcMessage() {
  // @@protoc_insertion_point(destructor:protorpc.RpcMessage)
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    ArenaDtor(this);
    return;
  }
  SharedDtor();
}

inline void RpcMessage::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  _impl_.header_.Destruct();
  _impl_.header_.~MapField();
  _impl_.requestid_.Destroy();
  _impl_.errorclassname_.Destroy();
  _impl_.errorcode_.Destroy();
  _impl_.errormessage_.Destroy();
  if (this != internal_default_instance()) delete _impl_.body_;
}

void RpcMessage::ArenaDtor(void* object) {
  RpcMessage* _this = reinterpret_cast< RpcMessage* >(object);
  _this->_impl_.header_.Destruct();
}
void RpcMessage::SetCachedSize(int size) const {
  _impl_._cached_size_.Set(size);
}

void RpcMessage::InternalSwap(RpcMessage* other) {
  using std::swap;
  GetReflection()->Swap(this, other);}

::PROTOBUF_NAMESPACE_ID::Metadata RpcMessage::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_ProtoRpc_2eproto_getter, &descriptor_table_ProtoRpc_2eproto_once,
      file_level_metadata_ProtoRpc_2eproto[1]);
}

// ===================================================================

class Mnfo2uhnf92hvqi2nviouq2hv9puhq::_Internal {
 public:
  static const ::PROTOBUF_NAMESPACE_ID::Empty& empty(const Mnfo2uhnf92hvqi2nviouq2hv9puhq* msg);
};

const ::PROTOBUF_NAMESPACE_ID::Empty&
Mnfo2uhnf92hvqi2nviouq2hv9puhq::_Internal::empty(const Mnfo2uhnf92hvqi2nviouq2hv9puhq* msg) {
  return *msg->_impl_.empty_;
}
void Mnfo2uhnf92hvqi2nviouq2hv9puhq::clear_empty() {
  if (GetArenaForAllocation() == nullptr && _impl_.empty_ != nullptr) {
    delete _impl_.empty_;
  }
  _impl_.empty_ = nullptr;
}
Mnfo2uhnf92hvqi2nviouq2hv9puhq::Mnfo2uhnf92hvqi2nviouq2hv9puhq(::PROTOBUF_NAMESPACE_ID::Arena* arena,
                         bool is_message_owned)
  : ::PROTOBUF_NAMESPACE_ID::Message(arena, is_message_owned) {
  SharedCtor(arena, is_message_owned);
  // @@protoc_insertion_point(arena_constructor:protorpc.Mnfo2uhnf92hvqi2nviouq2hv9puhq)
}
Mnfo2uhnf92hvqi2nviouq2hv9puhq::Mnfo2uhnf92hvqi2nviouq2hv9puhq(const Mnfo2uhnf92hvqi2nviouq2hv9puhq& from)
  : ::PROTOBUF_NAMESPACE_ID::Message() {
  Mnfo2uhnf92hvqi2nviouq2hv9puhq* const _this = this; (void)_this;
  new (&_impl_) Impl_{
      decltype(_impl_.empty_){nullptr}
    , /*decltype(_impl_._cached_size_)*/{}};

  _internal_metadata_.MergeFrom<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>(from._internal_metadata_);
  if (from._internal_has_empty()) {
    _this->_impl_.empty_ = new ::PROTOBUF_NAMESPACE_ID::Empty(*from._impl_.empty_);
  }
  // @@protoc_insertion_point(copy_constructor:protorpc.Mnfo2uhnf92hvqi2nviouq2hv9puhq)
}

inline void Mnfo2uhnf92hvqi2nviouq2hv9puhq::SharedCtor(
    ::_pb::Arena* arena, bool is_message_owned) {
  (void)arena;
  (void)is_message_owned;
  new (&_impl_) Impl_{
      decltype(_impl_.empty_){nullptr}
    , /*decltype(_impl_._cached_size_)*/{}
  };
}

Mnfo2uhnf92hvqi2nviouq2hv9puhq::~Mnfo2uhnf92hvqi2nviouq2hv9puhq() {
  // @@protoc_insertion_point(destructor:protorpc.Mnfo2uhnf92hvqi2nviouq2hv9puhq)
  if (auto *arena = _internal_metadata_.DeleteReturnArena<::PROTOBUF_NAMESPACE_ID::UnknownFieldSet>()) {
  (void)arena;
    return;
  }
  SharedDtor();
}

inline void Mnfo2uhnf92hvqi2nviouq2hv9puhq::SharedDtor() {
  GOOGLE_DCHECK(GetArenaForAllocation() == nullptr);
  if (this != internal_default_instance()) delete _impl_.empty_;
}

void Mnfo2uhnf92hvqi2nviouq2hv9puhq::SetCachedSize(int size) const {
  _impl_._cached_size_.Set(size);
}

void Mnfo2uhnf92hvqi2nviouq2hv9puhq::InternalSwap(Mnfo2uhnf92hvqi2nviouq2hv9puhq* other) {
  using std::swap;
  GetReflection()->Swap(this, other);}

::PROTOBUF_NAMESPACE_ID::Metadata Mnfo2uhnf92hvqi2nviouq2hv9puhq::GetMetadata() const {
  return ::_pbi::AssignDescriptors(
      &descriptor_table_ProtoRpc_2eproto_getter, &descriptor_table_ProtoRpc_2eproto_once,
      file_level_metadata_ProtoRpc_2eproto[2]);
}

// @@protoc_insertion_point(namespace_scope)
}  // namespace protorpc
PROTOBUF_NAMESPACE_OPEN
template<> PROTOBUF_NOINLINE ::protorpc::RpcMessage_HeaderEntry_DoNotUse*
Arena::CreateMaybeMessage< ::protorpc::RpcMessage_HeaderEntry_DoNotUse >(Arena* arena) {
  return Arena::CreateMessageInternal< ::protorpc::RpcMessage_HeaderEntry_DoNotUse >(arena);
}
template<> PROTOBUF_NOINLINE ::protorpc::RpcMessage*
Arena::CreateMaybeMessage< ::protorpc::RpcMessage >(Arena* arena) {
  return Arena::CreateMessageInternal< ::protorpc::RpcMessage >(arena);
}
template<> PROTOBUF_NOINLINE ::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq*
Arena::CreateMaybeMessage< ::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq >(Arena* arena) {
  return Arena::CreateMessageInternal< ::protorpc::Mnfo2uhnf92hvqi2nviouq2hv9puhq >(arena);
}
PROTOBUF_NAMESPACE_CLOSE

// @@protoc_insertion_point(global_scope)
#include <google/protobuf/port_undef.inc>
