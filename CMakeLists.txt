cmake_minimum_required(VERSION 3.20)
project(VisualOS C ASM_NASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_language(ASM_NASM)

#
# --- 1. Toolchain: i686-elf-gcc ---
#
set(CROSS_PREFIX i686-elf-)

set(CMAKE_C_COMPILER ${CROSS_PREFIX}gcc)
set(CMAKE_ASM_COMPILER nasm)
set(LD_EXE            ${CROSS_PREFIX}ld)
set(OBJCOPY_EXE       ${CROSS_PREFIX}objcopy)

#
# --- Find NASM ---
#
find_program(NASM_EXE nasm)
if (NOT NASM_EXE)
    message(FATAL_ERROR "nasm not found. Please install NASM and ensure it is in PATH.")
endif()

message(STATUS "Using cross compiler: ${CMAKE_C_COMPILER}")

#
# --- 2. Source files ---
#
set(SRC_ROOT ${CMAKE_SOURCE_DIR}/src)

set(BOOT_SRC    ${SRC_ROOT}/boot/boot.asm)

set(KERNEL_SRC
    ${SRC_ROOT}/core/kernel.c
    ${SRC_ROOT}/core/terminal.c
    ${SRC_ROOT}/core/fs.c
    ${SRC_ROOT}/core/keyboard.c
    ${SRC_ROOT}/core/input.c
    ${SRC_ROOT}/core/shell.c
    ${SRC_ROOT}/core/string.c
    ${SRC_ROOT}/core/pmm.c
    ${SRC_ROOT}/core/ide.c
    ${SRC_ROOT}/core/fat16.c
    ${SRC_ROOT}/core/elf_loader.c
    ${SRC_ROOT}/core/syscall.c
)

# --- User apps ---
set(SNAKE_SRC ${SRC_ROOT}/apps/snake/snake.c)
set(SNAKE_ELF ${CMAKE_BINARY_DIR}/snake.elf)

set(TEST_SRC ${SRC_ROOT}/apps/test/test.c)
set(TEST_ELF ${CMAKE_BINARY_DIR}/test.elf)

set(BOOT_BIN   ${CMAKE_BINARY_DIR}/boot.bin)
set(KERNEL_ELF ${CMAKE_BINARY_DIR}/kernel.elf)
set(KERNEL_BIN ${CMAKE_BINARY_DIR}/kernel.bin)
set(OS_IMG     ${CMAKE_BINARY_DIR}/os.img)


#
# --- 3. Boot sector: assemble to raw binary ---
#
add_custom_command(
    OUTPUT ${BOOT_BIN}
    COMMAND ${NASM_EXE} -f bin ${BOOT_SRC} -o ${BOOT_BIN}
    DEPENDS ${BOOT_SRC}
    COMMENT "Generating boot.bin"
)
add_custom_target(force_boot ALL DEPENDS ${BOOT_BIN})


#
# --- 4. Kernel: compile C sources to .o ---
#
set(KERNEL_CFLAGS
    -m32
    -ffreestanding
    -fno-pic
    -fno-stack-protector
    -march=i386
    -nostdlib -nostartfiles -nodefaultlibs
    -I${CMAKE_SOURCE_DIR}/include
)

set(KERNEL_OBJS)
foreach(SRC ${KERNEL_SRC})
    get_filename_component(name ${SRC} NAME_WE)
    set(obj ${CMAKE_BINARY_DIR}/${name}.o)
    list(APPEND KERNEL_OBJS ${obj})

    add_custom_command(
        OUTPUT ${obj}
        COMMAND ${CMAKE_C_COMPILER} ${KERNEL_CFLAGS} -c ${SRC} -o ${obj}
        DEPENDS ${SRC}
        COMMENT "Compiling ${name}.c"
    )
endforeach()
add_custom_target(kernel_obj ALL DEPENDS ${KERNEL_OBJS})


#
# --- 5. Link ELF kernel ---
#
add_custom_command(
    OUTPUT ${KERNEL_ELF}
    DEPENDS ${KERNEL_OBJS} ${CMAKE_SOURCE_DIR}/link.ld
    COMMAND ${LD_EXE}
            -m elf_i386
            -T ${CMAKE_SOURCE_DIR}/link.ld
            -nostdlib
            ${KERNEL_OBJS}
            -o ${KERNEL_ELF}
    COMMENT "Linking kernel ELF"
)
add_custom_target(kernel_elf ALL DEPENDS ${KERNEL_ELF})


#
# --- 6. Strip ELF → flat binary kernel ---
#
add_custom_command(
    OUTPUT ${KERNEL_BIN}
    DEPENDS ${KERNEL_ELF}
    COMMAND ${OBJCOPY_EXE} -O binary ${KERNEL_ELF} ${KERNEL_BIN}
    COMMENT "Generating flat kernel binary"
)
add_custom_target(kernel_bin ALL DEPENDS ${KERNEL_BIN})


#
# --- Build snake.elf user program ---
#
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/snake.o
    COMMAND ${CMAKE_C_COMPILER}
            -m32 -ffreestanding -fno-pic -fno-stack-protector
            -nostdlib -nostartfiles -nodefaultlibs
            -I${CMAKE_SOURCE_DIR}/include
            -c ${SNAKE_SRC}
            -o ${CMAKE_BINARY_DIR}/snake.o
    DEPENDS ${SNAKE_SRC}
    COMMENT "Compiling snake.c -> snake.o"
)
add_custom_target(snake_obj ALL DEPENDS ${CMAKE_BINARY_DIR}/snake.o)

add_custom_command(
    OUTPUT ${SNAKE_ELF}
    COMMAND ${LD_EXE}
            -m elf_i386
            -T ${SRC_ROOT}/apps/snake/snake.ld
            ${CMAKE_BINARY_DIR}/snake.o
            -o ${SNAKE_ELF}
    DEPENDS snake_obj ${SRC_ROOT}/apps/snake/snake.ld
    COMMENT "Linking snake.o -> snake.elf"
)
add_custom_target(snake_elf ALL DEPENDS ${SNAKE_ELF})


#
# --- Build test.elf user program ---
#
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test.o
    COMMAND ${CMAKE_C_COMPILER}
            -m32 -ffreestanding -fno-pic -fno-stack-protector
            -nostdlib -nostartfiles -nodefaultlibs
            -I${CMAKE_SOURCE_DIR}/include
            -c ${TEST_SRC}
            -o ${CMAKE_BINARY_DIR}/test.o
    DEPENDS ${TEST_SRC}
    COMMENT "Compiling test.c -> test.o"
)
add_custom_target(test_obj ALL DEPENDS ${CMAKE_BINARY_DIR}/test.o)

add_custom_command(
    OUTPUT ${TEST_ELF}
    COMMAND ${LD_EXE}
            -m elf_i386
            -T ${SRC_ROOT}/apps/test/test.ld
            ${CMAKE_BINARY_DIR}/test.o
            -o ${TEST_ELF}
    DEPENDS test_obj ${SRC_ROOT}/apps/test/test.ld
    COMMENT "Linking test.o -> test.elf"
)
add_custom_target(test_elf ALL DEPENDS ${TEST_ELF})


#
# --- 7. Make bootable OS image ---
#
add_custom_command(
    OUTPUT ${OS_IMG}
    DEPENDS ${BOOT_BIN} ${KERNEL_BIN} ${SNAKE_ELF} ${TEST_ELF}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${OS_IMG}
    COMMAND powershell -ExecutionPolicy Bypass -NoProfile
        -File "${CMAKE_SOURCE_DIR}/scripts/make_image.ps1"
    COMMENT "Creating bootable OS image (with snake.elf & test.elf)"
)
add_custom_target(os-image ALL DEPENDS ${OS_IMG})

#
# --- 8. QEMU run convenience target ---
#
add_custom_target(run-qemu
    COMMAND qemu-system-i386 -drive format=raw,file=${OS_IMG}
    DEPENDS os-image
    USES_TERMINAL
)
