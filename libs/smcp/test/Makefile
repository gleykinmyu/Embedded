# Тесты SMCP живут здесь. Сборка — стенд host: hub_can, sim_node, show/console.
#
#   make -C PUMS/Console/host check
#   make -C PUMS/Console/host segment-check

CONSOLE := $(abspath ../../../PUMS/Console)

.PHONY: check segment-check

check segment-check:
	$(MAKE) -C $(CONSOLE)/host $@
