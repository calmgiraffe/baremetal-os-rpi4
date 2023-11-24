#include <stddef.h>
#include <stdint.h>

#define REG_SIZE 4
#define REG_SIZE_B 32
#define GPIO_MAX_PIN 57

// GPIO section
enum {
    PERIPHERAL_BASE = 0xFE000000,
    GPIO_BASE = PERIPHERAL_BASE + 0x200000,
    GPFSEL0         = GPIO_BASE + 0x00, // function select
    GPSET0          = GPIO_BASE + 0x1C, // set
    GPCLR0          = GPIO_BASE + 0x28, // clear
    GPUP_PDN_CNTRL0 = GPIO_BASE + 0xE4, // pull up or down state
};

enum {
    PULL_NONE = 0x0,
    PULL_UP = 0x1,
    PULL_DOWN = 0x2,
};

enum {
    FUNC_INPUT = 0,
    FUNC_OUTPUT = 1,
    FUNC_ALT0 = 4,
    FUNC_ALT1 = 5,
    FUNC_ALT2 = 6,
    FUNC_ALT3 = 7,
    FUNC_ALT4 = 3,
    FUNC_ALT5 = 2,
};

inline void mmio_write(uint32_t reg, uint32_t val) { 
    *(volatile uint32_t *) reg = val; 
}

inline uint32_t mmio_read(uint32_t reg) { 
    return *(volatile uint32_t *) reg; 
}

/* 
Write the specified val (should be interpreted as bits of a field) into the
field of the respective pin, where the field is a part of the continguous set of
GPIO registers starting from base.

Return 1 on err, 0 on success.
*/
uint32_t gpio_call(uint32_t pin_num, uint32_t val, uint32_t base, uint32_t field_size) {
    uint32_t field_mask = (1 << field_size) - 1;
  
    if (pin_num > GPIO_MAX_PIN) 
        return 1;
    if (val > field_mask) 
        return 1;
    if (base % 4 != 0)
        return 1;

    // Consecutive fields of bits represent consecutive pins.
    // num_fields = 10 for GPFSELn (5 regs)
    // num_fields = 16 for PUP_PDN (4 regs)
    uint32_t num_fields = REG_SIZE_B / field_size;

    // Registers are continuous, ex, GPSET0 and GPSET1 are 0x1c and 0x20 respectively
    uint32_t reg = base + ((pin_num / num_fields) * REG_SIZE);
    uint32_t shift = (pin_num % num_fields) * field_size;

    // Preserve the other fields of the register.
    // Zero the field of interest, then write val into this field.
    uint32_t curval = mmio_read(reg);
    curval &= ~(field_mask << shift);
    curval |= val << shift;
    mmio_write(reg, curval);

    return 0;
}

// Used to map the pin to the provided function, of which there can be multiple for a pin
uint32_t gpio_function(unsigned int pin_number, unsigned int value) { 
    return gpio_call(pin_number, value, GPFSEL0, 3); 
}

uint32_t gpio_set(unsigned int pin_number) {
    // Two set registers: GPSET0 and GPSET1
    // GPSET0: bits 0..31 represent pins 0..31
    // GPSET1: bits 32..57 for pins 32..57
    return gpio_call(pin_number, 0x1, GPSET0, 1);
}

uint32_t gpio_clear(unsigned int pin_number) { 
    return gpio_call(pin_number, 0x1, GPCLR0, 1); 
}

// Sets the pull state of a given pin as indicated by pin_number
uint32_t gpio_pull(unsigned int pin_number, unsigned int value) { 
    return gpio_call(pin_number, value, GPUP_PDN_CNTRL0, 2); 
}

void gpio_useAsAlt5(unsigned int pin_number) {
    gpio_pull(pin_number, PULL_NONE);
    gpio_function(pin_number, FUNC_ALT5);
}

// UART section
enum {
    AUX_BASE        = PERIPHERAL_BASE + 0x215000,
    AUX_IRQ         = AUX_BASE + 0x00,
    AUX_ENABLES     = AUX_BASE + 0x04,
    AUX_MU_IO_REG   = AUX_BASE + 0x40,
    AUX_MU_IER_REG  = AUX_BASE + 0x44,
    AUX_MU_IIR_REG  = AUX_BASE + 0x48,
    AUX_MU_LCR_REG  = AUX_BASE + 0x4c,
    AUX_MU_MCR_REG  = AUX_BASE + 0x50,
    AUX_MU_LSR_REG  = AUX_BASE + 0x54,
    AUX_MU_CNTL_REG = AUX_BASE + 0x60,
    AUX_MU_BAUD_REG = AUX_BASE + 0x68,
    AUX_UART_CLOCK  = 500000000, // 500 MHz
    UART_MAX_QUEUE  = 16 * 1024
};

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)

void uart_init() {
    mmio_write(AUX_ENABLES, 1); // enable UART1
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_CNTL_REG, 0);
    mmio_write(AUX_MU_LCR_REG, 3); // 8 bits
    mmio_write(AUX_MU_MCR_REG, 0);
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_IIR_REG, 0xC6); // disable interrupts
    mmio_write(AUX_MU_BAUD_REG, AUX_MU_BAUD(115200));
    gpio_useAsAlt5(14);
    gpio_useAsAlt5(15);
    mmio_write(AUX_MU_CNTL_REG, 3); //enable RX/TX
}

uint32_t uart_isWriteByteReady() { 
    return mmio_read(AUX_MU_LSR_REG) & 0x20; 
}

void uart_writeByteBlockingActual(unsigned char ch) {
    while (!uart_isWriteByteReady()); 
    mmio_write(AUX_MU_IO_REG, (unsigned int) ch);
}

void uart_writeText(char *buffer) {
    while (*buffer) {
       if (*buffer == '\n') uart_writeByteBlockingActual('\r');
       uart_writeByteBlockingActual(*buffer++);
    }
}