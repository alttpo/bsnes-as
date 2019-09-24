
ifeq ($(platform),macos)
  angel.flags := $(flags.objcpp) -DAS_X64_GCC
else
  angel.flags := $(flags.cpp) -DAS_X64_GCC
endif

angel.flags += -fPIC -fno-strict-aliasing -I$(angel.path)/source

angel.options :=

angel.objects := as_atomic \
                 as_builder \
                 as_bytecode \
                 as_callfunc \
                 as_callfunc_mips \
                 as_callfunc_x86 \
                 as_callfunc_x64_gcc \
                 as_callfunc_x64_msvc \
                 as_callfunc_x64_mingw \
                 as_compiler \
                 as_configgroup \
                 as_context \
                 as_datatype \
                 as_gc \
                 as_generic \
                 as_globalproperty \
                 as_memory \
                 as_module \
                 as_objecttype \
                 as_outputbuffer \
                 as_parser \
                 as_restore \
                 as_scriptcode \
                 as_scriptengine \
                 as_scriptfunction \
                 as_scriptnode \
                 as_scriptobject \
                 as_string \
                 as_string_util \
                 as_thread \
                 as_tokenizer \
                 as_typeinfo \
                 as_variablescope
angel.objects := $(angel.objects:%=obj/%.o)

obj/as_atomic.o: $(angel.path)/source/as_atomic.cpp
obj/as_builder.o: $(angel.path)/source/as_builder.cpp
obj/as_bytecode.o: $(angel.path)/source/as_bytecode.cpp
obj/as_callfunc.o: $(angel.path)/source/as_callfunc.cpp
obj/as_callfunc_mips.o: $(angel.path)/source/as_callfunc_mips.cpp
obj/as_callfunc_x86.o: $(angel.path)/source/as_callfunc_x86.cpp
obj/as_callfunc_x64_gcc.o: $(angel.path)/source/as_callfunc_x64_gcc.cpp
obj/as_callfunc_x64_msvc.o: $(angel.path)/source/as_callfunc_x64_msvc.cpp
obj/as_callfunc_x64_mingw.o: $(angel.path)/source/as_callfunc_x64_mingw.cpp
obj/as_compiler.o: $(angel.path)/source/as_compiler.cpp
obj/as_configgroup.o: $(angel.path)/source/as_configgroup.cpp
obj/as_context.o: $(angel.path)/source/as_context.cpp
obj/as_datatype.o: $(angel.path)/source/as_datatype.cpp
obj/as_gc.o: $(angel.path)/source/as_gc.cpp
obj/as_generic.o: $(angel.path)/source/as_generic.cpp
obj/as_globalproperty.o: $(angel.path)/source/as_globalproperty.cpp
obj/as_memory.o: $(angel.path)/source/as_memory.cpp
obj/as_module.o: $(angel.path)/source/as_module.cpp
obj/as_objecttype.o: $(angel.path)/source/as_objecttype.cpp
obj/as_outputbuffer.o: $(angel.path)/source/as_outputbuffer.cpp
obj/as_parser.o: $(angel.path)/source/as_parser.cpp
obj/as_restore.o: $(angel.path)/source/as_restore.cpp
obj/as_scriptcode.o: $(angel.path)/source/as_scriptcode.cpp
obj/as_scriptengine.o: $(angel.path)/source/as_scriptengine.cpp
obj/as_scriptfunction.o: $(angel.path)/source/as_scriptfunction.cpp
obj/as_scriptnode.o: $(angel.path)/source/as_scriptnode.cpp
obj/as_scriptobject.o: $(angel.path)/source/as_scriptobject.cpp
obj/as_string.o: $(angel.path)/source/as_string.cpp
obj/as_string_util.o: $(angel.path)/source/as_string_util.cpp
obj/as_thread.o: $(angel.path)/source/as_thread.cpp
obj/as_tokenizer.o: $(angel.path)/source/as_tokenizer.cpp
obj/as_typeinfo.o: $(angel.path)/source/as_typeinfo.cpp
obj/as_variablescope.o: $(angel.path)/source/as_variablescope.cpp

angel.addons := scriptarray scriptdictionary
angel.objects += $(angel.addons:%=obj/as_addon_%.o)

obj/as_addon_scriptarray.o: $(angel.path)/add_on/scriptarray/scriptarray.cpp
obj/as_addon_scriptdictionary.o: $(angel.path)/add_on/scriptdictionary/scriptdictionary.cpp

flags += -I$(angel.path)/include $(angel.addons:%=-I$(angel.path)/add_on/%)

angel.verbose:
	$(info angelscript Compiler Flags:)
	$(foreach n,$(sort $(call unique,$(angel.flags))),$(if $(filter-out -I%,$n),$(info $([space]) $n)))
	$(info angelscript Linker Options:)
	$(foreach n,$(sort $(call unique,$(angel.options))),$(if $(filter-out -l%,$n),$(info $([space]) $n)))
