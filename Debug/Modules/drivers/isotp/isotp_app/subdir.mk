################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Modules/drivers/isotp/isotp_app/isotp_app.c 

OBJS += \
./Modules/drivers/isotp/isotp_app/isotp_app.o 

C_DEPS += \
./Modules/drivers/isotp/isotp_app/isotp_app.d 


# Each subdirectory must supply rules for building sources it contributes
Modules/drivers/isotp/isotp_app/%.o Modules/drivers/isotp/isotp_app/%.su Modules/drivers/isotp/isotp_app/%.cyclo: ../Modules/drivers/isotp/isotp_app/%.c Modules/drivers/isotp/isotp_app/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"/Users/XuanThanh/automoiive_uds_nhom1/Modules/drivers/isotp" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Modules-2f-drivers-2f-isotp-2f-isotp_app

clean-Modules-2f-drivers-2f-isotp-2f-isotp_app:
	-$(RM) ./Modules/drivers/isotp/isotp_app/isotp_app.cyclo ./Modules/drivers/isotp/isotp_app/isotp_app.d ./Modules/drivers/isotp/isotp_app/isotp_app.o ./Modules/drivers/isotp/isotp_app/isotp_app.su

.PHONY: clean-Modules-2f-drivers-2f-isotp-2f-isotp_app

