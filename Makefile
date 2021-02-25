#*******************************************************************************
#   Ledger App
#   (c) 2017 Ledger
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#*******************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif
include $(BOLOS_SDK)/Makefile.defines

#########
#  App  #
#########

APPNAME    = Lamden
ICONNAME   = lamden.gif
APPVERSION = 1.0.0

# The --path argument here restricts which BIP32 paths the app is allowed to derive.
APP_LOAD_PARAMS = --appFlags 0x40 --path "44'/789'" --curve ed25519 $(COMMON_LOAD_PARAMS)
APP_SOURCE_PATH = src
SDK_SOURCE_PATH = lib_stusb lib_stusb_impl

all: default

load: all
	python -m ledgerblue.loadApp $(APP_LOAD_PARAMS)

delete:
	python -m ledgerblue.deleteApp $(COMMON_DELETE_PARAMS)

############
# Platform #
############

DEFINES += HAVE_UX_LEGACY
DEFINES += HAVE_UX_FLOW
DEFINES += OS_IO_SEPROXYHAL
#ORIG: DEFINES += HAVE_BAGL HAVE_SPRINTF
DEFINES += HAVE_BAGL HAVE_SPRINTF
DEFINES += HAVE_IO_USB HAVE_L4_USBLIB IO_USB_MAX_ENDPOINTS=7 IO_HID_EP_LENGTH=64 HAVE_USB_APDU
DEFINES   += HAVE_LEGACY_PID
#DEFINES   += CUSTOM_IO_APDU_BUFFER_SIZE=768
DEFINES += APPVERSION=\"$(APPVERSION)\"

# Nano X Defines
ifeq ($(TARGET_NAME),TARGET_NANOX)
    DEFINES     += IO_SEPROXYHAL_BUFFER_SIZE_B=300
    DEFINES     += HAVE_BLE BLE_COMMAND_TIMEOUT_MS=2000
    DEFINES     += HAVE_BLE_APDU # basic ledger apdu transport over BLE

    DEFINES     += HAVE_GLO096
    DEFINES     += HAVE_BAGL BAGL_WIDTH=128 BAGL_HEIGHT=64
    DEFINES     += HAVE_BAGL_ELLIPSIS # long label truncation feature
    DEFINES     += HAVE_BAGL_FONT_OPEN_SANS_REGULAR_11PX
    DEFINES     += HAVE_BAGL_FONT_OPEN_SANS_EXTRABOLD_11PX
    DEFINES     += HAVE_BAGL_FONT_OPEN_SANS_LIGHT_16PX
else
    DEFINES     += IO_SEPROXYHAL_BUFFER_SIZE_B=128
endif

# Enabling debug PRINTF
DEBUG = 1
ifneq ($(DEBUG),0)
    ifeq ($(TARGET_NAME),TARGET_NANOX)
        DEFINES   += HAVE_PRINTF PRINTF=mcu_usb_printf
    else
        DEFINES   += HAVE_PRINTF PRINTF=screen_printf
    endif
else
    DEFINES   += PRINTF\(...\)=
endif

##############
#  Compiler  #
##############

ifneq ($(BOLOS_ENV),)
    $(info BOLOS_ENV=$(BOLOS_ENV))
    CLANGPATH := $(BOLOS_ENV)/clang-arm-fropi/bin/
    GCCPATH := $(BOLOS_ENV)/gcc-arm-none-eabi-5_3-2016q1/bin/
else
    $(info BOLOS_ENV is not set: falling back to CLANGPATH and GCCPATH)
endif

ifeq ($(CLANGPATH),)
    $(info CLANGPATH is not set: clang will be used from PATH)
endif

ifeq ($(GCCPATH),)
    $(info GCCPATH is not set: arm-none-eabi-* will be used from PATH)
endif

CC := $(CLANGPATH)clang
CFLAGS += -O3 -Os

AS := $(GCCPATH)arm-none-eabi-gcc
LD := $(GCCPATH)arm-none-eabi-gcc
LDFLAGS += -O3 -Os 
LDLIBS += -lm -lgcc -lc

##################
#  Dependencies  #
##################

### variables processed by the common makefile.rules of the SDK to grab source files and include dirs
#APP_SOURCE_PATH  += src
#SDK_SOURCE_PATH  += lib_stusb lib_stusb_impl

ifeq ($(TARGET_NAME),TARGET_NANOX)
    SDK_SOURCE_PATH     += lib_blewbxx lib_blewbxx_impl
    SDK_SOURCE_PATH     += lib_ux
endif



# import rules to compile glyphs
include $(BOLOS_SDK)/Makefile.glyphs
# import generic rules from the sdk
include $(BOLOS_SDK)/Makefile.rules

dep/%.d: %.c Makefile

listvariants:
	@echo VARIANTS COIN sia
