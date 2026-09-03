import subprocess

from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()


def git_short_sha():
    try:
        sha = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=env["PROJECT_DIR"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
        return sha or "unknown"
    except Exception:
        return "unknown"


env.Append(CPPDEFINES=[("BUILD_GIT_SHA", '\\"%s\\"' % git_short_sha())])
