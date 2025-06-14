cmake_minimum_required(VERSION 3.12)

set(TEST_SOURCES
    test_config.c
    test_synth.c
    test_midi.c
)

foreach(testfile ${TEST_SOURCES})
    get_filename_component(testname ${testfile} NAME_WE)
    add_executable(${testname}
        ${testfile}
        stubs.c
    )
    target_include_directories(${testname} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${testname}
        ${FLUIDSYNTH_LIBRARIES}
        ${ALSA_LIBRARIES}
        ${MATH_LIBRARIES}
        ${RT_LIBRARIES}
        Threads::Threads
        cmocka
    )
    add_test(NAME ${testname} COMMAND ${testname})
endforeach()
