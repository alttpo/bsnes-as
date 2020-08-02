name := bsnes

hiro.path := ../hiro
hiro.resource := $(ui)/resource/bsnes.rc
include $(hiro.path)/GNUmakefile

ruby.path := ../ruby
include $(ruby.path)/GNUmakefile

angel.path := ../angelscript
include $(angel.path)/GNUmakefile

discord.path := ../discord
include $(discord.path)/GNUmakefile

objects += ui-bsnes ui-program ui-input ui-presentation
objects += ui-settings ui-tools ui-resource
objects := $(objects:%=obj/%.o)

obj/ui-bsnes.o: $(ui)/bsnes.cpp
obj/ui-program.o: $(ui)/program/program.cpp
obj/ui-input.o: $(ui)/input/input.cpp
obj/ui-presentation.o: $(ui)/presentation/presentation.cpp
obj/ui-settings.o: $(ui)/settings/settings.cpp
obj/ui-tools.o: $(ui)/tools/tools.cpp
obj/ui-resource.o: $(ui)/resource/resource.cpp

all: $(objects) $(hiro.objects) $(ruby.objects) $(angel.objects) $(discord.objects)
	$(info Linking out/$(name) ...)
ifeq ($(platform),windows)
	cp ../lib/$(arch)/discord_game_sdk.dll* .
else ifeq ($(platform),macos)
	cp ../lib/$(arch)/discord_game_sdk.bundle .
	cp ../lib/$(arch)/discord_game_sdk.dylib .
else ifeq ($(platform),linux)
	cp ../lib/$(arch)/discord_game_sdk.so .
endif
	+@$(compiler) -o out/$(name) $(hiro.objects) $(ruby.objects) $(angel.objects) $(discord.objects) $(objects) $(hiro.options) $(ruby.options) $(angel.options) $(discord.options) $(options)
ifeq ($(platform),windows)
	cp discord_game_sdk.dll out/
else ifeq ($(platform),macos)
	rm -rf out/$(name).app
	mkdir -p out/$(name).app/Contents/MacOS/
	mkdir -p out/$(name).app/Contents/MacOS/Database/
	mkdir -p out/$(name).app/Contents/MacOS/Firmware/
	mkdir -p out/$(name).app/Contents/MacOS/Shaders/
	mkdir -p out/$(name).app/Contents/Resources/
	mv out/$(name) out/$(name).app/Contents/MacOS/$(name)
	install_name_tool -add_rpath @executable_path/ out/$(name).app/Contents/MacOS/bsnes
	cp discord_game_sdk.dylib out/$(name).app/Contents/MacOS/
	cp Database/* out/$(name).app/Contents/MacOS/Database/
	cp -r ../shaders/* out/$(name).app/Contents/MacOS/Shaders/
	cp $(ui)/resource/$(name).plist out/$(name).app/Contents/Info.plist
	sips -s format icns $(ui)/resource/$(name).png --out out/$(name).app/Contents/Resources/$(name).icns
else ifeq ($(platform),linux)
	cp discord_game_sdk.so out/
endif

verbose: hiro.verbose ruby.verbose angel.verbose nall.verbose all;

install: all
ifeq ($(platform),windows)
else ifeq ($(shell id -un),root)
	$(error "make install should not be run as root")
else ifeq ($(platform),macos)
	mkdir -p ~/Library/Application\ Support/$(name)/
	mkdir -p ~/Library/Application\ Support/$(name)/Database/
	mkdir -p ~/Library/Application\ Support/$(name)/Firmware/
	mkdir -p ~/Library/Application\ Support/$(name)/Shaders/
	cp -R out/$(name).app /Applications/$(name).app
else ifneq ($(filter $(platform),linux bsd),)
	mkdir -p $(prefix)/bin/
	mkdir -p $(prefix)/share/applications/
	mkdir -p $(prefix)/share/icons/
	mkdir -p $(prefix)/share/$(name)/
	mkdir -p $(prefix)/share/$(name)/Database/
	mkdir -p $(prefix)/share/$(name)/Firmware/
	mkdir -p $(prefix)/share/$(name)/Shaders/
	cp out/$(name) $(prefix)/bin/$(name)
	cp $(ui)/resource/$(name).desktop $(prefix)/share/applications/$(name).desktop
	cp $(ui)/resource/$(name).png $(prefix)/share/icons/$(name).png
	cp Database/* $(prefix)/share/$(name)/Database/
	cp -r ../shaders/* $(prefix)/share/$(name)/Shaders/
endif

uninstall:
ifeq ($(platform),windows)
else ifeq ($(shell id -un),root)
	$(error "make uninstall should not be run as root")
else ifeq ($(platform),macos)
	rm -rf /Applications/$(name).app
else ifneq ($(filter $(platform),linux bsd),)
	rm -f $(prefix)/bin/$(name)
	rm -f $(prefix)/share/applications/$(name).desktop
	rm -f $(prefix)/share/icons/$(name).png
	rm -rf $(prefix)/share/$(name)
endif
