#https://github.com/msgpack/msgpack-c
#tag cpp4.1.1

add_library(msgpack INTERFACE) 
target_include_directories(msgpack
  INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  )

install(TARGETS msgpack
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )

# install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
#   DESTINATION include
#   )
