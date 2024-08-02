add_library(debug_op STATIC src/debug_op.c)

target_include_directories(debug_op PUBLIC include)

target_link_libraries(debug_op
    utils
    time_op
    file_op
    privsep_op
    pthreads_op
    os_xml
    error_messages
    os_regex
    string_op
    cjson)