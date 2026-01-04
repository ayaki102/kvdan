Import("env")


def generate_compile_commands(*args, **kwargs):
    env.Execute("pio run -t compiledb")


env.AddPostAction("checkprogsize", generate_compile_commands)
