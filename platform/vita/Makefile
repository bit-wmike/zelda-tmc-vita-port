# The Minish Cap Vita — build file.
#
#   make TARGET=host   32-bit Linux test build (headless, for automated checks)
#   make TARGET=vita   PS Vita build (needs $VITASDK)
#
# Both builds compile the same engine + port sources as the 3DS port, with
# TMC_3DS (32-bit, SDL-free code paths) plus TMC_VITA defined. Only the files
# in source/ (Vita) or host/ (test harness) differ.

TARGET ?= vita
ROOT := ../..
BUILD := build-$(TARGET)

ACTOR_DIRS := playerItem enemy npc object projectile manager item subtask menu worldEvent
ACTOR_SRC := $(foreach d,$(ACTOR_DIRS),$(wildcard $(ROOT)/src/$(d)/*.c))
AGBPLAY_SRC := $(wildcard $(ROOT)/libs/agbplay_core/*.cpp)

ENGINE_SRC := $(addprefix $(ROOT)/,\
    src/main.c src/common.c src/interrupts.c src/game.c src/title.c src/fileselect.c \
    src/flags.c src/save.c src/fade.c src/color.c src/vram.c src/screenTileMap.c \
    src/message.c src/text.c src/sound.c \
    src/script.c src/scroll.c src/room.c src/roomInit.c src/entity.c src/physics.c \
    src/collision.c src/movement.c src/affine.c src/sineTable.c src/ui.c \
    src/player.c src/playerUtils.c src/playerHitbox.c src/playerItem.c \
    src/playerItemUtils.c src/playerItemDefinitions.c src/enemy.c src/enemyUtils.c \
    src/npc.c src/npcUtils.c src/npcFunctions.c src/npcDefinitions.c src/object.c \
    src/objectUtils.c src/objectDefinitions.c src/enemyUpdate.c src/projectile.c \
    src/projectileUtils.c src/projectileUpdate.c src/manager.c src/item.c \
    src/itemUtils.c src/itemDefinitions.c src/itemMetaData.c src/subtask.c \
    src/cutscene.c src/backgroundAnimations.c src/beanstalkSubtask.c \
    src/enterPortalSubtask.c src/gameOverTask.c src/gameUtils.c src/gameData.c \
    src/kinstone.c src/droptables.c src/demo.c src/debug.c src/flagDebug.c \
    src/staffroll.c src/data/figurineMenuData.c src/data/hitbox.c \
    src/data/areaMetadata.c src/data/caveBorderMapData.c src/data/data_080046A4.c \
    src/data/mapActTileToSurfaceType.c src/data/objPalettes.c \
    src/data/screenTransitions.c src/data/transitions.c src/code_08049CD4.c \
    src/code_08049DF4.c src/code_0805EC04.c)

PORT_SRC := $(addprefix $(ROOT)/,\
    port/port_bios.c port/port_debug_verbose.c port/port_inline_ptrs.c \
    port/port_script_addrs.c port/port_offset_remap.c port/port_rom.c \
    port/port_region_data.c port/port_bottle_compat.c port/port_bomb_compat.c \
    port/port_cloud_tops_fight.c port/port_vaati_progress.c \
    port/port_stubs.c port/stubs_autogen.c port/data_stubs_autogen.c \
    port/data_const_stubs.c port/flag_remap_generated.c port/port_rom_tables.c \
    port/port_linked_stubs.c port/port_figurines.c port/port_draw.c \
    port/port_3ds_full_view_policy.c \
    port/port_gba_mem.c port/port_hdma.c port/port_save.c port/port_softslots.c \
    port/port_dump_state.c port/port_second_screen_state.c \
    port/port_second_screen_render.c port/port_second_screen_worldmap.c \
    port/port_second_screen_dungeonmap.c port/port_second_screen_quest.c \
    port/port_second_screen_theme.c port/port_roll_attack_macro.c \
    port/port_filter.c port/port_animation.c port/port_math.c \
    port/port_text_render.c port/port_gameplay_stubs.c port/port_m4a_stubs.c \
    port/port_room_funcs.c port/port_script_funcs.c port/port_analog_movement.c \
    port/port_m4a_backend.cpp port/port_m4a_mixdown.c port/generated_sounds_embed.cpp \
    port/port_debug_actions.c port/port_reborn.cpp port/port_second_screen.c \
    port/ppu/src/virtuappu.c port/ppu/src/mode1.c)

COMMON_SRC := $(wildcard common/*.c)
ifneq ($(filter host armhost armlow,$(TARGET)),)
PLAT_SRC := $(wildcard host/*.c) $(COMMON_SRC)
else
PLAT_SRC := $(wildcard source/*.c) $(COMMON_SRC)
endif

SRC := $(ENGINE_SRC) $(ACTOR_SRC) $(PORT_SRC) $(AGBPLAY_SRC) $(PLAT_SRC)
OBJ := $(patsubst $(ROOT)/%,$(BUILD)/%,$(patsubst %,%.o,$(filter $(ROOT)/%,$(SRC)))) \
       $(patsubst %,$(BUILD)/local/%.o,$(PLAT_SRC))

DEFS := -DPC_PORT -DNON_MATCHING -DUSE_HDMA -DTMC_3DS -DTMC_VITA -DMULTI_REGION -DUSA \
        -DENGLISH -DREVISION=0 -DMODE1_GBA_WIDTH=400 -DMODE1_GBA_HEIGHT=240 \
        -DTMC_PORT_VERSION=\"$(shell cat VERSION)\" -DTMC_PC_VERSION=\"$(shell cat VERSION)\"
INCS := -I$(ROOT) -I$(ROOT)/include -I$(ROOT)/port -I$(ROOT)/port/ppu/include \
        -I$(ROOT)/libs/agbplay_core -I$(ROOT)/build/USA -Isource -Icommon -Icompat

WARN := -w
OPT ?= -O2
COMMON_FLAGS := -MMD -MP $(OPT) -ffunction-sections -fdata-sections -fno-strict-aliasing -fwrapv \
                -fno-strict-overflow $(WARN) -include region.h

ifeq ($(TARGET),host)
CC := gcc -m32
CXX := g++ -m32
ARCH_FLAGS := -g -pthread
LDLIBS := -lm -pthread
OUT := $(BUILD)/tmc_host
else ifeq ($(TARGET),armhost)
# Same headless harness, compiled for 32-bit ARMv7 hard-float like the Vita
# and run under qemu-arm: catches ARM-only code generation differences.
CC := arm-linux-gnueabihf-gcc
CXX := arm-linux-gnueabihf-g++
ARCH_FLAGS := -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb -g -pthread
LDLIBS := -lm -pthread -static -Wl,-Ttext-segment=0x81000000
OUT := $(BUILD)/tmc_armhost
else ifeq ($(TARGET),armlow)
# armhost at ordinary low addresses: differential reference for armhost.
CC := arm-linux-gnueabihf-gcc
CXX := arm-linux-gnueabihf-g++
ARCH_FLAGS := -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb -g -pthread
LDLIBS := -lm -pthread -static
OUT := $(BUILD)/tmc_armlow
else
PREFIX := arm-vita-eabi
CC := $(VITASDK)/bin/$(PREFIX)-gcc
CXX := $(VITASDK)/bin/$(PREFIX)-g++
ARCH_FLAGS := -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb \
              -fomit-frame-pointer
LDLIBS := -lvita2d -lSceDisplay_stub -lSceCtrl_stub -lSceTouch_stub -lSceAudio_stub \
          -lScePower_stub -lSceAppUtil_stub -lSceAppMgr_stub -lSceSysmodule_stub \
          -lSceCommonDialog_stub -lSceGxm_stub -lScePgf_stub \
          -lSceKernelDmacMgr_stub -lpthread -lm -lstdc++
OUT := $(BUILD)/tmc_vita.elf
endif

CFLAGS := $(ARCH_FLAGS) $(COMMON_FLAGS) $(DEFS) $(INCS) -std=gnu11
CXXFLAGS := $(ARCH_FLAGS) $(COMMON_FLAGS) $(DEFS) $(INCS) -std=gnu++20 -fno-rtti
FASTMATH := -ffast-math

all: $(OUT)

$(BUILD)/%.c.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# The software renderer is the hot path of every frame: optimised harder,
# like the 3DS port does for the same files.
PPU_OPT ?= -O3
$(BUILD)/port/ppu/src/%.c.o: $(ROOT)/port/ppu/src/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(PPU_OPT) -c $< -o $@

$(BUILD)/libs/agbplay_core/%.cpp.o: $(ROOT)/libs/agbplay_core/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(FASTMATH) -c $< -o $@

$(BUILD)/port/port_m4a_mixdown.c.o: $(ROOT)/port/port_m4a_mixdown.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(FASTMATH) -c $< -o $@

$(BUILD)/port/port_m4a_backend.cpp.o: $(ROOT)/port/port_m4a_backend.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(FASTMATH) -c $< -o $@

$(BUILD)/%.cpp.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/local/%.c.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(filter-out -w,$(CFLAGS)) -Wall -Wno-unused-function -Wno-unused-variable -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -c $< -o $@

$(OUT): $(OBJ)
	@echo LINK $@ && $(CXX) $(ARCH_FLAGS) -Wl,--gc-sections -Wl,-q $^ $(LDLIBS) -o $@

-include $(OBJ:.o=.d)

# Rebuild everything when the compile definitions change.
$(OBJ): $(BUILD)/flags.stamp
$(BUILD)/flags.stamp: FORCE
	@mkdir -p $(BUILD)
	@echo '$(CFLAGS) $(CXXFLAGS) $(PPU_OPT)' | cmp -s - $@ || echo '$(CFLAGS) $(CXXFLAGS) $(PPU_OPT)' > $@
FORCE:

TITLE_ID := TMCV00001
TITLE := The Minish Cap
VPK := $(BUILD)/TheMinishCap_Vita_$(shell cat VERSION).vpk
# Optional personal LiveArea artwork (icon0.png, livearea/contents/bg.png,
# startup.png) in sce_sys_local/ - git-ignored, so third-party art never lands
# in the repository. Missing files fall back to the placeholders in sce_sys/.
ART = $(if $(wildcard sce_sys_local/$(1)),sce_sys_local/$(1),sce_sys/$(1))

vpk: $(VPK)

$(VPK): $(OUT) $(call ART,icon0.png) $(call ART,livearea/contents/bg.png) $(call ART,livearea/contents/startup.png) sce_sys/livearea/contents/template.xml VERSION
	$(VITASDK)/bin/vita-elf-create $(OUT) $(BUILD)/tmc.velf
	$(VITASDK)/bin/vita-make-fself -s -c $(BUILD)/tmc.velf $(BUILD)/eboot.bin
	$(VITASDK)/bin/vita-mksfoex -s TITLE_ID=$(TITLE_ID) -s APP_VER=01.00 \
	    -d ATTRIBUTE2=12 "$(TITLE)" $(BUILD)/param.sfo
	$(VITASDK)/bin/vita-pack-vpk -s $(BUILD)/param.sfo -b $(BUILD)/eboot.bin \
	    -a $(call ART,icon0.png)=sce_sys/icon0.png \
	    -a $(call ART,livearea/contents/bg.png)=sce_sys/livearea/contents/bg.png \
	    -a $(call ART,livearea/contents/startup.png)=sce_sys/livearea/contents/startup.png \
	    -a sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
	    $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean vpk FORCE
