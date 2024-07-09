if (${APPLE})
    set(HOMEBREW_PREFIX "/opt/homebrew")

    set(LLVM_PREFIX "/opt/homebrew/opt/llvm")

    set(CMAKE_C_COMPILER "${LLVM_PREFIX}/bin/clang")
    set(CMAKE_CXX_COMPILER "${LLVM_PREFIX}/bin/clang++")
    set(ENV{CC} "${CMAKE_C_COMPILER}")
    set(ENV{CXX} "${CMAKE_CXX_COMPILER}")


    set(CMAKE_PREFIX_PATH
        "${LLVM_PREFIX}"
        "${HOMEBREW_PREFIX}"
    )

    list(TRANSFORM CMAKE_PREFIX_PATH APPEND "/include"
         OUTPUT_VARIABLE CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES)
    set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES "${CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES}")

    set(CMAKE_FIND_FRAMEWORK LAST)
    set(CMAKE_FIND_APPBUNDLE LAST)

    add_link_options("-L/opt/homebrew/opt/llvm/lib/c++" "-Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++")
    #set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)
    #set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)
endif()
