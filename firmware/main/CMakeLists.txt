file(GLOB_RECURSE ALL_SOURCE_FILES "./*")
set(modules_src "")

 foreach(file ${ALL_SOURCE_FILES})
     get_filename_component(extension ${file} EXT)
     if(NOT extension STREQUAL "")
        list(APPEND modules_src ${file})
    endif()
 endforeach()

 file(GLOB_RECURSE ALL_INCLUDE_PATHS "./*")
 set(headers_dirs "")

 foreach(path ${ALL_INCLUDE_PATHS})
     get_filename_component(parentDir ${path} DIRECTORY)

     if(IS_DIRECTORY ${parentDir})
         list(APPEND headers_dirs ${parentDir})
     endif()
 endforeach()

idf_component_register(
                    SRCS
                    "${modules_src}"
                    EMBED_FILES modules/web_file_browser/http_server/header.html modules/web_file_browser/http_server/favicon_b.ico apps/wifi/captive/root.html
                    modules/web_file_browser/http_server/style.css
                    INCLUDE_DIRS
                    "${headers_dirs}")
