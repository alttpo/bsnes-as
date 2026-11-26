name := bsnes-headless

# no GUI: the script GUI API is registered against hiro-fake.hpp stubs so scripts that build windows still compile and
# run; discord is never linked.
flags += -DDISABLE_HIRO=1 -DDISCORD_DISABLE=1

angel.path := ../angelscript
include $(angel.path)/GNUmakefile

# script-discord.cpp uses discord's wrapper, built with DISCORD_DISABLE into stubs, so the SDK itself is never linked:
discord.path := ../discord
discord := false
include $(discord.path)/GNUmakefile

objects += ui-headless
objects := $(objects:%=$(object.path)/%.o)

$(object.path)/ui-headless.o: $(ui)/headless.cpp

all: $(objects) $(angel.objects) $(discord.objects)
	$(info Linking out/$(name) ...)
	+@$(compiler) -o out/$(name) $(objects) $(angel.objects) $(discord.objects) $(angel.options) $(options)
