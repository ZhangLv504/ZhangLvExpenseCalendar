function(add_core_test name source)
    qt_add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE Qt6::Test zlexpense_core)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_core_test(tst_money  tst_money.cpp)
add_core_test(tst_database tst_database.cpp)
