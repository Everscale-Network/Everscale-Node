# This script replaces all absolute paths in the generated HTML documentation
# with relative one to make the documentation directory relocatable.
# It can be useful when documentation contains images generated by DIA. 
# The reference to such images is usually an absalute path, that needs to be updated.
#
# Expected input variables
# DOC_OUTPUT_DIR - Directory of the installed documenation

file(GLOB_RECURSE all_doc_files "${DOC_OUTPUT_DIR}/*.html")

foreach(f ${all_doc_files})
    file(READ ${f} f_text)
    string(REPLACE "${DOC_OUTPUT_DIR}/html/" "" modified_f_text "${f_text}")
    file(WRITE "${f}" "${modified_f_text}")
endforeach()
