find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

qt_add_executable(tst_providers tst_providers.cpp)
target_link_libraries(tst_providers PRIVATE imageclient_core Qt6::Test)
add_test(NAME tst_providers COMMAND tst_providers)
set_tests_properties(tst_providers PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
