file(GLOB_RECURSE ALL_SOURCE_FILES "./*.c" "./*.cpp")

set(modules_src "")

 foreach(file ${ALL_SOURCE_FILES})
     list(APPEND modules_src ${file})
 endforeach()

 file(GLOB_RECURSE ALL_INCLUDE_PATHS "./*.h")
 set(headers_dirs "")

 foreach(path ${ALL_INCLUDE_PATHS})
     get_filename_component(parentDir ${path} DIRECTORY)

     if(IS_DIRECTORY ${parentDir})
         list(APPEND headers_dirs ${parentDir})
     endif()
 endforeach()


idf_component_register(SRCS "${modules_src}"
                    INCLUDE_DIRS "${headers_dirs}"
                    PRIV_REQUIRES esp_wifi nvs_flash bt)
