"""PlatformIO pre-build script: the firmware version comes from one place.

Reads the VERSION file at the repository root. The release workflow sets the
RIPPLE_VERSION environment variable from the git tag instead (and checks that
the two agree).
"""
import os

Import("env")  # noqa: F821  (provided by PlatformIO / SCons)

root = env["PROJECT_DIR"]  # noqa: F821
version = os.environ.get("RIPPLE_VERSION") or open(os.path.join(root, "VERSION")).read().strip()
env.Append(CPPDEFINES=[("RIPPLE_VERSION", env.StringifyMacro(version))])  # noqa: F821
print("Ripple version: %s" % version)
