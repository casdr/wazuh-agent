add_library(debug_op src/debug_op.c)

include_directories(include)
include_directories(${CMAKE_SOURCE_DIR}/common/utils/)
include_directories(${CMAKE_SOURCE_DIR}/common/error_messages/include/)
include_directories(${CMAKE_SOURCE_DIR}/common/file_op/include/)
include_directories(${CMAKE_SOURCE_DIR}/common/os_xml/include/)
include_directories(${CMAKE_SOURCE_DIR}/common/privsep_op/include/)
include_directories(${CMAKE_SOURCE_DIR}/common/pthreads_op/include/)
include_directories(${CMAKE_SOURCE_DIR}/common/time_op/include/)

target_link_libraries(debug_op cjson)