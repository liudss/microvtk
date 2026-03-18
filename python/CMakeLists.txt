nanobind_add_module(
    _microvtk
    NB_STATIC # Link nanobind statically for simpler distribution
    microvtk_python.cpp
)

# Place the compiled extension inside the Python package directory
set_target_properties(_microvtk PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/microvtk"
)

target_link_libraries(_microvtk PRIVATE microvtk::microvtk)

# Add a thin Python wrapper if needed in the future
# For now, we'll just expose the module directly or via __init__.py
