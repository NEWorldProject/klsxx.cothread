cmake_minimum_required(VERSION 3.11)

project(KLSXX)
kls_add_library_module(kls.cothread klsxx::cothread)
kls_public_source_directory(kls.cothread Published)
kls_module_source_directory(kls.cothread Module)

target_link_libraries(kls.cothread PUBLIC kls.coroutine kls.essential)

find_package(Boost REQUIRED context)
target_link_libraries(kls.cothread PRIVATE Boost::boost Boost::context)
