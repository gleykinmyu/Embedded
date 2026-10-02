Import("env")

import os

name = env["PIOENV"]
root = env.subst("$PROJECT_DIR")
if name == "esp32_eth01":
    defaults = os.path.join(root, "sdkconfig.defaults.eth01")
else:
    defaults = os.path.join(root, "sdkconfig.defaults")

os.environ["SDKCONFIG_DEFAULTS"] = defaults.replace("\\", "/")

select = os.path.join(root, "board_select.cmake")
with open(select, "w", encoding="ascii") as out:
    out.write('set(APP_BOARD "%s")\n' % name)

print("APP_BOARD=%s" % name)
print("SDKCONFIG_DEFAULTS=%s" % os.environ["SDKCONFIG_DEFAULTS"])
