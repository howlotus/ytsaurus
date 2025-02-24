
# This file was generated by the build system used internally in the Yandex monorepo.
# Only simple modifications are allowed (adding source-files to targets, adding simple properties
# like target_include_directories). These modifications will be ported to original
# ya.make files by maintainers. Any complex modifications which can't be ported back to the
# original buildsystem will not be accepted.



add_executable(cpp-integration-test-rpc-api)
target_compile_options(cpp-integration-test-rpc-api PRIVATE
  -Wdeprecated-this-capture
)
target_link_libraries(cpp-integration-test-rpc-api PUBLIC
  contrib-libs-linux-headers
  contrib-libs-cxxsupp
  yutil
  library-cpp-cpuid_check
  cpp-testing-gtest
  cpp-testing-gtest_main
  tests-cpp-test_base
  yt-yt-ytlib
  yt-core-test_framework
  yt-library-named_value
  libs-apache-arrow
)
target_link_options(cpp-integration-test-rpc-api PRIVATE
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
target_sources(cpp-integration-test-rpc-api PRIVATE
  ${CMAKE_SOURCE_DIR}/yt/yt/tests/cpp/test_rpc_api/test_rpc_api.cpp
  ${CMAKE_SOURCE_DIR}/yt/yt/tests/cpp/test_type_v3.cpp
  ${CMAKE_SOURCE_DIR}/yt/yt/tests/cpp/test_api.cpp
  ${CMAKE_SOURCE_DIR}/yt/yt/tests/cpp/test_queues.cpp
  ${CMAKE_SOURCE_DIR}/yt/yt/tests/cpp/modify_rows_test.cpp
)
add_test(
  NAME
  cpp-integration-test-rpc-api
  COMMAND
  cpp-integration-test-rpc-api
)
set_property(
  TEST
  cpp-integration-test-rpc-api
  PROPERTY
  LABELS
  LARGE
)
set_property(
  TEST
  cpp-integration-test-rpc-api
  PROPERTY
  PROCESSORS
  4
)
set_property(
  TEST
  cpp-integration-test-rpc-api
  PROPERTY
  ENVIRONMENT
  USE_YT_RECIPE_BUILT_FROM_SOURCE=yes
)
target_allocator(cpp-integration-test-rpc-api
  cpp-ytalloc-impl
)
vcs_info(cpp-integration-test-rpc-api)
