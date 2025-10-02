from SCons.Script import Import

Import("env")

def after_upload(source, target, env):
    print("Uploading files...")
    env.Execute("pio run -t uploadfs")

env.AddPostAction("upload", after_upload)