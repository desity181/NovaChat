# Produces NovaChat_Proto static library:
#   - Protobuf-generated code from .proto files
#   - ProtocolCodec (encode/decode binary frames)
# Both client and server link this library.

set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/common.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/auth.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/contact.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/chat.proto
)

# vcpkg protobuf provides protobuf_generate() — not the legacy protobuf_generate_cpp()
protobuf_generate(
    LANGUAGE cpp
    OUT_VAR PROTO_GENERATED_FILES
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(NovaChat_Proto STATIC
    ${PROTO_GENERATED_FILES}
    ProtocolCodec.cpp
)

target_include_directories(NovaChat_Proto PUBLIC
    ${CMAKE_SOURCE_DIR}           # enables #include "proto/MessageType.h" etc.
    ${CMAKE_CURRENT_BINARY_DIR}   # generated .pb.h files
)

target_link_libraries(NovaChat_Proto PUBLIC
    protobuf::libprotobuf
    Qt6::Core   # Q_DECLARE_METATYPE requires QMetaType
)
