# build a normal library
#
add_library(nec_receive_library nec_receive.c)

# invoke pio_asm to assemble the state machine program
#
pico_generate_pio_header(nec_receive_library ${CMAKE_CURRENT_LIST_DIR}/nec_receive.pio)

target_link_libraries(nec_receive_library PRIVATE
        pico_stdlib
        hardware_pio
        )
