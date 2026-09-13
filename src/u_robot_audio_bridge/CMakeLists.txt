cmake_minimum_required(VERSION 3.8)
project(u_robot_audio_bridge)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

find_package(ament_cmake REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)

set(UNITREE_SDK2_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../sdk/unitree_sdk2")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  set(UNITREE_SDK2_ARCH aarch64)
else()
  set(UNITREE_SDK2_ARCH x86_64)
endif()
find_package(Threads REQUIRED)
# /opt contains an older archive without the uint16 JSON symbol required by
# A2 AudioClient::TtsMaker, so use the device-delivered SDK archive explicitly.
add_library(unitree_sdk2_audio STATIC IMPORTED)
set_target_properties(unitree_sdk2_audio PROPERTIES
  IMPORTED_LOCATION "${UNITREE_SDK2_SOURCE_DIR}/lib/${UNITREE_SDK2_ARCH}/libunitree_sdk2.a"
  INTERFACE_LINK_LIBRARIES "ddsc;ddscxx;Threads::Threads")

add_executable(audio_bridge_node src/audio_bridge_node.cpp)
target_compile_features(audio_bridge_node PUBLIC cxx_std_17)
target_include_directories(audio_bridge_node PRIVATE include)
ament_target_dependencies(audio_bridge_node diagnostic_msgs rclcpp std_msgs std_srvs)

add_executable(a2_audio_backend src/a2_audio_backend.cpp)
target_compile_features(a2_audio_backend PUBLIC cxx_std_17)
target_include_directories(a2_audio_backend PRIVATE include
  "${UNITREE_SDK2_SOURCE_DIR}/include"
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/include"
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/include/ddscxx")
target_link_directories(a2_audio_backend PRIVATE
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/lib/${UNITREE_SDK2_ARCH}")
target_link_libraries(a2_audio_backend unitree_sdk2_audio)

add_executable(a2_voice_service_probe src/a2_voice_service_probe.cpp)
target_compile_features(a2_voice_service_probe PUBLIC cxx_std_17)
target_include_directories(a2_voice_service_probe PRIVATE
  "${UNITREE_SDK2_SOURCE_DIR}/include"
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/include"
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/include/ddscxx")
target_link_directories(a2_voice_service_probe PRIVATE
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/lib/${UNITREE_SDK2_ARCH}")
target_link_libraries(a2_voice_service_probe unitree_sdk2_audio)

add_executable(a2_tts_probe src/a2_tts_probe.cpp)
target_compile_features(a2_tts_probe PUBLIC cxx_std_17)
target_include_directories(a2_tts_probe PRIVATE
  "${UNITREE_SDK2_SOURCE_DIR}/include"
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/include"
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/include/ddscxx")
target_link_directories(a2_tts_probe PRIVATE
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/lib/${UNITREE_SDK2_ARCH}")
target_link_libraries(a2_tts_probe unitree_sdk2_audio)

add_executable(a2_pcm_probe src/a2_pcm_probe.cpp)
target_compile_features(a2_pcm_probe PUBLIC cxx_std_17)
target_include_directories(a2_pcm_probe PRIVATE
  "${UNITREE_SDK2_SOURCE_DIR}/include"
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/include"
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/include/ddscxx")
target_link_directories(a2_pcm_probe PRIVATE
  "${UNITREE_SDK2_SOURCE_DIR}/thirdparty/lib/${UNITREE_SDK2_ARCH}")
target_link_libraries(a2_pcm_probe unitree_sdk2_audio)

install(TARGETS audio_bridge_node a2_audio_backend a2_voice_service_probe a2_tts_probe
  a2_pcm_probe
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})
install(DIRECTORY include/ DESTINATION include)
ament_package()
