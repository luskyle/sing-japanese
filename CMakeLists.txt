cmake_minimum_required(VERSION 3.16)
project(sing_japanese VERSION 0.3.1 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Quick Qml Multimedia PrintSupport)
find_package(ZLIB REQUIRED)
find_path(MECAB_INCLUDE_DIR mecab.h PATH_SUFFIXES mecab REQUIRED)
find_library(MECAB_LIBRARY NAMES mecab REQUIRED)
find_path(MINIZIP_INCLUDE_DIR zip.h PATH_SUFFIXES minizip REQUIRED)
find_library(MINIZIP_LIBRARY NAMES minizip libminizip REQUIRED)

if(WIN32)
  add_compile_definitions(NOMINMAX)
endif()

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(SING_JAPANESE_DICTIONARY "" CACHE PATH "Compiled UTF-8 IPADIC directory to bundle")
set(SING_JAPANESE_FONT "" CACHE FILEPATH "Noto Sans CJK JP Regular font to bundle")
if(SING_JAPANESE_FONT)
  if(NOT EXISTS "${SING_JAPANESE_FONT}")
    message(FATAL_ERROR "SING_JAPANESE_FONT does not exist")
  endif()
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/font.qrc"
    "<RCC><qresource prefix=\"/fonts\"><file alias=\"NotoSansCJKjp-Regular.otf\">${SING_JAPANESE_FONT}</file></qresource></RCC>")
  set(FONT_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/font.qrc")
endif()

file(GLOB AUDIO_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/public/audio/*.ogg")
list(LENGTH AUDIO_FILES AUDIO_COUNT)
if(NOT AUDIO_COUNT EQUAL 71)
  message(FATAL_ERROR "Expected 71 bundled kana recordings, found ${AUDIO_COUNT}")
endif()
set(QRC_CONTENT "<RCC><qresource prefix=\"/audio\">")
foreach(AUDIO_FILE IN LISTS AUDIO_FILES)
  get_filename_component(AUDIO_NAME "${AUDIO_FILE}" NAME)
  if(WIN32 OR APPLE)
    find_program(FFMPEG_EXECUTABLE ffmpeg REQUIRED)
    get_filename_component(AUDIO_STEM "${AUDIO_FILE}" NAME_WE)
    set(AUDIO_NAME "${AUDIO_STEM}.wav")
    set(WAV_FILE "${CMAKE_CURRENT_BINARY_DIR}/audio/${AUDIO_NAME}")
    if(NOT EXISTS "${WAV_FILE}")
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/audio")
      execute_process(COMMAND "${FFMPEG_EXECUTABLE}" -v error -y -i "${AUDIO_FILE}"
        -c:a pcm_s16le "${WAV_FILE}" RESULT_VARIABLE AUDIO_RESULT)
      if(NOT AUDIO_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to convert ${AUDIO_FILE} to WAV")
      endif()
    endif()
    set(AUDIO_FILE "${WAV_FILE}")
  endif()
  string(APPEND QRC_CONTENT "<file alias=\"${AUDIO_NAME}\">${AUDIO_FILE}</file>")
endforeach()
string(APPEND QRC_CONTENT "</qresource></RCC>")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/audio.qrc" "${QRC_CONTENT}")

add_executable(sing-japanese WIN32 MACOSX_BUNDLE
  app/main.cpp
  app/annotator.cpp
  app/annotator.h
  app/romaji.cpp
  app/romaji.h
  app/documentexport.cpp
  app/documentexport.h
  app/annotationview.cpp
  app/annotationview.h
  app/backend.cpp
  app/backend.h
  app/qml.qrc
  "${CMAKE_CURRENT_BINARY_DIR}/audio.qrc"
  ${FONT_RESOURCE}
)
target_compile_definitions(sing-japanese PRIVATE SING_JAPANESE_VERSION="${PROJECT_VERSION}")
set_target_properties(sing-japanese PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in")
target_include_directories(sing-japanese PRIVATE "${MECAB_INCLUDE_DIR}" "${MINIZIP_INCLUDE_DIR}")
target_link_libraries(sing-japanese PRIVATE
  Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Multimedia Qt5::PrintSupport "${MECAB_LIBRARY}" "${MINIZIP_LIBRARY}" ZLIB::ZLIB
)

include(GNUInstallDirs)
install(TARGETS sing-japanese BUNDLE DESTINATION . RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(SING_JAPANESE_DICTIONARY)
  foreach(DICTIONARY_FILE sys.dic unk.dic char.bin matrix.bin dicrc)
    if(NOT EXISTS "${SING_JAPANESE_DICTIONARY}/${DICTIONARY_FILE}")
      message(FATAL_ERROR "Bundled IPADIC is missing ${DICTIONARY_FILE}")
    endif()
  endforeach()
  file(COPY "${SING_JAPANESE_DICTIONARY}/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/resources/ipadic")
  configure_file(packaging/mecabrc "${CMAKE_CURRENT_BINARY_DIR}/resources/mecabrc" COPYONLY)
  if(APPLE)
    set(RESOURCE_DESTINATION "sing-japanese.app/Contents/Resources")
    add_custom_command(TARGET sing-japanese POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_BINARY_DIR}/resources" "$<TARGET_BUNDLE_DIR:sing-japanese>/Contents/Resources")
  else()
    set(RESOURCE_DESTINATION "${CMAKE_INSTALL_BINDIR}/resources")
  endif()
  install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources/" DESTINATION "${RESOURCE_DESTINATION}")
endif()
install(FILES public/licenses/IPADIC-NOTICE.txt public/RESOURCE-LICENSE.md
        public/audio-manifest.json
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/sing-japanese")
if(UNIX AND NOT APPLE)
  install(FILES packaging/sing-japanese.desktop
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()

set(CPACK_PACKAGE_NAME "sing-japanese")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "轻量日语假名学习与歌词注音工具")
set(CPACK_PACKAGE_CONTACT "luskyle")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "luskyle")
set(CPACK_DEBIAN_PACKAGE_SECTION "education")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
  "libqt5widgets5, libqt5quick5, libqt5qml5, qml-module-qtquick2, qml-module-qtquick-window2, qml-module-qtquick-controls2, qml-module-qtquick-layouts, libqt5multimedia5, libqt5multimedia5-plugins, libqt5printsupport5, libmecab2, libminizip1, mecab-ipadic-utf8, fonts-noto-cjk, gstreamer1.0-plugins-good")
if(APPLE)
  set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "DEB")
endif()
include(CPack)

enable_testing()
add_executable(sing-japanese-tests
  tests/test_annotator.cpp
  app/annotator.cpp
  app/annotator.h
  app/romaji.cpp
  app/romaji.h
  app/documentexport.cpp
  app/documentexport.h
  app/annotationview.cpp
  app/annotationview.h
  app/backend.cpp
  app/backend.h
  app/qml.qrc
  "${CMAKE_CURRENT_BINARY_DIR}/audio.qrc"
  ${FONT_RESOURCE}
)
target_include_directories(sing-japanese-tests PRIVATE "${MECAB_INCLUDE_DIR}" "${MINIZIP_INCLUDE_DIR}" app)
target_link_libraries(sing-japanese-tests PRIVATE
  Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Multimedia Qt5::PrintSupport "${MECAB_LIBRARY}" "${MINIZIP_LIBRARY}" ZLIB::ZLIB
)
add_test(NAME sing-japanese-tests COMMAND sing-japanese-tests)
set_tests_properties(sing-japanese-tests PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_DISABLE_DISK_CACHE=1")

add_executable(sing-japanese-document-tests
  tests/test_documents.cpp
  app/annotator.cpp
  app/romaji.cpp
  app/annotationview.cpp
  app/annotationview.h
  app/documentexport.cpp
  ${FONT_RESOURCE}
)
target_include_directories(sing-japanese-document-tests PRIVATE
  "${MECAB_INCLUDE_DIR}" "${MINIZIP_INCLUDE_DIR}" app)
target_link_libraries(sing-japanese-document-tests PRIVATE
  Qt5::Widgets Qt5::Quick Qt5::PrintSupport "${MECAB_LIBRARY}" "${MINIZIP_LIBRARY}" ZLIB::ZLIB)
add_test(NAME sing-japanese-document-tests COMMAND sing-japanese-document-tests)
set_tests_properties(sing-japanese-document-tests PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

if(SING_JAPANESE_DICTIONARY)
  foreach(APP_TARGET sing-japanese sing-japanese-tests sing-japanese-document-tests)
    if(NOT APPLE OR NOT "${APP_TARGET}" STREQUAL "sing-japanese")
      add_custom_command(TARGET ${APP_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${CMAKE_CURRENT_BINARY_DIR}/resources" "$<TARGET_FILE_DIR:${APP_TARGET}>/resources")
    endif()
  endforeach()
endif()
