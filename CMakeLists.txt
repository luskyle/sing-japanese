cmake_minimum_required(VERSION 3.16)
project(sing_japanese VERSION 0.3.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Quick Qml Multimedia PrintSupport)
find_path(MECAB_INCLUDE_DIR mecab.h REQUIRED)
find_library(MECAB_LIBRARY NAMES mecab REQUIRED)

file(GLOB AUDIO_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/public/audio/*.ogg")
list(LENGTH AUDIO_FILES AUDIO_COUNT)
if(NOT AUDIO_COUNT EQUAL 71)
  message(FATAL_ERROR "Expected 71 bundled kana recordings, found ${AUDIO_COUNT}")
endif()
set(QRC_CONTENT "<RCC><qresource prefix=\"/audio\">")
foreach(AUDIO_FILE IN LISTS AUDIO_FILES)
  get_filename_component(AUDIO_NAME "${AUDIO_FILE}" NAME)
  string(APPEND QRC_CONTENT "<file alias=\"${AUDIO_NAME}\">${AUDIO_FILE}</file>")
endforeach()
string(APPEND QRC_CONTENT "</qresource></RCC>")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/audio.qrc" "${QRC_CONTENT}")

add_executable(sing-japanese
  app/main.cpp
  app/annotator.cpp
  app/annotator.h
  app/annotationview.cpp
  app/annotationview.h
  app/backend.cpp
  app/backend.h
  app/qml.qrc
  "${CMAKE_CURRENT_BINARY_DIR}/audio.qrc"
)
target_include_directories(sing-japanese PRIVATE "${MECAB_INCLUDE_DIR}")
target_link_libraries(sing-japanese PRIVATE
  Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Multimedia Qt5::PrintSupport "${MECAB_LIBRARY}"
)
target_compile_options(sing-japanese PRIVATE -Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
install(TARGETS sing-japanese RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES public/licenses/IPADIC-NOTICE.txt public/RESOURCE-LICENSE.md
        public/audio-manifest.json
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/sing-japanese")
install(FILES packaging/sing-japanese.desktop
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

set(CPACK_PACKAGE_NAME "sing-japanese")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "轻量日语假名学习与歌词注音工具")
set(CPACK_PACKAGE_CONTACT "luskyle")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "luskyle")
set(CPACK_DEBIAN_PACKAGE_SECTION "education")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
  "libqt5widgets5, libqt5quick5, libqt5qml5, qml-module-qtquick2, qml-module-qtquick-window2, qml-module-qtquick-controls2, qml-module-qtquick-layouts, libqt5multimedia5, libqt5multimedia5-plugins, libqt5printsupport5, libmecab2, mecab-ipadic-utf8, fonts-noto-cjk, gstreamer1.0-plugins-good")
set(CPACK_GENERATOR "DEB")
include(CPack)

enable_testing()
add_executable(sing-japanese-tests
  tests/test_annotator.cpp
  app/annotator.cpp
  app/annotator.h
  app/annotationview.cpp
  app/annotationview.h
  app/backend.cpp
  app/backend.h
  app/qml.qrc
  "${CMAKE_CURRENT_BINARY_DIR}/audio.qrc"
)
target_include_directories(sing-japanese-tests PRIVATE "${MECAB_INCLUDE_DIR}" app)
target_link_libraries(sing-japanese-tests PRIVATE
  Qt5::Widgets Qt5::Quick Qt5::Qml Qt5::Multimedia Qt5::PrintSupport "${MECAB_LIBRARY}"
)
target_compile_options(sing-japanese-tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME sing-japanese-tests COMMAND sing-japanese-tests)
set_tests_properties(sing-japanese-tests PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QML_DISABLE_DISK_CACHE=1")
