set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../MonorepoPackages.cmake)
    message(STATUS "In monorepo sub-package ${CMAKE_CURRENT_SOURCE_DIR}")

    # if in monorepo, do not install vcpkg dependencies
    set(VCPKG_MANIFEST_INSTALL OFF)
   
    # if in monorepo, use the vcpkg dependencies from the monorepo root
    set(OWNPATH ../../build/vcpkg_installed)
    file(REAL_PATH ${OWNPATH} OWNPATH)
    message(STATUS "OWNPATH: ${OWNPATH}")
    set(VCPKG_INSTALLED_DIR ${OWNPATH})

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
                  file(REAL_PATH "../${MONOREPO_DEPENDENCY}/build" MONBUILDDIR)
                  set(${MONOREPO_DEPENDENCY}_DIR ${MONBUILDDIR})
                  list(APPEND CMAKE_PREFIX_PATH ${MONBUILDDIR})
                  list(APPEND CMAKE_PREFIX_PATH "${MONBUILDDIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share")
                  message(STATUS "${MONOREPO_DEPENDENCY}_DIR: ${${MONOREPO_DEPENDENCY}_DIR}")
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
