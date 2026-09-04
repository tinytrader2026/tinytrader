cmake_minimum_required (VERSION 3.15)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()



project(TinyTrader)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# =========================== 选项 ===========================
option(PYTHON_BINDINGS "Build Python bindings using nanobind" OFF)

# =========================== 目录设置 ===========================
set(FMT         ${CMAKE_SOURCE_DIR}/third_party/fmt-12.2.0)
set(FMTLOG      ${CMAKE_SOURCE_DIR}/third_party/fmtlog-2.3.0)
set(MAGIC_ENUM  ${CMAKE_SOURCE_DIR}/third_party/magic_enum-0.9.8)
set(CTP         ${CMAKE_SOURCE_DIR}/third_party/CTP-6.7.11)

include_directories(src ${FMT}/include ${FMTLOG} ${MAGIC_ENUM} ${CTP})

# =========================== 编译选项 ===========================
if (MSVC)
    add_compile_options(/utf-8 /wd4996 /wd4267 /wd4244)
elseif (UNIX)
    add_compile_options(-Wno-deprecated-declarations)
endif()

add_compile_definitions(FMT_HEADER_ONLY FMTLOG_HEADER_ONLY)

# =========================== CTP 库设置 ===========================
if (WIN32)
    set(CTP_LIBS ${CTP}/thosttraderapi_se.lib ${CTP}/thostmduserapi_se.lib)
elseif (UNIX)
    set(CTP_LIBS -L${CTP} -l:thosttraderapi_se.so -l:thostmduserapi_se.so)
endif()

# =========================== 辅助函数 ===========================
function(add_ctp_executable EXAMPLE SOURCE)
    add_executable(${EXAMPLE} ${SOURCE} "src/tinytrader.cpp")
    target_link_libraries(${EXAMPLE} PRIVATE ${CTP_LIBS})
endfunction()

# =========================== C++ 示例 ===========================
foreach(EXAMPLE minimal tick_to_order delay_tasks make_cache dual_moving_average)
    add_ctp_executable(${EXAMPLE} "examples/${EXAMPLE}.cpp")
endforeach()

add_executable(format_time "examples/format_time.cpp")


# =========================== Python 绑定 ===========================
if (PYTHON_BINDINGS)
    message(STATUS "Building Python bindings with nanobind")
    
    find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
    find_package(nanobind CONFIG REQUIRED)
    nanobind_add_module(_tinytrader 
        python/wrapper.cpp
        src/tinytrader.cpp
    )

    # 链接 CTP 库
    target_link_libraries(_tinytrader PRIVATE ${CTP_LIBS})    
       
    if (WIN32)
        set_target_properties(_tinytrader PROPERTIES SUFFIX ".pyd")
    endif()

    # 打包模式 (默认 OFF，即开发模式)
    option(PACKAGE_MODE "Package mode: output to version subdirectory for wheel building" OFF)

    # 非打包模式（开发模式）：拷贝到 python/tinytrader/
    if (NOT PACKAGE_MODE)
        add_custom_command(TARGET _tinytrader POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:_tinytrader>
                "${CMAKE_SOURCE_DIR}/python/tinytrader/$<TARGET_FILE_NAME:_tinytrader>"
            COMMENT "Copying $<TARGET_FILE_NAME:_tinytrader> to python/tinytrader/ for development"
        )
    endif()
endif()