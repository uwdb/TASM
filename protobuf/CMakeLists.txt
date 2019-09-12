file(GLOB protos *.proto)

protobuf_generate_cpp(PROTO_SRC PROTO_HEADER "${protos}")
message("Header: ${PROTO_HEADER}")
message("Src: ${PROTO_SRC}")
add_library(lightdb_protobuf SHARED ${PROTO_HEADER} ${PROTO_SRC})
