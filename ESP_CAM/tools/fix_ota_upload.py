# PlatformIO (espidf): IP в upload_port принудительно включает upload_protocol=espota,
# а UPLOADER берётся из framework-arduinoespressif32 (его нет) → TypeError.
# Переносим IP в CUSTOM_ESPOTA_IP и очищаем UPLOAD_PORT до builder/main.py.

Import("env")  # noqa: F821

import re

_ip_or_local = re.compile(
    r"\"?((([0-9]{1,3}\.){3}[0-9]{1,3})|[^\\/]+\.local)\"?$"
)

port = env.subst("$UPLOAD_PORT")  # noqa: F821
if port and _ip_or_local.match(port.strip()):
    env.Replace(CUSTOM_ESPOTA_IP=port.strip().strip('"'), UPLOAD_PORT="")  # noqa: F821
