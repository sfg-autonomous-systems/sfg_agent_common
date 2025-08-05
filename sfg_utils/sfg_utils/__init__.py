from . import sfg_utils_py

# Now, explicitly "lift" the symbols you want to be part of the public
# sfg_utils API into the package's top-level namespace.

# This tells Pylance: "The name 'fqn' inside the 'sfg_utils' package
# should be whatever 'fqn' is inside the 'sfg_utils_py' module."
# Pylance will then find the sfg_utils/fqn.pyi stub file and get the types.
fqn = sfg_utils_py.fqn

# Do the same for your other functions for a cleaner API.
get_agent_name = sfg_utils_py.get_agent_name
sanitize_agent_name = sfg_utils_py.sanitize_agent_name

# Optional: Define __all__ to control wildcard imports from your package
__all__ = [
    "fqn",
    "get_agent_name",
    "sanitize_agent_name",
]
