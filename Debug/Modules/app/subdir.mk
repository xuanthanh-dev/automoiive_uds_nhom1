################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Modules/app/app_canif.c \
../Modules/app/app_diag.c \
../Modules/app/app_engine.c 

OBJS += \
./Modules/app/app_canif.o \
./Modules/app/app_diag.o \
./Modules/app/app_engine.o 

C_DEPS += \
./Modules/app/app_canif.d \
./Modules/app/app_diag.d \
./Modules/app/app_engine.d 


# Each subdirectory must supply rules for building sources it contributes
Modules/app/%.o Modules/app/%.su Modules/app/%.cyclo: ../Modules/app/%.c Modules/app/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I/Users/XuanThanh/automoiive_uds_nhom1/Modules/app -I"/Users/XuanThanh/automoiive_uds_nhom1/Modules/drivers/isotp" -I"/Users/XuanThanh/automoiive_uds_nhom1/Modules/drivers/canif/Inc" -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"/Users/XuanThanh/automoiive_uds_nhom1/Modules/drivers/isotp" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Modules-2f-app

clean-Modules-2f-app:
	-$(RM) ./Modules/app/app_canif.cyclo ./Modules/app/app_canif.d ./Modules/app/app_canif.o ./Modules/app/app_canif.su ./Modules/app/app_diag.cyclo ./Modules/app/app_diag.d ./Modules/app/app_diag.o ./Modules/app/app_diag.su ./Modules/app/app_engine.cyclo ./Modules/app/app_engine.d ./Modules/app/app_engine.o ./Modules/app/app_engine.su

.PHONY: clean-Modules-2f-app

