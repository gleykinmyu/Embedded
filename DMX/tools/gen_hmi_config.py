# Generate nexHmiConfig.hpp: empty monitor (McUI) + net page.
from pathlib import Path

out = Path(__file__).resolve().parents[1] / "src" / "UI" / "nexHmiConfig.hpp"
print("nexHmiConfig.hpp is maintained by hand (monitor=ovl, net=HMI).")
print(out)
