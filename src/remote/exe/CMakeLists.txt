add_executable(comet_wsf_tcpclient comet_wsf_tcpclient.cc)
target_link_libraries(comet_wsf_tcpclient PRIVATE comet_wsf_remote getopt linenoiseng
   Threads::Threads
  $<$<AND:$<CXX_COMPILER_ID:GNU>,$<AND:$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>,$<VERSION_GREATER:$<CXX_COMPILER_VERSION>,5.3>>>:stdc++fs>)

install(TARGETS comet_wsf_tcpclient
  EXPORT ${PROJECT_NAME}Targets
  RUNTIME       DESTINATION bin      COMPONENT runtime
  LIBRARY       DESTINATION lib      COMPONENT runtime
  ARCHIVE       DESTINATION lib      COMPONENT devel
  PUBLIC_HEADER DESTINATION include  COMPONENT devel
  RESOURCE      DESTINATION resource COMPONENT runtime
  )
