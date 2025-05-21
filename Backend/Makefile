#Almost universal makefile

#directories with other modules (including itself)
WORKING_DIRS := ./ global/ ../LangGlobals/
#Name of directory where .o and .d files will be stored
OBJDIR := build
OBJ_DIRS := $(addsuffix $(OBJDIR),$(WORKING_DIRS))

CMD_DEL = rm -rf $(addsuffix /*,$(OBJ_DIRS))
CMD_MKDIR = mkdir -p $(OBJ_DIRS)

ASAN_FLAGS := -fsanitize=address,alignment,bool,bounds,enum,float-cast-overflow,float-divide-by-zero,integer-divide-by-zero,leak,nonnull-attribute,null,object-size,return,returns-nonnull-attribute,shift,signed-integer-overflow,undefined,unreachable,vla-bound,vptr

# Removed -Wswitch-enum
WARNING_FLAGS := -Wextra -Weffc++ -Waggressive-loop-optimizations -Wc++14-compat -Wmissing-declarations -Wcast-align -Wcast-qual -Wchar-subscripts -Wconditionally-supported -Wconversion \
-Wctor-dtor-privacy -Wempty-body -Wfloat-equal -Wformat-nonliteral -Wformat-security -Wformat-signedness -Wformat=2 -Winline -Wlogical-op -Wnon-virtual-dtor -Wopenmp-simd \
-Woverloaded-virtual -Wpacked -Wpointer-arith -Winit-self -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=2 -Wsuggest-attribute=noreturn \
-Wsuggest-final-methods -Wsuggest-final-types -Wsuggest-override -Wswitch-default  -Wsync-nand -Wundef -Wunreachable-code -Wunused -Wuseless-cast \
-Wvariadic-macros -Wno-literal-suffix -Wno-missing-field-initializers -Wno-narrowing -Wno-old-style-cast -Wno-varargs -Wstack-protector

FORMAT_FLAGS := -fcheck-new -fsized-deallocation -fstack-protector -fstrict-overflow -flto-odr-type-merging -fno-omit-frame-pointer

CUSTOM_DBG_FLAGS := -D_TREE_DUMP

override CFLAGS := -g -D _DEBUG -ggdb3 -std=c++17 -O0 $(CUSTOM_DBG_FLAGS) -Wall $(WARNING_FLAGS) $(FORMAT_FLAGS) -Wlarger-than=8192 -Wstack-usage=8192 -pie -fPIE -Werror=vla $(ASAN_FLAGS)

CFLAGS_RELEASE := -O3 -std=c++17 -DNDEBUG -DDISABLE_LOGGING -fstack-protector

BUILD = DEBUG

ifeq ($(BUILD),RELEASE)
	override CFLAGS := $(CFLAGS_RELEASE)
endif
#compilier
ifeq ($(origin CC),default)
	CC=g++
endif

#Name of compiled executable
NAME := ../back.out
#Name of directory with headers
INCLUDEDIRS := ./include ./global/include ../LangGlobals/include/

GLOBAL_SRCS     := $(addprefix global/source/, argvProcessor.cpp logger.cpp utils.cpp)
GLOBAL_OBJS     := $(subst source,$(OBJDIR), $(GLOBAL_SRCS:%.cpp=%.o))
GLOBAL_DEPS     := $(GLOBAL_OBJS:%.o=%.d)

LANG_GLOB_SRCS  := $(addprefix ../LangGlobals/source/, nameTable.c tree.c)
LANG_GLOB_OBJS  := $(subst source,$(OBJDIR), $(LANG_GLOB_SRCS:%.c=%.o))
LANG_GLOB_DEPS  := $(LANG_GLOB_OBJS:%.o=%.d)

LOCAL_SRCS      := $(addprefix source/, main.c backendInterface.c IRConverter.c backend_x86_64.c emitters_x86_64.c elfWriter.c)
LOCAL_OBJS      := $(subst source,$(OBJDIR), $(LOCAL_SRCS:%.c=%.o))
LOCAL_DEPS      := $(LOCAL_OBJS:%.o=%.d)

#flag to tell compiler where headers are located
override CFLAGS += $(addprefix -I,$(INCLUDEDIRS))
#Main target to compile executables
#Filtering other mains from objects
$(NAME): $(GLOBAL_OBJS) $(LANG_GLOB_OBJS) $(LOCAL_OBJS)
	$(CC) $(CFLAGS) $^ $(addprefix -l,$(LINK_LIBS)) -o $@

#Easy rebuild in release mode
RELEASE:
	make clean
	make BUILD=RELEASE

#Automatic target to compile object files
#$(OBJS) : $(CUR_DIR)/$(OBJDIR)/%.o : %.cpp
$(GLOBAL_OBJS)     : global/$(OBJDIR)/%.o : global/source/%.cpp ../LangGlobals/include/Context.h
	$(CMD_MKDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LANG_GLOB_OBJS)  : ../LangGlobals/$(OBJDIR)/%.o : ../LangGlobals/source/%.c ../LangGlobals/include/Context.h
	$(CMD_MKDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LOCAL_OBJS)      : $(OBJDIR)/%.o : source/%.c ../LangGlobals/include/Context.h
	$(CMD_MKDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# build/backend.o: ../LangGlobals/include/context.h backend.h
#Idk how it works, but is uses compiler preprocessor to automatically generate
#.d files with included headears that make can use
$(GLOBAL_DEPS)     : global/$(OBJDIR)/%.d : global/source/%.cpp
	$(CMD_MKDIR)
	$(CC) -E $(CFLAGS) $< -MM -MT $(@:.d=.o) > $@

$(LANG_GLOB_DEPS)  : ../LangGlobals/$(OBJDIR)/%.d : ../LangGlobals/source/%.c
	$(CMD_MKDIR)
	$(CC) -E $(CFLAGS) $< -MM -MT $(@:.d=.o) > $@

$(LOCAL_DEPS)      : $(OBJDIR)/%.d : source/%.c
	$(CMD_MKDIR)
	$(CC) -E $(CFLAGS) $< -MM -MT $(@:.d=.o) > $@

.PHONY:init
init:
	$(CMD_MKDIR)

#Deletes all object and .d files

.PHONY:clean stdlib asmTest
clean:
	$(CMD_DEL)

FILE=sub
asmTest:
	nasm -felf64 asm_tests/$(FILE).s -l asm_tests/$(FILE).lst -o /dev/null

stdlib:
	nasm -felf64 stdlib/stdlib.s -o build/stdlib.o
	ld build/stdlib.o -o stdlib/stdlib.elf

NODEPS = clean

#Includes make dependencies
ifeq (0, $(words $(findstring $(MAKECMDGOALS), $(NODEPS))))
include $(GLOBAL_DEPS)
include $(CONTAINERS_DEPS)
include $(LOCAL_DEPS)
endif
