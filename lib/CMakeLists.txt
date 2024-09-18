add_library(eeprom)
target_sources(eeprom INTERFACE
    eeprom/ee.c
    eeprom/ee.h
)
target_include_directories(eeprom INTERFACE
    eeprom
)

add_library(LCD)
target_sources(LCD INTERFACE
    LCD/LCD.c
    LCD/LCD.h
)
target_include_directories(LCD INTERFACE
    LCD
)

target_link_libraries(LCD stm32cubemx)
target_link_libraries(eeprom stm32cubemx)
    
add_subdirectory(stm32/cmake/stm32cubemx)
