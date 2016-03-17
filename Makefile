
# ARDUINO gcc distro
#ARDUINO   := $(HOME)/Documents/teensy/arduino-1.6.5-r5
#TOOLCHAIN := $(ARDUINO)/hardware/tools/arm/arm-none-eabi
#BIN       := $(ARDUINO)/hardware/tools/arm/bin
#CC        := $(BIN)/arm-none-eabi-gcc
#CXX       := $(BIN)/arm-none-eabi-g++
#OBJCOPY   := $(BIN)/arm-none-eabi-objcopy
#SIZE      := $(BIN)/arm-none-eabi-size

# Distro installed gcc
TOOLCHAIN = /usr/arm-none-eabi
CC        = arm-none-eabi-gcc
CXX       = arm-none-eabi-g++
OBJCOPY   = arm-none-eabi-objcopy
SIZE      = arm-none-eabi-size

TARGET    = main
SRC       = src
INCLUDE   = include
DEPENDS   = depends

# depends/
CORES         := $(DEPENDS)/cores-master-20160302/teensy3
CORES_INC     := $(CORES)
CORES_SRC     := $(CORES)
LINKER_SCRIPT := $(CORES)/mk20dx256.ld
SD            := $(DEPENDS)/adafruit-SD-master-20131105/utility
SD_INC        := $(SD)
SD_SRC        := $(SD)
SPI           := $(DEPENDS)/spi-master-20150403
SPI_INC       := $(SPI)
SPI_SRC       := $(SPI)


# Teensy 3.2 Options
OPTIONS = -DF_CPU=48000000 -D__MK20DX256__ 
# required for SPI.h
OPTIONS += -DTEENSYDUINO=121
# Enable logging to be tx'd on the hardware serial 1, comment out to disable
OPTIONS += -DDEBUG -DSERIAL_BAUD=115200

INCLUDES := -I$(TOOLCHAIN)/include -I$(INCLUDE) -I$(CORES_INC) -I$(SD_INC) -I$(SPI_INC)

# C/C++ Preprocessor flags
CPPFLAGS  = -Wall -Wextra -mcpu=cortex-m4 -mthumb -nostdlib -MMD 
CPPFLAGS += $(OPTIONS) $(INCLUDES) -Os -mapcs-frame 

CFLAGS = -ffunction-sections -fdata-sections -Wno-old-style-declaration

# C++ Flags
CXXFLAGS  = -std=gnu++0x -felide-constructors -fno-exceptions -fno-rtti

# Linker Flags
LDFLAGS  = -Wl,--gc-sections,--defsym=__rtc_localtime=0 --specs=nano.specs 
LDFLAGS += -mcpu=cortex-m4 -mthumb -T$(LINKER_SCRIPT) -Os

# Source files to create object files 
C_FILES         := $(wildcard $(SRC)/*.c) 
CXX_FILES       := $(wildcard $(SRC)/*.cpp)
CORES_S_FILES   := $(wildcard $(CORES_SRC)/*.S)
CORES_C_FILES   := $(wildcard $(CORES_SRC)/*.c)
CORES_CPP_FILES := $(wildcard $(CORES_SRC)/*.cpp)
SD_CPP_FILES    := $(wildcard $(SD_SRC)/*.cpp)
SPI_CPP_FILES   := $(wildcard $(SPI_SRC)/*.cpp)

OBJS := $(C_FILES:.c=.o) $(CXX_FILES:.cpp=.o) $(CORES_S_FILES:.S=.o)
OBJS += $(CORES_C_FILES:.c=.o) $(CORES_CPP_FILES:.cpp=.o) 
OBJS += $(SD_CPP_FILES:.cpp=.o) $(SPI_CPP_FILES:.cpp=.o)

###############################################################################

all: $(TARGET).hex

$(TARGET).elf: $(OBJS) $(LINKER_SCRIPT)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.hex: %.elf
	$(SIZE) $<
	$(OBJCOPY) -O ihex -R .eeprom $< $@

clean:
	rm -f $(TARGET).{hex,elf}  
	rm -f $(SRC)/*.{o,d}   
	rm -f $(CORES_SRC)/*.{o,d}
	rm -f $(SD_SRC)/*.{o,d}
	rm -f $(SPI_SRC)/*.{o,d}

# compiler generated dependency info
-include $(OBJS:.o=.d)

