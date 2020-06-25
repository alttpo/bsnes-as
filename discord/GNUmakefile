
ifeq ($(platform),macos)
  discord.flags := $(flags.objcpp)
  discord.options := ../lib/x86_64/discord_game_sdk.bundle
else
  discord.flags := $(flags.cpp)
  discord.options :=
endif

discord.flags += -fPIC -fno-strict-aliasing -I$(discord.path)

discord.objects := achievement_manager \
                   activity_manager \
                   application_manager \
                   core \
                   image_manager \
                   lobby_manager \
                   network_manager \
                   overlay_manager \
                   relationship_manager \
                   storage_manager \
                   store_manager \
                   types \
                   user_manager \
                   voice_manager
discord.objects := $(discord.objects:%=obj/%.o)

obj/achievement_manager.o: $(discord.path)/achievement_manager.cpp
obj/activity_manager.o: $(discord.path)/activity_manager.cpp
obj/application_manager.o: $(discord.path)/application_manager.cpp
obj/core.o: $(discord.path)/core.cpp
obj/image_manager.o: $(discord.path)/image_manager.cpp
obj/lobby_manager.o: $(discord.path)/lobby_manager.cpp
obj/network_manager.o: $(discord.path)/network_manager.cpp
obj/overlay_manager.o: $(discord.path)/overlay_manager.cpp
obj/relationship_manager.o: $(discord.path)/relationship_manager.cpp
obj/storage_manager.o: $(discord.path)/storage_manager.cpp
obj/store_manager.o: $(discord.path)/store_manager.cpp
obj/types.o: $(discord.path)/types.cpp
obj/user_manager.o: $(discord.path)/user_manager.cpp
obj/voice_manager.o: $(discord.path)/voice_manager.cpp

# for global flags
flags += -I$(discord.path)

discord.verbose:
	$(info discord Compiler Flags:)
	$(foreach n,$(sort $(call unique,$(discord.flags))),$(if $(filter-out -I%,$n),$(info $([space]) $n)))
	$(info discord Linker Options:)
	$(foreach n,$(sort $(call unique,$(discord.options))),$(if $(filter-out -l%,$n),$(info $([space]) $n)))
