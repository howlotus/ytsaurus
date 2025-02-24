
# This file was generated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_executable(unittester-data-node)
target_compile_options(unittester-data-node PRIVATE
  -Wdeprecated-this-capture
)
target_link_libraries(unittester-data-node PUBLIC
  contrib-libs-linux-headers
  contrib-libs-cxxsupp
  yutil
  library-cpp-cpuid_check
  cpp-testing-gtest
  cpp-testing-gtest_main
  yt-core-test_framework
  yt-server-node
  yt-server-tools
)
target_link_options(unittester-data-node PRIVATE
  -ldl
  -lrt
  -Wl,--no-as-needed
  -fPIC
  -fPIC
  -lpthread
  -lrt
  -ldl
  -lutil
)
target_sources(unittester-data-node PRIVATE
  ${CMAKE_SOURCE_DIR}/yt/yt/server/node/data_node/unittests/p2p_ut.cpp
  ${CMAKE_SOURCE_DIR}/yt/yt/server/node/data_node/unittests/journal_manager_ut.cpp
)
add_test(
  NAME
  unittester-data-node
  COMMAND
  unittester-data-node
)
set_property(
  TEST
  unittester-data-node
  PROPERTY
  LABELS
  LARGE
)
set_property(
  TEST
  unittester-data-node
  PROPERTY
  PROCESSORS
  1
)
set_property(
  TEST
  unittester-data-node
  PROPERTY
  ENVIRONMENT
  ASAN_OPTIONS=detect_leaks=0
)
target_allocator(unittester-data-node
  cpp-malloc-tcmalloc
  libs-tcmalloc-default
)
vcs_info(unittester-data-node)
