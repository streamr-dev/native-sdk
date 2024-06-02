
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../MonorepoPackages.cmake)
    message(STATUS "In monorepo sub-package ${CMAKE_CURRENT_SOURCE_DIR}")
    
    # if in monorepo, use the monorepo deps from filesystem
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json)
      file(READ ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json VCPKG_JSON)
      
      string(JSON VCPKG_JSON_DECODED ERROR_VARIABLE parse_error GET ${VCPKG_JSON} "features")
      if(VCPKG_JSON_DECODED)
          string(JSON MONOREPO_EXISTS ERROR_VARIABLE parse_error GET ${VCPKG_JSON_DECODED} "monorepo")
          if(MONOREPO_EXISTS)
              string(JSON MONOREPO_DEPENDENCIES ERROR_VARIABLE parse_error GET ${MONOREPO_EXISTS} "dependencies")
              set(MONOREPO_DEPENDENCIES ${MONOREPO_DEPENDENCIES})
              
              string(JSON MONOREPO_DEPENDENCIES_LENGTH ERROR_VARIABLE parse_error LENGTH ${MONOREPO_DEPENDENCIES})
              math(EXPR END_INDEX "${MONOREPO_DEPENDENCIES_LENGTH}-1")
              message(STATUS "END_INDEX: ${END_INDEX}")
              foreach(idx RANGE "${END_INDEX}")
                  string(JSON MONOREPO_DEPENDENCY ERROR_VARIABLE parse_error GET ${MONOREPO_DEPENDENCIES} "${idx}")
                  set(${MONOREPO_DEPENDENCY}_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../${MONOREPO_DEPENDENCY}/build)         
                  message(STATUS "${MONOREPO_DEPENDENCY}_ROOT: ${${MONOREPO_DEPENDENCY}_ROOT}")
            endforeach()
          endif()
      else()
          set(MONOREPO_DEPENDENCIES "")
      endif()
    endif()
else()
    # if not in monorepo, fetch also monorepo deps with vcpkg
    list(APPEND VCPKG_MANIFEST_FEATURES "monorepo")
endif()
