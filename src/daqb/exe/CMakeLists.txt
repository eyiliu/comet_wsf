add_executable(comet_wsf_ctrl comet_wsf_ctrl.cc)
target_link_libraries(comet_wsf_ctrl PRIVATE comet_wsf_daqb getopt linenoiseng)

add_executable(comet_wsf_datadump comet_wsf_datadump.cc)
target_link_libraries(comet_wsf_datadump PRIVATE getopt linenoiseng
  Threads::Threads
  $<$<AND:$<CXX_COMPILER_ID:GNU>,$<AND:$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>,$<VERSION_GREATER:$<CXX_COMPILER_VERSION>,5.3>>>:stdc++fs>)

install(TARGETS comet_wsf_ctrl comet_wsf_datadump
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
