set(srcs "main.c")

idf_component_register(
    SRCS "${srcs}"      # list the source files of this component
    INCLUDE_DIRS     "."# optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES        # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
