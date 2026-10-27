cmake_minimum_required(VERSION 3.10)
project(LoriTestBuild C)

# Colocar este archivo en TEST/loritest/CMakeLists.txt
# Compilar:  cmake -S TEST/loritest -B TEST/loritest/build && cmake --build TEST/loritest/build
# Ejecutar:  export DISPLAY=:0 && LIBGL_GL=43 TEST/loritest/build/loritest

get_filename_component(TESTDIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

find_package(Threads REQUIRED)

# X11 y GL: find_package si funciona, si no se buscan las librerias a mano
find_package(X11)
if(X11_FOUND)
    set(X11_LIBS ${X11_X11_LIB})
    set(X11_INCS ${X11_INCLUDE_DIR})
else()
    find_library(X11_LIBS X11 REQUIRED)
    set(X11_INCS "")
endif()
find_library(GL_LIB NAMES GL libGL.so.1 REQUIRED)

# Si el repo trae sus propias cabeceras/libreria de Lorica, se usan primero
get_filename_component(ROOT "${TESTDIR}/.." ABSOLUTE)
if(EXISTS ${ROOT}/include)
    include_directories(${ROOT}/include)
endif()
if(EXISTS ${ROOT}/lib/libGL.so.1)
    set(GL_LIB ${ROOT}/lib/libGL.so.1)
endif()

function(lorica_exe name src)
    add_executable(${name} ${src})
    target_include_directories(${name} PRIVATE ${X11_INCS})
    target_link_libraries(${name} ${X11_LIBS} ${GL_LIB} m ${CMAKE_DL_LIBS} Threads::Threads)
endfunction()

lorica_exe(loritest ${TESTDIR}/LoriTest.c)

if(EXISTS ${TESTDIR}/cubo_rubik.c)
    lorica_exe(cubo ${TESTDIR}/cubo_rubik.c)
endif()
