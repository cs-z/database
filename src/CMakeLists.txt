add_library(database_lib
    aggregate.cpp
    aggregate.hpp
    ast.cpp
    ast.hpp
    buffer.cpp
    buffer.hpp
    catalog.cpp
    catalog.hpp
    CMakeLists.txt
    common.cpp
    common.hpp
    compile.cpp
    compile.hpp
    error.cpp
    error.hpp
    execute.cpp
    execute.hpp
    expr.cpp
    expr.hpp
    fst.cpp
    fst.hpp
    index.cpp
    iter.cpp
    iter.hpp
    lexer.cpp
    lexer.hpp
    op.cpp
    op.hpp
    os.cpp
    os.hpp
    page.hpp
    parse.cpp
    parse.hpp
    row.cpp
    row.hpp
    sort.cpp
    sort.hpp
    token.cpp
    token.hpp
    type.cpp
    type.hpp
    value.cpp
    value.hpp
)

target_include_directories(database_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(database_lib PRIVATE
    warnings
)

add_executable(database
    main.cpp
)

target_link_libraries(database PRIVATE
    database_lib
    warnings
)
