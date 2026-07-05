# tests/integ_gen_config.mk — shared gen_config target for integration tests

gen_config:
	@mkdir -p $(GEN_INC)/ulmk
	@printf '#ifndef ULMK_CONFIG_H\n#define ULMK_CONFIG_H\n' > $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_MAX_THREADS      16\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_MAX_ENDPOINTS    32\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_MAX_NOTIFS       32\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_MAX_IRQ_BINDINGS 16\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_DEBUG_PRINTK     1\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#endif\n' >> $(GEN_INC)/ulmk/config.h
