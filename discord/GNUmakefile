
#ifeq ($(platform),macos)
#  discord.flags := $(flags.objcpp)
#else
#  discord.flags := $(flags.cpp)
#endif

ifeq ($(platform),windows)
  # arch is 'x86_64' or 'x86'
  discord.options := -L../lib/$(arch) -ldiscord_game_sdk.dll
else ifeq ($(platform),macos)
  ifeq ($(arch),x86_64)
    discord.options := discord_game_sdk.bundle
  else
    discord = false
  endif
else ifeq ($(platform),linux)
  ifeq ($(arch),x86_64)
    discord.options := -Wl,-rpath,'${ORIGIN}':./ discord_game_sdk.so
  else
    discord = false
  endif
else
  # TODO: unsupported
endif

ifeq ($(discord),false)
  discord.flags += -DDISCORD_DISABLE=1
  discord.options =
endif

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
discord.objects := $(discord.objects:%=$(object.path)/%.o)

$(object.path)/achievement_manager.o: $(discord.path)/achievement_manager.cpp
$(object.path)/activity_manager.o: $(discord.path)/activity_manager.cpp
$(object.path)/application_manager.o: $(discord.path)/application_manager.cpp
$(object.path)/core.o: $(discord.path)/core.cpp
$(object.path)/image_manager.o: $(discord.path)/image_manager.cpp
$(object.path)/lobby_manager.o: $(discord.path)/lobby_manager.cpp
$(object.path)/network_manager.o: $(discord.path)/network_manager.cpp
$(object.path)/overlay_manager.o: $(discord.path)/overlay_manager.cpp
$(object.path)/relationship_manager.o: $(discord.path)/relationship_manager.cpp
$(object.path)/storage_manager.o: $(discord.path)/storage_manager.cpp
$(object.path)/store_manager.o: $(discord.path)/store_manager.cpp
$(object.path)/types.o: $(discord.path)/types.cpp
$(object.path)/user_manager.o: $(discord.path)/user_manager.cpp
$(object.path)/voice_manager.o: $(discord.path)/voice_manager.cpp

# for global flags
flags += -I$(discord.path) $(discord.flags)

discord.verbose:
	$(info discord Compiler Flags:)
	$(foreach n,$(sort $(call unique,$(discord.flags))),$(if $(filter-out -I%,$n),$(info $([space]) $n)))
	$(info discord Linker Options:)
	$(foreach n,$(sort $(call unique,$(discord.options))),$(if $(filter-out -l%,$n),$(info $([space]) $n)))
