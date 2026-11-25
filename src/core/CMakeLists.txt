add_library(zlexpense_core STATIC
    Money.cpp
    Money.h
    Transaction.cpp
    Transaction.h
    Category.cpp
    Category.h
    DateTimeUtil.cpp
    DateTimeUtil.h
    Database.cpp
    Database.h)

target_include_directories(zlexpense_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(zlexpense_core PUBLIC Qt6::Core Qt6::Sql)
