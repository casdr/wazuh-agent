add_library(os_xml STATIC src/os_xml.c)

target_include_directories(os_xml PUBLIC include)

target_link_libraries(os_xml utils file_op)