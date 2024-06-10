cmake_minimum_required(VERSION 3.0)

project(gateway)

enable_language(C ASM)
get_filename_component(ROOT_DIR . ABSOLUTE)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CROSS_COMPILE /home/jinbum/github/islet/assets/toolchain/aarch64-none-elf/bin/aarch64-none-elf-)
set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})

message(C_COMP="${CMAKE_C_COMPILER}")

set(CMAKE_C_FLAGS "-march=armv8.5-a -fpie  -ffunction-sections -fdata-sections -mstrict-align -Os -ffreestanding -Wall -std=gnu99 -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes -Wextra -Wconversion -Wsign-conversion -Wcast-align -Wstrict-overflow -DCMAKE_GNUARM_COMPILE -Wno-packed-bitfield-compat")
set(CMAKE_ASM_FLAGS "-march=armv8.5-a -c -x assembler-with-cpp -Wall -ffunction-sections -fdata-sections -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes -Wextra -Wconversion -Wsign-conversion -Wcast-align -Wstrict-overflow -DCMAKE_GNUARM_COMPILE")

set(GNUARM_LINKER "${CROSS_COMPILE}ld" CACHE FILEPATH "The GNUARM linker" FORCE)
set(GNUARM_LINKER_FLAGS "--fatal-warnings" ${LINKER_PIE_SWITCH} ${LINKER_DEBUG_OPTIONS} "-O1" "--gc-sections" "--build-id=none")
set(GNUARM_OBJDUMP "${CROSS_COMPILE}objdump" CACHE FILEPATH "The GNUARM objdump" FORCE)
set(GNUARM_OBJDUMP_FLAGS    "-dSx")
set(GNUARM_OBJCOPY "${CROSS_COMPILE}objcopy" CACHE FILEPATH "The GNUARM objcopy" FORCE)
set(GNUARM_OBJCOPY_FLAGS    "-Obinary")

file(GLOB SOURCES
    "${ROOT_DIR}/src/*.c"
    "${ROOT_DIR}/src/*.S"
    "${ROOT_DIR}/inc/*.S"
)

set(EXE_NAME cvm_gateway)
set(LIB_NAME gateway)
set(OUTPUT_DIR ${ROOT_DIR}/build)

# 1. build as a library
add_library(gateway STATIC ${SOURCES})
target_include_directories(${LIB_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${ROOT_DIR}/inc
)

function (create_executable)
    set(SCATTER_INPUT_FILE "${ROOT_DIR}/image.ld.S")
    set(SCATTER_OUTPUT_FILE "${OUTPUT_DIR}/image.ld")

    # Preprocess the scatter file for image layout symbols
    add_custom_command(OUTPUT CPP-LD-${EXE_NAME}
                    COMMAND ${CMAKE_C_COMPILER} -E -P -I${ROOT_DIR}/inc/ ${SCATTER_INPUT_FILE} -o ${SCATTER_OUTPUT_FILE}
                    DEPENDS ${LIB_NAME})
    add_custom_target(CPP-LD-${EXE_NAME} ALL DEPENDS CPP-LD-${EXE_NAME})

    # Link the objects
    add_custom_command(OUTPUT ${EXE_NAME}.elf
                    COMMAND ${GNUARM_LINKER} ${CMAKE_LINKER_FLAGS} ${GNUARM_LINKER_FLAGS} -T ${SCATTER_OUTPUT_FILE} -o ${OUTPUT_DIR}/${EXE_NAME}.elf lib${LIB_NAME}.a
                    DEPENDS CPP-LD-${EXE_NAME})
    add_custom_target(${EXE_NAME}_elf ALL DEPENDS ${EXE_NAME}.elf)

    # Create the dump info
    add_custom_command(OUTPUT ${EXE_NAME}.dump
                    COMMAND ${GNUARM_OBJDUMP} ${GNUARM_OBJDUMP_FLAGS} ${OUTPUT_DIR}/${EXE_NAME}.elf > ${OUTPUT_DIR}/${EXE_NAME}.dump
                    DEPENDS ${EXE_NAME}_elf)
    add_custom_target(${EXE_NAME}_dump ALL DEPENDS ${EXE_NAME}.dump)

    # Create the binary
    add_custom_command(OUTPUT ${EXE_NAME}.bin
                    COMMAND ${GNUARM_OBJCOPY} ${GNUARM_OBJCOPY_FLAGS} ${OUTPUT_DIR}/${EXE_NAME}.elf ${OUTPUT_DIR}/${EXE_NAME}.bin
                    DEPENDS ${EXE_NAME}_dump)
    add_custom_target(${EXE_NAME}_bin ALL DEPENDS ${EXE_NAME}.bin)
endfunction()

# 2. create a kvmtool-loadable binary
create_executable()
