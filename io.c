#include <stddef.h>
#include <stdint.h>

// GPIO

enum {
    PERIPHERAL_BASE = 0xFE000000,
    GPFSEL0         = PERIPHERAL_BASE + 0x200000,
    GPSET0          = PERIPHERAL_BASE + 0x20001C,
    GPCLR0          = PERIPHERAL_BASE + 0x200028,
    GPPUPPDN0       = PERIPHERAL_BASE + 0x2000E4
};

enum {
    GPIO_MAX_PIN       = 53,
    GPIO_FUNCTION_ALT5 = 2,
};

#define PULL_NONE 0

void mmio_write(uint32_t reg, uint32_t val) { 
    *(volatile uint32_t *) reg = val; 
}

uint32_t mmio_read(uint32_t reg) { 
    return *(volatile uint32_t *) reg; 
}


uint32_t gpio_call(uint32_t pin_number, uint32_t value, uint32_t base, uint32_t field_size, uint32_t field_max) {
    uint32_t field_mask = (1 << field_size) - 1;
  
    if (pin_number > field_max) 
        return 0;
    if (value > field_mask) 
        return 0; 

    unsigned int num_fields = 32 / field_size;
    unsigned int reg = base + ((pin_number / num_fields) * 4);
    unsigned int shift = (pin_number % num_fields) * field_size;

    unsigned int curval = mmio_read(reg);
    curval &= ~(field_mask << shift);
    curval |= value << shift;
    mmio_write(reg, curval);

    return 1;
}


uint32_t gpio_set(unsigned int pin_number, unsigned int value) { 
    return gpio_call(pin_number, value, GPSET0, 1, GPIO_MAX_PIN);
}

uint32_t gpio_clear(unsigned int pin_number, unsigned int value) { 
    return gpio_call(pin_number, value, GPCLR0, 1, GPIO_MAX_PIN); 
}

// Sets the pull state of a given pin as indicated by pin_number
uint32_t gpio_pull(unsigned int pin_number, unsigned int value) { 
    return gpio_call(pin_number, value, GPPUPPDN0, 2, GPIO_MAX_PIN); 
}

// Used to map the pin to the provided function, of which there can be multiple for a pin
uint32_t gpio_function(unsigned int pin_number, unsigned int value) { 
    return gpio_call(pin_number, value, GPFSEL0, 3, GPIO_MAX_PIN); 
}


void gpio_useAsAlt5(unsigned int pin_number) {
    gpio_pull(pin_number, PULL_NONE);
    gpio_function(pin_number, GPIO_FUNCTION_ALT5);
}

// UART

enum {
    AUX_BASE        = PERIPHERAL_BASE + 0x215000,
    AUX_ENABLES     = AUX_BASE + 4,
    AUX_MU_IO_REG   = AUX_BASE + 64,
    AUX_MU_IER_REG  = AUX_BASE + 68,
    AUX_MU_IIR_REG  = AUX_BASE + 72,
    AUX_MU_LCR_REG  = AUX_BASE + 76,
    AUX_MU_MCR_REG  = AUX_BASE + 80,
    AUX_MU_LSR_REG  = AUX_BASE + 84,
    AUX_MU_CNTL_REG = AUX_BASE + 96,
    AUX_MU_BAUD_REG = AUX_BASE + 104,
    AUX_UART_CLOCK  = 500000000,
    UART_MAX_QUEUE  = 16 * 1024
};

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)

void uart_init() {
    mmio_write(AUX_ENABLES, 1); // enable UART1
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_CNTL_REG, 0);
    mmio_write(AUX_MU_LCR_REG, 3); //8 bits
    mmio_write(AUX_MU_MCR_REG, 0);
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_IIR_REG, 0xC6); //disable interrupts
    mmio_write(AUX_MU_BAUD_REG, AUX_MU_BAUD(115200));
    gpio_useAsAlt5(14);
    gpio_useAsAlt5(15);
    mmio_write(AUX_MU_CNTL_REG, 3); //enable RX/TX
}

unsigned int uart_isWriteByteReady() { return mmio_read(AUX_MU_LSR_REG) & 0x20; }

void uart_writeByteBlockingActual(unsigned char ch) {
    while (!uart_isWriteByteReady()); 
    mmio_write(AUX_MU_IO_REG, (unsigned int)ch);
}

void uart_writeText(char *buffer) {
    while (*buffer) {
       if (*buffer == '\n') uart_writeByteBlockingActual('\r');
       uart_writeByteBlockingActual(*buffer++);
    }
}
