# Source dir CMAKE

set (RTUTIL_EXE rtutil)

add_executable (${RTUTIL_EXE}
  ${CMAKE_CURRENT_SOURCE_DIR}/audio_device.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/play_audio_file.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/record_audio_file.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cc)
target_include_directories (${RTUTIL_EXE} PRIVATE
  ${PROJECT_INCLUDE_DIRS})
target_link_libraries (${RTUTIL_EXE} PRIVATE
  ${PROJECT_LIBRARIES})


if (WIN32)
  add_custom_command(TARGET ${RTUTIL_EXE} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${RTUTIL_EXE}> $<TARGET_FILE_DIR:${RTUTIL_EXE}>
    COMMAND_EXPAND_LISTS)
endif()
