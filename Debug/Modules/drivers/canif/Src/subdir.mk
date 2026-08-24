################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Modules/drivers/canif/Src/Can_if.c 

OBJS += \
./Modules/drivers/canif/Src/Can_if.o 

C_DEPS += \
./Modules/drivers/canif/Src/Can_if.d 


# Each subdirectory must supply rules for building sources it contributes
Modules/drivers/canif/Src/%.o Modules/drivers/canif/Src/%.su Modules/drivers/canif/Src/%.cyclo: ../Modules/drivers/canif/Src/%.c Modules/drivers/canif/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I/Users/XuanThanh/automoiive_uds_nhom1/Modules/app -I"/Users/XuanThanh/automoiive_uds_nhom1/Modules/drivers/isotp" -I"/Users/XuanThanh/automoiive_uds_nhom1/Modules/drivers/canif/Inc" -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"/Users/XuanThanh/automoiive_uds_nhom1/Modules/drivers/isotp" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Modules-2f-drivers-2f-canif-2f-Src

clean-Modules-2f-drivers-2f-canif-2f-Src:
	-$(RM) ./Modules/drivers/canif/Src/Can_if.cyclo ./Modules/drivers/canif/Src/Can_if.d ./Modules/drivers/canif/Src/Can_if.o ./Modules/drivers/canif/Src/Can_if.su

.PHONY: clean-Modules-2f-drivers-2f-canif-2f-Src

