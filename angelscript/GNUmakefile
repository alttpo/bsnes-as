
angel.options :=

angel.objects := as_atomic \
                 as_builder \
                 as_bytecode \
                 as_callfunc \
                 as_callfunc_mips \
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

ifeq ($(arch),arm64)
  ifeq ($(platform),macos)
    angel.objects += as_callfunc_arm64 as_callfunc_arm64_xcode
  else
    angel.objects += as_callfunc_arm64 as_callfunc_arm64_gcc
  endif
else ifeq ($(arch),x86_64)
  # TBD: include gcc and msvc?
  ifeq ($(platform),windows)
    angel.objects += as_callfunc_x86 as_callfunc_x64_gcc as_callfunc_x64_msvc as_callfunc_x64_mingw
  else
    angel.objects += as_callfunc_x86 as_callfunc_x64_gcc as_callfunc_x64_mingw
  endif
endif

angel.objects := $(angel.objects:%=$(object.path)/%.o)

$(object.path)/as_atomic.o: $(angel.path)/source/as_atomic.cpp
$(object.path)/as_builder.o: $(angel.path)/source/as_builder.cpp
$(object.path)/as_bytecode.o: $(angel.path)/source/as_bytecode.cpp
$(object.path)/as_callfunc.o: $(angel.path)/source/as_callfunc.cpp
$(object.path)/as_callfunc_mips.o: $(angel.path)/source/as_callfunc_mips.cpp
$(object.path)/as_callfunc_x86.o: $(angel.path)/source/as_callfunc_x86.cpp
$(object.path)/as_callfunc_x64_gcc.o: $(angel.path)/source/as_callfunc_x64_gcc.cpp
$(object.path)/as_callfunc_x64_msvc.o: $(angel.path)/source/as_callfunc_x64_msvc.cpp
$(object.path)/as_callfunc_x64_mingw.o: $(angel.path)/source/as_callfunc_x64_mingw.cpp
$(object.path)/as_callfunc_arm64.o: $(angel.path)/source/as_callfunc_arm64.cpp
$(object.path)/as_callfunc_arm64_gcc.o: $(angel.path)/source/as_callfunc_arm64_gcc.S
$(object.path)/as_callfunc_arm64_msvc.o: $(angel.path)/source/as_callfunc_arm64_msvc.asm
$(object.path)/as_callfunc_arm64_xcode.o: $(angel.path)/source/as_callfunc_arm64_xcode.S
$(object.path)/as_compiler.o: $(angel.path)/source/as_compiler.cpp
$(object.path)/as_configgroup.o: $(angel.path)/source/as_configgroup.cpp
$(object.path)/as_context.o: $(angel.path)/source/as_context.cpp
$(object.path)/as_datatype.o: $(angel.path)/source/as_datatype.cpp
$(object.path)/as_gc.o: $(angel.path)/source/as_gc.cpp
$(object.path)/as_generic.o: $(angel.path)/source/as_generic.cpp
$(object.path)/as_globalproperty.o: $(angel.path)/source/as_globalproperty.cpp
$(object.path)/as_memory.o: $(angel.path)/source/as_memory.cpp
$(object.path)/as_module.o: $(angel.path)/source/as_module.cpp
$(object.path)/as_objecttype.o: $(angel.path)/source/as_objecttype.cpp
$(object.path)/as_outputbuffer.o: $(angel.path)/source/as_outputbuffer.cpp
$(object.path)/as_parser.o: $(angel.path)/source/as_parser.cpp
$(object.path)/as_restore.o: $(angel.path)/source/as_restore.cpp
$(object.path)/as_scriptcode.o: $(angel.path)/source/as_scriptcode.cpp
$(object.path)/as_scriptengine.o: $(angel.path)/source/as_scriptengine.cpp
$(object.path)/as_scriptfunction.o: $(angel.path)/source/as_scriptfunction.cpp
$(object.path)/as_scriptnode.o: $(angel.path)/source/as_scriptnode.cpp
$(object.path)/as_scriptobject.o: $(angel.path)/source/as_scriptobject.cpp
$(object.path)/as_string.o: $(angel.path)/source/as_string.cpp
$(object.path)/as_string_util.o: $(angel.path)/source/as_string_util.cpp
$(object.path)/as_thread.o: $(angel.path)/source/as_thread.cpp
$(object.path)/as_tokenizer.o: $(angel.path)/source/as_tokenizer.cpp
$(object.path)/as_typeinfo.o: $(angel.path)/source/as_typeinfo.cpp
$(object.path)/as_variablescope.o: $(angel.path)/source/as_variablescope.cpp

angel.addons := scriptarray
angel.objects += $(angel.addons:%=$(object.path)/as_addon_%.o)

$(object.path)/as_addon_scriptarray.o: $(angel.path)/add_on/scriptarray/scriptarray.cpp

flags += -I$(angel.path)/include $(angel.addons:%=-I$(angel.path)/add_on/%)
# NOTE(jsd): enabling this causes internal build breaks in angelscript source (as_builder.cpp) and add-ons:
#flags += -DAS_NO_CLASS_METHODS

angel.verbose:
	$(info angelscript Compiler Flags:)
	$(foreach n,$(sort $(call unique,$(angel.flags))),$(if $(filter-out -I%,$n),$(info $([space]) $n)))
	$(info angelscript Linker Options:)
	$(foreach n,$(sort $(call unique,$(angel.options))),$(if $(filter-out -l%,$n),$(info $([space]) $n)))
