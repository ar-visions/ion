cmake_minimum_required(VERSION 3.20)

# this is just so ci can override .cmake files referenced such as FindVulkan which needed a patch
list(INSERT CMAKE_MODULE_PATH 0 "${ION}/ci")
