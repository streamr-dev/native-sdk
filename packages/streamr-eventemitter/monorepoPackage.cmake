
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../MonorepoPackages.cmake)
    message(STATUS "In monorepo sub-package ${CMAKE_CURRENT_SOURCE_DIR}")
    
    # if in monorepo, install vcpkg packages to ../../vcpkg_installed
    set(VCPKG_INSTALLED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg_installed)
    
    # if in monorepo, use the monorepo deps from filesystem
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json)
      file(READ ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json VCPKG_JSON)
      
      string(JSON VCPKG_JSON_DECODED ERROR_VARIABLE parse_error GET ${VCPKG_JSON} "features")
      if(VCPKG_JSON_DECODED)
          string(JSON MONOREPO_EXISTS ERROR_VARIABLE parse_error GET ${VCPKG_JSON_DECODED} "monorepo")
          if(MONOREPO_EXISTS)
              string(JSON MONOREPO_DEPENDENCIES ERROR_VARIABLE parse_error GET ${MONOREPO_EXISTS} "dependencies")
              set(MONOREPO_DEPENDENCIES ${MONOREPO_DEPENDENCIES})
          else()
              set(MONOREPO_DEPENDENCIES "")
          endif()
      else()
          set(MONOREPO_DEPENDENCIES "")
      endif()

      foreach(dependency ${MONOREPO_DEPENDENCIES})
          set(${dependency}_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../${dependency}/build)
      endforeach()
    endif()
else()
    # if not in monorepo, fetch also monorepo deps with vcpkg
    list(APPEND VCPKG_MANIFEST_FEATURES "monorepo")
endif()
